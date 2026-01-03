#pragma once

#include <string>
#include <vector>
#include "Utils/ErrorHandler.h"
#include "Utils/Logging.h"
#include "Utils/Utils.h"

// ---------- Grid Strategy ----------

// Single grid configuration
struct GridConfigData {
    std::string name;
    std::string instrument;
    double basePrice;
    int levelsBelow;
    int levelsAbove;
    double stepPercent;
    double percentOrderQty;
    double maxPosition;
    bool createPosition;
};

class GridConfig : public UTILS::Logging, public UTILS::ErrorHandler {
public:
    GridConfig(const std::string &path) : Logging("GridConfig") {
        LoadConfig(path);
    }

    bool LoadConfig(const std::string &path);
    bool LoadConfig(const UTILS::XmlDocPtr &pDoc);

    // Access to all grid configurations
    const std::vector<GridConfigData>& GetGridConfigs() const { return m_gridConfigs; }

private:
    std::vector<GridConfigData> m_gridConfigs;
};
