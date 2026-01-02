#include <sstream>
#include "Definitions.h"
#include "RestConnectionBase.h"

#include "Tools.h"
#include "Utils/Result.h"
#include "Poco/URI.h"
#include "Poco/UUID.h"
#include "Poco/UUIDGenerator.h"
#include "coinbase/ConnectionORD.h"
#include "coinbase/JWTGenerator.h"
#include <jwt-cpp/jwt.h>

#include "Crypto.h"
#include "libbase64.hpp"

namespace CORE {
	class ConnectionManager;
}

using namespace UTILS;

using namespace CORE::CRYPTO;

namespace CORE {
namespace COINBASE {
// Fetch and log the products list from Coinbase
std::string ConnectionORD::ListProducts()
{
	const std::string requestPath("products");
	CRYPTO::AuthHeader header = GetAuthHeader(requestPath, "GET");

	std::string msg = this->DoWebRequest(this->m_settings.m_orders_http+requestPath, Poco::Net::HTTPRequest::HTTP_GET, [&](std::string &path) {},
		[&](Poco::Net::HTTPRequest &request) {
			request.add("content-type", "application/json");
			request.add("Authorization", "Bearer " + std::get<CB_ACCESS_SIGN>(header));
		});

	return msg;
}
// Fetch product details for a specific product_id
std::string ConnectionORD::GetProductDetails(const std::string& productId)
{
	const std::string requestPath("products/" + productId);
	CRYPTO::AuthHeader header = GetAuthHeader(requestPath, "GET");

	std::string msg = this->DoWebRequest(this->m_settings.m_orders_http+requestPath, Poco::Net::HTTPRequest::HTTP_GET, [&](std::string &path) {},
		[&](Poco::Net::HTTPRequest &request) {
			request.add("content-type", "application/json");
			request.add("Authorization", "Bearer " + std::get<CB_ACCESS_SIGN>(header));
		});

	return msg;
}
// Pretty print the products list from Coinbase
void ConnectionORD::PrettyPrintProducts(const std::string& productsJson)
{
	try {
		Poco::JSON::Parser parser;
		auto result = parser.parse(productsJson);
		auto obj = result.extract<Poco::JSON::Object::Ptr>();
		auto products = obj->getArray("products");
		if (!products) {
			poco_warning_f1(this->logger(), "No 'products' array in response: %s", productsJson);
			return;
		}
		std::ostringstream oss;
		oss << "\nPRODUCTS LIST (product_id | base_name | quote_name | status | trading_disabled)\n";
		oss << "-----------------------------------------------------------------------------------\n";
		for (size_t i = 0; i < products->size(); ++i) {
			auto prod = products->getObject(i);
			std::string pid = prod->getValue<std::string>("product_id");
			std::string base = prod->getValue<std::string>("base_name");
			std::string quote = prod->getValue<std::string>("quote_name");
			std::string status = prod->getValue<std::string>("status");
			bool trading_disabled = prod->getValue<bool>("trading_disabled");
			oss << pid << " | " << base << " | " << quote << " | " << status << " | " << (trading_disabled ? "YES" : "NO") << "\n";
		}
		poco_information_f1(this->logger(), "%s", oss.str());
	} catch (const std::exception& e) {
		poco_error_f1(this->logger(), "PrettyPrintProducts exception: %s", std::string(e.what()));
	}
}

// Debug: Send a minimal hardcoded JSON limit order to Coinbase
std::string ConnectionORD::SendTestLimitOrder()
{
       // First, list available products for debugging
       std::string productsResp = ListProducts();
       PrettyPrintProducts(productsResp);

       // Get details for BTC-USDC
       std::string productDetails = GetProductDetails("BTC-USDC");
       poco_information_f1(this->logger(), "Product details for BTC-USDC: %s", productDetails);

       // Get accounts
       std::string accounts = GetAccounts();
       poco_information_f1(this->logger(), "Accounts: %s", accounts);

       const std::string requestPath("orders");
       std::string uuid = "test-" + Poco::UUIDGenerator::defaultGenerator().createRandom().toString();
       std::string body =
	       "{"
	       "\"client_order_id\": \"test123\","
	       "\"product_id\": \"BTC-USDC\","
	       "\"side\": \"BUY\","
	       "\"order_configuration\": {"
	       "\"limit_limit_gtc\": {"
	       "\"base_size\": \"0.001\","
	       "\"limit_price\": \"94525.00\""
	       "}"
	       "}"
	       "}";
       std::string endpoint = this->m_settings.m_orders_http + requestPath;
       // Coinbase REST API authentication (JWT)
       CRYPTO::AuthHeader header = GetAuthHeader(requestPath, "POST");
       poco_information_f1(this->logger(), "SendTestLimitOrder endpoint: %s", endpoint);

       std::string msg = this->DoWebRequest(endpoint, Poco::Net::HTTPRequest::HTTP_POST, [&](std::string &path) {},
           [header, this, &body](Poco::Net::HTTPRequest &request) {
               request.setContentLength(body.size());
               request.add("content-type", "application/json");
               request.add("Authorization", "Bearer " + std::get<CB_ACCESS_SIGN>(header));
               // Log all headers
               std::ostringstream oss;
               oss << "SendTestLimitOrder HTTP Headers:" << std::endl;
               for (const auto& h : request)
                   oss << h.first << ": " << h.second << std::endl;
               poco_information_f1(this->logger(), "%s", oss.str());
           },
	       [&](const Poco::Net::HTTPResponse &response) {
		       this->m_logger.Session().Information(response.getReason());
	       },
	       [&](std::ostream &ostr) {
		       ostr << body;
		       poco_information_f1(this->logger(), "SendTestLimitOrder JSON: %s", body);
		       this->m_logger.Protocol().Outging(body);
	       });

       this->m_logger.Protocol().Incoming(msg);
       poco_information_f1(this->logger(), "SendTestLimitOrder response: %s", msg);
       return msg;
}

//------------------------------------------------------------------------------
ConnectionORD::ConnectionORD(const CRYPTO::Settings &settings, const std::string &loggingPropsPath, const ConnectionManager& connectionManager)
		: RESTAPI::RestConnectionBase(settings, loggingPropsPath, settings.m_name)
{
	GetMessageProcessor().Register([] (std::shared_ptr<CRYPTO::JSONDocument> jd)
								   {
									   // Try checking type field 'e'
									   auto msgType = jd->GetValue<std::string>("e");
									   if (!msgType.empty())
									   {
										   return msgType;
									   } // it was simple - type was in the field

									   // Try error message
									   // {"error":{"code":3,"msg":"Invalid JSON: expected `,` or `]` at line 4 column 6"}}
									   if (jd->Has(MSGTYPE_Error))
									   {
										   return MSGTYPE_Error;
									   }

									   // Try result message
									   // {"result":null,"id":1}
									   if (jd->Has(MSGTYPE_Result) && jd->Has("id"))
									   {
										   return MSGTYPE_Result;
									   }

									   return MSGTYPE_Unknown;
								   } );


	GetMessageProcessor().Register(MSGTYPE_Result, [this](const std::shared_ptr<CRYPTO::JSONDocument> jd)
	{
		OnMsgResult(jd->GetValue<std::string>("result"), jd->GetValue<int>("id"), true);
	});

	GetMessageProcessor().Register(MSGTYPE_Error, [this](const std::shared_ptr<CRYPTO::JSONDocument> jd)
	{
		try
		{
			auto errDesc = jd->GetSubObject("error");
			if (errDesc)
			{
				OnMsgError(errDesc->get("code").convert<int>(), errDesc->get("msg").toString(), true);
			}
			else
			{
				OnMsgError(0, "", UTILS::BoolResult(false, "Invalid error message descriptor"));
			}
		}
		catch (std::exception &e)
		{
			OnMsgError(0, "", UTILS::BoolResult(false, "Invalid error message descriptor: %s", std::string(e.what())));
		}
	});
}
//------------------------------------------------------------------------------
/*! \brief called when Result message received
* @param result: extracted result
* @param id: extracted id
* @param res: parsing result
* */
void ConnectionORD::OnMsgResult(const std::string &result, const int id, const UTILS::BoolResult &res)
{
	poco_information_f2(logger(), "received result='%s' for id='%s'", result, std::to_string(id));
}


//------------------------------------------------------------------------------
/*! \brief called when Error message received
* @param errCode: extracted error code
* @param errMsg: extracted error message
* @param res: parsing result
* */
void ConnectionORD::OnMsgError(const int errCode, const std::string &errMsg, const UTILS::BoolResult &res)
{
	poco_error_f2(logger(), "received 'error': code='%s', msg='%s'", std::to_string(errCode), errMsg);
}

//Create JWT authentication token for Coinbase Advanced Trade API
const AuthHeader ConnectionORD::GetAuthHeader(const std::string& requestPath, const std::string& accessMethod)
{
	// Format URI as: "METHOD hostname/full/path"
	// Example: "GET api.coinbase.com/api/v3/brokerage/accounts"
	std::string uri = accessMethod + " api.coinbase.com/api/v3/brokerage/" + requestPath;
	
	std::string jwt_token = UTILS::create_jwt(
		m_settings.m_apikey,
		m_settings.m_secretkey,
		"",  // request_method (already included in uri)
		uri  // Full URI string
	);

	// Return JWT token in the sign field for Bearer authentication
	return AuthHeader(jwt_token, m_settings.m_apikey, "", "");
}

//------------------------------------------------------------------------------
std::string ConnectionORD::GetOrders()
{
	const std::string requestPath("orders/historical/batch");
	CRYPTO::AuthHeader header = GetAuthHeader(requestPath, "GET");

	return DoWebRequest(m_settings.m_orders_http+requestPath, Poco::Net::HTTPRequest::HTTP_GET, [&](std::string &path)
	{
	}, [&](Poco::Net::HTTPRequest &request)
						{
							request.add("content-type", "application/json");
							request.add("Authorization", "Bearer " + std::get<CB_ACCESS_SIGN>(header));
						});
}

//------------------------------------------------------------------------------
std::string ConnectionORD::GetAccounts()
{
	const std::string requestPath("accounts");
	CRYPTO::AuthHeader header = GetAuthHeader(requestPath, "GET");

	return DoWebRequest(m_settings.m_orders_http+requestPath, Poco::Net::HTTPRequest::HTTP_GET, [&](std::string &path)
	{
	}, [&](Poco::Net::HTTPRequest &request)
						{
							request.add("content-type", "application/json");
							request.add("Authorization", "Bearer " + std::get<CB_ACCESS_SIGN>(header));
						});
}

//------------------------------------------------------------------------------
std::string ConnectionORD::SendOrder(const UTILS::CurrencyPair &instrument, const UTILS::Side side, const RESTAPI::EOrderType orderType,
                                  const UTILS::TimeInForce timeInForce, const double price, const double quantity, const std::string &clientOrderId)
{
	const std::string requestPath("orders");
	CRYPTO::AuthHeader header = GetAuthHeader(requestPath, "POST");
	
	// Generate unique client order ID
	Poco::UUIDGenerator& generator = Poco::UUIDGenerator::defaultGenerator();
	Poco::UUID uuid = generator.createRandom();
	std::string uniqueClientOrderId = uuid.toString();
	
	std::string body("{ \"client_order_id\": \"" + uniqueClientOrderId + "\", \"product_id\":");
	body+="\""+TranslateSymbolToExchangeSpecific(instrument)+"\"";
	body+=",\"side\":";
	body+=side==UTILS::Side::BUY?"\"BUY\"":"\"SELL\"";
	body+=",\"order_configuration\":{";
	body+=timeInForce==UTILS::TimeInForce::GTC ? "\"limit_limit_gtc\":{" : "\"limit_limit_ioc\":{";
	body+="\"limit_price\":";
	body+="\""+std::to_string(price)+"\"";
	body+=",\"base_size\":";
	body+="\""+std::to_string(quantity)+"\"";
	body+=",\"post_only\":false";
	body+="}}}";

	std::string msg = DoWebRequest(m_settings.m_orders_http+requestPath, Poco::Net::HTTPRequest::HTTP_POST, [&](std::string &path)
	{

	},
	[&](Poco::Net::HTTPRequest &request)
						{
							request.setContentLength(body.size());
							request.add("content-type", "application/json");
							request.add("Authorization", "Bearer " + std::get<CB_ACCESS_SIGN>(header));
						},
	[&](const Poco::Net::HTTPResponse &response)
	{
		m_logger.Session().Information(response.getReason());
	},
	[&](std::ostream &ostr) {
		ostr << body;
		poco_information_f1(logger(), "SendOrder JSON: %s", body);
		m_logger.Protocol().Outging(body);
	});

	m_logger.Protocol().Incoming(msg);
	poco_information_f1(logger(), "SendOrder response: %s", msg);
	return msg;
 }

//------------------------------------------------------------------------------
std::string ConnectionORD::CancelOrder(const UTILS::CurrencyPair &instrument, const std::string &orderId,
									const std::optional<std::string> &origClientOrderId)
{
	const std::string requestPath("orders/batch_cancel");
	CRYPTO::AuthHeader header = GetAuthHeader(requestPath, "POST");
	std::string body("{ \"order_ids\": [ \""+orderId +"\"]}");

	std::string msg = DoWebRequest(m_settings.m_orders_http+requestPath, Poco::Net::HTTPRequest::HTTP_POST, [&](std::string &path)
	{

	},
	[&](Poco::Net::HTTPRequest &request)
						{
							request.setContentLength(body.size());
							request.add("CB-ACCESS-KEY", std::get<CB_ACCESS_KEY>(header));
							request.add("CB-ACCESS-SIGN", std::get<CB_ACCESS_SIGN>(header));
							request.add("CB-ACCESS-TIMESTAMP", std::get<CB_ACCESS_TIMESTAMP>(header));
							request.add("content-type", "application/json");
						},
	[&](const Poco::Net::HTTPResponse &response)
	{
		m_logger.Session().Information(response.getReason());
	},
	[&](std::ostream &ostr) {
		ostr << body;
		m_logger.Protocol().Outging(body);
	});

	m_logger.Protocol().Incoming(msg);
	return msg;
}

std::string ConnectionORD::QueryOrder(const UTILS::CurrencyPair &instrument, const std::string &orderId,
						   const std::optional<std::string> &origClientOrderId) {

	const std::string requestPath("orders/");
	CRYPTO::AuthHeader header = GetAuthHeader(requestPath, "POST");
	std::string body("{ \"order_ids\": [ \""+orderId +"\"]}");

	std::string msg = DoWebRequest(m_settings.m_orders_http+requestPath, Poco::Net::HTTPRequest::HTTP_POST, [&](std::string &path)
	{

	},
	[&](Poco::Net::HTTPRequest &request)
						{
							request.setContentLength(body.size());
							request.add("CB-ACCESS-KEY", std::get<CB_ACCESS_KEY>(header));
							request.add("CB-ACCESS-SIGN", std::get<CB_ACCESS_SIGN>(header));
							request.add("CB-ACCESS-TIMESTAMP", std::get<CB_ACCESS_TIMESTAMP>(header));
							request.add("content-type", "application/json");
						},
	[&](const Poco::Net::HTTPResponse &response)
	{
		m_logger.Session().Information(response.getReason());
	},
	[&](std::ostream &ostr) {
		ostr << body;
		m_logger.Protocol().Outging(body);
	});

	m_logger.Protocol().Incoming(msg);
	return msg;
}


//==============================================================================
// Binance web request wrapper
std::string ConnectionORD::DoWebRequest(const std::string &url, const std::string &requestType,
									 std::function<void(std::string &path)> customizeRequestPathFunc,
									 std::function<void(Poco::Net::HTTPRequest &request)> customizeRequestFunc,
									 std::function<void(const Poco::Net::HTTPResponse &response)> customizeResponseFunc,
									 std::function<void(std::ostream &)> handleRequestStreamFunc)
{
	return ExecuteWebRequest(url, requestType, customizeRequestPathFunc, customizeRequestFunc, customizeResponseFunc, handleRequestStreamFunc);
}

//------------------------------------------------------------------------------
// static
std::tuple<char, char> ConnectionORD::TranslateOrderStatus(const std::string &status)
{
	if (status == ORD_STATUS_NAME_NEW)
	{
		return std::make_tuple(ORDSTATUS_NEW, EXECTYPE_NEW);
	}
	else if (status == ORD_STATUS_NAME_PARTIALLY_FILLED)
	{
		return std::make_tuple(ORDSTATUS_PARTIALLY_FILLED, EXECTYPE_PARTIAL_FILL);
	}
	else if (status == ORD_STATUS_NAME_FILLED)
	{
		return std::make_tuple(ORDSTATUS_FILLED, EXECTYPE_FILL);
	}
	else if (status == ORD_STATUS_NAME_CANCELED)
	{
		return std::make_tuple(ORDSTATUS_CANCELED, EXECTYPE_CANCELED);
	}
	else if (status == ORD_STATUS_NAME_EXPIRED)
	{
		return std::make_tuple(ORDSTATUS_EXPIRED, EXECTYPE_EXPIRED);
	}
	
	// for any unknown state we consider it "REJECTED" (including ORD_STATUS_NAME_REJECTED)
	return std::make_tuple(ORDSTATUS_REJECTED, EXECTYPE_REJECTED);
}


//------------------------------------------------------------------------------
/*! \brief Translates order result
* @return: ExecutionReportData
* */
CORE::RESTAPI::RestConnectionBase::TExecutionReports ConnectionORD::TranslateOrderResult(const std::shared_ptr<CRYPTO::JSONDocument> jd) const
{
	return TranslateOrder(jd, &logger());
}

// static
CORE::RESTAPI::RestConnectionBase::TExecutionReports ConnectionORD::TranslateOrder(const std::shared_ptr<CRYPTO::JSONDocument> jd, Poco::Logger *logger) const
{
	// Check possible error
	CORE::RESTAPI::RestConnectionBase::TExecutionReports execs;
	const auto errCode = jd->GetValue<std::string>("code");
	if (!errCode.empty())
	{
		// It's an error
		// (expected format is like {"code":-1013,"msg":"Price * QTY is zero or less."})
		auto singleExecution { CORE::CRYPTO::TOOLS::CreateEmptyExecutionReportData() };
		singleExecution.m_ordStatus = ORDSTATUS_REJECTED;
		singleExecution.m_execType = EXECTYPE_REJECTED;
		singleExecution.m_text = UTILS::Format("The order has failed: Error code='%s', message='%s'", errCode, jd->GetValue<std::string>("msg"));
		if (logger)
		{
			poco_error_f1(*logger, "Connection::TranslateOrderResult error %s: ", singleExecution.m_text);
		}
		execs.push_back(singleExecution);
		return execs;
	}
	std::string arrayName { "fills" };
	
	CurrencyPair instrument { UTILS::CurrencyPair(jd->GetValue<std::string>("symbol")) };
	
	if (!instrument.Valid() && logger)
	{
		poco_error_f1(*logger, "Invalid instrument in exec report %s", instrument.ToString());
	}
	
	const auto orderQty = jd->GetValue<double>("origQty");
	const auto [ordStatus, ordExecType] = ConnectionORD::TranslateOrderStatus(jd->GetValue<std::string>("status"));
	
	double executedQty { 0. };
	
	// Returns pre-filled execution report
	const auto prepareExecReportFunc = [&]
	{
		auto singleExecution { CORE::CRYPTO::TOOLS::CreateEmptyExecutionReportData() };
		singleExecution.m_orderId = jd->GetValue<std::string>("orderId");
		singleExecution.m_clOrdID = jd->GetValue<std::string>("clientOrderId");
		singleExecution.m_ordType = jd->GetValue<std::string>("type") == "MARKET" ? ORDTYPE_MARKET : ORDTYPE_LIMIT;
		singleExecution.m_instrument = instrument;
		singleExecution.m_currency = instrument.BaseCCY();
		const auto side = jd->GetValue<std::string>("side");
		singleExecution.m_side = side == "SELL" ? UTILS::Side::SELL : (side == "BUY" ? UTILS::Side::BUY : UTILS::Side::INVALID);
		singleExecution.m_tif = UTILS::TimeInForce(jd->GetValue<std::string>("timeInForce"));
		singleExecution.m_orderPx = jd->GetValue<double>("price");
		singleExecution.m_orderQty = orderQty;
		singleExecution.m_ordStatus = ordStatus;
		singleExecution.m_execType = ordExecType;
		return singleExecution;
	}; // prepareExecReportFunc
	
	// Returns single execution array
	const auto generateSingleExecutionFunc = [&]
	{
		// we have no fills - just return the exec report
		auto singleExecution = prepareExecReportFunc();
		singleExecution.m_cumQty = jd->GetValue<double>("cummulativeQuoteQty");
		singleExecution.m_leavesQty = orderQty;
		execs.push_back(singleExecution);
	};
	
	if (auto array = jd->GetArray(arrayName))
	{
		Poco::Dynamic::Array fills = *array;
		auto numExecs { array->size() };
		
		if (numExecs == 0)
		{
			// Empty fills - just a single execution
			generateSingleExecutionFunc();
		}
		
		for (size_t i { 0 }; i < numExecs; ++i)
		{
			auto singleExecution = prepareExecReportFunc();
			
			singleExecution.m_lastPx = fills[i]["price"];
			singleExecution.m_lastQty = fills[i]["qty"];
			executedQty += singleExecution.m_lastQty;
			singleExecution.m_cumQty = executedQty;
			singleExecution.m_leavesQty = orderQty - executedQty;
			
			if (i < numExecs - 1)
			{
				singleExecution.m_ordStatus = ORDSTATUS_PARTIALLY_FILLED;
				singleExecution.m_execType = EXECTYPE_PARTIAL_FILL;
			}
			execs.push_back(singleExecution);
		}
	}
	else
	{
		// No fills - just a single execution
		generateSingleExecutionFunc();
	}
	return execs;
}

}
}
