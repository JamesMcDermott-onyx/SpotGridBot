#pragma once

#include "ConnectionBaseMD.h"
#include "OKX/Definitions.h"

namespace CORE {
namespace OKX {

    std::string Sign(
        const std::string& timeStampStr,
        const std::string& requestType,
        const std::string& requestPath,
        const std::string& requestBody,
        const std::string& secretKey);

    class ConnectionMD : public CORE::CRYPTO::ConnectionBaseMD {
    public:
		ConnectionMD(const CRYPTO::Settings& settings, const std::string& loggingPropsPath, const ConnectionManager& connectionManager);
		
        std::string TranslateSymbolToExchangeSpecific(const std::string& symbol) const override
        {
            return CORE::CRYPTO::TranslateSymbolToExchangeSpecific(symbol); // identical to coinbase
        }

        void Subscribe(const CRYPTO::ConnectionBase::TInstruments& instruments)
        {
			Subscribe(instruments, "subscribe");
        }

        void Unsubscribe(const CRYPTO::ConnectionBase::TInstruments& instruments)
        {
			Subscribe(instruments, "unsubscribe");
        }
	
		std::string TranslateSymbol(const std::string &symbol) const override
		{
			return CORE::CRYPTO::TranslateSymbol(symbol);
		}
		
	protected:

        void Subscribe(const CRYPTO::ConnectionBase::TInstruments& instruments, const std::string& method);

		void SideTranslator(const char *side, CRYPTO::PriceMessage::Levels &depth, const std::shared_ptr<CRYPTO::JSONDocument> jd) const override;
    };
	
} // ns OKX
}