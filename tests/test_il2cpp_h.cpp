//
// test_il2cpp_h.cpp — Unit tests for Phase 4 il2cpp.h generation
//

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "il2cpp_h.h"

int test_generate_il2cpp_h_empty() {
    int fail = 0;

    std::vector<StructInfo> types;
    auto content = generate_il2cpp_h_content(types);

    if (content.empty()) {
        printf("  FAIL: empty types should still produce content\n"); fail++;
    } else { printf("  OK: empty types produces content\n"); }

    // Should have forward declarations section
    if (content.find("// Forward declarations") == std::string::npos) {
        printf("  FAIL: missing forward declarations section\n"); fail++;
    } else { printf("  OK: forward declarations section present\n"); }

    printf("  test_generate_il2cpp_h_empty: %d failures\n", fail);
    return fail;
}

int test_generate_il2cpp_h_simple_class() {
    int fail = 0;

    std::vector<StructInfo> types;

    // Simple class with no parent, one int field
    StructInfo player;
    player.type_name = "Game_Player";
    player.is_value_type = false;
    player.is_enum = false;
    player.parent_name = "";

    StructFieldInfo health;
    health.type_name = "int32_t";
    health.field_name = "health";
    health.is_value_type = false;
    health.is_custom_type = false;
    player.fields.push_back(health);

    StructFieldInfo name_field;
    name_field.type_name = "System_String_o*";
    name_field.field_name = "playerName";
    name_field.is_value_type = false;
    name_field.is_custom_type = true;
    player.fields.push_back(name_field);

    types.push_back(player);

    auto content = generate_il2cpp_h_content(types);

    // Check _Fields struct
    if (content.find("struct Game_Player_Fields {") == std::string::npos) {
        printf("  FAIL: _Fields struct not found\n"); fail++;
    } else { printf("  OK: _Fields struct found\n"); }

    // Check field declarations
    if (content.find("int32_t health;") == std::string::npos) {
        printf("  FAIL: int32_t health field not found\n"); fail++;
    } else { printf("  OK: int32_t health field found\n"); }

    if (content.find("struct System_String_o* playerName;") == std::string::npos) {
        printf("  FAIL: struct System_String_o* playerName not found\n"); fail++;
    } else { printf("  OK: custom type field with struct prefix found\n"); }

    // Check _c struct
    if (content.find("struct Game_Player_c {") == std::string::npos) {
        printf("  FAIL: _c struct not found\n"); fail++;
    } else { printf("  OK: _c struct found\n"); }

    // Check _o struct
    if (content.find("struct Game_Player_o {") == std::string::npos) {
        printf("  FAIL: _o struct not found\n"); fail++;
    } else { printf("  OK: _o struct found\n"); }

    // Check klass and monitor in _o (reference type)
    if (content.find("Game_Player_c *klass;") == std::string::npos) {
        printf("  FAIL: klass field not found in _o\n"); fail++;
    } else { printf("  OK: klass field in _o\n"); }

    if (content.find("void *monitor;") == std::string::npos) {
        printf("  FAIL: monitor field not found in _o\n"); fail++;
    } else { printf("  OK: monitor field in _o\n"); }

    // Check array type
    if (content.find("struct Game_Player_array {") == std::string::npos) {
        printf("  FAIL: _array struct not found\n"); fail++;
    } else { printf("  OK: _array struct found\n"); }

    if (content.find("Game_Player_o* m_Items[65535];") == std::string::npos) {
        printf("  FAIL: m_Items pointer array not found\n"); fail++;
    } else { printf("  OK: m_Items pointer array found\n"); }

    printf("  test_generate_il2cpp_h_simple_class: %d failures\n", fail);
    return fail;
}

int test_generate_il2cpp_h_value_type() {
    int fail = 0;

    std::vector<StructInfo> types;

    // Value type (struct in C#)
    StructInfo vec3;
    vec3.type_name = "UnityEngine_Vector3";
    vec3.is_value_type = true;
    vec3.is_enum = false;
    vec3.parent_name = "";

    StructFieldInfo x;
    x.type_name = "float";
    x.field_name = "x";
    x.is_value_type = false;
    x.is_custom_type = false;
    vec3.fields.push_back(x);

    StructFieldInfo y;
    y.type_name = "float";
    y.field_name = "y";
    y.is_value_type = false;
    y.is_custom_type = false;
    vec3.fields.push_back(y);

    StructFieldInfo z;
    z.type_name = "float";
    z.field_name = "z";
    z.is_value_type = false;
    z.is_custom_type = false;
    vec3.fields.push_back(z);

    types.push_back(vec3);

    auto content = generate_il2cpp_h_content(types);

    // Value type _o should NOT have klass and monitor
    if (content.find("UnityEngine_Vector3_c *klass;") != std::string::npos) {
        printf("  FAIL: value type _o should not have klass\n"); fail++;
    } else { printf("  OK: value type _o has no klass\n"); }

    // Check fields
    if (content.find("float x;") == std::string::npos) {
        printf("  FAIL: float x field not found\n"); fail++;
    } else { printf("  OK: float x field found\n"); }

    // Array for value type should embed the value, not pointer
    if (content.find("UnityEngine_Vector3_o m_Items[65535];") == std::string::npos) {
        printf("  FAIL: value type array should embed _o, not _o*\n"); fail++;
    } else { printf("  OK: value type array embeds _o\n"); }

    printf("  test_generate_il2cpp_h_value_type: %d failures\n", fail);
    return fail;
}

