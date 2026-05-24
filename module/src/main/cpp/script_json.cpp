//
// script_json.cpp — Generate Il2CppDumper-compatible script.json output
//
// Phase 1: ScriptMethod + Addresses
// Phase 2: ScriptString + stringliteral.json
// Phase 3: ScriptMetadata + ScriptMetadataMethod
//

#include "script_json.h"

// Logging: use Android logcat in production, printf in test mode.
// When SCRIPT_JSON_TEST is defined (via CMake for unit tests),
// log.h is not included and LOGI/LOGE are defined as no-ops.
#ifndef SCRIPT_JSON_TEST
#include "log.h"
#else
#include <cstdio>
#define LOGI(...) do { printf("[INFO ] "); printf(__VA_ARGS__); printf("\n"); } while(0)
#define LOGE(...) do { fprintf(stderr, "[ERROR] "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } while(0)
#endif

#include "il2cpp-class.h"
#include "il2cpp-tabledefs.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>
#include <map>
#include <string>
#include <vector>

// ----------------------------------------------------------------
// il2cpp API function pointers (resolved at runtime in il2cpp_dump.cpp)
// Declared as extern — defined in il2cpp_dump.cpp (real) or stubs (test).
// ----------------------------------------------------------------

extern const char* (*il2cpp_class_get_name)(Il2CppClass * klass);
extern const char* (*il2cpp_class_get_namespace)(Il2CppClass * klass);
extern Il2CppClass* (*il2cpp_class_from_type)(const Il2CppType * type);
extern Il2CppClass* (*il2cpp_class_get_parent)(Il2CppClass * klass);
extern const Il2CppType* (*il2cpp_class_get_type)(Il2CppClass * klass);
extern Il2CppClass* (*il2cpp_class_get_element_class)(Il2CppClass * klass);
extern bool (*il2cpp_class_is_valuetype)(const Il2CppClass * klass);
extern bool (*il2cpp_class_is_enum)(const Il2CppClass * klass);
extern const Il2CppType* (*il2cpp_class_enum_basetype)(Il2CppClass * klass);
extern const char* (*il2cpp_method_get_name)(const MethodInfo * method);
extern const Il2CppType* (*il2cpp_method_get_return_type)(const MethodInfo * method);
extern uint32_t (*il2cpp_method_get_param_count)(const MethodInfo * method);
extern const Il2CppType* (*il2cpp_method_get_param)(const MethodInfo * method, uint32_t index);
extern const char* (*il2cpp_method_get_param_name)(const MethodInfo * method, uint32_t index);
extern uint32_t (*il2cpp_method_get_flags)(const MethodInfo * method, uint32_t * iflags);
extern bool (*il2cpp_type_is_byref)(const Il2CppType * type);

// Additional API pointers needed for Phase 3 (metadata extraction)
extern FieldInfo* (*il2cpp_class_get_fields)(Il2CppClass * klass, void* *iter);
extern const char* (*il2cpp_field_get_name)(FieldInfo * field);
extern const Il2CppType* (*il2cpp_field_get_type)(FieldInfo * field);
extern Il2CppDomain* (*il2cpp_domain_get)();
extern const Il2CppAssembly** (*il2cpp_domain_get_assemblies)(const Il2CppDomain * domain, size_t * size);
extern const Il2CppImage* (*il2cpp_assembly_get_image)(const Il2CppAssembly * assembly);
extern const char* (*il2cpp_image_get_name)(const Il2CppImage * image);
extern size_t (*il2cpp_image_get_class_count)(const Il2CppImage * image);
extern const Il2CppClass* (*il2cpp_image_get_class)(const Il2CppImage * image, size_t index);
extern const MethodInfo* (*il2cpp_class_get_methods)(Il2CppClass * klass, void* *iter);

// External: il2cpp_base is set in il2cpp_dump.cpp via dladdr()
extern uint64_t il2cpp_base;

// ================================================================
// Pure functions — no il2cpp runtime dependency
// ================================================================

std::string fix_name(const std::string &name) {
    // C++ keywords that would conflict with identifiers
    static const std::set<std::string> keywords = {
        "auto", "break", "case", "char", "class", "const", "continue",
        "default", "do", "double", "else", "enum", "extern", "float",
        "for", "friend", "goto", "if", "int", "long",
        "mutable", "namespace", "new", "operator", "private", "protected",
        "public", "register", "return", "short", "signed", "sizeof",
        "static", "struct", "switch", "template", "this", "throw",
        "typedef", "typename", "union", "unsigned", "using", "virtual",
        "void", "volatile", "while",
        // C99/C11
        "restrict", "_Bool", "_Complex", "_Imaginary",
        // C++11+ context-sensitive keywords
        "override", "final", "nullptr", "constexpr", "noexcept",
        // Common C runtime identifiers
        "stdin", "stdout", "stderr", "errno",
        // il2cpp internal field names
        "klass", "monitor",
    };

    static const std::set<std::string> prefix_keywords = {
        "inline", "near", "far",
    };

    if (name.empty()) return "_";

    std::string result;
    result.reserve(name.size());
    for (char c : name) {
        result += (isalnum(static_cast<unsigned char>(c)) || c == '_') ? c : '_';
    }

    // Prefix digits with underscore
    if (!result.empty() && isdigit(static_cast<unsigned char>(result[0]))) {
        result = "_" + result;
    }

    // Prefix C++ keywords with underscore
    if (keywords.count(result)) {
        result = "_" + result;
    }

    // Special handling for prefix_keywords
    if (prefix_keywords.count(result)) {
        result = "_" + result + "_";
    }

    // Ensure not empty after transformations
    if (result.empty()) result = "_";

    return result;
}

