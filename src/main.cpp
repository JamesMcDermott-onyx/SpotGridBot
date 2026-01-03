#include <Poco/Logger.h>

#include "Utils/ContextBase.h"
#include "Utils/AppHandler.h"

#include "Options.h"

#include "ConnectionManager.h"
#include "Poco/Net/HTTPStreamFactory.h"
#include "Poco/Net/HTTPSStreamFactory.h"
#include "Poco/Util/Application.h"
#include "Poco/URI.h"

#include "GridConfig.h"
#include "GridStrategy.h"
#include "OrderManager.h"
#include "coinbase/ConnectionORD.h"

using namespace CORE;
using namespace UTILS;
using namespace std;

//------------------------------------------------------------------------------
int main(int argc, char** argv)
{
    Poco::Logger& logger { Poco::Logger::get("main") };

    try
    {
        Poco::Net::initializeSSL();

        Poco::Net::HTTPStreamFactory::registerFactory();
        Poco::Net::HTTPSStreamFactory::registerFactory();

        CurrencyPair::InitializeCurrencyConfigs();

        auto m_orderBook = std::make_shared<BOOK::OrderBook>();

        Options options(argc, argv);
        auto m_connectionManager = make_shared<ConnectionManager>(options.ConfigPath(), options.LoggingPropsPath(), m_orderBook);
        auto m_orderManager = make_shared<OrderManager>(m_connectionManager);
        
        // Set OrderManager reference in ConnectionManager so WebSocket connections can push order updates
        m_connectionManager->SetOrderManager(m_orderManager);

        STRATEGY::GridStrategy strat(m_orderManager, options.ConfigPath());

        m_orderBook->Initialise([&strat]() { strat.CheckFilledOrders(); });

        m_connectionManager->Connect(); //connect market data and populate orderbook.
        
        // Initialize account balances from exchange
        m_orderManager->InitializeBalances();
        m_orderManager->PrintAllBalances();

        // Load existing open orders from exchange for all configured instruments
        // Note: LoadOpenOrders will be called for each instrument in the grid
        m_orderManager->LoadOpenOrders(UTILS::CurrencyPair("BTC/USDC"));
        // Add more instruments as needed, or make this dynamic based on grid configs
        
        // Load existing orders into strategy before starting
        strat.LoadExistingOrders();

        // List all available products from Coinbase
        // auto restConn = std::dynamic_pointer_cast<COINBASE::ConnectionORD>(m_connectionManager->OrderConnection());
        // if (restConn) {
        //     std::string productsResp = restConn->ListProducts();
        //     restConn->PrettyPrintProducts(productsResp);
        //     // Optionally, still send the test order for debugging
        //     std::string testOrderResp = restConn->SendTestLimitOrder();
        //     poco_information(logger, Poco::format("SendTestLimitOrder returned: %s", testOrderResp));
        // } else {
        //     poco_warning(logger, "OrderConnection is not a COINBASE::ConnectionORD instance");
        // }
        
        // Start strategy after connections are established
        strat.Start();

        poco_information(logger, "SpotGridBot has started - press <enter> to exit ..");
        std::cin.get();

        m_connectionManager->Disconnect();
    }
    catch (Poco::Exception& e) // explicitly catch poco exceptions
    {
        std::cerr << "Poco exception: " << e.message() << std::endl;
        poco_fatal(logger, e.message());
    }

    Poco::Net::uninitializeSSL();
   	poco_information(logger, "SpotGridBot has stopped successfully.");

    return 0;
}
