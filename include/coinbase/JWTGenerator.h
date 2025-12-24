//
// Created by james on 07/12/2025.
//

#ifndef SPOTGRIDBOT_JWTGENERATOR_H
#define SPOTGRIDBOT_JWTGENERATOR_H

#include <string>
#include <jwt-cpp/jwt.h>
#include <openssl/rand.h>

namespace UTILS {

        std::string create_jwt(
            const std::string& api_key,
            const std::string& ec_private_key_pem
        ) {
            using clock = std::chrono::system_clock;

            auto now = clock::now();

            // Coinbase wants SHORT lived tokens
            auto exp = now + std::chrono::seconds{30};

            // Generate random nonce (binary-safe → base64)
            std::array<unsigned char, 16> nonce_raw{};
            RAND_bytes(nonce_raw.data(), nonce_raw.size());

            std::ostringstream oss;
            for (auto b : nonce_raw)
                oss << std::hex << std::setw(2) << std::setfill('0') << (int)b;

            std::string nonce = oss.str();

            auto token = jwt::create()
                .set_issuer("coinbase-cloud")          // ✅ REQUIRED
                .set_subject(api_key)                  // org/.../apiKeys/...
                .set_not_before(now)                   // numeric nbf
                .set_expires_at(exp)                   // numeric exp
                .set_header_claim("kid", jwt::claim(api_key))
                .set_header_claim("nonce", jwt::claim(nonce))
                .sign(jwt::algorithm::es256(
                    "",                                // public key not needed
                    ec_private_key_pem,                // EC private key
                    "", ""
                ));

            return token;
        }

}

#endif //SPOTGRIDBOT_JWTGENERATOR_H