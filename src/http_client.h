#pragma once

#include <string>
#include <vector>

namespace gsheetpp::detail {

struct HttpResponse {
    long status = 0;
    std::string body;
};

/// Minimal blocking HTTP client over libcurl. One instance per thread.
class HttpClient {
public:
    HttpClient();
    ~HttpClient();
    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    /// Performs a request. `method` is "GET", "POST", "PUT", ...
    /// Throws HttpError on transport failure; non-2xx statuses are returned, not thrown.
    HttpResponse request(const std::string& method, const std::string& url,
                         const std::vector<std::string>& headers = {},
                         const std::string& body = {});

    /// Percent-encodes a string for use in a URL path or query component.
    std::string escape(const std::string& s);

private:
    void* curl_;  // CURL*
};

} // namespace gsheetpp::detail
