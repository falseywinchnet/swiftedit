cmake_minimum_required(VERSION 3.24)
project(SwiftEditPortableCore LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# This gate builds production CSV/formula code without the native SDK. It is
# deliberately not named or packaged as a SwiftEdit application build.
enable_testing()
add_library(swiftedit_csv STATIC ../../src/csv.cpp)
target_include_directories(swiftedit_csv PUBLIC ../../src)
add_executable(swiftedit-csv-tests ../../tests/csv_tests.cpp)
target_link_libraries(swiftedit-csv-tests PRIVATE swiftedit_csv)
add_test(NAME csv COMMAND swiftedit-csv-tests)
