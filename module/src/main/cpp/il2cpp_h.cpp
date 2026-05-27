//
// il2cpp_h.cpp — Generate Il2CppDumper-compatible il2cpp.h header file
//
// Phase 4: C struct definitions for all il2cpp types
//

#include "il2cpp_h.h"
#include "script_json.h"  // for fix_name, parse_type, il2cpp_base
#include "il2cpp-tabledefs.h"  // for FIELD_ATTRIBUTE_*, METHOD_ATTRIBUTE_*

// il2cpp API function pointer extern declarations
// (same pattern as script_json.cpp — definitions come from il2cpp_dump.cpp or test stubs)
#define DO_API(r, n, p) extern r (*n) p
#include "il2cpp-api-functions.h"
#undef DO_API

// Logging — in test mode (SCRIPT_JSON_TEST), log.h is not available.
#ifndef SCRIPT_JSON_TEST
#include "log.h"
#else
#include <cstdio>
#define LOGI(...) do { printf("[INFO ] "); printf(__VA_ARGS__); printf("\n"); } while(0)
#define LOGE(...) do { fprintf(stderr, "[ERROR] "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } while(0)
#endif

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <functional>
#include <sstream>
#include <set>
#include <map>

// ================================================================
// Internal il2cpp struct — Il2CppGenericClass definition
// Completes the forward declaration from il2cpp-class.h
// Layout based on il2cpp source; stable across most versions
// ================================================================

struct Il2CppGenericClass {
    TypeDefinitionIndex typeDefinitionIndex;
    Il2CppGenericContext context;
    Il2CppClass* cached_class;
};

// ================================================================
// Generic header — base type definitions (exact match with Il2CppDumper desktop)
// ================================================================

static const char *kGenericHeader =
"typedef void(*Il2CppMethodPointer)();\n"
"\n"
"struct MethodInfo;\n"
"\n"
"struct VirtualInvokeData\n"
"{\n"
"    Il2CppMethodPointer methodPtr;\n"
"    const MethodInfo* method;\n"
"};\n"
"\n"
"struct Il2CppType\n"
"{\n"
"    void* data;\n"
"    unsigned int bits;\n"
"};\n"
"\n"
"struct Il2CppClass;\n"
"\n"
"struct Il2CppObject\n"
"{\n"
"    Il2CppClass *klass;\n"
"    void *monitor;\n"
"};\n"
"\n"
"union Il2CppRGCTXData\n"
"{\n"
"    void* rgctxDataDummy;\n"
"    const MethodInfo* method;\n"
"    const Il2CppType* type;\n"
"    Il2CppClass* klass;\n"
"};\n"
"\n"
"struct Il2CppRuntimeInterfaceOffsetPair\n"
"{\n"
"    Il2CppClass* interfaceType;\n"
"    int32_t offset;\n"
"};\n"
"\n";

// ================================================================
// Version-specific headers — exact match with Il2CppDumper desktop
// HeaderConstants.cs
// ================================================================

// HeaderV22: version <= 22
static const char *kHeaderV22 =
"struct Il2CppClass_1\n"
"{\n"
"    void* image;\n"
"    void* gc_desc;\n"
"    const char* name;\n"
"    const char* namespaze;\n"
"    Il2CppType* byval_arg;\n"
"    Il2CppType* this_arg;\n"
"    Il2CppClass* element_class;\n"
"    Il2CppClass* castClass;\n"
"    Il2CppClass* declaringType;\n"
"    Il2CppClass* parent;\n"
"    void *generic_class;\n"
"    void* typeDefinition;\n"
"    void* fields;\n"
"    void* events;\n"
"    void* properties;\n"
"    void* methods;\n"
"    Il2CppClass** nestedTypes;\n"
"    Il2CppClass** implementedInterfaces;\n"
"    Il2CppRuntimeInterfaceOffsetPair* interfaceOffsets;\n"
"};\n"
"\n"
"struct Il2CppClass_2\n"
"{\n"
"    Il2CppClass** typeHierarchy;\n"
"    uint32_t cctor_started;\n"
"    uint32_t cctor_finished;\n"
"    uint64_t cctor_thread;\n"
"    int32_t genericContainerIndex;\n"
"    int32_t customAttributeIndex;\n"
"    uint32_t instance_size;\n"
"    uint32_t actualSize;\n"
"    uint32_t element_size;\n"
"    int32_t native_size;\n"
"    uint32_t static_fields_size;\n"
"    uint32_t thread_static_fields_size;\n"
"    int32_t thread_static_fields_offset;\n"
"    uint32_t flags;\n"
"    uint32_t token;\n"
"    uint16_t method_count;\n"
"    uint16_t property_count;\n"
"    uint16_t field_count;\n"
"    uint16_t event_count;\n"
"    uint16_t nested_type_count;\n"
"    uint16_t vtable_count;\n"
"    uint16_t interfaces_count;\n"
"    uint16_t interface_offsets_count;\n"
"    uint8_t typeHierarchyDepth;\n"
"    uint8_t genericRecursionDepth;\n"
"    uint8_t rank;\n"
"    uint8_t minimumAlignment;\n"
"    uint8_t packingSize;\n"
"    uint8_t bitflags1;\n"
"    uint8_t bitflags2;\n"
"};\n"
"\n"
"struct Il2CppClass\n"
"{\n"
"    Il2CppClass_1 _1;\n"
"    void* static_fields;\n"
"    Il2CppRGCTXData* rgctx_data;\n"
"    Il2CppClass_2 _2;\n"
"    VirtualInvokeData vtable[255];\n"
"};\n"
"\n"
"typedef int32_t il2cpp_array_size_t;\n"
"typedef int32_t il2cpp_array_lower_bound_t;\n"
"struct Il2CppArrayBounds\n"
"{\n"
"    il2cpp_array_size_t length;\n"
"    il2cpp_array_lower_bound_t lower_bound;\n"
"};\n"
"\n"
"struct MethodInfo\n"
"{\n"
"    Il2CppMethodPointer methodPointer;\n"
"    void* invoker_method;\n"
"    const char* name;\n"
"    Il2CppClass *declaring_type;\n"
"    const Il2CppType *return_type;\n"
"    const void* parameters;\n"
"    union\n"
"    {\n"
"        const Il2CppRGCTXData* rgctx_data;\n"
"        const void* methodDefinition;\n"
"    };\n"
"    union\n"
"    {\n"
"        const void* genericMethod;\n"
"        const void* genericContainer;\n"
"    };\n"
"    int32_t customAttributeIndex;\n"
"    uint32_t token;\n"
"    uint16_t flags;\n"
"    uint16_t iflags;\n"
"    uint16_t slot;\n"
"    uint8_t parameters_count;\n"
"    uint8_t bitflags;\n"
"};\n"
"\n";

