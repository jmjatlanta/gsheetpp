// Offline unit tests: no network access or real credentials required.

#include "base64.h"
#include "json_util.h"
#include "sheets_json.h"

#include <gsheetpp/gsheetpp.h>

#include <gtest/gtest.h>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace {

std::string base64url_decode(const std::string& in) {
    auto val = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '-') return 62;
        if (c == '_') return 63;
        return -1;
    };
    std::string out;
    int buf = 0, bits = 0;
    for (char c : in) {
        int v = val(c);
        if (v < 0) break;
        buf = (buf << 6) | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out += static_cast<char>((buf >> bits) & 0xFF);
        }
    }
    return out;
}

using PKeyPtr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;

PKeyPtr generate_rsa_key() {
    return PKeyPtr(EVP_RSA_gen(2048), &EVP_PKEY_free);
}

std::string private_key_pem(EVP_PKEY* key) {
    std::unique_ptr<BIO, decltype(&BIO_free)> bio(BIO_new(BIO_s_mem()), &BIO_free);
    PEM_write_bio_PrivateKey(bio.get(), key, nullptr, nullptr, 0, nullptr, nullptr);
    char* data = nullptr;
    long len = BIO_get_mem_data(bio.get(), &data);
    return std::string(data, static_cast<size_t>(len));
}

bool verify_rs256(EVP_PKEY* key, const std::string& data, const std::string& sig) {
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(),
                                                                &EVP_MD_CTX_free);
    if (EVP_DigestVerifyInit(ctx.get(), nullptr, EVP_sha256(), nullptr, key) != 1)
        return false;
    return EVP_DigestVerify(ctx.get(), reinterpret_cast<const unsigned char*>(sig.data()),
                            sig.size(), reinterpret_cast<const unsigned char*>(data.data()),
                            data.size()) == 1;
}

std::string escape_json(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '\n') out += "\\n";
        else if (c == '"') out += "\\\"";
        else out += c;
    }
    return out;
}

std::string make_service_account_json(const std::string& pem) {
    return R"({
  "type": "service_account",
  "project_id": "test-project",
  "private_key_id": "abc123",
  "private_key": ")" + escape_json(pem) + R"(",
  "client_email": "tester@test-project.iam.gserviceaccount.com",
  "client_id": "1234567890",
  "token_uri": "https://oauth2.googleapis.com/token"
})";
}

TEST(Base64Url, EncodesRfc4648TestVectors) {
    using gsheetpp::detail::base64url_encode;
    EXPECT_EQ(base64url_encode(""), "");
    EXPECT_EQ(base64url_encode("f"), "Zg");
    EXPECT_EQ(base64url_encode("fo"), "Zm8");
    EXPECT_EQ(base64url_encode("foo"), "Zm9v");
    EXPECT_EQ(base64url_encode("foob"), "Zm9vYg");
    EXPECT_EQ(base64url_encode("fooba"), "Zm9vYmE");
    EXPECT_EQ(base64url_encode("foobar"), "Zm9vYmFy");
}

TEST(Base64Url, UsesUrlSafeAlphabet) {
    EXPECT_EQ(gsheetpp::detail::base64url_encode(std::string("\xfb\xff", 2)), "-_8");
}

TEST(ServiceAccountInfo, ParsesValidJson) {
    auto info = gsheetpp::ServiceAccountInfo::from_json(make_service_account_json("PEM"));
    EXPECT_EQ(info.type, "service_account");
    EXPECT_EQ(info.project_id, "test-project");
    EXPECT_EQ(info.private_key_id, "abc123");
    EXPECT_EQ(info.private_key, "PEM");
    EXPECT_EQ(info.client_email, "tester@test-project.iam.gserviceaccount.com");
    EXPECT_EQ(info.token_uri, "https://oauth2.googleapis.com/token");
}

TEST(ServiceAccountInfo, RejectsInvalidJson) {
    using gsheetpp::AuthError;
    using gsheetpp::ServiceAccountInfo;
    EXPECT_THROW(ServiceAccountInfo::from_json("not json"), AuthError);
    EXPECT_THROW(ServiceAccountInfo::from_json("[]"), AuthError);
}

TEST(ServiceAccountInfo, RejectsWrongCredentialType) {
    EXPECT_THROW(gsheetpp::ServiceAccountInfo::from_json(
                     R"({"type":"authorized_user","private_key":"x","client_email":"y"})"),
                 gsheetpp::AuthError);
}

