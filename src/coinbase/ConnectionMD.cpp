#include "ConnectionBase.h"
#include "CryptoCommon.h"

#include "coinbase/Messages.h"
#include "coinbase/helper_functions.h"
#include "coinbase/ConnectionMD.h"
#include "Poco/URI.h"
#include <vector>
#include <jwt-cpp/jwt.h>
#include "nlohmann/json.hpp"


#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Net/WebSocket.h>
#include <Poco/Net/SSLManager.h>
#include <Poco/Net/Context.h>
#include <Poco/Net/AcceptCertificateHandler.h>
#include <Poco/URI.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Stringifier.h>

namespace {

    using namespace Poco::Net;

    void initSSL()
    {
        static bool inited = false;
        if (inited) return;

        Context::Ptr context = new Context(
            Context::CLIENT_USE,
            "",
            "",
            "",
            Context::VERIFY_RELAXED,
            9,
            false,
            "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH"
        );

        SSLManager::instance().initializeClient(
            nullptr,
            new AcceptCertificateHandler(false),
            context
        );

        inited = true;
    }
}

using namespace UTILS;

namespace CORE {
    namespace COINBASE {
        //----------------------------------------------------------------------
        ConnectionMD::ConnectionMD(const CRYPTO::Settings &settings, const std::string &loggingPropsPath, const ConnectionManager& connectionManager)
            : ConnectionBase(settings, loggingPropsPath, settings.m_name, connectionManager) {

            initSSL();

            GetMessageProcessor().Register([](const std::shared_ptr<CRYPTO::JSONDocument> message)
                                            {
                                                return message->GetValue<std::string>("type");
                                            });

            // Register messages
            GetMessageProcessor().Register(MSG_TYPE_SNAPSHOT, [this](const std::shared_ptr<CRYPTO::JSONDocument> jd) {
                auto cp = GetCurrency(jd);
                if (!cp.Valid()) {
                    poco_error(logger(), "Invalid (or not supported) instrument ignored");
                    return;
                }
                const auto update = ParseMessage(jd, "bids", "asks");
                PublishQuotes(ParseQuote(update->Bids, QuoteType::BID, cp));
                PublishQuotes(ParseQuote(update->Asks, QuoteType::OFFER, cp));

                poco_information_f2(logger(), "QT_SNAPSHOT %s bid Levels: %d ", cp.ToString(), int(update->Bids.size()));
                poco_information_f2(logger(), "QT_SNAPSHOT %s ask Levels: %d ", cp.ToString(), int(update->Asks.size()));
            });

            GetMessageProcessor().Register(MSG_TYPE_L2UPDATE, [this](const std::shared_ptr<CRYPTO::JSONDocument> jd) {
                auto cp = GetCurrency(jd);
                if (!cp.Valid()) {
                    poco_error(logger(), "Invalid (or not supported) instrument - ignored");
                    return;
                }
                const auto publishFunc = [&cp, this](const std::vector<change::Ptr> changes) {
                    for (const auto &iter: changes) {
                        std::vector level{std::make_shared<CORE::CRYPTO::Level>(iter->price, iter->size)};
                        PublishQuotes(ParseQuote(level, (iter->side == "buy" ? QuoteType::BID : QuoteType::OFFER), cp));
                    }
                };

                publishFunc(L2Update(jd).GetChanges());
            });

            GetMessageProcessor().Register(MSG_TYPE_HEARTBEAT, [this](const std::shared_ptr<CRYPTO::JSONDocument> jd) {
                static int cnt = 0;
                if (cnt++ < 10) //We show a few msgs only on startup, to verify we are heart beating.
                {
                    poco_information_f1(logger(), "Received Heartbeat: %s", GetCurrency(jd).ToString());
                }
            });

            GetMessageProcessor().Register(MSG_TYPE_SUBSCRIPTIONS,
                                           [this](const std::shared_ptr<CRYPTO::JSONDocument> jd) {
                                               poco_information(logger(), "Received Subscription response..");
                                           });
        }

        //----------------------------------------------------------------------
        UTILS::CurrencyPair ConnectionMD::GetCurrency(const std::shared_ptr<CRYPTO::JSONDocument> msg) const {
            return GetCurrencyPair(TranslateSymbol(msg->GetValue<std::string>("product_id")));
        }

        //----------------------------------------------------------------------
        void ConnectionMD::Subscribe(const CRYPTO::ConnectionBase::TInstruments &instruments, const std::string &method,
                                     const std::string &channels) {
            // std::string prods;
            // for (const auto &inst: instruments) {
            //     prods += (prods.empty() ? "" : ",") + std::string("\"") + inst + "\"";
            // }

            // std::string payload = "{ \"type\": \"" + method + "\", \"product_ids\": [" + prods + "], \"channel\":  \"" + channels
            //             + "\" , \"jwt\": \"" + create_jwt(m_settings.m_apikey, m_settings.m_secretkey) +"\"}";

            //nlohmann::json payload = "{ \"type\": \"" + method + "\", \"product_ids\": [" + prods + "], \"channel\":  \"" + channels + "\" }";


            std::vector<std::string> products = { "BTC-USD" };

            nlohmann::json payload = {
                {"type", "subscribe"},
                {"channel", "level2"},
                {"product_ids", products}
            };

            auto signed_payload = sign_with_jwt(payload);
            Send(signed_payload);
        }
    }
}