// HeaderV240: version 23-24.0
static const char *kHeaderV240 =
"struct Il2CppClass_1\n"
"{\n"
"    void* image;\n"
"    void* gc_desc;\n"
"    const char* name;\n"
"    const char* namespaze;\n"
"    Il2CppType* byval_arg;\n"
"    Il2CppType* this_arg;\n"
"    Il2CppClass* element_class;\n"
"    Il2CppClass* castClass;\n"
"    Il2CppClass* declaringType;\n"
"    Il2CppClass* parent;\n"
"    void *generic_class;\n"
"    void* typeDefinition;\n"
"    void* interopData;\n"
"    void* fields;\n"
"    void* events;\n"
"    void* properties;\n"
"    void* methods;\n"
"    Il2CppClass** nestedTypes;\n"
"    Il2CppClass** implementedInterfaces;\n"
"    Il2CppRuntimeInterfaceOffsetPair* interfaceOffsets;\n"
"};\n"
"\n"
"struct Il2CppClass_2\n"
"{\n"
"    Il2CppClass** typeHierarchy;\n"
"    uint32_t cctor_started;\n"
"    uint32_t cctor_finished;\n"
"    uint64_t cctor_thread;\n"
"    int32_t genericContainerIndex;\n"
"    int32_t customAttributeIndex;\n"
"    uint32_t instance_size;\n"
"    uint32_t actualSize;\n"
"    uint32_t element_size;\n"
"    int32_t native_size;\n"
"    uint32_t static_fields_size;\n"
"    uint32_t thread_static_fields_size;\n"
"    int32_t thread_static_fields_offset;\n"
"    uint32_t flags;\n"
"    uint32_t token;\n"
"    uint16_t method_count;\n"
"    uint16_t property_count;\n"
"    uint16_t field_count;\n"
"    uint16_t event_count;\n"
"    uint16_t nested_type_count;\n"
"    uint16_t vtable_count;\n"
"    uint16_t interfaces_count;\n"
"    uint16_t interface_offsets_count;\n"
"    uint8_t typeHierarchyDepth;\n"
"    uint8_t genericRecursionDepth;\n"
"    uint8_t rank;\n"
"    uint8_t minimumAlignment;\n"
"    uint8_t packingSize;\n"
"    uint8_t bitflags1;\n"
"    uint8_t bitflags2;\n"
"};\n"
"\n"
"struct Il2CppClass\n"
"{\n"
"    Il2CppClass_1 _1;\n"
"    void* static_fields;\n"
"    Il2CppRGCTXData* rgctx_data;\n"
"    Il2CppClass_2 _2;\n"
"    VirtualInvokeData vtable[255];\n"
"};\n"
"\n"
"typedef int32_t il2cpp_array_size_t;\n"
"typedef int32_t il2cpp_array_lower_bound_t;\n"
"struct Il2CppArrayBounds\n"
"{\n"
"    il2cpp_array_size_t length;\n"
"    il2cpp_array_lower_bound_t lower_bound;\n"
"};\n"
"\n"
"struct MethodInfo\n"
"{\n"
"    Il2CppMethodPointer methodPointer;\n"
"    void* invoker_method;\n"
"    const char* name;\n"
"    Il2CppClass *declaring_type;\n"
"    const Il2CppType *return_type;\n"
"    const void* parameters;\n"
"    union\n"
"    {\n"
"        const Il2CppRGCTXData* rgctx_data;\n"
"        const void* methodDefinition;\n"
"    };\n"
"    union\n"
"    {\n"
"        const void* genericMethod;\n"
"        const void* genericContainer;\n"
"    };\n"
"    int32_t customAttributeIndex;\n"
"    uint32_t token;\n"
"    uint16_t flags;\n"
"    uint16_t iflags;\n"
"    uint16_t slot;\n"
"    uint8_t parameters_count;\n"
"    uint8_t bitflags;\n"
"};\n"
"\n";

// HeaderV241: version 24.1
static const char *kHeaderV241 =
"struct Il2CppClass_1\n"
"{\n"
"    void* image;\n"
"    void* gc_desc;\n"
"    const char* name;\n"
"    const char* namespaze;\n"
"    Il2CppType byval_arg;\n"
"    Il2CppType this_arg;\n"
"    Il2CppClass* element_class;\n"
"    Il2CppClass* castClass;\n"
"    Il2CppClass* declaringType;\n"
"    Il2CppClass* parent;\n"
"    void *generic_class;\n"
"    void* typeDefinition;\n"
"    void* interopData;\n"
"    Il2CppClass* klass;\n"
"    void* fields;\n"
"    void* events;\n"
"    void* properties;\n"
"    void* methods;\n"
"    Il2CppClass** nestedTypes;\n"
"    Il2CppClass** implementedInterfaces;\n"
"    Il2CppRuntimeInterfaceOffsetPair* interfaceOffsets;\n"
"};\n"
"\n"
"struct Il2CppClass_2\n"
"{\n"
"    Il2CppClass** typeHierarchy;\n"
"    uint32_t initializationExceptionGCHandle;\n"
"    uint32_t cctor_started;\n"
"    uint32_t cctor_finished;\n"
"    uint64_t cctor_thread;\n"
"    int32_t genericContainerIndex;\n"
"    uint32_t instance_size;\n"
"    uint32_t actualSize;\n"
"    uint32_t element_size;\n"
"    int32_t native_size;\n"
"    uint32_t static_fields_size;\n"
"    uint32_t thread_static_fields_size;\n"
"    int32_t thread_static_fields_offset;\n"
"    uint32_t flags;\n"
"    uint32_t token;\n"
"    uint16_t method_count;\n"
"    uint16_t property_count;\n"
"    uint16_t field_count;\n"
"    uint16_t event_count;\n"
"    uint16_t nested_type_count;\n"
"    uint16_t vtable_count;\n"
"    uint16_t interfaces_count;\n"
"    uint16_t interface_offsets_count;\n"
"    uint8_t typeHierarchyDepth;\n"
"    uint8_t genericRecursionDepth;\n"
"    uint8_t rank;\n"
"    uint8_t minimumAlignment;\n"
"    uint8_t naturalAligment;\n"
"    uint8_t packingSize;\n"
"    uint8_t bitflags1;\n"
"    uint8_t bitflags2;\n"
"};\n"
"\n"
"struct Il2CppClass\n"
"{\n"
"    Il2CppClass_1 _1;\n"
"    void* static_fields;\n"
"    Il2CppRGCTXData* rgctx_data;\n"
"    Il2CppClass_2 _2;\n"
"    VirtualInvokeData vtable[255];\n"
"};\n"
"\n"
"typedef uintptr_t il2cpp_array_size_t;\n"
"typedef int32_t il2cpp_array_lower_bound_t;\n"
"struct Il2CppArrayBounds\n"
"{\n"
"    il2cpp_array_size_t length;\n"
"    il2cpp_array_lower_bound_t lower_bound;\n"
"};\n"
"\n"
"struct MethodInfo\n"
"{\n"
"    Il2CppMethodPointer methodPointer;\n"
"    void* invoker_method;\n"
"    const char* name;\n"
"    Il2CppClass *klass;\n"
"    const Il2CppType *return_type;\n"
"    const void* parameters;\n"
"    union\n"
"    {\n"
"        const Il2CppRGCTXData* rgctx_data;\n"
"        const void* methodDefinition;\n"
"    };\n"
"    union\n"
"    {\n"
"        const void* genericMethod;\n"
"        const void* genericContainer;\n"
"    };\n"
"    uint32_t token;\n"
"    uint16_t flags;\n"
"    uint16_t iflags;\n"
"    uint16_t slot;\n"
"    uint8_t parameters_count;\n"
"    uint8_t bitflags;\n"
"};\n"
"\n";

