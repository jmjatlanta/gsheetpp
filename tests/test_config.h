#pragma once

// Settings shared by the integration tests.

namespace gsheetpp::test {

// The test spreadsheet: its ID, its name, and the Drive folder it lives in.
inline constexpr const char* kSpreadsheetId = "1_YuhgBzrhjz6eoAhlvhBnv11m7Z8ACBRjgu1vpe76-E";
inline constexpr const char* kSpreadsheetName = "GSheet Test";
inline constexpr const char* kFolderName = "GSheet";

// Service account key (git-ignored). Tests are skipped if it is missing.
inline constexpr const char* kServiceAccountFile =
    GSHEETPP_TEST_RESOURCES_DIR "/service_account_gsheetpp.json";

} // namespace gsheetpp::test
