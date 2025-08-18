#include "OrderManager.h"

#include "ConnectionManager.h"
#include "Utils/CurrencyPair.h"

namespace CORE {
    std::string OrderManager::PlaceLimitOrder(const UTILS::CurrencyPair cp, UTILS::Side side, double price, double quantity)
    {
        std::lock_guard<std::mutex> g(m_mutex);

        CRYPTO::JSONDocument response(m_connectionManager->OrderConnection()->SendOrder(cp, side, RESTAPI::EOrderType::Limit, UTILS::TimeInForce::GTC, price, quantity));

        if (response.GetValue<std::string>("success").compare("true")==0)
        {
            Order order;
            order.side = side;
            order.price = price;
            order.quantity = quantity;
            order.filled = 0.0;
            order.status = OrderStatus::NEW;

            CRYPTO::JSONDocument sr(response.GetValue<std::string>("success_response"));
            order.id = sr.GetValue<std::string>("order_id");
            m_orders[order.id] = order;

            poco_information_f1(logger(), "Placed order %s" ,  order.id + " " + (side==UTILS::Side::BUY ? "BUY" : "SELL") + " @" + std::to_string(price) + " qty=" + std::to_string(quantity));

            return order.id;
        }

        return "";
    }

    bool OrderManager::CancelOrder(const UTILS::CurrencyPair cp, const std::string &orderId)
    {
        std::lock_guard<std::mutex> g(m_mutex);

        if (!m_orders.count(orderId))
            return false;

        auto &order = m_orders[orderId];
        if (order.status == OrderStatus::FILLED || order.status == OrderStatus::CANCELED)
            return false;

        order.status = OrderStatus::CANCELED;
        m_orders.erase(orderId);

        m_connectionManager->OrderConnection()->CancelOrder(cp, orderId);
        poco_information_f1(logger(), "Canceled order %s", orderId);

        return true;
    }

    std::optional<Order> OrderManager::GetOrder(const UTILS::CurrencyPair cp, const std::string &orderId)
    {
        std::lock_guard<std::mutex> g(m_mutex);

        if (!m_orders.count(orderId))
            return {};

        return m_orders[orderId];
    }

    double OrderManager::GetBalance(const UTILS::Currency &currency)
    {
        std::lock_guard<std::mutex> g(m_mutex);
        return m_balance[currency];
    }

    void OrderManager::SetBalance(const UTILS::Currency &currency, double balance)
    {
        std::lock_guard<std::mutex> g(m_mutex);
        m_balance[currency]=balance;
    }

    void OrderManager::PrintBalances(UTILS::CurrencyPair cp)
    {
        std::lock_guard<std::mutex> g(m_mutex);
        std::cout << "Balances: " << cp.BaseCCY().ToString() << "  " << m_balance[cp.BaseCCY()] << " " << cp.QuoteCCY().ToString() << " " << m_balance[cp.QuoteCCY()] << std::endl;
    }
}