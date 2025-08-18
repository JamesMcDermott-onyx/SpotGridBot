//
// Created by helmut on 19.02.18.
//

#ifndef COROUT_ORDERBOOK_H
#define COROUT_ORDERBOOK_H

#include <thread>
#include <shared_mutex>

#include "OrderBook/Quote.h"
#include "OrderBook/BookBase.h"
#include "OrderBook/BookView.h"

#define ATTR_BATCHSIZE            "batchsize"
#define DFLT_BATCHSIZE            1
#define ATTR_CLEANUP_INTERVAL    "cleanup_interval"
#define DFLT_CLEANUP_INTERVAL    "10s" // 10 seconds
#define ATTR_MAX_QUOTE_COUNT    "max_quote_count"
#define DFLT_MAX_QUOTE_COUNT    10 // quotes
#define ATTR_MAX_QUOTE_AGE        "max_quote_age"
#define DFLT_MAX_QUOTE_AGE        "1m" // 1 minute

namespace CORE {
namespace BOOK {

/** @brief This class represents an order book, sorting raw quotes and grouping
 * them by market depth. It provides functions that iterate over the levels and
 * copy them to a vector.
 */
class OrderBook : public BookBase
{
public:
	/** @brief Type alias for vector of shared pointers to single quotes. */
	using QuoteVec = std::vector<Quote::Ptr>;
	
	/** @brief Type alias for unique pointer to vector of shared pointers to single quotes. */
	using QuoteVecPtr = std::shared_ptr<QuoteVec>;
	using QuoteVecPtrToConst = std::shared_ptr<const QuoteVec>;

	/** @brief Constructor. */
	explicit OrderBook()
			: BookBase("OrderBook") { }
	
	/** @brief Constructor accepting an explicit logger name (used by derived classes like FakeBook). */
	OrderBook(const std::string &loggerName)
			: BookBase(loggerName) { }
	
	~OrderBook() = default;
	
	/** @brief Default copy constructor. */
	OrderBook(const OrderBook & /* other */) = default;
	
	/** @brief Default copy assignment operator. */
	OrderBook &operator=(const OrderBook & /* other */) = default;
	
	/** @brief Default move constructor. */
	OrderBook(OrderBook && /* other */) = default;
	
	/** @brief Default move assignment operator. */
	OrderBook &operator=(OrderBook && /* other */) = default;
	
	void printBook(std::ostream &ostr, UTILS::CurrencyPair cp, bool bid, unsigned int levels) const;
	
	void printBooks(std::ostream &ostr, bool bid, unsigned int levels) const;
	
	/** @brief Returns a vector of quote groups for an instrument and side (bid/ask)
	 *
	 * This function creates a new vector of quote groups, and fills it with one
	 * quote group per level.
	 *
	 * @param instr     Instrument as string
	 * @param bid       @a true -> bid quote, @a false -> ask quote
	 * @param n         Maximum number of levels
	 * @param quotePred (optional) predicate to be fulfilled by a quote to be added to a quote group
	 *
	 * @return Vector of quote groups
	 */
	BookView::QuoteGroupVec GetLevels(UTILS::CurrencyPair cp, bool bid, unsigned int n, const BookView::QuotePred &quotePred = nullptr) const;

	/** @brief Dynamically creates quote groups and passes them to the provided
	 * callback action.
	 *
	 * This function creates quote groups representing one market level, and
	 * sequentially passes each group to the provided callback action.
	 *
	 * @param instr     Instrument as string
	 * @param bid       @a true -> bid quote, @a false -> ask quote
	 * @param action    void-function to be called for every generated quote group.
	 * @param quotePred (optional) predicate to be fulfilled by a quote to be added to a quote group
	 */
	void IterateQuoteGroups(UTILS::CurrencyPair cp, bool bid, const BookView::QuoteGroupFunc &action,
							const BookView::QuotePred &quotePred = nullptr) const;

	/**
	 * Executes an action for each quote of a ccy pair and side.
	 * @tparam A Template type for action to be executed
	 * @param cp Currency pair
	 * @param bid @a true -> bid, @a false -> ask
	 * @param action Action to be executed for each task.
	 * 					Signature: void action(const BOOK:Quote:Ptr &quote, bool &cont) or void action(BOOK:Quote:Ptr quote, bool &cont).
	 * 					           If @a cont is set to @a false, the iteration loop will will exited
	 */
	template <typename A>
	void IterateQuotes(UTILS::CurrencyPair cp, bool bid, A action) const
	{
		UTILS::BidAskPair<Quote::Ptr> result { nullptr, nullptr };
		std::shared_lock lockMap { m_quoteVectorMap.Mutex() };
		const auto &it { m_quoteVectorMap->find(cp) };
		if (it != m_quoteVectorMap->end())
		{
			std::shared_lock lock { GetLock(cp, bid) };
			QuoteVecPtrToConst quotes { it->second.Get(bid) };
			if (!quotes->empty())
			{
				bool cont { true };
				for (const auto &q: *quotes)
				{
					action(q, cont);
					if (!cont)
					{
						break;
					}
				}
			}
		}
	}
	
	/**
	 * This function returns the current best prices (bid/ask) of a given ccy pair. The choice may be constrained by several parameters
	 *
	 * @tparam P Function template: (bool, Quote&) -> bool
	 * @param cp Currency pair
	 * @param acceptPredicate Predicate that returns @a true if the quote is accepted, @a false otherwise
	 * @param allowSkewSafePrices @a true -> Quotes from skew-safe sessions may be included
	 * @return Pair (bid/ask) of prices of the best accepted quotes from the sortbook
	 */
	template <typename P>
	UTILS::BidAskPair<int64_t> GetBestPrices(UTILS::CurrencyPair cp, P acceptPredicate) const
	{
		const auto bestQuotes { GetBestQuotes(cp, std::move(acceptPredicate)) };
		return { bestQuotes.Bid() ? bestQuotes.Bid()->Price() : 0, bestQuotes.Ask() ? bestQuotes.Ask()->Price() : 0 };
	}
	
