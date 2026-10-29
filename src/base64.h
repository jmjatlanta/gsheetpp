#pragma once

#include <string>

namespace gsheetpp::detail {

/// URL-safe base64 without padding (RFC 4648 §5), as used by JWTs.
std::string base64url_encode(const std::string& data);

} // namespace gsheetpp::detail
