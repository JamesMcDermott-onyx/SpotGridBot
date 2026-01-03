//
// Created by james on 08/08/2025.
//
#pragma once

#include <memory>
#include <unordered_map>
#include <vector>
#include <string>

#include "IOrderManager.h"
#include "Utils/CurrencyPair.h"
#include "GridConfig.h"

namespace STRATEGY {

  // Single grid bot instance managing one instrument
  class GridBot {
  public:
    GridBot(const GridConfigData& cfg, std::shared_ptr<CORE::IOrderManager> orderManager);
    
    void Start();
    void LoadExistingOrders();
    void CheckFilledOrders();
    void PrintStatus();
    
    const std::string& GetInstrument() const { return m_cfg.instrument; }
    
  private:
    GridConfigData m_cfg;
    std::shared_ptr<CORE::IOrderManager> m_orderManager;
    std::vector<std::string> m_activeOrders;
    struct OrderDetails { UTILS::Side side; double price; double qty; };
    std::unordered_map<std::string, OrderDetails> m_orderDetails;
    std::unordered_map<std::string, double> m_knownFills;
    UTILS::CurrencyPair m_cp;
    Poco::Logger& m_logger;
  };

  // Strategy manager coordinating multiple grid bots
  class GridStrategy : public UTILS::Logging, public UTILS::ErrorHandler {
  public:
    GridStrategy(std::shared_ptr<CORE::IOrderManager> orderManager, const std::string& path);
    ~GridStrategy() = default;

    void Start();
    void LoadExistingOrders();
    void CheckFilledOrders();
    void PrintStatus();

  private:
    std::shared_ptr<CORE::IOrderManager> m_orderManager;
    GridConfig m_cfgLoader;
    std::vector<std::unique_ptr<GridBot>> m_gridBots;
  };
}