std::string escape_json(const std::string &s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                    out += buf;
                } else {
                    out += c;
                }
                break;
        }
    }
    return out;
}

char type_to_signature_char(const Il2CppType *type) {
    if (!type) return 'i';  // fallback

    switch (type->type) {
        case IL2CPP_TYPE_VOID:    return 'v';
        case IL2CPP_TYPE_R4:      return 'f';
        case IL2CPP_TYPE_R8:      return 'd';
        case IL2CPP_TYPE_I8:
        case IL2CPP_TYPE_U8:      return 'j';
        default:                  return 'i';  // all integer/bool/ptr/ref types
    }
}

std::string build_type_signature(const MethodInfo *method) {
    if (!method) return "";

    std::string sig;

    // Return type
    auto ret = il2cpp_method_get_return_type(method);
    if (ret) {
        sig += type_to_signature_char(ret);
    }

    // Parameter types
    auto count = il2cpp_method_get_param_count(method);
    for (uint32_t i = 0; i < count; i++) {
        auto param = il2cpp_method_get_param(method, i);
        if (param) {
            sig += type_to_signature_char(param);
        }
    }

    return sig;
}

// ================================================================
// Functions requiring il2cpp runtime
// ================================================================

std::string parse_type(const Il2CppType *type) {
    if (!type) return "void";

    // Handle byref first — recurse on inner type, then add *
    bool is_byref = type->byref;
    if (il2cpp_type_is_byref) {
        is_byref = il2cpp_type_is_byref(type);
    }

    std::string result;

    switch (type->type) {
        case IL2CPP_TYPE_VOID:      result = "void"; break;
        case IL2CPP_TYPE_BOOLEAN:   result = "bool"; break;
        case IL2CPP_TYPE_CHAR:      result = "uint16_t"; break;  // C# char = UTF-16
        case IL2CPP_TYPE_I1:        result = "int8_t"; break;
        case IL2CPP_TYPE_U1:        result = "uint8_t"; break;
        case IL2CPP_TYPE_I2:        result = "int16_t"; break;
        case IL2CPP_TYPE_U2:        result = "uint16_t"; break;
        case IL2CPP_TYPE_I4:        result = "int32_t"; break;
        case IL2CPP_TYPE_U4:        result = "uint32_t"; break;
        case IL2CPP_TYPE_I8:        result = "int64_t"; break;
        case IL2CPP_TYPE_U8:        result = "uint64_t"; break;
        case IL2CPP_TYPE_R4:        result = "float"; break;
        case IL2CPP_TYPE_R8:        result = "double"; break;
        case IL2CPP_TYPE_STRING:    result = "System_String_o*"; break;
        case IL2CPP_TYPE_OBJECT:    result = "Il2CppObject*"; break;
        case IL2CPP_TYPE_I:         result = "intptr_t"; break;
        case IL2CPP_TYPE_U:         result = "uintptr_t"; break;

        case IL2CPP_TYPE_PTR:
            if (type->data.type) {
                result = parse_type(type->data.type) + "*";
            } else {
                result = "void*";
            }
            break;

        case IL2CPP_TYPE_SZARRAY:
        case IL2CPP_TYPE_ARRAY: {
            auto klass = il2cpp_class_from_type(type);
            if (klass && il2cpp_class_get_element_class) {
                auto elem = il2cpp_class_get_element_class(klass);
                if (elem) {
                    result = fix_name(il2cpp_class_get_name(elem)) + "_array*";
                    break;
                }
            }
            result = "Il2CppArray*";
            break;
        }

        case IL2CPP_TYPE_VALUETYPE: {
            auto klass = il2cpp_class_from_type(type);
            if (klass) {
                if (il2cpp_class_is_enum && il2cpp_class_is_enum(klass)) {
                    if (il2cpp_class_enum_basetype) {
                        auto base = il2cpp_class_enum_basetype(klass);
                        if (base) {
                            result = parse_type(base);
                            break;
                        }
                    }
                    result = "int32_t";  // default enum underlying type
                    break;
                }
                result = fix_name(il2cpp_class_get_name(klass)) + "_o";
                break;
            }
            result = "Il2CppObject*";
            break;
        }

        case IL2CPP_TYPE_CLASS:
        case IL2CPP_TYPE_GENERICINST: {
            auto klass = il2cpp_class_from_type(type);
            if (klass) {
                if (il2cpp_class_is_valuetype && il2cpp_class_is_valuetype(klass)) {
                    result = fix_name(il2cpp_class_get_name(klass)) + "_o";
                } else {
                    result = fix_name(il2cpp_class_get_name(klass)) + "_o*";
                }
                break;
            }
            result = "Il2CppObject*";
            break;
        }

        default:
            result = "Il2CppObject*";
            break;
    }

    if (is_byref) {
        result += "*";
    }

    return result;
}

