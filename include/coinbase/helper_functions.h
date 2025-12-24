//
// Created by james on 14/12/2025.
//

#ifndef WEBSOCKET_HELPERS_H
#define WEBSOCKET_HELPERS_H

#include <jwt-cpp/jwt.h>
#include <nlohmann/json.hpp>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <random>
#include <iomanip>
#include <sstream>

const std::string API_KEY =
    "organizations/a9df3ebf-0eb3-4667-8f8c-493ea6e5f73f/apiKeys/2930fb2a-c257-408f-a4a0-8c228c3435d9";

const std::string SIGNING_KEY = R"(-----BEGIN EC PRIVATE KEY-----
MHcCAQEEIKrF3dQQU+aOrCdBuGtZPWKgqBk74wBKyEayZDF7ehgvoAoGCCqGSM49
AwEHoUQDQgAE8U6PxTdpbfYDciXC/Mi88Sq1MWTxOl7Z1FDgVx8t1exvhrW8YUDW
gBJ9T8h6k/o+TGCFKrjfT4ahe7w+0jsy4Q==
-----END EC PRIVATE KEY-----)";

const std::string WS_API_URL = "wss://advanced-trade-ws.coinbase.com";

std::string random_nonce() {
    unsigned char random_bytes[16];
    std::random_device rd;
    for (auto &b : random_bytes) {
        b = static_cast<unsigned char>(rd());
    }

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(random_bytes, sizeof(random_bytes), hash);

    std::ostringstream oss;
    for (unsigned char c : hash) {
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(c);
    }
    return oss.str();
}


std::string sign_with_jwt(nlohmann::json &message) {
    using clock = std::chrono::system_clock;

    auto now = clock::now();
    auto nbf = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()
    ).count();

    auto exp = nbf + 30;

    auto token = jwt::create()
        .set_issuer("coinbase-cloud")
        .set_subject(API_KEY)
        .set_payload_claim("nbf", jwt::claim(std::to_string(nbf)))
        .set_payload_claim("exp", jwt::claim(std::to_string(exp)))
        .set_header_claim("kid", jwt::claim(API_KEY))
        .set_header_claim("nonce", jwt::claim(random_nonce()))
        .sign(jwt::algorithm::es256("", SIGNING_KEY, "", ""));

    message["jwt"] = token;
    return message.dump();
}



















std::string create_jwt() {
    // Set request parameters
    std::string api_key = "organizations/a9df3ebf-0eb3-4667-8f8c-493ea6e5f73f/apiKeys/2930fb2a-c257-408f-a4a0-8c228c3435d9";
    std::string api_secret = "-----BEGIN EC PRIVATE KEY-----\nMHcCAQEEIKrF3dQQU+aOrCdBuGtZPWKgqBk74wBKyEayZDF7ehgvoAoGCCqGSM49\nAwEHoUQDQgAE8U6PxTdpbfYDciXC/Mi88Sq1MWTxOl7Z1FDgVx8t1exvhrW8YUDW\ngBJ9T8h6k/o+TGCFKrjfT4ahe7w+0jsy4Q==\n-----END EC PRIVATE KEY-----\n";

    // Generate a random nonce
    unsigned char nonce_raw[16];
    RAND_bytes(nonce_raw, sizeof(nonce_raw));
    std::string nonce(reinterpret_cast<char*>(nonce_raw), sizeof(nonce_raw));

    // Create JWT token
    auto token = jwt::create()
        .set_subject(api_key)
        .set_issuer("cdp")
        .set_not_before(std::chrono::system_clock::now())
        .set_expires_at(std::chrono::system_clock::now() + std::chrono::seconds{120})
        .set_header_claim("kid", jwt::claim(api_key))
        .set_header_claim("nonce", jwt::claim(nonce))
        .sign(jwt::algorithm::es256(api_key, api_secret));

    return token;
};


#endif //WEBSOCKET_HELPERS_H