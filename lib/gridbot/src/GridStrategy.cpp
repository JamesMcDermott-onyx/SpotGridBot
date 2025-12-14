
#include <bits/stdc++.h>

#include "GridStrategy.h"

#include <Poco/Logger.h>

#include "IOrderManager.h"
#include "Utils/CurrencyPair.h"
#include "Utils/Round.h"

using namespace std;
namespace STRATEGY {
  void GridStrategy::Start()
  {
    double base = m_cfg.m_basePrice;
    double step = m_cfg.m_stepPercent;

    for (int i=1;i<=m_cfg.m_levelsBelow;i++)
    {
      double price = base * (1.0 - step * i);
      string orderId = m_orderManager->PlaceLimitOrder(m_cp, UTILS::Side::BUY, price, m_cfg.m_percentOrderQty);
      m_activeOrders.push_back(orderId);
      m_orderDetails[orderId] = {UTILS::Side::BUY, price, m_cfg.m_percentOrderQty};
    }

    for (int i=1;i<=m_cfg.m_levelsAbove;i++)
    {
      double price = base * (1.0 + step * i);
      string orderId = m_orderManager->PlaceLimitOrder(m_cp, UTILS::Side::SELL, price, m_cfg.m_percentOrderQty);
      m_activeOrders.push_back(orderId);
      m_orderDetails[orderId] = {UTILS::Side::SELL, price, m_cfg.m_percentOrderQty};
    }

    poco_information_f1(logger(), "Initial grid placed: %s orders", to_string(m_activeOrders.size()));
  }

  void GridStrategy::CheckFilledOrders()
  {
    vector<string> toRemove; // store orders to remove after iteration

    poco_information(logger(), "Checking filled state...");

    // Loop over all active orders we’re tracking
    for (auto &orderId : m_activeOrders)
    {
        // Query the exchange/order manager for the latest status of this order
        auto maybe = m_orderManager->GetOrder(m_cp, orderId);
        if (!maybe) {
            continue; // If no data (order not found), skip
        }

        CORE::Order order = *maybe; // Unwrap the optional

        //-----------------------------
        // CASE 1: Fully filled orders
        //-----------------------------
        if (order.status == CORE::OrderStatus::FILLED)
        {
            if (m_orderDetails[orderId].side == UTILS::Side::BUY)
            {
                // Calculate the next sell price (one step above)
                double sellPrice = m_orderDetails[orderId].price * (1.0 + m_cfg.m_stepPercent);

                // Check if holding too much 'base currency' before selling
                double base = m_orderManager->GetBalance(m_cp.BaseCCY());
                if (base > UTILS::Round<double>(m_cfg.m_maxPosition))
                {
                    poco_warning(logger(), "Max 'base currency' position exceeded, not placing hedge sell");
                }
                else
                {
                    // Place sell order for the same quantity
                    string newId = m_orderManager->PlaceLimitOrder(m_cp, UTILS::Side::SELL, sellPrice, m_orderDetails[orderId].qty);
                    // Track the new order
                    m_activeOrders.push_back(newId);
                    m_orderDetails[newId] = {UTILS::Side::SELL, sellPrice, m_orderDetails[orderId].qty};
                }
            }
            else // It was a SELL order
            {
                // Calculate the next buy price (one step below)
                double buyPrice = m_orderDetails[orderId].price * (1.0 - m_cfg.m_stepPercent);

                // Check if we have enough 'quote currency' to buy back
                double quote = m_orderManager->GetBalance(m_cp.QuoteCCY());
                double cost = buyPrice * m_orderDetails[orderId].qty;
                if (UTILS::Round<double>(quote) < cost)
                {
                    poco_warning(logger(), "Insufficient 'quote currency' to place re-buy");
                }
                else
                {
                    string newId = m_orderManager->PlaceLimitOrder(m_cp, UTILS::Side::BUY, buyPrice, m_orderDetails[orderId].qty);
                    m_activeOrders.push_back(newId);
                    m_orderDetails[newId] = {UTILS::Side::BUY, buyPrice, m_orderDetails[orderId].qty};
                }
            }
            // Mark the filled order for removal
            toRemove.push_back(orderId);
        }

        //-----------------------------
        // CASE 2: Partially filled orders
        //-----------------------------
        else if (order.status == CORE::OrderStatus::PARTIALLY_FILLED)
        {
            // Get how much is currently filled
            double filled = order.filled;
            double knownFilled = m_knownFills[orderId]; // what we’ve already processed

            // Check if there's new fill since last check
            if ( UTILS::Round<double>(filled - knownFilled) )
            {
                double delta = filled - knownFilled; // amount newly filled
                m_knownFills[orderId] = filled;          // update record

                poco_information_f2(logger(), "Detected new partial fill  %s delta=%s", orderId, to_string(delta).c_str());

                // Place opposite hedge order for just the filled portion
                if (m_orderDetails[orderId].side == UTILS::Side::BUY)
                {
                    double sellPrice = m_orderDetails[orderId].price * (1.0 + m_cfg.m_stepPercent);
                    double base = m_orderManager->GetBalance(m_cp.BaseCCY());

                    // Only place hedge if we’re under the max position
                    if (base <= UTILS::Round<double>(m_cfg.m_maxPosition))
                    {
                        string newId = m_orderManager->PlaceLimitOrder(m_cp, UTILS::Side::SELL, sellPrice, delta);
                        m_activeOrders.push_back(newId);
                        m_orderDetails[newId] = {UTILS::Side::SELL, sellPrice, delta};
                    }
                    else
                    {
                        poco_warning(logger(), "Exceeded max position - not placing sell order");
                    }
                }
                else // partial SELL fill
                {
                    double buyPrice = m_orderDetails[orderId].price * (1.0 - m_cfg.m_stepPercent);
                    double quote = m_orderManager->GetBalance(m_cp.QuoteCCY());
                    double cost = buyPrice * delta;

                    if (UTILS::Round<double>(quote) >= cost)
                    {
                        string newId = m_orderManager->PlaceLimitOrder(m_cp, UTILS::Side::BUY, buyPrice, delta);
                        m_activeOrders.push_back(newId);
                        m_orderDetails[newId] = {UTILS::Side::BUY, buyPrice, delta};
                    }
                }
            }
        }

        //-----------------------------
        // CASE 3: Failed or canceled orders
        //-----------------------------
        else if (order.status == CORE::OrderStatus::REJECTED || order.status == CORE::OrderStatus::CANCELED)
        {
            toRemove.push_back(orderId);
        }
    }

    //-----------------------------
    // Remove processed orders
    //-----------------------------
    for (auto &r : toRemove)
    {
        m_activeOrders.erase(remove(m_activeOrders.begin(), m_activeOrders.end(), r), m_activeOrders.end());
        m_orderDetails.erase(r);
        m_knownFills.erase(r);
    }
  }

  void GridStrategy::PrintStatus()
  {
    poco_information_f1(logger(), "Active orders: %s",to_string(m_activeOrders.size()));
    for (auto &orderId : m_activeOrders)
    {
        auto m = m_orderDetails[orderId];
        cout << " - " << orderId << " " << (m.side==UTILS::Side::BUY ? "BUY" : "SELL") << " @" << m.price << " qty="<<m.qty<<endl;
    }
  }
}
