# Setting up a Google Cloud service account

This guide shows how to use the Google Cloud Console website to create a service account and
download its JSON key file, which gsheetpp uses to access Google Sheets.

> Google sometimes renames or moves items in the Cloud Console. If a menu item below doesn't
> match what you see, use the search bar at the top of the console. For example, search for
> "Service Accounts" or "Google Sheets API".

## Prerequisites

- A Google account.
- Access to the [Google Cloud Console](https://console.cloud.google.com/). Creating a project is
  free, and the Sheets API has no charge (usage quotas apply).

## 1. Create or select a project

1. Go to <https://console.cloud.google.com/>.
2. In the top bar, click the **project selector** (it shows the current project's name, or
   "Select a project").
3. Either pick an existing project, or click **New Project**:
   - Enter a **Project name** (for example, `gsheetpp-demo`).
   - Choose an **Organization/Location** if asked. Personal accounts can leave this as
     "No organization".
   - Click **Create**, then make sure the new project is selected in the project selector.

## 2. Enable the Google Sheets API

1. Open the navigation menu (☰) and go to **APIs & Services → Library**.
2. Search for **Google Sheets API** and click it.
3. Click **Enable**.

If you want to use `DriveClient` to find spreadsheets by name or list folders, also enable the
**Google Drive API** the same way. `SheetsClient` doesn't need it.

## 3. Create the service account

1. Open the navigation menu (☰) and go to **IAM & Admin → Service Accounts**.
2. Click **+ Create service account**.
3. **Service account details:**
   - **Service account name:** for example, `sheets-reader`.
   - **Service account ID:** filled in automatically. This becomes the first part of the
     account's email address.
   - **Description:** optional.
   - Click **Create and continue**.
4. **Grant this service account access to project (optional):** skip this step. To access a
   spreadsheet, the service account needs the sheet to be *shared* with it (step 5), not a
   project IAM role. Click **Continue**.
5. **Grant users access to this service account (optional):** skip this step. Click **Done**.

The new account appears in the list with an email address like:

```
sheets-reader@your-project-id.iam.gserviceaccount.com
```

Copy this address. You'll need it in step 5.

## 4. Create and download a JSON key

1. In the **Service Accounts** list, click the email address of the account you just created.
2. Open the **Keys** tab.
3. Click **Add key → Create new key**.
4. Select **JSON** and click **Create**.
5. Your browser downloads a `.json` file. **This is the only copy of the private key.** Google
   can't download it again, so if you lose it you must create a new key.

The file looks like this (values shortened):

```json
{
  "type": "service_account",
  "project_id": "your-project-id",
  "private_key_id": "1a2b3c...",
  "private_key": "-----BEGIN PRIVATE KEY-----\n...\n-----END PRIVATE KEY-----\n",
  "client_email": "sheets-reader@your-project-id.iam.gserviceaccount.com",
  "client_id": "123456789012345678901",
  "auth_uri": "https://accounts.google.com/o/oauth2/auth",
  "token_uri": "https://oauth2.googleapis.com/token",
  ...
}
```

gsheetpp reads `type`, `private_key`, `private_key_id`, `client_email` and `token_uri` from
this file.

> **"Key creation is disabled"?** Some organizations block service account key creation with the
> `iam.disableServiceAccountKeyCreation` organization policy. Ask your Google Cloud
> administrator to make an exception for your project, or use a project outside the
> organization.

## 5. Share the spreadsheet with the service account

A service account is a separate Google identity. It can't see your spreadsheets until you share
them with it.

1. Open the spreadsheet in Google Sheets.
2. Click **Share**.
3. Paste the service account's email address (`client_email` from the JSON file).
4. Choose a role:
   - **Viewer** if you only read data (`get_spreadsheet`, `get_values`, `batch_get_values`).
   - **Editor** if you also write data (`update_values`, `append_values`, `clear_values`).
5. Uncheck **Notify people**, since the service account has no mailbox, and click **Share**
   (or **Send**).

## 6. Find the spreadsheet ID

The spreadsheet ID is the long string between `/d/` and `/edit` in the sheet's URL:

```
https://docs.google.com/spreadsheets/d/1BxiMVs0XRA5nFMdKvBdBZjgmUUqptlbs74OgvE2upms/edit#gid=0
                                       └──────────────── spreadsheet ID ───────────┘
```

## 7. Test it with gsheetpp

```sh
./build/examples/read_sheet /path/to/key.json <spreadsheet_id> "Sheet1!A1:D10"
```

Or, in code:

```cpp
auto creds = gsheetpp::ServiceAccountCredentials::from_file(
    "/path/to/key.json", {gsheetpp::scopes::kSpreadsheetsReadOnly});
gsheetpp::SheetsClient client(creds);
auto values = client.get_values("<spreadsheet_id>", "Sheet1!A1:D10");
```

Use `gsheetpp::scopes::kSpreadsheets` instead of `kSpreadsheetsReadOnly` if you need to write.

## Troubleshooting

| Error | Likely cause |
|-------|--------------|
| `HTTP 403: The caller does not have permission` | The spreadsheet isn't shared with the service account's `client_email`, or you're writing with Viewer access. |
| `HTTP 403: Google Sheets API has not been used in project ... or it is disabled` | Step 2 wasn't done, or it was done in a different project than the one that owns the service account. Enabling can take a few minutes to take effect. |
| `Drive API ... HTTP 403: Google Drive API has not been used in project ...` | The Drive API isn't enabled. See step 2. |
| `DriveClient` finds no files or folders | The folder or file isn't shared with the service account. Sharing a folder gives access to everything inside it. |
| `HTTP 404: Requested entity was not found` | The spreadsheet ID is wrong. |
| `HTTP 400: Unable to parse range` | The range is invalid, or the sheet (tab) name is misspelled. Quote names that contain spaces: `'My Sheet'!A1:B2`. |
| `invalid_grant (Invalid JWT Signature)` | The key was deleted or disabled in the console. Create a new key. |
| `invalid_grant (Invalid JWT: Token must be a short-lived token...)` | Your computer's clock is wrong. Sync it with NTP. |
| `invalid_grant (account not found)` | The service account was deleted, or `client_email` is wrong. |

## Security notes

- Treat the JSON key like a password. **Never commit it to source control.** This project's
  `.gitignore` excludes `service_account*.json`, so naming your key that way gives some
  protection, but storing it outside the repository is safer.
- Share only the spreadsheets the service account actually needs, with the lowest role that works.
- Request the narrowest scope that works (`spreadsheets.readonly` when you only read).
- Rotate keys periodically: create a new key, deploy it, then delete the old key from the
  **Keys** tab.
- If a key leaks, delete it from the **Keys** tab right away.

## Optional: domain-wide delegation (Google Workspace only)

To have the service account act as a user in your Workspace domain (instead of sharing each sheet
with it), a Workspace super administrator must grant domain-wide delegation:

1. In the Cloud Console, open the service account and copy its **Unique ID** (the `client_id`).
2. In the [Google Admin console](https://admin.google.com/), go to **Security → Access and data
   control → API controls → Manage Domain Wide Delegation**.
3. Click **Add new**, paste the client ID, and enter the scopes, for example
   `https://www.googleapis.com/auth/spreadsheets`.
4. In code, call `creds->set_subject("user@yourdomain.com")` before making requests.
