// Integration test against a real Google Sheet.
//
// Requires network access and the service account key described in
// test_config.h. The test sheet must be shared with that account as an Editor (the update
// test writes to it). If the key file is missing, the tests are skipped.

#include "test_config.h"

#include <gsheetpp/gsheetpp.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>

namespace {

using gsheetpp::test::kServiceAccountFile;
using gsheetpp::test::kSpreadsheetId;

// The cell holding the answer, on the first sheet.
constexpr const char* kAnswerCell = "B2";

class SheetTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!std::filesystem::exists(kServiceAccountFile))
            GTEST_SKIP() << "Service account key not found: " << kServiceAccountFile;

        auto creds = gsheetpp::ServiceAccountCredentials::from_file(
            kServiceAccountFile, {gsheetpp::scopes::kSpreadsheets});
        client_ = std::make_unique<gsheetpp::SheetsClient>(creds);
    }

    std::unique_ptr<gsheetpp::SheetsClient> client_;
};

TEST_F(SheetTest, ReadsFirstTwoColumnsOfFirstTwoRows) {
    // A range without a sheet name refers to the first sheet.
    gsheetpp::ValueRange result;
    try {
        result = client_->get_values(kSpreadsheetId, "A1:B2");
    } catch (const gsheetpp::HttpError& e) {
        FAIL() << "HTTP " << e.status() << ": " << e.what() << "\n" << e.body();
    } catch (const gsheetpp::Error& e) {
        FAIL() << e.what();
    }

    ASSERT_EQ(result.values.size(), 2u);
    ASSERT_EQ(result.values[0].size(), 2u);
    ASSERT_EQ(result.values[1].size(), 2u);

    EXPECT_EQ(result.values[0][0], "Hello");
    EXPECT_EQ(result.values[0][1], "World");
    EXPECT_EQ(result.values[1][0], "The Answer is");
    EXPECT_EQ(result.values[1][1], "42");
}

// Writes `value` to the answer cell and returns what the API reports updating.
gsheetpp::UpdateResult set_answer(gsheetpp::SheetsClient& client, const std::string& value) {
    return client.update_values(kSpreadsheetId, kAnswerCell, {{value}});
}

// Restores the answer cell to 42 when it goes out of scope, so the sheet is
// left as it was even if an assertion fails partway through the test.
class RestoreAnswer {
public:
    explicit RestoreAnswer(gsheetpp::SheetsClient& client) : client_(client) {}
    ~RestoreAnswer() {
        try {
            set_answer(client_, "42");
        } catch (const std::exception& e) {
            ADD_FAILURE() << "Failed to restore " << kAnswerCell << " to 42: " << e.what();
        }
    }

private:
    gsheetpp::SheetsClient& client_;
};

TEST_F(SheetTest, UpdatesAnswerAndRestoresIt) {
    try {
        RestoreAnswer restore(*client_);

        gsheetpp::UpdateResult update = set_answer(*client_, "43");
        EXPECT_EQ(update.updated_cells, 1);

        gsheetpp::ValueRange result = client_->get_values(kSpreadsheetId, kAnswerCell);
        ASSERT_EQ(result.values.size(), 1u);
        ASSERT_EQ(result.values[0].size(), 1u);
        EXPECT_EQ(result.values[0][0], "43");
    } catch (const gsheetpp::HttpError& e) {
        FAIL() << "HTTP " << e.status() << ": " << e.what() << "\n" << e.body();
    } catch (const gsheetpp::Error& e) {
        FAIL() << e.what();
    }

    // The guard has written 42 back; confirm it.
    gsheetpp::ValueRange restored = client_->get_values(kSpreadsheetId, kAnswerCell);
    ASSERT_EQ(restored.values.size(), 1u);
    ASSERT_EQ(restored.values[0].size(), 1u);
    EXPECT_EQ(restored.values[0][0], "42");
}

} // namespace