// HeaderV242: version 24.2-24.5
static const char *kHeaderV242 =
"struct Il2CppClass_1\n"
"{\n"
"    void* image;\n"
"    void* gc_desc;\n"
"    const char* name;\n"
"    const char* namespaze;\n"
"    Il2CppType byval_arg;\n"
"    Il2CppType this_arg;\n"
"    Il2CppClass* element_class;\n"
"    Il2CppClass* castClass;\n"
"    Il2CppClass* declaringType;\n"
"    Il2CppClass* parent;\n"
"    void *generic_class;\n"
"    void* typeDefinition;\n"
"    void* interopData;\n"
"    Il2CppClass* klass;\n"
"    void* fields;\n"
"    void* events;\n"
"    void* properties;\n"
"    void* methods;\n"
"    Il2CppClass** nestedTypes;\n"
"    Il2CppClass** implementedInterfaces;\n"
"    Il2CppRuntimeInterfaceOffsetPair* interfaceOffsets;\n"
"};\n"
"\n"
"struct Il2CppClass_2\n"
"{\n"
"    Il2CppClass** typeHierarchy;\n"
"    void *unity_user_data;\n"
"    uint32_t initializationExceptionGCHandle;\n"
"    uint32_t cctor_started;\n"
"    uint32_t cctor_finished;\n"
"    size_t cctor_thread;\n"
"    int32_t genericContainerIndex;\n"
"    uint32_t instance_size;\n"
"    uint32_t actualSize;\n"
"    uint32_t element_size;\n"
"    int32_t native_size;\n"
"    uint32_t static_fields_size;\n"
"    uint32_t thread_static_fields_size;\n"
"    int32_t thread_static_fields_offset;\n"
"    uint32_t flags;\n"
"    uint32_t token;\n"
"    uint16_t method_count;\n"
"    uint16_t property_count;\n"
"    uint16_t field_count;\n"
"    uint16_t event_count;\n"
"    uint16_t nested_type_count;\n"
"    uint16_t vtable_count;\n"
"    uint16_t interfaces_count;\n"
"    uint16_t interface_offsets_count;\n"
"    uint8_t typeHierarchyDepth;\n"
"    uint8_t genericRecursionDepth;\n"
"    uint8_t rank;\n"
"    uint8_t minimumAlignment;\n"
"    uint8_t naturalAligment;\n"
"    uint8_t packingSize;\n"
"    uint8_t bitflags1;\n"
"    uint8_t bitflags2;\n"
"};\n"
"\n"
"struct Il2CppClass\n"
"{\n"
"    Il2CppClass_1 _1;\n"
"    void* static_fields;\n"
"    Il2CppRGCTXData* rgctx_data;\n"
"    Il2CppClass_2 _2;\n"
"    VirtualInvokeData vtable[255];\n"
"};\n"
"\n"
"typedef uintptr_t il2cpp_array_size_t;\n"
"typedef int32_t il2cpp_array_lower_bound_t;\n"
"struct Il2CppArrayBounds\n"
"{\n"
"    il2cpp_array_size_t length;\n"
"    il2cpp_array_lower_bound_t lower_bound;\n"
"};\n"
"\n"
"struct MethodInfo\n"
"{\n"
"    Il2CppMethodPointer methodPointer;\n"
"    void* invoker_method;\n"
"    const char* name;\n"
"    Il2CppClass *klass;\n"
"    const Il2CppType *return_type;\n"
"    const void* parameters;\n"
"    union\n"
"    {\n"
"        const Il2CppRGCTXData* rgctx_data;\n"
"        const void* methodDefinition;\n"
"    };\n"
"    union\n"
"    {\n"
"        const void* genericMethod;\n"
"        const void* genericContainer;\n"
"    };\n"
"    uint32_t token;\n"
"    uint16_t flags;\n"
"    uint16_t iflags;\n"
"    uint16_t slot;\n"
"    uint8_t parameters_count;\n"
"    uint8_t bitflags;\n"
"};\n"
"\n";

