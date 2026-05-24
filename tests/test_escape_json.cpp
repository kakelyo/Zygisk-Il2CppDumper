//
// test_escape_json.cpp — Unit tests for escape_json()
//

#include <cassert>
#include <cstdio>
#include <string>

#include "script_json.h"

int test_escape_json() {
    int fail = 0;

    // Normal strings pass through unchanged
    if (escape_json("hello world") != "hello world") {
        printf("  FAIL: normal string\n");
        fail++;
    } else { printf("  OK: normal string unchanged\n"); }

    // Double quote
    if (escape_json("say \"hi\"") != "say \\\"hi\\\"") {
        printf("  FAIL: escape_json(\"say \\\"hi\\\"\") = \"%s\"\n", escape_json("say \"hi\"").c_str());
        fail++;
    } else { printf("  OK: double quotes escaped\n"); }

    // Backslash
    if (escape_json("path\\to\\file") != "path\\\\to\\\\file") {
        printf("  FAIL: backslash\n");
        fail++;
    } else { printf("  OK: backslashes escaped\n"); }

    // Newline
    if (escape_json("line1\nline2") != "line1\\nline2") {
        printf("  FAIL: newline\n");
        fail++;
    } else { printf("  OK: newline escaped\n"); }

    // Carriage return
    if (escape_json("cr\r") != "cr\\r") {
        printf("  FAIL: carriage return\n");
        fail++;
    } else { printf("  OK: carriage return escaped\n"); }

    // Tab
    if (escape_json("col1\tcol2") != "col1\\tcol2") {
        printf("  FAIL: tab\n");
        fail++;
    } else { printf("  OK: tab escaped\n"); }

    // Backspace
    if (escape_json("bs\b") != "bs\\b") {
        printf("  FAIL: backspace\n");
        fail++;
    } else { printf("  OK: backspace escaped\n"); }

    // Form feed
    if (escape_json("ff\f") != "ff\\f") {
        printf("  FAIL: form feed\n");
        fail++;
    } else { printf("  OK: form feed escaped\n"); }

    // Control character (< 0x20) → \uXXXX
    std::string input_ctrl(1, 0x01);  // SOH
    auto escaped_ctrl = escape_json(input_ctrl);
    if (escaped_ctrl != "\\u0001") {
        printf("  FAIL: control char, got \"%s\"\n", escaped_ctrl.c_str());
        fail++;
    } else { printf("  OK: control char → \\uXXXX\n"); }

    // Combined
    if (escape_json("a\"b\\c\nd") != "a\\\"b\\\\c\\nd") {
        printf("  FAIL: combined escapes, got \"%s\"\n", escape_json("a\"b\\c\nd").c_str());
        fail++;
    } else { printf("  OK: combined escapes\n"); }

    // Empty string
    if (escape_json("") != "") {
        printf("  FAIL: empty string\n");
        fail++;
    } else { printf("  OK: empty string unchanged\n"); }

    // C++ method signature with common chars
    if (escape_json("void Player_TakeDamage (Player_o* __this, int32_t damage, const MethodInfo* method);")
        != "void Player_TakeDamage (Player_o* __this, int32_t damage, const MethodInfo* method);") {
        printf("  FAIL: realistic signature\n");
        fail++;
    } else { printf("  OK: realistic signature unchanged\n"); }

    printf("  escape_json: %d failures\n", fail);
    return fail;
}