TEST(ServiceAccountInfo, RejectsMissingPrivateKey) {
    EXPECT_THROW(gsheetpp::ServiceAccountInfo::from_json(
                     R"({"type":"service_account","client_email":"y"})"),
                 gsheetpp::AuthError);
}

TEST(ServiceAccountInfo, RejectsMissingFile) {
    EXPECT_THROW(gsheetpp::ServiceAccountInfo::from_file("/nonexistent/path.json"),
                 gsheetpp::AuthError);
}

TEST(ServiceAccountCredentials, BuildsSignedAssertion) {
    PKeyPtr key = generate_rsa_key();
    ASSERT_NE(key, nullptr);

    auto info = gsheetpp::ServiceAccountInfo::from_json(
        make_service_account_json(private_key_pem(key.get())));
    gsheetpp::ServiceAccountCredentials creds(
        info, {gsheetpp::scopes::kSpreadsheets, "https://www.googleapis.com/auth/drive"});
    creds.set_subject("user@example.com");

    auto now = std::chrono::system_clock::time_point(std::chrono::seconds(1700000000));
    std::string jwt = creds.make_assertion(now);

    auto dot1 = jwt.find('.');
    ASSERT_NE(dot1, std::string::npos);
    auto dot2 = jwt.find('.', dot1 + 1);
    ASSERT_NE(dot2, std::string::npos);
    EXPECT_EQ(jwt.find('.', dot2 + 1), std::string::npos);

    rapidjson::Document header, claims;
    header.Parse(base64url_decode(jwt.substr(0, dot1)).c_str());
    claims.Parse(base64url_decode(jwt.substr(dot1 + 1, dot2 - dot1 - 1)).c_str());
    ASSERT_FALSE(header.HasParseError());
    ASSERT_FALSE(claims.HasParseError());

    using gsheetpp::detail::json_string;
    EXPECT_EQ(json_string(header, "alg"), "RS256");
    EXPECT_EQ(json_string(header, "typ"), "JWT");
    EXPECT_EQ(json_string(header, "kid"), "abc123");

    EXPECT_EQ(json_string(claims, "iss"), "tester@test-project.iam.gserviceaccount.com");
    EXPECT_EQ(json_string(claims, "aud"), "https://oauth2.googleapis.com/token");
    EXPECT_EQ(json_string(claims, "sub"), "user@example.com");
    EXPECT_EQ(json_string(claims, "scope"),
              "https://www.googleapis.com/auth/spreadsheets https://www.googleapis.com/auth/drive");
    ASSERT_TRUE(claims.HasMember("iat") && claims["iat"].IsInt64());
    ASSERT_TRUE(claims.HasMember("exp") && claims["exp"].IsInt64());
    EXPECT_EQ(claims["iat"].GetInt64(), 1700000000);
    EXPECT_EQ(claims["exp"].GetInt64(), 1700003600);

    std::string signature = base64url_decode(jwt.substr(dot2 + 1));
    EXPECT_TRUE(verify_rs256(key.get(), jwt.substr(0, dot2), signature));
    EXPECT_FALSE(verify_rs256(key.get(), jwt.substr(0, dot2) + "x", signature));
}

TEST(ServiceAccountCredentials, RejectsBadPrivateKey) {
    auto info = gsheetpp::ServiceAccountInfo::from_json(make_service_account_json("garbage"));
    gsheetpp::ServiceAccountCredentials creds(info, {gsheetpp::scopes::kSpreadsheets});
    EXPECT_THROW(creds.make_assertion(std::chrono::system_clock::now()), gsheetpp::AuthError);
}

TEST(Clients, RequireCredentials) {
    EXPECT_THROW(gsheetpp::SheetsClient(nullptr), gsheetpp::Error);
    EXPECT_THROW(gsheetpp::DriveClient(nullptr), gsheetpp::Error);
}

TEST(DriveClient, QuotesQueryStrings) {
    using gsheetpp::DriveClient;
    EXPECT_EQ(DriveClient::quote("GSheetPP"), "'GSheetPP'");
    EXPECT_EQ(DriveClient::quote("Bob's sheet"), "'Bob\\'s sheet'");
    EXPECT_EQ(DriveClient::quote("a\\b"), "'a\\\\b'");
}

TEST(Spreadsheet, FindsSheetByTitle) {
    gsheetpp::Spreadsheet s;
    s.sheets = {{0, "Orders", 0, 1000, 26}, {42, "Sim1", 1, 1000, 27}};
    ASSERT_NE(s.find_sheet("Sim1"), nullptr);
    EXPECT_EQ(s.find_sheet("Sim1")->sheet_id, 42);
    EXPECT_EQ(s.find_sheet("Sim2"), nullptr);
}

