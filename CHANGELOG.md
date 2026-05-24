# Changelog

## 2026-05-23 — global-metadata.dat 内存导出

### 新增功能

- **`il2cpp_dump_global_metadata()`** — 从内存中直接导出 `global-metadata.dat`，不再依赖 `libil2cpp.so` 文件
  - 通过 `il2cpp_image_get_name()` 返回的字符串指针作为"面包屑"
  - 从 `/proc/self/maps` 反向定位 metadata 所在内存区域
  - 导出后自动验证文件头：魔数 `0xFAB11BAF` + 版本号范围 `16-32`
  - 验证通过后输出 key counts（methodsCount、typeDefsCount、imagesCount）

### 关键代码路径

```
hack_start()
  ├─ xdl_open("libil2cpp.so") → il2cpp_api_init()
  │    ├─ init_il2cpp_api()     → xdl_sym 解析全部 API 函数指针
  │    ├─ dladdr → 计算 il2cpp_base
  │    ├─ 等待 il2cpp_is_vm_thread()
  │    └─ il2cpp_thread_attach()
  │
  └─ il2cpp_dump()
       ├─ Phase 1: dump.cs (runtime 反射/API)
       └─ Phase 2: il2cpp_dump_global_metadata()
            ├─ 取 assemblies[0] 的 image name
            ├─ 查 /proc/self/maps 定位其所在区域
            └─ 写入 → validate_metadata_file()
```

### 文件结构

- `module/src/main/cpp/hack.cpp` — 入口调度：xdl_open → API 初始化 → dump
- `module/src/main/cpp/il2cpp_dump.cpp` — dump 主逻辑：dump.cs + metadata.dat
- `module/src/main/cpp/il2cpp_dump.h` — 对外接口声明
- `module/src/main/cpp/il2cpp-api-functions.h` — il2cpp API 函数签名表

### 已知限制

1. **dump.cs 生成**：`<2018.3` 反射路径（`il2cpp_image_get_class` 不可用时），若 `Assembly::Load` 或 `Assembly::GetTypes` 缺失会提前 return，连带跳过 metadata dump
2. **HybridCLR 兼容**：`global-metadata.dat` 已可导出，但 Il2CppDumper 手动模式可能仍因 CodeRegistration 格式差异报"protected"
3. **时序依赖**：若 `il2cpp_domain_get_assemblies` 在 `xdl_open` 后尚未就绪，`il2cpp_api_init()` 会提前退出

### 构建

```bash
./gradlew :module:assembleRelease
```
