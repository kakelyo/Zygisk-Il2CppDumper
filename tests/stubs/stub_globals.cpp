// stub_globals.cpp — Provides global variable definitions needed by script_json.cpp in tests

#include <cstdint>

// il2cpp_base — set at runtime in real code, zero in tests
uint64_t il2cpp_base = 0;

// il2cpp API function pointer definitions (null in test mode).
// script_json.cpp declares these as extern; this file provides the definitions.
// il2cpp-api-functions.h already includes il2cpp-class.h for type definitions.
#define DO_API(r, n, p) r (*n) p = nullptr
#define DO_API_NO_RETURN(r, n, p) DO_API(r, n, p)
#include "il2cpp-api-functions.h"
#undef DO_API
#undef DO_API_NO_RETURN
