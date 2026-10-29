add_executable(gsheetpp_tests test_main.cpp)
target_include_directories(gsheetpp_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(gsheetpp_tests PRIVATE gsheetpp::gsheetpp OpenSSL::Crypto gsheetpp_rapidjson)
add_test(NAME gsheetpp_tests COMMAND gsheetpp_tests)

# --- Google Test (git submodule) ---------------------------------------------

set(GOOGLETEST_DIR "${PROJECT_SOURCE_DIR}/external/googletest")
if(NOT EXISTS "${GOOGLETEST_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "googletest submodule not found. Run: git submodule update --init --recursive")
endif()
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
add_subdirectory("${GOOGLETEST_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/googletest" EXCLUDE_FROM_ALL)

include(GoogleTest)

# Integration tests: these talk to the real Sheets API using the service account
# key in tests/test_resources (git-ignored). They are skipped if the key is absent.
add_executable(gsheetpp_integration_tests sheet_read_test.cpp drive_test.cpp)
target_link_libraries(gsheetpp_integration_tests PRIVATE gsheetpp::gsheetpp GTest::gtest_main)
target_compile_definitions(gsheetpp_integration_tests PRIVATE
    GSHEETPP_TEST_RESOURCES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test_resources")
gtest_discover_tests(gsheetpp_integration_tests PROPERTIES LABELS integration)
