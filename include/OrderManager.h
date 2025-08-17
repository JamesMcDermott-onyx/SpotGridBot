#pragma once
#include <iostream>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "exchange.h"
#include "IOrderManager.h"
#include "Utils/CurrencyPair.h"
#include "Utils/FixTypes.h"

namespace CORE {

  class OrderManager : public CORE::IOrderManager {
  public:
    OrderManager(std::shared_ptr<CORE::ConnectionManager> connectionManager) : m_connectionManager(connectionManager)
    {
    }

    std::string PlaceLimitOrder(const UTILS::CurrencyPair cp, UTILS::Side side, double price, double quantity);
    bool CancelOrder(const UTILS::CurrencyPair cp, const std::string &orderId);
    std::optional<Order> GetOrder(const UTILS::CurrencyPair cp, const std::string &orderId);
    double GetBalance(const std::string &asset);
    void SetBalances(double base, double quote);
    void PrintBalances(UTILS::CurrencyPair cp);

    std::shared_ptr<CORE::ConnectionManager> GetConnectionManager() { return m_connectionManager; }

  private:
    std::mutex m_mutex;
    std::unordered_map<std::string,Order> m_orders;

    double m_price;
    double m_base = 10000.0;
    double m_quote = 0.0;

    std::shared_ptr<CORE::ConnectionManager> m_connectionManager;
  };
}