std::string build_signature(const MethodInfo *method, Il2CppClass *klass) {
    if (!method) return "void _unknown_ ();";

    std::stringstream ss;

    // Return type
    auto ret_type = il2cpp_method_get_return_type(method);
    ss << parse_type(ret_type) << " ";

    // Method full name: Namespace_Class_Method
    auto ns = il2cpp_class_get_namespace(klass);
    auto cn = il2cpp_class_get_name(klass);
    auto mn = il2cpp_method_get_name(method);

    std::string full;
    if (ns && ns[0] != '\0') {
        full = std::string(ns) + "_" + std::string(cn) + "_" + std::string(mn);
    } else {
        full = std::string(cn) + "_" + std::string(mn);
    }
    ss << fix_name(full) << " (";

    // Parameters
    std::vector<std::string> params;

    uint32_t iflags = 0;
    auto flags = il2cpp_method_get_flags(method, &iflags);

    // Instance method: add 'this' parameter
    if (!(flags & METHOD_ATTRIBUTE_STATIC)) {
        auto this_type = parse_type(il2cpp_class_get_type(klass));
        params.push_back(this_type + " __this");
    }

    // Method parameters
    auto param_count = il2cpp_method_get_param_count(method);
    for (uint32_t i = 0; i < param_count; i++) {
        auto param = il2cpp_method_get_param(method, i);
        auto ptype = parse_type(param);
        auto pname = il2cpp_method_get_param_name(method, i);
        params.push_back(ptype + " " + fix_name(pname ? pname : ""));
    }

    // Sentinel: const MethodInfo* method (matches Il2CppDumper behavior)
    params.push_back("const MethodInfo* method");

    for (size_t i = 0; i < params.size(); i++) {
        if (i > 0) ss << ", ";
        ss << params[i];
    }
    ss << ");";

    return ss.str();
}

// ================================================================
// Metadata parsing — string literal extraction (Phase 2)
// ================================================================

// Metadata header structure (offset+size pairs starting at offset 8).
// We only need the first two fields for string literals.
struct MetadataHeader {
    uint32_t sanity;          // 0xFAB11BAF
    int32_t version;          // e.g. 29, 31
    uint32_t stringLiteralOffset;
    int32_t stringLiteralSize;
    uint32_t stringLiteralDataOffset;
    int32_t stringLiteralDataSize;
};

// Il2CppStringLiteral entry: {uint32 length, int32 dataIndex} — 8 bytes
struct Il2CppStringLiteralEntry {
    uint32_t length;
    int32_t dataIndex;
};

std::vector<StringEntry> extract_strings(const uint8_t *metadata, size_t metadata_size,
                                          uint64_t il2cpp_base_val) {
    std::vector<StringEntry> result;

    if (!metadata || metadata_size < sizeof(MetadataHeader)) {
        return result;
    }

    auto *hdr = reinterpret_cast<const MetadataHeader *>(metadata);

    // Validate magic
    if (hdr->sanity != 0xFAB11BAF) {
        return result;
    }

    // Validate version range
    if (hdr->version < 16 || hdr->version > 31) {
        return result;
    }

    // Validate string literal table bounds
    uint32_t str_lit_off = hdr->stringLiteralOffset;
    int32_t str_lit_size = hdr->stringLiteralSize;
    uint32_t str_data_off = hdr->stringLiteralDataOffset;
    int32_t str_data_size = hdr->stringLiteralDataSize;

    if (str_lit_size <= 0 || str_data_size <= 0) {
        return result;
    }

    // Each entry is 8 bytes
    size_t entry_count = static_cast<size_t>(str_lit_size) / sizeof(Il2CppStringLiteralEntry);
    if (str_lit_size % sizeof(Il2CppStringLiteralEntry) != 0) {
        return result;  // unexpected size
    }

    // Bounds check
    if (str_lit_off + str_lit_size > metadata_size ||
        str_data_off + str_data_size > metadata_size) {
        return result;
    }

    auto *entries = reinterpret_cast<const Il2CppStringLiteralEntry *>(metadata + str_lit_off);

    for (size_t i = 0; i < entry_count; i++) {
        auto &entry = entries[i];
        if (entry.length == 0) {
            // Empty string — include with empty value
            StringEntry se;
            se.address = 0;
            se.value = "";
            result.push_back(se);
            continue;
        }

        // Bounds check for string data
        if (entry.dataIndex < 0 ||
            static_cast<size_t>(entry.dataIndex) + entry.length > static_cast<size_t>(str_data_size)) {
            // Skip invalid entry
            continue;
        }

        auto str_start = str_data_off + entry.dataIndex;
        if (str_start + entry.length > metadata_size) {
            continue;
        }

        // Read UTF-8 string data
        auto *str_ptr = metadata + str_start;
        std::string value(reinterpret_cast<const char *>(str_ptr), entry.length);

        StringEntry se;
        // Address computation:
        // In the desktop Il2CppDumper, ScriptString.Address comes from metadataUsage
        // and is the RVA of the pointer in libil2cpp.so that references this string.
        // At runtime we don't have metadataUsage, so we use the metadata offset
        // as a meaningful unique identifier. If il2cpp_base is provided, we could
        // compute a more precise address, but for now the metadata offset is
        // the best we can do.
        se.address = static_cast<uint64_t>(entry.dataIndex);
        se.value = std::move(value);
        result.push_back(se);
    }

    return result;
}

// ================================================================
// Metadata parsing — metadataUsage extraction (Phase 3)
// ================================================================

// Il2CppMetadataUsage enum values (from Il2CppDumper)
enum Il2CppMetadataUsage {
    kIl2CppMetadataUsageInvalid      = 0,
    kIl2CppMetadataUsageTypeInfo     = 1,
    kIl2CppMetadataUsageIl2CppType   = 2,
    kIl2CppMetadataUsageMethodDef    = 3,
    kIl2CppMetadataUsageFieldInfo    = 4,
    kIl2CppMetadataUsageStringLiteral = 5,
    kIl2CppMetadataUsageMethodRef    = 6,
};

// Decode the usage type from an encoded source index
static uint32_t get_encoded_index_type(uint32_t index) {
    return (index & 0xE0000000) >> 29;
}

// Decode the method/type index from an encoded source index
static uint32_t get_decoded_method_index(uint32_t index, int32_t version) {
    if (version >= 27) {
        return (index & 0x1FFFFFFEU) >> 1;
    }
    return index & 0x1FFFFFFFU;
}

