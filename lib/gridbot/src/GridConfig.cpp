#include <Poco/Logger.h>
#include "Utils/XmlConfigReader.h"
#include "GridConfig.h"

const std::string TAG_GRID_CONFIG = "GridConfig";

const std::string ATTR_INSTRUMENT = "instrument";
const std::string ATTR_BASE_PRICE = "base_price";
const std::string ATTR_LEVELS_BELOW = "levels_below";
const std::string ATTR_LEVELS_ABOVE = "levels_above";
const std::string ATTR_STEP_PERCENT = "step_percent";
const std::string ATTR_PERCENT_ORDER_QTY = "percent_order_qty";
const std::string ATTR_MAX_POSITION = "max_position";

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

	 	if (auto *baseNode = UTILS::GetConfigNode(pDoc, TAG_GRID_CONFIG, &errMsg))
	 	{
	 		poco_information_f1(logger(), "Reading %s attributes from XML", TAG_GRID_CONFIG);

	 		m_instrument = UTILS::GetXmlAttribute(baseNode, ATTR_INSTRUMENT, "");
	 		
	 		std::string basePriceStr = UTILS::GetXmlAttribute(baseNode, ATTR_BASE_PRICE, "0.0");
	 		std::string levelsBelowStr = UTILS::GetXmlAttribute(baseNode, ATTR_LEVELS_BELOW, "0");
	 		std::string levelsAboveStr = UTILS::GetXmlAttribute(baseNode, ATTR_LEVELS_ABOVE, "0");
	 		std::string stepPercentStr = UTILS::GetXmlAttribute(baseNode, ATTR_STEP_PERCENT, "0.0");
	 		std::string percentOrderQtyStr = UTILS::GetXmlAttribute(baseNode, ATTR_PERCENT_ORDER_QTY, "0.0");
	 		std::string maxPositionStr = UTILS::GetXmlAttribute(baseNode, ATTR_MAX_POSITION, "0.0");
	 		
	 		poco_information_f1(logger(), "instrument: %s", m_instrument);
	 		poco_information_f1(logger(), "base_price string: %s", basePriceStr);
	 		
	 		m_basePrice = std::stod(basePriceStr);
	 		m_levelsBelow = std::stoi(levelsBelowStr);
	 		m_levelsAbove = std::stoi(levelsAboveStr);
	 		m_stepPercent = std::stod(stepPercentStr);
	 		m_percentOrderQty = std::stod(percentOrderQtyStr);
	 		m_maxPosition = std::stod(maxPositionStr);
	 		
	 		std::string configSummary = "Loaded config: base=" + std::to_string(m_basePrice) + 
	 		                           ", levels=" + std::to_string(m_levelsBelow) + "/" + std::to_string(m_levelsAbove) + 
	 		                           ", step=" + std::to_string(m_stepPercent) + 
	 		                           ", qty=" + std::to_string(m_percentOrderQty) + 
	 		                           ", max=" + std::to_string(m_maxPosition);
	 		poco_information(logger(), configSummary);
	 	}
	 	else
	 	{
	 		poco_error(logger(), "Error loading config: Invalid base node");
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
