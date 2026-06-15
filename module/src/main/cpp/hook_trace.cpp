//
// hook_trace.cpp
// 追踪 RoleLoginRes 解码与 FuncOpenMgr 菜单功能激活流程
//
// 用法: adb logcat -s HookTrace
//
// 原理: 通过 il2cpp API 获取 MethodInfo，替换 methodPointer 实现 hook
//       不依赖任何 inline hook 框架（Dobby 等），更稳定
//
// 多策略查找: 1. namespace+className 精确匹配
//            2. className only 匹配（忽略 namespace）
//            3. RVA 匹配（最可靠，直接用 il2cpp_base + RVA 计算地址）
//            4. 诊断日志：输出所有包含关键词的类名
//

#include "hook_trace.h"
#include "il2cpp_dump.h"
#include "il2cpp-class.h"
#include "log.h"

#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstring>

// ==================== il2cpp API 函数指针 ====================
// 这些在 il2cpp_dump.cpp 中定义，这里只 extern 声明我们需要的几个

extern Il2CppDomain *(*il2cpp_domain_get)();
extern const Il2CppAssembly **(*il2cpp_domain_get_assemblies)(const Il2CppDomain *domain, size_t *size);
extern Il2CppImage *(*il2cpp_assembly_get_image)(const Il2CppAssembly *assembly);
extern const char *(*il2cpp_image_get_name)(const Il2CppImage *image);
extern size_t (*il2cpp_image_get_class_count)(const Il2CppImage *image);
extern const Il2CppClass *(*il2cpp_image_get_class)(const Il2CppImage *image, size_t index);
extern const MethodInfo *(*il2cpp_class_get_method_from_name)(Il2CppClass *klass, const char *name, int argsCount);
extern const char *(*il2cpp_class_get_name)(Il2CppClass *klass);
extern const char *(*il2cpp_class_get_namespace)(Il2CppClass *klass);
extern const MethodInfo *(*il2cpp_class_get_methods)(Il2CppClass *klass, void **iter);

// ==================== 安全读取工具 ====================

static inline int32_t safeReadS32(const void *p) {
    if (!p) return -1;
    int32_t v;
    memcpy(&v, p, sizeof(v));
    return v;
}

static inline uint32_t safeReadU32(const void *p) {
    if (!p) return 0;
    uint32_t v;
    memcpy(&v, p, sizeof(v));
    return v;
}

static inline uint8_t safeReadU8(const void *p) {
    if (!p) return 0;
    uint8_t v;
    memcpy(&v, p, sizeof(v));
    return v;
}

static inline void *safeReadPtr(const void *p) {
    if (!p) return nullptr;
    void *v;
    memcpy(&v, p, sizeof(v));
    return v;
}

// 读取 Il2Cpp List<uint> (size at +0x18, items ptr at +0x10 -> uint[] at +0x20)
static void readUintList(const void *listPtr, int32_t *outSize, uint32_t *outBuf, int maxItems) {
    *outSize = 0;
    if (!listPtr) return;
    auto itemsArr = safeReadPtr((const uint8_t *)listPtr + 0x10);
    auto size = safeReadS32((const uint8_t *)listPtr + 0x18);
    if (size < 0) size = 0;
    *outSize = size;
    int readCount = size > maxItems ? maxItems : size;
    if (itemsArr) {
        for (int i = 0; i < readCount; i++) {
            outBuf[i] = safeReadU32((const uint8_t *)itemsArr + 0x20 + i * 4);
        }
    }
}

// 读取 Il2Cpp uint[] (length at +0x18, data at +0x20)
static void readUintArray(const void *arrPtr, int32_t *outLen, uint32_t *outBuf, int maxItems) {
    *outLen = 0;
    if (!arrPtr) return;
    auto len = safeReadU32((const uint8_t *)arrPtr + 0x18);
    *outLen = (int32_t)len;
    int readCount = (int)len > maxItems ? maxItems : (int)len;
    for (int i = 0; i < readCount; i++) {
        outBuf[i] = safeReadU32((const uint8_t *)arrPtr + 0x20 + i * 4);
    }
}

