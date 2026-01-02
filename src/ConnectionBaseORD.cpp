#include "ConnectionBaseORD.h"
#include "ConnectionManager.h"

namespace CORE {
namespace CRYPTO {

ConnectionBaseORD::ConnectionBaseORD(const CRYPTO::Settings &settings, const std::string &loggingPropsPath, 
                                     const std::string &loggerName, const ConnectionManager& connectionManager)
	: ConnectionBase(settings, loggingPropsPath, loggerName, connectionManager)
{
}

ConnectionBaseORD::~ConnectionBaseORD()
{
}

} // namespace CRYPTO
} // namespace CORE
