#include <memory>

#include <Poco/DOM/Node.h>
#include <Poco/DOM/DOMParser.h>
#include <Poco/DOM/NodeIterator.h>
#include <Poco/DOM/NodeList.h>
#include <Poco/DOM/AutoPtr.h>

#include "Utils/Utils.h"
#include "Utils/CurrencyPair.h"
#include "OrderBook/OrderBook.h"

#include <Poco/Logger.h>

using namespace UTILS;

namespace CORE {
namespace BOOK {

std::string OrderBook::propDefaultValue(const std::string &name) const
{
	if (name == ATTR_BATCHSIZE)
	{
		return std::to_string(DFLT_BATCHSIZE);
	}
	else if (name == ATTR_CLEANUP_INTERVAL)
	{
		return DFLT_CLEANUP_INTERVAL;
	}
	else if (name == ATTR_MAX_QUOTE_COUNT)
	{
		return std::to_string(DFLT_MAX_QUOTE_COUNT);
	}
	else if (name == ATTR_MAX_QUOTE_AGE)
	{
		return DFLT_MAX_QUOTE_AGE;
	}
	else
	{
		return BookBase::propDefaultValue(name);
	}
}

void OrderBook::AddEntry(int64_t key, int64_t refKey, int64_t receiveTime, CurrencyPair cp, const BookUpdate::Entry &entry)
{
    AddQuote(cp, entry.entryType.Bid(),
		QuotePool::getQuote(__PRETTY_FUNCTION__,
							true,
							std::this_thread::get_id(),
							entry.adptReceiveTime,
							receiveTime,
							CurrentTimestamp(),
							entry.quoteId,
							1,
							cp.DblToCpip(entry.price),
							cp.DoubleToQty(entry.volume),
							cp.DoubleToQty(entry.minQty),
							key,
							refKey,
							0,
							int(entry.updateType),
							int(entry.positionNo),
							entry.settlDate,
							entry.originators));
}

void OrderBook::AddEntry(int64_t key, int64_t refKey, int64_t sendTime, int64_t receiveTime, CurrencyPair cp,
						const BookUpdate::Entry &entry)
{
    AddQuote(cp, entry.entryType.Bid(),
		QuotePool::getQuote(__PRETTY_FUNCTION__, true, std::this_thread::get_id(),
			entry.adptReceiveTime, receiveTime, CurrentTimestamp(), entry.quoteId,
							1, cp.DblToCpip(entry.price),
							cp.DoubleToQty(entry.volume), cp.DoubleToQty(entry.minQty), key, refKey, sendTime, int(entry.updateType),
							int(entry.positionNo), entry.settlDate, entry.originators));
}

void OrderBook::AddQuote(CurrencyPair cp, bool bid, Quote::Ptr quote)
{
	{
		std::shared_lock lockMap { m_quoteVectorMap.Mutex() };
		
		auto it { m_quoteVectorMap->find(cp) };
		
		if (it == m_quoteVectorMap->end())
		{
			lockMap.unlock();
			{
				std::unique_lock uniqueLock { m_quoteVectorMap.Mutex() };
				m_quoteVectorMap->emplace(cp, BidAskPair<QuoteVecPtr>(std::make_shared<QuoteVec>(), std::make_shared<QuoteVec>()));
			}
			lockMap.lock();
			it = m_quoteVectorMap->find(cp);
			if (it == m_quoteVectorMap->end())
			{
				poco_error_f1(logger(), "FAILED TO CREATE QUOTE VECTOR ENTRY FOR %s", cp.ToString());
				return;
			}
		}
		
		std::unique_lock lock { GetLock(cp, bid) };
		
		QuoteVecPtr vec { it->second.Get(bid) };
		lockMap.unlock();
		// delete, check if there is something to delete, then erase element and change rest of affected quotes
		if (quote->QuoteType() == QT_DELETE || quote->QuoteType() == QT_UPDATE)
		{
			if (!vec->empty() && quote->RefKey() > 0)
			{
				const auto
						&resultIter { std::find_if(vec->begin(), vec->end(), [&quote](const Quote::Ptr &q) { return q->Key() == quote->RefKey(); }) };
				if (resultIter != vec->end())
				{
					(*resultIter)->SetInvalid(quote);
					vec->erase(resultIter);
				}
				else
				{
                    std::string msg = UTILS::Format("*** %s %s/%Ld: FAILED UPDATE/DELETE: Quote with RefKey %Ld %Ld not found !!! ***", cp.ToString(),
                                                    quote->SeqNum(), quote->RefKey(), quote->Price());
					poco_error(logger(),msg);
				}
			}
			else
			{
				poco_error_f3(logger(), "*** %s/%Ld %Ld: Missing RefKey in UPDATE/DELETE ***", cp.ToString(),  quote->SeqNum(), quote->Price());
			}
		}
		if (quote->QuoteType() != QT_DELETE)
		{
			if (vec->empty()) // vector empty, create and no need to sort
			{
				vec->push_back(quote);
			}
			else // vector NOT empty, update and sort or insert at correct position
			{
				//insert quote at right place
				vec->insert(std::find_if(vec->begin(), vec->end(), [bid, &quote](const Quote::Ptr &q)
				{
					if (quote->Price() != q->Price()) // different price -> better price first
					{
						return bid ? quote->Price() > q->Price() : quote->Price() < q->Price();
					}
					else // same price -> greater volume first
					{
						return quote->Volume() >= q->Volume();
					}
				}), quote);
			}
		}
	}

	std::atomic_store(&m_lastQuote, quote);

	if (bid) {
		if (quote->Price() > GetBestPrice(cp, bid)) {
			m_action();
		}
	}
	else {
		if (quote->Price() < GetBestPrice(cp, bid)) {
			m_action();
		}
	}

	// static int cnt=1;
	//
	// if (cnt++ == 30)
	// {
	// 	printBooks(std::cout, true, 10);
	// 	printBooks(std::cout, false, 10);
	// 	cnt = 1;
	// }
}

BookView::QuoteGroupVec OrderBook::GetLevels(CurrencyPair cp, bool bid, unsigned int n, const BookView::QuotePred &quotePred) const
{
	BookView::QuoteGroupVec out;
	IterateQuoteGroups(cp, bid, [&out, &n](int level, QuoteGroup::Ptr &qg, bool &cont)
	{
		out.push_back(qg);
		cont = n == 0 || --n > 0; // n == 0 => unlimited levels; n > 0 => n levels (break when --n reaches 0)
	}, quotePred);
	return out;
}

size_t OrderBook::GetQuoteCount(CurrencyPair cp, bool bid) const
{
	size_t result { 0 };
	std::shared_lock lockMap { m_quoteVectorMap.Mutex() };
	const auto &it { m_quoteVectorMap->find(cp) };
	if (it != m_quoteVectorMap->end())
	{
		std::shared_lock lock { GetLock(cp, bid) };
		result = it->second.Get(bid)->size();
	}
	return result;
}

void OrderBook::IterateQuoteGroups(CurrencyPair cp, bool bid, const BookView::QuoteGroupFunc &action, const BookView::QuotePred &quotePred) const
{
	std::shared_lock lockMap { m_quoteVectorMap.Mutex() };
	const auto &it { m_quoteVectorMap->find(cp) };
	if (it != m_quoteVectorMap->end())
	{
		std::shared_lock lock { GetLock(cp, bid) };
		QuoteVecPtrToConst quotes { it->second.Get(bid) };
		lockMap.unlock();
		QuoteGroup::Ptr quoteGroup;
		bool cont { true };
		int level { 1 };
		for (auto itQuote { quotes->begin() }; cont && itQuote != quotes->end(); ++level)
		{
			itQuote = getNextLevel(quoteGroup, *quotes, quotePred, itQuote);
			if (quoteGroup)
			{
				action(level, quoteGroup, cont);
			}
		}
	}
}

BidAskPair<int64_t> OrderBook::GetBestPrices(CurrencyPair cp) const
{
	return GetBestPrices(cp, [](bool, BOOK::Quote&) { return true; });
}

int64_t OrderBook::GetBestPrice(CurrencyPair cp, bool bid) const
{
	return GetBestPrice(cp, bid, [](bool, BOOK::Quote&) { return true; });
}

Quote::Ptr OrderBook::GetBestQuote(CurrencyPair cp, bool bid) const
{
	return GetBestQuote(cp, bid, [](bool, BOOK::Quote&) { return true; });
}

int64_t OrderBook::GetMidPrice(CurrencyPair cp) const
{
	return GetMidPrice(cp, [](bool, BOOK::Quote&) { return true; });
}

BidAskPair<Quote::Ptr> OrderBook::GetBestQuotes(CurrencyPair cp) const
{
	return GetBestQuotes(cp, [](bool, BOOK::Quote&) { return true; });
}

void OrderBook::Clear()
{
	{
		std::unique_lock lockQuoteVectorMap { m_quoteVectorMap.Mutex() };
		std::unique_lock lockAccessMap { m_accessMap.Mutex() };
		
		// set invalid all quotes:
		for (const auto &it: *m_quoteVectorMap)
		{
			for (bool bid: { true, false })
			{
				const QuoteVecPtr &qvp { it.second.Get(bid) };
				if (qvp)
				{
					for (auto &q: *qvp)
					{
						if (q)
						{
							q->SetInvalid(nullptr);
						}
					}
				}
			}
		}
		
		m_quoteVectorMap->clear();
		m_accessMap->Bid().clear();
		m_accessMap->Ask().clear();
	}

	std::atomic_store(&m_lastQuote, BOOK::Quote::Ptr(nullptr));
}

OrderBook::QuoteVec::const_iterator OrderBook::getNextLevel(QuoteGroup::Ptr &quoteGroup, const QuoteVec &srcVec, const BookView::QuotePred &quotePred,
														  QuoteVec::const_iterator iterator) const
{
	bool success { false };
	quoteGroup = QuoteGroup::Create();
	while (iterator != srcVec.end())
	{
		int64_t price { iterator->get()->Price() };
		while (iterator != srcVec.end() && price == iterator->get()->Price())
		{
			if (!quotePred || quotePred(**iterator))
			{
				quoteGroup->AddQuote(*iterator);
				success = true;
			}
			++iterator;
		}
		if (success) // if quoteGroup contains at least one quote -> done
		{
			break;
		}
	}
	if (!success)
	{
		quoteGroup = nullptr;
	}
	return iterator;
}

// DEBUG
//static
//void OrderBook::printBook(BookView *view, std::ostream& ostr, const std::string& instr, bool bid, unsigned int levels)
void OrderBook::printBook(std::ostream &ostr, CurrencyPair cp, bool bid, unsigned int levels) const
{
	ostr << ">>>>> getting book " << cp.ToString() << " (" << (bid ? "BID" : "ASK") << ", " << levels << " levels) start"
		 << std::endl;
	BookView::QuoteGroupVec book { GetLevels(cp, bid, levels) };
	for (auto &qb: book)
	{
		ostr << qb->AvgPrice();
		if (!qb->SinglePrice())
		{
			ostr << " (" << qb->MinPrice() << " - " << qb->MaxPrice() << ")";
		}
		ostr << " " << qb->TotalVolume() << " size: " << qb->QuoteCount() << " " << std::endl;
	}
	ostr << std::endl << ">>>>> getting book done" << std::endl;
}

void OrderBook::printBooks(std::ostream &ostr, bool bid, unsigned int levels) const
{
	std::shared_lock lockMap { m_quoteVectorMap.Mutex() };
	QuoteVectorMap mapCopy { *m_quoteVectorMap };
	lockMap.unlock();
	for (auto &it: mapCopy)
	{
		std::shared_lock lock { GetLock(it.first, bid) };
		printBook(ostr, it.first, bid, levels);
	}
}


std::shared_mutex &OrderBook::GetLock(CurrencyPair cp, bool bid) const
{
	std::shared_lock slock { m_accessMap.Mutex() };
	AccessMap &map { m_accessMap->Get(bid) };
	const auto &it { map.find(cp) };
	if (it != map.end())
	{
		return *(it->second);
	}
	else
	{
		slock.unlock();
		std::unique_lock ulock { m_accessMap.Mutex() };
		AccessMap &map2 { m_accessMap->Get(bid) }; // "map" could have been invalidated while unlocked
		return *map2.emplace(cp, std::make_unique<std::shared_mutex>()).first->second;
	}
}


void OrderBook::CleanupQuoteVec(CurrencyPair cp, QuoteVec &vec, int64_t maxAge)
{
	if (maxAge > 0)
	{
		auto it = vec.begin();
		while (it != vec.end())
		{
			if ((*it)->AgeSinceSend() > maxAge)
			{
				poco_information_f2(logger(), "Erase outdated quote: %s (older than MaxAge = %s)", (*it)->ToString(cp), NanosecondsToString(maxAge));
				(*it)->SetInvalid(nullptr);
				it = vec.erase(it);
			}
			else
			{
				++it;
			}
		}
	}
}

Quote::Ptr OrderBook::GetLastQuote() const
{
	return std::atomic_load(&m_lastQuote);
}

} // namespace BOOK
} // namespace CORE
