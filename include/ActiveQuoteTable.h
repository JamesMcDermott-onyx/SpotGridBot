#pragma once

#include <numeric>
#include <cstdint>
#include <Utils/FixTypes.h>


namespace CORE {

class ActiveQuoteTable
{
public:
	/** Struct of information assigned to a quote in the table */
	struct QuoteInfo
	{
		int64_t key; //!< Key (unique timestamp)
		UTILS::CurrencyPair cp; //!< Currency pair
		UTILS::QuoteType entryType; //!< bid/ask
		size_t hashValue; //!< hash value built from volume, price, and minQty
		int64_t oriKey; //!< original key
		uint64_t sequenceTag;
	};
	
	/** Alias for unique pointer to a QuoteInfo object */
	using QuoteInfoPtr = std::unique_ptr<QuoteInfo>;
	
	static size_t CalculateHashValue(const UTILS::BookUpdate::Entry &entry);
	
	bool Empty() const;
	
	/** Find a quote info assigned to a reference ID
	 *
	 * @param refId ID to be looked for
	 * @param quoteInfo Reference to a quote info structure to receive the assigned data
	 * @return @a true if @a refId was found and the quote info data was copied
	 */
	bool FindQuoteInfo(const std::string &refId, QuoteInfo &quoteInfo);
	
	/** Replaces a quote info assigned to a reference ID with a new one
	 *
	 * @param refId ID to be looked for. If empty -> create a new entry
	 * @param newId New ID for the quote info
	 * @param args Arguments used to construct a new QuoteInfo struct
	 * @return Unique pointer to the previous quote info (that was replaced
	 * by the new one)
	 */
	template <typename ...Args>
	QuoteInfoPtr ReplaceQuoteInfo(const std::string &refId, const std::string &newId, Args &&...args)
	{
		QuoteInfoPtr newQuote { QuoteInfoPtr(new QuoteInfo { std::forward<Args>(args)..., 0 }) };
		newQuote->oriKey = newQuote->key; // new quote -> oriKey = key
		
		std::unique_lock lock { m_activeQuoteMap.Mutex() };
		return ReplaceQuoteInfoAt((!refId.empty() ? m_activeQuoteMap->find(refId) : m_activeQuoteMap->end()), newId, std::move(newQuote));
	}
	
	/** Replaces a quote info assigned to a reference ID with a new one
	 *
	 * This version replaces an existing quote info only if the hash value of
	 * the new quote info differs from the old one.
	 *
	 * @param forceKey @a true -> always store new key, @a false -> store new key only if hash value has changed
	 * @param refId ID to be looked for. If empty -> create a new entry
	 * @param newId New ID for the quote info
	 * @param volume Volume of the new quote (used to calculate the hash value)
	 * @param price Price of the new quote (used to calculate the hash value)
	 * @param minQty Minimum qty of the new quote (used to calculate the hash value)
	 * @param args Arguments used to construct a new QuoteInfo struct
	 * @return Unique pointer to the previous quote info (that was replaced
	 * by the new one)
	 */
	template <typename ...Args>
	std::pair<QuoteInfoPtr, bool> ReplaceQuoteInfo(bool forceKey, const std::string &refId, const std::string &newId, size_t hashValue,
												   uint64_t sequenceTag, Args &&...args)
	{
		QuoteInfoPtr newQuote { QuoteInfoPtr(new QuoteInfo { std::forward<Args>(args)..., hashValue, int64_t(0), sequenceTag }) };
		
		std::unique_lock lock { m_activeQuoteMap.Mutex() };
		const auto &it { !refId.empty() ? m_activeQuoteMap->find(refId) : m_activeQuoteMap->end() };
		
		const bool skipKey { !forceKey && it != m_activeQuoteMap->end() && it->second->hashValue == hashValue && refId == newId };
		newQuote->oriKey = skipKey ? it->second->oriKey : newQuote->key;
		return { ReplaceQuoteInfoAt(it, newId, std::move(newQuote)), skipKey };
	}
	
	/** Removes a quote info assigned to a reference ID
	 *
	 * @param refId ID of the quote to be removed
	 * @return Unique pointer to the removed quote info
	 */
	QuoteInfoPtr RemoveQuoteInfo(const std::string &refId);
	
	/** Removes all active quotes older than a given key
	 *
	 * @param limitKey All quotes older than @a limitKey are removed
	 * @param action Action to be executed for each reoved quote
	 */
	template <typename A>
	void RemoveOldQuoteInfos(int64_t limitKey, A action)
	{
		std::unique_lock lock { m_activeQuoteMap.Mutex() };
		auto it = m_activeQuoteMap->begin();
		while (it != m_activeQuoteMap->end())
		{
			if (it->second->key < limitKey)
			{
				action(it->first, std::move(it->second));
				it = m_activeQuoteMap->erase(it);
			}
			else
			{
				++it;
			}
		}
	}


protected:
	
	using QuoteMap = std::map<std::string, QuoteInfoPtr>; //!< Type alias for the quote map
	
	/** Shared lockable map (ID -> QuoteInfo struct) */
	UTILS::SharedLockable<QuoteMap> m_activeQuoteMap;
	
	/** Replaces a quote info pointed to by an iterator
	 *
	 * This function DOES NOT LOCK the quote info map (@a m_activeQuoteMap).
	 * Locking must be handled by the calling function.
	 *
	 * @param it Iterator pointing to the quote info to be replaced. If the
	 * iterator is not valid, the new quote is inserted with @a emplace.
	 * @param newId New ID for the quote info
	 * @param newQuote R-Value reference to the new quote info pointer
	 * @return Unique pointer to the previous quote info (that was replaced
	 * with the new one)
	 */
	QuoteInfoPtr ReplaceQuoteInfoAt(QuoteMap::iterator it, const std::string &newId, QuoteInfoPtr &&newQuote);
};

}