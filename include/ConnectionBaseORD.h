#pragma once

#include "ConnectionBase.h"

namespace CORE {

namespace CRYPTO {

////////////////////////////////////////////////////////////////////////////
/*! \brief Order Management Connection Base Class
 * 
 * Inherits from ConnectionBase and adds order management specific functionality
 * for handling order placement, cancellation, and execution reports.
 */
////////////////////////////////////////////////////////////////////////////
class ConnectionBaseORD : public ConnectionBase
{
public:
	ConnectionBaseORD(const CRYPTO::Settings &settings, const std::string &loggingPropsPath, 
	                  const std::string &loggerName, const ConnectionManager& connectionManager);
	
	virtual ~ConnectionBaseORD();
	
	/*! \brief Start order connection - subscribes to order updates */
	void Start() override
	{
		const auto instruments = GetInstruments();
		Subscribe(instruments);
	}
	
	/*! \brief Subscribe to order updates for instruments */
	virtual void Subscribe(const TInstruments &instruments) { }
	
	/*! \brief Unsubscribe from order updates for instruments */
	virtual void Unsubscribe(const TInstruments &instruments) { };

protected:
	// Order management specific methods can be added here as virtual functions
	// for derived classes to implement
};

} // namespace CRYPTO
} // namespace CORE
