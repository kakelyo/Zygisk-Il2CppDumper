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

struct StructInfo {
    std::string type_name;       // Unique struct base name (e.g. "Game_Player")
    bool is_value_type;          // true if this is a value type (struct in C#)
    bool is_enum;                // true if this is an enum type
    std::string parent_name;     // Parent struct base name (empty if System.Object or none)
    std::vector<StructFieldInfo> fields;          // Instance fields
    std::vector<StructFieldInfo> static_fields;   // Static fields
    bool has_static_fields() const { return !static_fields.empty(); }
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
void write_il2cpp_h(const char *outDir, const std::vector<StructInfo> &types);

// ----------------------------------------------------------------
// Pure logic — unit-testable on host
// ----------------------------------------------------------------

// Generate the complete il2cpp.h content as a string.
// This is the core logic that can be tested without file I/O.
std::string generate_il2cpp_h_content(const std::vector<StructInfo> &types);

// Get the C type string for an Il2CppType at runtime.
// Delegates to parse_type() but also handles field-specific logic.
std::string get_field_type_name(const Il2CppType *type, bool *is_value_type, bool *is_custom_type);