// Version-aware metadata header field offset computation.
// The metadata header is a sequence of uint32_t/int32_t fields,
// some of which are only present in certain version ranges.
// We compute the byte offset of a specific field by iterating
// through all fields that come before it, adding 4 bytes for each
// field that is present in the given version.

struct HeaderFieldRange {
    int min_version;  // inclusive (in tenths: 241 = v24.1)
    int max_version;  // inclusive
};

// All header fields in declaration order, with version ranges.
// Version ranges from Il2CppDumper's MetadataClass.cs.
static const struct {
    const char *name;
    HeaderFieldRange range;  // {min, max} in tenths
} kHeaderFields[] = {
    // Fields always present (min=0, max=990 covers all practical versions)
    {"sanity",                          {0, 990}},
    {"version",                         {0, 990}},
    {"stringLiteralOffset",             {0, 990}},
    {"stringLiteralSize",               {0, 990}},
    {"stringLiteralDataOffset",         {0, 990}},
    {"stringLiteralDataSize",           {0, 990}},
    {"stringOffset",                    {0, 990}},
    {"stringSize",                      {0, 990}},
    {"eventsOffset",                    {0, 990}},
    {"eventsSize",                      {0, 990}},
    {"propertiesOffset",                {0, 990}},
    {"propertiesSize",                  {0, 990}},
    {"methodsOffset",                   {0, 990}},
    {"methodsSize",                     {0, 990}},
    {"parameterDefaultValuesOffset",    {0, 990}},
    {"parameterDefaultValuesSize",      {0, 990}},
    {"fieldDefaultValuesOffset",        {0, 990}},
    {"fieldDefaultValuesSize",          {0, 990}},
    {"fieldAndParameterDefaultValueDataOffset", {0, 990}},
    {"fieldAndParameterDefaultValueDataSize",   {0, 990}},
    {"fieldMarshaledSizesOffset",       {0, 990}},
    {"fieldMarshaledSizesSize",         {0, 990}},
    {"parametersOffset",                {0, 990}},
    {"parametersSize",                  {0, 990}},
    {"fieldsOffset",                    {0, 990}},
    {"fieldsSize",                      {0, 990}},
    {"genericParametersOffset",         {0, 990}},
    {"genericParametersSize",           {0, 990}},
    {"genericParameterConstraintsOffset", {0, 990}},
    {"genericParameterConstraintsSize",   {0, 990}},
    {"genericContainersOffset",         {0, 990}},
    {"genericContainersSize",           {0, 990}},
    {"nestedTypesOffset",               {0, 990}},
    {"nestedTypesSize",                 {0, 990}},
    {"interfacesOffset",                {0, 990}},
    {"interfacesSize",                  {0, 990}},
    {"vtableMethodsOffset",             {0, 990}},
    {"vtableMethodsSize",               {0, 990}},
    {"interfaceOffsetsOffset",          {0, 990}},
    {"interfaceOffsetsSize",            {0, 990}},
    {"typeDefinitionsOffset",           {0, 990}},
    {"typeDefinitionsSize",             {0, 990}},
    // Version-dependent fields
    {"rgctxEntriesOffset",              {0, 241}},   // Max=24.1
    {"rgctxEntriesCount",               {0, 241}},   // Max=24.1
    {"imagesOffset",                    {0, 990}},
    {"imagesSize",                      {0, 990}},
    {"assembliesOffset",                {0, 990}},
    {"assembliesSize",                  {0, 990}},
    {"metadataUsageListsOffset",        {190, 245}}, // Min=19, Max=24.5
    {"metadataUsageListsCount",         {190, 245}},
    {"metadataUsagePairsOffset",        {190, 245}},
    {"metadataUsagePairsCount",         {190, 245}},
    {"fieldRefsOffset",                 {190, 990}}, // Min=19
    {"fieldRefsSize",                   {190, 990}},
    {"referencedAssembliesOffset",      {200, 990}}, // Min=20
    {"referencedAssembliesSize",        {200, 990}},
    {"attributesInfoOffset",            {210, 272}}, // Min=21, Max=27.2
    {"attributesInfoCount",             {210, 272}},
    {"attributeTypesOffset",            {210, 272}},
    {"attributeTypesCount",             {210, 272}},
    {"attributeDataOffset",             {290, 990}}, // Min=29
    {"attributeDataSize",               {290, 990}},
    {"attributeDataRangeOffset",        {290, 990}},
    {"attributeDataRangeSize",          {290, 990}},
    {"unresolvedVirtualCallParameterTypesOffset",   {220, 990}},
    {"unresolvedVirtualCallParameterTypesSize",      {220, 990}},
    {"unresolvedVirtualCallParameterRangesOffset",   {220, 990}},
    {"unresolvedVirtualCallParameterRangesSize",      {220, 990}},
    {"windowsRuntimeTypeNamesOffset",   {230, 990}},
    {"windowsRuntimeTypeNamesSize",     {230, 990}},
    {"windowsRuntimeStringsOffset",     {270, 990}},
    {"windowsRuntimeStringsSize",       {270, 990}},
    {"exportedTypeDefinitionsOffset",   {240, 990}},
    {"exportedTypeDefinitionsSize",     {240, 990}},
};

static constexpr size_t kHeaderFieldCount = sizeof(kHeaderFields) / sizeof(kHeaderFields[0]);

