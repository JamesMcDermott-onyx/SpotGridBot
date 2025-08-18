//
// Created by jameson 04.06.18.
//

#include <Poco/StringTokenizer.h>

#include "Utils/ObjectPool.h"
#include "Utils/Utils.h"
#include "Utils/FixTypes.h"

namespace UTILS {

//definitions of static const variables:
const char Side::BUY = '1';
const char Side::SELL = '2';
const char Side::INVALID = '\0';

//static
std::map<char, std::string>Side::nameMap = {{ '1',  "BUY" },
											{ '2',  "SELL" },
											{ '\0', "INVALID" }, };

//definitions of static const variables:
const char QuoteType::BID = '0';
const char QuoteType::OFFER = '1';
const char QuoteType::INVALID = '\0';

//static
std::map<char, std::string>QuoteType::nameMap = {{ BID,     "BID" },
												 { OFFER,   "OFFER" },
												 { INVALID, "INVALID" }, };

//definitions of static const variables:
const char TimeInForce::DAY = '0';
const char TimeInForce::GTC = '1';
const char TimeInForce::OPG = '2';
const char TimeInForce::IOC = '3';
const char TimeInForce::FOK = '4';
const char TimeInForce::GTX = '5';
const char TimeInForce::GTD = '6';
const char TimeInForce::CLS = '7';
const char TimeInForce::INVALID = '\0';

//static
std::vector<std::string> TimeInForce::names = { "DAY", "GTC", "OPG", "IOC", "FOK", "GTX", "GTD", "CLS", "INVALID" };

//static
char TimeInForce::StringToChar(const std::string &str)
{
	int ix { (int) UTILS::IndexOf(names, str) }; // represented as string ("DAY", "GTC", ...)
	if (ix >= 0 && ix < TIF_VALID_COUNT)
	{
		return (char) ix + '0';
	}
	else
	{
		return INVALID;
	}
}

//definitions of static const variables:
const uint TimeInForceSet::NONE = 0;

//static
std::string TimeInForceSet::FlagsToString(uint flags, bool useNames)
{
	std::string result;
	for (uint ix { 0 }; ix < TIF_VALID_COUNT; ++ix)
	{
		if ((flags & (1u << ix)) != 0)
		{
			if (!result.empty())
			{
				result += TIF_DELIMITER;
			}
			if (useNames)
			{
				result += TimeInForce((char) ix + '0').ToString(); // use string representation of the time in force value ("DAY", "GTC", ...)
			}
			else
			{
				result += std::to_string(ix);
			} // use character representation ('0', '1', ...)
		}
	}
	return result;
}

//static
std::string TimeInForceSet::FlagsToChars(uint flags)
{
	std::string result;
	for (uint ix { 0 }; ix < TIF_VALID_COUNT; ++ix)
	{
		if ((flags & (1u << ix)) != 0)
		{
			result += std::to_string(ix);
		} // use character representation ('0', '1', ...)
	}
	return result;
}

//static
uint TimeInForceSet::StringToFlags(const std::string &str)
{
	uint result { 0 };
	Poco::StringTokenizer tNames { str, TIF_DELIMITER, Poco::StringTokenizer::TOK_TRIM };
	for (const std::string &name : tNames)
	{
		if (name.length() == 1)
		{
			result |= CharToFlag(name[0]); // represented as character ('0', '1', ...)
		}
		else
		{
			TimeInForce tif { name };
			if (tif.Valid())
			{
				result |= ToFlag(tif);
			}
		}
	}
	return result;
}

}
