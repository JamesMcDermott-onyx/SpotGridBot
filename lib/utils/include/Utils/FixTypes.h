#pragma once

#include <string>
#include <map>
#include <vector>
#include <memory>

#include "CurrencyPair.h"
#include "FixDefs.h"
#include "ObjectPool.h"

namespace UTILS
{
    enum class BinaryMessageType
    {
        bmtLogon, bmtLogoff, bmtMarketData, bmtNone
    };
    
    inline int64_t UpdateActionToQuoteType(char updateAction)
    {
        return int64_t(updateAction - '0');
    }
    
    inline char QuoteTypeToUpdateAction(int64_t quoteType)
    {
        return char(quoteType) + '0';
    }
    
/*! \brief This class represents the side (BUY/SELL) of a trade. */
class Side
{
public:
	
	/*! \brief char representation of the BUY side */
	static const char BUY;
	
	/*! \brief char representation of the SELL side */
	static const char SELL;
	
	/*! \brief char representation of an invalid side */
	static const char INVALID;
	
	/*! \brief Constructor, setting the side to INVALID */
	Side()
			: m_value(INVALID) { }
	
	/*! \brief Implicit constructor, setting the side to \a value */
	Side(char value)
			: m_value(value) { }
	
	/*! \brief Copy constructor. */
	Side(const Side &other)
			: Side(other.m_value) { }
	
	/*! \brief Assignment operator. */
	Side &operator=(char c)
	{
		m_value = c;
		return *this;
	}
	
	/*! \brief Implicit conversion operator. */
	operator char() const { return m_value; }
	
	/*! \brief Explicit conversion operator. */
	explicit operator std::string() const { return ToString(); }
	
	/*! \brief Reverse operator.
	 *
	 * @return Opposite side: BUY -> SELL, SELL -> BUY, INVALID -> INVALID
	 * */
	Side operator~() const { return Side(Buy() ? SELL : (Sell() ? BUY : INVALID)); }
	
	/*! \brief Is the side valid?
	 *
	 * @return \a true if the value of the side is either BUY or SELL
	 * */
	bool Valid() const { return m_value == BUY || m_value == SELL; }
	
	/*! \brief Is BUY the value of the side?
	 *
	 * @return \a true if the value of the side is BUY
	 * */
	bool Buy() const { return m_value == BUY; }
	
	/*! \brief Is SELL the value of the side?
	 *
	 * @return \a true if the value of the side is SELL
	 * */
	bool Sell() const { return m_value == SELL; }
	
	/*! \brief String representation of the side
	 *
	 * @return BUY, SELL, or INVALID
	 * */
	std::string ToString() const { return nameMap[Valid() ? m_value : INVALID]; }
	
	/*! \brief Overload of the ostream << operator. */
	friend std::ostream &operator<<(std::ostream &stream, const Side &side)
	{
		return stream << side.ToString();
	}

private:
	/*! \brief Map of string representations of sides (char -> string) */
	static std::map<char, std::string> nameMap;
	
	/*! \brief Char value representing the side */
	char m_value;
};


/*! \brief This class represents the type (BID/OFFER) of a quote. */
class QuoteType
{
public:
	
	/*! \brief char representation of quote type BID */
	static const char BID;
	
	/*! \brief char representation of quote type OFFER */
	static const char OFFER;
	
	/*! \brief char representation of invalid quote type */
	static const char INVALID;
	
	/*! \brief Constructor, setting the quote type to INVALID */
	QuoteType()
			: m_value(INVALID) { }
	
	/*! \brief Implicit constructor, setting the quote type to the given value */
	QuoteType(char value)
			: m_value(value) { }
	
	/*! \brief Copy constructor. */
	QuoteType(const QuoteType &other)
			: QuoteType(other.m_value) { }
	
	/*! \brief Copy assignment operator. */
	QuoteType &operator=(char c)
	{
		m_value = c;
		return *this;
	}
	
	/*! \brief Implicit conversion operator. */
	operator char() const { return m_value; }
	
	/*! \brief Explicit conversion operator. */
	explicit operator std::string() const { return ToString(); }
	
