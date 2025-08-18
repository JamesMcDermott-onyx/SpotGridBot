//
// Created by aleksandra on 1/19/23.
//

#include "ActiveQuoteTable.h"
#include "Utils/Utils.h"

namespace CORE {

//static
size_t ActiveQuoteTable::CalculateHashValue(const UTILS::BookUpdate::Entry &entry)
{
	std::hash<double> dblHash;
	size_t result { dblHash(entry.volume) ^ dblHash(entry.price) ^ dblHash(entry.minQty) };
	if (!entry.quoteId.empty()) // if quote ID is set -> include in hash value
	{
		std::hash<std::string> strHash;
		result ^= strHash(entry.quoteId);
	}
	return result;
}


bool ActiveQuoteTable::Empty() const
{
	std::shared_lock lock { m_activeQuoteMap.Mutex() };
	return m_activeQuoteMap->empty();
}


bool ActiveQuoteTable::FindQuoteInfo(const std::string &refId, QuoteInfo &quoteInfo)
{
	bool result { false };
	std::shared_lock lock { m_activeQuoteMap.Mutex() };
	if (!refId.empty())
	{
		const auto &it { m_activeQuoteMap->find(refId) };
		if (it != m_activeQuoteMap->end())
		{
			quoteInfo = *it->second;
			result = true;
		}
	}
	return result;
}


ActiveQuoteTable::QuoteInfoPtr ActiveQuoteTable::ReplaceQuoteInfoAt(QuoteMap::iterator it, const std::string &newId, QuoteInfoPtr &&newQuote)
{
	ActiveQuoteTable::QuoteInfoPtr lastQuote { nullptr };
	if (it != m_activeQuoteMap->end())
	{
		lastQuote = std::move(it->second); // save replaced entry
		it->second = std::move(newQuote); // replace quote info
		if (newId != it->first) // new ID -> replace map key:
		{
			auto nh { m_activeQuoteMap->extract(it) };
			nh.key() = newId;
			m_activeQuoteMap->insert(move(nh));
		}
	}
	else // Ref ID not found -> create new entry
	{
		m_activeQuoteMap->emplace(newId, std::move(newQuote));
	}
	return lastQuote;
}


ActiveQuoteTable::QuoteInfoPtr ActiveQuoteTable::RemoveQuoteInfo(const std::string &refId)
{
	QuoteInfoPtr lastQuote { nullptr };

	std::unique_lock lock { m_activeQuoteMap.Mutex() };
	if (!refId.empty())
	{
		const auto &it { m_activeQuoteMap->find(refId) };
		if (it != m_activeQuoteMap->end())
		{
			lastQuote = std::move(it->second);
			m_activeQuoteMap->erase(it);
		}
	}
	return lastQuote;
}

}