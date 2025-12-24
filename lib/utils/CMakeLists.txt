project(Utils
        LANGUAGES CXX
        DESCRIPTION "Base utilities for Mercury"
)

set(SOURCE_FILES
       ${Utils_SOURCE_DIR}/src/CurrencyPair.cpp
       ${Utils_SOURCE_DIR}/src/ErrorHandler.cpp
       ${Utils_SOURCE_DIR}/src/MessageData.cpp
       ${Utils_SOURCE_DIR}/src/BgwMonitor.cpp
       ${Utils_SOURCE_DIR}/src/Result.cpp
       ${Utils_SOURCE_DIR}/src/Timer.cpp
       ${Utils_SOURCE_DIR}/src/CmdArgParser.cpp
       ${Utils_SOURCE_DIR}/src/CurrentRateManager.cpp
       ${Utils_SOURCE_DIR}/src/FixTypes.cpp
       ${Utils_SOURCE_DIR}/src/Logging.cpp
       ${Utils_SOURCE_DIR}/src/QuoteBuffer.cpp
       ${Utils_SOURCE_DIR}/src/StopWatch.cpp
       ${Utils_SOURCE_DIR}/src/Utils.cpp
       ${Utils_SOURCE_DIR}/src/XmlConfigReader.cpp
)

add_library(Utils SHARED ${SOURCE_FILES})

target_include_directories(Utils PUBLIC "${Utils_SOURCE_DIR}/include/")

target_link_libraries(Utils PUBLIC
        Poco::Foundation
        Poco::Util
        Poco::XML
)
