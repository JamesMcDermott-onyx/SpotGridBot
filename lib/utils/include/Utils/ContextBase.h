/**
 * @file
 * @author      james <james.doll@bbf-it.at>
 * @date        13.02.18
 * @version     10.05.19
 * 				Fix8GlobalLogPath()
 * @copyright   (c) 2019 BBF-IT
 */


#pragma once

#include <string>
#include <atomic>

#include "Timer.h"
#include "Lockable.h"
#include "CurrentRateManager.h"

namespace UTILS {

class StopWatch;

enum ShutdownType
{
	sdtNone, sdtTransactionReload, sdtSessionShutdown, sdtProgramShutdown
};

/*! \brief Abstract base class of the concrete class to store the global context.
 */
class ContextBase
{
public:
	static constexpr uint64_t DISABLE_MEMORY_POOLS = 1 << 18;
	static constexpr uint64_t LOG_TIMINGS = 1 << 19;

    virtual ~ContextBase() = default;

    UTILS::Timer &Timer() const { return m_timer; }
	
	UTILS::Timer &MonitoringTimer() const { return m_monitoringTimer; }
	
	/*! \brief Reference to the CurrentRateManager object */
    UTILS::CurrentRateManager &CurrentRateManager() { return m_currentRateManager; }
	
	/*! \brief Const reference to the CurrentRateManager object */
	const UTILS::CurrentRateManager &CurrentRateManager() const { return m_currentRateManager; }
	
	/*! \brief Path to the configuration file.
	 *
	 * Pure virtual function.
	 */
	virtual const std::string &ConfigPath() const = 0;
	
	/*! \brief Path to the FIX8 configuration file.
	 *
	 * Pure virtual function.
	 */
	virtual const std::string &Fix8CfgPath() const = 0;
	
	/*! \brief Path to the logging properties file.
	 *
	 * Pure virtual function.
	 */
	virtual const std::string &LoggingPropsPath() const = 0;
	
	/*! \brief Path to the fix8 global log file
	 *
	 * Pure virtual function.
	 */
	virtual const std::string &Fix8GlobalLogPath() const = 0;
	
	/*! \brief Store StopWatch data in the Mongo DB.
	 *
	 * Virtual function. Stores StopWatch data in the Mongo DB. Overriden
	 * in DataContext.
	 */
	virtual bool StoreStopWatch(StopWatch &stopWatch, const std::string &collectionSuffix) { return false; };
	
	int64_t LoggingFlags() const { return m_loggingFlags.load(); }
    
    void SetLoggingFlag(int64_t flag, bool on)
    {
        if (on)
        {
            m_loggingFlags = m_loggingFlags.load() | flag;
        }
        else
        {
            m_loggingFlags = m_loggingFlags.load() & ~flag;
        }
    }
	
	bool IsLoggingFlagSet(int64_t flag) const { return (m_loggingFlags.load() & flag) != 0; }

private:
	
	/*! \brief Timer object */
	mutable UTILS::Timer m_timer { *this };
    
    /*! \brief Timer object for monitoring tasks */
    mutable UTILS::Timer m_monitoringTimer { *this };
    
	/*! \brief CurrentRateManager object */
	UTILS::CurrentRateManager m_currentRateManager;
	
	std::atomic_int64_t m_loggingFlags { 0 };
};

} // namespace CORE