TEST(SheetsJson, NamesRenderOptions) {
    using namespace gsheetpp;
    EXPECT_STREQ(detail::to_string(ValueRenderOption::Formatted), "FORMATTED_VALUE");
    EXPECT_STREQ(detail::to_string(ValueRenderOption::Unformatted), "UNFORMATTED_VALUE");
    EXPECT_STREQ(detail::to_string(ValueRenderOption::Formula), "FORMULA");
    EXPECT_STREQ(detail::to_string(DateTimeRenderOption::FormattedString), "FORMATTED_STRING");
    EXPECT_STREQ(detail::to_string(DateTimeRenderOption::SerialNumber), "SERIAL_NUMBER");
}

TEST(SheetsJson, SerializesBatchUpdateValues) {
    std::vector<gsheetpp::ValueRange> data{{"Orders!A2", {{"1", "Buy"}}}, {"Orders!A5", {{"2"}}}};
    EXPECT_EQ(gsheetpp::detail::serialize_batch_update_values(data,
                                                              gsheetpp::ValueInputOption::Raw),
              R"({"valueInputOption":"RAW","data":[)"
              R"({"range":"Orders!A2","majorDimension":"ROWS","values":[["1","Buy"]]},)"
              R"({"range":"Orders!A5","majorDimension":"ROWS","values":[["2"]]}]})");
}

TEST(SheetsJson, SerializesAddAndDeleteSheet) {
    EXPECT_EQ(gsheetpp::detail::serialize_add_sheet("Sim \"1\""),
              R"({"requests":[{"addSheet":{"properties":{"title":"Sim \"1\""}}}]})");
    EXPECT_EQ(gsheetpp::detail::serialize_delete_sheet(42),
              R"({"requests":[{"deleteSheet":{"sheetId":42}}]})");
}

TEST(SheetsJson, NormalizesRowRangesBottomUp) {
    using gsheetpp::RowRange;
    auto ranges = gsheetpp::detail::normalize_row_ranges(
        {{1, 2}, {7, 9}, {5, 5}, {3, 4}, {8, 10}, {4, 6}, {-1, 3}});
    // {5,5} and {-1,3} are dropped; {3,4}, {4,6} merge; {7,9}, {8,10} merge.
    ASSERT_EQ(ranges.size(), 3u);
    EXPECT_EQ(ranges[0].start, 7);
    EXPECT_EQ(ranges[0].end, 10);
    EXPECT_EQ(ranges[1].start, 3);
    EXPECT_EQ(ranges[1].end, 6);
    EXPECT_EQ(ranges[2].start, 1);
    EXPECT_EQ(ranges[2].end, 2);
}

TEST(SheetsJson, SerializesDeleteRows) {
    EXPECT_EQ(gsheetpp::detail::serialize_delete_rows(7, {{1, 2}, {4, 6}}),
              R"({"requests":[)"
              R"({"deleteDimension":{"range":{"sheetId":7,"dimension":"ROWS","startIndex":4,"endIndex":6}}},)"
              R"({"deleteDimension":{"range":{"sheetId":7,"dimension":"ROWS","startIndex":1,"endIndex":2}}}]})");
    EXPECT_EQ(gsheetpp::detail::serialize_delete_rows(7, {{3, 3}}), "");
}

TEST(SheetsJson, ParsesSheetProperties) {
    rapidjson::Document doc;
    doc.Parse(R"({"sheetId":42,"title":"Sim1","index":2,)"
              R"("gridProperties":{"rowCount":1000,"columnCount":27}})");
    auto sp = gsheetpp::detail::parse_sheet_properties(doc);
    EXPECT_EQ(sp.sheet_id, 42);
    EXPECT_EQ(sp.title, "Sim1");
    EXPECT_EQ(sp.index, 2);
    EXPECT_EQ(sp.row_count, 1000);
    EXPECT_EQ(sp.column_count, 27);
}

TEST(SheetsJson, ParsesUnformattedNumbers) {
    rapidjson::Document doc;
    doc.Parse(R"({"range":"A1:C1","values":[[1234.5,42,true]]})");
    auto vr = gsheetpp::detail::parse_value_range(doc);
    ASSERT_EQ(vr.values.size(), 1u);
    EXPECT_EQ(vr.values[0], (gsheetpp::Row{"1234.5", "42", "true"}));
}

} // namespace
