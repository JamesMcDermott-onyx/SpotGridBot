
#include "Utils/Result.h"
#include "Definitions.h"
#include "binance/ConnectionMD.h"
#include "binance/ConnectionSS.h"

using namespace UTILS;
using namespace CORE::CRYPTO;

namespace CORE {
namespace BINANCE {
////////////////////////////////////////////////////////////////////////////////
// ConnectionTR
////////////////////////////////////////////////////////////////////////////////

//------------------------------------------------------------------------------
ConnectionMD::ConnectionMD(const CRYPTO::Settings &settings, const std::string &loggingPropsPath, const ConnectionManager& connectionManager)
		: CORE::CRYPTO::ConnectionBaseMD(settings, loggingPropsPath, settings.m_name, connectionManager)
{
	m_connectionSS = std::make_unique<CORE::BINANCE::ConnectionSS>("ConnectionSS");
	
	GetMessageProcessor().Register([](const std::shared_ptr<CRYPTO::JSONDocument> jd)
								   {
									   // Try checking type field 'e'
									   auto msgType = jd->GetValue<std::string>("e");
									   if (!msgType.empty())
									   {
										   return msgType;
									   } // it was simple - type was in the field
									   else
									   {
										   msgType = jd->GetValue<std::string>("lastUpdateId"); //Incremental updates contain field 'lastUpdateId'
										   if (!msgType.empty())
										   {
											   return MSGTYPE_DepthNUpdate;
										   }
									   }
									   // Try error message
									   // {"error":{"code":3,"msg":"Invalid JSON: expected `,` or `]` at line 4 column 6"}}
									   if (jd->Has(MSGTYPE_Error))
									   {
										   return MSGTYPE_Error;
									   }
		
									   // Try result message
									   // {"result":null,"id":1}
									   if (jd->Has(MSGTYPE_Result) && jd->Has("id"))
									   {
										   return MSGTYPE_Result;
									   }
		
									   return MSGTYPE_Unknown;
								   });
	
	// Register messages
	//For unlimited depth we use the following msg
	GetMessageProcessor().Register(MSGTYPE_DepthUpdate, [this](const std::shared_ptr<CRYPTO::JSONDocument> jd)
	{
		auto snapshot = m_snapshot.find(UTILS::CurrencyPair(jd->GetValue<std::string>("s")));
		if (!snapshot->second)
		{
			m_messages.emplace_back(jd); //snapshot has not completed so cache msgs..
			return;
		}
		
		while (!m_messages.empty())
		{
			DepthUpdate(m_messages.front());  //flush cached msgs as snapshot download is now completed..
			m_messages.pop_front();
		}
		
		DepthUpdate(jd); //process incoming msg
	});
	
	//For limited depth 'n' levels we use the following msg
	GetMessageProcessor().Register(MSGTYPE_DepthNUpdate, [this](const std::shared_ptr<CRYPTO::JSONDocument> jd)
	{
		auto snapshot = m_snapshot.find(UTILS::CurrencyPair(jd->GetValue<std::string>("s")));
		if (!snapshot->second)
		{
			m_messages.emplace_back(jd);
			return;
		}
		
		while (!m_messages.empty())
		{
			DepthNUpdate(m_messages.front());  //flush cached msgs as snapshot download is now completed..
			m_messages.pop_front();
		}
		
		DepthNUpdate(jd); //process incoming msg
	});
	
	GetMessageProcessor().Register(MSGTYPE_Result, [this](const std::shared_ptr<CRYPTO::JSONDocument> jd)
	{
		OnMsgResult(jd->GetValue<std::string>("result"), jd->GetValue<int>("id"), true);
	});
	
	GetMessageProcessor().Register(MSGTYPE_Error, [this](const std::shared_ptr<CRYPTO::JSONDocument> jd)
	{
		try
		{
			auto errDesc = jd->GetSubObject("error");
			if (errDesc)
			{
				OnMsgError(errDesc->get("code").convert<int>(), errDesc->get("msg").toString(), true);
			}
			else
			{
				OnMsgError(0, "", UTILS::BoolResult(false, "Invalid error message descriptor"));
			}
		}
		catch (std::exception &e)
		{
			OnMsgError(0, "", UTILS::BoolResult(false, "Invalid error message descriptor: %s", std::string(e.what())));
		}
	});
}

void ConnectionMD::DepthUpdate(const std::shared_ptr<CRYPTO::JSONDocument> jd)
{
	static bool stale(true); // On startup check for old/stale msgs to be dropped.
	
	int64_t u = stol(jd->GetValue<std::string>("u"));
	if (stale)
	{
		if (u <= m_lastUpdateId)
		{
			poco_warning_f2(logger(), "Snapshot '%ld' dropping stale msg %ld", m_lastUpdateId, u);
			return;
		}
		stale = false; //Now receiving fresh msgs..
	}
		
	int64_t U = stol(jd->GetValue<std::string>("U"));
	if (U <= m_lastUpdateId + 1 && u >= m_lastUpdateId)
	{
		const auto update = ParseMessage(jd, "b", "a");
		PublishQuotes(ParseQuote(update->Bids, QuoteType::BID, jd->GetValue<std::string>("s")));
		PublishQuotes(ParseQuote(update->Asks, QuoteType::OFFER, jd->GetValue<std::string>("s")));
		m_lastUpdateId = u + 1;
	}
	else
	{
		poco_error_f3(logger(), "Snapshot SKIPPING '%ld' sequence U %ld u", m_lastUpdateId, U, u);
	}
}


void ConnectionMD::DepthNUpdate(const std::shared_ptr<CRYPTO::JSONDocument> jd)
{
	int64_t lastUpdateId = stol(jd->GetValue<std::string>("lastUpdateId"));
	if (m_lastUpdateId <= lastUpdateId)
	{
		const auto update = ParseMessage(jd, "bids", "asks");
		PublishQuotes(ParseQuote(update->Bids, QuoteType::BID, jd->GetValue<std::string>("s")));
		PublishQuotes(ParseQuote(update->Asks, QuoteType::OFFER, jd->GetValue<std::string>("s")));
	}
	else
	{
		poco_information_f2(logger(), "Snapshot '%ld' ignoring msg %ld", m_lastUpdateId, lastUpdateId);
	}
}

//------------------------------------------------------------------------------
/*! \brief called when Result message received
* @param result: extracted result
* @param id: extracted id
* @param res: parsing result
* */
void ConnectionMD::OnMsgResult(const std::string &result, const int id, const UTILS::BoolResult &res)
{
	poco_information_f2(logger(), "received result='%s' for id='%s'", result, std::to_string(id));
}


//------------------------------------------------------------------------------
/*! \brief called when Error message received
* @param errCode: extracted error code
* @param errMsg: extracted error message
* @param res: parsing result
* */
void ConnectionMD::OnMsgError(const int errCode, const std::string &errMsg, const UTILS::BoolResult &res)
{
	poco_error_f2(logger(), "received 'error': code='%s', msg='%s'", std::to_string(errCode), errMsg);
}


//------------------------------------------------------------------------------
/*! \brief Processing snapshot for each instrument */
void ConnectionMD::Snapshot(const TInstruments &instruments)
{
	std::condition_variable_any cv;
	std:: mutex mtx;

	for (const auto &inst: instruments)
	{
		Poco::replaceInPlace(m_settings.m_snapshot_http, std::string("INSTRUMENT"), inst);
		poco_information_f1(logger(), "Start SNAPSHOT for '%s'...", inst);
		try
		{
			const auto msg = m_connectionSS->GetSnapshot(m_settings.m_snapshot_http);
			if (msg.empty())
			{
				return;
			} // nothing to do
			
			// Create json and push in the queue
			auto jd = std::make_shared<CRYPTO::JSONDocument>(msg);
			m_lastUpdateId = stol(jd->GetValue<std::string>("lastUpdateId"));
			
			GetMessageProcessor().Enqueue(jd, [this, inst, &cv](const std::shared_ptr<CRYPTO::JSONDocument> jd)
			{
				const auto update = ParseMessage(jd, "bids", "asks");
				PublishQuotes(ParseQuote(update->Bids, QuoteType::BID, inst));
				PublishQuotes(ParseQuote(update->Asks, QuoteType::OFFER, inst));
				
				poco_information_f2(logger(), "QT_SNAPSHOT %s bid Levels: %d ", inst, int(update->Bids.size()));
				poco_information_f2(logger(), "QT_SNAPSHOT %s ask Levels: %d ", inst, int(update->Asks.size()));
				cv.notify_one();
			});

			cv.wait(mtx);
			poco_information_f1(logger(), "Finished SNAPSHOT for '%s'", inst);

			auto snapshot = m_snapshot.find(UTILS::CurrencyPair(inst));
			snapshot->second = true;
		}
		catch (...)
		{
			poco_error_f2(logger(), "Exception during SNAPSHOT for '%s' %s", inst, GetMessage(std::current_exception()));
		}
	} // for
}


//------------------------------------------------------------------------------
/*! \brief subscribe/unsubscribe helper
* @param instruments: list of instruments separated by comma
* @param method: SUBSCRIBE or UNSUBSCRIBE
* */
void ConnectionMD::Subscribe(const CRYPTO::ConnectionBase::TInstruments &instruments, const std::string &method, unsigned int levels)
{
	std::string depthStr;
	std::string depth { levels > 0 ? std::to_string(levels) : "" };
	for (const auto &inst: instruments)
	{
		m_snapshot.insert(std::make_pair<UTILS::CurrencyPair, bool>(UTILS::CurrencyPair(inst), false));
		
		depthStr += (depthStr.empty() ? "" : ",") + std::string("\"") + UTILS::tolower(inst) + // note: instrument must be in lower case for feed
					"@depth" + depth + "@100ms\"";
	}
	
	const auto signature = CORE::CRYPTO::TOOLS::EncryptWithHMAC(GetSettings().m_secretkey, depthStr);
	
	std::string payload = "{\n"
						  "\t\"method\": \"_METHOD_\",\n"
						  "\t\"params\": [_INSTRUMENTS_],\n"
						  "\t\"id\":1}";
	
	Poco::replaceInPlace(payload, std::string("_INSTRUMENTS_"), depthStr);
	Poco::replaceInPlace(payload, std::string("_METHOD_"), method);
	Send(payload);
}

}
}
