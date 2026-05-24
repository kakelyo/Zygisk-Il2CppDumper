# 与 Il2CppDumper 桌面版的功能差异

> 基于 Il2CppDumper 桌面版源码 (D:\Github\Il2CppDumper) 与当前 Zygisk-Il2CppDumper 实现的逐项对比。
> 
> 标记说明：🔴 严重（IDA 无法正确导入/功能缺失）、🟡 中等（信息不完整但可工作）、🟢 轻微（格式/风格差异）

---

## 一、Phase 1：ScriptMethod + Addresses

### 1.1 Signature 方法名分隔符 🟡

| | 桌面版 | Zygisk |
|---|---|---|
| 拼接方式 | `typeName + "$$" + methodName`，经 FixName 后 `$$` → `__` | `ns + "_" + cn + "_" + mn`，经 fix_name 后为单下划线 |
| 示例 | `void Game_Player__TakeDamage (...)` | `void Game_Player_TakeDamage (...)` |

桌面版 Signature 中类名和方法名之间是**双下划线**（`$$` 经 FixName 转换），Zygisk 是**单下划线**。

### 1.2 静态方法的 `__this` 参数 🟡

| | 桌面版 | Zygisk |
|---|---|---|
| version ≤ 24 | 静态方法添加 `Il2CppObject* __this` | 不添加 |
| version > 24 | 不添加 | 不添加 |

桌面版在旧版本中为静态方法也添加 `__this`，Zygisk 一律不添加。

### 1.3 泛型实例方法 (MethodSpec) 🔴

桌面版为每个泛型方法实例单独生成 ScriptMethod 条目（如 `List<int>.Add()`），Zygisk **完全不处理泛型实例方法**。

### 1.4 TypeSignature 范围 🟡

| | 桌面版 | Zygisk |
|---|---|---|
| 包含 `__this` | 是 | 否 |
| 包含 `const MethodInfo*` | 是 | 否 |
| byref 编码 | 编码为 `IL2CPP_TYPE_PTR`（即 `i`） | 使用原始类型编码 |

桌面版的 TypeSignature 包含 `__this` 和尾部 `MethodInfo` 参数，Zygisk 不包含。

### 1.5 泛型实例方法的 MethodInfo 参数名 🟢

桌面版对泛型实例方法使用具体的 MethodInfo 结构名（如 `MethodInfo_1A2B* method`），Zygisk 统一用 `const MethodInfo* method`。

---

## 二、ParseType（Il2CppType → C 类型字符串）

### 2.1 泛型参数替换 🔴

| | 桌面版 | Zygisk |
|---|---|---|
| `IL2CPP_TYPE_VAR` | 通过 `Il2CppGenericContext.class_inst` 替换为实际类型 | 返回 `"Il2CppObject*"` |
| `IL2CPP_TYPE_MVAR` | 通过 `Il2CppGenericContext.method_inst` 替换为实际类型 | 返回 `"Il2CppObject*"` |
| 泛型上下文参数 | `ParseType(type, context)` | `parse_type(type)` — 无上下文 |

这是最核心的差异。桌面版能将泛型参数 `T` 替换为实际类型（如 `int32_t`），Zygisk 一律退化为 `Il2CppObject*`。

### 2.2 指针类型 (`IL2CPP_TYPE_PTR`) 🟡

| | 桌面版 | Zygisk |
|---|---|---|
| 处理方式 | 递归解析元素类型 + `"*"` | 返回 `"void*"`（不递归） |

桌面版能生成 `int32_t*`、`System_String_o**` 等精确类型，Zygisk 一律为 `void*`。

### 2.3 泛型实例中的枚举 🟡

| | 桌面版 | Zygisk |
|---|---|---|
| `IL2CPP_TYPE_GENERICINST` + 枚举 | 递归解析枚举底层类型 | 不检查枚举，走值类型分支 |

### 2.4 泛型实例结构名 🔴

