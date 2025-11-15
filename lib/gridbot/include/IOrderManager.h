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
    virtual std::optional<Order> GetOrder(const UTILS::CurrencyPair cp, const std::string &orderId)=0;
    virtual double GetBalance(const UTILS::Currency& currency)=0;
    virtual void SetBalance(const UTILS::Currency &currency, double balance)=0;
    virtual void PrintBalances(const UTILS::CurrencyPair cp)=0;
  };
}
