#include "gsheetpp/sheets_client.h"

#include "gsheetpp/error.h"
#include "api_client.h"
#include "json_util.h"
#include "sheets_json.h"

namespace gsheetpp {

namespace {

constexpr const char* kDefaultBaseUrl = "https://sheets.googleapis.com/v4";

using detail::parse_update_result;
using detail::parse_value_range;
using detail::to_string;

std::string render_query(ValueRenderOption render, DateTimeRenderOption date_time_render) {
    return std::string("valueRenderOption=") + to_string(render) +
           "&dateTimeRenderOption=" + to_string(date_time_render);
}

} // namespace

const SheetProperties* Spreadsheet::find_sheet(const std::string& title) const {
    for (const auto& sheet : sheets)
        if (sheet.title == title)
            return &sheet;
    return nullptr;
}

struct SheetsClient::Impl {
    explicit Impl(std::shared_ptr<Credentials> credentials)
        : api(std::move(credentials), "Sheets API") {}

    detail::ApiClient api;
    std::string base_url = kDefaultBaseUrl;

    std::string spreadsheet_url(const std::string& id) {
        return base_url + "/spreadsheets/" + api.escape(id);
    }

    rapidjson::Document call(const std::string& method, const std::string& url,
                             const std::string& body = {}) {
        return api.call(method, url, body);
    }

