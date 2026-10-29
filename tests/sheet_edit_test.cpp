// Integration tests that change the structure of a real Google Sheet: adding
// and deleting sheets (tabs) and rows, batch writes, and read render options.
//
// Each test works in its own temporary sheet, which is deleted afterwards, so
// the test spreadsheet is left as it was. Requires network access and the
// service account key described in test_config.h, with Editor access to the
// test sheet. If the key file is missing, the tests are skipped.

#include "test_config.h"

#include <gsheetpp/gsheetpp.h>

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>

namespace {

using gsheetpp::test::kServiceAccountFile;
using gsheetpp::test::kSpreadsheetId;

// A sheet name unlikely to clash with a real one, or with a parallel test run.
std::string temp_sheet_title() {
    auto now = std::chrono::system_clock::now().time_since_epoch();
    return "gsheetpp-test-" +
           std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

class SheetEditTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!std::filesystem::exists(kServiceAccountFile))
            GTEST_SKIP() << "Service account key not found: " << kServiceAccountFile;

        auto creds = gsheetpp::ServiceAccountCredentials::from_file(
            kServiceAccountFile, {gsheetpp::scopes::kSpreadsheets});
        client_ = std::make_unique<gsheetpp::SheetsClient>(creds);

        title_ = temp_sheet_title();
        try {
            sheet_ = client_->add_sheet(kSpreadsheetId, title_);
            added_ = true;
        } catch (const gsheetpp::HttpError& e) {
            FAIL() << "add_sheet: HTTP " << e.status() << ": " << e.what() << "\n" << e.body();
        }
    }

    void TearDown() override {
        if (!added_)
            return;
        try {
            client_->delete_sheet(kSpreadsheetId, sheet_.sheet_id);
        } catch (const std::exception& e) {
            ADD_FAILURE() << "Failed to delete temporary sheet " << title_ << ": " << e.what();
        }
    }

    // `cells` on the temporary sheet, e.g. "A1:B2".
    std::string range(const std::string& cells) const { return "'" + title_ + "'!" + cells; }

    std::unique_ptr<gsheetpp::SheetsClient> client_;
    std::string title_;
    gsheetpp::SheetProperties sheet_;
    bool added_ = false;
};

TEST_F(SheetEditTest, AddsSheetThatAppearsInMetadata) {
    EXPECT_EQ(sheet_.title, title_);

    gsheetpp::Spreadsheet s = client_->get_spreadsheet(kSpreadsheetId);
    const gsheetpp::SheetProperties* found = s.find_sheet(title_);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->sheet_id, sheet_.sheet_id);
}

TEST_F(SheetEditTest, AddingDuplicateSheetThrows) {
    EXPECT_THROW(client_->add_sheet(kSpreadsheetId, title_), gsheetpp::HttpError);
}

TEST_F(SheetEditTest, BatchUpdatesSeveralRanges) {
    try {
        auto results = client_->batch_update_values(
            kSpreadsheetId, {{range("A1"), {{"a", "b"}}}, {range("A3"), {{"c"}}}});
        ASSERT_EQ(results.size(), 2u);
        EXPECT_EQ(results[0].updated_cells, 2);
        EXPECT_EQ(results[1].updated_cells, 1);

        auto values = client_->get_values(kSpreadsheetId, range("A1:B3")).values;
        ASSERT_EQ(values.size(), 3u);
        EXPECT_EQ(values[0], (gsheetpp::Row{"a", "b"}));
        EXPECT_TRUE(values[1].empty());
        EXPECT_EQ(values[2], (gsheetpp::Row{"c"}));
    } catch (const gsheetpp::HttpError& e) {
        FAIL() << "HTTP " << e.status() << ": " << e.what() << "\n" << e.body();
    }
}

TEST_F(SheetEditTest, DeletesRowsAndShiftsTheRestUp) {
    try {
        client_->update_values(kSpreadsheetId, range("A1"),
                               {{"0"}, {"1"}, {"2"}, {"3"}, {"4"}, {"5"}});

        // Delete UI rows 2 and 4-5 in one request; ranges refer to the original rows.
        client_->delete_rows(kSpreadsheetId, sheet_.sheet_id, {{1, 2}, {3, 5}});

        auto values = client_->get_values(kSpreadsheetId, range("A1:A6")).values;
        ASSERT_EQ(values.size(), 3u);
        EXPECT_EQ(values[0], (gsheetpp::Row{"0"}));
        EXPECT_EQ(values[1], (gsheetpp::Row{"2"}));
        EXPECT_EQ(values[2], (gsheetpp::Row{"5"}));

        // The single-range overload.
        client_->delete_rows(kSpreadsheetId, sheet_.sheet_id, gsheetpp::RowRange{0, 1});
        values = client_->get_values(kSpreadsheetId, range("A1:A6")).values;
        ASSERT_EQ(values.size(), 2u);
        EXPECT_EQ(values[0], (gsheetpp::Row{"2"}));
    } catch (const gsheetpp::HttpError& e) {
        FAIL() << "HTTP " << e.status() << ": " << e.what() << "\n" << e.body();
    }
}

TEST_F(SheetEditTest, ReadsWithRenderOptions) {
    using gsheetpp::DateTimeRenderOption;
    using gsheetpp::ValueRenderOption;
    try {
        // USER_ENTERED turns these into a number, a formula and a date.
        client_->update_values(kSpreadsheetId, range("A1"), {{"1234.5", "=A1*2", "2000-01-01"}});
        const std::string cells = range("A1:C1");

        auto formula = client_->get_values(kSpreadsheetId, cells, ValueRenderOption::Formula);
        ASSERT_EQ(formula.values.size(), 1u);
        ASSERT_EQ(formula.values[0].size(), 3u);
        EXPECT_EQ(formula.values[0][1], "=A1*2");

        auto unformatted = client_->get_values(kSpreadsheetId, cells,
                                               ValueRenderOption::Unformatted,
                                               DateTimeRenderOption::SerialNumber);
        ASSERT_EQ(unformatted.values.size(), 1u);
        EXPECT_EQ(unformatted.values[0], (gsheetpp::Row{"1234.5", "2469", "36526"}));

        // Dates as text, numbers as numbers: the same date text as the default read.
        auto formatted = client_->get_values(kSpreadsheetId, cells);
        auto mixed = client_->batch_get_values(kSpreadsheetId, {cells},
                                               ValueRenderOption::Unformatted,
                                               DateTimeRenderOption::FormattedString);
        ASSERT_EQ(formatted.values.size(), 1u);
        ASSERT_EQ(mixed.size(), 1u);
        ASSERT_EQ(mixed[0].values.size(), 1u);
        EXPECT_EQ(mixed[0].values[0][0], "1234.5");
        EXPECT_EQ(mixed[0].values[0][2], formatted.values[0][2]);
    } catch (const gsheetpp::HttpError& e) {
        FAIL() << "HTTP " << e.status() << ": " << e.what() << "\n" << e.body();
    }
}

} // namespace