// ==================== 限流计数器 ====================

struct CallCounter {
    int count;
    int limit;
};

static CallCounter g_counters[7] = {
    {0, 10}, {0, 10}, {0, 10}, {0, 10},
    {0, 10}, {0, 50}, {0, 10}
};

static bool shouldLog(int idx) {
    g_counters[idx].count++;
    if (g_counters[idx].count <= g_counters[idx].limit) return true;
    if (g_counters[idx].count == g_counters[idx].limit + 1) {
        LOGH("[H%d] ... (subsequent calls suppressed, total so far=%d)",
             idx + 1, g_counters[idx].count);
    }
    return false;
}

// ==================== Hook 回调 ====================

// --- H1: CSRoleLoginRes.unpack(self, srcBuf, cutVer, stack) ---
typedef void (*H1_Fn)(void *self, void *srcBuf, uint32_t cutVer, void *stack);
static H1_Fn orig_H1 = nullptr;

static void hook_H1(void *self, void *srcBuf, uint32_t cutVer, void *stack) {
    orig_H1(self, srcBuf, cutVer, stack);
    if (!shouldLog(0)) return;
    auto result = safeReadS32((const uint8_t *)self + 0x18);
    auto lastLogout = safeReadU32((const uint8_t *)self + 0x20);
    auto wearShape = safeReadU8((const uint8_t *)self + 0x24);
    auto regionID = safeReadS32((const uint8_t *)self + 0x30);
    LOGH("[H1] CSRoleLoginRes.unpack: Result=%d LastLogoutTime=%u WearShape=%u RegionID=%d",
         result, lastLogout, wearShape, regionID);
}

// --- H2: CSGuideFuncOpenedRes.unpack(self, srcBuf, cutVer, stack) ---
typedef void (*H2_Fn)(void *self, void *srcBuf, uint32_t cutVer, void *stack);
static H2_Fn orig_H2 = nullptr;

static void hook_H2(void *self, void *srcBuf, uint32_t cutVer, void *stack) {
    orig_H2(self, srcBuf, cutVer, stack);
    if (!shouldLog(1)) return;
    auto cnt = safeReadS32((const uint8_t *)self + 0x10);
    auto listPtr = safeReadPtr((const uint8_t *)self + 0x18);
    uint32_t buf[200];
    int32_t arrLen = 0;
    readUintArray(listPtr, &arrLen, buf, 200);
    char out[1024];
    int pos = 0;
    pos += snprintf(out + pos, sizeof(out) - pos, "FuncOpenedCnt=%d List=[", cnt);
    int printCount = arrLen > 50 ? 50 : arrLen;
    for (int i = 0; i < printCount; i++) {
        pos += snprintf(out + pos, sizeof(out) - pos, "%u,", buf[i]);
    }
    if (arrLen > 50) pos += snprintf(out + pos, sizeof(out) - pos, "...(%d more)", arrLen - 50);
    if (printCount > 0) pos--;
    pos += snprintf(out + pos, sizeof(out) - pos, "]");
    LOGH("[H2] CSGuideFuncOpenedRes.unpack: %s", out);
}

// --- H3: FuncOpenMgr.CheckOpenList(self, svrData) ---
typedef void (*H3_Fn)(void *self, void *svrData);
static H3_Fn orig_H3 = nullptr;

