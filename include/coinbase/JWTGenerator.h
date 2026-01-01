//
// Created by james on 07/12/2025.
//

#ifndef SPOTGRIDBOT_JWTGENERATOR_H
#define SPOTGRIDBOT_JWTGENERATOR_H

#include <string>
#include <jwt-cpp/jwt.h>
#include <openssl/rand.h>
#include <Poco/String.h>
#include <Poco/UUIDGenerator.h>

namespace UTILS {

        // Helper: Convert XML escape sequences to actual newlines
        inline std::string process_pem_key(const std::string& raw_key) {
            std::string result = raw_key;
            // Replace literal \n strings with actual newlines
            size_t pos = 0;
            while ((pos = result.find("\\n", pos)) != std::string::npos) {
                result.replace(pos, 2, "\n");
                pos += 1;
            }
            return Poco::trim(result);
        }

        inline std::string create_jwt(
            const std::string& api_key,
            const std::string& ec_private_key_pem,
            const std::string& request_method = "",
            const std::string& request_host_path = ""
        ) {
            using clock = std::chrono::system_clock;

            auto now = clock::now();

            // Coinbase wants SHORT lived tokens
            auto exp = now + std::chrono::seconds{120};

            // Generate random nonce (UUID string)
            std::string nonce = Poco::UUIDGenerator::defaultGenerator().createRandom().toString();

            // Process PEM key to handle XML escape sequences
            std::string processed_key = process_pem_key(ec_private_key_pem);

            auto builder = jwt::create()
                .set_issuer("coinbase")          // ✅ REQUIRED
                .set_subject(api_key)                  // org/.../apiKeys/...
                .set_not_before(now)                   // numeric nbf
                .set_expires_at(exp)                   // numeric exp
                .set_header_claim("kid", jwt::claim(api_key))
                .set_header_claim("nonce", jwt::claim(nonce));

            // For REST API, add URI claim
            // Format can be either "METHOD host/path" or just passed as full URI string
            if (!request_host_path.empty())
            {
                std::string uri;
                if (!request_method.empty())
                {
                    uri = request_method + " " + request_host_path;
                }
                else
                {
                    uri = request_host_path;  // Already formatted
                }
                builder.set_payload_claim("uri", jwt::claim(uri));
            }

            auto token = builder.sign(jwt::algorithm::es256(
                    "",                                // public key not needed
                    processed_key,                     // EC private key (with actual newlines)
                    "", ""
                ));

            return token;
        }

}

#endif //SPOTGRIDBOT_JWTGENERATOR_H