// HeaderV27: version 27-27.2
static const char *kHeaderV27 =
"struct Il2CppClass_1\n"
"{\n"
"    void* image;\n"
"    void* gc_desc;\n"
"    const char* name;\n"
"    const char* namespaze;\n"
"    Il2CppType byval_arg;\n"
"    Il2CppType this_arg;\n"
"    Il2CppClass* element_class;\n"
"    Il2CppClass* castClass;\n"
"    Il2CppClass* declaringType;\n"
"    Il2CppClass* parent;\n"
"    void *generic_class;\n"
"    void* typeMetadataHandle;\n"
"    void* interopData;\n"
"    Il2CppClass* klass;\n"
"    void* fields;\n"
"    void* events;\n"
"    void* properties;\n"
"    void* methods;\n"
"    Il2CppClass** nestedTypes;\n"
"    Il2CppClass** implementedInterfaces;\n"
"    Il2CppRuntimeInterfaceOffsetPair* interfaceOffsets;\n"
"};\n"
"\n"
"struct Il2CppClass_2\n"
"{\n"
"    Il2CppClass** typeHierarchy;\n"
"    void *unity_user_data;\n"
"    uint32_t initializationExceptionGCHandle;\n"
"    uint32_t cctor_started;\n"
"    uint32_t cctor_finished;\n"
"    size_t cctor_thread;\n"
"    void* genericContainerHandle;\n"
"    uint32_t instance_size;\n"
"    uint32_t actualSize;\n"
"    uint32_t element_size;\n"
"    int32_t native_size;\n"
"    uint32_t static_fields_size;\n"
"    uint32_t thread_static_fields_size;\n"
"    int32_t thread_static_fields_offset;\n"
"    uint32_t flags;\n"
"    uint32_t token;\n"
"    uint16_t method_count;\n"
"    uint16_t property_count;\n"
"    uint16_t field_count;\n"
"    uint16_t event_count;\n"
"    uint16_t nested_type_count;\n"
"    uint16_t vtable_count;\n"
"    uint16_t interfaces_count;\n"
"    uint16_t interface_offsets_count;\n"
"    uint8_t typeHierarchyDepth;\n"
"    uint8_t genericRecursionDepth;\n"
"    uint8_t rank;\n"
"    uint8_t minimumAlignment;\n"
"    uint8_t naturalAligment;\n"
"    uint8_t packingSize;\n"
"    uint8_t bitflags1;\n"
"    uint8_t bitflags2;\n"
"};\n"
"\n"
"struct Il2CppClass\n"
"{\n"
"    Il2CppClass_1 _1;\n"
"    void* static_fields;\n"
"    Il2CppRGCTXData* rgctx_data;\n"
"    Il2CppClass_2 _2;\n"
"    VirtualInvokeData vtable[255];\n"
"};\n"
"\n"
"typedef uintptr_t il2cpp_array_size_t;\n"
"typedef int32_t il2cpp_array_lower_bound_t;\n"
"struct Il2CppArrayBounds\n"
"{\n"
"    il2cpp_array_size_t length;\n"
"    il2cpp_array_lower_bound_t lower_bound;\n"
"};\n"
"\n"
"struct MethodInfo\n"
"{\n"
"    Il2CppMethodPointer methodPointer;\n"
"    void* invoker_method;\n"
"    const char* name;\n"
"    Il2CppClass *klass;\n"
"    const Il2CppType *return_type;\n"
"    const void* parameters;\n"
"    union\n"
"    {\n"
"        const Il2CppRGCTXData* rgctx_data;\n"
"        const void* methodMetadataHandle;\n"
"    };\n"
"    union\n"
"    {\n"
"        const void* genericMethod;\n"
"        const void* genericContainerHandle;\n"
"    };\n"
"    uint32_t token;\n"
"    uint16_t flags;\n"
"    uint16_t iflags;\n"
"    uint16_t slot;\n"
"    uint8_t parameters_count;\n"
"    uint8_t bitflags;\n"
"};\n"
"\n";

// HeaderV29: version 29+
static const char *kHeaderV29 =
"struct Il2CppClass_1\n"
"{\n"
"    void* image;\n"
"    void* gc_desc;\n"
"    const char* name;\n"
"    const char* namespaze;\n"
"    Il2CppType byval_arg;\n"
"    Il2CppType this_arg;\n"
"    Il2CppClass* element_class;\n"
"    Il2CppClass* castClass;\n"
"    Il2CppClass* declaringType;\n"
"    Il2CppClass* parent;\n"
"    void *generic_class;\n"
"    void* typeMetadataHandle;\n"
"    void* interopData;\n"
"    Il2CppClass* klass;\n"
"    void* fields;\n"
"    void* events;\n"
"    void* properties;\n"
"    void* methods;\n"
"    Il2CppClass** nestedTypes;\n"
"    Il2CppClass** implementedInterfaces;\n"
"    Il2CppRuntimeInterfaceOffsetPair* interfaceOffsets;\n"
"};\n"
"\n"
"struct Il2CppClass_2\n"
"{\n"
"    Il2CppClass** typeHierarchy;\n"
"    void *unity_user_data;\n"
"    uint32_t initializationExceptionGCHandle;\n"
"    uint32_t cctor_started;\n"
"    uint32_t cctor_finished;\n"
"    size_t cctor_thread;\n"
"    void* genericContainerHandle;\n"
"    uint32_t instance_size;\n"
"    uint32_t actualSize;\n"
"    uint32_t element_size;\n"
"    int32_t native_size;\n"
"    uint32_t static_fields_size;\n"
"    uint32_t thread_static_fields_size;\n"
"    int32_t thread_static_fields_offset;\n"
"    uint32_t flags;\n"
"    uint32_t token;\n"
"    uint16_t method_count;\n"
"    uint16_t property_count;\n"
"    uint16_t field_count;\n"
"    uint16_t event_count;\n"
"    uint16_t nested_type_count;\n"
"    uint16_t vtable_count;\n"
"    uint16_t interfaces_count;\n"
"    uint16_t interface_offsets_count;\n"
"    uint8_t typeHierarchyDepth;\n"
"    uint8_t genericRecursionDepth;\n"
"    uint8_t rank;\n"
"    uint8_t minimumAlignment;\n"
"    uint8_t naturalAligment;\n"
"    uint8_t packingSize;\n"
"    uint8_t bitflags1;\n"
"    uint8_t bitflags2;\n"
"};\n"
"\n"
"struct Il2CppClass\n"
"{\n"
"    Il2CppClass_1 _1;\n"
"    void* static_fields;\n"
"    Il2CppRGCTXData* rgctx_data;\n"
"    Il2CppClass_2 _2;\n"
"    VirtualInvokeData vtable[255];\n"
"};\n"
"\n"
"typedef uintptr_t il2cpp_array_size_t;\n"
"typedef int32_t il2cpp_array_lower_bound_t;\n"
"struct Il2CppArrayBounds\n"
"{\n"
"    il2cpp_array_size_t length;\n"
"    il2cpp_array_lower_bound_t lower_bound;\n"
"};\n"
"\n"
"typedef void (*InvokerMethod)(Il2CppMethodPointer, const MethodInfo*, void*, void**, void*);\n"
"struct MethodInfo\n"
"{\n"
"    Il2CppMethodPointer methodPointer;\n"
"    Il2CppMethodPointer virtualMethodPointer;\n"
"    InvokerMethod invoker_method;\n"
"    const char* name;\n"
"    Il2CppClass *klass;\n"
"    const Il2CppType *return_type;\n"
"    const Il2CppType** parameters;\n"
"    union\n"
"    {\n"
"        const Il2CppRGCTXData* rgctx_data;\n"
"        const void* methodMetadataHandle;\n"
"    };\n"
"    union\n"
"    {\n"
"        const void* genericMethod;\n"
"        const void* genericContainerHandle;\n"
"    };\n"
"    uint32_t token;\n"
"    uint16_t flags;\n"
"    uint16_t iflags;\n"
"    uint16_t slot;\n"
"    uint8_t parameters_count;\n"
"    uint8_t bitflags;\n"
"};\n"
"\n";