    // spreadsheets:batchUpdate, for structural changes such as adding sheets.
    rapidjson::Document batch_update(const std::string& spreadsheet_id, const std::string& body) {
        return call("POST", spreadsheet_url(spreadsheet_id) + ":batchUpdate", body);
    }
};

SheetsClient::SheetsClient(std::shared_ptr<Credentials> credentials)
    : impl_(std::make_unique<Impl>(std::move(credentials))) {}

SheetsClient::~SheetsClient() = default;
SheetsClient::SheetsClient(SheetsClient&&) noexcept = default;
SheetsClient& SheetsClient::operator=(SheetsClient&&) noexcept = default;

void SheetsClient::set_base_url(std::string url) {
    while (!url.empty() && url.back() == '/')
        url.pop_back();
    impl_->base_url = std::move(url);
}

Spreadsheet SheetsClient::get_spreadsheet(const std::string& spreadsheet_id) {
    const std::string fields =
        "spreadsheetId,spreadsheetUrl,properties(title,locale,timeZone),"
        "sheets.properties(sheetId,title,index,gridProperties)";
    auto doc = impl_->call("GET", impl_->spreadsheet_url(spreadsheet_id) +
                                      "?fields=" + impl_->api.escape(fields));

    Spreadsheet s;
    s.spreadsheet_id = detail::json_string(doc, "spreadsheetId");
    s.url = detail::json_string(doc, "spreadsheetUrl");
    if (doc.HasMember("properties")) {
        const auto& props = doc["properties"];
        s.title = detail::json_string(props, "title");
        s.locale = detail::json_string(props, "locale");
        s.time_zone = detail::json_string(props, "timeZone");
    }
    if (doc.HasMember("sheets") && doc["sheets"].IsArray()) {
        for (const auto& sheet : doc["sheets"].GetArray()) {
            if (!sheet.IsObject() || !sheet.HasMember("properties"))
                continue;
            s.sheets.push_back(detail::parse_sheet_properties(sheet["properties"]));
        }
    }
    return s;
}

ValueRange SheetsClient::get_values(const std::string& spreadsheet_id, const std::string& range,
                                    ValueRenderOption render,
                                    DateTimeRenderOption date_time_render) {
    auto doc = impl_->call("GET", impl_->spreadsheet_url(spreadsheet_id) + "/values/" +
                                      impl_->api.escape(range) + "?" +
                                      render_query(render, date_time_render));
    return parse_value_range(doc);
}

std::vector<ValueRange> SheetsClient::batch_get_values(const std::string& spreadsheet_id,
                                                       const std::vector<std::string>& ranges,
                                                       ValueRenderOption render,
                                                       DateTimeRenderOption date_time_render) {
    if (ranges.empty())
        return {};
    std::string url = impl_->spreadsheet_url(spreadsheet_id) + "/values:batchGet?" +
                      render_query(render, date_time_render);
    for (const auto& r : ranges)
        url += "&ranges=" + impl_->api.escape(r);
    auto doc = impl_->call("GET", url);

    std::vector<ValueRange> result;
    if (doc.HasMember("valueRanges") && doc["valueRanges"].IsArray()) {
        for (const auto& vr : doc["valueRanges"].GetArray())
            result.push_back(parse_value_range(vr));
    }
    return result;
}

UpdateResult SheetsClient::update_values(const std::string& spreadsheet_id,
                                         const std::string& range, const Grid& values,
                                         ValueInputOption option) {
    auto doc = impl_->call("PUT",
                           impl_->spreadsheet_url(spreadsheet_id) + "/values/" +
                               impl_->api.escape(range) +
                               "?valueInputOption=" + to_string(option),
                           detail::serialize_value_range(range, values));
    return parse_update_result(doc);
}

std::vector<UpdateResult> SheetsClient::batch_update_values(const std::string& spreadsheet_id,
                                                          const std::vector<ValueRange>& data,
                                                          ValueInputOption option) {
    if (data.empty())
        return {};
    auto doc = impl_->call("POST", impl_->spreadsheet_url(spreadsheet_id) + "/values:batchUpdate",
                           detail::serialize_batch_update_values(data, option));

    std::vector<UpdateResult> result;
    if (doc.HasMember("responses") && doc["responses"].IsArray()) {
        for (const auto& r : doc["responses"].GetArray())
            result.push_back(parse_update_result(r));
    }
    return result;
}

UpdateResult SheetsClient::append_values(const std::string& spreadsheet_id,
                                         const std::string& range, const Grid& values,
                                         ValueInputOption option) {
    auto doc = impl_->call("POST",
                           impl_->spreadsheet_url(spreadsheet_id) + "/values/" +
                               impl_->api.escape(range) + ":append?valueInputOption=" +
                               to_string(option) + "&insertDataOption=INSERT_ROWS",
                           detail::serialize_value_range(range, values));
    if (doc.HasMember("updates"))
        return parse_update_result(doc["updates"]);
    return {};
}

void SheetsClient::clear_values(const std::string& spreadsheet_id, const std::string& range) {
    impl_->call("POST",
                impl_->spreadsheet_url(spreadsheet_id) + "/values/" + impl_->api.escape(range) +
                    ":clear",
                "{}");
}

SheetProperties SheetsClient::add_sheet(const std::string& spreadsheet_id,
                                        const std::string& title) {
    auto doc = impl_->batch_update(spreadsheet_id, detail::serialize_add_sheet(title));

    // {"replies": [{"addSheet": {"properties": {...}}}]}
    if (doc.HasMember("replies") && doc["replies"].IsArray() && !doc["replies"].Empty()) {
        const auto& reply = doc["replies"][0];
        if (reply.IsObject() && reply.HasMember("addSheet") && reply["addSheet"].IsObject() &&
            reply["addSheet"].HasMember("properties"))
            return detail::parse_sheet_properties(reply["addSheet"]["properties"]);
    }
    throw Error("Sheets API addSheet reply did not include the new sheet's properties");
}

void SheetsClient::delete_sheet(const std::string& spreadsheet_id, int sheet_id) {
    impl_->batch_update(spreadsheet_id, detail::serialize_delete_sheet(sheet_id));
}

void SheetsClient::delete_rows(const std::string& spreadsheet_id, int sheet_id, RowRange rows) {
    delete_rows(spreadsheet_id, sheet_id, std::vector<RowRange>{rows});
}

void SheetsClient::delete_rows(const std::string& spreadsheet_id, int sheet_id,
                               const std::vector<RowRange>& rows) {
    std::string body = detail::serialize_delete_rows(sheet_id, rows);
    if (!body.empty())
        impl_->batch_update(spreadsheet_id, body);
}

} // namespace gsheetpp
