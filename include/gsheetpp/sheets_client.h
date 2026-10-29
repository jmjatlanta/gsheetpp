#pragma once

#include "gsheetpp/credentials.h"

#include <memory>
#include <string>
#include <vector>

namespace gsheetpp {

/// One row of cell values, as returned by the Sheets API (formatted strings).
using Row = std::vector<std::string>;
using Grid = std::vector<Row>;

struct SheetProperties {
    int sheet_id = 0;
    std::string title;
    int index = 0;
    int row_count = 0;
    int column_count = 0;
};

struct Spreadsheet {
    std::string spreadsheet_id;
    std::string title;
    std::string locale;
    std::string time_zone;
    std::string url;
    std::vector<SheetProperties> sheets;
};

struct ValueRange {
    std::string range;  // A1 notation, as resolved by the API
    Grid values;
};

/// How input data is interpreted when writing values.
enum class ValueInputOption {
    Raw,          // stored as-is
    UserEntered,  // parsed as if typed in the UI (formulas, numbers, dates)
};

/// Result of an update/append call.
struct UpdateResult {
    std::string updated_range;
    int updated_rows = 0;
    int updated_columns = 0;
    int updated_cells = 0;
};

/// Client for the Google Sheets v4 REST API.
///
/// A SheetsClient instance is not thread-safe; use one per thread. The
/// Credentials object may be shared between clients.
class SheetsClient {
public:
    explicit SheetsClient(std::shared_ptr<Credentials> credentials);
    ~SheetsClient();

    SheetsClient(const SheetsClient&) = delete;
    SheetsClient& operator=(const SheetsClient&) = delete;
    SheetsClient(SheetsClient&&) noexcept;
    SheetsClient& operator=(SheetsClient&&) noexcept;

    /// Spreadsheet metadata, including the list of sheets (tabs).
    Spreadsheet get_spreadsheet(const std::string& spreadsheet_id);

    /// Reads a range in A1 notation, e.g. "Sheet1!A1:D10".
    ValueRange get_values(const std::string& spreadsheet_id, const std::string& range);

    /// Reads several ranges in one request.
    std::vector<ValueRange> batch_get_values(const std::string& spreadsheet_id,
                                             const std::vector<std::string>& ranges);

    /// Overwrites the cells in `range` with `values`.
    UpdateResult update_values(const std::string& spreadsheet_id, const std::string& range,
                               const Grid& values,
                               ValueInputOption option = ValueInputOption::UserEntered);

    /// Appends rows after the last row of the table found in `range`.
    UpdateResult append_values(const std::string& spreadsheet_id, const std::string& range,
                               const Grid& values,
                               ValueInputOption option = ValueInputOption::UserEntered);

    /// Clears the values (not formatting) in `range`.
    void clear_values(const std::string& spreadsheet_id, const std::string& range);

    /// Override the API root (default "https://sheets.googleapis.com/v4"), e.g. for testing.
    void set_base_url(std::string url);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gsheetpp
