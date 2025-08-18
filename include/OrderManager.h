#pragma once
#include <iostream>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "exchange.h"
#include "IOrderManager.h"
#include "Utils/CurrencyPair.h"
#include "Utils/ErrorHandler.h"
#include "Utils/FixTypes.h"
#include "Utils/Logging.h"

namespace CORE {

  class OrderManager : public CORE::IOrderManager, UTILS::Logging, public UTILS::ErrorHandler {
  public:
    OrderManager(std::shared_ptr<CORE::ConnectionManager> connectionManager) : Logging("OrderManager"), ErrorHandler(pLogger()), m_connectionManager(connectionManager)
    {
    }

    std::string PlaceLimitOrder(const UTILS::CurrencyPair cp, UTILS::Side side, double price, double quantity);
    bool CancelOrder(const UTILS::CurrencyPair cp, const std::string &orderId);
    std::optional<Order> GetOrder(const UTILS::CurrencyPair cp, const std::string &orderId);
    double GetBalance(const UTILS::Currency &currency);
    void SetBalance(const UTILS::Currency &currency, double balance);
    void PrintBalances(UTILS::CurrencyPair cp);

    std::shared_ptr<CORE::ConnectionManager> GetConnectionManager() { return m_connectionManager; }

  private:
    std::mutex m_mutex;
    std::unordered_map<std::string,Order> m_orders;

    std::vector<double> m_balance; //the balance off the base and quote currencies

    std::shared_ptr<CORE::ConnectionManager> m_connectionManager;
  };
}
