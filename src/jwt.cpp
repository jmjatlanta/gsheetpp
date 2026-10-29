#include "jwt.h"

#include "base64.h"
#include "gsheetpp/error.h"

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#include <memory>

namespace gsheetpp::detail {

namespace {

std::string openssl_error(const char* what) {
    std::string msg = what;
    if (unsigned long e = ERR_get_error()) {
        char buf[256];
        ERR_error_string_n(e, buf, sizeof(buf));
        msg += ": ";
        msg += buf;
    }
    ERR_clear_error();
    return msg;
}

} // namespace

std::string rs256_sign(const std::string& data, const std::string& pem_private_key) {
    std::unique_ptr<BIO, decltype(&BIO_free)> bio(
        BIO_new_mem_buf(pem_private_key.data(), static_cast<int>(pem_private_key.size())),
        &BIO_free);
    if (!bio)
        throw AuthError(openssl_error("Failed to allocate BIO"));

    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(
        PEM_read_bio_PrivateKey(bio.get(), nullptr, nullptr, nullptr), &EVP_PKEY_free);
    if (!key)
        throw AuthError(openssl_error("Failed to parse service account private key"));

    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(),
                                                                &EVP_MD_CTX_free);
    if (!ctx)
        throw AuthError(openssl_error("Failed to allocate digest context"));

    if (EVP_DigestSignInit(ctx.get(), nullptr, EVP_sha256(), nullptr, key.get()) != 1)
        throw AuthError(openssl_error("EVP_DigestSignInit failed"));

    size_t sig_len = 0;
    auto in = reinterpret_cast<const unsigned char*>(data.data());
    if (EVP_DigestSign(ctx.get(), nullptr, &sig_len, in, data.size()) != 1)
        throw AuthError(openssl_error("EVP_DigestSign (size) failed"));

    std::string sig(sig_len, '\0');
    if (EVP_DigestSign(ctx.get(), reinterpret_cast<unsigned char*>(sig.data()), &sig_len, in,
                       data.size()) != 1)
        throw AuthError(openssl_error("EVP_DigestSign failed"));
    sig.resize(sig_len);
    return sig;
}

std::string make_rs256_jwt(const std::string& header_json, const std::string& claims_json,
                           const std::string& pem_private_key) {
    std::string signing_input = base64url_encode(header_json) + "." + base64url_encode(claims_json);
    return signing_input + "." + base64url_encode(rs256_sign(signing_input, pem_private_key));
}

} // namespace gsheetpp::detail
