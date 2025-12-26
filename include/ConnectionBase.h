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
/*! \brief Base Connection Class for WebSocket Connections
 * 
 * Provides common WebSocket functionality for both market data and order
 * management connections. Handles WebSocket lifecycle, message processing,
 * and connection management.
 */
////////////////////////////////////////////////////////////////////////////
class ConnectionBase : public CRYPTO::IConnection, public UTILS::Logging, public UTILS::ErrorHandler
{
public:
	ConnectionBase(const CRYPTO::Settings &settings, const std::string &loggingPropsPath, const std::string &loggerName, const ConnectionManager& connectionManager);
	
	virtual ~ConnectionBase();
	
	using TInstruments = std::set<std::string>;
	
	/*! \brief Connect to WebSocket endpoint */
	UTILS::BoolResult Connect() override;
	
	/*! \brief Disconnect from WebSocket */
	void Disconnect() override;
	
	/*! \brief Check if WebSocket is connected */
	bool IsConnected() const override
	{
		return m_connected;
	}
	
	/*! \brief Set connection active/inactive state */
	void SetActive(bool active) override
	{ m_active = active; }
	
	/*! \brief Check if connection is active */
	bool IsActive() override
	{ return m_active; }
	
	/*! \brief Returns last message receive time (in ns) */
	int64_t GetLastMessageTime() const
	{
		return m_lastMessageTime.load();
	}
	
	/*! \brief Returns reference to settings */
	const CRYPTO::Settings &GetSettings() const override
	{
		return m_settings;
	}
	
	/*! \brief Returns a set of instruments from configuration */
	TInstruments GetInstruments() const;
	
	/*! \brief Translate symbol from exchange format to internal format */
	virtual std::string TranslateSymbol(const std::string &symbol) const
	{
		return symbol;
	}
	
	/*! \brief Translate symbol from internal format to exchange-specific format */
	virtual std::string TranslateSymbolToExchangeSpecific(const std::string &symbol) const
	{
		return symbol;
	}
	
	/*! \brief Start the connection - to be overridden by derived classes */
	virtual void Start() override { }
	
	/*! \brief Returns reference to message processor */
	CRYPTO::MessageProcessor &GetMessageProcessor()
	{
		return m_messageProcessor;
	}

protected:
	
	/*! \brief Creates internal websocket */
	virtual void CreateWebSocket();
	
	/*! \brief Receive data from WebSocket */
	virtual int ReceiveWebSocketData(Poco::Net::WebSocket *ws, char *buffer, size_t bufferSize, int &outFlags);
	
	/*! \brief Helper: sends a payload to websocket
	* @param payload: payload to be sent
	* @return: true in success
	* */
	virtual bool Send(const std::string &payload);
	
	/*! \brief Get currency pair from symbol */
	UTILS::CurrencyPair GetCurrencyPair(const std::string &symbol) const
	{
		return m_cpHash.GetCurrencyPair(symbol);
	}
	
	Settings m_settings;
	Logger m_logger; //Session logger..
	const ConnectionManager& m_connectionManager;
	
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
};

} // namespace CRYPTO
} // namespace CORE
