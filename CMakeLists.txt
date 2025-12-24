cmake_minimum_required(VERSION 3.20)

project(SpotGridBot
        VERSION 1.0.0
        LANGUAGES CXX C
        DESCRIPTION "SpotGridBot, a client for crypto market data and execution"
        )
set(protobuf_MODULE_COMPATIBLE ON CACHE BOOL "")

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)

    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_C_STANDARD 11)
    set(CMAKE_CXX_STANDARD_REQUIRED True)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
    set(CMAKE_CXX_EXTENSIONS OFF)

    string(REPLACE ":" ";" INCLUDE_LIST "$ENV{INCLUDE_PATH}")
    include_directories(${INCLUDE_LIST})

    string(REPLACE ":" ";" LINK_DIRS "$ENV{LD_LIBRARY_PATH}")
    link_directories( ${LINK_DIRS} )

    find_package(Poco 1.9 REQUIRED Foundation Util Net JSON XML NetSSL Crypto Data)
    find_package(jwt-cpp CONFIG REQUIRED)
    find_package(nlohmann_json CONFIG REQUIRED)

    include_directories(
            ${SpotGridBot_SOURCE_DIR}/lib/utils/include
            ${SpotGridBot_SOURCE_DIR}/lib/orderbook/include
            ${SpotGridBot_SOURCE_DIR}/lib/gridbot/include
    )

    add_subdirectory(lib/utils)
    add_subdirectory(lib/orderbook)
    add_subdirectory(lib/gridbot)
    add_subdirectory(src)

#   find_package(GTest REQUIRED)
#   enable_testing()
#   add_subdirectory(tests)
endif()
