//
// test_extract_strings.cpp — Unit tests for extract_strings() from global-metadata.dat
//

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "script_json.h"

// Helper: read entire binary file into vector
static std::vector<uint8_t> read_binary_file(const std::string &path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) return {};
    auto size = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> buf(size);
    f.read(reinterpret_cast<char*>(buf.data()), size);
    return buf;
}

int test_extract_strings() {
    int fail = 0;

    const char *metadata_path = "/mnt/d/tmp/wxlhs6/global-metadata-decrypted-5.dat";

    // --- Test 1: Null pointer ---
    {
        auto result = extract_strings(nullptr, 1000);
        if (!result.empty()) {
            printf("  FAIL: null pointer should return empty\n"); fail++;
        } else { printf("  OK: null pointer returns empty\n"); }
    }

    // --- Test 2: Too small buffer ---
    {
        uint8_t tiny[10] = {0xAF, 0x1B, 0xB1, 0xFA, 0x1F, 0, 0, 0, 0, 0};
        auto result = extract_strings(tiny, 10);
        if (!result.empty()) {
            printf("  FAIL: tiny buffer should return empty\n"); fail++;
        } else { printf("  OK: tiny buffer returns empty\n"); }
    }

    // --- Test 3: Bad magic ---
    {
        uint8_t bad_magic[64] = {};
        bad_magic[0] = 0x00; bad_magic[1] = 0x00; bad_magic[2] = 0x00; bad_magic[3] = 0x00;
        auto result = extract_strings(bad_magic, 64);
        if (!result.empty()) {
            printf("  FAIL: bad magic should return empty\n"); fail++;
        } else { printf("  OK: bad magic returns empty\n"); }
    }

    // --- Test 4: Real metadata file ---
    {
        auto data = read_binary_file(metadata_path);
        if (data.empty()) {
            printf("  SKIP: metadata file not found at %s\n", metadata_path);
            printf("  extract_strings: 0 failures (skipped)\n");
            return fail;
        }

        printf("  Loaded metadata: %zu bytes\n", data.size());

        auto result = extract_strings(data.data(), data.size());

        if (result.empty()) {
            printf("  FAIL: extract_strings returned empty from valid metadata\n"); fail++;
        } else {
            printf("  OK: extracted %zu string literals\n", result.size());

            // Verify we got a reasonable count
            // This metadata has 20426 string literals
            if (result.size() < 1000) {
                printf("  FAIL: expected at least 1000 strings, got %zu\n", result.size()); fail++;
            } else { printf("  OK: string count is reasonable (%zu)\n", result.size()); }

            // Check that some strings have readable content
            size_t readable = 0;
            size_t empty_count = 0;
            for (const auto &s : result) {
                if (s.value.empty()) {
                    empty_count++;
                } else {
                    size_t printable = 0;
                    for (char c : s.value) {
                        if (c >= 32 && c < 127) printable++;
                    }
                    if (printable > s.value.length() * 0.5 && s.value.length() >= 2) {
                        readable++;
                    }
                }
            }

            printf("  String stats: %zu total, %zu empty, %zu readable\n",
                   result.size(), empty_count, readable);

            if (readable < 100) {
                printf("  FAIL: expected at least 100 readable strings, got %zu\n", readable); fail++;
            } else { printf("  OK: %zu readable strings found\n", readable); }

            // Check that Address field is set (non-zero for non-empty strings)
            bool has_nonzero_address = false;
            for (const auto &s : result) {
                if (s.address != 0) {
                    has_nonzero_address = true;
                    break;
                }
            }
            if (!has_nonzero_address && result.size() > 1) {
                printf("  FAIL: all addresses are zero\n"); fail++;
            } else { printf("  OK: non-zero addresses present\n"); }

            // Print a few sample strings
            printf("  Sample strings:\n");
            int shown = 0;
            for (const auto &s : result) {
                if (s.value.length() >= 3 && s.value.length() <= 80) {
                    size_t printable = 0;
                    for (char c : s.value) {
                        if (c >= 32 && c < 127) printable++;
                    }
                    if (printable > s.value.length() * 0.7) {
                        printf("    addr=%zu, value=%s\n", (size_t)s.address, s.value.c_str());
                        if (++shown >= 5) break;
                    }
                }
            }
        }
    }

    // --- Test 5: Hand-crafted mini metadata ---
    {
        // Create a minimal valid metadata header with one string literal
        // Header: magic(4) + version(4) + stringLiteral(8) + stringLiteralData(8) = 24 bytes minimum
        // We need enough header space. The actual header is larger but we'll pad with zeros.
        // The header parsing only reads the first 24 bytes for what we need.
        size_t header_size = 256;  // enough padding
        size_t data_section_offset = header_size;
        size_t entry_count = 2;

        // String literal entries: each 8 bytes {uint32 length, int32 dataIndex}
        size_t entries_size = entry_count * 8;
        // String data: "Hi" (2 bytes) + "World" (5 bytes)
        const char *str1 = "Hi";
        const char *str2 = "World";
        size_t str_data_size = 2 + 5;
        size_t str_data_offset = data_section_offset + entries_size;
        size_t total_size = str_data_offset + str_data_size;

        std::vector<uint8_t> mini(total_size, 0);

        // Write header
        uint32_t magic = 0xFAB11BAF;
        int32_t version = 29;
        memcpy(mini.data(), &magic, 4);
        memcpy(mini.data() + 4, &version, 4);
        uint32_t str_lit_off = (uint32_t)data_section_offset;
        int32_t str_lit_size = (int32_t)entries_size;
        uint32_t str_data_off = (uint32_t)str_data_offset;
        int32_t str_data_sz = (int32_t)str_data_size;
        memcpy(mini.data() + 8, &str_lit_off, 4);
        memcpy(mini.data() + 12, &str_lit_size, 4);
        memcpy(mini.data() + 16, &str_data_off, 4);
        memcpy(mini.data() + 20, &str_data_sz, 4);

        // Write string literal entries
        uint32_t len1 = 2; int32_t idx1 = 0;
        uint32_t len2 = 5; int32_t idx2 = 2;
        memcpy(mini.data() + data_section_offset, &len1, 4);
        memcpy(mini.data() + data_section_offset + 4, &idx1, 4);
        memcpy(mini.data() + data_section_offset + 8, &len2, 4);
        memcpy(mini.data() + data_section_offset + 12, &idx2, 4);

        // Write string data
        memcpy(mini.data() + str_data_offset, str1, 2);
        memcpy(mini.data() + str_data_offset + 2, str2, 5);

        auto result = extract_strings(mini.data(), mini.size());

        if (result.size() != 2) {
            printf("  FAIL: mini metadata: expected 2 strings, got %zu\n", result.size()); fail++;
        } else {
            printf("  OK: mini metadata extracted %zu strings\n", result.size());
            if (result[0].value != "Hi") {
                printf("  FAIL: first string = '%s', expected 'Hi'\n", result[0].value.c_str()); fail++;
            } else { printf("  OK: first string = 'Hi'\n"); }
            if (result[1].value != "World") {
                printf("  FAIL: second string = '%s', expected 'World'\n", result[1].value.c_str()); fail++;
            } else { printf("  OK: second string = 'World'\n"); }
        }
    }

    printf("  extract_strings: %d failures\n", fail);
    return fail;
}
