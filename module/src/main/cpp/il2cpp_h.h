//
// il2cpp_h.h — Generate Il2CppDumper-compatible il2cpp.h header file
//
// Phase 4: C struct definitions for all il2cpp types
//

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <set>
#include <map>

#include "il2cpp-class.h"

// ----------------------------------------------------------------
// Data structures for il2cpp.h generation
// ----------------------------------------------------------------

struct StructFieldInfo {
    std::string type_name;       // C type string (e.g. "int32_t", "System_String_o*")
    std::string field_name;      // C field name (fixed)
    bool is_value_type;          // true if the field type is a value type (embedded, not pointer)
    bool is_custom_type;         // true if the field type needs "struct" prefix
};

struct VTableMethodInfo {
    std::string name;  // method name or "unknown"
};

enum RGCTXEntryType { RGCTX_DATA_TYPE, RGCTX_DATA_CLASS, RGCTX_DATA_METHOD };

struct RGCTXEntryInfo {
    RGCTXEntryType type;
    std::string name;
};

struct StructInfo {
    std::string type_name;       // Unique struct base name (e.g. "Game_Player")
    bool is_value_type;          // true if this is a value type (struct in C#)
    bool is_enum;                // true if this is an enum type
    std::string parent_name;     // Parent struct base name (empty if System.Object or none)
    std::vector<StructFieldInfo> fields;          // Instance fields
    std::vector<StructFieldInfo> static_fields;   // Static fields
    std::vector<VTableMethodInfo> vtable_methods; // Virtual table methods
    std::vector<RGCTXEntryInfo> rgctx_entries;    // Runtime generic context entries
    bool has_static_fields() const { return !static_fields.empty(); }
    bool has_vtable() const { return !vtable_methods.empty(); }
};

// ----------------------------------------------------------------
// Functions requiring il2cpp runtime — not unit-testable on host
// ----------------------------------------------------------------

// Collect all type information from il2cpp runtime API.
// Returns a vector of StructInfo for all loaded types.
std::vector<StructInfo> collect_type_info();

// ----------------------------------------------------------------
// I/O — write il2cpp.h to disk
// ----------------------------------------------------------------

// Write il2cpp.h with struct definitions for all types.
// outDir: output directory (il2cpp.h goes into outDir/files/)
// types: type information collected from collect_type_info()
// version: il2cpp metadata version (default 29 for latest)
void write_il2cpp_h(const char *outDir, const std::vector<StructInfo> &types, int32_t version = 29);

// ----------------------------------------------------------------
// Pure logic — unit-testable on host
// ----------------------------------------------------------------

// Generate the complete il2cpp.h content as a string.
// This is the core logic that can be tested without file I/O.
std::string generate_il2cpp_h_content(const std::vector<StructInfo> &types);

// Get the C type string for an Il2CppType at runtime.
// Delegates to parse_type() but also handles field-specific logic.
// context: optional generic context for resolving IL2CPP_TYPE_VAR/MVAR
std::string get_field_type_name(const Il2CppType *type, bool *is_value_type, bool *is_custom_type,
                                 const Il2CppGenericContext *context = nullptr);

// Get the version-specific header string for the given il2cpp metadata version.
const char* get_version_header(int32_t version);

// Ensure struct names are unique by appending _1, _2 etc.
std::string get_unique_struct_name(const std::string &name, std::set<std::string> &used_names);
