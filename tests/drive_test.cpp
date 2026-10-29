// Integration test against Google Drive.
//
// Requires network access, the service account key described in
// test_config.h, and the Google Drive API enabled for the key's Cloud project.
// The "GoShox" folder must be shared with the service account. If the key
// file is missing, the test is skipped.

#include "test_config.h"

#include <gsheetpp/gsheetpp.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <vector>

namespace {

using gsheetpp::test::kServiceAccountFile;
using gsheetpp::test::kSpreadsheetId;

class DriveTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!std::filesystem::exists(kServiceAccountFile))
            GTEST_SKIP() << "Service account key not found: " << kServiceAccountFile;

        auto creds = gsheetpp::ServiceAccountCredentials::from_file(
            kServiceAccountFile, {gsheetpp::scopes::kDriveMetadataReadOnly});
        client_ = std::make_unique<gsheetpp::DriveClient>(creds);
    }

    std::unique_ptr<gsheetpp::DriveClient> client_;
};

TEST_F(DriveTest, FindsSpreadsheetByNameInFolder) {
    try {
        std::vector<gsheetpp::DriveFile> folders = client_->find_folders("GoShox");
        ASSERT_EQ(folders.size(), 1u) << "Expected exactly one folder named GoShox";
        EXPECT_EQ(folders[0].mime_type, gsheetpp::mime_types::kFolder);

        std::vector<gsheetpp::DriveFile> sheets =
            client_->find_spreadsheets("GSheet Test", folders[0].id);
        ASSERT_EQ(sheets.size(), 1u) << "Expected exactly one spreadsheet named GSheet Test";
        EXPECT_EQ(sheets[0].name, "GSheet Test");
        EXPECT_EQ(sheets[0].mime_type, gsheetpp::mime_types::kSpreadsheet);
        EXPECT_EQ(sheets[0].id, kSpreadsheetId);
    } catch (const gsheetpp::HttpError& e) {
        FAIL() << "HTTP " << e.status() << ": " << e.what() << "\n" << e.body();
    } catch (const gsheetpp::Error& e) {
        FAIL() << e.what();
    }
}

} // namespace
