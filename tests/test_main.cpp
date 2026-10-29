// Offline unit tests: no network access or real credentials required.

#include "base64.h"
#include "json_util.h"

#include <gsheetpp/gsheetpp.h>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            std::cerr << __FILE__ << ":" << __LINE__ << ": CHECK failed: " #cond "\n"; \
            ++g_failures;                                                              \
        }                                                                              \
    } while (0)

template <typename E>
bool throws(const std::function<void()>& fn) {
    try {
        fn();
    } catch (const E&) {
        return true;
    } catch (...) {
    }
    return false;
}

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

void test_base64url() {
    using gsheetpp::detail::base64url_encode;
    CHECK(base64url_encode("") == "");
    CHECK(base64url_encode("f") == "Zg");
    CHECK(base64url_encode("fo") == "Zm8");
    CHECK(base64url_encode("foo") == "Zm9v");
    CHECK(base64url_encode("foob") == "Zm9vYg");
    CHECK(base64url_encode("fooba") == "Zm9vYmE");
    CHECK(base64url_encode("foobar") == "Zm9vYmFy");
    CHECK(base64url_encode(std::string("\xfb\xff", 2)) == "-_8");
}

void test_service_account_parsing() {
    using gsheetpp::AuthError;
    using gsheetpp::ServiceAccountInfo;

    auto info = ServiceAccountInfo::from_json(make_service_account_json("PEM"));
    CHECK(info.type == "service_account");
    CHECK(info.project_id == "test-project");
    CHECK(info.private_key_id == "abc123");
    CHECK(info.private_key == "PEM");
    CHECK(info.client_email == "tester@test-project.iam.gserviceaccount.com");
    CHECK(info.token_uri == "https://oauth2.googleapis.com/token");

    CHECK(throws<AuthError>([] { ServiceAccountInfo::from_json("not json"); }));
    CHECK(throws<AuthError>([] { ServiceAccountInfo::from_json("[]"); }));
    CHECK(throws<AuthError>([] {
        ServiceAccountInfo::from_json(R"({"type":"authorized_user","private_key":"x","client_email":"y"})");
    }));
    CHECK(throws<AuthError>([] {
        ServiceAccountInfo::from_json(R"({"type":"service_account","client_email":"y"})");
    }));
    CHECK(throws<AuthError>([] { ServiceAccountInfo::from_file("/nonexistent/path.json"); }));
}

void test_jwt_assertion() {
    PKeyPtr key = generate_rsa_key();
    CHECK(key != nullptr);
    if (!key) return;

    auto info = gsheetpp::ServiceAccountInfo::from_json(
        make_service_account_json(private_key_pem(key.get())));
    gsheetpp::ServiceAccountCredentials creds(
        info, {gsheetpp::scopes::kSpreadsheets, "https://www.googleapis.com/auth/drive"});
    creds.set_subject("user@example.com");

    auto now = std::chrono::system_clock::time_point(std::chrono::seconds(1700000000));
    std::string jwt = creds.make_assertion(now);

    auto dot1 = jwt.find('.');
    auto dot2 = jwt.find('.', dot1 + 1);
    CHECK(dot1 != std::string::npos && dot2 != std::string::npos);
    CHECK(jwt.find('.', dot2 + 1) == std::string::npos);

    rapidjson::Document header, claims;
    header.Parse(base64url_decode(jwt.substr(0, dot1)).c_str());
    claims.Parse(base64url_decode(jwt.substr(dot1 + 1, dot2 - dot1 - 1)).c_str());

    using gsheetpp::detail::json_string;
    CHECK(json_string(header, "alg") == "RS256");
    CHECK(json_string(header, "typ") == "JWT");
    CHECK(json_string(header, "kid") == "abc123");

    CHECK(json_string(claims, "iss") == "tester@test-project.iam.gserviceaccount.com");
    CHECK(json_string(claims, "aud") == "https://oauth2.googleapis.com/token");
    CHECK(json_string(claims, "sub") == "user@example.com");
    CHECK(json_string(claims, "scope") ==
          "https://www.googleapis.com/auth/spreadsheets https://www.googleapis.com/auth/drive");
    CHECK(claims["iat"].GetInt64() == 1700000000);
    CHECK(claims["exp"].GetInt64() == 1700003600);

    std::string signature = base64url_decode(jwt.substr(dot2 + 1));
    CHECK(verify_rs256(key.get(), jwt.substr(0, dot2), signature));
    CHECK(!verify_rs256(key.get(), jwt.substr(0, dot2) + "x", signature));
}

void test_bad_private_key() {
    auto info = gsheetpp::ServiceAccountInfo::from_json(make_service_account_json("garbage"));
    gsheetpp::ServiceAccountCredentials creds(info, {gsheetpp::scopes::kSpreadsheets});
    CHECK(throws<gsheetpp::AuthError>(
        [&] { creds.make_assertion(std::chrono::system_clock::now()); }));
}

void test_client_requires_credentials() {
    CHECK(throws<gsheetpp::Error>([] { gsheetpp::SheetsClient client(nullptr); }));
    CHECK(throws<gsheetpp::Error>([] { gsheetpp::DriveClient client(nullptr); }));
}

void test_drive_query_quoting() {
    using gsheetpp::DriveClient;
    CHECK(DriveClient::quote("GoShox") == "'GoShox'");
    CHECK(DriveClient::quote("Bob's sheet") == "'Bob\\'s sheet'");
    CHECK(DriveClient::quote("a\\b") == "'a\\\\b'");
}

} // namespace

int main() {
    test_base64url();
    test_service_account_parsing();
    test_jwt_assertion();
    test_bad_private_key();
    test_client_requires_credentials();
    test_drive_query_quoting();

    if (g_failures) {
        std::cerr << g_failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "All tests passed\n";
    return 0;
}
