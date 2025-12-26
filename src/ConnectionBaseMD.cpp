#include "ConnectionBaseMD.h"
#include "ConnectionManager.h"
#include "Utils/Result.h"

using namespace UTILS;

namespace {
std::string GenerateStandardEntryId(const UTILS::CurrencyPair &cp, UTILS::QuoteType entryType, const std::string &price)
{
	return UTILS::Format("%s_%c%s", cp.ToString(), entryType.Bid() ? 'B' : 'A', price);
}
}

namespace CORE {
namespace CRYPTO {

ConnectionBaseMD::ConnectionBaseMD(const CRYPTO::Settings &settings, const std::string &loggingPropsPath, 
                                   const std::string &loggerName, const ConnectionManager& connectionManager)
	: ConnectionBase(settings, loggingPropsPath, loggerName, connectionManager)
{
}

ConnectionBaseMD::~ConnectionBaseMD()
{
}

//------------------------------------------------------------------------------
UTILS::BoolResult ConnectionBaseMD::PublishQuote(int64_t key, int64_t refKey, int64_t timestamp,
                                                  int64_t receiveTime, UTILS::CurrencyPair cp, const UTILS::BookUpdate::Entry &entry)
{
	m_connectionManager.GetOrderBook()->AddEntry(key, refKey, timestamp, receiveTime, cp, entry);
	return true;
}

//------------------------------------------------------------------------------
UTILS::BoolResult ConnectionBaseMD::SubscribeInstrument(const std::string &symbol)
{
	const auto instStr = UTILS::toupper(symbol);
	if (UTILS::CurrencyPair(TranslateSymbol(instStr)).Invalid())
	{
		return BoolResult(false, UTILS::Format("Invalid instrument '%s'", instStr));
	}
	
	const auto existingInstruments = GetInstruments();
	if (existingInstruments.find(instStr) != existingInstruments.cend())
	{
		return BoolResult(false, UTILS::Format("Instrument '%s' has been already subscribed", instStr));
	}
	
	// Update config
	m_settings.m_instruments += (m_settings.m_instruments.empty() ? "" : ",") + instStr;
	
	// Request snapshot and subscribe
	Subscribe({ instStr });
	return true;
}

//------------------------------------------------------------------------------
UTILS::BoolResult ConnectionBaseMD::UnsubscribeInstrument(const std::string &symbol)
{
	const auto instStr = UTILS::toupper(symbol);
	auto existingInstruments = GetInstruments();
	if (!existingInstruments.erase(instStr))
	{
		return BoolResult(false, UTILS::Format("Instrument '%s' has not been subscribed", instStr));
	}
	
	m_settings.m_instruments = existingInstruments.empty() ? "" : UTILS::VecToStr(existingInstruments, ",");
	
	Unsubscribe({ instStr });
	return true;
}

//------------------------------------------------------------------------------
UTILS::BookUpdate::Ptr ConnectionBaseMD::ParseQuote(CORE::CRYPTO::PriceMessage::Levels &levels, const char side, const std::string &instrument)
{
	BookUpdate::Ptr nmd { std::make_unique<BookUpdate>() };
	BookUpdate::Entry *entry;
	
	nmd->entries.resize(levels.size());
	BidAskPair<int64_t> currentLevel { 0, 0 };
	for (size_t i { 0 }; i < levels.size(); ++i)
	{
		entry = &(nmd->entries[i]);
		entry->entryType = side;
		
		bool bid { entry->entryType.Bid() };

		entry->instrument = GetCurrencyPair(instrument);
		entry->price = std::stod(levels[i]->price);
		entry->volume = std::stod(levels[i]->size);
		entry->updateType = (entry->volume == 0) ? QT_DELETE : QT_NEW;
		entry->refId = entry->id = GenerateStandardEntryId(entry->instrument, entry->entryType, levels[i]->price);
		entry->quoteId = "";
		entry->positionNo = currentLevel.Get(bid);
	}
	return nmd;
}

//------------------------------------------------------------------------------
void ConnectionBaseMD::PublishQuotes(UTILS::BookUpdate::Ptr nmd)
{
	if (nmd)
	{
		int64_t key { 0 }, refKey { 0 };
		ActiveQuoteTable::QuoteInfoPtr replacedQuoteRef;

		const size_t cnt { nmd->entries.size() };
		uint64_t sequenceTag { std::hash<std::string>()("") };

		for (size_t i { 0 }; i < cnt; ++i)
		{
			BookUpdate::Entry &entry { nmd->entries[i] };
			entry.endOfMessage = (i == cnt - 1);
			entry.sequenceTag = sequenceTag;
			CurrencyPair cp = entry.instrument;
			if ((!entry.entryType.Valid() || !cp.Valid())) //no entry type with UPDATEs and DELETEs -> lookup entry and guess type
			{
				if (!entry.refId.empty())
				{
					ActiveQuoteTable::QuoteInfo quoteInfo;
					if (m_activeQuoteTable.FindQuoteInfo(entry.refId, quoteInfo))
					{
						if (!cp.Valid())
						{
							cp = quoteInfo.cp;
						}
						if (!entry.entryType.Valid())
						{
							entry.entryType = quoteInfo.entryType;
						}
					}
					else
					{
						poco_error_f3(logger(), "Session %ld - ERROR: No quote info found for entry '%s'->'%s' -> QUOTE SKIPPED", GetSettings().m_numId, entry.id,
									  entry.refId);
						continue;
					}
				}
				else
				{
					poco_error_f3(logger(), "Session %ld - ERROR: No entry type and/or symbol and no ref ID in entry '%s'-> '%s' -> QUOTE SKIPPED",
								  GetSettings().m_numId, entry.id, entry.refId);
					continue;
				}
			}

			key = NewInt64Key();
			if (entry.updateType == QT_DELETE)
			{
				replacedQuoteRef = entry.refId.empty() ? nullptr : m_activeQuoteTable.RemoveQuoteInfo(entry.refId);
			}
			else
			{
				replacedQuoteRef = m_activeQuoteTable.ReplaceQuoteInfo(entry.refId, entry.id, key, cp, entry.entryType);
			}
			if (replacedQuoteRef)
			{
				if (entry.updateType == QT_NEW) // NEW refers to existing quote-> UPDATE
				{
					entry.updateType = QT_UPDATE;
				}
				refKey = replacedQuoteRef->key;
			}
			else
			{
				if (entry.updateType == QT_DELETE)
				{
					poco_error_f3(logger(), "%ld - ERROR: DELETE referring to non-existent entry '%s' --> '%s'", GetSettings().m_numId, entry.id, entry.refId);
					return;
				}
				else if (entry.updateType == QT_UPDATE) // UPDATE -> NEW
				{
					entry.updateType = QT_NEW;
				}
				refKey = 0;
			}

			PublishQuote(key, refKey, CurrentTimestamp(), CurrentTimestamp(), cp, entry);
			m_publishedQuotesCounter++;
		}
	}
	else
	{
		poco_error(logger(), "ConnectionBaseMD::PublishQuotes: Normalized Market Data Ptr null");
	}
}

} // namespace CRYPTO
} // namespace CORE
