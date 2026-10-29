# gsheetpp

A small C++17 library for reading and writing Google Sheets via the Sheets v4 REST API.

## Dependencies

| Dependency | How it is provided |
|------------|--------------------|
| libcurl    | System install (`find_package(CURL)`) |
| rapidjson  | Git submodule at `external/rapidjson` (header-only) |
| Google Test | Git submodule at `external/googletest` (tests only) |
| OpenSSL (libcrypto) | System install; used only to RS256-sign the service account JWT |

## Building

```sh
git submodule update --init --recursive
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

On macOS the build looks up Homebrew's `openssl@3` automatically. Elsewhere, pass
`-DOPENSSL_ROOT_DIR=...` if OpenSSL is in a non-standard location.

Options: `GSHEETPP_BUILD_EXAMPLES` (ON), `GSHEETPP_BUILD_TESTS` (ON).

### Tests

All tests use Google Test (a git submodule at `external/googletest`). They're labelled for CTest:

- `unit`: offline tests that need no network or credentials (`tests/unit_test.cpp`).
- `integration`: tests that use the real Google APIs (see below).

```sh
ctest --test-dir build -L unit
```

### Integration tests

`tests/sheet_read_test.cpp` and `tests/drive_test.cpp` use a real spreadsheet and Drive folder.
They read their credentials from `tests/test_resources/service_account_gsheetpp.json`, which is
git-ignored and must be supplied locally. The test sheet and the `GSheet` folder must be shared
with that service account. If the file is missing, these tests are skipped.

```sh
ctest --test-dir build -L integration --output-on-failure
```

To use from another CMake project:

```cmake
add_subdirectory(gsheetpp)
target_link_libraries(my_app PRIVATE gsheetpp::gsheetpp)
```

## Usage

```cpp
#include <gsheetpp/gsheetpp.h>

auto creds = gsheetpp::ServiceAccountCredentials::from_file(
    "service_account.json", {gsheetpp::scopes::kSpreadsheets});
gsheetpp::SheetsClient client(creds);

auto meta   = client.get_spreadsheet(spreadsheet_id);
auto values = client.get_values(spreadsheet_id, "Sheet1!A1:C10");
client.update_values(spreadsheet_id, "Sheet1!A1", {{"Name", "Score"}, {"Ada", "42"}});
client.append_values(spreadsheet_id, "Sheet1!A:B", {{"Grace", "99"}});
```

### Finding spreadsheets by name

`DriveClient` searches Google Drive, so you can look up a spreadsheet's ID by its name. It needs
a Drive scope and the Google Drive API enabled in your Cloud project:

```cpp
auto creds = gsheetpp::ServiceAccountCredentials::from_file(
    "service_account.json",
    {gsheetpp::scopes::kSpreadsheets, gsheetpp::scopes::kDriveMetadataReadOnly});
gsheetpp::DriveClient drive(creds);

auto folders = drive.find_folders("GSheet");
auto sheets  = drive.find_spreadsheets("GSheet Test", folders.at(0).id);
gsheetpp::SheetsClient client(creds);
auto values  = client.get_values(sheets.at(0).id, "A1:B2");
```

Names aren't unique in Drive, so these functions return every match. The service account only
sees files and folders that are shared with it. `list_folder()` lists a folder's contents, and
`search()` runs any [Drive search query](https://developers.google.com/workspace/drive/api/guides/search-files).

The spreadsheet must be shared with the service account's `client_email`. See
[SERVICE_ACCOUNT_SETUP.md](SERVICE_ACCOUNT_SETUP.md) for how to create a service
account and its key in the Google Cloud Console.
For Google Workspace domain-wide delegation, call `creds->set_subject("user@domain")`.

See `examples/read_sheet.cpp`:

```sh
./build/examples/read_sheet service_account.json <spreadsheet_id> "Sheet1!A1:D20"
```

## Authentication design

`SheetsClient` and `DriveClient` depend only on the abstract `gsheetpp::Credentials` interface
(`include/gsheetpp/credentials.h`), which has one method: `std::string access_token()`.

- `ServiceAccountCredentials` implements the OAuth2 JWT-bearer flow and caches the token,
  refreshing it 60 seconds before it expires.
- OAuth2 user authentication (authorization code + refresh token) can be added later as
  another `Credentials` subclass. Neither client will need any changes.

## Errors

All exceptions derive from `gsheetpp::Error`:
- `AuthError`: a bad key file, a signing failure, or the token endpoint rejected the request.
- `HttpError`: a transport failure or a non-2xx API response. Includes `status()` and `body()`.

## License

MIT. See [LICENSE](LICENSE).