int test_generate_il2cpp_h_enum() {
    int fail = 0;

    std::vector<StructInfo> types;

    // Enum type — should NOT generate struct definitions
    StructInfo color;
    color.type_name = "Game_Color";
    color.is_value_type = true;
    color.is_enum = true;
    color.parent_name = "";

    StructFieldInfo red;
    red.type_name = "int32_t";
    red.field_name = "Red";
    red.is_value_type = false;
    red.is_custom_type = false;
    color.fields.push_back(red);

    types.push_back(color);

    auto content = generate_il2cpp_h_content(types);

    // Enum should NOT have _Fields struct
    if (content.find("struct Game_Color_Fields {") != std::string::npos) {
        printf("  FAIL: enum should not have _Fields struct\n"); fail++;
    } else { printf("  OK: enum has no _Fields struct\n"); }

    // Enum should NOT have _o struct
    if (content.find("struct Game_Color_o {") != std::string::npos) {
        printf("  FAIL: enum should not have _o struct\n"); fail++;
    } else { printf("  OK: enum has no _o struct\n"); }

    // Enum should NOT have _array struct
    if (content.find("struct Game_Color_array {") != std::string::npos) {
        printf("  FAIL: enum should not have _array struct\n"); fail++;
    } else { printf("  OK: enum has no _array struct\n"); }

    printf("  test_generate_il2cpp_h_enum: %d failures\n", fail);
    return fail;
}

int test_generate_il2cpp_h_inheritance() {
    int fail = 0;

    std::vector<StructInfo> types;

    // Parent class
    StructInfo base;
    base.type_name = "Game_Entity";
    base.is_value_type = false;
    base.is_enum = false;
    base.parent_name = "";

    StructFieldInfo id;
    id.type_name = "int32_t";
    id.field_name = "id";
    id.is_value_type = false;
    id.is_custom_type = false;
    base.fields.push_back(id);

    types.push_back(base);

    // Child class
    StructInfo child;
    child.type_name = "Game_Player";
    child.is_value_type = false;
    child.is_enum = false;
    child.parent_name = "Game_Entity";

    StructFieldInfo score;
    score.type_name = "int32_t";
    score.field_name = "score";
    score.is_value_type = false;
    score.is_custom_type = false;
    child.fields.push_back(score);

    types.push_back(child);

    auto content = generate_il2cpp_h_content(types);

    // Check inheritance in _Fields
    if (content.find("struct Game_Player_Fields : Game_Entity_Fields {") == std::string::npos) {
        printf("  FAIL: _Fields inheritance not found\n"); fail++;
    } else { printf("  OK: _Fields inheritance found\n"); }

    // Parent _Fields should be emitted before child
    auto parent_pos = content.find("struct Game_Entity_Fields {");
    auto child_pos = content.find("struct Game_Player_Fields : Game_Entity_Fields {");
    if (parent_pos == std::string::npos || child_pos == std::string::npos) {
        printf("  FAIL: parent or child _Fields not found\n"); fail++;
    } else if (parent_pos >= child_pos) {
        printf("  FAIL: parent _Fields should come before child\n"); fail++;
    } else { printf("  OK: parent _Fields emitted before child\n"); }

    printf("  test_generate_il2cpp_h_inheritance: %d failures\n", fail);
    return fail;
}

int test_generate_il2cpp_h_static_fields() {
    int fail = 0;

    std::vector<StructInfo> types;

    StructInfo mgr;
    mgr.type_name = "Game_Manager";
    mgr.is_value_type = false;
    mgr.is_enum = false;
    mgr.parent_name = "";

    // Instance field
    StructFieldInfo instance_field;
    instance_field.type_name = "int32_t";
    instance_field.field_name = "count";
    instance_field.is_value_type = false;
    instance_field.is_custom_type = false;
    mgr.fields.push_back(instance_field);

    // Static field
    StructFieldInfo static_field;
    static_field.type_name = "Game_Manager_o*";
    static_field.field_name = "Instance";
    static_field.is_value_type = false;
    static_field.is_custom_type = true;
    mgr.static_fields.push_back(static_field);

    types.push_back(mgr);

    auto content = generate_il2cpp_h_content(types);

    // Check _StaticFields struct
    if (content.find("struct Game_Manager_StaticFields {") == std::string::npos) {
        printf("  FAIL: _StaticFields struct not found\n"); fail++;
    } else { printf("  OK: _StaticFields struct found\n"); }

    // Check static field in _StaticFields
    if (content.find("struct Game_Manager_o* Instance;") == std::string::npos) {
        printf("  FAIL: static field not found in _StaticFields\n"); fail++;
    } else { printf("  OK: static field in _StaticFields\n"); }

    // Check _c references _StaticFields
    if (content.find("Game_Manager_StaticFields* static_fields;") == std::string::npos) {
        printf("  FAIL: _c should reference _StaticFields*\n"); fail++;
    } else { printf("  OK: _c references _StaticFields*\n"); }

    printf("  test_generate_il2cpp_h_static_fields: %d failures\n", fail);
    return fail;
}
