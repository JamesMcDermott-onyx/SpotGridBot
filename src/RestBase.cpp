//
// Created by james on 24/05/23.
//

#include <Poco/URI.h>
#include <iostream>
#include <Utils/Utils.h>
#include <Poco/Logger.h>
#include "RestBase.h"
#include "JSONDocument.h"

namespace CORE {
namespace RESTAPI {

//------------------------------------------------------------------------------
std::string RestBase::ExecuteWebRequest(const std::string &url, const std::string &requestType,
												  std::function<void(std::string &path)> customizeRequestPathFunc,
												  std::function<void(Poco::Net::HTTPRequest &request)> customizeRequestFunc,
												  std::function<void(const Poco::Net::HTTPResponse &response)> processResponseFunc,
												  std::function<void(std::ostream &)> handleRequestStreamFunc) const
{
	try
	{
		Poco::URI uri(url);
		std::string path(uri.getPathAndQuery());
		if (path.empty())
		{
			path = "/";
		}
		
		if (customizeRequestPathFunc)
		{
			customizeRequestPathFunc(path);
		}

		Poco::Net::Context::Ptr	ctx = new Poco::Net::Context(Poco::Net::Context::CLIENT_USE, "", Poco::Net::Context::VerificationMode::VERIFY_NONE, 9, true);
		
		Poco::Net::HTTPSClientSession session(uri.getHost(), uri.getPort(), ctx);
		Poco::Net::HTTPRequest request(requestType, path, Poco::Net::HTTPMessage::HTTP_1_1);
		if (customizeRequestFunc)
		{
			customizeRequestFunc(request);
		}
		
		return SendWebRequest(url, session, request, processResponseFunc, handleRequestStreamFunc);
	}
	catch (...)
	{
		poco_fatal(logger(), UTILS::GetMessage(std::current_exception()));
		return CRYPTO::CreateJSONMessageWithCode(UTILS::GetMessage(std::current_exception()));
	}
}


//------------------------------------------------------------------------------
std::string RestBase::SendWebRequest(const std::string &url, Poco::Net::HTTPSClientSession &session, Poco::Net::HTTPRequest &request,
											   std::function<void(const Poco::Net::HTTPResponse &response)> processResponseFunc,
											   std::function<void(std::ostream &)> handleRequestStreamFunc) const
{
	poco_information_f1(logger(), "Sending request: %s", url);
	Poco::Net::HTTPResponse response;
	auto &ostr = session.sendRequest(request);
	if (handleRequestStreamFunc)
	{
		handleRequestStreamFunc(ostr);
	}
	
	auto &rs = session.receiveResponse(response);
	// Call response function if specified
	if (processResponseFunc)
	{
		processResponseFunc(response);
	}
	
	// Process response
	std::string msg;
	for (std::string line; std::getline(rs, line);)
	{
		//Commented out for now because it produces enormous noise in trace
		//poco_information_f1(logger(), "RECEIVED LINE: '%s'", line);
		msg += line;
	}
	
	poco_information_f1(logger(), "Received WEB RESPONSE %s bytes", std::to_string(msg.size()));
	
	if (response.getStatus() == Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED)
	{
		throw std::runtime_error(UTILS::Format("Request '%s' has not expected status %s, reason: HTTP_UNAUTHORIZED, response: '%s'", request.getURI(), response.getReason(), msg));
	}
	
	return msg;
}

}
}