static void hook_H3(void *self, void *svrData) {
    if (!shouldLog(2)) {
        orig_H3(self, svrData);
        return;
    }
    auto initOpenList = safeReadU8((const uint8_t *)self + 0x10) != 0;
    auto openFuncListPtr = safeReadPtr((const uint8_t *)self + 0x18);
    int32_t listSize = 0;
    uint32_t listBuf[200];
    readUintList(openFuncListPtr, &listSize, listBuf, 200);

    char svrInfo[512] = "<null>";
    if (svrData) {
        auto svrCnt = safeReadS32((const uint8_t *)svrData + 0x10);
        auto svrListPtr = safeReadPtr((const uint8_t *)svrData + 0x18);
        int32_t svrArrLen = 0;
        uint32_t svrBuf[200];
        readUintArray(svrListPtr, &svrArrLen, svrBuf, 200);
        int sPos = 0;
        sPos += snprintf(svrInfo, sizeof(svrInfo), "FuncOpenedCnt=%d List=[", svrCnt);
        int printCount = svrArrLen > 30 ? 30 : svrArrLen;
        for (int i = 0; i < printCount; i++) {
            sPos += snprintf(svrInfo + sPos, sizeof(svrInfo) - sPos, "%u,", svrBuf[i]);
        }
        if (printCount > 0) sPos--;
        snprintf(svrInfo + sPos, sizeof(svrInfo) - sPos, "]");
    }

    char listOut[512];
    int lPos = 0;
    lPos += snprintf(listOut, sizeof(listOut), "size=%d [", listSize);
    int printCount = listSize > 50 ? 50 : listSize;
    for (int i = 0; i < printCount; i++) {
        lPos += snprintf(listOut + lPos, sizeof(listOut) - lPos, "%u,", listBuf[i]);
    }
    if (printCount > 0) lPos--;
    snprintf(listOut + lPos, sizeof(listOut) - lPos, "]");

    LOGH("[H3] FuncOpenMgr.CheckOpenList: m_initOpenList=%d m_openFuncList=%s svrData=%s",
         initOpenList, listOut, svrInfo);

    orig_H3(self, svrData);
}

// --- H4: FuncOpenMgr.ReqFuncOpenData(self) ---
typedef void (*H4_Fn)(void *self);
static H4_Fn orig_H4 = nullptr;

static void hook_H4(void *self) {
    orig_H4(self);
    if (!shouldLog(3)) return;
    LOGH("[H4] FuncOpenMgr.ReqFuncOpenData: called");
}

// --- H5: FuncOpenMgr.HandleNotifyFuncOpened(self, result, msg) ---
typedef void (*H5_Fn)(void *self, int32_t result, void *msg);
static H5_Fn orig_H5 = nullptr;

static void hook_H5(void *self, int32_t result, void *msg) {
    orig_H5(self, result, msg);
    if (!shouldLog(4)) return;
    uint16_t cmd = 0;
    auto head = safeReadPtr((const uint8_t *)msg + 0x10);
    if (head) {
        memcpy(&cmd, (const uint8_t *)head + 0x12, 2);
    }
    LOGH("[H5] FuncOpenMgr.HandleNotifyFuncOpened: result=%d msg.Cmd=0x%04X(%u)",
         result, cmd, cmd);
}

// --- H6: FuncOpenMgr.CheckFuncOpen(self, funcType, showTips) -> bool ---
typedef uint8_t (*H6_Fn)(void *self, uint32_t funcType, uint8_t showTips);
static H6_Fn orig_H6 = nullptr;

static uint8_t hook_H6(void *self, uint32_t funcType, uint8_t showTips) {
    auto ret = orig_H6(self, funcType, showTips);
    if (!shouldLog(5)) return ret;
    auto initOpenList = safeReadU8((const uint8_t *)self + 0x10) != 0;
    auto openFuncListPtr = safeReadPtr((const uint8_t *)self + 0x18);
    int32_t listSize = 0;
    readUintList(openFuncListPtr, &listSize, nullptr, 0);
    LOGH("[H6] FuncOpenMgr.CheckFuncOpen: funcType=%u showTips=%u ret=%u m_initOpenList=%d m_openFuncList.size=%d",
         funcType, showTips, ret, initOpenList, listSize);
    return ret;
}

// --- H7: FuncOpenNetMgr.NotifySysOpenCfg(self, result, msg) ---
typedef void (*H7_Fn)(void *self, int32_t result, void *msg);
static H7_Fn orig_H7 = nullptr;

