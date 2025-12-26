#pragma once

#include "ConnectionBaseORD.h"
#include "JSONDocument.h"
#include "RestConnectionBase.h"
#include <nlohmann/json.hpp>

namespace CORE {
	class ConnectionManager;
}

namespace CORE {
namespace COINBASE {

////////////////////////////////////////////////////////////////////////////
/*! \brief Coinbase WebSocket Order connection class */
////////////////////////////////////////////////////////////////////////////
class ConnectionORDWS : public CORE::CRYPTO::ConnectionBaseORD
{
public:
	ConnectionORDWS(const CRYPTO::Settings &settings, const std::string &loggingPropsPath, const ConnectionManager& connectionManager);

	// Order management methods
	std::string SendOrder(const UTILS::CurrencyPair &instrument, const UTILS::Side side, const RESTAPI::EOrderType orderType,
						  const UTILS::TimeInForce timeInForce, const double price, const double quantity,
						  const std::string &clientOrderId = "");
	
	std::string CancelOrder(const UTILS::CurrencyPair &instrument, const std::string &orderId,
							const std::optional<std::string> &origClientOrderId = std::nullopt);

	std::string GetOrders();

	// Translation methods for execution reports
	RESTAPI::RestConnectionBase::TExecutionReports TranslateOrderResult(const std::shared_ptr<CRYPTO::JSONDocument> jd) const;
	RESTAPI::RestConnectionBase::TExecutionReports TranslateOrder(const std::shared_ptr<CRYPTO::JSONDocument> jd) const;

	static std::tuple<char, char> TranslateOrderStatus(const std::string &status);

protected:
	void Subscribe(const CRYPTO::ConnectionBase::TInstruments &instruments) override;
	void Unsubscribe(const CRYPTO::ConnectionBase::TInstruments &instruments) override;

	// Message handlers
	virtual void OnOrderUpdate(const std::shared_ptr<CRYPTO::JSONDocument> jd);
	virtual void OnOrderResponse(const std::shared_ptr<CRYPTO::JSONDocument> jd);
	virtual void OnOrderError(const std::shared_ptr<CRYPTO::JSONDocument> jd);

	// Helper to create JWT-signed order messages
	std::string CreateSignedOrderMessage(const nlohmann::json& payload);

private:
	// Counter for client order IDs
	std::atomic<uint64_t> m_orderIdCounter{0};
	
	// Store pending orders for response matching
	struct PendingOrder {
		std::string clientOrderId;
		UTILS::CurrencyPair instrument;
		UTILS::Side side;
		double price;
		double quantity;
	};
	
	std::map<std::string, PendingOrder> m_pendingOrders;
	mutable std::mutex m_pendingOrdersMutex;
};

} // ns COINBASE
} // ns CORE
