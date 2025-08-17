
#include <bits/stdc++.h>

#include "exchange.h"
#include "gridstrategy.h"
#include "IOrderManager.h"
#include "Utils/CurrencyPair.h"

using namespace std;
namespace STRATEGY {
  void GridStrategy::Start()
  {
    PlaceInitialGrid();
  }

  void GridStrategy::OnTicker()
  {
    CheckFilledOrders();
  }

  void GridStrategy::PlaceInitialGrid()
  {
    double base = m_cfg.m_basePrice;
    double step = m_cfg.m_stepPercent;

    for (int i=1;i<=m_cfg.m_levelsBelow;i++)
    {
      double price = base * (1.0 - step * i);
      string oid = m_orderManager->PlaceLimitOrder(m_cp, UTILS::Side::BUY, price, m_cfg.m_percentOrderQty);
      m_activeOrders.push_back(oid);
      m_orderMeta[oid] = {UTILS::Side::BUY, price, m_cfg.m_percentOrderQty};
    }

    for (int i=1;i<=m_cfg.m_levelsAbove;i++)
    {
      double price = base * (1.0 + step * i);
      string oid = m_orderManager->PlaceLimitOrder(m_cp, UTILS::Side::SELL, price, m_cfg.m_percentOrderQty);
      m_activeOrders.push_back(oid);
      m_orderMeta[oid] = {UTILS::Side::SELL, price, m_cfg.m_percentOrderQty};
    }
    Logger::info("Initial grid placed: " + to_string(m_activeOrders.size()) + " orders");
  }

  void GridStrategy::CheckFilledOrders()
  {
    vector<string> toRemove; // store orders to remove after iteration

    // Loop over all active orders we’re tracking
    for (auto &oid : m_activeOrders)
    {
        // Query the exchange/order manager for the latest status of this order
        auto maybe = m_orderManager->GetOrder(m_cp, oid);
        if (!maybe) continue; // If no data (order not found), skip

        Order order = *maybe; // Unwrap the optional

        //-----------------------------
        // CASE 1: Fully filled orders
        //-----------------------------
        if (order.status == OrderStatus::FILLED)
        {
            if (m_orderMeta[oid].side == UTILS::Side::BUY)
            {
                // Calculate the next sell price (one step above)
                double sellPrice = m_orderMeta[oid].price * (1.0 + m_cfg.m_stepPercent);

                // Check if holding too much 'base currency' before selling
                double base = m_orderManager->GetBalance(m_cp.BaseCCY());
                if (base > m_cfg.m_maxPosition + 1e-12)
                {
                    Logger::warn("Max 'base currency' position exceeded, not placing hedge sell");
                }
                else
                {
                    // Place sell order for the same quantity
                    string newId = m_orderManager->PlaceLimitOrder(m_cp, UTILS::Side::SELL, sellPrice, m_orderMeta[oid].qty);
                    // Track the new order
                    m_activeOrders.push_back(newId);
                    m_orderMeta[newId] = {UTILS::Side::SELL, sellPrice, m_orderMeta[oid].qty};
                }
            }
            else // It was a SELL order
            {
                // Calculate the next buy price (one step below)
                double buyPrice = m_orderMeta[oid].price * (1.0 - m_cfg.m_stepPercent);

                // Check if we have enough 'quote currency' to buy back
                double quote = m_orderManager->GetBalance(m_cp.QuoteCCY());
                double cost = buyPrice * m_orderMeta[oid].qty;
                if (quote + 1e-12 < cost)
                {
                    Logger::warn("Insufficient 'quote currency' to place re-buy");
                }
                else
                {
                    string newId = m_orderManager->PlaceLimitOrder(m_cp, UTILS::Side::BUY, buyPrice, m_orderMeta[oid].qty);
                    m_activeOrders.push_back(newId);
                    m_orderMeta[newId] = {UTILS::Side::BUY, buyPrice, m_orderMeta[oid].qty};
                }
            }
            // Mark the filled order for removal
            toRemove.push_back(oid);
        }

        //-----------------------------
        // CASE 2: Partially filled orders
        //-----------------------------
        else if (order.status == OrderStatus::PARTIALLY_FILLED)
        {
            // Get how much is currently filled
            double filled = order.filled;
            double knownFilled = m_knownFills[oid]; // what we’ve already processed

            // Check if there's new fill since last check
            if (filled - knownFilled > 1e-12)
            {
                double delta = filled - knownFilled; // amount newly filled
                m_knownFills[oid] = filled;          // update record

                Logger::info("Detected new partial fill " + oid + " delta=" + to_string(delta));

                // Place opposite hedge order for just the filled portion
                if (m_orderMeta[oid].side == UTILS::Side::BUY)
                {
                    double sellPrice = m_orderMeta[oid].price * (1.0 + m_cfg.m_stepPercent);
                    double btc = m_orderManager->GetBalance(m_cp.BaseCCY());

                    // Only place hedge if we’re under the max position
                    if (btc <= m_cfg.m_maxPosition + 1e-12)
                    {
                        string newId = m_orderManager->PlaceLimitOrder(m_cp, UTILS::Side::SELL, sellPrice, delta);
                        m_activeOrders.push_back(newId);
                        m_orderMeta[newId] = {UTILS::Side::SELL, sellPrice, delta};
                    }
                }
                else // partial SELL fill
                {
                    double buyPrice = m_orderMeta[oid].price * (1.0 - m_cfg.m_stepPercent);
                    double usdt = m_orderManager->GetBalance(m_cp.QuoteCCY());
                    double cost = buyPrice * delta;

                    if (usdt + 1e-12 >= cost)
                    {
                        string newId = m_orderManager->PlaceLimitOrder(m_cp, UTILS::Side::BUY, buyPrice, delta);
                        m_activeOrders.push_back(newId);
                        m_orderMeta[newId] = {UTILS::Side::BUY, buyPrice, delta};
                    }
                }
            }
        }

        //-----------------------------
        // CASE 3: Failed or canceled orders
        //-----------------------------
        else if (order.status == OrderStatus::REJECTED || order.status == OrderStatus::CANCELED)
        {
            toRemove.push_back(oid);
        }
    }

    //-----------------------------
    // Remove processed orders
    //-----------------------------
    for (auto &r : toRemove)
    {
        m_activeOrders.erase(remove(m_activeOrders.begin(), m_activeOrders.end(), r), m_activeOrders.end());
        m_orderMeta.erase(r);
        m_knownFills.erase(r);
    }
  }

  void GridStrategy::DumpStatus()
  {
    Logger::info("Active orders: " + to_string(m_activeOrders.size()));
    for (auto &oid : m_activeOrders)
    {
      auto m = m_orderMeta[oid];
      cout << " - " << oid << " " << (m.side==UTILS::Side::BUY?"BUY":"SELL") << " @"<< m.price << " qty="<<m.qty<<endl;
    }
  }
}
