#include <Poco/Logger.h>
#include <Poco/DOM/NodeList.h>
#include "Utils/XmlConfigReader.h"
#include "GridConfig.h"

const std::string TAG_GRID_BOTS = "GridBots";
const std::string TAG_GRID_CONFIG = "GridConfig";

const std::string ATTR_NAME = "name";
const std::string ATTR_INSTRUMENT = "instrument";
const std::string ATTR_BASE_PRICE = "base_price";
const std::string ATTR_LEVELS_BELOW = "levels_below";
const std::string ATTR_LEVELS_ABOVE = "levels_above";
const std::string ATTR_STEP_PERCENT = "step_percent";
const std::string ATTR_PERCENT_ORDER_QTY = "percent_order_qty";
const std::string ATTR_MAX_POSITION = "max_position";
const std::string ATTR_CREATE_POSITION = "create_position";

const std::string TAG_SESSION_CONFIG = "SessionConfig";
const std::string TAG_SESSION = "Session";

using namespace UTILS;

bool GridConfig::LoadConfig(const std::string &path)
{
	poco_information_f1(logger(), "Loading definitions using: %s", path);
	return LoadConfig(GetConfigDoc(path));
}

bool GridConfig::LoadConfig(const UTILS::XmlDocPtr &pDoc)
{
	 try
	 {
	 	std::string errMsg;
	 	m_gridConfigs.clear();

	 	// Try to find GridBots container first (new format)
	 	if (auto *gridBotsNode = UTILS::GetConfigNode(pDoc, TAG_GRID_BOTS, &errMsg))
	 	{
	 		poco_information(logger(), "Reading GridBots container with multiple GridConfig entries");
	 		
	 		// Iterate through all GridConfig child nodes using Poco DOM API
	 		if (gridBotsNode->hasChildNodes())
	 		{
	 			Poco::XML::NodeList *children = gridBotsNode->childNodes();
	 			if (children)
	 			{
	 				for (unsigned long i = 0; i < children->length(); ++i)
	 				{
	 					Poco::XML::Node *gridNode = children->item(i);
	 					if (gridNode->nodeType() == Poco::XML::Node::ELEMENT_NODE && 
	 					    gridNode->localName() == TAG_GRID_CONFIG)
	 					{
	 						GridConfigData cfg;
	 						
	 						cfg.name = UTILS::GetXmlAttribute(gridNode, ATTR_NAME, "");
	 						cfg.instrument = UTILS::GetXmlAttribute(gridNode, ATTR_INSTRUMENT, "");
	 						
	 						std::string basePriceStr = UTILS::GetXmlAttribute(gridNode, ATTR_BASE_PRICE, "0.0");
	 						std::string levelsBelowStr = UTILS::GetXmlAttribute(gridNode, ATTR_LEVELS_BELOW, "0");
	 						std::string levelsAboveStr = UTILS::GetXmlAttribute(gridNode, ATTR_LEVELS_ABOVE, "0");
	 						std::string stepPercentStr = UTILS::GetXmlAttribute(gridNode, ATTR_STEP_PERCENT, "0.0");
	 						std::string percentOrderQtyStr = UTILS::GetXmlAttribute(gridNode, ATTR_PERCENT_ORDER_QTY, "0.0");
	 						std::string maxPositionStr = UTILS::GetXmlAttribute(gridNode, ATTR_MAX_POSITION, "0.0");	 					std::string createPositionStr = UTILS::GetXmlAttribute(gridNode, ATTR_CREATE_POSITION, "true");	 						
	 						cfg.basePrice = std::stod(basePriceStr);
	 						cfg.levelsBelow = std::stoi(levelsBelowStr);
	 						cfg.levelsAbove = std::stoi(levelsAboveStr);
	 						cfg.stepPercent = std::stod(stepPercentStr);
	 						cfg.percentOrderQty = std::stod(percentOrderQtyStr);
	 						cfg.maxPosition = std::stod(maxPositionStr);
	 						
	 						m_gridConfigs.push_back(cfg);
	 						
	 						std::string configSummary = "Loaded grid '" + cfg.name + "' for " + cfg.instrument + 
	 						                           ": base=" + std::to_string(cfg.basePrice) + 
	 						                           ", levels=" + std::to_string(cfg.levelsBelow) + "/" + std::to_string(cfg.levelsAbove) + 
	 						                           ", step=" + std::to_string(cfg.stepPercent) + 
	 						                           ", qty=" + std::to_string(cfg.percentOrderQty) + 
	 						                           ", max=" + std::to_string(cfg.maxPosition);
	 						poco_information(logger(), configSummary);
	 					}
	 				}
	 				children->release();
	 			}
	 		}
	 		
	 		poco_information_f1(logger(), "Loaded %s grid configurations", std::to_string(m_gridConfigs.size()));
	 	}
	 	// Fallback: Try single GridConfig node (old format for backwards compatibility)
	 	else if (auto *baseNode = UTILS::GetConfigNode(pDoc, TAG_GRID_CONFIG, &errMsg))
	 	{
	 		poco_information(logger(), "Reading single GridConfig (legacy format)");
	 		
	 		GridConfigData cfg;
	 		cfg.name = "grid1";
	 		cfg.instrument = UTILS::GetXmlAttribute(baseNode, ATTR_INSTRUMENT, "");
	 		
	 		std::string basePriceStr = UTILS::GetXmlAttribute(baseNode, ATTR_BASE_PRICE, "0.0");
	 		std::string levelsBelowStr = UTILS::GetXmlAttribute(baseNode, ATTR_LEVELS_BELOW, "0");
	 		std::string levelsAboveStr = UTILS::GetXmlAttribute(baseNode, ATTR_LEVELS_ABOVE, "0");
	 		std::string stepPercentStr = UTILS::GetXmlAttribute(baseNode, ATTR_STEP_PERCENT, "0.0");
	 		std::string percentOrderQtyStr = UTILS::GetXmlAttribute(baseNode, ATTR_PERCENT_ORDER_QTY, "0.0");
	 		std::string maxPositionStr = UTILS::GetXmlAttribute(baseNode, ATTR_MAX_POSITION, "0.0");
	 		std::string createPositionStr = UTILS::GetXmlAttribute(baseNode, ATTR_CREATE_POSITION, "true");
	 		
	 		cfg.basePrice = std::stod(basePriceStr);
	 		cfg.levelsBelow = std::stoi(levelsBelowStr);
	 		cfg.levelsAbove = std::stoi(levelsAboveStr);
	 		cfg.stepPercent = std::stod(stepPercentStr);
	 		cfg.percentOrderQty = std::stod(percentOrderQtyStr);
	 		cfg.maxPosition = std::stod(maxPositionStr);
	 		cfg.createPosition = (createPositionStr == "true");
	 		
	 		m_gridConfigs.push_back(cfg);
	 		
	 		std::string configSummary = "Loaded config for " + cfg.instrument + 
	 		                           ": base=" + std::to_string(cfg.basePrice) + 
	 		                           ", levels=" + std::to_string(cfg.levelsBelow) + "/" + std::to_string(cfg.levelsAbove) + 
	 		                           ", step=" + std::to_string(cfg.stepPercent) + 
	 		                           ", qty=" + std::to_string(cfg.percentOrderQty) + 
	 		                           ", max=" + std::to_string(cfg.maxPosition);
	 		poco_information(logger(), configSummary);
	 	}
	 	else
	 	{
	 		poco_error(logger(), "Error loading config: No GridBots or GridConfig node found");
	 		return false;
	 	}
	 }
	 catch (std::exception &e)
	 {
	 	poco_error_f1(logger(), "Error loading config: %s", std::string(e.what()));
	 	return false;
	 }

	return true;
}
