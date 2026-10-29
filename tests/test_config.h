#pragma once

// Settings shared by the integration tests.

namespace gsheetpp::test {

// The "GSheet Test" spreadsheet, in the "GoShox" Drive folder.
inline constexpr const char* kSpreadsheetId = "1_YuhgBzrhjz6eoAhlvhBnv11m7Z8ACBRjgu1vpe76-E";

// Service account key (git-ignored). Tests are skipped if it is missing.
inline constexpr const char* kServiceAccountFile =
    GSHEETPP_TEST_RESOURCES_DIR "/service_account_gsheetpp.json";

} // namespace gsheetpp::test
