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

  class GridStrategy : public UTILS::Logging, public UTILS::ErrorHandler {
  public:
    GridStrategy(std::shared_ptr<CORE::IOrderManager> orderManager, const std::string& path) :
      Logging("GridStrategy"), ErrorHandler(pLogger()), m_orderManager(orderManager), m_cfg(path), m_cp(m_cfg.m_instrument)
    {
    }

    ~GridStrategy() = default;

    void Start();

    void PlaceInitialGrid();
    void CheckFilledOrders();
    void PrintStatus();

  private:
    std::shared_ptr<CORE::IOrderManager> m_orderManager;
    GridConfig m_cfg;
    std::vector<std::string> m_activeOrders;
    struct Meta { UTILS::Side side; double price; double qty; };

    std::unordered_map<std::string, Meta> m_orderMeta;
    std::unordered_map<std::string, double> m_knownFills;
    UTILS::CurrencyPair m_cp;
  };
}
