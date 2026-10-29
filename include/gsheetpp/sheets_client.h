#pragma once

#include "gsheetpp/credentials.h"

#include <memory>
#include <string>
#include <vector>

namespace gsheetpp {

/// One row of cell values, as returned by the Sheets API (see ValueRenderOption).
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

    /// The sheet (tab) named `title`, or nullptr if there is none.
    const SheetProperties* find_sheet(const std::string& title) const;
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

/// How cell values are returned when reading.
enum class ValueRenderOption {
    Formatted,    // as displayed in the UI, e.g. "$1,234.50"
    Unformatted,  // the underlying value, e.g. "1234.5"
    Formula,      // formulas rather than their results, e.g. "=A1*2"
};

/// How dates and times are returned when reading with ValueRenderOption::Unformatted
/// or ValueRenderOption::Formula. Ignored for ValueRenderOption::Formatted.
enum class DateTimeRenderOption {
    FormattedString,  // as displayed in the UI, e.g. "2026-10-29 16:05:12"
    SerialNumber,     // days since 1899-12-30, e.g. "46324.67"
};

/// Result of an update/append call.
struct UpdateResult {
    std::string updated_range;
    int updated_rows = 0;
    int updated_columns = 0;
    int updated_cells = 0;
};

/// A block of rows, numbered from 0 (row 1 in the UI is row 0 here).
/// `end` is exclusive, so {1, 3} is UI rows 2 and 3.
struct RowRange {
    int start = 0;
    int end = 0;
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
    ValueRange get_values(const std::string& spreadsheet_id, const std::string& range,
                          ValueRenderOption render = ValueRenderOption::Formatted,
                          DateTimeRenderOption date_time_render =
                              DateTimeRenderOption::FormattedString);

    /// Reads several ranges in one request.
    std::vector<ValueRange> batch_get_values(const std::string& spreadsheet_id,
                                             const std::vector<std::string>& ranges,
                                             ValueRenderOption render = ValueRenderOption::Formatted,
                                             DateTimeRenderOption date_time_render =
                                                 DateTimeRenderOption::FormattedString);

    /// Overwrites the cells in `range` with `values`.
    UpdateResult update_values(const std::string& spreadsheet_id, const std::string& range,
                               const Grid& values,
                               ValueInputOption option = ValueInputOption::UserEntered);

    /// Overwrites several ranges in one request. Each ValueRange's `range` says
    /// where its `values` go. Returns one result per range, in the same order.
    std::vector<UpdateResult> batch_update_values(
        const std::string& spreadsheet_id, const std::vector<ValueRange>& data,
        ValueInputOption option = ValueInputOption::UserEntered);

    /// Appends rows after the last row of the table found in `range`.
    UpdateResult append_values(const std::string& spreadsheet_id, const std::string& range,
                               const Grid& values,
                               ValueInputOption option = ValueInputOption::UserEntered);

    /// Clears the values (not formatting) in `range`.
    void clear_values(const std::string& spreadsheet_id, const std::string& range);

    /// Adds a sheet (tab) named `title` at the end and returns its properties.
    /// Throws HttpError if a sheet with that name already exists.
    SheetProperties add_sheet(const std::string& spreadsheet_id, const std::string& title);

    /// Deletes the sheet (tab) whose ID is `sheet_id` (see SheetProperties::sheet_id).
    void delete_sheet(const std::string& spreadsheet_id, int sheet_id);

    /// Deletes rows from a sheet, shifting the rows below them up.
    /// `sheet_id` is SheetProperties::sheet_id, not the sheet's title.
    void delete_rows(const std::string& spreadsheet_id, int sheet_id, RowRange rows);

    /// Deletes several blocks of rows in one request. The ranges may be in any
    /// order and may overlap; each range refers to row numbers before any
    /// deletion. Empty ranges are ignored.
    void delete_rows(const std::string& spreadsheet_id, int sheet_id,
                     const std::vector<RowRange>& rows);

    /// Override the API root (default "https://sheets.googleapis.com/v4"), e.g. for testing.
    void set_base_url(std::string url);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gsheetpp
