//
// Created by Perfare on 2020/7/4.
//

#ifndef ZYGISK_IL2CPPDUMPER_IL2CPP_DUMP_H
#define ZYGISK_IL2CPPDUMPER_IL2CPP_DUMP_H

#include <cstdint>

// il2cpp_base is set in il2cpp_api_init() via dladdr().
// Declared extern here so script_json.cpp can reference it.
extern uint64_t il2cpp_base;

void il2cpp_api_init(void *handle);

void il2cpp_dump(const char *outDir);

// Dumps global-metadata.dat from memory to disk.
// On success, sets *out_meta_size to the metadata size and returns the pointer.
// On failure, returns nullptr.
const uint8_t* il2cpp_dump_global_metadata(const char *outDir, size_t *out_meta_size = nullptr);

#endif //ZYGISK_IL2CPPDUMPER_IL2CPP_DUMP_H
