#pragma once

#include <string>

namespace gsheetpp {

/// Common OAuth2 scopes for the Sheets API.
namespace scopes {
inline constexpr const char* kSpreadsheets = "https://www.googleapis.com/auth/spreadsheets";
inline constexpr const char* kSpreadsheetsReadOnly =
    "https://www.googleapis.com/auth/spreadsheets.readonly";
} // namespace scopes

/// Source of OAuth2 bearer tokens for Google APIs.
///
/// SheetsClient only depends on this interface, so any authentication scheme
/// (service account, user OAuth2 flow, externally supplied token, ...) can be
/// plugged in by implementing it.
class Credentials {
public:
    virtual ~Credentials() = default;

    /// Returns a valid access token, refreshing it first if needed.
    /// Implementations must be safe to call from multiple threads.
    virtual std::string access_token() = 0;
};

} // namespace gsheetpp
