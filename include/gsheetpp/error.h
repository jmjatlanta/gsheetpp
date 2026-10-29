#pragma once

#include <stdexcept>
#include <string>

namespace gsheetpp {

/// Base class for all errors thrown by gsheetpp.
class Error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/// A problem with credentials: unreadable key file, bad JSON, signing failure,
/// or the token endpoint refusing the request.
class AuthError : public Error {
public:
    using Error::Error;
};

/// A transport failure or a non-2xx response from a Google API.
class HttpError : public Error {
public:
    HttpError(const std::string& msg, long status, std::string body)
        : Error(msg), status_(status), body_(std::move(body)) {}

    /// HTTP status code, or 0 if the request never got a response.
    long status() const noexcept { return status_; }
    /// Raw response body (usually a Google JSON error object).
    const std::string& body() const noexcept { return body_; }

private:
    long status_;
    std::string body_;
};

} // namespace gsheetpp
