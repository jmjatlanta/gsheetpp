#pragma once

#include "gsheetpp/credentials.h"

#include <memory>
#include <string>
#include <vector>

namespace gsheetpp {

/// Drive MIME types used when searching.
namespace mime_types {
inline constexpr const char* kFolder = "application/vnd.google-apps.folder";
inline constexpr const char* kSpreadsheet = "application/vnd.google-apps.spreadsheet";
} // namespace mime_types

/// Metadata for a file or folder in Google Drive.
struct DriveFile {
    std::string id;  // for a spreadsheet, this is the spreadsheet ID used by SheetsClient
    std::string name;
    std::string mime_type;
    std::vector<std::string> parents;  // IDs of the containing folders
    std::string modified_time;         // RFC 3339, e.g. "2026-10-29T16:05:12.345Z"
    std::string web_view_link;
};

/// Client for searching Google Drive (Drive v3 REST API).
///
/// Requires a Drive scope such as scopes::kDriveMetadataReadOnly, and the
/// Drive API to be enabled for the Cloud project. A service account sees only
/// files and folders shared with it, or in shared drives it is a member of.
///
/// Names are not unique in Drive, so lookups by name return every match.
/// Trashed items are always excluded.
///
/// A DriveClient instance is not thread-safe; use one per thread. The
/// Credentials object may be shared between clients.
class DriveClient {
public:
    explicit DriveClient(std::shared_ptr<Credentials> credentials);
    ~DriveClient();

    DriveClient(const DriveClient&) = delete;
    DriveClient& operator=(const DriveClient&) = delete;
    DriveClient(DriveClient&&) noexcept;
    DriveClient& operator=(DriveClient&&) noexcept;

    /// Folders named `name`.
    std::vector<DriveFile> find_folders(const std::string& name);

    /// Spreadsheets named `name`. If `folder_id` is non-empty, only those
    /// directly inside that folder.
    std::vector<DriveFile> find_spreadsheets(const std::string& name,
                                             const std::string& folder_id = {});

    /// Items directly inside a folder. If `mime_type` is non-empty, only
    /// items of that type (see mime_types).
    std::vector<DriveFile> list_folder(const std::string& folder_id,
                                       const std::string& mime_type = {});

    /// Runs a raw Drive search query (the `q` parameter of files.list), e.g.
    /// "name contains 'Budget' and modifiedTime > '2026-01-01T00:00:00'".
    /// Fetches every page of results.
    /// See https://developers.google.com/workspace/drive/api/guides/search-files
    std::vector<DriveFile> search(const std::string& query);

    /// Override the API root (default "https://www.googleapis.com/drive/v3"), e.g. for testing.
    void set_base_url(std::string url);

    /// Quotes `value` for use as a string literal in a Drive query,
    /// escaping backslashes and single quotes.
    static std::string quote(const std::string& value);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gsheetpp