// Compute the byte offset of a named header field for a given metadata version.
// Returns -1 if the field doesn't exist in this version.
static int32_t get_header_field_offset(int32_t version, const char *field_name) {
    // Convert version to tenths for comparison (e.g., 24 → 240, 29 → 290)
    int32_t v10 = version * 10;

    int32_t offset = 0;
    for (size_t i = 0; i < kHeaderFieldCount; i++) {
        if (strcmp(kHeaderFields[i].name, field_name) == 0) {
            // Check if this field exists in this version
            if (v10 < kHeaderFields[i].range.min_version || v10 > kHeaderFields[i].range.max_version) {
                return -1;  // Field not present in this version
            }
            return offset;
        }
        // Add 4 bytes if this field is present in this version
        if (v10 >= kHeaderFields[i].range.min_version && v10 <= kHeaderFields[i].range.max_version) {
            offset += 4;
        }
    }
    return -1;  // Field not found
}

// Read a uint32_t from metadata at a given byte offset
static bool read_metadata_u32(const uint8_t *metadata, size_t metadata_size,
                               int32_t offset, uint32_t &out) {
    if (offset < 0 || static_cast<size_t>(offset) + 4 > metadata_size) return false;
    memcpy(&out, metadata + offset, 4);
    return true;
}

// Read an int32_t from metadata at a given byte offset
static bool read_metadata_i32(const uint8_t *metadata, size_t metadata_size,
                               int32_t offset, int32_t &out) {
    if (offset < 0 || static_cast<size_t>(offset) + 4 > metadata_size) return false;
    memcpy(&out, metadata + offset, 4);
    return true;
}

// Il2CppMetadataUsageList: {uint32 start, uint32 count} — 8 bytes
struct Il2CppMetadataUsageListEntry {
    uint32_t start;
    uint32_t count;
};

// Il2CppMetadataUsagePair: {uint32 destinationIndex, uint32 encodedSourceIndex} — 8 bytes
struct Il2CppMetadataUsagePairEntry {
    uint32_t destinationIndex;
    uint32_t encodedSourceIndex;
};

