
#include <bits/stdc++.h>

#include "GridStrategy.h"

#include <Poco/Logger.h>

#include "IOrderManager.h"
#include "Utils/CurrencyPair.h"
#include "Utils/Round.h"

using namespace std;
namespace STRATEGY {

  //==============================================================================
  // GridBot Implementation (single grid instance)
  //==============================================================================
  
  GridBot::GridBot(const GridConfigData& cfg, std::shared_ptr<CORE::IOrderManager> orderManager)
    : m_cfg(cfg), m_orderManager(orderManager), m_cp(cfg.instrument), m_logger(Poco::Logger::get("GridBot[" + cfg.name + "]"))
  {
    poco_information_f2(m_logger, "Created GridBot '%s' for instrument %s", m_cfg.name, m_cfg.instrument);
  }

  void GridBot::LoadExistingOrders()
  {
    poco_information(m_logger, "Loading existing orders from exchange...");
    
    // Get all orders that were synced from the exchange
    auto allOrders = m_orderManager->GetAllOrders();
    
    for (const auto& [orderId, order] : allOrders)
    {
      // Only track NEW (OPEN) orders that match our instrument
      if (order.status == CORE::OrderStatus::NEW)
      {
        m_activeOrders.push_back(orderId);
        m_orderDetails[orderId] = {order.side, order.price, order.quantity};
        
        poco_information_f4(m_logger, "Loaded order %s: %s @ %f qty=%f", 
                           orderId.c_str(),
                           order.side == UTILS::Side::BUY ? "BUY" : "SELL",
                           order.price,
                           order.quantity);
      }
    }
    
    poco_information_f1(m_logger, "Loaded %s existing orders into grid", to_string(m_activeOrders.size()));
  }

  void GridBot::Start()
  {
    // If create_position is false, skip placing new orders (intra-day restart)
    if (!m_cfg.createPosition)
    {
      poco_information(m_logger, "create_position=false, skipping new order placement (using existing orders only)");
      return;
    }
    
    double base = m_cfg.basePrice;
    double step = m_cfg.stepPercent;
    double tolerance = 0.01; // 1% price tolerance for matching existing orders

    // If base_price is 0, fetch current market price dynamically
    if (base == 0.0)
    {
      poco_information(m_logger, "Base price is 0 - fetching current market price...");
      base = m_orderManager->GetCurrentMarketPrice(m_cp);
      
      if (base == 0.0)
      {
        poco_error(m_logger, "Failed to fetch current market price - cannot place orders!");
        return;
      }
      
      poco_information_f1(m_logger, "Using dynamic base price: %f", base);
    }

    // Build a map of expected grid prices
    std::map<double, bool> expectedBuyLevels;
    std::map<double, bool> expectedSellLevels;

    for (int i=1;i<=m_cfg.levelsBelow;i++)
    {
      double price = base * (1.0 - step * i);
      expectedBuyLevels[price] = false; // not yet placed
    }

    for (int i=1;i<=m_cfg.levelsAbove;i++)
    {
      double price = base * (1.0 + step * i);
      expectedSellLevels[price] = false; // not yet placed
    }

    // Check if we already have orders tracked (from previous session or WebSocket sync)
    for (const auto& orderId : m_activeOrders)
    {
      if (m_orderDetails.find(orderId) != m_orderDetails.end())
      {
        const auto& details = m_orderDetails[orderId];
        
        // Check if this matches a BUY grid level
        if (details.side == UTILS::Side::BUY)
        {
          for (auto& [expectedPrice, placed] : expectedBuyLevels)
          {
            if (abs(details.price - expectedPrice) / expectedPrice < tolerance)
            {
              placed = true;
              poco_information_f2(m_logger, "Found existing BUY order %s at %f", orderId.c_str(), details.price);
              break;
            }
          }
        }
        // Check if this matches a SELL grid level
        else if (details.side == UTILS::Side::SELL)
        {
          for (auto& [expectedPrice, placed] : expectedSellLevels)
          {
            if (abs(details.price - expectedPrice) / expectedPrice < tolerance)
            {
              placed = true;
              poco_information_f2(m_logger, "Found existing SELL order %s at %f", orderId.c_str(), details.price);
              break;
            }
          }
        }
      }
    }

    // Place missing BUY orders
    int newOrdersPlaced = 0;
    for (const auto& [price, placed] : expectedBuyLevels)
    {
      if (!placed)
      {
        string orderId = m_orderManager->PlaceLimitOrder(m_cp, UTILS::Side::BUY, price, m_cfg.percentOrderQty);
        m_activeOrders.push_back(orderId);
        m_orderDetails[orderId] = {UTILS::Side::BUY, price, m_cfg.percentOrderQty};
        newOrdersPlaced++;
        poco_information_f2(m_logger, "Placed new BUY order %s at %f", orderId, price);
      }
    }

    // Place missing SELL orders
    for (const auto& [price, placed] : expectedSellLevels)
    {
      if (!placed)
      {
        string orderId = m_orderManager->PlaceLimitOrder(m_cp, UTILS::Side::SELL, price, m_cfg.percentOrderQty);
        m_activeOrders.push_back(orderId);
        m_orderDetails[orderId] = {UTILS::Side::SELL, price, m_cfg.percentOrderQty};
        newOrdersPlaced++;
        poco_information_f2(m_logger, "Placed new SELL order %s at %f", orderId, price);
      }
    }

    poco_information_f3(m_logger, "Grid initialization complete: %s existing orders, %s new orders placed, %s total", 
                       to_string(m_activeOrders.size() - newOrdersPlaced), to_string(newOrdersPlaced), to_string(m_activeOrders.size()));
  }

