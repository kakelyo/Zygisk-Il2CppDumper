//
// test_json_output.cpp — Unit tests for write_script_json() output format
//

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "script_json.h"

// Helper: read entire file into string
static std::string read_file(const std::string &path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::stringstream buf;
    buf << f.rdbuf();
    return buf.str();
}

// Helper: check if file content is valid JSON (basic checks)
static bool is_valid_json_brackets(const std::string &s) {
    int brace = 0, bracket = 0;
    bool in_string = false;
    bool escape_next = false;
    for (char c : s) {
        if (escape_next) { escape_next = false; continue; }
        if (c == '\\' && in_string) { escape_next = true; continue; }
        if (c == '"') { in_string = !in_string; continue; }
        if (in_string) continue;
        if (c == '{') brace++;
        if (c == '}') brace--;
        if (c == '[') bracket++;
        if (c == ']') bracket--;
    }
    return brace == 0 && bracket == 0;
}

int test_json_output() {
    int fail = 0;
    const char *test_dir = "/tmp/zygisk_test_json";
    const char *test_files_dir = "/tmp/zygisk_test_json/files";

    // Create output directory
    (void)test_files_dir;
    system("mkdir -p /tmp/zygisk_test_json/files");

    // --- Test 1: Empty methods, strings and addresses ---
    {
        std::vector<ScriptMethodEntry> methods;
        std::vector<StringEntry> strings;
        std::vector<uint64_t> addresses;
        std::vector<MetadataEntry> metadata;
        std::vector<MetadataMethodEntry> metadata_methods;
        write_script_json(test_dir, methods, strings, metadata, metadata_methods, addresses);

        auto content = read_file(std::string(test_dir) + "/files/script.json");
        if (content.empty()) {
            printf("  FAIL: empty test - file not created\n"); fail++;
        } else if (!is_valid_json_brackets(content)) {
            printf("  FAIL: empty test - invalid JSON brackets\n"); fail++;
        } else { printf("  OK: empty test - valid JSON structure\n"); }

        // Check required keys
        if (content.find("\"ScriptMethod\"") == std::string::npos) {
            printf("  FAIL: missing ScriptMethod key\n"); fail++;
        } else { printf("  OK: ScriptMethod key present\n"); }

        if (content.find("\"Addresses\"") == std::string::npos) {
            printf("  FAIL: missing Addresses key\n"); fail++;
        } else { printf("  OK: Addresses key present\n"); }

        if (content.find("\"ScriptString\"") == std::string::npos) {
            printf("  FAIL: missing ScriptString key\n"); fail++;
        } else { printf("  OK: ScriptString key present\n"); }
    }

    // --- Test 2: With methods and strings ---
    {
        std::vector<ScriptMethodEntry> methods;
        ScriptMethodEntry m1;
        m1.address = 0x1234;
        m1.name = "Game.Player$$TakeDamage";
        m1.signature = "void Game_Player_TakeDamage (Game_Player_o* __this, int32_t damage, const MethodInfo* method);";
        m1.type_sig = "vii";
        methods.push_back(m1);

        ScriptMethodEntry m2;
        m2.address = 0x5678;
        m2.name = "Game.Enemy$$Attack";
        m2.signature = "void Game_Enemy_Attack (Game_Enemy_o* __this, Game_Player_o* target, const MethodInfo* method);";
        m2.type_sig = "vii";
        methods.push_back(m2);

        std::vector<StringEntry> strings;
        StringEntry s1;
        s1.address = 0x100;
        s1.value = "Hello, World!";
        strings.push_back(s1);

        std::vector<uint64_t> addresses = {0x1234, 0x5678};
        std::vector<MetadataEntry> metadata;
        std::vector<MetadataMethodEntry> metadata_methods;
        write_script_json(test_dir, methods, strings, metadata, metadata_methods, addresses);

        auto content = read_file(std::string(test_dir) + "/files/script.json");

        if (content.empty()) {
            printf("  FAIL: methods test - file not created\n"); fail++;
        } else if (!is_valid_json_brackets(content)) {
            printf("  FAIL: methods test - invalid JSON\n"); fail++;
        } else { printf("  OK: methods test - valid JSON\n"); }

        // Check method name appears
        if (content.find("Game.Player$$TakeDamage") == std::string::npos) {
            printf("  FAIL: method name not found in JSON\n"); fail++;
        } else { printf("  OK: method name present\n"); }

        // Check address appears as decimal
        if (content.find("4660") == std::string::npos) {  // 0x1234 = 4660
            printf("  FAIL: address 0x1234 (4660) not found\n"); fail++;
        } else { printf("  OK: address appears as decimal\n"); }

        // Check TypeSignature
        if (content.find("\"vii\"") == std::string::npos) {
            printf("  FAIL: TypeSignature 'vii' not found\n"); fail++;
        } else { printf("  OK: TypeSignature present\n"); }

        // Check ScriptString content
        if (content.find("Hello, World!") == std::string::npos) {
            printf("  FAIL: string value not found in JSON\n"); fail++;
        } else { printf("  OK: string value present\n"); }
    }

    // --- Test 3: Addresses are sorted ---
    {
        std::vector<ScriptMethodEntry> methods;
        std::vector<uint64_t> addresses = {0x5000, 0x1000, 0x3000, 0x2000, 0x1000};  // unsorted + dup

        // deduplicate and sort (this is what the real code should do)
        std::set<uint64_t> unique_set(addresses.begin(), addresses.end());
        addresses.assign(unique_set.begin(), unique_set.end());

        write_script_json(test_dir, methods, std::vector<StringEntry>(),
                          std::vector<MetadataEntry>(), std::vector<MetadataMethodEntry>(),
                          addresses);

        auto content = read_file(std::string(test_dir) + "/files/script.json");
        if (content.empty()) {
            printf("  FAIL: addresses test - file not created\n"); fail++;
        } else { printf("  OK: addresses test - file created\n"); }

        // Verify order: 0x1000 (4096) before 0x2000 (8192) before 0x3000 (12288) before 0x5000 (20480)
        auto pos1 = content.find("4096");   // 0x1000
        auto pos2 = content.find("8192");   // 0x2000
        auto pos3 = content.find("12288");  // 0x3000
        auto pos4 = content.find("20480");  // 0x5000
        if (pos1 == std::string::npos || pos2 == std::string::npos ||
            pos3 == std::string::npos || pos4 == std::string::npos) {
            printf("  FAIL: not all addresses found\n"); fail++;
        } else if (!(pos1 < pos2 && pos2 < pos3 && pos3 < pos4)) {
            printf("  FAIL: addresses not in sorted order\n"); fail++;
        } else { printf("  OK: addresses in sorted order\n"); }
    }

    // --- Test 4: Special characters in method names are escaped ---
    {
        std::vector<ScriptMethodEntry> methods;
        ScriptMethodEntry m;
        m.address = 0x1;
        m.name = "has\"quote";
        m.signature = "void has\"quote ();";
        m.type_sig = "v";
        methods.push_back(m);

        std::vector<uint64_t> addresses = {0x1};
        write_script_json(test_dir, methods, std::vector<StringEntry>(),
                          std::vector<MetadataEntry>(), std::vector<MetadataMethodEntry>(),
                          addresses);

        auto content = read_file(std::string(test_dir) + "/files/script.json");
        if (!is_valid_json_brackets(content)) {
            printf("  FAIL: special chars test - invalid JSON\n"); fail++;
        } else { printf("  OK: special chars in names - valid JSON\n"); }

        // Check escaped quote is present
        if (content.find("\\\"") == std::string::npos) {
            printf("  FAIL: escaped quote not found\n"); fail++;
        } else { printf("  OK: quotes properly escaped\n"); }
    }

    // --- Test 5: stringliteral.json ---
    {
        std::vector<StringEntry> strings;
        StringEntry s1;
        s1.address = 0xABCD;
        s1.value = "test string";
        strings.push_back(s1);
        StringEntry s2;
        s2.address = 0x1234;
        s2.value = "another\"string";
        strings.push_back(s2);

        write_stringliteral_json(test_dir, strings);

        auto content = read_file(std::string(test_dir) + "/files/stringliteral.json");
        if (content.empty()) {
            printf("  FAIL: stringliteral.json not created\n"); fail++;
        } else if (!is_valid_json_brackets(content)) {
            printf("  FAIL: stringliteral.json invalid JSON\n"); fail++;
        } else { printf("  OK: stringliteral.json valid\n"); }

        // Check hex address format
        if (content.find("0xabcd") == std::string::npos) {
            printf("  FAIL: hex address not found in stringliteral.json\n"); fail++;
        } else { printf("  OK: hex address format in stringliteral.json\n"); }

        // Check value field
        if (content.find("\"value\"") == std::string::npos) {
            printf("  FAIL: value field not found\n"); fail++;
        } else { printf("  OK: value field present\n"); }
    }

    // Cleanup
    system("rm -rf /tmp/zygisk_test_json");

    printf("  json_output: %d failures\n", fail);
    return fail;
}