static void hook_H7(void *self, int32_t result, void *msg) {
    orig_H7(self, result, msg);
    if (!shouldLog(6)) return;
    uint16_t cmd = 0;
    auto head = safeReadPtr((const uint8_t *)msg + 0x10);
    if (head) {
        memcpy(&cmd, (const uint8_t *)head + 0x12, 2);
    }
    LOGH("[H7] FuncOpenNetMgr.NotifySysOpenCfg: result=%d msg.Cmd=0x%04X(%u)",
         result, cmd, cmd);
}

// ==================== 多策略 Hook 注册 ====================

struct HookEntry {
    const char *tag;       // 日志标签 H1/H2/...
    const char *ns;        // 命名空间 (可为 nullptr 表示忽略)
    const char *cls;       // 类名
    const char *method;    // 方法名
    int argCounts[4];      // 尝试的参数个数列表 (不含 self), -1 结尾
    uint64_t rva;          // RVA 地址 (来自 dump.cs), 0 表示不用 RVA
    void *fake_fn;         // hook 函数
    void **orig_fn;        // 保存原始函数指针
};

// 全局 il2cpp_base，在 register_trace_hooks 中设置
static uint64_t g_il2cpp_base = 0;

// 辅助: 检查字符串是否包含子串 (不区分大小写)
static bool strContainsI(const char *haystack, const char *needle) {
    if (!haystack || !needle) return false;
    size_t hLen = strlen(haystack);
    size_t nLen = strlen(needle);
    if (nLen > hLen) return false;
    for (size_t i = 0; i <= hLen - nLen; i++) {
        bool match = true;
        for (size_t j = 0; j < nLen; j++) {
            char hc = haystack[i + j];
            char nc = needle[j];
            if (hc >= 'A' && hc <= 'Z') hc += 32;
            if (nc >= 'A' && nc <= 'Z') nc += 32;
            if (hc != nc) { match = false; break; }
        }
        if (match) return true;
    }
    return false;
}

// 诊断: 输出所有包含关键词的类
static void diagnoseClasses(const char *keyword) {
    if (!il2cpp_domain_get || !il2cpp_domain_get_assemblies ||
        !il2cpp_assembly_get_image || !il2cpp_image_get_class_count ||
        !il2cpp_image_get_class || !il2cpp_class_get_name ||
        !il2cpp_class_get_namespace) {
        return;
    }

    auto domain = il2cpp_domain_get();
    if (!domain) return;

    size_t asmCount = 0;
    auto assemblies = il2cpp_domain_get_assemblies(domain, &asmCount);
    if (!assemblies) return;

    int found = 0;
    for (size_t i = 0; i < asmCount && found < 20; i++) {
        auto image = il2cpp_assembly_get_image(assemblies[i]);
        if (!image) continue;
        auto classCount = il2cpp_image_get_class_count(image);
        for (size_t j = 0; j < classCount && found < 20; j++) {
            auto klass = const_cast<Il2CppClass *>(il2cpp_image_get_class(image, j));
            if (!klass) continue;
            auto name = il2cpp_class_get_name(klass);
            if (!name) continue;
            if (strContainsI(name, keyword)) {
                auto ns = il2cpp_class_get_namespace(klass);
                LOGH("[DIAG] Found: '%s.%s'", ns ? ns : "", name);
                found++;
            }
        }
    }
    if (found == 0) {
        LOGH("[DIAG] No class containing '%s' found", keyword);
    }
}

