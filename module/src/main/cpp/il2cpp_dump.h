//
// Created by Perfare on 2020/7/4.
//

#ifndef ZYGISK_IL2CPPDUMPER_IL2CPP_DUMP_H
#define ZYGISK_IL2CPPDUMPER_IL2CPP_DUMP_H

void il2cpp_api_init(void *handle);

void il2cpp_dump(const char *outDir);

// 获取 libil2cpp.so 基地址，供 hook_trace 使用
uint64_t get_il2cpp_base();

#endif //ZYGISK_IL2CPPDUMPER_IL2CPP_DUMP_H
