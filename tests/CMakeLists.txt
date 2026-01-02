# generate unittest.h
include(GenerateTarget.cmake)
configure_file(include/unittest.h.in ${SpotGridBot_BINARY_DIR}/unittest.h)

# use simple macro for create test target with given files
macro(package_add_test TESTNAME)

    # Adding something we can run (${ARGN}) - Output name matches ${TESTNAME} name
    add_executable(${TESTNAME} ${ARGN})

    # We need this directory, no other can use this target so use PRIVATE
    target_include_directories(${TESTNAME} PRIVATE
            "${SpotGridBot_SOURCE_DIR}/include"
            "${SpotGridBot_BINARY_DIR}"
            "${SpotGridBot_SOURCE_DIR}/lib/utils/include"
            )

    # We need this libs
    target_link_libraries(${TESTNAME} PRIVATE
            gtest
            gmock
            gtest_main
            crypto
            Poco::Foundation
            Poco::Util
            Poco::Net
            Poco::NetSSL
            Poco::JSON
            Poco::Data
            Poco::Data
            Poco::XML
            )

    set_target_properties(${TESTNAME} PROPERTIES FOLDER tests)

endmacro()


file(GLOB SOURCES_ADDITIONAL
    ${SpotGridBot_SOURCE_DIR}/src/Tools.cpp
    ${SpotGridBot_SOURCE_DIR}/src/ProgramOptions.cpp
    ${SpotGridBot_SOURCE_DIR}/src/ConnectionBase.cpp
    ${SpotGridBot_SOURCE_DIR}/src/RestConnectionBase.cpp
    ${SpotGridBot_SOURCE_DIR}/src/AdapterExecutor.cpp
    ${SpotGridBot_SOURCE_DIR}/src/SessionConfig.cpp
    ${SpotGridBot_SOURCE_DIR}/src/Crypto.cpp
    ${SpotGridBot_SOURCE_DIR}/src/binance/ConnectionMD.cpp
    ${SpotGridBot_SOURCE_DIR}/src/binance/ConnectionSS.cpp
    ${SpotGridBot_SOURCE_DIR}/src/coinbase/ConnectionMD.cpp
    ${SpotGridBot_SOURCE_DIR}/src/binance/ConnectionTT.cpp
    ${SpotGridBot_SOURCE_DIR}/src/MessageProcessor.cpp
    ${SpotGridBot_SOURCE_DIR}/src/RestBase.cpp
    ${SpotGridBot_SOURCE_DIR}/src/ActiveQuoteTable.cpp
    )

file(GLOB LIB_SOURCES ${SpotGridBot_SOURCE_DIR}/lib/utils/src/*.cpp)
file(GLOB SOURCES src/*.cpp)

package_add_test(AllTests ${SOURCES} ${SOURCES_ADDITIONAL} ${LIB_SOURCES})