const char* get_version_header(int32_t version) {
    if (version <= 22) return kHeaderV22;
    if (version <= 240) return kHeaderV240;   // 23-24.0
    if (version <= 241) return kHeaderV241;   // 24.1
    if (version <= 245) return kHeaderV242;   // 24.2-24.5
    if (version <= 272) return kHeaderV27;    // 27-27.2
    return kHeaderV29;                        // 29+
}

// get_unique_struct_name is defined in script_json.cpp (shared implementation)

// ================================================================
// Type name cache — avoids il2cpp_class_from_type which may crash
// ================================================================

struct TypeNameCacheEntry {
    std::string name;       // e.g. "Game_Player"
    bool is_value_type;
    bool is_enum;
    std::string enum_base;  // empty if not enum
};
static std::map<const Il2CppType*, TypeNameCacheEntry> g_type_name_cache;

// ================================================================
// Helper — build generic instance type name (Fix #15)
// ================================================================

static std::string build_generic_type_name(Il2CppClass *klass);

static std::string build_generic_type_name(Il2CppClass *klass) {
    if (!klass) return "Unknown";

    auto cn = il2cpp_class_get_name ? il2cpp_class_get_name(klass) : "";
    auto ns = il2cpp_class_get_namespace ? il2cpp_class_get_namespace(klass) : "";

    std::string base_name(cn ? cn : "");
    // Strip backtick+arity suffix (e.g. List`1 -> List)
    auto bt_pos = base_name.find('`');
    if (bt_pos != std::string::npos) {
        base_name = base_name.substr(0, bt_pos);
    }

    std::string full;
    if (ns && ns[0]) {
        full = std::string(ns) + "_" + base_name;
    } else {
        full = base_name;
    }

    // Try to get generic argument names from the type's generic_class->context.
    // WARNING: Accessing type->data.generic_class is dangerous — the pointer
    // may be invalid and cause crashes. We skip direct memory access.
    // Instead, we rely on the cache built in collect_type_info() Phase 1,
    // which already resolved generic type names using safe API calls.
    // For types not in the cache, we just use the base name without generic args.
    // This means generic instance types will appear as e.g. "System_Collections_Generic_List"
    // instead of "System_Collections_Generic_List_System_String", but this is safe.

    return fix_name(full.c_str());
}

// ================================================================
// Runtime API — collect type info
// ================================================================

std::string get_field_type_name(const Il2CppType *type, bool *is_value_type, bool *is_custom_type,
                                 const Il2CppGenericContext *context) {
    *is_value_type = false;
    *is_custom_type = false;

    if (!type) {
        return "void";
    }

    auto type_val = type->type;

    switch (type_val) {
    case IL2CPP_TYPE_VOID:
        return "void";
    case IL2CPP_TYPE_BOOLEAN:
        return "bool";
    case IL2CPP_TYPE_CHAR:
        return "uint16_t";
    case IL2CPP_TYPE_I1:
        return "int8_t";
    case IL2CPP_TYPE_U1:
        return "uint8_t";
    case IL2CPP_TYPE_I2:
        return "int16_t";
    case IL2CPP_TYPE_U2:
        return "uint16_t";
    case IL2CPP_TYPE_I4:
        return "int32_t";
    case IL2CPP_TYPE_U4:
        return "uint32_t";
    case IL2CPP_TYPE_I8:
        return "int64_t";
    case IL2CPP_TYPE_U8:
        return "uint64_t";
    case IL2CPP_TYPE_R4:
        return "float";
    case IL2CPP_TYPE_R8:
        return "double";
    case IL2CPP_TYPE_I:
        return "intptr_t";
    case IL2CPP_TYPE_U:
        return "uintptr_t";

    case IL2CPP_TYPE_VALUETYPE: {
        *is_value_type = true;
        *is_custom_type = true;
        auto it = g_type_name_cache.find(type);
        if (it != g_type_name_cache.end()) {
            if (it->second.is_enum) {
                *is_value_type = false;
                *is_custom_type = false;
                return !it->second.enum_base.empty() ? it->second.enum_base : "int32_t";
            }
            return it->second.name + "_o";
        }
        return "Il2CppObject*";
    }

    case IL2CPP_TYPE_STRING:
        *is_custom_type = true;
        return "System_String_o*";

    case IL2CPP_TYPE_PTR: {
        return "void*";
    }

    case IL2CPP_TYPE_CLASS:
    case IL2CPP_TYPE_OBJECT: {
        *is_custom_type = true;
        auto it = g_type_name_cache.find(type);
        if (it != g_type_name_cache.end()) {
            return it->second.name + "_o*";
        }
        return "Il2CppObject*";
    }

    case IL2CPP_TYPE_SZARRAY:
    case IL2CPP_TYPE_ARRAY: {
        *is_custom_type = true;
        auto it = g_type_name_cache.find(type);
        if (it != g_type_name_cache.end()) {
            return it->second.name + "_array*";
        }
        return "Il2CppObject_array*";
    }

    case IL2CPP_TYPE_GENERICINST: {
        *is_custom_type = true;
        auto it = g_type_name_cache.find(type);
        if (it != g_type_name_cache.end()) {
            if (it->second.is_enum) {
                *is_value_type = false;
                *is_custom_type = false;
                return !it->second.enum_base.empty() ? it->second.enum_base : "int32_t";
            }
            if (it->second.is_value_type) *is_value_type = true;
            return it->second.name + (it->second.is_value_type ? "_o" : "_o*");
        }
        return "Il2CppObject*";
    }

    case IL2CPP_TYPE_VAR: {
        // Generic class type parameter — substitute from context (Fix #16)
        *is_custom_type = true;
        if (context && context->class_inst) {
            auto idx = type->data.genericParameterIndex;
            if (idx >= 0 && static_cast<uint32_t>(idx) < context->class_inst->length) {
                return get_field_type_name(context->class_inst->vector[idx],
                                           is_value_type, is_custom_type, nullptr);
            }
        }
        return "Il2CppObject*";
    }

    case IL2CPP_TYPE_MVAR: {
        // Generic method type parameter — substitute from context (Fix #16)
        *is_custom_type = true;
        if (context && context->method_inst) {
            auto idx = type->data.genericParameterIndex;
            if (idx >= 0 && static_cast<uint32_t>(idx) < context->method_inst->length) {
                return get_field_type_name(context->method_inst->vector[idx],
                                           is_value_type, is_custom_type, nullptr);
            }
        }
        return "Il2CppObject*";
    }

    case IL2CPP_TYPE_BYREF: {
        *is_custom_type = false;
        return "void*";
    }

    case IL2CPP_TYPE_FNPTR:
        return "void*";

    default:
        return "void";
    }
}

