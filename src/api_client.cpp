#include "api_client.h"

#include "gsheetpp/error.h"
#include "json_util.h"

#include <vector>

namespace gsheetpp::detail {

ApiClient::ApiClient(std::shared_ptr<Credentials> credentials, std::string api_name)
    : credentials_(std::move(credentials)), api_name_(std::move(api_name)) {
    if (!credentials_)
        throw Error(api_name_ + " client requires non-null credentials");
}

rapidjson::Document ApiClient::call(const std::string& method, const std::string& url,
                                    const std::string& body) {
    std::vector<std::string> headers{"Authorization: Bearer " + credentials_->access_token(),
                                     "Accept: application/json"};
    if (method != "GET")
        headers.emplace_back("Content-Type: application/json");

    HttpResponse resp = http_.request(method, url, headers, body);

    rapidjson::Document doc;
    doc.Parse(resp.body.data(), resp.body.size());

    if (resp.status < 200 || resp.status >= 300) {
        std::string msg = api_name_ + " " + method + " returned HTTP " +
                          std::to_string(resp.status);
        if (!doc.HasParseError() && doc.IsObject() && doc.HasMember("error")) {
            std::string detail_msg = json_string(doc["error"], "message");
            if (!detail_msg.empty())
                msg += ": " + detail_msg;
        }
        throw HttpError(msg, resp.status, std::move(resp.body));
    }
    if (doc.HasParseError() || !doc.IsObject())
        throw HttpError(api_name_ + " returned invalid JSON", resp.status, std::move(resp.body));
    return doc;
}

} // namespace gsheetpp::detail