| | 桌面版 | Zygisk |
|---|---|---|
| 名称来源 | `genericClassStructNameDic`（预计算，如 `List_Int32`） | `il2cpp_class_get_name()`（原始名，如 `List\`1`） |
| 唯一化 | `GetUniqueName` 确保无重名 | 无唯一化处理 |

桌面版能生成 `List_Int32_o*`，Zygisk 只能生成 `List_1__o*`（丢失泛型参数信息）。

### 2.5 值类型/类类型结构名唯一化 🔴

桌面版使用 `structNameDic` + `GetUniqueName` 确保不同命名空间下的同名类不冲突（如 `A.Foo` 和 `B.Foo` → `Foo` 和 `Foo_1`），Zygisk **没有唯一化处理**，可能产生重名。

### 2.6 其他类型处理差异 🟢

| 类型 | 桌面版 | Zygisk |
|---|---|---|
| `IL2CPP_TYPE_FNPTR` | 抛出 NotSupportedException | 返回 `"Il2CppObject*"` |
| `IL2CPP_TYPE_CMOD_REQD/OPT` | 抛出 NotSupportedException | 返回 `"Il2CppObject*"` |
| `IL2CPP_TYPE_TYPEDBYREF` | 返回 `"Il2CppObject*"` | 落入 default 返回 `"void"` |
| `IL2CPP_TYPE_ARRAY` vs `SZARRAY` | 分别处理（ARRAY 通过 Il2CppArrayType） | 合并处理 |

---

## 三、FixName

### 3.1 关键字集合差异 🟢

桌面版独有的关键字：`_cs`, `flat`, `default`, `_ds`, `interrupt`, `signed`, `asm`, `new`, `_`

Zygisk 的关键字列表更全面（60+ 个 C/C++ 关键字），但缺少桌面版的上述 9 个。

### 3.2 处理顺序差异 🟢

- 桌面版：先检查关键字 → 再替换特殊字符
- Zygisk：先替换特殊字符 → 再检查关键字

实际效果基本相同，极少数边界情况可能不同。

### 3.3 结构名唯一化 🔴

桌面版有 `GetUniqueName` 机制，Zygisk 没有。不同命名空间的同名类经 fix_name 后可能冲突。

---

## 四、Phase 2：ScriptString

### 4.1 Address 语义不同 🔴

| | 桌面版 | Zygisk |
|---|---|---|
| ScriptString.Address | libil2cpp.so 中**指向字符串的指针**的 RVA | 字符串数据在 global-metadata.dat 中的**偏移** |
| IDA 中的效果 | 可直接定位到 libil2cpp.so 中的字符串引用 | 无法在 libil2cpp.so 中定位 |

桌面版的 Address 指向 libil2cpp.so .data 段中的一个指针槽位，该槽位存储着字符串的地址。Zygisk 的 Address 是 metadata 文件内部的偏移量，在 IDA 分析 libil2cpp.so 时无意义。

---

## 五、Phase 3：ScriptMetadata + ScriptMetadataMethod

### 5.1 extract_metadata_from_blob() 名称解析 🔴

Zygisk 的 blob 版本虽然读取了 metadataUsageLists/Pairs 并正确解码了 usage type 和 decoded index，但**所有名称都是数字占位符**：

| 条目类型 | 桌面版 Name | Zygisk blob Name |
|---|---|---|
| TypeInfo | `System.String_TypeInfo` | `TypeInfo_0` |
| Il2CppType | `System.String_var` | `Il2CppType_1` |
| MethodDef | `Method$System.String.Concat()` | `Method$MethodDef_5` |
| FieldInfo | `Field$Game.Player.health` | `Field$FieldRef_3` |
| MethodRef | `Method$List<Int32>.Add()` | `Method$MethodRef_4` |

原因：Zygisk 读取了 stringOffset、typeDefinitionsOffset 等偏移量，但全部用 `(void)` 丢弃了，没有实现从 metadata 字符串表解析名称的逻辑。

### 5.2 extract_metadata_from_blob() Address 全部为 0 🔴