void extract_metadata_from_blob(const uint8_t *metadata, size_t metadata_size,
                                 uint64_t il2cpp_base_val,
                                 std::vector<MetadataEntry> &meta_out,
                                 std::vector<MetadataMethodEntry> &meta_method_out) {
    if (!metadata || metadata_size < 8) return;

    // Read and validate header
    uint32_t sanity, version_u32;
    memcpy(&sanity, metadata, 4);
    memcpy(&version_u32, metadata + 4, 4);
    int32_t version = static_cast<int32_t>(version_u32);

    if (sanity != 0xFAB11BAF) {
        LOGE("extract_metadata_from_blob: bad magic 0x%08X", sanity);
        return;
    }
    if (version < 19 || version > 31) {
        LOGE("extract_metadata_from_blob: version %d out of range [19,31]", version);
        return;
    }

    // metadataUsageLists/Pairs only exist in v19-v24.5
    // For v25+, these fields are not in the header
    if (version >= 25) {
        LOGI("extract_metadata_from_blob: version %d >= 25, no metadataUsageLists/Pairs in header", version);
        return;
    }

    // Read metadataUsageLists offset/count
    auto mul_off = get_header_field_offset(version, "metadataUsageListsOffset");
    auto mul_cnt_off = get_header_field_offset(version, "metadataUsageListsCount");
    auto mup_off = get_header_field_offset(version, "metadataUsagePairsOffset");
    auto mup_cnt_off = get_header_field_offset(version, "metadataUsagePairsCount");

    if (mul_off < 0 || mul_cnt_off < 0 || mup_off < 0 || mup_cnt_off < 0) {
        LOGE("extract_metadata_from_blob: could not locate metadataUsage fields in header");
        return;
    }

    uint32_t mul_offset, mup_offset;
    int32_t mul_count, mup_count;
    if (!read_metadata_u32(metadata, metadata_size, mul_off, mul_offset) ||
        !read_metadata_i32(metadata, metadata_size, mul_cnt_off, mul_count) ||
        !read_metadata_u32(metadata, metadata_size, mup_off, mup_offset) ||
        !read_metadata_i32(metadata, metadata_size, mup_cnt_off, mup_count)) {
        LOGE("extract_metadata_from_blob: failed to read metadataUsage header fields");
        return;
    }

    if (mul_count <= 0 || mup_count <= 0) {
        LOGI("extract_metadata_from_blob: no metadataUsage entries (lists=%d, pairs=%d)",
             mul_count, mup_count);
        return;
    }

    // Bounds check
    size_t mul_bytes = static_cast<size_t>(mul_count) * sizeof(Il2CppMetadataUsageListEntry);
    size_t mup_bytes = static_cast<size_t>(mup_count) * sizeof(Il2CppMetadataUsagePairEntry);
    if (mul_offset + mul_bytes > metadata_size || mup_offset + mup_bytes > metadata_size) {
        LOGE("extract_metadata_from_blob: metadataUsage tables out of bounds");
        return;
    }

    auto *usage_lists = reinterpret_cast<const Il2CppMetadataUsageListEntry *>(metadata + mul_offset);
    auto *usage_pairs = reinterpret_cast<const Il2CppMetadataUsagePairEntry *>(metadata + mup_offset);

    // Decode metadataUsage entries into per-usage-type sorted maps
    // Map: destinationIndex → decodedSourceIndex, for each usage type
    std::map<uint32_t, uint32_t> usage_maps[7];  // index 1-6 used

    for (int32_t li = 0; li < mul_count; li++) {
        auto &list = usage_lists[li];
        for (uint32_t j = 0; j < list.count; j++) {
            uint32_t pair_idx = list.start + j;
            if (pair_idx >= static_cast<uint32_t>(mup_count)) continue;
            auto &pair = usage_pairs[pair_idx];
            auto usage = get_encoded_index_type(pair.encodedSourceIndex);
            auto decoded = get_decoded_method_index(pair.encodedSourceIndex, version);
            if (usage >= 1 && usage <= 6) {
                usage_maps[usage][pair.destinationIndex] = decoded;
            }
        }
    }

    // Now we have the decoded metadataUsage entries, but we need the
    // metadataUsages pointer array from Il2CppMetadataRegistration to compute
    // the actual Address values. At runtime, we can't easily locate this array.
    //
    // Strategy: Generate entries with the information we have.
    // For Address, we use 0 as a placeholder since we don't have the
    // metadataUsages array. The extract_metadata_from_runtime() function
    // will provide Address values using the runtime API approach.
    //
    // However, we can still extract useful Name/Signature info from the
    // metadata blob by reading the string table and type definitions.

    // Read the string table offset/size from header
    auto str_off = get_header_field_offset(version, "stringOffset");
    auto str_sz_off = get_header_field_offset(version, "stringSize");
    uint32_t string_offset = 0;
    int32_t string_size = 0;
    if (str_off >= 0 && str_sz_off >= 0) {
        read_metadata_u32(metadata, metadata_size, str_off, string_offset);
        read_metadata_i32(metadata, metadata_size, str_sz_off, string_size);
    }

    // Helper to read a null-terminated string from the metadata string table
    // (Reserved for future use — currently names come from runtime API)
    (void)string_offset; (void)string_size;

    // Read typeDefinitions offset/size
    auto td_off = get_header_field_offset(version, "typeDefinitionsOffset");
    auto td_sz_off = get_header_field_offset(version, "typeDefinitionsSize");
    uint32_t td_offset = 0;
    int32_t td_size = 0;
    if (td_off >= 0 && td_sz_off >= 0) {
        read_metadata_u32(metadata, metadata_size, td_off, td_offset);
        read_metadata_i32(metadata, metadata_size, td_sz_off, td_size);
    }

    // Read methods offset/size
    auto meth_off = get_header_field_offset(version, "methodsOffset");
    auto meth_sz_off = get_header_field_offset(version, "methodsSize");
    uint32_t methods_offset = 0;
    int32_t methods_size = 0;
    if (meth_off >= 0 && meth_sz_off >= 0) {
        read_metadata_u32(metadata, metadata_size, meth_off, methods_offset);
        read_metadata_i32(metadata, metadata_size, meth_sz_off, methods_size);
    }

    // Read fields offset/size
    auto fld_off = get_header_field_offset(version, "fieldsOffset");
    auto fld_sz_off = get_header_field_offset(version, "fieldsSize");
    uint32_t fields_offset = 0;
    int32_t fields_size = 0;
    if (fld_off >= 0 && fld_sz_off >= 0) {
        read_metadata_u32(metadata, metadata_size, fld_off, fields_offset);
        read_metadata_i32(metadata, metadata_size, fld_sz_off, fields_size);
    }

    // Read fieldRefs offset/size
    auto fr_off = get_header_field_offset(version, "fieldRefsOffset");
    auto fr_sz_off = get_header_field_offset(version, "fieldRefsSize");
    uint32_t fieldrefs_offset = 0;
    int32_t fieldrefs_size = 0;
    if (fr_off >= 0 && fr_sz_off >= 0) {
        read_metadata_u32(metadata, metadata_size, fr_off, fieldrefs_offset);
        read_metadata_i32(metadata, metadata_size, fr_sz_off, fieldrefs_size);
    }

    // Il2CppTypeDefinition layout (simplified — we only need nameIndex and namespaceIndex)
    // The layout varies by version, but the first few fields are relatively stable:
    //   int32_t nameIndex, int32_t namespaceIndex, ...
    // We only read the nameIndex (offset 0) and namespaceIndex (offset 4)
    struct RawTypeDef {
        int32_t nameIndex;
        int32_t namespaceIndex;
    };

    // Il2CppMethodDefinition layout (simplified):
    //   int32_t nameIndex, int32_t declaringType, ...
    struct RawMethodDef {
        int32_t nameIndex;
        int32_t declaringType;
    };

    // Il2CppFieldDefinition layout (simplified):
    //   int32_t nameIndex, ...
    struct RawFieldDef {
        int32_t nameIndex;
    };

    // Il2CppFieldRef layout:
    //   int32_t typeIndex, int32_t fieldIndex
    struct RawFieldRef {
        int32_t typeIndex;
        int32_t fieldIndex;
    };

    // Compute TypeDef size based on version
    // We don't need the exact stride since we're using runtime API for names
    (void)td_offset; (void)td_size;
    (void)methods_offset; (void)methods_size;
    (void)fields_offset; (void)fields_size;
    (void)fieldrefs_offset; (void)fieldrefs_size;

    // Process each usage type
    // TypeInfo (usage=1): Name = "{TypeName}_TypeInfo", Signature = "{FixName}_c*"
    for (auto &[dest_idx, decoded_idx] : usage_maps[kIl2CppMetadataUsageTypeInfo]) {
        MetadataEntry entry;
        entry.address = 0;  // Would need metadataUsages[dest_idx] for real address
        // Try to get type name from typeDefinitions
        if (td_offset > 0 && td_size > 0 && decoded_idx < static_cast<uint32_t>(td_size)) {
            // Read nameIndex from the TypeDef at the appropriate offset
            // TypeDef stride is version-dependent; we need to be careful here.
            // For now, we skip precise name resolution from metadata and
            // let extract_metadata_from_runtime() handle it.
        }
        entry.name = "TypeInfo_" + std::to_string(decoded_idx);
        entry.signature = "Il2CppClass*";
        meta_out.push_back(entry);
    }

    // Il2CppType (usage=2): Name = "{TypeName}_var", Signature = "Il2CppType*"
    for (auto &[dest_idx, decoded_idx] : usage_maps[kIl2CppMetadataUsageIl2CppType]) {
        MetadataEntry entry;
        entry.address = 0;
        entry.name = "Il2CppType_" + std::to_string(decoded_idx);
        entry.signature = "Il2CppType*";
        meta_out.push_back(entry);
    }

    // MethodDef (usage=3): Name = "Method${TypeName.MethodName}()"
    for (auto &[dest_idx, decoded_idx] : usage_maps[kIl2CppMetadataUsageMethodDef]) {
        MetadataMethodEntry entry;
        entry.address = 0;
        entry.name = "Method$MethodDef_" + std::to_string(decoded_idx);
        entry.method_address = 0;
        meta_method_out.push_back(entry);
    }

    // FieldInfo (usage=4): Name = "Field${TypeName.FieldName}"
    for (auto &[dest_idx, decoded_idx] : usage_maps[kIl2CppMetadataUsageFieldInfo]) {
        MetadataEntry entry;
        entry.address = 0;
        entry.name = "Field$FieldRef_" + std::to_string(decoded_idx);
        meta_out.push_back(entry);
    }

    // StringLiteral (usage=5): already handled by ScriptString in Phase 2
    // MethodRef (usage=6): Name = "Method${GenericMethod}()"
    for (auto &[dest_idx, decoded_idx] : usage_maps[kIl2CppMetadataUsageMethodRef]) {
        MetadataMethodEntry entry;
        entry.address = 0;
        entry.name = "Method$MethodRef_" + std::to_string(decoded_idx);
        entry.method_address = 0;
        meta_method_out.push_back(entry);
    }

    LOGI("extract_metadata_from_blob: %zu ScriptMetadata, %zu ScriptMetadataMethod",
         meta_out.size(), meta_method_out.size());
}

