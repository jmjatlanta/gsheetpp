#include "http_client.h"

#include "gsheetpp/error.h"

#include <curl/curl.h>

#include <memory>

namespace gsheetpp::detail {

namespace {

// curl_global_init is not thread-safe; do it once for the process.
struct CurlGlobal {
    CurlGlobal() { curl_global_init(CURL_GLOBAL_DEFAULT); }
    ~CurlGlobal() { curl_global_cleanup(); }
};

void ensure_curl_global() {
    static CurlGlobal instance;
}

size_t write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    static_cast<std::string*>(userdata)->append(ptr, size * nmemb);
    return size * nmemb;
}

} // namespace

HttpClient::HttpClient() {
    ensure_curl_global();
    curl_ = curl_easy_init();
    if (!curl_)
        throw HttpError("curl_easy_init failed", 0, {});
}

HttpClient::~HttpClient() {
    curl_easy_cleanup(static_cast<CURL*>(curl_));
}

HttpResponse HttpClient::request(const std::string& method, const std::string& url,
                                 const std::vector<std::string>& headers,
                                 const std::string& body) {
    CURL* curl = static_cast<CURL*>(curl_);
    curl_easy_reset(curl);

    std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> header_list(nullptr,
                                                                           &curl_slist_free_all);
    for (const auto& h : headers) {
        curl_slist* next = curl_slist_append(header_list.get(), h.c_str());
        if (!next)
            throw HttpError("curl_slist_append failed", 0, {});
        header_list.release();
        header_list.reset(next);
    }

    HttpResponse response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list.get());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");  // allow gzip etc.

    if (method == "GET") {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    } else {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(body.size()));
    }

    CURLcode rc = curl_easy_perform(curl);
    if (rc != CURLE_OK)
        throw HttpError(std::string(method) + " " + url + " failed: " + curl_easy_strerror(rc), 0,
                        {});

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status);
    return response;
}

std::string HttpClient::escape(const std::string& s) {
    char* escaped =
        curl_easy_escape(static_cast<CURL*>(curl_), s.data(), static_cast<int>(s.size()));
    if (!escaped)
        throw HttpError("curl_easy_escape failed", 0, {});
    std::string out(escaped);
    curl_free(escaped);
    return out;
}

} // namespace gsheetpp::detail
