#pragma once

#include <string>

namespace gsheetpp::detail {

/// Signs `data` with RSASSA-PKCS1-v1_5 / SHA-256 using a PEM private key and
/// returns the raw signature bytes. Throws AuthError on failure.
std::string rs256_sign(const std::string& data, const std::string& pem_private_key);

/// Builds a compact RS256 JWT from already-serialized header and claims JSON.
std::string make_rs256_jwt(const std::string& header_json, const std::string& claims_json,
                           const std::string& pem_private_key);

} // namespace gsheetpp::detail