// ================================================================
// Runtime API-based metadata extraction (Phase 3)
// ================================================================

// Helper: build a full type name like "Namespace.TypeName" or just "TypeName"
static std::string get_full_type_name(Il2CppClass *klass) {
    if (!klass) return "";
    auto ns = il2cpp_class_get_namespace(klass);
    auto cn = il2cpp_class_get_name(klass);
    if (ns && ns[0] != '\0') {
        return std::string(ns) + "." + std::string(cn);
    }
    return std::string(cn);
}

void extract_metadata_from_runtime(std::vector<MetadataEntry> &meta_out,
                                    std::vector<MetadataMethodEntry> &meta_method_out) {
    if (!il2cpp_domain_get || !il2cpp_domain_get_assemblies ||
        !il2cpp_assembly_get_image || !il2cpp_image_get_class_count ||
        !il2cpp_image_get_class) {
        LOGE("extract_metadata_from_runtime: required API functions not available");
        return;
    }

    auto domain = il2cpp_domain_get();
    if (!domain) return;

    size_t asm_count = 0;
    auto assemblies = il2cpp_domain_get_assemblies(domain, &asm_count);
    if (!assemblies || asm_count == 0) return;

    for (size_t i = 0; i < asm_count; i++) {
        auto image = il2cpp_assembly_get_image(assemblies[i]);
        if (!image) continue;
        auto class_count = il2cpp_image_get_class_count(image);

        for (size_t j = 0; j < class_count; j++) {
            auto klass = const_cast<Il2CppClass *>(il2cpp_image_get_class(image, j));
            if (!klass) continue;

            auto full_name = get_full_type_name(klass);
            if (full_name.empty()) continue;

            // --- TypeInfo entry ---
            // Address = RVA of the Il2CppClass struct itself
            {
                MetadataEntry entry;
                auto klass_ptr = reinterpret_cast<uint64_t>(klass);
                entry.address = (klass_ptr > il2cpp_base) ? (klass_ptr - il2cpp_base) : 0;
                entry.name = full_name + "_TypeInfo";

                // Signature: if it's an array type, use "Il2CppClass*",
                // otherwise use "{FixName}_c*"
                auto type = il2cpp_class_get_type ? il2cpp_class_get_type(klass) : nullptr;
                if (type && (type->type == IL2CPP_TYPE_SZARRAY || type->type == IL2CPP_TYPE_ARRAY)) {
                    entry.signature = "Il2CppClass*";
                } else {
                    entry.signature = fix_name(il2cpp_class_get_name(klass)) + "_c*";
                }
                meta_out.push_back(entry);
            }

            // --- Il2CppType entry ---
            if (il2cpp_class_get_type) {
                auto type_ptr = il2cpp_class_get_type(klass);
                if (type_ptr) {
                    MetadataEntry entry;
                    auto addr = reinterpret_cast<uint64_t>(type_ptr);
                    entry.address = (addr > il2cpp_base) ? (addr - il2cpp_base) : 0;
                    entry.name = full_name + "_var";
                    entry.signature = "Il2CppType*";
                    meta_out.push_back(entry);
                }
            }

            // --- FieldInfo entries ---
            if (il2cpp_class_get_fields && il2cpp_field_get_name && il2cpp_field_get_type) {
                void *field_iter = nullptr;
                while (auto field = il2cpp_class_get_fields(klass, &field_iter)) {
                    auto fname = il2cpp_field_get_name(field);
                    if (!fname) continue;

                    MetadataEntry entry;
                    auto field_ptr = reinterpret_cast<uint64_t>(field);
                    entry.address = (field_ptr > il2cpp_base) ? (field_ptr - il2cpp_base) : 0;
                    entry.name = "Field$" + full_name + "." + std::string(fname);
                    // Signature not typically set for FieldInfo in Il2CppDumper
                    entry.signature = "";
                    meta_out.push_back(entry);
                }
            }

            // --- MethodDef entries ---
            if (il2cpp_class_get_methods && il2cpp_method_get_name &&
                il2cpp_method_get_flags) {
                void *method_iter = nullptr;
                while (auto method = il2cpp_class_get_methods(klass, &method_iter)) {
                    if (!method) continue;
                    auto mname = il2cpp_method_get_name(method);
                    if (!mname) continue;

                    MetadataMethodEntry entry;
                    // Address = 0 (we don't have the pointer slot address)
                    entry.address = 0;
                    entry.name = "Method$" + full_name + "." + std::string(mname) + "()";

                    // MethodAddress = method code RVA
                    if (method->methodPointer) {
                        uint64_t rva = reinterpret_cast<uint64_t>(method->methodPointer) - il2cpp_base;
                        entry.method_address = (rva > 0) ? rva : 0;
                    } else {
                        entry.method_address = 0;
                    }
                    meta_method_out.push_back(entry);
                }
            }
        }
    }

    LOGI("extract_metadata_from_runtime: %zu ScriptMetadata, %zu ScriptMetadataMethod",
         meta_out.size(), meta_method_out.size());
}

