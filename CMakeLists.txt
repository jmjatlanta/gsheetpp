cmake_minimum_required(VERSION 3.16)
project(gsheetpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(GSHEETPP_BUILD_EXAMPLES "Build gsheetpp examples" ON)
option(GSHEETPP_BUILD_TESTS "Build gsheetpp tests" ON)

# --- Dependencies -----------------------------------------------------------

# libcurl: expected to be installed on the system
find_package(CURL REQUIRED)

# OpenSSL: needed to sign the RS256 JWT used by service-account auth
if(APPLE AND NOT OPENSSL_ROOT_DIR)
    execute_process(COMMAND brew --prefix openssl@3
        OUTPUT_VARIABLE _brew_openssl OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(_brew_openssl)
        set(OPENSSL_ROOT_DIR "${_brew_openssl}")
    endif()
endif()
find_package(OpenSSL REQUIRED)

# rapidjson: header-only, pulled in as a git submodule. Only the headers are
# used; rapidjson's own CMake project (tests, docs, examples) is not built.
set(RAPIDJSON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/rapidjson")
if(NOT EXISTS "${RAPIDJSON_DIR}/include/rapidjson/document.h")
    message(FATAL_ERROR
        "rapidjson submodule not found. Run: git submodule update --init --recursive")
endif()
add_library(gsheetpp_rapidjson INTERFACE)
target_include_directories(gsheetpp_rapidjson SYSTEM INTERFACE "${RAPIDJSON_DIR}/include")

# --- Library ----------------------------------------------------------------

add_library(gsheetpp
    src/api_client.cpp
    src/base64.cpp
    src/drive_client.cpp
    src/http_client.cpp
    src/jwt.cpp
    src/service_account_credentials.cpp
    src/sheets_client.cpp
)
add_library(gsheetpp::gsheetpp ALIAS gsheetpp)

target_include_directories(gsheetpp
    PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(gsheetpp
    PRIVATE CURL::libcurl OpenSSL::Crypto gsheetpp_rapidjson
)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(gsheetpp PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- Examples / tests -------------------------------------------------------

if(GSHEETPP_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(GSHEETPP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
