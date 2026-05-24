//
// test_metadata.cpp — Unit tests for Phase 3 metadata extraction
//

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "script_json.h"

// Test: get_header_field_offset is not directly testable (it's static),
// but we can test extract_metadata_from_blob with crafted metadata.

int test_metadata_from_blob() {
    int fail = 0;

    // --- Test 1: Null pointer ---
    {
        std::vector<MetadataEntry> meta;
        std::vector<MetadataMethodEntry> meta_methods;
        extract_metadata_from_blob(nullptr, 1000, 0, meta, meta_methods);
        if (!meta.empty() || !meta_methods.empty()) {
            printf("  FAIL: null pointer should return empty\n"); fail++;
        } else { printf("  OK: null pointer returns empty\n"); }
    }

    // --- Test 2: Bad magic ---
    {
        uint8_t bad[64] = {};
        std::vector<MetadataEntry> meta;
        std::vector<MetadataMethodEntry> meta_methods;
        extract_metadata_from_blob(bad, 64, 0, meta, meta_methods);
        if (!meta.empty() || !meta_methods.empty()) {
            printf("  FAIL: bad magic should return empty\n"); fail++;
        } else { printf("  OK: bad magic returns empty\n"); }
    }

    // --- Test 3: Version out of range ---
    {
        uint8_t buf[64] = {};
        uint32_t magic = 0xFAB11BAF;
        int32_t version = 15;  // too old
        memcpy(buf, &magic, 4);
        memcpy(buf + 4, &version, 4);
        std::vector<MetadataEntry> meta;
        std::vector<MetadataMethodEntry> meta_methods;
        extract_metadata_from_blob(buf, 64, 0, meta, meta_methods);
        if (!meta.empty() || !meta_methods.empty()) {
            printf("  FAIL: version 15 should return empty\n"); fail++;
        } else { printf("  OK: version 15 returns empty\n"); }
    }

    // --- Test 4: Version >= 25 (no metadataUsageLists/Pairs in header) ---
    {
        uint8_t buf[256] = {};
        uint32_t magic = 0xFAB11BAF;
        int32_t version = 29;
        memcpy(buf, &magic, 4);
        memcpy(buf + 4, &version, 4);
        std::vector<MetadataEntry> meta;
        std::vector<MetadataMethodEntry> meta_methods;
        extract_metadata_from_blob(buf, 256, 0, meta, meta_methods);
        if (!meta.empty() || !meta_methods.empty()) {
            printf("  FAIL: version 29 should return empty (no metadataUsage in header)\n"); fail++;
        } else { printf("  OK: version 29 returns empty\n"); }
    }

    // --- Test 5: Valid v24 metadata with metadataUsageLists/Pairs ---
    {
        // Build a minimal metadata blob with metadataUsageLists/Pairs
        // For v24 (treated as v24.0), the header layout includes rgctxEntries
        // metadataUsageListsOffset is at byte 192 for v<=24.1

        size_t header_size = 256;  // enough for the full header
        size_t mul_offset = header_size;
        size_t mup_offset = mul_offset + 16;  // 2 usage lists × 8 bytes

        // Create 1 usage list with 2 pairs
        // Usage list: {start=0, count=2}
        // Pair 0: TypeInfo (usage=1), destinationIndex=0, encodedSourceIndex = (1<<29)|0
        // Pair 1: MethodDef (usage=3), destinationIndex=1, encodedSourceIndex = (3<<29)|5

        uint32_t encoded_typeinfo = (1u << 29) | 0u;   // TypeInfo, index=0
        uint32_t encoded_methoddef = (3u << 29) | 5u;   // MethodDef, index=5

        size_t total_size = mup_offset + 16;  // 2 pairs × 8 bytes
        std::vector<uint8_t> buf(total_size, 0);

        // Write header
        uint32_t magic = 0xFAB11BAF;
        int32_t version = 24;
        memcpy(buf.data(), &magic, 4);
        memcpy(buf.data() + 4, &version, 4);

        // Write metadataUsageListsOffset at byte 192 (for v24.0 with rgctxEntries)
        uint32_t mul_off_val = (uint32_t)mul_offset;
        int32_t mul_count_val = 1;
        memcpy(buf.data() + 192, &mul_off_val, 4);
        memcpy(buf.data() + 196, &mul_count_val, 4);

        // Write metadataUsagePairsOffset at byte 200
        uint32_t mup_off_val = (uint32_t)mup_offset;
        int32_t mup_count_val = 2;
        memcpy(buf.data() + 200, &mup_off_val, 4);
        memcpy(buf.data() + 204, &mup_count_val, 4);

        // Write usage list: {start=0, count=2}
        uint32_t list_start = 0, list_count = 2;
        memcpy(buf.data() + mul_offset, &list_start, 4);
        memcpy(buf.data() + mul_offset + 4, &list_count, 4);

        // Write usage pairs
        uint32_t dest0 = 0, src0 = encoded_typeinfo;
        uint32_t dest1 = 1, src1 = encoded_methoddef;
        memcpy(buf.data() + mup_offset, &dest0, 4);
        memcpy(buf.data() + mup_offset + 4, &src0, 4);
        memcpy(buf.data() + mup_offset + 8, &dest1, 4);
        memcpy(buf.data() + mup_offset + 12, &src1, 4);

        std::vector<MetadataEntry> meta;
        std::vector<MetadataMethodEntry> meta_methods;
        extract_metadata_from_blob(buf.data(), buf.size(), 0x1000, meta, meta_methods);

        // Should have 1 TypeInfo entry + 0 Il2CppType + 0 FieldInfo = 1 ScriptMetadata
        // And 1 MethodDef + 0 MethodRef = 1 ScriptMetadataMethod
        if (meta.size() != 1) {
            printf("  FAIL: expected 1 ScriptMetadata, got %zu\n", meta.size()); fail++;
        } else {
            printf("  OK: got %zu ScriptMetadata entries\n", meta.size());
            if (meta[0].name.find("TypeInfo") == std::string::npos) {
                printf("  FAIL: TypeInfo entry name should contain 'TypeInfo', got '%s'\n",
                       meta[0].name.c_str()); fail++;
            } else { printf("  OK: TypeInfo entry name = '%s'\n", meta[0].name.c_str()); }
        }

        if (meta_methods.size() != 1) {
            printf("  FAIL: expected 1 ScriptMetadataMethod, got %zu\n", meta_methods.size()); fail++;
        } else {
            printf("  OK: got %zu ScriptMetadataMethod entries\n", meta_methods.size());
            if (meta_methods[0].name.find("MethodDef") == std::string::npos) {
                printf("  FAIL: MethodDef entry name should contain 'MethodDef', got '%s'\n",
                       meta_methods[0].name.c_str()); fail++;
            } else { printf("  OK: MethodDef entry name = '%s'\n", meta_methods[0].name.c_str()); }
        }
    }

    printf("  test_metadata_from_blob: %d failures\n", fail);
    return fail;
}

