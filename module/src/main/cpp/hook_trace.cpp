//
// hook_trace.cpp
// 追踪 RoleLoginRes 解码与 FuncOpenMgr 菜单功能激活流程
//
// 用法: adb logcat -s HookTrace
//
// 原理: 通过 il2cpp API 获取 MethodInfo，替换 methodPointer 实现 hook
//       不依赖任何 inline hook 框架（Dobby 等），更稳定
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
extern Il2CppClass *(*il2cpp_class_from_name)(const Il2CppImage *image, const char *namespaze, const char *name);
extern const MethodInfo *(*il2cpp_class_get_method_from_name)(Il2CppClass *klass, const char *name, int argsCount);

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

// ==================== methodPointer 替换工具 ====================

struct HookEntry {
    const char *ns;        // 命名空间
    const char *cls;       // 类名
    const char *method;    // 方法名
    int argCount;          // 参数个数
    void *fake_fn;         // hook 函数
    void **orig_fn;        // 保存原始函数指针
};

static bool hookMethod(const HookEntry &entry) {
    if (!il2cpp_domain_get || !il2cpp_domain_get_assemblies ||
        !il2cpp_assembly_get_image || !il2cpp_class_from_name ||
        !il2cpp_class_get_method_from_name) {
        LOGH("ERROR: il2cpp API not initialized");
        return false;
    }

    auto domain = il2cpp_domain_get();
    if (!domain) {
        LOGH("ERROR: il2cpp_domain_get() returned null");
        return false;
    }

    size_t asmCount = 0;
    auto assemblies = il2cpp_domain_get_assemblies(domain, &asmCount);
    if (!assemblies) {
        LOGH("ERROR: il2cpp_domain_get_assemblies() returned null");
        return false;
    }

    for (size_t i = 0; i < asmCount; i++) {
        auto image = il2cpp_assembly_get_image(assemblies[i]);
        if (!image) continue;
        auto klass = il2cpp_class_from_name(image, entry.ns, entry.cls);
        if (!klass) continue;
        auto method = il2cpp_class_get_method_from_name(klass, entry.method, entry.argCount);
        if (!method) continue;

        // 保存原始 methodPointer
        *entry.orig_fn = (void *)method->methodPointer;
        // 替换为 hook 函数 (const_cast 因为 il2cpp API 返回 const 指针)
        const_cast<MethodInfo *>(method)->methodPointer = (Il2CppMethodPointer)entry.fake_fn;

        LOGH("[OK] %s.%s.%s @ %p -> %p",
             entry.ns, entry.cls, entry.method, *entry.orig_fn, entry.fake_fn);
        return true;
    }

    LOGH("[FAIL] %s.%s.%s not found in any assembly", entry.ns, entry.cls, entry.method);
    return false;
}

// ==================== Hook 注册 ====================

void register_trace_hooks() {
    auto base = get_il2cpp_base();
    if (!base) {
        LOGH("ERROR: il2cpp_base is 0, cannot register hooks");
        return;
    }

    LOGH("il2cpp_base=0x%" PRIx64, base);

    HookEntry entries[] = {
        { "net",      "CSRoleLoginRes",       "unpack",               4, (void *)hook_H1, (void **)&orig_H1 },
        { "net",      "CSGuideFuncOpenedRes", "unpack",               4, (void *)hook_H2, (void **)&orig_H2 },
        { "S6Game",   "FuncOpenMgr",          "CheckOpenList",        1, (void *)hook_H3, (void **)&orig_H3 },
        { "S6Game",   "FuncOpenMgr",          "ReqFuncOpenData",      0, (void *)hook_H4, (void **)&orig_H4 },
        { "S6Game",   "FuncOpenMgr",          "HandleNotifyFuncOpened", 2, (void *)hook_H5, (void **)&orig_H5 },
        { "S6Game",   "FuncOpenMgr",          "CheckFuncOpen",        2, (void *)hook_H6, (void **)&orig_H6 },
        { "S6Game",   "FuncOpenNetMgr",       "NotifySysOpenCfg",     2, (void *)hook_H7, (void **)&orig_H7 },
    };

    int ok = 0;
    for (int i = 0; i < 7; i++) {
        if (hookMethod(entries[i])) {
            ok++;
        }
    }

    LOGH("Registered %d/7 hooks. Waiting for game login...", ok);
}