// ================================================================
// I/O: Write script.json
// ================================================================

void write_script_json(const char *outDir,
                       const std::vector<ScriptMethodEntry> &methods,
                       const std::vector<StringEntry> &strings,
                       const std::vector<MetadataEntry> &metadata,
                       const std::vector<MetadataMethodEntry> &metadata_methods,
                       const std::vector<uint64_t> &addresses) {
    auto outPath = std::string(outDir).append("/files/script.json");
    LOGI("writing script.json to %s", outPath.c_str());

    std::ofstream f(outPath);
    if (!f.is_open()) {
        LOGE("failed to create %s", outPath.c_str());
        return;
    }

    f << "{\n";

    // --- ScriptMethod ---
    f << "  \"ScriptMethod\": [\n";
    for (size_t i = 0; i < methods.size(); i++) {
        const auto &m = methods[i];
        f << "    {\n"
          << "      \"Address\": " << m.address << ",\n"
          << "      \"Name\": \"" << escape_json(m.name) << "\",\n"
          << "      \"Signature\": \"" << escape_json(m.signature) << "\",\n"
          << "      \"TypeSignature\": \"" << escape_json(m.type_sig) << "\"\n"
          << "    }";
        if (i < methods.size() - 1) f << ",";
        f << "\n";
    }
    f << "  ],\n";

    // --- ScriptString ---
    f << "  \"ScriptString\": [\n";
    for (size_t i = 0; i < strings.size(); i++) {
        const auto &s = strings[i];
        f << "    {\n"
          << "      \"Address\": " << s.address << ",\n"
          << "      \"Value\": \"" << escape_json(s.value) << "\"\n"
          << "    }";
        if (i < strings.size() - 1) f << ",";
        f << "\n";
    }
    f << "  ],\n";

    // --- ScriptMetadata ---
    f << "  \"ScriptMetadata\": [\n";
    for (size_t i = 0; i < metadata.size(); i++) {
        const auto &m = metadata[i];
        f << "    {\n"
          << "      \"Address\": " << m.address << ",\n"
          << "      \"Name\": \"" << escape_json(m.name) << "\",\n"
          << "      \"Signature\": \"" << escape_json(m.signature) << "\"\n"
          << "    }";
        if (i < metadata.size() - 1) f << ",";
        f << "\n";
    }
    f << "  ],\n";

    // --- ScriptMetadataMethod ---
    f << "  \"ScriptMetadataMethod\": [\n";
    for (size_t i = 0; i < metadata_methods.size(); i++) {
        const auto &mm = metadata_methods[i];
        f << "    {\n"
          << "      \"Address\": " << mm.address << ",\n"
          << "      \"Name\": \"" << escape_json(mm.name) << "\",\n"
          << "      \"MethodAddress\": " << mm.method_address << "\n"
          << "    }";
        if (i < metadata_methods.size() - 1) f << ",";
        f << "\n";
    }
    f << "  ],\n";

    // --- Addresses ---
    f << "  \"Addresses\": [\n";
    for (size_t i = 0; i < addresses.size(); i++) {
        f << "    " << addresses[i];
        if (i < addresses.size() - 1) f << ",";
        f << "\n";
    }
    f << "  ]\n";

    f << "}\n";
    f.close();

    LOGI("script.json done! %zu ScriptMethod, %zu ScriptString, %zu ScriptMetadata, %zu ScriptMetadataMethod, %zu Addresses",
         methods.size(), strings.size(), metadata.size(), metadata_methods.size(), addresses.size());
}

void write_stringliteral_json(const char *outDir,
                               const std::vector<StringEntry> &strings) {
    auto outPath = std::string(outDir).append("/files/stringliteral.json");
    LOGI("writing stringliteral.json to %s", outPath.c_str());

    std::ofstream f(outPath);
    if (!f.is_open()) {
        LOGE("failed to create %s", outPath.c_str());
        return;
    }

    f << "[\n";
    for (size_t i = 0; i < strings.size(); i++) {
        const auto &s = strings[i];
        f << "  {\n"
          << "    \"value\": \"" << escape_json(s.value) << "\",\n"
          << "    \"address\": \"0x" << std::hex << s.address << std::dec << "\"\n"
          << "  }";
        if (i < strings.size() - 1) f << ",";
        f << "\n";
    }
    f << "]\n";
    f.close();

    LOGI("stringliteral.json done! %zu entries", strings.size());
}
