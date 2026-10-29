// Usage: read_sheet <service_account.json> <spreadsheet_id> [range]
//
// The spreadsheet must be shared with the service account's client_email.

#include <gsheetpp/gsheetpp.h>

#include <iostream>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <service_account.json> <spreadsheet_id> [range]\n";
        return 1;
    }

    try {
        auto creds = gsheetpp::ServiceAccountCredentials::from_file(
            argv[1], {gsheetpp::scopes::kSpreadsheetsReadOnly});
        gsheetpp::SheetsClient client(creds);

        gsheetpp::Spreadsheet sheet = client.get_spreadsheet(argv[2]);
        std::cout << "Spreadsheet: " << sheet.title << "\n";
        for (const auto& tab : sheet.sheets)
            std::cout << "  [" << tab.index << "] " << tab.title << " (" << tab.row_count << "x"
                      << tab.column_count << ")\n";

        std::string range = argc > 3 ? argv[3] : (sheet.sheets.empty() ? "A1:Z20"
                                                                        : sheet.sheets[0].title);
        gsheetpp::ValueRange values = client.get_values(argv[2], range);
        std::cout << "\n" << values.range << ":\n";
        for (const auto& row : values.values) {
            for (size_t i = 0; i < row.size(); ++i)
                std::cout << (i ? "\t" : "") << row[i];
            std::cout << "\n";
        }
    } catch (const gsheetpp::HttpError& e) {
        std::cerr << "HTTP error (" << e.status() << "): " << e.what() << "\n";
        return 1;
    } catch (const gsheetpp::Error& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
