
#include <Poco/Logger.h>

#include "Utils/ContextBase.h"
#include "Utils/AppHandler.h"

#include "Options.h"

#include "ConnectionManager.h"
#include "Poco/Net/HTTPStreamFactory.h"
#include "Poco/Net/HTTPSStreamFactory.h"
#include "Poco/Util/Application.h"
#include "Poco/URI.h"

#include "exchange.h"
#include "GridConfig.h"
#include "gridstrategy.h"
#include "OrderManager.h"

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
        //m_connectionManager->Connect(); //connect market data and populate orderbook.

        auto m_orderManager = make_shared<OrderManager>(m_connectionManager);

        sleep(2); //need to implement wait

        STRATEGY::GridStrategy strat(m_orderManager, options.ConfigPath());
        strat.Start();

        //strat.OnTicker();

        poco_information(logger, "SpotGridBot has started - press <enter> to exit ..");
        std::cin.get();
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
