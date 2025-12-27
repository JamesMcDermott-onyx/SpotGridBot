#pragma once
#include "Utils/FixTypes.h"

namespace UTILS {
  class CurrencyPair;
}

namespace CORE {
  class ConnectionManager;

  enum class OrderSide { BUY, SELL };
  enum class OrderStatus { NEW, PARTIALLY_FILLED, FILLED, CANCELED, REJECTED };

  inline OrderStatus order_status(const std::string& status) {
    if (status == "NEW")               return OrderStatus::NEW;
    if (status == "PARTIALLY_FILLED")  return OrderStatus::PARTIALLY_FILLED;
    if (status == "FILLED")            return OrderStatus::FILLED;
    if (status == "CANCELED")          return OrderStatus::CANCELED;
    if (status == "REJECTED")          return OrderStatus::REJECTED;

    throw std::invalid_argument("Invalid OrderStatus string: " + status);
  }

  struct Order {
    std::string id;
    UTILS::Side side;
    double price;
    double quantity;      // original quantity
    double filled = 0.0;  // filled so far
    OrderStatus status = OrderStatus::NEW;
  };


  class IOrderManager {
  public:
    virtual ~IOrderManager() = default;

    virtual std::string PlaceLimitOrder(const UTILS::CurrencyPair cp, UTILS::Side side, double p, double q)=0;
    virtual bool CancelOrder(const UTILS::CurrencyPair cp, const std::string &orderId)=0;
    
    // REST-based: Query the exchange for order status (use for REST connections)
    virtual std::optional<Order> GetOrder(const UTILS::CurrencyPair cp, const std::string &orderId)=0;
    
    // WebSocket-based: Get locally cached order status (use for WebSocket connections with push updates)
    virtual std::optional<Order> GetOrderLocal(const std::string &orderId)=0;
    
    // Update order status from WebSocket push notifications
    virtual void UpdateOrder(const std::string &orderId, OrderStatus status, double filled)=0;
    
    // Sync order from external source (e.g., startup snapshot) - creates or updates order in cache
    virtual void SyncOrder(const std::string &orderId, UTILS::Side side, double price, double quantity, OrderStatus status, double filled)=0;
    
    virtual double GetBalance(const UTILS::Currency& currency)=0;
    virtual void SetBalance(const UTILS::Currency &currency, double balance)=0;
    virtual void PrintBalances(const UTILS::CurrencyPair cp)=0;
  };
}