// 策略1: namespace+className 精确匹配
static bool hookByExactName(const HookEntry &entry) {
    auto domain = il2cpp_domain_get();
    if (!domain) return false;

    size_t asmCount = 0;
    auto assemblies = il2cpp_domain_get_assemblies(domain, &asmCount);
    if (!assemblies) return false;

    for (size_t i = 0; i < asmCount; i++) {
        auto image = il2cpp_assembly_get_image(assemblies[i]);
        if (!image) continue;
        auto classCount = il2cpp_image_get_class_count(image);
        for (size_t j = 0; j < classCount; j++) {
            auto klass = const_cast<Il2CppClass *>(il2cpp_image_get_class(image, j));
            if (!klass) continue;
            auto ns = il2cpp_class_get_namespace(klass);
            auto name = il2cpp_class_get_name(klass);
            if (!ns || !name) continue;
            if (strcmp(ns, entry.ns) != 0 || strcmp(name, entry.cls) != 0) continue;

            // 找到类，尝试不同 argCount
            for (int k = 0; entry.argCounts[k] >= 0; k++) {
                auto method = il2cpp_class_get_method_from_name(klass, entry.method, entry.argCounts[k]);
                if (method) {
                    *entry.orig_fn = (void *)method->methodPointer;
                    const_cast<MethodInfo *>(method)->methodPointer = (Il2CppMethodPointer)entry.fake_fn;
                    LOGH("[%s OK] %s.%s.%s(argCount=%d) @ %p -> %p",
                         entry.tag, entry.ns, entry.cls, entry.method, entry.argCounts[k],
                         *entry.orig_fn, entry.fake_fn);
                    return true;
                }
            }
            LOGH("[%s WARN] Found class %s.%s but method '%s' not found with any argCount",
                 entry.tag, entry.ns, entry.cls, entry.method);
            return false;
        }
    }
    return false;
}

// 策略2: 只按 className 匹配（忽略 namespace）
static bool hookByClassNameOnly(const HookEntry &entry) {
    auto domain = il2cpp_domain_get();
    if (!domain) return false;

    size_t asmCount = 0;
    auto assemblies = il2cpp_domain_get_assemblies(domain, &asmCount);
    if (!assemblies) return false;

    for (size_t i = 0; i < asmCount; i++) {
        auto image = il2cpp_assembly_get_image(assemblies[i]);
        if (!image) continue;
        auto classCount = il2cpp_image_get_class_count(image);
        for (size_t j = 0; j < classCount; j++) {
            auto klass = const_cast<Il2CppClass *>(il2cpp_image_get_class(image, j));
            if (!klass) continue;
            auto name = il2cpp_class_get_name(klass);
            if (!name || strcmp(name, entry.cls) != 0) continue;

            auto ns = il2cpp_class_get_namespace(klass);
            LOGH("[%s FALLBACK] Found %s.%s (expected %s.%s), trying method...",
                 entry.tag, ns ? ns : "", name, entry.ns, entry.cls);

            for (int k = 0; entry.argCounts[k] >= 0; k++) {
                auto method = il2cpp_class_get_method_from_name(klass, entry.method, entry.argCounts[k]);
                if (method) {
                    *entry.orig_fn = (void *)method->methodPointer;
                    const_cast<MethodInfo *>(method)->methodPointer = (Il2CppMethodPointer)entry.fake_fn;
                    LOGH("[%s OK] %s.%s.%s(argCount=%d) @ %p -> %p",
                         entry.tag, ns ? ns : "", entry.cls, entry.method, entry.argCounts[k],
                         *entry.orig_fn, entry.fake_fn);
                    return true;
                }
            }
            LOGH("[%s WARN] Found class %s.%s but method '%s' not found",
                 entry.tag, ns ? ns : "", name, entry.method);
            return false;
        }
    }
    return false;
}

// 策略3: RVA 直接匹配 (最可靠)
// 遍历所有类的方法，找到 methodPointer 等于 il2cpp_base + rva 的方法
static bool hookByRVA(const HookEntry &entry) {
    if (!g_il2cpp_base || !entry.rva) return false;

    auto domain = il2cpp_domain_get();
    if (!domain) return false;

    size_t asmCount = 0;
    auto assemblies = il2cpp_domain_get_assemblies(domain, &asmCount);
    if (!assemblies) return false;

    void *targetAddr = (void *)(g_il2cpp_base + entry.rva);

    for (size_t i = 0; i < asmCount; i++) {
        auto image = il2cpp_assembly_get_image(assemblies[i]);
        if (!image) continue;
        auto classCount = il2cpp_image_get_class_count(image);
        for (size_t j = 0; j < classCount; j++) {
            auto klass = const_cast<Il2CppClass *>(il2cpp_image_get_class(image, j));
            if (!klass) continue;

            // 用 il2cpp_class_get_methods 遍历方法
            void *iter = nullptr;
            const MethodInfo *method = nullptr;
            while ((method = il2cpp_class_get_methods(klass, &iter)) != nullptr) {
                if ((void *)method->methodPointer == targetAddr) {
                    auto ns = il2cpp_class_get_namespace(klass);
                    auto name = il2cpp_class_get_name(klass);
                    *entry.orig_fn = (void *)method->methodPointer;
                    const_cast<MethodInfo *>(method)->methodPointer = (Il2CppMethodPointer)entry.fake_fn;
                    LOGH("[%s OK-RVA] %s.%s @ %p -> %p [RVA=0x%llx]",
                         entry.tag, ns ? ns : "", name ? name : "?",
                         *entry.orig_fn, entry.fake_fn, (unsigned long long)entry.rva);
                    return true;
                }
            }
        }
    }

    LOGH("[%s FAIL-RVA] No method with RVA=0x%llx (addr=%p) found",
         entry.tag, (unsigned long long)entry.rva, targetAddr);
    return false;
}

