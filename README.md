# gsheetpp

A small C++17 library for reading and writing Google Sheets via the Sheets v4 REST API.

## Dependencies

| Dependency | How it is provided |
|------------|--------------------|
| libcurl    | System install (`find_package(CURL)`) |
| rapidjson  | Git submodule at `external/rapidjson` (header-only) |
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

The spreadsheet must be shared with the service account's `client_email`.
For Google Workspace domain-wide delegation, call `creds->set_subject("user@domain")`.

See `examples/read_sheet.cpp`:

```sh
./build/examples/read_sheet service_account.json <spreadsheet_id> "Sheet1!A1:D20"
```

## Authentication design

`SheetsClient` depends only on the abstract `gsheetpp::Credentials` interface
(`include/gsheetpp/credentials.h`), which has one method: `std::string access_token()`.

- `ServiceAccountCredentials` implements the OAuth2 JWT-bearer flow and caches the token,
  refreshing it 60 seconds before it expires.
- OAuth2 user authentication (authorization code + refresh token) can be added later as
  another `Credentials` subclass. `SheetsClient` won't need any changes.

## Errors

All exceptions derive from `gsheetpp::Error`:
- `AuthError`: a bad key file, a signing failure, or the token endpoint rejected the request.
- `HttpError`: a transport failure or a non-2xx API response. Includes `status()` and `body()`.

## License

MIT. See [LICENSE](LICENSE).
