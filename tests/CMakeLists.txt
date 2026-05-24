cmake_minimum_required(VERSION 3.18)
project(script_json_tests CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The source file under test
add_executable(test_runner
    test_runner.cpp
    test_fix_name.cpp
    test_escape_json.cpp
    test_type_signature.cpp
    test_json_output.cpp
    test_extract_strings.cpp
    test_metadata.cpp
    test_il2cpp_h.cpp
    ../module/src/main/cpp/script_json.cpp
    ../module/src/main/cpp/il2cpp_h.cpp
    stubs/stub_globals.cpp
)

# Stub headers take priority (for il2cpp-api-functions.h, log.h replacement)
target_include_directories(test_runner BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
)

# Real source headers (for il2cpp-class.h, il2cpp-tabledefs.h that have no Android deps)
target_include_directories(test_runner AFTER PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../module/src/main/cpp
)

# Define SCRIPT_JSON_TEST so script_json.cpp skips Android log.h
target_compile_definitions(test_runner PRIVATE SCRIPT_JSON_TEST)

# Link pthread
find_package(Threads REQUIRED)
target_link_libraries(test_runner Threads::Threads)

# Enable warnings
target_compile_options(test_runner PRIVATE -Wall -Wextra -Wno-unused-parameter)
