#include "ConnectionBase.h"


#include "ConnectionManager.h"
#include "Utils/Result.h"

#include "Poco/URI.h"
#include "CryptoCommon.h"

using namespace UTILS;
using namespace Poco;

namespace {
std::string GenerateStandardEntryId(const UTILS::CurrencyPair &cp, UTILS::QuoteType entryType, const std::string &price)
{
	return UTILS::Format("%s_%c%s", cp.ToString(), entryType.Bid() ? 'B' : 'A', price);
}
}

namespace CORE {
namespace CRYPTO {
ConnectionBase::ConnectionBase(const CRYPTO::Settings &settings, const std::string &loggingPropsPath, const std::string &loggerName, const ConnectionManager& connectionManager)
		: Logging(loggerName), m_settings(settings), m_logger(settings, loggingPropsPath), m_connectionManager(connectionManager)
{
	// Make sure all instruments are in upper case
	m_settings.m_instruments = UTILS::toupper(m_settings.m_instruments);
}


//------------------------------------------------------------------------------
ConnectionBase::~ConnectionBase()
{
	Disconnect();
}


//------------------------------------------------------------------------------
BoolResult ConnectionBase::Connect()
{
	if (!m_connected)
	{
		try
		{
			CreateWebSocket();
		}
		catch (Poco::Exception &e)
		{
			poco_error_f2(logger(), "Exception in session '%s' when attempting to create a websocket: %s", m_settings.m_name,
						  std::string(e.displayText()));
			return false;
		}
		
		m_messageProcessor.Start();
		m_connected = true;
		
		// Start listener thread..
		m_listenerThread = std::make_unique<std::thread>([this]()
														 {
															 int exceptionCounter = 0;
															 while (m_connected)
															 {
																 try
																 {
																	 memset(m_buffer, 0, sizeof(m_buffer));
																	 int flags { };
																	 const auto bytes =
																			 ReceiveWebSocketData(m_ws.get(), m_buffer, sizeof(m_buffer), flags);
					
																	 // Process ping/pong
																	 using namespace Poco::Net;
																	 if ((flags & WebSocket::FRAME_OP_BITMASK) == WebSocket::FRAME_OP_PING)
																	 {
																		 poco_information(logger(), "received PING");
																		 m_ws->sendFrame(m_buffer, 1,
																						 WebSocket::FRAME_FLAG_FIN | WebSocket::FRAME_OP_PONG);
																		 poco_information(logger(), "sent successfully");
																		 continue;
																	 }
																	 if ((flags & WebSocket::FRAME_OP_BITMASK) == WebSocket::FRAME_OP_PONG)
																	 {
																		 poco_information(logger(), "received PONG: ignored");
																		 continue;
																	 }

																 	 if ((flags & WebSocket::FRAME_OP_BITMASK) == WebSocket::FRAME_OP_CLOSE)
																 	 {
																		 poco_error(logger(), "socket closed at source...");
																 	  	 m_connected=false;
																		 return;
																	 }

																	 // Processing bytes
																	 if (bytes)
																	 {
																		 const auto res = GetMessageProcessor().ProcessMessage(
																				 std::make_shared<CRYPTO::JSONDocument>(m_buffer));
																		 if (!res)
																		 {
																			 poco_error_f2(logger(), "Message processor error: %s [buffer='%s']",
																						   res.ErrorMessage(), std::string(m_buffer));
																		 }
																		 
																		 m_logger.Protocol().Incoming(m_buffer);
																	 }
																	 else
																	 {
																		 break;
																	 }
																	 m_lastMessageTime.store(UTILS::CurrentTimestamp());
																	 exceptionCounter = 0;
																 }
																 catch (std::exception &e)
																 {
																	 poco_error_f2(logger(), "Exception in reader thread for session '%s': %s",
																				   m_settings.m_name, std::string(e.what()));
																	 if (++exceptionCounter > MAX_NUMBER_OF_EXCEPTIONS_IN_CONNECTION_THREAD)
																	 {
																		 poco_error_f1(logger(),
																					   "Too many exceptions (%s and counting) in the reader thread. Breaking...",
																					   std::to_string(exceptionCounter));
																		 break;
																	 }
																 }
															 }
															 poco_information_f1(logger(), "Listener thread for session '%s' has stopped",
																				 m_settings.m_name);
															 m_connected = false;
														 });
		
		m_logger.Session().Start(m_settings.m_name);
		poco_information_f1(logger(), "Session started: %s", m_settings.m_name);

		const auto instruments = GetInstruments();
		Subscribe(instruments);
		Snapshot(instruments);
	}
	else
	{
		poco_information_f1(logger(), "Session already started: %s", m_settings.m_name);
	}
	
	return true;
}


//------------------------------------------------------------------------------
void ConnectionBase::Disconnect()
{
	m_connected = false;
	if (m_ws)
	{
		m_ws->close();
	}
	
	if (m_listenerThread)
	{
		m_listenerThread->join();
		m_listenerThread.reset();
	}

	m_messageProcessor.Stop();
	m_logger.Session().Stop(m_settings.m_name);
	
	poco_information_f1(logger(), "Session '%s' has stopped", m_settings.m_name);
}


//------------------------------------------------------------------------------
/*! \brief Helper: sends a payload to websocket
* @param payload: payload to be sent
* @return: true in success
* */
bool ConnectionBase::Send(const std::string &payload)
{
	if (!m_ws)
	{
		poco_error(logger(), "Failed to send data: connection to web socket has not been created yet");
		return false;
	}

	poco_information_f1(logger(), "Sending data %s", payload);

	m_ws->sendFrame(payload.data(), static_cast<int>(payload.size()));
	m_logger.Protocol().Outging(payload);
	
	return true;
}


//------------------------------------------------------------------------------//
UTILS::BoolResult ConnectionBase::PublishQuote(int64_t key, int64_t refKey, int64_t timestamp,
															 int64_t receiveTime, UTILS::CurrencyPair cp, const UTILS::BookUpdate::Entry &entry)
{
	m_connectionManager.GetOrderBook()->AddEntry(key, refKey, timestamp, receiveTime, cp, entry);

	return true;
}


//------------------------------------------------------------------------------
/*! \brief Returns a set of instruments from configuration */
ConnectionBase::TInstruments ConnectionBase::GetInstruments() const
{
	StringTokenizer tok(m_settings.m_instruments, ",", StringTokenizer::TOK_TRIM | StringTokenizer::TOK_IGNORE_EMPTY);
	TInstruments instruments;
	for (size_t i = 0; i < tok.count(); ++i)
	{
		instruments.insert(TranslateSymbolToExchangeSpecific(tok[i]));
	}
	
	return instruments;
}

//------------------------------------------------------------------------------
UTILS::BoolResult ConnectionBase::SubscribeInstrument(const std::string &symbol)
{
	const auto instStr = UTILS::toupper(symbol);
	if (UTILS::CurrencyPair(TranslateSymbol(instStr)).Invalid())
	{
		return BoolResult(false, UTILS::Format("Invalid instrument '%s'", instStr));
	}
	
	const auto existingInstruments = GetInstruments();
	if (existingInstruments.find(instStr) != existingInstruments.cend())
	{
		return BoolResult(false, UTILS::Format("Instrument '%s' has been already subscribed", instStr));
	}
	
	// Update config
	m_settings.m_instruments += (m_settings.m_instruments.empty() ? "" : ",") + instStr;
	
	// Request snapshot and subscribe
	Snapshot({ instStr });
	Subscribe({ instStr });
	return true;
}


//------------------------------------------------------------------------------
UTILS::BoolResult ConnectionBase::UnsubscribeInstrument(const std::string &symbol)
{
	const auto instStr = UTILS::toupper(symbol);
	auto existingInstruments = GetInstruments();
	if (!existingInstruments.erase(instStr))
	{
		return BoolResult(false, UTILS::Format("Instrument '%s' has not been subscribed", instStr));
	}
	
	m_settings.m_instruments = existingInstruments.empty() ? "" : UTILS::VecToStr(existingInstruments, ",");
	
	Unsubscribe({ instStr });
	return true;
}


//------------------------------------------------------------------------------
/*! \brief creates internal websocket */
void ConnectionBase::CreateWebSocket()
{
	using namespace Poco::Net;
	Context::Ptr ctx = new Context(Context::CLIENT_USE, "", Context::VerificationMode::VERIFY_NONE, 9, true);
	
	m_cs = std::make_unique<Poco::Net::HTTPSClientSession>(m_settings.m_host, m_settings.m_port, ctx);
	HTTPRequest request(HTTPRequest::HTTP_GET, WSPostFixURL, HTTPMessage::HTTP_1_1);
	HTTPResponse response;
	m_ws = std::make_unique<WebSocket>(*m_cs.get(), request, response);
}


//------------------------------------------------------------------------------
int ConnectionBase::ReceiveWebSocketData(Poco::Net::WebSocket *ws, char *buffer, size_t bufferSize, int &outFlags)
{
	int result { ws ? ws->receiveFrame(buffer, bufferSize, outFlags) : 0 };
	
	return result;
}


//------------------------------------------------------------------------------
UTILS::BookUpdate::Ptr ConnectionBase::ParseQuote(CORE::CRYPTO::PriceMessage::Levels &levels, const char side, const std::string &instrument)
{
	BookUpdate::Ptr nmd { std::make_unique<BookUpdate>() };
	BookUpdate::Entry *entry;
	
	nmd->entries.resize(levels.size());
	BidAskPair<int64_t> currentLevel { 0, 0 };
	for (size_t i { 0 }; i < levels.size(); ++i)
	{
		entry = &(nmd->entries[i]);
		entry->entryType = side;
		
		bool bid { entry->entryType.Bid() };

		entry->instrument = GetCurrencyPair(instrument);
		entry->price = std::stod(levels[i]->price);
		entry->volume = std::stod(levels[i]->size);
		entry->updateType = (entry->volume == 0) ? QT_DELETE : QT_NEW;
		entry->refId = entry->id = GenerateStandardEntryId(entry->instrument, entry->entryType, levels[i]->price);
		entry->quoteId = "";
		entry->positionNo = currentLevel.Get(bid);
	}
	return nmd;
}

//------------------------------------------------------------------------------
void ConnectionBase::PublishQuotes(UTILS::BookUpdate::Ptr nmd)
{
	if (nmd)
	{
		int64_t key { 0 }, refKey { 0 };
		ActiveQuoteTable::QuoteInfoPtr replacedQuoteRef;

		const size_t cnt { nmd->entries.size() };
		uint64_t sequenceTag { std::hash<std::string>()("") };

		for (size_t i { 0 }; i < cnt; ++i)
		{
			BookUpdate::Entry &entry { nmd->entries[i] };
			entry.endOfMessage = (i == cnt - 1);
			entry.sequenceTag = sequenceTag;
			CurrencyPair cp = entry.instrument;
			if ((!entry.entryType.Valid() || !cp.Valid())) //no entry type with UPDATEs and DELETEs -> lookup entry and guess type
			{
				if (!entry.refId.empty())
				{
					ActiveQuoteTable::QuoteInfo quoteInfo;
					if (m_activeQuoteTable.FindQuoteInfo(entry.refId, quoteInfo))
					{
						if (!cp.Valid())
						{
							cp = quoteInfo.cp;
						}
						if (!entry.entryType.Valid())
						{
							entry.entryType = quoteInfo.entryType;
						}
					}
					else
					{
						poco_error_f3(logger(), "Session %ld - ERROR: No quote info found for entry '%s'->'%s' -> QUOTE SKIPPED", GetSettings().m_numId, entry.id,
									  entry.refId);
						continue;
					}
				}
				else
				{
					poco_error_f3(logger(), "Session %ld - ERROR: No entry type and/or symbol and no ref ID in entry '%s'-> '%s' -> QUOTE SKIPPED",
								  GetSettings().m_numId, entry.id, entry.refId);
					continue;
				}
			}

			key = NewInt64Key();
			if (entry.updateType == QT_DELETE)
			{
				replacedQuoteRef = entry.refId.empty() ? nullptr : m_activeQuoteTable.RemoveQuoteInfo(entry.refId);
			}
			else
			{
				replacedQuoteRef = m_activeQuoteTable.ReplaceQuoteInfo(entry.refId, entry.id, key, cp, entry.entryType);
			}
			if (replacedQuoteRef)
			{
				if (entry.updateType == QT_NEW) // NEW refers to existing quote-> UPDATE
				{
					entry.updateType = QT_UPDATE;
				}
				refKey = replacedQuoteRef->key;
			}
			else
			{
				if (entry.updateType == QT_DELETE)
				{
					poco_error_f3(logger(), "%ld - ERROR: DELETE referring to non-existent entry '%s' --> '%s'", GetSettings().m_numId, entry.id, entry.refId);
					return;
				}
				else if (entry.updateType == QT_UPDATE) // UPDATE -> NEW
				{
					entry.updateType = QT_NEW;
				}
				refKey = 0;
			}

			PublishQuote(key, refKey, CurrentTimestamp(), CurrentTimestamp(), cp, entry);
		}
	}
	else
	{
		poco_error(logger(), "Connection::PublishQuotes: Normalized Market Data Ptr null");
	}
}

}
}