blob 版本无法访问 `Il2CppMetadataRegistration.metadataUsages` 指针数组，因此所有 Address 字段都为 0。

### 5.3 extract_metadata_from_runtime() Address 语义不同 🔴

| | 桌面版 | Zygisk runtime |
|---|---|---|
| TypeInfo.Address | metadataUsage **指针槽位** 的 RVA | Il2CppClass **结构体本身** 的 RVA |
| Il2CppType.Address | metadataUsage 指针槽位的 RVA | Il2CppType 结构体本身的 RVA |
| FieldInfo.Address | metadataUsage 指针槽位的 RVA | FieldInfo 结构体本身的 RVA |
| MethodDef.Address | metadataUsage 指针槽位的 RVA | 固定 0 |

桌面版的 Address 指向 libil2cpp.so 中存储指针的那个位置（.data/.bss 段），Zygisk 指向被引用对象本身。IDA 脚本依赖桌面版的语义来定位和重命名指针槽位。

### 5.4 MethodRef（泛型方法实例）完全缺失 🔴

| | 桌面版 | Zygisk |
|---|---|---|
| blob 版 | 有条目但名称为占位符 | 有条目但名称为占位符 |
| runtime 版 | 不适用 | **完全不生成 MethodRef 条目** |

runtime 版遍历 `il2cpp_class_get_methods()` 只能获取 MethodDef 级别的方法，无法获取泛型方法实例（MethodRef）。

### 5.5 StringLiteral (usage=5) 处理不当 🟡

桌面版将 usage=5 的条目放入 ScriptString（Address 为指针槽位 RVA），Zygisk blob 版直接跳过。Phase 2 的 `extract_strings()` 输出的 ScriptString.Address 是 metadata 偏移而非 libil2cpp.so RVA，语义不同。

### 5.6 v25+ 版本不支持 🔴

| | 桌面版 | Zygisk |
|---|---|---|
| v19-v24.5 | 从 header 读取 metadataUsageLists/Pairs | blob 版有但名称为占位符 |
| v25-v26 | 从 header 读取（字段仍在） | blob 版 version>=25 直接返回 |
| v27+ | 扫描 libil2cpp.so 的 data 段 | **完全不支持** |

v25+（Unity 2019.3+）是当前主流版本，Zygisk 对这些版本的 ScriptMetadata 输出完全依赖 runtime API，且 runtime API 无法提供 metadataUsage 指针槽位地址。

### 5.7 TypeInfo Signature 不区分数组类型 🟡

桌面版对数组类型的 TypeInfo 使用 `Signature = "Il2CppClass*"`，非数组使用 `FixName(structName) + "_c*"`。Zygisk blob 版固定 `"Il2CppClass*"`，不区分。

---

## 六、Phase 4：il2cpp.h

### 6.1 缺少 VTable 结构体生成 🔴

桌面版为每个有虚方法的类型生成 `_VTable` 结构体（包含 `VirtualInvokeData` 条目），Zygisk **完全缺失**。

### 6.2 缺少 RGCTX 结构体生成 🔴

桌面版为有泛型上下文数据的类型生成 `_RGCTXs` 结构体，Zygisk **完全缺失**。

### 6.3 缺少 MethodInfo 结构体生成 🔴

桌面版为泛型方法实例生成 `MethodInfo_XXXX` 结构体（含版本适配的字段布局），Zygisk **完全缺失**。

### 6.4 版本特定头文件 🔴

| | 桌面版 | Zygisk |
|---|---|---|
| 版本头数量 | 6 个（V22, V240, V241, V242, V27, V29） | 1 个固定版本 |
| 版本选择 | 根据 metadata version 自动选择 | 无选择逻辑 |
| Il2CppClass 布局 | 每个版本不同 | 固定布局，不对应任何桌面版版本 |

Zygisk 的 `Il2CppClass_1`/`Il2CppClass_2` 混合了多个版本的特征，与桌面版的任何版本都不匹配。