	/*! \brief Reverse operator.
	 *
	 * @return Opposite side: BID -> OFFER, OFFER -> BID, INVALID -> INVALID
	 * */
	QuoteType operator~() const { return QuoteType(Bid() ? OFFER : (Offer() ? BID : INVALID)); }
	
	/*! \brief Is the quote type valid?
	 *
	 * @return \a true if the value of the quote type is either BID or OFFER
	 * */
	bool Valid() const { return m_value == BID || m_value == OFFER; }
	
	/*! \brief Is BID the value of the quote type?
	 *
	 * @return \a true if the value of the quote type is BID
	 * */
	bool Bid() const { return m_value == BID; }
	
	/*! \brief Is OFFER the value of the quote type?
	 *
	 * @return \a true if the value of the quote type is OFFER
	 * */
	bool Offer() const { return m_value == OFFER; }
	
	/*! \brief String representation of the quote type
	 *
	 * @return BID, OFFER, or INVALID
	 * */
	std::string ToString() const { return nameMap[Valid() ? m_value : INVALID]; }
	
	/*! \brief Overload of the ostream << operator. */
	friend std::ostream &operator<<(std::ostream &stream, const QuoteType &quoteType)
	{
		return stream << quoteType.ToString();
	}

private:
	/*! \brief Map of string representations of quote types (char -> string) */
	static std::map<char, std::string> nameMap;
	
	/*! \brief Char value representing the quote type */
	char m_value;
};

#define TIF_MIN     TimeInForce::DAY
#define TIF_MAX     TimeInForce::CLS

#define TIF_VALID_COUNT   8
#define TIF_IDX_INVALID  TIF_VALID_COUNT
#define TIF_DELIMITER   ","


/*! \brief This class represents a time in force value. */
class TimeInForce
{
public:
	static const char DAY; // '0': Day (or session)
	static const char GTC; // '1': Good till Cancel
	static const char OPG; // '2': At the Opening
	static const char IOC; // '3': Immediate or Cancel
	static const char FOK; // '4': Fill or Kill
	static const char GTX; // '5': Good till Crossing
	static const char GTD; // '6': Good till date
	static const char CLS; // '7': At the Close
	static const char INVALID;
	
	/*! \brief Constructor, setting the time in force value to INVALID */
	TimeInForce()
			: m_value(INVALID) { }
	
	/*! \brief Implicit constructor, setting the time in force value to the given value */
	TimeInForce(char value)
			: m_value(IsValid(value) ? value : INVALID) { }
	
	/*! \brief Implicit constructor, setting the time in force value to the given value */
	TimeInForce(const std::string &value)
			: m_value(StringToChar(value)) { }
	
	/*! \brief Copy constructor. */
	TimeInForce(const TimeInForce &other)
			: TimeInForce(other.m_value) { }
	
	/*! \brief Char assignment operator. */
	TimeInForce &operator=(char c)
	{
		m_value = c;
		return *this;
	}
	
	/*! \brief String a assignment operator. */
	TimeInForce &operator=(const std::string &s)
	{
		m_value = StringToChar(s);
		return *this;
	}
	
	/*! \brief Implicit char conversion operator. */
	operator char() const { return m_value; }
	
	/*! \brief Explicit string conversion operator. */
	explicit operator std::string() const { return ToString(); }
	
	/*! \brief Is the time in force value valid?
	 *
	 * @return \a true if time in force has one of the eight defined values
	 * */
	bool Valid() const { return IsValid(m_value); }
	
	/*! \brief Index of the Time in force value
	 *
	 * @return 0 -> DAY, 3 -> IOC, 4 -> FOK, ...
	 * */
	int Index() const { return CharToIndex(m_value); }
	
	/*! \brief String representation of the time in force value
	 *
	 * @return "DAY", "IOC",  "FOK", ...
	 * */
	std::string ToString() const { return CharToString(m_value); }
	
	/*! \brief Overload of the ostream << operator. */
	friend std::ostream &operator<<(std::ostream &stream, const TimeInForce &tif)
	{
		return stream << tif.ToString();
	}

private:
	/*! \brief Static vector of string representations of time in force values */
	static std::vector<std::string> names;
	
	/*! Time in force value */
	char m_value;
	
	/*! Is \a c a valid time in force value?  */
	static bool IsValid(char c) { return c >= TIF_MIN && c <= TIF_MAX; }
	
