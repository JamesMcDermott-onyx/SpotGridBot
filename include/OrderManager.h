#pragma once
#include <iostream>
#include <memory>
#include <mutex>
#include <unordered_map>

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
    
    // REST-based: Query the exchange for order status (use for REST connections)
    std::optional<Order> GetOrder(const UTILS::CurrencyPair cp, const std::string &orderId);
    
    // WebSocket-based: Get locally cached order status (use for WebSocket connections with push updates)
    std::optional<Order> GetOrderLocal(const std::string &orderId);
    
    // Update order status from WebSocket push notifications
    void UpdateOrder(const std::string &orderId, OrderStatus status, double filled);
    
    // Sync order from external source (e.g., startup snapshot) - creates or updates order in cache
    void SyncOrder(const std::string &orderId, UTILS::Side side, double price, double quantity, OrderStatus status, double filled);
    
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