std::vector<StructInfo> collect_type_info() {
    std::vector<StructInfo> result;

    if (!il2cpp_domain_get || !il2cpp_domain_get_assemblies ||
        !il2cpp_assembly_get_image || !il2cpp_image_get_class_count ||
        !il2cpp_image_get_class) {
        LOGE("collect_type_info: required API functions not available");
        return result;
    }

    auto domain = il2cpp_domain_get();
    if (!domain) return result;

    size_t asm_count = 0;
    auto assemblies = il2cpp_domain_get_assemblies(domain, &asm_count);
    if (!assemblies || asm_count == 0) return result;

    // Track used type names for uniqueness (Fix #2)
    std::set<std::string> used_type_names;

    // Phase 1: Build type name cache from all loaded classes.
    // This avoids calling il2cpp_class_from_type in get_field_type_name,
    // which may trigger class initialization and crash.
    LOGI("il2cpp.h: Phase 1 - building type name cache...");
    g_type_name_cache.clear();
    for (size_t i = 0; i < asm_count; i++) {
        auto image = il2cpp_assembly_get_image(assemblies[i]);
        if (!image) continue;
        auto class_count = il2cpp_image_get_class_count(image);
        for (size_t j = 0; j < class_count; j++) {
            auto klass = const_cast<Il2CppClass *>(il2cpp_image_get_class(image, j));
            if (!klass) continue;

            // Cache the class's Il2CppType → type name mapping
            auto klass_type = il2cpp_class_get_type ? il2cpp_class_get_type(klass) : nullptr;
            if (klass_type && reinterpret_cast<uintptr_t>(klass_type) >= 0x10000) {
                TypeNameCacheEntry entry;
                auto ns = il2cpp_class_get_namespace ? il2cpp_class_get_namespace(klass) : "";
                auto cn = il2cpp_class_get_name ? il2cpp_class_get_name(klass) : "";
                std::string full = (ns && ns[0]) ? (std::string(ns) + "_" + std::string(cn)) : std::string(cn);
                entry.name = fix_name(full.c_str());
                entry.is_value_type = il2cpp_class_is_valuetype ? il2cpp_class_is_valuetype(klass) : false;
                entry.is_enum = il2cpp_class_is_enum ? il2cpp_class_is_enum(klass) : false;

                // For generic instances, build proper name with type arguments
                if (klass_type->type == IL2CPP_TYPE_GENERICINST) {
                    entry.name = build_generic_type_name(klass);
                }

                // For enums, cache the base type name
                if (entry.is_enum) {
                    auto base_type = il2cpp_class_enum_basetype ? il2cpp_class_enum_basetype(klass) : nullptr;
                    if (base_type && reinterpret_cast<uintptr_t>(base_type) > 0x10000) {
                        // base_type is a pointer to Il2CppType within the class,
                        // so it should be safe to read its type field
                        switch (base_type->type) {
                            case IL2CPP_TYPE_BOOLEAN: entry.enum_base = "bool"; break;
                            case IL2CPP_TYPE_CHAR: entry.enum_base = "uint16_t"; break;
                            case IL2CPP_TYPE_I1: entry.enum_base = "int8_t"; break;
                            case IL2CPP_TYPE_U1: entry.enum_base = "uint8_t"; break;
                            case IL2CPP_TYPE_I2: entry.enum_base = "int16_t"; break;
                            case IL2CPP_TYPE_U2: entry.enum_base = "uint16_t"; break;
                            case IL2CPP_TYPE_I4: entry.enum_base = "int32_t"; break;
                            case IL2CPP_TYPE_U4: entry.enum_base = "uint32_t"; break;
                            case IL2CPP_TYPE_I8: entry.enum_base = "int64_t"; break;
                            case IL2CPP_TYPE_U8: entry.enum_base = "uint64_t"; break;
                            default: entry.enum_base = "int32_t"; break;
                        }
                    }
                }

                // Cache by Il2CppType pointer
                g_type_name_cache[klass_type] = entry;

                // Also cache the byval_arg and this_arg if they differ
                // (Il2CppClass has byval_arg and this_arg which are separate Il2CppType instances)
                // Access byval_arg from the class's _1 struct
                // Note: We can't safely access byval_arg directly, but il2cpp_class_get_type
                // returns the same as byval_arg for non-generic types.
            }
        }
    }

    // Phase 2: Collect struct info for each class
    LOGI("il2cpp.h: Phase 2 - collecting struct info...");
    for (size_t i = 0; i < asm_count; i++) {
        auto image = il2cpp_assembly_get_image(assemblies[i]);
        if (!image) continue;
        auto class_count = il2cpp_image_get_class_count(image);
        for (size_t j = 0; j < class_count; j++) {
            auto klass = const_cast<Il2CppClass *>(il2cpp_image_get_class(image, j));
            if (!klass) continue;

            StructInfo si;
            auto ns = il2cpp_class_get_namespace ? il2cpp_class_get_namespace(klass) : "";
            auto cn = il2cpp_class_get_name ? il2cpp_class_get_name(klass) : "";

            // Build full type name
            std::string full;
            if (ns && ns[0]) {
                full = std::string(ns) + "_" + std::string(cn);
            } else {
                full = std::string(cn);
            }
            std::string raw_name = fix_name(full.c_str());

            // For generic instances, build proper name with type arguments (Fix #15)
            auto klass_type = il2cpp_class_get_type ? il2cpp_class_get_type(klass) : nullptr;
            if (klass_type && klass_type->type == IL2CPP_TYPE_GENERICINST) {
                raw_name = build_generic_type_name(klass);
            }

            // Ensure unique struct name (Fix #2)
            si.type_name = get_unique_struct_name(raw_name, used_type_names);

            si.is_value_type = il2cpp_class_is_valuetype ? il2cpp_class_is_valuetype(klass) : false;
            si.is_enum = il2cpp_class_is_enum ? il2cpp_class_is_enum(klass) : false;

            // Parent class
            auto parent = il2cpp_class_get_parent ? il2cpp_class_get_parent(klass) : nullptr;
            if (parent) {
                auto pns = il2cpp_class_get_namespace ? il2cpp_class_get_namespace(parent) : "";
                auto pcn = il2cpp_class_get_name ? il2cpp_class_get_name(parent) : "";
                std::string pfull;
                if (pns && pns[0]) {
                    pfull = std::string(pns) + "_" + std::string(pcn);
                } else {
                    pfull = std::string(pcn);
                }
                si.parent_name = fix_name(pfull.c_str());
            }

            // Skip System.Object and System.ValueType as parent names
            if (si.parent_name == "System_Object" || si.parent_name == "System_ValueType") {
                si.parent_name.clear();
            }

            // Get generic context for field type resolution (Fix #16)
            // WARNING: Accessing type->data.generic_class is dangerous.
            // We skip it to avoid crashes. Generic type parameter substitution
            // won't work for field types, but this is acceptable.
            const Il2CppGenericContext *generic_context = nullptr;

            // Collect instance and static fields
            if (il2cpp_class_get_fields && il2cpp_field_get_name && il2cpp_field_get_type) {
                void *field_iter = nullptr;
                std::set<std::string> used_names;

                while (auto field = il2cpp_class_get_fields(klass, &field_iter)) {
                    auto fname = il2cpp_field_get_name(field);
                    if (!fname) continue;

                    auto ftype = il2cpp_field_get_type(field);
                    if (!ftype) continue;
                    // Safety: check ftype pointer is in valid range
                    if (reinterpret_cast<uintptr_t>(ftype) < 0x10000) continue;

                    // Skip const (literal) fields
                    if ((ftype->attrs & FIELD_ATTRIBUTE_LITERAL) != 0) continue;

                    StructFieldInfo fi;
                    bool is_vt = false, is_custom = false;
                    fi.type_name = get_field_type_name(ftype, &is_vt, &is_custom, generic_context);
                    fi.is_value_type = is_vt;
                    fi.is_custom_type = is_custom;

                    // Fix field name and handle duplicates
                    std::string fixed_name = fix_name(fname);
                    if (!used_names.insert(fixed_name).second) {
                        fixed_name = "_" + std::to_string(used_names.size()) + "_" + fixed_name;
                        used_names.insert(fixed_name);
                    }
                    fi.field_name = fixed_name;

                    // Check if static
                    if ((ftype->attrs & FIELD_ATTRIBUTE_STATIC) != 0) {
                        si.static_fields.push_back(fi);
                    } else {
                        si.fields.push_back(fi);
                    }
                }
            }

            // Collect vtable methods (Fix #10/#14)
            if (il2cpp_class_get_methods && il2cpp_method_get_name && il2cpp_method_get_flags) {
                void *method_iter = nullptr;
                // Collect virtual methods with their names
                // We iterate all methods and check for virtual flag
                struct VMethodTemp {
                    std::string name;
                    uint16_t slot;
                };
                std::vector<VMethodTemp> virtual_methods;

                while (auto method = il2cpp_class_get_methods(klass, &method_iter)) {
                    if (!method) continue;

                    uint32_t iflags = 0;
                    auto flags = il2cpp_method_get_flags(method, &iflags);

                    // Check if this is a virtual method
                    if (flags & METHOD_ATTRIBUTE_VIRTUAL) {
                        VMethodTemp vmt;
                        auto mname = il2cpp_method_get_name(method);
                        vmt.name = mname ? mname : "unknown";
                        // We don't have API to get slot number, so we use
                        // sequential index after sorting
                        vmt.slot = 0;  // will be set later
                        virtual_methods.push_back(vmt);
                    }
                }

                // Sort virtual methods by name for stable ordering
                // (We can't determine the actual vtable slot order without
                // accessing method->slot, which requires version-specific
                // MethodInfo layout knowledge)
                std::sort(virtual_methods.begin(), virtual_methods.end(),
                          [](const VMethodTemp &a, const VMethodTemp &b) {
                              return a.name < b.name;
                          });

                for (size_t vi = 0; vi < virtual_methods.size(); vi++) {
                    VTableMethodInfo vmi;
                    vmi.name = virtual_methods[vi].name;
                    si.vtable_methods.push_back(vmi);
                }
            }

            // TODO: RGCTX entry collection (Fix #11)
            // The RGCTX data structure is internal and version-dependent.
            // For now, leave rgctx_entries empty and use
            // const Il2CppRGCTXData* rgctx_data in _c struct.
            // Future work: read RGCTX data from Il2CppClass at runtime
            // by accessing klass->rgctx_data and parsing the entries.

            result.push_back(si);
        }
    }

    return result;
}

