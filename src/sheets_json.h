#pragma once

// Request bodies and query values for the Sheets API. Kept apart from
// sheets_client.cpp so they can be unit tested without the network.

#include "gsheetpp/sheets_client.h"

#include <rapidjson/document.h>

#include <string>
#include <vector>

namespace gsheetpp::detail {

const char* to_string(ValueInputOption option);
const char* to_string(ValueRenderOption option);
const char* to_string(DateTimeRenderOption option);

/// Body of a values update/append request.
std::string serialize_value_range(const std::string& range, const Grid& values);

/// Body of a values:batchUpdate request.
std::string serialize_batch_update_values(const std::vector<ValueRange>& data,
                                          ValueInputOption option);

/// Body of a spreadsheets:batchUpdate request that adds a sheet named `title`.
std::string serialize_add_sheet(const std::string& title);

/// Body of a spreadsheets:batchUpdate request that deletes sheet `sheet_id`.
std::string serialize_delete_sheet(int sheet_id);

/// Body of a spreadsheets:batchUpdate request that deletes `rows` from sheet
/// `sheet_id`. Returns an empty string if there is nothing to delete.
std::string serialize_delete_rows(int sheet_id, const std::vector<RowRange>& rows);

/// Sorts `rows`, drops empty ranges and merges overlapping or adjacent ones,
/// then orders them bottom-up so that deleting one doesn't shift the others.
std::vector<RowRange> normalize_row_ranges(std::vector<RowRange> rows);

ValueRange parse_value_range(const rapidjson::Value& obj);
UpdateResult parse_update_result(const rapidjson::Value& obj);
SheetProperties parse_sheet_properties(const rapidjson::Value& obj);

} // namespace gsheetpp::detail
