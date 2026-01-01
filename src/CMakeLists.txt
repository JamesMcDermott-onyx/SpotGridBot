find_package(OpenSSL REQUIRED)
include_directories(/usr/local/include)
link_directories(/usr/local/lib)
find_package(Poco REQUIRED COMPONENTS Foundation Util Net NetSSL JSON Data Crypto)
cmake_minimum_required(VERSION 3.14)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)

file(
    GLOB SOURCE_LIST CONFIGURE_DEPENDS
    "${SpotGridBot_SOURCE_DIR}/src/*.cpp"
    "${SpotGridBot_SOURCE_DIR}/src/binance/*.cpp"
    "${SpotGridBot_SOURCE_DIR}/src/coinbase/*.cpp"
    "${SpotGridBot_SOURCE_DIR}/src/OKX/*.cpp"
    )

add_executable(SpotGridBot ${SOURCE_LIST})

target_include_directories(SpotGridBot PRIVATE
        "${SpotGridBot_SOURCE_DIR}/include"
        "${SpotGridBot_SOURCE_DIR}/lib/util/include"
        "${SpotGridBot_SOURCE_DIR}/lib/orderbook/include"
        "${SpotGridBot_SOURCE_DIR}/lib/gridbot/include"
        "${SpotGridBot_SOURCE_DIR}/external/json/include"
        "${SpotGridBot_SOURCE_DIR}/external/jwt-cpp/include"
        )

target_link_libraries(SpotGridBot PUBLIC
                        Poco::Foundation
                        Poco::Util
                        Poco::Net
                        Poco::NetSSL
                        Poco::JSON
                        Poco::Data
                        Poco::Crypto
                        OpenSSL::SSL
                        OpenSSL::Crypto
                        jwt-cpp::jwt-cpp
                        nlohmann_json::nlohmann_json
                        protobuf
                        crypto
                        Utils
                        OrderBook
                        GridBot
                        )

install(TARGETS SpotGridBot DESTINATION bin)