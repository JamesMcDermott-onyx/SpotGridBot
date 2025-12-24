#include "ConnectionBase.h"
#include "CryptoCommon.h"

#include "coinbase/Messages.h"
#include "coinbase/ConnectionMD.h"
#include "coinbase/helper_functions.h"
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
                                                // Try "channel" field first (new Advanced Trade API)
                                                auto channel = message->GetValue<std::string>("channel");
                                                if (!channel.empty())
                                                    return channel;
                                                // Fall back to "type" field (old API)
                                                return message->GetValue<std::string>("type");
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

            // Handler for new Advanced Trade API l2_data channel
            GetMessageProcessor().Register(MSG_TYPE_L2DATA, [this](const std::shared_ptr<CRYPTO::JSONDocument> jd) {
                // New format: {"channel":"l2_data","events":[{"type":"update","product_id":"BTC-USD","updates":[...]}]}
                auto events = jd->GetArray("events");
                if (!events || events->size() == 0) {
                    poco_warning(logger(), "l2_data message has no events");
                    return;
                }
                
                Poco::Dynamic::Array eventsArray = *events;
                
                // Process each event in the array
                for (size_t i = 0; i < events->size(); i++) {
                    auto eventObj = eventsArray[i].extract<Poco::JSON::Object::Ptr>();
                    if (!eventObj) continue;
                    
                    std::string product_id = eventObj->getValue<std::string>("product_id");
                    auto cp = GetCurrencyPair(TranslateSymbol(product_id));
                    if (!cp.Valid()) {
                        poco_error(logger(), "Invalid (or not supported) instrument - ignored");
                        continue;
                    }
                    
                    std::string event_type = eventObj->getValue<std::string>("type");
                    if (event_type == "update") {
                        // Handle update: updates array with side, price_level, new_quantity
                        auto updates = eventObj->getArray("updates");
                        if (updates) {
                            Poco::Dynamic::Array updatesArray = *updates;
                            for (size_t j = 0; j < updates->size(); j++) {
                                auto updateObj = updatesArray[j].extract<Poco::JSON::Object::Ptr>();
                                if (!updateObj) continue;
                                
                                std::string side = updateObj->getValue<std::string>("side");
                                std::string price = updateObj->getValue<std::string>("price_level");
                                std::string qty = updateObj->getValue<std::string>("new_quantity");
                                
                                std::vector level{std::make_shared<CORE::CRYPTO::Level>(price, qty)};
                                PublishQuotes(ParseQuote(level, (side == "bid" ? QuoteType::BID : QuoteType::OFFER), cp));
                            }
                            poco_information_f2(logger(), "l2_data UPDATE %s: %d updates", cp.ToString(), (int)updates->size());
                        }
                    }
                }
            });
        }

        //----------------------------------------------------------------------
        UTILS::CurrencyPair ConnectionMD::GetCurrency(const std::shared_ptr<CRYPTO::JSONDocument> msg) const {
            return GetCurrencyPair(TranslateSymbol(msg->GetValue<std::string>("product_id")));
        }

        //----------------------------------------------------------------------
        void ConnectionMD::Subscribe(const CRYPTO::ConnectionBase::TInstruments &instruments, const std::string &method,
                                     const std::string &channels) {

            std::vector<std::string> products;
            for (const auto &inst: instruments) {
                products.push_back(inst);
            }

            nlohmann::json payload = {
                {"type", "subscribe"},
                {"channel", channels},
                {"product_ids", products}
            };

            auto signed_payload = sign_with_jwt(payload);
            Send(signed_payload);
        }
    }
}
