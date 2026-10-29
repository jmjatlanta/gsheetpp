#pragma once

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <string>

namespace gsheetpp::detail {

inline std::string json_string(const rapidjson::Value& obj, const char* key,
                               const std::string& fallback = {}) {
    if (!obj.IsObject())
        return fallback;
    auto it = obj.FindMember(key);
    if (it == obj.MemberEnd() || !it->value.IsString())
        return fallback;
    return {it->value.GetString(), it->value.GetStringLength()};
}

inline int json_int(const rapidjson::Value& obj, const char* key, int fallback = 0) {
    if (!obj.IsObject())
        return fallback;
    auto it = obj.FindMember(key);
    if (it == obj.MemberEnd() || !it->value.IsInt())
        return fallback;
    return it->value.GetInt();
}

/// Converts a scalar JSON cell value to its string form.
inline std::string json_scalar_to_string(const rapidjson::Value& v) {
    if (v.IsString())
        return {v.GetString(), v.GetStringLength()};
    if (v.IsNull())
        return {};
    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
    v.Accept(writer);
    return {buf.GetString(), buf.GetSize()};
}

} // namespace gsheetpp::detail
