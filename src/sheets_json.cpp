#include "sheets_json.h"

#include "json_util.h"

#include <algorithm>

namespace gsheetpp::detail {

namespace {

using JsonWriter = rapidjson::Writer<rapidjson::StringBuffer>;

void write_string(JsonWriter& w, const std::string& s) {
    w.String(s.data(), static_cast<rapidjson::SizeType>(s.size()));
}

// Writes {"range": ..., "majorDimension": "ROWS", "values": [[...], ...]}.
void write_value_range(JsonWriter& w, const std::string& range, const Grid& values) {
    w.StartObject();
    w.Key("range");
    write_string(w, range);
    w.Key("majorDimension");
    w.String("ROWS");
    w.Key("values");
    w.StartArray();
    for (const auto& row : values) {
        w.StartArray();
        for (const auto& cell : row)
            write_string(w, cell);
        w.EndArray();
    }
    w.EndArray();
    w.EndObject();
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
                r.push_back(json_scalar_to_string(cell));
        }
        grid.push_back(std::move(r));
    }
    return grid;
}

} // namespace

const char* to_string(ValueInputOption option) {
    switch (option) {
    case ValueInputOption::Raw:
        return "RAW";
    case ValueInputOption::UserEntered:
        return "USER_ENTERED";
    }
    return "USER_ENTERED";
}

const char* to_string(ValueRenderOption option) {
    switch (option) {
    case ValueRenderOption::Formatted:
        return "FORMATTED_VALUE";
    case ValueRenderOption::Unformatted:
        return "UNFORMATTED_VALUE";
    case ValueRenderOption::Formula:
        return "FORMULA";
    }
    return "FORMATTED_VALUE";
}

const char* to_string(DateTimeRenderOption option) {
    switch (option) {
    case DateTimeRenderOption::FormattedString:
        return "FORMATTED_STRING";
    case DateTimeRenderOption::SerialNumber:
        return "SERIAL_NUMBER";
    }
    return "FORMATTED_STRING";
}

std::string serialize_value_range(const std::string& range, const Grid& values) {
    rapidjson::StringBuffer buf;
    JsonWriter w(buf);
    write_value_range(w, range, values);
    return {buf.GetString(), buf.GetSize()};
}

std::string serialize_batch_update_values(const std::vector<ValueRange>& data,
                                          ValueInputOption option) {
    rapidjson::StringBuffer buf;
    JsonWriter w(buf);
    w.StartObject();
    w.Key("valueInputOption");
    w.String(to_string(option));
    w.Key("data");
    w.StartArray();
    for (const auto& vr : data)
        write_value_range(w, vr.range, vr.values);
    w.EndArray();
    w.EndObject();
    return {buf.GetString(), buf.GetSize()};
}

std::string serialize_add_sheet(const std::string& title) {
    rapidjson::StringBuffer buf;
    JsonWriter w(buf);
    w.StartObject();
    w.Key("requests");
    w.StartArray();
    w.StartObject();
    w.Key("addSheet");
    w.StartObject();
    w.Key("properties");
    w.StartObject();
    w.Key("title");
    write_string(w, title);
    w.EndObject();
    w.EndObject();
    w.EndObject();
    w.EndArray();
    w.EndObject();
    return {buf.GetString(), buf.GetSize()};
}

std::string serialize_delete_sheet(int sheet_id) {
    rapidjson::StringBuffer buf;
    JsonWriter w(buf);
    w.StartObject();
    w.Key("requests");
    w.StartArray();
    w.StartObject();
    w.Key("deleteSheet");
    w.StartObject();
    w.Key("sheetId");
    w.Int(sheet_id);
    w.EndObject();
    w.EndObject();
    w.EndArray();
    w.EndObject();
    return {buf.GetString(), buf.GetSize()};
}

std::vector<RowRange> normalize_row_ranges(std::vector<RowRange> rows) {
    rows.erase(std::remove_if(rows.begin(), rows.end(),
                              [](const RowRange& r) { return r.start < 0 || r.end <= r.start; }),
               rows.end());
    std::sort(rows.begin(), rows.end(),
              [](const RowRange& a, const RowRange& b) { return a.start < b.start; });

    std::vector<RowRange> merged;
    for (const auto& r : rows) {
        if (!merged.empty() && r.start <= merged.back().end)
            merged.back().end = std::max(merged.back().end, r.end);
        else
            merged.push_back(r);
    }
    std::reverse(merged.begin(), merged.end());
    return merged;
}

std::string serialize_delete_rows(int sheet_id, const std::vector<RowRange>& rows) {
    auto ranges = normalize_row_ranges(rows);
    if (ranges.empty())
        return {};

    rapidjson::StringBuffer buf;
    JsonWriter w(buf);
    w.StartObject();
    w.Key("requests");
    w.StartArray();
    for (const auto& r : ranges) {
        w.StartObject();
        w.Key("deleteDimension");
        w.StartObject();
        w.Key("range");
        w.StartObject();
        w.Key("sheetId");
        w.Int(sheet_id);
        w.Key("dimension");
        w.String("ROWS");
        w.Key("startIndex");
        w.Int(r.start);
        w.Key("endIndex");
        w.Int(r.end);
        w.EndObject();
        w.EndObject();
        w.EndObject();
    }
    w.EndArray();
    w.EndObject();
    return {buf.GetString(), buf.GetSize()};
}

ValueRange parse_value_range(const rapidjson::Value& obj) {
    return ValueRange{json_string(obj, "range"), parse_grid(obj)};
}

UpdateResult parse_update_result(const rapidjson::Value& obj) {
    UpdateResult r;
    r.updated_range = json_string(obj, "updatedRange");
    r.updated_rows = json_int(obj, "updatedRows");
    r.updated_columns = json_int(obj, "updatedColumns");
    r.updated_cells = json_int(obj, "updatedCells");
    return r;
}

SheetProperties parse_sheet_properties(const rapidjson::Value& obj) {
    SheetProperties sp;
    sp.sheet_id = json_int(obj, "sheetId");
    sp.title = json_string(obj, "title");
    sp.index = json_int(obj, "index");
    if (obj.IsObject() && obj.HasMember("gridProperties")) {
        sp.row_count = json_int(obj["gridProperties"], "rowCount");
        sp.column_count = json_int(obj["gridProperties"], "columnCount");
    }
    return sp;
}

} // namespace gsheetpp::detail
