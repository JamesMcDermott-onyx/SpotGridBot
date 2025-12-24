#pragma once

#include <queue>

#include "Utils/Logging.h"
#include "Utils/MessageData.h"
#include "Utils/ErrorHandler.h"
#include "Utils/Result.h"

#include <Poco/DOM/Node.h>
#include "Poco/Net/HTTPSStreamFactory.h"
#include "Poco/Net/HTTPClientSession.h"
#include "Poco/Net/HTTPSClientSession.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/Net/WebSocket.h"
#include <Poco/Logger.h>

#include "Config.h"
#include "Crypto.h"
#include "JSONDocument.h"
#include "MessageProcessor.h"
#include "Tools.h"
#include "CryptoCommon.h"
#include "ActiveQuoteTable.h"
#include "Logger.h"
#include "IConnection.h"

namespace CORE {
	class ConnectionManager;
}

namespace CORE {

namespace CRYPTO {
const size_t MAX_BUFF = 10000000;  // 10MB buffer for large WebSocket messages (e.g., level2 snapshots)
const std::string JSON_ERROR_NOT_IMPLEMENTED = CreateJSONMessageWithCode("Not implemented");
// If connection thread has more exceptions in a row than this, connection breaks:
const int MAX_NUMBER_OF_EXCEPTIONS_IN_CONNECTION_THREAD = 100;

////////////////////////////////////////////////////////////////////////////
// Connection
////////////////////////////////////////////////////////////////////////////
class ConnectionBase : public CRYPTO::IConnection, public UTILS::Logging, public UTILS::ErrorHandler
{
public:
	ConnectionBase(const CRYPTO::Settings &settings, const std::string &loggingPropsPath, const std::string &loggerName, const ConnectionManager& connectionManager);
	
	virtual ~ConnectionBase();
	
	using TInstruments = std::set<std::string>;
	
	UTILS::BoolResult Connect() override;
	
	void Disconnect() override;
	
	void PublishQuotes(UTILS::BookUpdate::Ptr nmd);
	
	bool IsConnected() const override
	{
		return m_connected;
	}
	
	void SetActive(bool active) override
	{ m_active = active; }
	
	bool IsActive() override
	{ return m_active; }
	
	/*! \brief Returns last message receive time (in ns) */
	int64_t GetLastMessageTime() const
	{
		return m_lastMessageTime.load();
	}
	
	/*! \brief returns reference to settings */
	const CRYPTO::Settings &GetSettings() const override
	{
		return m_settings;
	}
	
	/*! \brief Returns a set of instruments from configuration */
	TInstruments GetInstruments() const;
	
	/*! \brief Returns depth from configuration */
	unsigned int GetDepth() const
	{
		return GetSettings().m_depth;
	}
	
	UTILS::BoolResult SubscribeInstrument(const std::string &symbol);
	
	UTILS::BoolResult UnsubscribeInstrument(const std::string &symbol);
	
	UTILS::BookUpdate::Ptr ParseQuote(PriceMessage::Levels &levels, const char side, const std::string &instrument);
	
	virtual std::string TranslateSymbol(const std::string &symbol) const
	{
		return symbol;
	}
	
	virtual std::string TranslateSymbolToExchangeSpecific(const std::string &symbol) const
	{
		return symbol;
	}
	
	void Start() override
	{
		const auto instruments = GetInstruments();
		Snapshot(instruments);
		Subscribe(instruments);
	}
	
	/*! \brief Processing snapshot for each instrument
	* @param instruments: set of instruments
	* */
	virtual void Snapshot(const TInstruments &instruments) { }
	
	virtual void Subscribe(const TInstruments &instruments) { }
	
	virtual void Unsubscribe(const TInstruments &instruments) { };
	
	//------------------------------------------------------------------------------
	virtual void SideTranslator(const char *side, PriceMessage::Levels &depth, const std::shared_ptr<JSONDocument> jd) const
	{
		if (auto levels = jd->GetArray(side))
		{
			Poco::Dynamic::Array da = *levels;
			for (size_t i = 0; i < levels->size(); ++i)
			{
				depth.emplace_back(std::make_shared<Level>());
				depth.back()->price = da[i][0].toString();
				depth.back()->size = da[i][1].toString();
			}
		}
	}
	
	//Parse Snapshot or incremental msg..
	virtual std::unique_ptr<PriceMessage> ParseMessage(const std::shared_ptr<JSONDocument> jd, const std::string &bidName, const std::string &askName) const
	{
		auto msg = std::make_unique<PriceMessage>();
		SideTranslator(bidName.c_str(), msg->Bids, jd);
		SideTranslator(askName.c_str(), msg->Asks, jd);
		return msg;
	}

/*! \brief Returns reference to message processor */
CRYPTO::MessageProcessor &GetMessageProcessor()
{
	return m_messageProcessor;
}

protected:
	
	/*! \brief creates internal websocket */
	virtual void CreateWebSocket();
	
	virtual int ReceiveWebSocketData(Poco::Net::WebSocket *ws, char *buffer, size_t bufferSize, int &outFlags);
	
	/*! \brief Helper: sends a payload to websocket
	* @param payload: payload to be sent
	* @return: true in success
	* */
	virtual bool Send(const std::string &payload);
	
	UTILS::CurrencyPair GetCurrencyPair(const std::string &symbol) const
	{
		return m_cpHash.GetCurrencyPair(symbol);
	}
	
	UTILS::BoolResult PublishQuote(int64_t key, int64_t refKey, int64_t timestamp,
															 int64_t receiveTime, UTILS::CurrencyPair cp, const UTILS::BookUpdate::Entry &entry);
	
	Settings m_settings;
	Logger m_logger; //Session logger..
private:
	bool m_active;
	
	std::unique_ptr<Poco::Net::HTTPSClientSession> m_cs;
	std::unique_ptr<Poco::Net::WebSocket> m_ws;
	
	
	/*! \brief Flag indicating that connection is connected or disconnected */
	std::atomic<bool> m_connected { false };
	
	/*! \brief Last received message time (in ns). Used to track inactivity */
	std::atomic<int64_t> m_lastMessageTime { 0 };
	
	/*! \brief Hash to make quicker search the currency pairs by symbols */
	UTILS::CurrencyPairHash m_cpHash;
	
	using MessageQueue = std::queue<std::shared_ptr<JSONDocument>>;
	MessageQueue m_messageQueue;
	
	char m_buffer[MAX_BUFF] { };
	std::string m_fragmentedMessage; // Accumulator for fragmented WebSocket messages

	std::unique_ptr<std::thread> m_listenerThread;
	
	CRYPTO::MessageProcessor m_messageProcessor;
	
	CORE::ActiveQuoteTable m_activeQuoteTable;
	
	// Number of published quotes
	std::atomic<unsigned long> m_publishedQuotesCounter { 0 };
	mutable std::atomic<unsigned long> m_publishedQuotesOld { 0 }; // to calculate delta
	const ConnectionManager& m_connectionManager;
};
}
}
