//
// Created by james on 07/12/2025.
//

#ifndef SPOTGRIDBOT_JWTGENERATOR_H
#define SPOTGRIDBOT_JWTGENERATOR_H

#include <string>
#include <jwt-cpp/jwt.h>
#include <openssl/rand.h>

namespace UTILS {
    std::string create_jwt(std::string key_name, std::string key_secret) {

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
}
#endif //SPOTGRIDBOT_JWTGENERATOR_H