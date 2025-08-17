#pragma once

namespace UTILS {
  class CurrencyPair;
}

namespace CORE {
  class ConnectionManager;

  class IOrderManager {
  public:
    virtual ~IOrderManager() = default;

    virtual std::string PlaceLimitOrder(const UTILS::CurrencyPair cp, UTILS::Side side, double p, double q)=0;
    virtual bool CancelOrder(const UTILS::CurrencyPair cp, const std::string &orderId)=0;
    virtual std::optional<Order> GetOrder(const UTILS::CurrencyPair cp, const std::string &orderId)=0;
    virtual double GetBalance(const std::string &asset)=0;
    virtual void SetBalances(double base, double quote)=0;
    virtual void PrintBalances(const UTILS::CurrencyPair cp)=0;
  };
}
