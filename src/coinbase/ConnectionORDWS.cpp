#include "coinbase/ConnectionORDWS.h"
#include "coinbase/JWTGenerator.h"
#include "ConnectionManager.h"
#include "OrderManager.h"
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

	// Register handler for "user" channel messages (snapshots and updates)
	GetMessageProcessor().Register("user", [this](const std::shared_ptr<CRYPTO::JSONDocument> jd)
	{
		auto events = jd->GetArray("events");
		if (!events || events->size() == 0)
		{
			poco_debug(logger(), "user channel message has no events");
			return;
		}

		Poco::Dynamic::Array eventsArray = *events;
		for (size_t i = 0; i < events->size(); i++)
		{
			auto eventObj = eventsArray[i].extract<Poco::JSON::Object::Ptr>();
			if (!eventObj) continue;

			std::string event_type = eventObj->getValue<std::string>("type");
			
			if (event_type == "snapshot")
			{
				// Initial snapshot of orders and positions - sync to OrderManager
				poco_information(logger(), "Received user snapshot");
				
				if (eventObj->has("orders"))
				{
					auto orders = eventObj->getArray("orders");
					if (orders && orders->size() > 0)
					{
						int syncCount = 0;
						poco_information_f1(logger(), "Snapshot contains %d active orders - syncing to OrderManager", (int)orders->size());
						
						// Get OrderManager to sync orders
						auto orderManager = m_connectionManager.GetOrderManager();
						if (orderManager)
						{
							Poco::Dynamic::Array ordersArray = *orders;
							for (size_t j = 0; j < orders->size(); j++)
							{
								auto orderObj = ordersArray[j].extract<Poco::JSON::Object::Ptr>();
								if (!orderObj) continue;
								
								try
								{
									std::string order_id = orderObj->optValue<std::string>("order_id", "");
									std::string side_str = orderObj->optValue<std::string>("side", "");
									std::string status = orderObj->optValue<std::string>("status", "");
									std::string size_str = orderObj->optValue<std::string>("size", "0");
									std::string price_str = orderObj->optValue<std::string>("price", "0");
									std::string filled_size_str = orderObj->optValue<std::string>("filled_size", "0");
									
									if (order_id.empty()) continue;
									
									// Translate side
									UTILS::Side side = (side_str == "BUY") ? UTILS::Side::BUY : UTILS::Side::SELL;
									
									// Translate status
									CORE::OrderStatus orderStatus;
									if (status == "OPEN" || status == "PENDING")
										orderStatus = CORE::OrderStatus::NEW;
									else if (status == "FILLED" || status == "DONE")
										orderStatus = CORE::OrderStatus::FILLED;
									else if (status == "CANCELLED")
										orderStatus = CORE::OrderStatus::CANCELED;
									else if (status == "REJECTED" || status == "FAILED")
										orderStatus = CORE::OrderStatus::REJECTED;
									else if (status == "PARTIALLY_FILLED")
										orderStatus = CORE::OrderStatus::PARTIALLY_FILLED;
									else
										orderStatus = CORE::OrderStatus::NEW;
									
									// Parse numeric values
									double quantity = std::stod(size_str);
									double price = std::stod(price_str);
									double filled = std::stod(filled_size_str);
									
									// Sync to OrderManager
									orderManager->SyncOrder(order_id, side, price, quantity, orderStatus, filled);
									syncCount++;
								}
								catch (std::exception &e)
								{
									poco_warning_f1(logger(), "Error syncing order from snapshot: %s", std::string(e.what()));
								}
							}
							
							poco_information_f1(logger(), "Successfully synced %d orders to OrderManager cache", syncCount);
						}
						else
						{
							poco_warning(logger(), "OrderManager not available for snapshot sync");
						}
					}
					else
					{
						poco_information(logger(), "Snapshot: no active orders to sync");
					}
				}
			}
			else if (event_type == "update")
			{
				// Order update event
				poco_information(logger(), "Received user order update");
				OnOrderUpdate(jd);
			}
			else
			{
				poco_debug_f1(logger(), "Unhandled user event type: %s", event_type);
			}
		}
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

	std::string sideStr = (side == UTILS::Side::BUY) ? "BUY" : "SELL";
	std::string logMsg = "Sent order: " + clOrdId + " " + sideStr + 
	                     " @" + std::to_string(price) + " qty=" + std::to_string(quantity);
	poco_information(logger(), logMsg);

	// Return JSON response format matching REST API for OrderManager compatibility
	nlohmann::json response = {
		{"success", "true"},
		{"success_response", {
			{"order_id", clOrdId},
			{"client_order_id", clOrdId},
			{"product_id", TranslateSymbolToExchangeSpecific(instrument)},
			{"side", side == UTILS::Side::BUY ? "BUY" : "SELL"}
		}}
	};
	
	return response.dump();
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
			std::string filled_size_str = eventObj->optValue<std::string>("filled_size", "0");

			poco_information_f3(logger(), "Order update: id=%s, client_id=%s, status=%s",
				order_id, client_order_id, status);

			// Get OrderManager from ConnectionManager and update order status
			auto orderManager = m_connectionManager.GetOrderManager();
			if (orderManager)
			{
				// Translate Coinbase status to internal OrderStatus
				CORE::OrderStatus orderStatus;
				if (status == "OPEN" || status == "PENDING")
					orderStatus = CORE::OrderStatus::NEW;
				else if (status == "FILLED" || status == "DONE")
					orderStatus = CORE::OrderStatus::FILLED;
				else if (status == "CANCELLED")
					orderStatus = CORE::OrderStatus::CANCELED;
				else if (status == "REJECTED" || status == "FAILED")
					orderStatus = CORE::OrderStatus::REJECTED;
				else if (status == "PARTIALLY_FILLED")
					orderStatus = CORE::OrderStatus::PARTIALLY_FILLED;
				else
					orderStatus = CORE::OrderStatus::NEW; // Default

				double filled = 0.0;
				try {
					filled = std::stod(filled_size_str);
				} catch (...) {
					filled = 0.0;
				}

				// Push update to OrderManager
				orderManager->UpdateOrder(order_id, orderStatus, filled);
			}
			else
			{
				poco_warning(logger(), "OrderManager not available for order updates");
			}
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
