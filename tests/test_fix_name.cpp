//
// test_fix_name.cpp — Unit tests for fix_name()
//

#include <cassert>
#include <cstdio>
#include <string>

#include "script_json.h"

int test_fix_name() {
    int fail = 0;

    // Normal names pass through unchanged
    if (fix_name("Player") != "Player") {
        printf("  FAIL: fix_name(\"Player\") = \"%s\", expected \"Player\"\n", fix_name("Player").c_str());
        fail++;
    } else { printf("  OK: normal name unchanged\n"); }

    if (fix_name("Player_TakeDamage") != "Player_TakeDamage") {
        printf("  FAIL: fix_name(\"Player_TakeDamage\")\n");
        fail++;
    } else { printf("  OK: underscore name unchanged\n"); }

    // C++ keywords get prefixed
    if (fix_name("class") != "_class") {
        printf("  FAIL: fix_name(\"class\") = \"%s\", expected \"_class\"\n", fix_name("class").c_str());
        fail++;
    } else { printf("  OK: keyword 'class' → _class\n"); }

    if (fix_name("register") != "_register") {
        printf("  FAIL: fix_name(\"register\") = \"%s\", expected \"_register\"\n", fix_name("register").c_str());
        fail++;
    } else { printf("  OK: keyword 'register' → _register\n"); }

    if (fix_name("return") != "_return") {
        printf("  FAIL: fix_name(\"return\") = \"%s\"\n", fix_name("return").c_str());
        fail++;
    } else { printf("  OK: keyword 'return' → _return\n"); }

    if (fix_name("void") != "_void") {
        printf("  FAIL: fix_name(\"void\") = \"%s\"\n", fix_name("void").c_str());
        fail++;
    } else { printf("  OK: keyword 'void' → _void\n"); }

    if (fix_name("static") != "_static") {
        printf("  FAIL: fix_name(\"static\") = \"%s\"\n", fix_name("static").c_str());
        fail++;
    } else { printf("  OK: keyword 'static' → _static\n"); }

    if (fix_name("klass") != "_klass") {
        printf("  FAIL: fix_name(\"klass\") = \"%s\"\n", fix_name("klass").c_str());
        fail++;
    } else { printf("  OK: il2cpp field 'klass' → _klass\n"); }

    if (fix_name("monitor") != "_monitor") {
        printf("  FAIL: fix_name(\"monitor\") = \"%s\"\n", fix_name("monitor").c_str());
        fail++;
    } else { printf("  OK: il2cpp field 'monitor' → _monitor\n"); }

    // Prefix keywords
    if (fix_name("inline") != "_inline_") {
        printf("  FAIL: fix_name(\"inline\") = \"%s\", expected \"_inline_\"\n", fix_name("inline").c_str());
        fail++;
    } else { printf("  OK: prefix keyword 'inline' → _inline_\n"); }

    // Numbers at start get underscore prefix
    if (fix_name("1abc") != "_1abc") {
        printf("  FAIL: fix_name(\"1abc\") = \"%s\", expected \"_1abc\"\n", fix_name("1abc").c_str());
        fail++;
    } else { printf("  OK: leading digit → _1abc\n"); }

    if (fix_name("0x1234") != "_0x1234") {
        printf("  FAIL: fix_name(\"0x1234\") = \"%s\", expected \"_0x1234\"\n", fix_name("0x1234").c_str());
        fail++;
    } else { printf("  OK: leading zero → _0x1234\n"); }

    // Special characters become underscores
    if (fix_name("my-method") != "my_method") {
        printf("  FAIL: fix_name(\"my-method\") = \"%s\", expected \"my_method\"\n", fix_name("my-method").c_str());
        fail++;
    } else { printf("  OK: hyphen → underscore\n"); }

    if (fix_name("my$class") != "my_class") {
        printf("  FAIL: fix_name(\"my$class\") = \"%s\"\n", fix_name("my$class").c_str());
        fail++;
    } else { printf("  OK: dollar → underscore\n"); }

    if (fix_name("method<T>") != "method_T_") {
        printf("  FAIL: fix_name(\"method<T>\") = \"%s\"\n", fix_name("method<T>").c_str());
        fail++;
    } else { printf("  OK: angle brackets → underscores\n"); }

    if (fix_name("a.b.c") != "a_b_c") {
        printf("  FAIL: fix_name(\"a.b.c\") = \"%s\"\n", fix_name("a.b.c").c_str());
        fail++;
    } else { printf("  OK: dots → underscores\n"); }

    // Empty string
    if (fix_name("") != "_") {
        printf("  FAIL: fix_name(\"\") = \"%s\", expected \"_\"\n", fix_name("").c_str());
        fail++;
    } else { printf("  OK: empty → _\n"); }

    // Namespace.Type$$Method format (the key format used in script.json Name field)
    if (fix_name("Game.Player$$TakeDamage") != "Game_Player__TakeDamage") {
        printf("  FAIL: fix_name(\"Game.Player$$TakeDamage\") = \"%s\"\n", fix_name("Game.Player$$TakeDamage").c_str());
        fail++;
    } else { printf("  OK: Namespace.Type$$Method format handled\n"); }

    printf("  fix_name: %d failures\n", fail);
    return fail;
}
