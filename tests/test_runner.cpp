//
// test_runner.cpp — Entry point for script_json unit tests
//

#include <cstdio>
#include <cstdlib>

// Test function declarations
int test_fix_name();
int test_escape_json();
int test_type_signature();
int test_json_output();
int test_extract_strings();
int test_metadata_from_blob();
int test_metadata_json_output();
int test_generate_il2cpp_h_empty();
int test_generate_il2cpp_h_simple_class();
int test_generate_il2cpp_h_value_type();
int test_generate_il2cpp_h_enum();
int test_generate_il2cpp_h_inheritance();
int test_generate_il2cpp_h_static_fields();

int main() {
    int failures = 0;

    printf("=== script_json unit tests ===\n\n");

    printf("--- test_fix_name ---\n");
    failures += test_fix_name();

    printf("--- test_escape_json ---\n");
    failures += test_escape_json();

    printf("--- test_type_signature ---\n");
    failures += test_type_signature();

    printf("--- test_json_output ---\n");
    failures += test_json_output();

    printf("--- test_extract_strings ---\n");
    failures += test_extract_strings();

    printf("--- test_metadata_from_blob ---\n");
    failures += test_metadata_from_blob();

    printf("--- test_metadata_json_output ---\n");
    failures += test_metadata_json_output();

    printf("--- test_generate_il2cpp_h_empty ---\n");
    failures += test_generate_il2cpp_h_empty();

    printf("--- test_generate_il2cpp_h_simple_class ---\n");
    failures += test_generate_il2cpp_h_simple_class();

    printf("--- test_generate_il2cpp_h_value_type ---\n");
    failures += test_generate_il2cpp_h_value_type();

    printf("--- test_generate_il2cpp_h_enum ---\n");
    failures += test_generate_il2cpp_h_enum();

    printf("--- test_generate_il2cpp_h_inheritance ---\n");
    failures += test_generate_il2cpp_h_inheritance();

    printf("--- test_generate_il2cpp_h_static_fields ---\n");
    failures += test_generate_il2cpp_h_static_fields();

    printf("\n=== %s (failures: %d) ===\n",
           failures == 0 ? "ALL PASSED" : "SOME FAILED", failures);

    return failures;
}
