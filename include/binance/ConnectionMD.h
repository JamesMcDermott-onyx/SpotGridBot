#pragma once

#include "Definitions.h"
#include "ConnectionBaseMD.h"
#include "JSONDocument.h"
#include "binance/ConnectionSS.h"

namespace CORE {
namespace BINANCE {

// Message types
const std::string MSGTYPE_DepthUpdate = "depthUpdate";
const std::string MSGTYPE_DepthNUpdate = "depthNUpdate"; // the top 'n' depth

////////////////////////////////////////////////////////////////////////////
/*! \brief Binance connection class */
////////////////////////////////////////////////////////////////////////////
class ConnectionMD : public CORE::CRYPTO::ConnectionBaseMD
{
public:
	ConnectionMD(const CRYPTO::Settings &settings, const std::string &loggingPropsPath, const ConnectionManager& connectionManager);

	std::string TranslateSymbolToExchangeSpecific(const std::string &symbol) const override
	{
		const UTILS::CurrencyPair cp(symbol);
		return cp.BaseCCY().ToString() + cp.QuoteCCY().ToString();
	}
	
	/*! \brief detects message type from json document
	* @param jd: json document
	* @return: message type as a string
	* */
	
	void Subscribe(const CRYPTO::ConnectionBase::TInstruments &instruments) override
	{
		Subscribe(instruments, "SUBSCRIBE", GetDepth());
	}
	
	void Unsubscribe(const CRYPTO::ConnectionBase::TInstruments &instruments) override
	{
		Subscribe(instruments, "UNSUBSCRIBE");
	}

protected:
	int64_t m_lastUpdateId;
	std::unique_ptr<CORE::BINANCE::ConnectionSS> m_connectionSS; //REST Connector for Snapshot.
	
	/*! \brief called when Result message received
	* @param result: extracted result
	* @param id: extracted id
	* @param res: parsing result
	* */
	virtual void OnMsgResult(const std::string &result, const int id, const UTILS::BoolResult &res);
	
	/*! \brief called when Error message received
	* @param errCode: extracted error code
	* @param errMsg: extracted error message
	* @param res: parsing result
	* */
	virtual void OnMsgError(const int errCode, const std::string &errMsg, const UTILS::BoolResult &res);
	
	 void Start() override
	{
		const auto instruments = GetInstruments();
		Subscribe(instruments); //Binance we need to subscribe to stream and enqueue msgs until Snapshot is completed..
		Snapshot(instruments);
	}
	
	void Snapshot(const CRYPTO::ConnectionBase::TInstruments &instruments) override;
	
	/*! \brief subscribe/unsubscribe helper
	* @param instruments: list of instruments separated by comma
	* @param method: SUBSCRIBE or UNSUBSCRIBE
	* */
	void Subscribe(const CRYPTO::ConnectionBase::TInstruments &instruments, const std::string &method, unsigned int levels = 0);

private:
	void DepthUpdate(const std::shared_ptr<CRYPTO::JSONDocument> jd);
	
	//Parse Incremental update of 'n' diff depth...
	void DepthNUpdate(const std::shared_ptr<CRYPTO::JSONDocument> jd);
	
	std::unordered_map<UTILS::CurrencyPair, bool> m_snapshot;
	std::deque<std::shared_ptr<CRYPTO::JSONDocument>> m_messages;
};

} // ns BINANCE
}