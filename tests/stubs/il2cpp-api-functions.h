#pragma once
// Stub for il2cpp-api-functions.h — provides function pointer DEFINITIONS for testing
//
// script_json.cpp uses extern declarations for these pointers.
// This stub provides the actual definitions (= nullptr) so the test binary links.

#include "il2cpp-class.h"  // for Il2CppType, Il2CppClass, MethodInfo etc.

#ifndef DO_API
#define DO_API(r, n, p) r (*n) p = nullptr
#endif

#ifndef DO_API_NO_RETURN
#define DO_API_NO_RETURN(r, n, p) DO_API(r, n, p)
#endif

// Only define the APIs that script_json.cpp actually calls
DO_API(const char*, il2cpp_class_get_name, (Il2CppClass * klass));
DO_API(const char*, il2cpp_class_get_namespace, (Il2CppClass * klass));
DO_API(Il2CppClass*, il2cpp_class_from_type, (const Il2CppType * type));
DO_API(Il2CppClass*, il2cpp_class_get_parent, (Il2CppClass * klass));
DO_API(const Il2CppType*, il2cpp_class_get_type, (Il2CppClass * klass));
DO_API(Il2CppClass*, il2cpp_class_get_element_class, (Il2CppClass * klass));
DO_API(bool, il2cpp_class_is_valuetype, (const Il2CppClass * klass));
DO_API(bool, il2cpp_class_is_enum, (const Il2CppClass * klass));
DO_API(const Il2CppType*, il2cpp_class_enum_basetype, (Il2CppClass * klass));
DO_API(const char*, il2cpp_method_get_name, (const MethodInfo * method));
DO_API(const Il2CppType*, il2cpp_method_get_return_type, (const MethodInfo * method));
DO_API(uint32_t, il2cpp_method_get_param_count, (const MethodInfo * method));
DO_API(const Il2CppType*, il2cpp_method_get_param, (const MethodInfo * method, uint32_t index));
DO_API(const char*, il2cpp_method_get_param_name, (const MethodInfo * method, uint32_t index));
DO_API(uint32_t, il2cpp_method_get_flags, (const MethodInfo * method, uint32_t * iflags));
DO_API(bool, il2cpp_type_is_byref, (const Il2CppType * type));

// Phase 3 additional APIs
DO_API(FieldInfo*, il2cpp_class_get_fields, (Il2CppClass * klass, void* *iter));
DO_API(const char*, il2cpp_field_get_name, (FieldInfo * field));
DO_API(const Il2CppType*, il2cpp_field_get_type, (FieldInfo * field));
DO_API(Il2CppDomain*, il2cpp_domain_get, ());
DO_API(const Il2CppAssembly**, il2cpp_domain_get_assemblies, (const Il2CppDomain * domain, size_t * size));
DO_API(const Il2CppImage*, il2cpp_assembly_get_image, (const Il2CppAssembly * assembly));
DO_API(const char*, il2cpp_image_get_name, (const Il2CppImage * image));
DO_API(size_t, il2cpp_image_get_class_count, (const Il2CppImage * image));
DO_API(const Il2CppClass*, il2cpp_image_get_class, (const Il2CppImage * image, size_t index));
DO_API(const MethodInfo*, il2cpp_class_get_methods, (Il2CppClass * klass, void* *iter));

#undef DO_API
#undef DO_API_NO_RETURN
