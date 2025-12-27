#pragma once

#include <iomanip>
#include <unordered_map>
#include <map>
#include <memory>

#include "Utils/Logging.h"
#include "ConnectionBase.h"
#include "RestConnectionBase.h"
#include "OrderBook/OrderBook.h"
#include "Utils/ErrorHandler.h"

namespace CORE {
	class OrderManager;
}

namespace CORE {

class ConnectionManager final : UTILS::Logging, public UTILS::ErrorHandler
{
public:
	ConnectionManager(const std::string& configPath, const std::string& loggingPropsPath, std::shared_ptr<BOOK::OrderBook> orderBook);

	~ConnectionManager() {
		Disconnect();
	}
	
	/*! \brief Creates a session
     * @param numId: session id in the SQL database
     */
	void CreateSession(int64_t numId);
	
	UTILS::BoolResult Connect(const std::string &name);
	
	void Connect();
	
	void Disconnect();

	using TConnectionCreator = std::function<std::shared_ptr<CRYPTO::IConnection>(const CRYPTO::Settings &)>;
	
	TConnectionCreator FindConnectionCreator(const std::string &schema) const
	{
		auto factoryIter = m_connectionsFactory.find(schema);
		return factoryIter != m_connectionsFactory.cend() ? factoryIter->second : (TConnectionCreator) nullptr;
	}
	
	// Sessions settings <session id : settings>
	using TSettingsCollection = std::unordered_map<int64_t, CRYPTO::Settings>;
	
	const TSettingsCollection &GetSettingsCollection() const
	{
		return m_settingsCollection;
	}

	std::shared_ptr<BOOK::OrderBook> GetOrderBook() const {
		return m_orderBook;
	}

	// Load sessions settings..
	bool LoadConfig();
	bool LoadConfig(const UTILS::XmlDocPtr &pDoc);

	template <typename TConnection>
	void RegisterConnectionCreator(const std::string &schema)
	{
		m_connectionsFactory.emplace(schema, [this](const CRYPTO::Settings &setting)
									{
										return std::make_shared<TConnection>(setting, m_loggingPropsPath, *this);
									});
	}

	std::shared_ptr<RESTAPI::RestConnectionBase> OrderConnection() {
		return std::static_pointer_cast<RESTAPI::RestConnectionBase>(m_connections[m_orderConnection]);
	}

	void SetOrderManager(std::shared_ptr<OrderManager> orderManager) {
		m_orderManager = orderManager;
	}

	std::shared_ptr<OrderManager> GetOrderManager() const {
		return m_orderManager;
	}
private:
	
	TSettingsCollection m_settingsCollection;
	
	std::map<std::string, TConnectionCreator> m_connectionsFactory;
	std::string m_configPath;
	std::string m_loggingPropsPath;

	using TSessionsInstruments = std::map<std::string, std::string>; // <session name: instruments separated by comma>
	TSessionsInstruments m_sessionsInstruments;

	std::map<std::string, std::shared_ptr<CRYPTO::IConnection>> m_connections;
	std::shared_ptr<BOOK::OrderBook> m_orderBook;
	std::string m_orderConnection; //Name of order connection
	std::shared_ptr<OrderManager> m_orderManager; // Reference to OrderManager for WebSocket order updates
}; // ConnectionManager

} // namespace CORE
