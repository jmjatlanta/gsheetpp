#pragma once

#include "gsheetpp/credentials.h"

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace gsheetpp {

/// Fields of a Google service account key file (the .json downloaded from
/// the Cloud console).
struct ServiceAccountInfo {
    std::string type;
    std::string project_id;
    std::string private_key_id;
    std::string private_key;  // PEM encoded
    std::string client_email;
    std::string client_id;
    std::string token_uri;

    /// Parses a service account key file. Throws AuthError on failure.
    static ServiceAccountInfo from_file(const std::string& path);
    /// Parses the JSON contents of a service account key. Throws AuthError on failure.
    static ServiceAccountInfo from_json(const std::string& json);
};

/// Credentials obtained via the OAuth2 JWT-bearer flow for a service account.
///
/// Tokens are cached and transparently refreshed shortly before they expire.
class ServiceAccountCredentials : public Credentials {
public:
    ServiceAccountCredentials(ServiceAccountInfo info, std::vector<std::string> scopes);

    /// Convenience: load the key file at `path`.
    static std::shared_ptr<ServiceAccountCredentials> from_file(
        const std::string& path,
        std::vector<std::string> scopes = {scopes::kSpreadsheets});

    /// Act on behalf of `user_email` (requires domain-wide delegation).
    void set_subject(std::string user_email);

    std::string access_token() override;

    const ServiceAccountInfo& info() const noexcept { return info_; }

    /// Builds the signed JWT assertion sent to the token endpoint.
    /// Exposed mainly for testing.
    std::string make_assertion(std::chrono::system_clock::time_point now) const;

private:
    void refresh();

    ServiceAccountInfo info_;
    std::vector<std::string> scopes_;
    std::string subject_;

    std::mutex mutex_;
    std::string token_;
    std::chrono::steady_clock::time_point expiry_{};
};

} // namespace gsheetpp
