#include "Utils/FixTypes.h"
#include "OKX/ConnectionMD.h"

using namespace UTILS;

namespace CORE {
namespace OKX {

//------------------------------------------------------------------------------
ConnectionMD::ConnectionMD(const CRYPTO::Settings &settings, const std::string &loggingPropsPath, const ConnectionManager& connectionManager)
				: CORE::CRYPTO::ConnectionBaseMD(settings, loggingPropsPath, settings.m_name, connectionManager)
{
	GetMessageProcessor().Register([](const std::shared_ptr<CRYPTO::JSONDocument> jd)
								   {
									   // Try checking type field 'action'
									   auto msgType = jd->GetValue<std::string>("action");
									   if (!msgType.empty())
									   {
										   return msgType;
									   } // it was simple - type was in the field

									   // Try checking type field 'event'
									   msgType = jd->GetValue<std::string>("event");
									   if (!msgType.empty())
									   {
										   return msgType;
									   }
									   return MSGTYPE_Unknown;
								   });
	// Register messages
	GetMessageProcessor().Register(MSGTYPE_Snapshot, [this](const std::shared_ptr<CRYPTO::JSONDocument> jd)
	{
		auto arg = jd->GetSubObject("arg");
		if (!arg)
		{
			poco_error(logger(), "QT_SNAPSHOT Invalid (or not supported) arg not found");
			return;
		}

		const auto inst = TranslateSymbol(arg->get("instId").toString());
		const auto update = ParseMessage(jd, "bids", "asks");
		PublishQuotes(ParseQuote(update->Bids, QuoteType::BID, inst));
		PublishQuotes(ParseQuote(update->Asks, QuoteType::OFFER, inst));

		poco_information_f2(logger(), "QT_SNAPSHOT %s bid Levels: %d ", inst, int(update->Bids.size()));
		poco_information_f2(logger(), "QT_SNAPSHOT %s ask Levels: %d ", inst, int(update->Asks.size()));
	});

	GetMessageProcessor().Register(MSGTYPE_Update, [this](const std::shared_ptr<CRYPTO::JSONDocument> jd)
	{
		auto arg = jd->GetSubObject("arg");
		if (!arg)
		{
			poco_error(logger(), "QT_UPDATE Invalid (or not supported) arg not found");
			return;
		}

		const auto inst = TranslateSymbol(arg->get("instId").toString());
		const auto update = ParseMessage(jd, "bids", "asks");
		PublishQuotes(ParseQuote(update->Bids, QuoteType::BID, inst));
		PublishQuotes(ParseQuote(update->Asks, QuoteType::OFFER, inst));
	});

	GetMessageProcessor().Register(MSGTYPE_Subscribe, [this](const std::shared_ptr<CRYPTO::JSONDocument> jd)
	{
		auto arg = jd->GetSubObject("arg");
		poco_information_f1(logger(), "Subscribed '%s'", arg->get("instId").toString());
	});

	GetMessageProcessor().Register(MSGTYPE_Unsubscribe, [this](const std::shared_ptr<CRYPTO::JSONDocument> jd)
	{
		auto arg = jd->GetSubObject("arg");
		poco_information_f1(logger(), "Unsubscribed '%s'", arg->get("instId").toString());
	});
}

// Helper: translates levels from snapshot or incremental update
void ConnectionMD::SideTranslator(const char *side, CRYPTO::PriceMessage::Levels &depth, const std::shared_ptr<CRYPTO::JSONDocument> jd) const
{
	if (auto data = jd->GetArray("data"))
	{
		Poco::Dynamic::Array dynArray = *data;
		for (size_t i = 0; i < data->size(); ++i)
		{
			const auto &s = dynArray[i];
			const auto prices = s[side];
			if (prices.isArray())
			{
				for (size_t i = 0; i < prices.size(); ++i)
				{
					depth.emplace_back(std::make_shared<CRYPTO::Level>());
					depth.back()->price = prices[i][0].toString();
					depth.back()->size = prices[i][1].toString();
				}
			}
		}
	}
}

//------------------------------------------------------------------------------
/*! \brief subscribe/unsubscribe helper
* @param instruments: list of instruments separated by comma
* @param method: SUBSCRIBE or UNSUBSCRIBE
* */
void ConnectionMD::Subscribe(const CRYPTO::ConnectionBase::TInstruments &instruments, const std::string &method)
{
	std::string depthStr;
	for (const auto &inst: instruments)
	{
		depthStr += (depthStr.empty() ? "" : ",") + UTILS::Format("{\n"
																  "\"channel\": \"books\",\n"
																  "\"instId\": \"%s\"\n"
																  "}", UTILS::toupper(inst));
	}

	std::string payload = "{\n"
						  "\"op\": \"" + method + "\",\n"
												  "\"args\":\n"
												  "[" + depthStr + "]\n"
																   "}";

	Send(payload);
}

}
}