add_executable(gsheetpp_tests test_main.cpp)
target_include_directories(gsheetpp_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(gsheetpp_tests PRIVATE gsheetpp::gsheetpp OpenSSL::Crypto gsheetpp_rapidjson)
add_test(NAME gsheetpp_tests COMMAND gsheetpp_tests)