	UTILS::BidAskPair<int64_t> GetBestPrices(UTILS::CurrencyPair cp) const;
	
	int64_t GetBestPrice(UTILS::CurrencyPair cp, bool bid) const;
	
	template <typename P>
	int64_t GetBestPrice(UTILS::CurrencyPair cp, bool bid, P acceptPredicate) const
	{
		Quote::Ptr quote { GetBestQuote(cp, bid, std::move(acceptPredicate)) };
		return quote ? quote->Price() : 0;
	}

	Quote::Ptr GetBestQuote(UTILS::CurrencyPair cp, bool bid) const;
	
	template <typename P>
	Quote::Ptr GetBestQuote(UTILS::CurrencyPair cp, bool bid, P acceptPredicate) const
	{
		Quote::Ptr result { nullptr };
		std::shared_lock lockMap { m_quoteVectorMap.Mutex() };
		const auto &it { m_quoteVectorMap->find(cp) };
		if (it != m_quoteVectorMap->end())
		{
			std::shared_lock lock { GetLock(cp, bid) };
			QuoteVecPtrToConst quotes { it->second.Get(bid) };
			lockMap.unlock();
			if (!quotes->empty())
			{
				for (const auto &q: *quotes)
				{
					if (q->Price() > 0 )
					{
						result = q;
						break;
					}
				}
			}
		}
		return result;
	}

	int64_t GetMidPrice(UTILS::CurrencyPair cp) const;
	
	template <typename P>
	int64_t GetMidPrice(UTILS::CurrencyPair cp, P acceptPredicate) const
	{
		const UTILS::BidAskPair<int64_t> bestPrices { GetBestPrices(cp, std::move(acceptPredicate)) };
		return bestPrices.Bid() > 0 && bestPrices.Ask() > 0 ? (bestPrices.Bid() + bestPrices.Ask()) / 2 : 0;
	}
	
	template <typename P>
	UTILS::BidAskPair<Quote::Ptr> GetBestQuotes(UTILS::CurrencyPair cp, P acceptPredicate) const
	{
		UTILS::BidAskPair<Quote::Ptr> result { nullptr, nullptr };
		std::shared_lock lockMap { m_quoteVectorMap.Mutex() };
		const auto &it { m_quoteVectorMap->find(cp) };
		if (it != m_quoteVectorMap->end())
		{
			for (bool bid: { true, false })
			{
				std::shared_lock lock { GetLock(cp, bid) };
				QuoteVecPtrToConst quotes { it->second.Get(bid) };
				if (quotes && !quotes->empty())
				{
					for (const auto &q: *quotes)
					{
						if (q->Price() > 0 && acceptPredicate(bid, *q))
						{
							result.Get(bid) = q;
							break;
						}
					}
				}
			}
		}
		return result;
	}
	
	UTILS::BidAskPair<Quote::Ptr> GetBestQuotes(UTILS::CurrencyPair cp) const;
	
	void AddEntry(int64_t key, int64_t refKey,
				  int64_t receiveTime, UTILS::CurrencyPair cp, const UTILS::BookUpdate::Entry &entry);
	
	void AddEntry(int64_t key, int64_t refKey, int64_t sendTime, int64_t receiveTime, UTILS::CurrencyPair cp,
				  const UTILS::BookUpdate::Entry &entry);
	
	size_t GetQuoteCount(UTILS::CurrencyPair cp, bool bid) const;
	
	void Clear();
	
	Quote::Ptr GetLastQuote() const;

protected:
	
	using QuoteVectorMap = std::map<UTILS::CurrencyPair, UTILS::BidAskPair<QuoteVecPtr>>;
	
	UTILS::SharedLockable<QuoteVectorMap> m_quoteVectorMap;
	
	/** @brief Connection type name.
	 *
	 * Returns the type of the connection as string.
	 *
	 * @return Type name.
	 */
	std::string typeString() const { return "OrderBook"; }
	
	std::string propDefaultValue(const std::string &name) const;

private:
	
	Quote::Ptr m_lastQuote;
	
	void AddQuote(UTILS::CurrencyPair cp, bool bid, Quote::Ptr quote);

	OrderBook::QuoteVec::const_iterator getNextLevel(QuoteGroup::Ptr &quoteGroup, const QuoteVec &srcVec, const BookView::QuotePred &quotePred,
													QuoteVec::const_iterator iterator) const;
	
	/** @brief Type alias for map of shared exclusive locks. */
	using AccessMap = std::map<UTILS::CurrencyPair, std::unique_ptr<std::shared_mutex>>;
	
	/** @brief Maps for shared exclusive locks (@a ask and @a bid side. */
	mutable UTILS::SharedLockable<UTILS::BidAskPair<AccessMap>> m_accessMap;
	
	UTILS::SharedLockable<std::map<UTILS::CurrencyPair, UTILS::BidAskPair<std::optional<int64_t>>>> m_lastCleanupMap; //!< timestamps of last call to CleanupQuoteVec()
	
	std::shared_mutex &GetLock(UTILS::CurrencyPair cp, bool bid) const;
	
	void CleanupQuoteVec(UTILS::CurrencyPair cp, QuoteVec &vec, int64_t maxAge);
	
	std::optional<int64_t> GetLastCleanupTime(UTILS::CurrencyPair cp, bool bid) const;
};

} // namespace BOOK
} // namespace CORE

#endif //COROUT_ORDERBOOK_H
