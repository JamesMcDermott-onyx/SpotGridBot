#include "ConnectionBase.h"

#include "ConnectionManager.h"
#include "Utils/Result.h"

#include "Poco/URI.h"
#include "CryptoCommon.h"

using namespace UTILS;
using namespace Poco;

namespace CORE {
namespace CRYPTO {
ConnectionBase::ConnectionBase(const CRYPTO::Settings &settings, const std::string &loggingPropsPath, const std::string &loggerName, const ConnectionManager& connectionManager)
		: Logging(loggerName), m_settings(settings), m_logger(settings, loggingPropsPath), m_connectionManager(connectionManager)
{
	// Make sure all instruments are in upper case
	m_settings.m_instruments = UTILS::toupper(m_settings.m_instruments);
}


//------------------------------------------------------------------------------
ConnectionBase::~ConnectionBase()
{
	Disconnect();
}


//------------------------------------------------------------------------------
BoolResult ConnectionBase::Connect()
{
	if (!m_connected)
	{
		try
		{
			poco_information_f2(logger(), "Session '%s' connecting to endpoint %s ", m_settings.m_name, m_settings.m_host );
			CreateWebSocket();
		}
		catch (Poco::Exception &e)
		{
			poco_error_f2(logger(), "Exception in session '%s' when attempting to create a websocket: %s", m_settings.m_name,
						  std::string(e.displayText()));
			return false;
		}
		
		m_messageProcessor.Start();
		m_connected = true;
		
		// Start listener thread..
		m_listenerThread = std::make_unique<std::thread>([this]()
														 {
															 int exceptionCounter = 0;
															 while (m_connected)
															 {
																 try
																 {
																	 memset(m_buffer, 0, sizeof(m_buffer));
																	 int flags { };
																	 const auto bytes =
																			 ReceiveWebSocketData(m_ws.get(), m_buffer, sizeof(m_buffer), flags);
					
																	 // Process ping/pong
																	 using namespace Poco::Net;
																	 if ((flags & WebSocket::FRAME_OP_BITMASK) == WebSocket::FRAME_OP_PING)
																	 {
																		 poco_information(logger(), "received PING");
																	 	m_ws->sendFrame(
																			 m_buffer,
																			 bytes,  // ✅ MUST echo full payload
																			 WebSocket::FRAME_FLAG_FIN | WebSocket::FRAME_OP_PONG
																		 );

																		 poco_information(logger(), "sent PONG successfully");
																		 continue;
																	 }
																	 if ((flags & WebSocket::FRAME_OP_BITMASK) == WebSocket::FRAME_OP_PONG)
																	 {
																		 poco_information(logger(), "received PONG: ignored");
																		 continue;
																	 }

																 	if ((flags & WebSocket::FRAME_OP_BITMASK) == WebSocket::FRAME_OP_CLOSE)
																 	{
																		 poco_error(logger(), "socket closed at source...");
																		 m_ws->sendFrame(nullptr, 0,
																			 WebSocket::FRAME_FLAG_FIN | WebSocket::FRAME_OP_CLOSE);
																		 m_connected = false;
																		 return;
																	 }


																	 // Processing bytes
																	 if (bytes)
																	 {
																	 	std::string msg(m_buffer, bytes);
																		 const auto res = GetMessageProcessor().ProcessMessage(
																				 std::make_shared<CRYPTO::JSONDocument>(msg));
																		 if (!res)
																		 {
																			 poco_error_f2(logger(), "Message processor error: %s [buffer='%s']",
																						   res.ErrorMessage(), msg);
																		 }
																		 
																		 m_logger.Protocol().Incoming(m_buffer);
																	 }
																	 else
																	 {
																		 break;
																	 }
																	 m_lastMessageTime.store(UTILS::CurrentTimestamp());
																	 exceptionCounter = 0;
																 }
																 catch (std::exception &e)
																 {
																	 poco_error_f2(logger(), "Exception in reader thread for session '%s': %s",
																				   m_settings.m_name, std::string(e.what()));
																	 if (++exceptionCounter > MAX_NUMBER_OF_EXCEPTIONS_IN_CONNECTION_THREAD)
																	 {
																		 poco_error_f1(logger(),
																					   "Too many exceptions (%s and counting) in the reader thread. Breaking...",
																					   std::to_string(exceptionCounter));
																		 break;
																	 }
																 }
															 }
															 poco_information_f1(logger(), "Listener thread for session '%s' has stopped",
																				 m_settings.m_name);
															 m_connected = false;
														 });
		
		m_logger.Session().Start(m_settings.m_name);
		poco_information_f1(logger(), "Session started: %s", m_settings.m_name);
	}
	else
	{
		poco_information_f1(logger(), "Session already started: %s", m_settings.m_name);
	}
	
	return true;
}


//------------------------------------------------------------------------------
void ConnectionBase::Disconnect()
{
	m_connected = false;
	if (m_ws)
	{
		m_ws->close();
	}
	
	if (m_listenerThread)
	{
		m_listenerThread->join();
		m_listenerThread.reset();
	}

	m_messageProcessor.Stop();
	m_logger.Session().Stop(m_settings.m_name);
	
	poco_information_f1(logger(), "Session '%s' has disconnected", m_settings.m_name);
}

bool ConnectionBase::Send(const std::string &payload)
{
	if (!m_ws)
	{
		poco_error(logger(), "Failed to send data: connection to web socket has not been created yet");
		return false;
	}

	poco_information_f1(logger(), "Sending data %s", payload);

	m_ws->sendFrame(
		payload.data(),
		static_cast<int>(payload.size()),
		Poco::Net::WebSocket::FRAME_TEXT
	);

	m_logger.Protocol().Outging(payload);
	return true;
}


//------------------------------------------------------------------------------
/*! \brief Returns a set of instruments from configuration */
ConnectionBase::TInstruments ConnectionBase::GetInstruments() const
{
	StringTokenizer tok(m_settings.m_instruments, ",", StringTokenizer::TOK_TRIM | StringTokenizer::TOK_IGNORE_EMPTY);
	TInstruments instruments;
	for (size_t i = 0; i < tok.count(); ++i)
	{
		instruments.insert(TranslateSymbolToExchangeSpecific(tok[i]));
	}
	
	return instruments;
}

//------------------------------------------------------------------------------
/*! \brief creates internal websocket */
void ConnectionBase::CreateWebSocket()
{
	using namespace Poco::Net;
	Context::Ptr ctx = new Context(Context::CLIENT_USE, "", Context::VerificationMode::VERIFY_NONE, 9, true);
	
	m_cs = std::make_unique<Poco::Net::HTTPSClientSession>(m_settings.m_host, m_settings.m_port, ctx);
	HTTPRequest request(HTTPRequest::HTTP_GET, WSPostFixURL, HTTPMessage::HTTP_1_1);
	HTTPResponse response;
	m_ws = std::make_unique<WebSocket>(*m_cs.get(), request, response);
}


//------------------------------------------------------------------------------
int ConnectionBase::ReceiveWebSocketData(Poco::Net::WebSocket *ws, char *buffer, size_t bufferSize, int &outFlags)
{
	int result { ws ? ws->receiveFrame(buffer, bufferSize, outFlags) : 0 };
	
	return result;
}

} // namespace CRYPTO
} // namespace CORE