	/*! Index of value \a c */
	static int CharToIndex(char c) { return IsValid(c) ? (int) (c - '0') : -1; }
	
	/*! String representation of value \a c */
	static const std::string &CharToString(char c) { return names[IsValid(c) ? CharToIndex(c) : TIF_IDX_INVALID]; }
	
	/*! Map string representation \a str to char value */
	static char StringToChar(const std::string &str);
};

class TimeInForceSet
{
public:
	/*! \brief uint representation of empty set  */
	static const uint NONE;
	
	/*! \brief Constructor, setting no time in force value */
	TimeInForceSet()
			: m_tif(NONE) { }
	
	/*! \brief Implicit constructor, setting the time in force set to the given value */
	TimeInForceSet(TimeInForce value)
			: m_tif(ToFlag(value)) { }
	
	/*! \brief Implicit constructor, setting the time in force set from a string */
	TimeInForceSet(const std::string &value)
			: m_tif(StringToFlags(value)) { }
	
	/*! \brief Copy constructor. */
	TimeInForceSet(const TimeInForceSet &other)
			: m_tif(other.m_tif) { }
	
	/*! \brief Char assignment operator. */
	TimeInForceSet &operator=(char c)
	{
		m_tif = CharToFlag(c);
		return *this;
	}
	
	/*! \brief String assignment operator. */
	TimeInForceSet &operator=(const std::string &s)
	{
		m_tif = StringToFlags(s);
		return *this;
	}
	
	/*! \brief Explicit conversion operator. */
	explicit operator std::string() const { return ToString(); }
	
	/*! \brief Compound assignment operator: Adds a time in force value from the set */
	TimeInForceSet &operator+=(const TimeInForce &tif)
	{
		m_tif |= CharToFlag(tif);
		return *this;
	}
	
	/*! \brief Compound assignment operator: Adds the flags of another set */
	TimeInForceSet &operator+=(const TimeInForceSet &other)
	{
		m_tif |= other.m_tif;
		return *this;
	}
	
	/*! \brief Compound assignment operator: Removes a time in force value from the set */
	TimeInForceSet &operator-=(const TimeInForce &tif)
	{
		m_tif &= ~CharToFlag(tif);
		return *this;
	}
	
	/*! \brief Compound assignment operator: Removes the flags of another set */
	TimeInForceSet &operator-=(const TimeInForceSet &other)
	{
		m_tif &= ~other.m_tif;
		return *this;
	}
	
	/*! \brief Empty set? */
	bool Empty() const { return m_tif == NONE; }
	
	/*! \brief Does the set contain the value \a tif ? */
	bool Contains(TimeInForce tif) const { return (m_tif & CharToFlag(tif)) != 0; }
	
	/*! \brief String representation of the time in force set
	 *
	 * This function returns a comma-separated list of time in force values
	 *
	 * @param useNames \a true -> "IOC,FOK", \a false -> "3,4"
	 * @return String containing a comma-separated list of time in force values
	 * */
	std::string ToString(bool useNames = true) const { return FlagsToString(m_tif, useNames); }
	
	/*! \brief Overload of the ostream << operator. */
	friend std::ostream &operator<<(std::ostream &stream, const TimeInForceSet &tifSet)
	{
		return stream << tifSet.ToString();
	}
	
	/*! \brief String of char representations of the time in
	 * force set
	 *
	 * This function returns a concatenation of the time in
	 * force values in the set
	 *
	 * @return Concatenation of time in force value (e.g. "34", meaning IOC + FOK)
	 * */
	std::string ToChars() const { return FlagsToChars(m_tif); }

private:
	
	/*! \brief uint bit field representing a combination of time in force values */
	uint m_tif;
	
	/*! \brief Convert a time in force character to a flag value */
	static uint CharToFlag(char c) { return c >= TIF_MIN && c <= TIF_MAX ? (1u << (uint) (c - '0')) : NONE; }
	
	static std::string FlagsToString(uint flags, bool useNames);
	
	static std::string FlagsToChars(uint flags);
	
	static uint StringToFlags(const std::string &str);
	