// 综合注册: 依次尝试 精确名 -> 类名匹配 -> RVA
static bool hookMethodMulti(const HookEntry &entry) {
    // 策略1: 精确 namespace+className
    if (entry.ns && hookByExactName(entry)) return true;

    // 策略2: 只按 className
    if (hookByClassNameOnly(entry)) return true;

    // 策略3: RVA
    if (entry.rva && hookByRVA(entry)) return true;

    LOGH("[%s FAIL] All strategies failed for %s.%s.%s",
         entry.tag, entry.ns ? entry.ns : "", entry.cls, entry.method);
    return false;
}

// ==================== Hook 注册 ====================

void register_trace_hooks() {
    auto base = get_il2cpp_base();
    if (!base) {
        LOGH("ERROR: il2cpp_base is 0, cannot register hooks");
        return;
    }

    g_il2cpp_base = base;
    LOGH("il2cpp_base=0x%" PRIx64, base);

    // 先做诊断: 搜索包含关键词的类
    LOGH("=== DIAGNOSTIC: searching classes ===");
    diagnoseClasses("RoleLogin");
    diagnoseClasses("GuideFuncOpened");
    diagnoseClasses("FuncOpen");
    LOGH("=== DIAGNOSTIC done ===");

    // argCounts: 尝试不同的参数个数 (不含 self)
    // unpack(PbReadBuf, uint, PbStack) -> 3 个参数 (不含 self)
    // 但 il2cpp 有时把返回值也算参数，所以也尝试 4
    HookEntry entries[] = {
        { "H1", "net",    "CSRoleLoginRes",       "unpack",               {3, 4, -1},       0x251E144, (void *)hook_H1, (void **)&orig_H1 },
        { "H2", "net",    "CSGuideFuncOpenedRes", "unpack",               {3, 4, -1},       0x24770A0, (void *)hook_H2, (void **)&orig_H2 },
        { "H3", "S6Game", "FuncOpenMgr",          "CheckOpenList",        {1, 0, -1},       0x1FB21A0, (void *)hook_H3, (void **)&orig_H3 },
        { "H4", "S6Game", "FuncOpenMgr",          "ReqFuncOpenData",      {0, -1},          0x1FB2900, (void *)hook_H4, (void **)&orig_H4 },
        { "H5", "S6Game", "FuncOpenMgr",          "HandleNotifyFuncOpened", {2, 1, -1},     0x1FB209C, (void *)hook_H5, (void **)&orig_H5 },
        { "H6", "S6Game", "FuncOpenMgr",          "CheckFuncOpen",        {2, -1},          0x1FB29A0, (void *)hook_H6, (void **)&orig_H6 },
        { "H7", "S6Game", "FuncOpenNetMgr",       "NotifySysOpenCfg",     {2, -1},          0x1E858B8, (void *)hook_H7, (void **)&orig_H7 },
    };

    int ok = 0;
    for (int i = 0; i < 7; i++) {
        if (hookMethodMulti(entries[i])) {
            ok++;
        }
    }

    LOGH("Registered %d/7 hooks. Waiting for game login...", ok);
}
