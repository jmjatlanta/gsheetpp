#include "gsheetpp/service_account_credentials.h"

#include "gsheetpp/error.h"
#include "http_client.h"
#include "json_util.h"
#include "jwt.h"

#include <fstream>
#include <sstream>

namespace gsheetpp {

namespace {

constexpr const char* kDefaultTokenUri = "https://oauth2.googleapis.com/token";
constexpr const char* kJwtBearerGrant = "urn:ietf:params:oauth:grant-type:jwt-bearer";
constexpr int kTokenLifetimeSeconds = 3600;  // max allowed by Google
constexpr auto kRefreshMargin = std::chrono::seconds(60);

} // namespace

ServiceAccountInfo ServiceAccountInfo::from_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw AuthError("Unable to open service account file: " + path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return from_json(ss.str());
}

ServiceAccountInfo ServiceAccountInfo::from_json(const std::string& json) {
    rapidjson::Document doc;
    doc.Parse(json.data(), json.size());
    if (doc.HasParseError() || !doc.IsObject())
        throw AuthError("Service account file is not a valid JSON object");

    using detail::json_string;
    ServiceAccountInfo info;
    info.type = json_string(doc, "type");
    info.project_id = json_string(doc, "project_id");
    info.private_key_id = json_string(doc, "private_key_id");
    info.private_key = json_string(doc, "private_key");
    info.client_email = json_string(doc, "client_email");
    info.client_id = json_string(doc, "client_id");
    info.token_uri = json_string(doc, "token_uri", kDefaultTokenUri);

    if (info.type != "service_account")
        throw AuthError("Credential file type is \"" + info.type +
                        "\", expected \"service_account\"");
    if (info.private_key.empty())
        throw AuthError("Service account file is missing \"private_key\"");
    if (info.client_email.empty())
        throw AuthError("Service account file is missing \"client_email\"");
    return info;
}

ServiceAccountCredentials::ServiceAccountCredentials(ServiceAccountInfo info,
                                                     std::vector<std::string> scopes)
    : info_(std::move(info)), scopes_(std::move(scopes)) {
    if (scopes_.empty())
        throw AuthError("At least one OAuth2 scope is required");
}

std::shared_ptr<ServiceAccountCredentials> ServiceAccountCredentials::from_file(
    const std::string& path, std::vector<std::string> scopes) {
    return std::make_shared<ServiceAccountCredentials>(ServiceAccountInfo::from_file(path),
                                                       std::move(scopes));
}

void ServiceAccountCredentials::set_subject(std::string user_email) {
    std::lock_guard<std::mutex> lock(mutex_);
    subject_ = std::move(user_email);
    token_.clear();  // a token for another principal is no longer valid
}

std::string ServiceAccountCredentials::make_assertion(
    std::chrono::system_clock::time_point now) const {
    using rapidjson::StringBuffer;
    using rapidjson::Writer;

    StringBuffer header;
    {
        Writer<StringBuffer> w(header);
        w.StartObject();
        w.Key("alg");
        w.String("RS256");
        w.Key("typ");
        w.String("JWT");
        if (!info_.private_key_id.empty()) {
            w.Key("kid");
            w.String(info_.private_key_id.c_str());
        }
        w.EndObject();
    }

    std::string scope;
    for (const auto& s : scopes_) {
        if (!scope.empty())
            scope += ' ';
        scope += s;
    }

    const int64_t iat =
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

    StringBuffer claims;
    {
        Writer<StringBuffer> w(claims);
        w.StartObject();
        w.Key("iss");
        w.String(info_.client_email.c_str());
        w.Key("scope");
        w.String(scope.c_str());
        w.Key("aud");
        w.String(info_.token_uri.c_str());
        w.Key("iat");
        w.Int64(iat);
        w.Key("exp");
        w.Int64(iat + kTokenLifetimeSeconds);
        if (!subject_.empty()) {
            w.Key("sub");
            w.String(subject_.c_str());
        }
        w.EndObject();
    }

    return detail::make_rs256_jwt(header.GetString(), claims.GetString(), info_.private_key);
}

std::string ServiceAccountCredentials::access_token() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (token_.empty() || std::chrono::steady_clock::now() + kRefreshMargin >= expiry_)
        refresh();
    return token_;
}

// Caller must hold mutex_.
void ServiceAccountCredentials::refresh() {
    const auto requested_at = std::chrono::steady_clock::now();
    const std::string assertion = make_assertion(std::chrono::system_clock::now());

    detail::HttpClient http;
    const std::string body =
        "grant_type=" + http.escape(kJwtBearerGrant) + "&assertion=" + http.escape(assertion);

    detail::HttpResponse resp;
    try {
        resp = http.request("POST", info_.token_uri,
                            {"Content-Type: application/x-www-form-urlencoded"}, body);
    } catch (const HttpError& e) {
        throw AuthError(std::string("Token request failed: ") + e.what());
    }

    rapidjson::Document doc;
    doc.Parse(resp.body.data(), resp.body.size());

    if (resp.status < 200 || resp.status >= 300) {
        std::string msg = "Token endpoint returned HTTP " + std::to_string(resp.status);
        if (!doc.HasParseError()) {
            std::string err = detail::json_string(doc, "error");
            std::string desc = detail::json_string(doc, "error_description");
            if (!err.empty())
                msg += ": " + err;
            if (!desc.empty())
                msg += " (" + desc + ")";
        }
        throw AuthError(msg);
    }

    if (doc.HasParseError() || !doc.IsObject())
        throw AuthError("Token endpoint returned invalid JSON");

    std::string token = detail::json_string(doc, "access_token");
    if (token.empty())
        throw AuthError("Token endpoint response has no access_token");

    token_ = std::move(token);
    expiry_ = requested_at +
              std::chrono::seconds(detail::json_int(doc, "expires_in", kTokenLifetimeSeconds));
}

} // namespace gsheetpp
