#pragma once

#include "gsheetpp/credentials.h"
#include "http_client.h"

#include <rapidjson/document.h>

#include <memory>
#include <string>

namespace gsheetpp::detail {

/// Sends authorized JSON requests to a Google REST API. Shared by the
/// SheetsClient and DriveClient implementations.
class ApiClient {
public:
    /// `api_name` is used only in error messages, e.g. "Sheets API".
    ApiClient(std::shared_ptr<Credentials> credentials, std::string api_name);

    /// Sends an authorized request and returns the parsed JSON body.
    /// Throws HttpError for transport failures and non-2xx responses.
    rapidjson::Document call(const std::string& method, const std::string& url,
                             const std::string& body = {});

    std::string escape(const std::string& s) { return http_.escape(s); }

private:
    std::shared_ptr<Credentials> credentials_;
    std::string api_name_;
    HttpClient http_;
};

} // namespace gsheetpp::detail
