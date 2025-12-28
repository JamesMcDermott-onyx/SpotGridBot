#include "OrderManager.h"

#include "ConnectionManager.h"
#include "Utils/CurrencyPair.h"
#include "RestConnectionBase.h"
#include "coinbase/ConnectionORDWS.h"

namespace CORE {
    std::string OrderManager::PlaceLimitOrder(const UTILS::CurrencyPair cp, UTILS::Side side, double price, double quantity)
    {
        std::lock_guard<std::mutex> g(m_mutex);

        std::string responseStr;
        auto connection = m_connectionManager->OrderConnection();
        
        // Try WebSocket connection first
        if (auto wsConn = std::dynamic_pointer_cast<COINBASE::ConnectionORDWS>(connection))
        {
            responseStr = wsConn->SendOrder(cp, side, RESTAPI::EOrderType::Limit, UTILS::TimeInForce::GTC, price, quantity);
        }
        // Fallback to REST connection
        else if (auto restConn = std::dynamic_pointer_cast<RESTAPI::RestConnectionBase>(connection))
        {
            responseStr = restConn->SendOrder(cp, side, RESTAPI::EOrderType::Limit, UTILS::TimeInForce::GTC, price, quantity);
        }
        else
        {
            poco_error(logger(), "Unknown order connection type");
            return "";
        }

        CRYPTO::JSONDocument response(responseStr);

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

        auto connection = m_connectionManager->OrderConnection();
        
        // Try WebSocket connection first
        if (auto wsConn = std::dynamic_pointer_cast<COINBASE::ConnectionORDWS>(connection))
        {
            wsConn->CancelOrder(cp, orderId);
        }
        // Fallback to REST connection
        else if (auto restConn = std::dynamic_pointer_cast<RESTAPI::RestConnectionBase>(connection))
        {
            restConn->CancelOrder(cp, orderId);
        }
        
        poco_information_f1(logger(), "Canceled order %s", orderId);

        return true;
    }

    std::optional<Order> OrderManager::GetOrder(const UTILS::CurrencyPair cp, const std::string &orderId)
    {
        std::lock_guard<std::mutex> g(m_mutex);

        if (!m_orders.count(orderId))
            return {};

        auto connection = m_connectionManager->OrderConnection();
        
        // For REST connections, query the exchange
        if (auto restConn = std::dynamic_pointer_cast<RESTAPI::RestConnectionBase>(connection))
        {
            CRYPTO::JSONDocument response(restConn->QueryOrder(cp, orderId));

            if (response.GetValue<std::string>("success").compare("true")==0)
            {
                m_orders[orderId].status = order_status(response.GetValue<std::string>("status"));
                m_orders[orderId].filled = response.GetValue<double>("filled_size");

                return m_orders[orderId];
            }
        }
        // For WebSocket connections, use local cache (WebSocket pushes updates automatically)
        else if (auto wsConn = std::dynamic_pointer_cast<COINBASE::ConnectionORDWS>(connection))
        {
            poco_warning(logger(), "GetOrder() called with WebSocket connection - use GetOrderLocal() instead for better performance");
            return m_orders[orderId];
        }

        return {};
    }

    std::optional<Order> OrderManager::GetOrderLocal(const std::string &orderId)
    {
        std::lock_guard<std::mutex> g(m_mutex);

        if (!m_orders.count(orderId))
            return {};

        return m_orders[orderId];
    }

    void OrderManager::UpdateOrder(const std::string &orderId, OrderStatus status, double filled)
    {
        std::lock_guard<std::mutex> g(m_mutex);

        if (!m_orders.count(orderId))
        {
            poco_warning_f1(logger(), "UpdateOrder called for unknown order: %s", orderId);
            return;
        }

        m_orders[orderId].status = status;
        m_orders[orderId].filled = filled;

        poco_information_f3(logger(), "Order updated: %s, status=%d, filled=%f", 
            orderId, static_cast<int>(status), filled);
    }

    void OrderManager::SyncOrder(const std::string &orderId, UTILS::Side side, double price, double quantity, OrderStatus status, double filled)
    {
        std::lock_guard<std::mutex> g(m_mutex);

        // Check if order already exists in cache
        if (m_orders.count(orderId))
        {
            // Update existing order
            m_orders[orderId].status = status;
            m_orders[orderId].filled = filled;
            poco_information_f1(logger(), "Order synced (updated): %s", orderId);
        }
        else
        {
            // Add new order to cache
            Order order;
            order.id = orderId;
            order.side = side;
            order.price = price;
            order.quantity = quantity;
            order.status = status;
            order.filled = filled;
            
            m_orders[orderId] = order;
            poco_information_f1(logger(), "Order synced (new): %s", orderId);
        }
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