  void GridBot::CheckFilledOrders()
  {
    vector<string> toRemove; // store orders to remove after iteration


    // Loop over all active orders we’re tracking
    for (auto &orderId : m_activeOrders)
    {
        // Get the locally cached order status (updated by WebSocket push notifications)
        // Note: With WebSocket connections, order status is pushed to OrderManager automatically,
        // so we don't need to query the exchange - just read the cached state
        auto maybe = m_orderManager->GetOrderLocal(orderId);
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
                double sellPrice = m_orderDetails[orderId].price * (1.0 + m_cfg.stepPercent);

                // Check if holding too much 'base currency' before selling
                double base = m_orderManager->GetBalance(m_cp.BaseCCY());
                if (base > UTILS::Round<double>(m_cfg.maxPosition))
                {
                    poco_warning(m_logger, "Max 'base currency' position exceeded, not placing hedge sell");
                }
                else
                {
                    // Place sell order for the same quantity
                    string newId = m_orderManager->PlaceLimitOrder(m_cp, UTILS::Side::SELL, sellPrice, m_orderDetails[orderId].qty);
                    // Track the new order
                    m_activeOrders.push_back(newId);
                    m_orderDetails[newId] = {UTILS::Side::SELL, sellPrice, m_orderDetails[orderId].qty};
                    
                    // Log the fill and expected profit
                    double profit = m_orderDetails[orderId].price * m_cfg.stepPercent * m_orderDetails[orderId].qty;
                    poco_information_f4(m_logger, "BUY order %s filled at %f, placed hedge SELL at %f, expected profit %f", orderId.c_str(), m_orderDetails[orderId].price, sellPrice, profit);
                }
            }
            else // It was a SELL order
            {
                // Calculate the next buy price (one step below)
                double buyPrice = m_orderDetails[orderId].price * (1.0 - m_cfg.stepPercent);

                // Check if we have enough 'quote currency' to buy back
                double quote = m_orderManager->GetBalance(m_cp.QuoteCCY());
                double cost = buyPrice * m_orderDetails[orderId].qty;
                if (UTILS::Round<double>(quote) < cost)
                {
                    poco_warning(m_logger, "Insufficient 'quote currency' to place re-buy");
                }
                else
                {
                    string newId = m_orderManager->PlaceLimitOrder(m_cp, UTILS::Side::BUY, buyPrice, m_orderDetails[orderId].qty);
                    m_activeOrders.push_back(newId);
                    m_orderDetails[newId] = {UTILS::Side::BUY, buyPrice, m_orderDetails[orderId].qty};
                    
                    // Log the fill and expected profit
                    double profit = m_orderDetails[orderId].price * m_cfg.stepPercent * m_orderDetails[orderId].qty;
                    poco_information_f4(m_logger, "SELL order %s filled at %f, placed hedge BUY at %f, expected profit %f", orderId.c_str(), m_orderDetails[orderId].price, buyPrice, profit);
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

                poco_information_f2(m_logger, "Detected new partial fill  %s delta=%s", orderId, to_string(delta).c_str());

                // Place opposite hedge order for just the filled portion
                if (m_orderDetails[orderId].side == UTILS::Side::BUY)
                {
                    double sellPrice = m_orderDetails[orderId].price * (1.0 + m_cfg.stepPercent);
                    double base = m_orderManager->GetBalance(m_cp.BaseCCY());

                    // Only place hedge if we’re under the max position
                    if (base <= UTILS::Round<double>(m_cfg.maxPosition))
                    {
                        string newId = m_orderManager->PlaceLimitOrder(m_cp, UTILS::Side::SELL, sellPrice, delta);
                        m_activeOrders.push_back(newId);
                        m_orderDetails[newId] = {UTILS::Side::SELL, sellPrice, delta};                        
                        // Log the partial fill and expected profit
                        double profit = m_orderDetails[orderId].price * m_cfg.stepPercent * delta;
                        poco_information_f4(m_logger, "Partial BUY fill %s delta=%f, placed hedge SELL at %f, expected profit %f", orderId.c_str(), delta, sellPrice, profit);                    }
                    else
                    {
                        poco_warning(m_logger, "Exceeded max position - not placing sell order");
                    }
                }
                else // partial SELL fill
                {
                    double buyPrice = m_orderDetails[orderId].price * (1.0 - m_cfg.stepPercent);
                    double quote = m_orderManager->GetBalance(m_cp.QuoteCCY());
                    double cost = buyPrice * delta;

                    if (UTILS::Round<double>(quote) >= cost)
                    {
                        string newId = m_orderManager->PlaceLimitOrder(m_cp, UTILS::Side::BUY, buyPrice, delta);
                        m_activeOrders.push_back(newId);
                        m_orderDetails[newId] = {UTILS::Side::BUY, buyPrice, delta};
                        
                        // Log the partial fill and expected profit
                        double profit = m_orderDetails[orderId].price * m_cfg.stepPercent * delta;
                        poco_information_f4(m_logger, "Partial SELL fill %s delta=%f, placed hedge BUY at %f, expected profit %f", orderId.c_str(), delta, buyPrice, profit);
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

  void GridBot::PrintStatus()
  {
    poco_information_f1(m_logger, "Active orders: %s",to_string(m_activeOrders.size()));
    for (auto &orderId : m_activeOrders)
    {
        auto m = m_orderDetails[orderId];
        cout << " - " << orderId << " " << (m.side==UTILS::Side::BUY ? "BUY" : "SELL") << " @" << m.price << " qty="<<m.qty<<endl;
    }
  }

  //==============================================================================
  // GridStrategy Implementation (multi-grid coordinator)
  //==============================================================================
  
  GridStrategy::GridStrategy(std::shared_ptr<CORE::IOrderManager> orderManager, const std::string& path)
    : Logging("GridStrategy"), ErrorHandler(pLogger()), m_orderManager(orderManager), m_cfgLoader(path)
  {
    // Create a GridBot instance for each configuration
    const auto& configs = m_cfgLoader.GetGridConfigs();
    
    if (configs.empty())
    {
      poco_error(logger(), "No grid configurations found!");
      return;
    }
    
    poco_information_f1(logger(), "Initializing %s grid bots", to_string(configs.size()));
    
    for (const auto& cfg : configs)
    {
      m_gridBots.push_back(std::make_unique<GridBot>(cfg, m_orderManager));
    }
  }
  
  void GridStrategy::LoadExistingOrders()
  {
    poco_information(logger(), "Loading existing orders for all grids...");
    for (auto& bot : m_gridBots)
    {
      bot->LoadExistingOrders();
    }
  }
  
  void GridStrategy::Start()
  {
    poco_information(logger(), "Starting all grid bots...");
    for (auto& bot : m_gridBots)
    {
      bot->Start();
    }
    poco_information(logger(), "All grid bots started successfully");
  }
  
  void GridStrategy::CheckFilledOrders()
  {
    for (auto& bot : m_gridBots)
    {
      bot->CheckFilledOrders();
    }
  }
  
  void GridStrategy::PrintStatus()
  {
    poco_information(logger(), "=== Grid Strategy Status ===");
    for (auto& bot : m_gridBots)
    {
      bot->PrintStatus();
    }
  }
  
  std::vector<std::string> GridStrategy::GetInstruments() const
  {
    std::vector<std::string> instruments;
    for (const auto& bot : m_gridBots)
    {
      instruments.push_back(bot->GetInstrument());
    }
    return instruments;
  }
}
