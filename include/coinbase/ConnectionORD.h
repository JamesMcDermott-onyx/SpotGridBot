#pragma once

#include "RestConnectionBase.h"
#include "JSONDocument.h"

namespace CORE {
	class ConnectionManager;
}

constexpr int CB_ACCESS_SIGN=0;
constexpr int CB_ACCESS_KEY=1;
constexpr int CB_ACCESS_PASSPHRASE=2;
constexpr int CB_ACCESS_TIMESTAMP= 3;

namespace CORE {
namespace COINBASE {

////////////////////////////////////////////////////////////////////////////
/*! \brief OKX connection class */
////////////////////////////////////////////////////////////////////////////
class ConnectionORD : public CORE::RESTAPI::RestConnectionBase
{
public:
	ConnectionORD(const CRYPTO::Settings &settings, const std::string &loggingPropsPath, const ConnectionManager& connectionManager);

	std::string GetOrders();
	
	std::string SendOrder(const UTILS::CurrencyPair &instrument, const UTILS::Side side, const RESTAPI::EOrderType orderType,
						  const UTILS::TimeInForce timeInForce, const double price, const double quantity,
						  const std::string &clientOrderId = "");
	
	std::string QueryOrder(const UTILS::CurrencyPair &instrument, const std::string &orderId,
						   const std::optional<std::string> &origClientOrderId = std::nullopt);
	
	std::string CancelOrder(const UTILS::CurrencyPair &instrument, const std::string &orderId,
							const std::optional<std::string> &origClientOrderId = std::nullopt);

	TExecutionReports TranslateOrderResult(const std::shared_ptr<CRYPTO::JSONDocument> jd) const override;
	
	TExecutionReports TranslateOrder(const std::shared_ptr<CRYPTO::JSONDocument> jd, Poco::Logger *logger = nullptr) const override;

	static std::tuple<char, char> TranslateOrderStatus(const std::string &status);

protected:
	const CRYPTO::AuthHeader GetAuthHeader(const std::string& requestPath, const std::string& accessMethod);

	// Web request wrapper
	std::string DoWebRequest(const std::string &url, const std::string &requestType,
							 std::function<void(std::string &path)> customizeRequestPathFunc = nullptr,
							 std::function<void(Poco::Net::HTTPRequest &request)> customizeRequestFunc = nullptr,
							 std::function<void(const Poco::Net::HTTPResponse &response)> customizeResponseFunc = nullptr,
							 std::function<void(std::ostream &)> handleRequestStreamFunc = nullptr);

	virtual void OnMsgResult(const std::string &result, const int id, const UTILS::BoolResult &res);
	virtual void OnMsgError(const int errCode, const std::string &errMsg, const UTILS::BoolResult &res);
	
private:

	enum class ERequestType
	{
		QueryOrder, CancelOrder
	};
	
	std::string QueryOrderOrCancel(const ERequestType requestType, const UTILS::CurrencyPair &instrument, const std::string &orderId,
								   const std::optional<std::string> &origClientOrderId = std::nullopt);

};

} // ns BINANCE
}
