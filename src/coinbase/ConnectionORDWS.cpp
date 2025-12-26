#include "coinbase/ConnectionORDWS.h"
#include "coinbase/JWTGenerator.h"
#include "ConnectionManager.h"
#include "Definitions.h"
#include "Tools.h"
#include "Utils/Result.h"
#include "Utils/FixDefs.h"

#include <nlohmann/json.hpp>
#include <Poco/URI.h>

using namespace UTILS;
using namespace CORE::CRYPTO;

namespace CORE {
namespace COINBASE {

// Message type constants for WebSocket order management
const char *const MSG_TYPE_ORDER_UPDATE = "update";
const char *const MSG_TYPE_ORDER_RESPONSE = "response";
const char *const MSG_TYPE_ORDER_ERROR = "error";
const char *const MSG_CHANNEL_ORDERS = "orders";

//------------------------------------------------------------------------------
ConnectionORDWS::ConnectionORDWS(const CRYPTO::Settings &settings, const std::string &loggingPropsPath, const ConnectionManager& connectionManager)
		: ConnectionBaseORD(settings, loggingPropsPath, settings.m_name, connectionManager)
{
	// Register message type detector
	GetMessageProcessor().Register([](const std::shared_ptr<CRYPTO::JSONDocument> message)
	{
		// Check for channel field first
		auto channel = message->GetValue<std::string>("channel");
		if (!channel.empty())
		{
			return channel;
		}
		
		// Check for type field
		auto msgType = message->GetValue<std::string>("type");
		if (!msgType.empty())
		{
			return msgType;
		}
		
		return std::string("unknown");
	});

	// Register message handlers
	GetMessageProcessor().Register(MSG_CHANNEL_ORDERS, [this](const std::shared_ptr<CRYPTO::JSONDocument> jd)
	{
		OnOrderUpdate(jd);
	});

	GetMessageProcessor().Register(MSG_TYPE_ORDER_RESPONSE, [this](const std::shared_ptr<CRYPTO::JSONDocument> jd)
	{
		OnOrderResponse(jd);
	});

	GetMessageProcessor().Register(MSG_TYPE_ORDER_ERROR, [this](const std::shared_ptr<CRYPTO::JSONDocument> jd)
	{
		OnOrderError(jd);
	});

	GetMessageProcessor().Register("subscriptions", [this](const std::shared_ptr<CRYPTO::JSONDocument> jd)
	{
		poco_information(logger(), "Received subscriptions confirmation");
	});
}

//------------------------------------------------------------------------------
void ConnectionORDWS::Subscribe(const CRYPTO::ConnectionBase::TInstruments &instruments)
{
	// Subscribe to user order updates channel
	nlohmann::json payload = {
		{"type", "subscribe"},
		{"channel", "user"},
		{"product_ids", nlohmann::json::array()}
	};

	// Add instruments
	for (const auto &inst : instruments)
	{
		payload["product_ids"].push_back(TranslateSymbolToExchangeSpecific(inst));
	}

	std::string signed_payload = CreateSignedOrderMessage(payload);
	Send(signed_payload);
}

//------------------------------------------------------------------------------
void ConnectionORDWS::Unsubscribe(const CRYPTO::ConnectionBase::TInstruments &instruments)
{
	nlohmann::json payload = {
		{"type", "unsubscribe"},
		{"channel", "user"},
		{"product_ids", nlohmann::json::array()}
	};

	for (const auto &inst : instruments)
	{
		payload["product_ids"].push_back(TranslateSymbolToExchangeSpecific(inst));
	}

	std::string signed_payload = CreateSignedOrderMessage(payload);
	Send(signed_payload);
}

//------------------------------------------------------------------------------
std::string ConnectionORDWS::CreateSignedOrderMessage(const nlohmann::json& payload)
{
	// Create JWT token
	std::string jwt_token = UTILS::create_jwt(
		GetSettings().m_apikey,
		GetSettings().m_secretkey
	);

	// Add JWT to payload
	nlohmann::json signed_payload = payload;
	signed_payload["jwt"] = jwt_token;

	return signed_payload.dump();
}

//------------------------------------------------------------------------------
std::string ConnectionORDWS::SendOrder(const UTILS::CurrencyPair &instrument, const UTILS::Side side, 
									   const RESTAPI::EOrderType orderType, const UTILS::TimeInForce timeInForce, 
									   const double price, const double quantity, const std::string &clientOrderId)
{
	// Generate client order ID if not provided
	std::string clOrdId = clientOrderId;
	if (clOrdId.empty())
	{
		clOrdId = "ws_" + std::to_string(UTILS::CurrentTimestamp()) + "_" + std::to_string(m_orderIdCounter++);
	}

	// Build order payload according to Coinbase Advanced Trade WebSocket API
	nlohmann::json order_config;
	if (orderType == RESTAPI::EOrderType::Limit)
	{
		if (timeInForce == UTILS::TimeInForce::GTC)
		{
			order_config["limit_limit_gtc"] = {
				{"base_size", std::to_string(quantity)},
				{"limit_price", std::to_string(price)},
				{"post_only", false}
			};
		}
		else // IOC
		{
			order_config["limit_limit_gtc"] = {
				{"base_size", std::to_string(quantity)},
				{"limit_price", std::to_string(price)}
			};
		}
	}
	else // Market order
	{
		order_config["market_market_ioc"] = {
			{"base_size", std::to_string(quantity)}
		};
	}

	nlohmann::json payload = {
		{"type", "order"},
		{"action", "create"},
		{"client_order_id", clOrdId},
		{"product_id", TranslateSymbolToExchangeSpecific(instrument)},
		{"side", side == UTILS::Side::BUY ? "BUY" : "SELL"},
		{"order_configuration", order_config}
	};

	// Store pending order
	{
		std::lock_guard<std::mutex> lock(m_pendingOrdersMutex);
		m_pendingOrders[clOrdId] = PendingOrder{clOrdId, instrument, side, price, quantity};
	}

	std::string signed_payload = CreateSignedOrderMessage(payload);
	Send(signed_payload);

	poco_information_f4(logger(), "Sent order: %s %s @%f qty=%f", 
		clOrdId, side == UTILS::Side::BUY ? "BUY" : "SELL", price, quantity);

	return clOrdId;
}

//------------------------------------------------------------------------------
std::string ConnectionORDWS::CancelOrder(const UTILS::CurrencyPair &instrument, const std::string &orderId,
										const std::optional<std::string> &origClientOrderId)
{
	nlohmann::json payload = {
		{"type", "order"},
		{"action", "cancel"},
		{"order_id", orderId}
	};

	if (origClientOrderId.has_value())
	{
		payload["client_order_id"] = origClientOrderId.value();
	}

	std::string signed_payload = CreateSignedOrderMessage(payload);
	Send(signed_payload);

	poco_information_f1(logger(), "Sent cancel order: %s", orderId);

	return orderId;
}

//------------------------------------------------------------------------------
std::string ConnectionORDWS::GetOrders()
{
	// For WebSocket, we subscribe to user channel and receive updates
	// This method could request order list if supported by API
	nlohmann::json payload = {
		{"type", "list_orders"}
	};

	std::string signed_payload = CreateSignedOrderMessage(payload);
	Send(signed_payload);

	return "list_orders_requested";
}

//------------------------------------------------------------------------------
void ConnectionORDWS::OnOrderUpdate(const std::shared_ptr<CRYPTO::JSONDocument> jd)
{
	try
	{
		// Parse order update from WebSocket
		auto events = jd->GetArray("events");
		if (!events || events->size() == 0)
		{
			poco_warning(logger(), "Order update has no events");
			return;
		}

		Poco::Dynamic::Array eventsArray = *events;
		
		for (size_t i = 0; i < events->size(); i++)
		{
			auto eventObj = eventsArray[i].extract<Poco::JSON::Object::Ptr>();
			if (!eventObj) continue;

			std::string order_id = eventObj->optValue<std::string>("order_id", "");
			std::string client_order_id = eventObj->optValue<std::string>("client_order_id", "");
			std::string status = eventObj->optValue<std::string>("status", "");
			std::string product_id = eventObj->optValue<std::string>("product_id", "");

			poco_information_f3(logger(), "Order update: id=%s, client_id=%s, status=%s",
				order_id, client_order_id, status);

			// Here you would translate this to ExecutionReportData and publish to system
			// Similar to how REST ConnectionORD does it
		}
	}
	catch (std::exception &e)
	{
		poco_error_f1(logger(), "Error processing order update: %s", std::string(e.what()));
	}
}

//------------------------------------------------------------------------------
void ConnectionORDWS::OnOrderResponse(const std::shared_ptr<CRYPTO::JSONDocument> jd)
{
	try
	{
		std::string response_type = jd->GetValue<std::string>("response_type");
		bool success = jd->GetValue<bool>("success");
		
		if (success)
		{
			poco_information_f1(logger(), "Order response: %s - SUCCESS", response_type);
		}
		else
		{
			std::string error_msg = jd->GetValue<std::string>("error_message");
			poco_error_f2(logger(), "Order response: %s - FAILED: %s", response_type, error_msg);
		}
	}
	catch (std::exception &e)
	{
		poco_error_f1(logger(), "Error processing order response: %s", std::string(e.what()));
	}
}

//------------------------------------------------------------------------------
void ConnectionORDWS::OnOrderError(const std::shared_ptr<CRYPTO::JSONDocument> jd)
{
	try
	{
		std::string error_message = jd->GetValue<std::string>("message");
		int error_code = jd->GetValue<int>("code");
		
		poco_error_f2(logger(), "Order error [%d]: %s", error_code, error_message);
	}
	catch (std::exception &e)
	{
		poco_error_f1(logger(), "Error processing order error message: %s", std::string(e.what()));
	}
}

//------------------------------------------------------------------------------
RESTAPI::RestConnectionBase::TExecutionReports 
ConnectionORDWS::TranslateOrderResult(const std::shared_ptr<CRYPTO::JSONDocument> jd) const
{
	RESTAPI::RestConnectionBase::TExecutionReports reports;
	
	try
	{
		std::string order_id = jd->GetValue<std::string>("order_id");
		std::string client_order_id = jd->GetValue<std::string>("client_order_id");
		
		std::string product_id = jd->GetValue<std::string>("product_id");
		auto instrument = GetCurrencyPair(TranslateSymbol(product_id));
		
		std::string side_str = jd->GetValue<std::string>("side");
		char side = (side_str == "BUY") ? Side::BUY : Side::SELL;
		
		// Translate order status
		std::string status = jd->GetValue<std::string>("status");
		auto [ordStatus, execType] = TranslateOrderStatus(status);
		
		// Parse quantities and prices
		double orderQty = std::stod(jd->GetValue<std::string>("order_size"));
		double limitPrice = std::stod(jd->GetValue<std::string>("limit_price"));
		double filledSize = std::stod(jd->GetValue<std::string>("filled_size"));
		double leavesQty = orderQty - filledSize;
		
		// Create execution report
		UTILS::MESSAGE::ExecutionReportData report(
			order_id,                          // orderId
			client_order_id,                   // clOrdID
			ORDTYPE_LIMIT,                     // ordType
			instrument,                        // instrument
			instrument.BaseCCY(),              // currency
			order_id,                          // execID (use order_id as execID)
			"",                                // settlDate
			execType,                          // execType
			ordStatus,                         // ordStatus
			side,                              // side
			instrument.DoubleToQty(orderQty),  // orderQty
			instrument.DblToCpip(limitPrice),  // orderPx
			instrument.DoubleToQty(filledSize),// lastQty
			instrument.DblToCpip(limitPrice),  // lastPx
			instrument.DoubleToQty(leavesQty), // leavesQty
			instrument.DoubleToQty(filledSize),// cumQty
			instrument.DblToCpip(limitPrice),  // avgPx
			"",                                // text
			"",                                // account
			"",                                // orderText
			"",                                // username
			"",                                // quoteId
			TimeInForce::GTC,                  // tif
			"",                                // customPbTag
			0                                  // transactionTime
		);
		
		reports.push_back(report);
	}
	catch (std::exception &e)
	{
		poco_error_f1(logger(), "Error translating order result: %s", std::string(e.what()));
	}
	
	return reports;
}

//------------------------------------------------------------------------------
RESTAPI::RestConnectionBase::TExecutionReports 
ConnectionORDWS::TranslateOrder(const std::shared_ptr<CRYPTO::JSONDocument> jd) const
{
	return TranslateOrderResult(jd);
}

//------------------------------------------------------------------------------
std::tuple<char, char> ConnectionORDWS::TranslateOrderStatus(const std::string &status)
{
	// Based on Coinbase order statuses
	// https://docs.cloud.coinbase.com/advanced-trade-api/docs/rest-api-orders
	
	if (status == "OPEN" || status == "PENDING" || status == ORD_STATUS_NAME_NEW)
	{
		return std::make_tuple(ORDSTATUS_NEW, EXECTYPE_NEW);
	}
	else if (status == "FILLED" || status == "DONE" || status == ORD_STATUS_NAME_FILLED)
	{
		return std::make_tuple(ORDSTATUS_FILLED, EXECTYPE_FILL);
	}
	else if (status == "CANCELLED" || status == ORD_STATUS_NAME_CANCELED)
	{
		return std::make_tuple(ORDSTATUS_CANCELED, EXECTYPE_CANCELED);
	}
	else if (status == "REJECTED" || status == "FAILED" || status == ORD_STATUS_NAME_REJECTED)
	{
		return std::make_tuple(ORDSTATUS_REJECTED, EXECTYPE_REJECTED);
	}
	else if (status == "PARTIALLY_FILLED" || status == ORD_STATUS_NAME_PARTIALLY_FILLED)
	{
		return std::make_tuple(ORDSTATUS_PARTIALLY_FILLED, EXECTYPE_PARTIAL_FILL);
	}
	else if (status == "EXPIRED" || status == ORD_STATUS_NAME_EXPIRED)
	{
		return std::make_tuple(ORDSTATUS_EXPIRED, EXECTYPE_EXPIRED);
	}
	
	// Default
	return std::make_tuple(ORDSTATUS_NEW, EXECTYPE_NEW);
}

} // ns COINBASE
} // ns CORE
