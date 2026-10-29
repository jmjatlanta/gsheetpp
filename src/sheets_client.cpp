#include "gsheetpp/sheets_client.h"

#include "gsheetpp/error.h"
#include "api_client.h"
#include "json_util.h"

namespace gsheetpp {

namespace {

constexpr const char* kDefaultBaseUrl = "https://sheets.googleapis.com/v4";

const char* to_string(ValueInputOption option) {
    switch (option) {
    case ValueInputOption::Raw:
        return "RAW";
    case ValueInputOption::UserEntered:
        return "USER_ENTERED";
    }
    return "USER_ENTERED";
}

Grid parse_grid(const rapidjson::Value& obj) {
    Grid grid;
    auto it = obj.FindMember("values");
    if (it == obj.MemberEnd() || !it->value.IsArray())
        return grid;  // the API omits "values" for empty ranges
    grid.reserve(it->value.Size());
    for (const auto& row : it->value.GetArray()) {
        Row r;
        if (row.IsArray()) {
            r.reserve(row.Size());
            for (const auto& cell : row.GetArray())
                r.push_back(detail::json_scalar_to_string(cell));
        }
        grid.push_back(std::move(r));
    }
    return grid;
}

ValueRange parse_value_range(const rapidjson::Value& obj) {
    return ValueRange{detail::json_string(obj, "range"), parse_grid(obj)};
}

UpdateResult parse_update_result(const rapidjson::Value& obj) {
    UpdateResult r;
    r.updated_range = detail::json_string(obj, "updatedRange");
    r.updated_rows = detail::json_int(obj, "updatedRows");
    r.updated_columns = detail::json_int(obj, "updatedColumns");
    r.updated_cells = detail::json_int(obj, "updatedCells");
    return r;
}

std::string serialize_value_range(const std::string& range, const Grid& values) {
    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> w(buf);
    w.StartObject();
    w.Key("range");
    w.String(range.data(), static_cast<rapidjson::SizeType>(range.size()));
    w.Key("majorDimension");
    w.String("ROWS");
    w.Key("values");
    w.StartArray();
    for (const auto& row : values) {
        w.StartArray();
        for (const auto& cell : row)
            w.String(cell.data(), static_cast<rapidjson::SizeType>(cell.size()));
        w.EndArray();
    }
    w.EndArray();
    w.EndObject();
    return {buf.GetString(), buf.GetSize()};
}

} // namespace

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
            const auto& p = sheet["properties"];
            SheetProperties sp;
            sp.sheet_id = detail::json_int(p, "sheetId");
            sp.title = detail::json_string(p, "title");
            sp.index = detail::json_int(p, "index");
            if (p.IsObject() && p.HasMember("gridProperties")) {
                sp.row_count = detail::json_int(p["gridProperties"], "rowCount");
                sp.column_count = detail::json_int(p["gridProperties"], "columnCount");
            }
            s.sheets.push_back(std::move(sp));
        }
    }
    return s;
}

ValueRange SheetsClient::get_values(const std::string& spreadsheet_id, const std::string& range) {
    auto doc = impl_->call("GET", impl_->spreadsheet_url(spreadsheet_id) + "/values/" +
                                      impl_->api.escape(range));
    return parse_value_range(doc);
}

std::vector<ValueRange> SheetsClient::batch_get_values(const std::string& spreadsheet_id,
                                                       const std::vector<std::string>& ranges) {
    if (ranges.empty())
        return {};
    std::string url = impl_->spreadsheet_url(spreadsheet_id) + "/values:batchGet";
    char sep = '?';
    for (const auto& r : ranges) {
        url += sep;
        url += "ranges=" + impl_->api.escape(r);
        sep = '&';
    }
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
                           serialize_value_range(range, values));
    return parse_update_result(doc);
}

UpdateResult SheetsClient::append_values(const std::string& spreadsheet_id,
                                         const std::string& range, const Grid& values,
                                         ValueInputOption option) {
    auto doc = impl_->call("POST",
                           impl_->spreadsheet_url(spreadsheet_id) + "/values/" +
                               impl_->api.escape(range) + ":append?valueInputOption=" +
                               to_string(option) + "&insertDataOption=INSERT_ROWS",
                           serialize_value_range(range, values));
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

} // namespace gsheetpp
