#pragma once
//
// script_json.h — Generate Il2CppDumper-compatible script.json output
//
// Phase 1: ScriptMethod + Addresses
// Phase 2: ScriptString + stringliteral.json
//

#include <cstdint>
#include <string>
#include <vector>

// Forward declarations (from il2cpp-class.h)
struct Il2CppType;
struct Il2CppClass;
struct MethodInfo;

// ----------------------------------------------------------------
// Data structures for script.json entries
// ----------------------------------------------------------------

struct ScriptMethodEntry {
    uint64_t address;        // RVA (methodPointer - il2cpp_base)
    std::string name;        // "Namespace.Type$$Method"
    std::string signature;   // "void Namespace_Type_Method (Namespace_Type_o* __this, int32_t x, const MethodInfo* method);"
    std::string type_sig;    // "vii" etc.
};

struct StringEntry {
    uint64_t address;        // RVA in libil2cpp.so (metadataUsage pointer)
    std::string value;       // Decoded UTF-8 string value
};

struct MetadataEntry {
    uint64_t address;        // RVA of the pointer slot referencing this metadata
    std::string name;        // e.g. "System.String_TypeInfo" or "Field$Game.Player.health"
    std::string signature;   // e.g. "System_String_c*" or "Il2CppType*"
};

struct MetadataMethodEntry {
    uint64_t address;        // RVA of the pointer slot referencing this method
    std::string name;        // e.g. "Method$Game.Player.TakeDamage()"
    uint64_t method_address; // RVA of the method code itself
};

// ----------------------------------------------------------------
// Pure functions — testable without il2cpp runtime
// ----------------------------------------------------------------

// Clean a name for use as a C identifier:
// - Replace non-alnum/underscore with underscore
// - Prefix C++ keywords with underscore
// - Prefix leading digits with underscore
std::string fix_name(const std::string &name);

// Escape a string for JSON output (handles quotes, backslash, control chars)
std::string escape_json(const std::string &s);

// Map a single Il2CppType to its TypeSignature character
// void→'v', float→'f', double→'d', int64/uint64→'j', everything else→'i'
char type_to_signature_char(const Il2CppType *type);

// Build the full TypeSignature string for a method (return + params)
std::string build_type_signature(const MethodInfo *method);

// ----------------------------------------------------------------
// Metadata parsing — string literal extraction (Phase 2)
// ----------------------------------------------------------------

// Extract string literals from global-metadata.dat in memory.
// metadata: pointer to the raw metadata blob
// metadata_size: size of the blob in bytes
// il2cpp_base: base address of libil2cpp.so (for computing RVAs)
//   If il2cpp_base == 0, Address fields will be set to the metadata-internal offset.
// Returns a vector of StringEntry with all valid string literals.
std::vector<StringEntry> extract_strings(const uint8_t *metadata, size_t metadata_size,
                                          uint64_t il2cpp_base = 0);

// ----------------------------------------------------------------
// Metadata parsing — metadataUsage extraction (Phase 3)
// ----------------------------------------------------------------

// Extract ScriptMetadata/ScriptMetadataMethod from global-metadata.dat.
// Only works for metadata versions 19-24.5 (where metadataUsageLists/Pairs exist).
// metadata: pointer to the raw metadata blob
// metadata_size: size of the blob in bytes
// il2cpp_base: base address of libil2cpp.so
// meta_out: output vector for ScriptMetadata entries
// meta_method_out: output vector for ScriptMetadataMethod entries
void extract_metadata_from_blob(const uint8_t *metadata, size_t metadata_size,
                                 uint64_t il2cpp_base,
                                 std::vector<MetadataEntry> &meta_out,
                                 std::vector<MetadataMethodEntry> &meta_method_out);

// ----------------------------------------------------------------
// Functions requiring il2cpp runtime — not unit-testable on host
// ----------------------------------------------------------------

// Convert an Il2CppType to a C type string for use in signatures
// e.g. IL2CPP_TYPE_I4 → "int32_t", IL2CPP_TYPE_CLASS → "ClassName_o*"
std::string parse_type(const Il2CppType *type);

// Build the full C-style method signature string
std::string build_signature(const MethodInfo *method, Il2CppClass *klass);

// Extract ScriptMetadata/ScriptMetadataMethod using il2cpp runtime API.
// This is the fallback for versions where metadataUsageLists/Pairs are not available,
// or when the metadataUsages pointer array cannot be located.
// Iterates all classes/methods/fields via the runtime API.
void extract_metadata_from_runtime(std::vector<MetadataEntry> &meta_out,
                                    std::vector<MetadataMethodEntry> &meta_method_out);

// ----------------------------------------------------------------
// I/O — write script.json and stringliteral.json to disk
// ----------------------------------------------------------------

// Write a complete script.json with all sections
void write_script_json(const char *outDir,
                       const std::vector<ScriptMethodEntry> &methods,
                       const std::vector<StringEntry> &strings,
                       const std::vector<MetadataEntry> &metadata,
                       const std::vector<MetadataMethodEntry> &metadata_methods,
                       const std::vector<uint64_t> &addresses);

// Write stringliteral.json (lightweight format for offline analysis)
void write_stringliteral_json(const char *outDir,
                               const std::vector<StringEntry> &strings);