	/*! \brief Convert a time in force integer value to a flag value */
	static uint IndexToFlag(int idx) { return (idx >= 0 && idx < TIF_VALID_COUNT ? (1u << (uint) idx) : NONE); }
	
	/*! \brief Convert a time in force value to a flag value */
	static uint ToFlag(TimeInForce tif) { return IndexToFlag(tif.Index()); }
};

template <typename T>
struct BidAskPair : std::pair<T, T>
{
	BidAskPair() = default;
	
	template <typename ...Args>
	BidAskPair(Args &&...args)
			: std::pair<T, T>(std::forward<Args>(args)...) { }
	
	const T &Bid() const { return this->first; }
	
	T &Bid() { return this->first; }
	
	const T &Ask() const { return this->second; }
	
	T &Ask() { return this->second; }
	
	const T &Get(bool bid) const { return bid ? Bid() : Ask(); }
	
	T &Get(bool bid) { return bid ? Bid() : Ask(); }
	
	template <typename A>
	void ForEach(A action)
	{
		action(Bid(), true);
		action(Ask(), false);
	}
	
	template <typename A>
	void ForEach(A action) const
	{
		action(Bid(), true);
		action(Ask(), false);
	}
};

//#define PARSE_ORIGINATORS

/**
 * Normalized representation of a market data message
 */
struct BookUpdate
{
	static constexpr uint64_t POOL_SIZE = 1L << 20;
	// using Ptr = std::unique_ptr<NormalizedMDData>; // can't use this because of custom deleter
	using Ptr = std::shared_ptr<BookUpdate>;

#ifdef PARSE_ORIGINATORS
	using Originator = std::pair<std::string, double>;
	using OriginatorVector = std::vector<Originator>;
	using OriginatorVectorPtr = std::shared_ptr<OriginatorVector >;
#endif
	
	struct Entry
	{
		std::string id { "" }; //!< tag 278 (if not present: generated id)
		std::string refId { "" }; //!< tag 280 (if not present: generated id)
		std::string quoteId { "" }; //!< tag 177 (if not present: leave empty)
		std::string settlDate { "" }; //!< tag 64 (if not present: leave empty)
		int64_t updateType { 0 }; //!< tag 279
		int64_t positionNo { 0 }; //!< tag 290
		QuoteType entryType { QuoteType::INVALID }; //!< tag 269
		CurrencyPair instrument { }; //!< tag 55
		Currency currency { }; //!< tag 15
		double price { 0.0 }, volume { 0.0 }, minQty { 0.0 }; //!< tags 270, 271, 110, respectively
#ifdef PARSE_ORIGINATORS
		OriginatorVectorPtr originators;
#else
		std::string originators { "" }; //!< tag 282
#endif
        int64_t adptReceiveTime { 0 };
        uint64_t sequenceTag {0};
		bool endOfMessage { false }; //!< Is this the last entry of the message?
	};
	
	std::string mdReqID { "" }; //!< tag 262
	std::vector<Entry> entries; //!< vector of entries
};


/**
 * Normalized representation of an execution report
 */
struct NormalizedERData
{
	using Ptr = std::unique_ptr<NormalizedERData>;
	
	std::string orderID { "" }; //!< tag 37
	std::string execID { "" }; //!< tag 17
	std::string clOrdID { "" }; //!< tag 11
	std::string origClOrdID { "" }; //!< tag 41
	CurrencyPair instrument { }; //!< tag 55
	Currency currency { };  //!< tag 15
	double orderQty { 0.0 };  //!< tag 38
	double orderPx { 0.0 };  //!< tag 44
	char execType { '\0' }; //!< tag 150, value set as in FIX 4.3 (no EXECTYPE_TRADE!)
	char orderStatus { '\0' }; //!< tag 39, value set as in FIX 4.3
	double leavesQty { 0.0 };  //!< tag 151
	double cumQty { 0.0 }; //!< tag 14
	double avgPx { 0.0 }; //!< tag 6
	double lastQty { 0.0 }; //!< tag 32
	double lastPx { 0.0 }; //!< tag 31
	std::string settlDate { "" }; //!< tag 64
	std::string text { "" }; //!< tag 58
	std::string customPbTag {""}; //!< tag 375 (contra broker with Euronext/Fastmatch), or others
};

}