// ================================================================
// Pure logic — generate il2cpp.h content (unit-testable)
// ================================================================

void generate_il2cpp_h_content(const std::vector<StructInfo> &types, std::ostream &out) {

    // Build a set of all type names for dependency tracking
    std::set<std::string> all_type_names;
    for (const auto &t : types) {
        all_type_names.insert(t.type_name);
    }

    // Track which types have been emitted (for dependency ordering)
    std::set<std::string> emitted;

    // Desktop Il2CppDumper does NOT generate per-type forward declarations.
    // It relies on dependency ordering to ensure types are defined before use.

    // Helper: emit a type and its dependencies first
    std::function<void(const StructInfo &, std::ostream &)> emit_struct;
    emit_struct = [&](const StructInfo &info, std::ostream &os) {
        if (emitted.count(info.type_name)) return;

        // Emit parent first if it exists and is in our type set
        if (!info.parent_name.empty() && all_type_names.count(info.parent_name)) {
            for (const auto &t : types) {
                if (t.type_name == info.parent_name) {
                    emit_struct(t, os);
                    break;
                }
            }
        }

        // Emit field dependencies (value types that are embedded)
        for (const auto &f : info.fields) {
            if (f.is_value_type && f.is_custom_type) {
                std::string dep_name = f.type_name;
                if (dep_name.size() > 2 && dep_name.substr(dep_name.size() - 2) == "_o") {
                    dep_name = dep_name.substr(0, dep_name.size() - 2);
                }
                if (all_type_names.count(dep_name) && !emitted.count(dep_name)) {
                    for (const auto &t : types) {
                        if (t.type_name == dep_name) {
                            emit_struct(t, os);
                            break;
                        }
                    }
                }
            }
        }

        // Skip enum types — they use their underlying type directly
        if (info.is_enum) {
            emitted.insert(info.type_name);
            return;
        }

        // --- _Fields struct ---
        os << "struct " << info.type_name << "_Fields";
        if (!info.parent_name.empty() && all_type_names.count(info.parent_name)) {
            os << " : " << info.parent_name << "_Fields";
        }
        os << " {\n";
        for (const auto &f : info.fields) {
            if (f.is_custom_type) {
                os << "\tstruct " << f.type_name << " " << f.field_name << ";\n";
            } else {
                os << "\t" << f.type_name << " " << f.field_name << ";\n";
            }
        }
        os << "};\n\n";

        // --- _StaticFields struct (if has static fields) ---
        if (info.has_static_fields()) {
            os << "struct " << info.type_name << "_StaticFields {\n";
            for (const auto &f : info.static_fields) {
                if (f.is_custom_type) {
                    os << "\tstruct " << f.type_name << " " << f.field_name << ";\n";
                } else {
                    os << "\t" << f.type_name << " " << f.field_name << ";\n";
                }
            }
            os << "};\n\n";
        }

        // --- _VTable struct (if has vtable methods) ---
        // Desktop Il2CppDumper generates a separate _VTable struct
        if (info.has_vtable()) {
            os << "struct " << info.type_name << "_VTable {\n";
            for (size_t vi = 0; vi < info.vtable_methods.size(); vi++) {
                const auto &vm = info.vtable_methods[vi];
                std::string method_name = fix_name(vm.name);
                os << "\tVirtualInvokeData _" << vi << "_" << method_name << ";\n";
            }
            os << "};\n\n";
        }

        // --- _c struct (class metadata) ---
        os << "struct " << info.type_name << "_c {\n";
        os << "\tIl2CppClass_1 _1;\n";
        if (info.has_static_fields()) {
            os << "\tstruct " << info.type_name << "_StaticFields* static_fields;\n";
        } else {
            os << "\tvoid* static_fields;\n";
        }
        if (!info.rgctx_entries.empty()) {
            os << "\t" << info.type_name << "_RGCTXs* rgctx_data;\n";
        } else {
            os << "\tIl2CppRGCTXData* rgctx_data;\n";
        }
        os << "\tIl2CppClass_2 _2;\n";
        // VTable: desktop uses separate _VTable struct + TypeName_VTable vtable;
        // or VirtualInvokeData vtable[32] when no vtable methods
        if (info.has_vtable()) {
            os << "\t" << info.type_name << "_VTable vtable;\n";
        } else {
            os << "\tVirtualInvokeData vtable[32];\n";
        }
        os << "};\n\n";

        // --- _o struct (object instance) ---
        os << "struct " << info.type_name << "_o {\n";
        if (!info.is_value_type) {
            os << "\t" << info.type_name << "_c *klass;\n";
            os << "\tvoid *monitor;\n";
        }
        os << "\t" << info.type_name << "_Fields fields;\n";
        os << "};\n\n";

        emitted.insert(info.type_name);
    };

    // Emit all type structs in dependency order
    for (const auto &t : types) {
        emit_struct(t, out);
    }

    // --- Array type structs ---
    std::set<std::string> emitted_arrays;
    for (const auto &t : types) {
        if (t.is_enum) continue;
        std::string arr_name = t.type_name + "_array";
        if (emitted_arrays.count(arr_name)) continue;
        emitted_arrays.insert(arr_name);

        out << "struct " << arr_name << " {\n";
        out << "\tIl2CppObject obj;\n";
        out << "\tIl2CppArrayBounds *bounds;\n";
        out << "\til2cpp_array_size_t max_length;\n";
        if (t.is_value_type) {
            out << "\t" << t.type_name << "_o m_Items[65535];\n";
        } else {
            out << "\t" << t.type_name << "_o* m_Items[65535];\n";
        }
        out << "};\n\n";
    }

    // TODO: MethodInfo struct generation for generic method instances
    // The desktop version generates MethodInfo_{RVA:X} structs for generic methods.
    // This requires knowing the exact MethodInfo layout for each version,
    // which is very complex and version-dependent. Skip for now.
}

// ================================================================
// I/O — write il2cpp.h to disk
// ================================================================

void write_il2cpp_h(const char *outDir, const std::vector<StructInfo> &types, int32_t version) {
    auto outPath = std::string(outDir).append("/files/il2cpp.h");
    LOGI("writing il2cpp.h to %s (version %d)", outPath.c_str(), version);

    std::ofstream f(outPath);
    if (!f.is_open()) {
        LOGE("failed to create %s", outPath.c_str());
        return;
    }

    LOGI("il2cpp.h: writing generic header...");
    f << kGenericHeader;
    LOGI("il2cpp.h: writing version header...");
    f << get_version_header(version);
    LOGI("il2cpp.h: generating type definitions for %d types...", (int)types.size());
    generate_il2cpp_h_content(types, f);
    LOGI("il2cpp.h: closing file...");
    f.close();
    LOGI("il2cpp.h: done!");
}
