//
// Created by james on 10/05/23.
//

#pragma once

#include <memory>
#include <algorithm>

#include "Poco/Logger.h"
#include "Poco/FileChannel.h"
#include "Poco/AutoPtr.h"

#include <Poco/Util/PropertyFileConfiguration.h>
#include <Poco/Util/LoggingConfigurator.h>
#include "Poco/FormattingChannel.h"
#include "Poco/PatternFormatter.h"

#include "Utils/Logging.h"
#include "Utils/ErrorHandler.h"
#include "Config.h"

namespace CORE {
namespace CRYPTO {

class Session final
{
public:
	Session(const std::string &name, const CORE::CRYPTO::Settings &settings, const std::string &loggingPropsPath)
			: m_logger(Poco::Logger::get(name + ":" + settings.m_name))
	{
		Poco::AutoPtr<Poco::FileChannel> channel(new Poco::FileChannel);
		Poco::AutoPtr<Poco::PatternFormatter> pf(new Poco::PatternFormatter);
		pf->setProperty("pattern", "%Y-%m-%d %H:%M:%S.%F :%t");
		
		Poco::AutoPtr<Poco::Util::PropertyFileConfiguration> pLoggingConfig { new Poco::Util::PropertyFileConfiguration(loggingPropsPath) };
		pLoggingConfig->load(loggingPropsPath);
		auto path = pLoggingConfig->getString("logging.channels.fileChannel.path");
		path = path.substr(0, path.find_last_of("/") + 1);
		channel->setProperty("path", path + name+"."+settings.m_name + "."+settings.m_protocol+".log");
		
		Poco::AutoPtr<Poco::FormattingChannel> pfc(new Poco::FormattingChannel(pf, channel));
		
		m_logger.setChannel(pfc);
	}
	
	void Start(const std::string &msg) const
	{
		m_logger.information("Started %s", msg);
	}
	
	void Stop(const std::string &msg) const
	{
		m_logger.information("Stopped %s", msg);
	}
	
	void Information(const std::string &msg) const
	{
		m_logger.information("%s", msg);
	}
	
private:
	Poco::Logger &m_logger;
};

class Protocol final
{
public:
	Protocol(const std::string &name, const CORE::CRYPTO::Settings &settings, const std::string &loggingPropsPath)
			: m_logger(Poco::Logger::get(name + ":" + settings.m_name))
	{
		Poco::AutoPtr<Poco::FileChannel> channel(new Poco::FileChannel);
		Poco::AutoPtr<Poco::PatternFormatter> pf(new Poco::PatternFormatter);
		pf->setProperty("pattern", "{\n\t\"timestamp\":%Y-%m-%d %H:%M:%S.%F,\n\t%t\n}");

		Poco::AutoPtr<Poco::Util::PropertyFileConfiguration> pLoggingConfig { new Poco::Util::PropertyFileConfiguration(loggingPropsPath) };
		pLoggingConfig->load(loggingPropsPath);
		auto path = pLoggingConfig->getString("logging.channels.fileChannel.path");
		path = path.substr(0, path.find_last_of("/") + 1);
		
		// Sanitize the API key for use in filename (replace slashes with underscores)
		std::string sanitized_apikey = settings.m_apikey;
		std::replace(sanitized_apikey.begin(), sanitized_apikey.end(), '/', '_');
		
		channel->setProperty("path", path + name + "."+settings.m_protocol+".log." + settings.m_name + "." + sanitized_apikey);

		Poco::AutoPtr<Poco::FormattingChannel> pfc(new Poco::FormattingChannel(pf, channel));

		m_logger.setChannel(pfc);
	}
	
	void Incoming(const std::string &msg) const
	{
		m_logger.information("\"in\":%s", msg);
	}
	
	void Outging(const std::string &msg) const
	{
		m_logger.information("\"out\":%s", msg);
	}
	
private:
	Poco::Logger &m_logger;
};

class Logger final : public UTILS::Logging, UTILS::ErrorHandler
{
public:
	Logger() = delete;
	
	Logger(const CORE::CRYPTO::Settings &settings, const std::string &loggingPropsPath = PATH_LOGGINGPROPS)
			: Logging("CRYPTO::Logger"), m_session("session", settings, loggingPropsPath), m_protocol("protocol", settings, loggingPropsPath)
	{
	}
	
	const CRYPTO::Session &Session() const
	{
		return m_session;
	}
	
	const CRYPTO::Protocol &Protocol() const
	{
		return m_protocol;
	}

private:
	CRYPTO::Session m_session;
	CRYPTO::Protocol m_protocol;
};


}
}