### 6.5 `_c` 结构体缺少 vtable 字段 🔴

桌面版的 `_c` 包含 `X_VTable vtable`（或 `VirtualInvokeData vtable[32]`），Zygisk 的 `_c` 缺少此字段。

### 6.6 `_c` 结构体 rgctx_data 类型不正确 🟡

桌面版在有 RGCTXs 时使用 `X_RGCTXs*`（自定义结构体指针），无 RGCTXs 时用 `Il2CppRGCTXData*`。Zygisk 始终为 `const Il2CppRGCTXData*`。

### 6.7 `_Fields` 缺少对齐修饰符 🟢

桌面版对 PE 格式非值类型添加 `__declspec(align(4/8))`，Zygisk 没有。对 Android ELF 格式影响不大。

### 6.8 泛型实例类型名称不正确 🔴

Zygisk 使用 `il2cpp_class_get_name()` 获取的原始名称（如 `List\`1`），无法展开泛型参数。桌面版使用预计算的泛型实例结构名（如 `List_Int32`）。

### 6.9 泛型实例字段类型解析无泛型上下文 🔴

Zygisk 的 `get_field_type_name()` 不接受泛型上下文参数，`IL2CPP_TYPE_VAR`/`IL2CPP_TYPE_MVAR` 一律退化为 `Il2CppObject*`。桌面版通过泛型上下文替换为实际类型。

### 6.10 数组类型为所有类型生成 🟢

桌面版只为实际被引用的类型生成数组结构体，Zygisk 为所有非枚举类型生成。结果正确但有冗余。

### 6.11 重复类型名处理 🔴

桌面版有 `GetUniqueName` 机制避免重名，Zygisk **没有**。不同命名空间的同名类会产生冲突。

### 6.12 静态字段中的值类型依赖未递归 🟡

桌面版在依赖排序时递归处理 `_StaticFields` 中的值类型字段依赖，Zygisk 只处理实例字段的依赖。

### 6.13 Il2CppRGCTXData 定义方式不同 🟢

桌面版定义为 `union`，Zygisk 定义为 `struct`（内含 union）。功能等价但语法不同。

---

## 七、差异统计

| 严重程度 | 数量 | 说明 |
|---|---|---|
| 🔴 严重 | 16 | 功能缺失或输出不兼容，IDA 无法正确使用 |
| 🟡 中等 | 8 | 信息不完整但基本可工作 |
| 🟢 轻微 | 6 | 格式/风格差异，不影响功能 |

### 🔴 严重差异汇总（需优先修复）

| # | 差异 | 影响范围 |
|---|---|---|
| 1 | 泛型参数替换不支持（VAR/MVAR → Il2CppObject*） | Phase 1/3/4 全部 |
| 2 | 结构名唯一化缺失 | Phase 1/3/4 全部 |
| 3 | 泛型实例方法 (MethodSpec) 不处理 | Phase 1 ScriptMethod |
| 4 | ScriptString.Address 语义不同（metadata 偏移 vs libil2cpp RVA） | Phase 2 |
| 5 | metadataUsage 名称解析为占位符 | Phase 3 blob 版 |
| 6 | metadataUsage Address 全部为 0 | Phase 3 blob 版 |
| 7 | metadataUsage Address 语义不同（对象本身 vs 指针槽位） | Phase 3 runtime 版 |
| 8 | MethodRef（泛型方法实例）缺失 | Phase 3 runtime 版 |
| 9 | v25+ 不支持 metadataUsage 提取 | Phase 3 |
| 10 | VTable 结构体缺失 | Phase 4 |
| 11 | RGCTX 结构体缺失 | Phase 4 |
| 12 | MethodInfo 结构体缺失 | Phase 4 |
| 13 | 版本特定头文件缺失 | Phase 4 |
| 14 | `_c` 缺少 vtable 字段 | Phase 4 |
| 15 | 泛型实例类型名称不正确 | Phase 4 |
| 16 | 泛型实例字段类型解析无上下文 | Phase 4 |
