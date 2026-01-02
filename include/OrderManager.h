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
    
    // Get all cached orders (for startup sync)
    std::unordered_map<std::string, Order> GetAllOrders() const { std::lock_guard<std::mutex> lock(m_mutex); return m_orders; }
    
    double GetBalance(const UTILS::Currency &currency);
    void SetBalance(const UTILS::Currency &currency, double balance);
    void InitializeBalances();
    void LoadOpenOrders(const UTILS::CurrencyPair &cp);
    double GetCurrentMarketPrice(const UTILS::CurrencyPair &cp);
    void PrintBalances(UTILS::CurrencyPair cp);
    void PrintAllBalances();

    std::shared_ptr<CORE::ConnectionManager> GetConnectionManager() { return m_connectionManager; }

  private:
    mutable std::mutex m_mutex;
    std::unordered_map<std::string,Order> m_orders;

    std::unordered_map<UTILS::Currency, double> m_balance; //the balance of currencies

    std::shared_ptr<CORE::ConnectionManager> m_connectionManager;
  };
}
