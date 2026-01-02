#pragma once

#include "Config.h"
#include "ConnectionBaseMD.h"

#include "coinbase/Messages.h"

namespace CORE {
namespace COINBASE {
const char *const SCHEMA = "Coinbase";
// Message types
const char *const MSG_TYPE_L2DATA = "l2_data";  // New Advanced Trade API
const char *const MSG_TYPE_HEARTBEAT = "heartbeat";
const char *const MSG_TYPE_SUBSCRIPTIONS = "subscriptions";

class ConnectionMD : public CORE::CRYPTO::ConnectionBaseMD
{
public:
	ConnectionMD(const CRYPTO::Settings &settings, const std::string &loggingPropsPath, const ConnectionManager& connectionManager);

	UTILS::CurrencyPair GetCurrency(const std::shared_ptr<CRYPTO::JSONDocument> msg) const;

	std::string TranslateSymbol(const std::string &symbol) const override
	{
		return CORE::CRYPTO::TranslateSymbol(symbol);
	}
	
	std::string TranslateSymbolToExchangeSpecific(const std::string &symbol) const override
	{
		return CORE::CRYPTO::TranslateSymbolToExchangeSpecific(symbol);
	}

private:
	void Subscribe(const CRYPTO::ConnectionBase::TInstruments &instruments) override
	{
		Poco::StringTokenizer tok(GetSettings().m_channels, ",", Poco::StringTokenizer::TOK_TRIM | Poco::StringTokenizer::TOK_IGNORE_EMPTY);
		for (size_t i = 0; i < tok.count(); ++i)
		{
			Subscribe(instruments, "subscribe", tok[i]);
		}
	}
	
	void Unsubscribe(const CRYPTO::ConnectionBase::TInstruments &instruments) override
	{
		Poco::StringTokenizer tok(GetSettings().m_channels, ",", Poco::StringTokenizer::TOK_TRIM | Poco::StringTokenizer::TOK_IGNORE_EMPTY);
		for (size_t i = 0; i < tok.count(); ++i)
		{
			Subscribe(instruments, "unsubscribe", tok[i]);
		}
	}

	void Subscribe(const CRYPTO::ConnectionBase::TInstruments &instruments, const std::string &method, const std::string &channel);
};
}
}