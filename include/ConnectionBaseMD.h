#pragma once

#include "ConnectionBase.h"
#include "ActiveQuoteTable.h"

namespace CORE {

namespace CRYPTO {

////////////////////////////////////////////////////////////////////////////
/*! \brief Market Data Connection Base Class
 * 
 * Inherits from ConnectionBase and adds market data specific functionality
 * for handling order book updates, quotes, and market data subscriptions.
 */
////////////////////////////////////////////////////////////////////////////
class ConnectionBaseMD : public ConnectionBase
{
public:
	ConnectionBaseMD(const CRYPTO::Settings &settings, const std::string &loggingPropsPath, 
	                 const std::string &loggerName, const ConnectionManager& connectionManager);
	
	virtual ~ConnectionBaseMD();
	
	/*! \brief Publishes market data quotes to the system */
	void PublishQuotes(UTILS::BookUpdate::Ptr nmd);
	
	/*! \brief Returns depth from configuration */
	unsigned int GetDepth() const
	{
		return GetSettings().m_depth;
	}
	
	/*! \brief Subscribe to a specific instrument for market data */
	UTILS::BoolResult SubscribeInstrument(const std::string &symbol);
	
	/*! \brief Unsubscribe from a specific instrument */
	UTILS::BoolResult UnsubscribeInstrument(const std::string &symbol);
	
	/*! \brief Parse quote from price levels */
	UTILS::BookUpdate::Ptr ParseQuote(PriceMessage::Levels &levels, const char side, const std::string &instrument);
	
	/*! \brief Start market data connection - subscribes to instruments */
	void Start() override
	{
		const auto instruments = GetInstruments();
		Snapshot(instruments);
		Subscribe(instruments);
	}
	
	/*! \brief Processing snapshot for each instrument
	* @param instruments: set of instruments
	* */
	virtual void Snapshot(const TInstruments &instruments) { }
	
	/*! \brief Subscribe to market data for instruments */
	virtual void Subscribe(const TInstruments &instruments) { }
	
	/*! \brief Unsubscribe from market data for instruments */
	virtual void Unsubscribe(const TInstruments &instruments) { };
	
	/*! \brief Helper to translate side of order book from JSON */
	virtual void SideTranslator(const char *side, PriceMessage::Levels &depth, const std::shared_ptr<JSONDocument> jd) const
	{
		if (auto levels = jd->GetArray(side))
		{
			Poco::Dynamic::Array da = *levels;
			for (size_t i = 0; i < levels->size(); ++i)
			{
				depth.emplace_back(std::make_shared<Level>());
				depth.back()->price = da[i][0].toString();
				depth.back()->size = da[i][1].toString();
			}
		}
	}
	
	/*! \brief Parse market data message from JSON */
	virtual std::unique_ptr<PriceMessage> ParseMessage(const std::shared_ptr<JSONDocument> jd, 
	                                                     const std::string &bidName, const std::string &askName) const
	{
		auto msg = std::make_unique<PriceMessage>();
		SideTranslator(bidName.c_str(), msg->Bids, jd);
		SideTranslator(askName.c_str(), msg->Asks, jd);
		return msg;
	}

protected:
	/*! \brief Publish individual quote entry */
	UTILS::BoolResult PublishQuote(int64_t key, int64_t refKey, int64_t timestamp,
	                                int64_t receiveTime, UTILS::CurrencyPair cp, const UTILS::BookUpdate::Entry &entry);

private:
	CORE::ActiveQuoteTable m_activeQuoteTable;
	
	// Number of published quotes
	std::atomic<unsigned long> m_publishedQuotesCounter { 0 };
	mutable std::atomic<unsigned long> m_publishedQuotesOld { 0 }; // to calculate delta
};

} // namespace CRYPTO
} // namespace CORE
