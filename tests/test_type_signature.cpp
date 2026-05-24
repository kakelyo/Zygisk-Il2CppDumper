//
// test_type_signature.cpp — Unit tests for type_to_signature_char()
//

#include <cassert>
#include <cstring>
#include <cstdio>
#include <string>

#include "script_json.h"
#include "il2cpp-class.h"

int test_type_signature() {
    int fail = 0;

    // Helper: create a minimal Il2CppType on the stack
    auto make_type = [](Il2CppTypeEnum t, bool byref = false) -> Il2CppType {
        Il2CppType type;
        memset(&type, 0, sizeof(type));
        type.type = t;
        type.byref = byref;
        return type;
    };

    // --- type_to_signature_char ---

    // void → 'v'
    {
        auto t = make_type(IL2CPP_TYPE_VOID);
        if (type_to_signature_char(&t) != 'v') {
            printf("  FAIL: void → expected 'v'\n"); fail++;
        } else { printf("  OK: void → 'v'\n"); }
    }

    // float → 'f'
    {
        auto t = make_type(IL2CPP_TYPE_R4);
        if (type_to_signature_char(&t) != 'f') {
            printf("  FAIL: float → expected 'f'\n"); fail++;
        } else { printf("  OK: float → 'f'\n"); }
    }

    // double → 'd'
    {
        auto t = make_type(IL2CPP_TYPE_R8);
        if (type_to_signature_char(&t) != 'd') {
            printf("  FAIL: double → expected 'd'\n"); fail++;
        } else { printf("  OK: double → 'd'\n"); }
    }

    // int64 → 'j'
    {
        auto t = make_type(IL2CPP_TYPE_I8);
        if (type_to_signature_char(&t) != 'j') {
            printf("  FAIL: int64 → expected 'j'\n"); fail++;
        } else { printf("  OK: int64 → 'j'\n"); }
    }

    // uint64 → 'j'
    {
        auto t = make_type(IL2CPP_TYPE_U8);
        if (type_to_signature_char(&t) != 'j') {
            printf("  FAIL: uint64 → expected 'j'\n"); fail++;
        } else { printf("  OK: uint64 → 'j'\n"); }
    }

    // int32 → 'i' (default for integers)
    {
        auto t = make_type(IL2CPP_TYPE_I4);
        if (type_to_signature_char(&t) != 'i') {
            printf("  FAIL: int32 → expected 'i'\n"); fail++;
        } else { printf("  OK: int32 → 'i'\n"); }
    }

    // bool → 'i' (default)
    {
        auto t = make_type(IL2CPP_TYPE_BOOLEAN);
        if (type_to_signature_char(&t) != 'i') {
            printf("  FAIL: bool → expected 'i'\n"); fail++;
        } else { printf("  OK: bool → 'i'\n"); }
    }

    // string → 'i' (reference type)
    {
        auto t = make_type(IL2CPP_TYPE_STRING);
        if (type_to_signature_char(&t) != 'i') {
            printf("  FAIL: string → expected 'i'\n"); fail++;
        } else { printf("  OK: string → 'i'\n"); }
    }

    // class → 'i' (reference type)
    {
        auto t = make_type(IL2CPP_TYPE_CLASS);
        if (type_to_signature_char(&t) != 'i') {
            printf("  FAIL: class → expected 'i'\n"); fail++;
        } else { printf("  OK: class → 'i'\n"); }
    }

    // null type → 'i' (fallback)
    {
        if (type_to_signature_char(nullptr) != 'i') {
            printf("  FAIL: null → expected 'i'\n"); fail++;
        } else { printf("  OK: null → 'i' (fallback)\n"); }
    }

    // --- parse_type (basic types, no il2cpp runtime needed) ---
    // These only use the type enum, not API calls

    {
        auto t = make_type(IL2CPP_TYPE_VOID);
        auto r = parse_type(&t);
        if (r != "void") {
            printf("  FAIL: parse_type(void) = \"%s\"\n", r.c_str()); fail++;
        } else { printf("  OK: parse_type(void) → \"void\"\n"); }
    }

    {
        auto t = make_type(IL2CPP_TYPE_BOOLEAN);
        auto r = parse_type(&t);
        if (r != "bool") {
            printf("  FAIL: parse_type(bool) = \"%s\"\n", r.c_str()); fail++;
        } else { printf("  OK: parse_type(bool) → \"bool\"\n"); }
    }

    {
        auto t = make_type(IL2CPP_TYPE_CHAR);
        auto r = parse_type(&t);
        if (r != "uint16_t") {
            printf("  FAIL: parse_type(char) = \"%s\"\n", r.c_str()); fail++;
        } else { printf("  OK: parse_type(char) → \"uint16_t\"\n"); }
    }

    {
        auto t = make_type(IL2CPP_TYPE_I4);
        auto r = parse_type(&t);
        if (r != "int32_t") {
            printf("  FAIL: parse_type(int32) = \"%s\"\n", r.c_str()); fail++;
        } else { printf("  OK: parse_type(int32) → \"int32_t\"\n"); }
    }

    {
        auto t = make_type(IL2CPP_TYPE_U4);
        auto r = parse_type(&t);
        if (r != "uint32_t") {
            printf("  FAIL: parse_type(uint32) = \"%s\"\n", r.c_str()); fail++;
        } else { printf("  OK: parse_type(uint32) → \"uint32_t\"\n"); }
    }

    {
        auto t = make_type(IL2CPP_TYPE_I8);
        auto r = parse_type(&t);
        if (r != "int64_t") {
            printf("  FAIL: parse_type(int64) = \"%s\"\n", r.c_str()); fail++;
        } else { printf("  OK: parse_type(int64) → \"int64_t\"\n"); }
    }

    {
        auto t = make_type(IL2CPP_TYPE_R4);
        auto r = parse_type(&t);
        if (r != "float") {
            printf("  FAIL: parse_type(float) = \"%s\"\n", r.c_str()); fail++;
        } else { printf("  OK: parse_type(float) → \"float\"\n"); }
    }

    {
        auto t = make_type(IL2CPP_TYPE_R8);
        auto r = parse_type(&t);
        if (r != "double") {
            printf("  FAIL: parse_type(double) = \"%s\"\n", r.c_str()); fail++;
        } else { printf("  OK: parse_type(double) → \"double\"\n"); }
    }

    {
        auto t = make_type(IL2CPP_TYPE_STRING);
        auto r = parse_type(&t);
        if (r != "System_String_o*") {
            printf("  FAIL: parse_type(string) = \"%s\"\n", r.c_str()); fail++;
        } else { printf("  OK: parse_type(string) → \"System_String_o*\"\n"); }
    }

    {
        auto t = make_type(IL2CPP_TYPE_OBJECT);
        auto r = parse_type(&t);
        if (r != "Il2CppObject*") {
            printf("  FAIL: parse_type(object) = \"%s\"\n", r.c_str()); fail++;
        } else { printf("  OK: parse_type(object) → \"Il2CppObject*\"\n"); }
    }

    {
        auto t = make_type(IL2CPP_TYPE_I);
        auto r = parse_type(&t);
        if (r != "intptr_t") {
            printf("  FAIL: parse_type(intptr) = \"%s\"\n", r.c_str()); fail++;
        } else { printf("  OK: parse_type(intptr) → \"intptr_t\"\n"); }
    }

    {
        auto t = make_type(IL2CPP_TYPE_U);
        auto r = parse_type(&t);
        if (r != "uintptr_t") {
            printf("  FAIL: parse_type(uintptr) = \"%s\"\n", r.c_str()); fail++;
        } else { printf("  OK: parse_type(uintptr) → \"uintptr_t\"\n"); }
    }

    // byref adds *
    {
        auto t = make_type(IL2CPP_TYPE_I4, true);
        auto r = parse_type(&t);
        if (r != "int32_t*") {
            printf("  FAIL: parse_type(int32&) = \"%s\"\n", r.c_str()); fail++;
        } else { printf("  OK: parse_type(int32&) → \"int32_t*\"\n"); }
    }

    // null type → "void"
    {
        auto r = parse_type(nullptr);
        if (r != "void") {
            printf("  FAIL: parse_type(null) = \"%s\"\n", r.c_str()); fail++;
        } else { printf("  OK: parse_type(null) → \"void\"\n"); }
    }

    // IL2CPP_TYPE_PTR → recursive element type + "*"
    {
        Il2CppType elem;
        memset(&elem, 0, sizeof(elem));
        elem.type = IL2CPP_TYPE_I4;

        Il2CppType ptr_type;
        memset(&ptr_type, 0, sizeof(ptr_type));
        ptr_type.type = IL2CPP_TYPE_PTR;
        ptr_type.data.type = &elem;

        auto r = parse_type(&ptr_type);
        if (r != "int32_t*") {
            printf("  FAIL: parse_type(int32*) = \"%s\"\n", r.c_str()); fail++;
        } else { printf("  OK: parse_type(int32*) → \"int32_t*\"\n"); }
    }

    // IL2CPP_TYPE_TYPEDBYREF → "Il2CppObject*"
    {
        auto t = make_type(IL2CPP_TYPE_TYPEDBYREF);
        auto r = parse_type(&t);
        if (r != "Il2CppObject*") {
            printf("  FAIL: parse_type(typedbyref) = \"%s\"\n", r.c_str()); fail++;
        } else { printf("  OK: parse_type(typedbyref) → \"Il2CppObject*\"\n"); }
    }

    // --- get_unique_struct_name ---
    {
        std::set<std::string> names;
        auto r1 = get_unique_struct_name("Foo", names);
        auto r2 = get_unique_struct_name("Foo", names);
        auto r3 = get_unique_struct_name("Bar", names);
        if (r1 != "Foo") {
            printf("  FAIL: first Foo = \"%s\"\n", r1.c_str()); fail++;
        } else { printf("  OK: first Foo → \"Foo\"\n"); }
        if (r2 != "Foo_1") {
            printf("  FAIL: second Foo = \"%s\"\n", r2.c_str()); fail++;
        } else { printf("  OK: second Foo → \"Foo_1\"\n"); }
        if (r3 != "Bar") {
            printf("  FAIL: Bar = \"%s\"\n", r3.c_str()); fail++;
        } else { printf("  OK: Bar → \"Bar\"\n"); }
    }

    // --- set_metadata_version ---
    {
        set_metadata_version(24);
        // Version is set, no crash = OK
        printf("  OK: set_metadata_version(24) no crash\n");
    }

    printf("  type_signature: %d failures\n", fail);
    return fail;
}