int test_metadata_json_output() {
    int fail = 0;

    // --- Test: ScriptMetadata and ScriptMetadataMethod JSON output ---
    {
        std::vector<ScriptMethodEntry> methods;
        std::vector<StringEntry> strings;
        std::vector<uint64_t> addresses = {0x1000, 0x2000};

        std::vector<MetadataEntry> metadata;
        MetadataEntry me;
        me.address = 0x5000;
        me.name = "System.String_TypeInfo";
        me.signature = "System_String_c*";
        metadata.push_back(me);

        std::vector<MetadataMethodEntry> metadata_methods;
        MetadataMethodEntry mme;
        mme.address = 0x6000;
        mme.name = "Method$System.String.Concat()";
        mme.method_address = 0x3000;
        metadata_methods.push_back(mme);

        // Write to a temp file
        const char *tmp_dir = "/tmp/test_script_json_phase3";
        system("mkdir -p /tmp/test_script_json_phase3/files");
        write_script_json(tmp_dir, methods, strings, metadata, metadata_methods, addresses);

        // Read back and verify
        auto path = std::string(tmp_dir) + "/files/script.json";
        FILE *f = fopen(path.c_str(), "r");
        if (!f) {
            printf("  FAIL: could not open %s\n", path.c_str()); fail++;
        } else {
            fseek(f, 0, SEEK_END);
            auto sz = ftell(f);
            fseek(f, 0, SEEK_SET);
            std::string content(sz, '\0');
            fread(content.data(), 1, sz, f);
            fclose(f);

            // Check ScriptMetadata section exists
            if (content.find("\"ScriptMetadata\"") == std::string::npos) {
                printf("  FAIL: ScriptMetadata section not found\n"); fail++;
            } else { printf("  OK: ScriptMetadata section found\n"); }

            // Check ScriptMetadataMethod section exists
            if (content.find("\"ScriptMetadataMethod\"") == std::string::npos) {
                printf("  FAIL: ScriptMetadataMethod section not found\n"); fail++;
            } else { printf("  OK: ScriptMetadataMethod section found\n"); }

            // Check TypeInfo entry
            if (content.find("System.String_TypeInfo") == std::string::npos) {
                printf("  FAIL: TypeInfo name not found in output\n"); fail++;
            } else { printf("  OK: TypeInfo name found\n"); }

            // Check MethodDef entry
            if (content.find("Method$System.String.Concat()") == std::string::npos) {
                printf("  FAIL: MethodDef name not found in output\n"); fail++;
            } else { printf("  OK: MethodDef name found\n"); }

            // Check MethodAddress field
            if (content.find("\"MethodAddress\"") == std::string::npos) {
                printf("  FAIL: MethodAddress field not found\n"); fail++;
            } else { printf("  OK: MethodAddress field found\n"); }
        }
    }

    printf("  test_metadata_json_output: %d failures\n", fail);
    return fail;
}
