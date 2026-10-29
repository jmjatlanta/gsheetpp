#include "gsheetpp/drive_client.h"

#include "api_client.h"
#include "json_util.h"

namespace gsheetpp {

namespace {

constexpr const char* kDefaultBaseUrl = "https://www.googleapis.com/drive/v3";
constexpr const char* kFileFields = "id,name,mimeType,parents,modifiedTime,webViewLink";

DriveFile parse_file(const rapidjson::Value& obj) {
    DriveFile f;
    f.id = detail::json_string(obj, "id");
    f.name = detail::json_string(obj, "name");
    f.mime_type = detail::json_string(obj, "mimeType");
    f.modified_time = detail::json_string(obj, "modifiedTime");
    f.web_view_link = detail::json_string(obj, "webViewLink");
    auto it = obj.FindMember("parents");
    if (it != obj.MemberEnd() && it->value.IsArray()) {
        for (const auto& p : it->value.GetArray())
            if (p.IsString())
                f.parents.emplace_back(p.GetString(), p.GetStringLength());
    }
    return f;
}

} // namespace

struct DriveClient::Impl {
    explicit Impl(std::shared_ptr<Credentials> credentials)
        : api(std::move(credentials), "Drive API") {}

    detail::ApiClient api;
    std::string base_url = kDefaultBaseUrl;
};

DriveClient::DriveClient(std::shared_ptr<Credentials> credentials)
    : impl_(std::make_unique<Impl>(std::move(credentials))) {}

DriveClient::~DriveClient() = default;
DriveClient::DriveClient(DriveClient&&) noexcept = default;
DriveClient& DriveClient::operator=(DriveClient&&) noexcept = default;

void DriveClient::set_base_url(std::string url) {
    while (!url.empty() && url.back() == '/')
        url.pop_back();
    impl_->base_url = std::move(url);
}

std::string DriveClient::quote(const std::string& value) {
    std::string out = "'";
    for (char c : value) {
        if (c == '\\' || c == '\'')
            out += '\\';
        out += c;
    }
    out += '\'';
    return out;
}

std::vector<DriveFile> DriveClient::search(const std::string& query) {
    auto& api = impl_->api;
    const std::string base = impl_->base_url + "/files?q=" + api.escape(query) +
                             "&fields=" +
                             api.escape(std::string("nextPageToken,files(") + kFileFields + ")") +
                             "&pageSize=1000&supportsAllDrives=true&includeItemsFromAllDrives=true";

    std::vector<DriveFile> files;
    std::string page_token;
    do {
        std::string url = base;
        if (!page_token.empty())
            url += "&pageToken=" + api.escape(page_token);

        auto doc = api.call("GET", url);
        if (doc.HasMember("files") && doc["files"].IsArray()) {
            for (const auto& f : doc["files"].GetArray())
                files.push_back(parse_file(f));
        }
        page_token = detail::json_string(doc, "nextPageToken");
    } while (!page_token.empty());
    return files;
}

std::vector<DriveFile> DriveClient::find_folders(const std::string& name) {
    return search("name = " + quote(name) + " and mimeType = " + quote(mime_types::kFolder) +
                  " and trashed = false");
}

std::vector<DriveFile> DriveClient::find_spreadsheets(const std::string& name,
                                                      const std::string& folder_id) {
    std::string query = "name = " + quote(name) + " and mimeType = " +
                        quote(mime_types::kSpreadsheet) + " and trashed = false";
    if (!folder_id.empty())
        query += " and " + quote(folder_id) + " in parents";
    return search(query);
}

std::vector<DriveFile> DriveClient::list_folder(const std::string& folder_id,
                                                const std::string& mime_type) {
    std::string query = quote(folder_id) + " in parents and trashed = false";
    if (!mime_type.empty())
        query += " and mimeType = " + quote(mime_type);
    return search(query);
}

} // namespace gsheetpp
