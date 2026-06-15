//
// hook_trace.cpp
// 追踪 RoleLoginRes 解码与 FuncOpenMgr 菜单功能激活流程
//
// 用法: adb logcat -s HookTrace
//
// 原理: 通过 il2cpp API 获取 MethodInfo，替换 methodPointer 实现 hook
//       同时 patch Il2CppClass 的 vtable 条目（虚方法分派走 vtable）
//       最终使用 inline code patching（修改目标函数入口机器码）确保可靠 hook
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
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>

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

// 前向声明 (定义在 inline patching 部分)
static void unpatchInline(int idx);
static void repatchInline(int idx);

struct CallCounter {
    int count;
    int limit;
};

static CallCounter g_counters[8] = {
    {0, 10}, {0, 10}, {0, 10}, {0, 10},
    {0, 50}, {0, 10}, {0, 10}, {0, 10}
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

// --- H1: CSRoleLoginRes.unpack(self, srcBuf, cutVer, stack) -> PbError.ErrorType ---
typedef int32_t (*H1_Fn)(void *self, void *srcBuf, uint32_t cutVer, void *stack);
static H1_Fn orig_H1 = nullptr;

static int32_t hook_H1(void *self, void *srcBuf, uint32_t cutVer, void *stack) {
    unpatchInline(0);
    auto ret = orig_H1(self, srcBuf, cutVer, stack);
    repatchInline(0);
    if (!shouldLog(0)) return ret;
    // CSRoleLoginRes layout (dump.cs):
    //   +0x10  UinInfo (ptr)    - 引用类型，8字节指针
    //   +0x18  ProtoResult (ptr) - 引用类型，8字节指针！不是 int
    //   +0x20  LastLogoutTime (uint)
    //   +0x24  WearShape (byte)
    //   +0x28  RoleData (ptr)
    //   +0x30  RegionID (int)
    // ProtoResult layout:
    //   +0x10  Ret (int) - 真正的返回码
    auto resultPtr = safeReadPtr((const uint8_t *)self + 0x18);
    int32_t resultRet = -999;
    if (resultPtr) {
        resultRet = safeReadS32((const uint8_t *)resultPtr + 0x10);
    }
    auto lastLogout = safeReadU32((const uint8_t *)self + 0x20);
    auto wearShape = safeReadU8((const uint8_t *)self + 0x24);
    auto regionID = safeReadS32((const uint8_t *)self + 0x30);
    LOGH("[H1] CSRoleLoginRes.unpack: ret=%d Result.Ret=%d (protoPtr=%p) LastLogoutTime=%u WearShape=%u RegionID=%d",
         ret, resultRet, resultPtr, lastLogout, wearShape, regionID);
    return ret;
}

// --- H2: CSGuideFuncOpenedRes.unpack(self, srcBuf, cutVer, stack) -> PbError.ErrorType ---
typedef int32_t (*H2_Fn)(void *self, void *srcBuf, uint32_t cutVer, void *stack);
static H2_Fn orig_H2 = nullptr;

static int32_t hook_H2(void *self, void *srcBuf, uint32_t cutVer, void *stack) {
    unpatchInline(1);
    auto ret = orig_H2(self, srcBuf, cutVer, stack);
    repatchInline(1);
    if (!shouldLog(1)) return ret;
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
    LOGH("[H2] CSGuideFuncOpenedRes.unpack: ret=%d %s", ret, out);
    return ret;
}

// --- H3: FuncOpenMgr.CheckOpenList(self, svrData) ---
typedef void (*H3_Fn)(void *self, void *svrData);
static H3_Fn orig_H3 = nullptr;

static void hook_H3(void *self, void *svrData) {
    if (!shouldLog(2)) {
        unpatchInline(2);
        orig_H3(self, svrData);
        repatchInline(2);
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

    unpatchInline(2);
    orig_H3(self, svrData);
    repatchInline(2);
}

// --- H4: FuncOpenMgr.ReqFuncOpenData(self) ---
typedef void (*H4_Fn)(void *self);
static H4_Fn orig_H4 = nullptr;

static void hook_H4(void *self) {
    unpatchInline(3);
    orig_H4(self);
    repatchInline(3);
    if (!shouldLog(3)) return;
    LOGH("[H4] FuncOpenMgr.ReqFuncOpenData: called");
}

// --- H5: FuncOpenMgr.HandleNotifyFuncOpened(self, result, msg) ---
typedef void (*H5_Fn)(void *self, int32_t result, void *msg);
static H5_Fn orig_H5 = nullptr;

static void hook_H5(void *self, int32_t result, void *msg) {
    unpatchInline(4);
    orig_H5(self, result, msg);
    repatchInline(4);
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

// 强制 CheckFuncOpen 返回 true，绕过功能开放检查
// 当 m_openFuncList 为空时 (FuncOpenMgr 未初始化)，所有功能检查都返回 false
// 导致游戏卡在登录界面无法进入，强制返回 true 可以绕过此限制
static bool g_forceFuncOpen = true;

static uint8_t hook_H6(void *self, uint32_t funcType, uint8_t showTips) {
    unpatchInline(5);
    auto ret = orig_H6(self, funcType, showTips);
    repatchInline(5);
    if (!shouldLog(5)) return g_forceFuncOpen ? 1 : ret;
    auto initOpenList = safeReadU8((const uint8_t *)self + 0x10) != 0;
    auto openFuncListPtr = safeReadPtr((const uint8_t *)self + 0x18);
    int32_t listSize = 0;
    readUintList(openFuncListPtr, &listSize, nullptr, 0);
    if (g_forceFuncOpen && !ret) {
        LOGH("[H6] FuncOpenMgr.CheckFuncOpen: funcType=%u showTips=%u origRet=%u -> FORCED TRUE (m_initOpenList=%d m_openFuncList.size=%d)",
             funcType, showTips, ret, initOpenList, listSize);
        return 1;
    }
    LOGH("[H6] FuncOpenMgr.CheckFuncOpen: funcType=%u showTips=%u ret=%u m_initOpenList=%d m_openFuncList.size=%d",
         funcType, showTips, ret, initOpenList, listSize);
    return ret;
}

// --- H7: FuncOpenNetMgr.NotifySysOpenCfg(self, result, msg) ---
typedef void (*H7_Fn)(void *self, int32_t result, void *msg);
static H7_Fn orig_H7 = nullptr;

static void hook_H7(void *self, int32_t result, void *msg) {
    unpatchInline(6);
    orig_H7(self, result, msg);
    repatchInline(6);
    if (!shouldLog(6)) return;
    uint16_t cmd = 0;
    auto head = safeReadPtr((const uint8_t *)msg + 0x10);
    if (head) {
        memcpy(&cmd, (const uint8_t *)head + 0x12, 2);
    }
    LOGH("[H7] FuncOpenNetMgr.NotifySysOpenCfg: result=%d msg.Cmd=0x%04X(%u)",
         result, cmd, cmd);
}

// --- H8: FuncOpenMgr.Init(self) ---
typedef void (*H8_Fn)(void *self);
static H8_Fn orig_H8 = nullptr;

static void hook_H8(void *self) {
    unpatchInline(7);
    orig_H8(self);
    repatchInline(7);
    auto initOpenList = safeReadU8((const uint8_t *)self + 0x10) != 0;
    auto openFuncListPtr = safeReadPtr((const uint8_t *)self + 0x18);
    int32_t listSize = 0;
    uint32_t listBuf[200];
    readUintList(openFuncListPtr, &listSize, listBuf, 200);
    char listOut[512];
    int lPos = 0;
    lPos += snprintf(listOut, sizeof(listOut), "size=%d [", listSize);
    int printCount = listSize > 30 ? 30 : listSize;
    for (int i = 0; i < printCount; i++) {
        lPos += snprintf(listOut + lPos, sizeof(listOut) - lPos, "%u,", listBuf[i]);
    }
    if (printCount > 0) lPos--;
    snprintf(listOut + lPos, sizeof(listOut) - lPos, "]");
    LOGH("[H8] FuncOpenMgr.Init: m_initOpenList=%d m_openFuncList=%s", initOpenList, listOut);
}

// ==================== VTable Patch ====================
//
// il2cpp 虚方法通过 vtable 分派:
//   obj->klass->vtable[slot].methodPointer(args)
// 仅替换 MethodInfo.methodPointer 不够，还需 patch vtable 中的副本
//
// VirtualInvokeData 布局 (64-bit):
//   offset 0: Il2CppMethodPointer methodPointer  (8 bytes)
//   offset 8: const MethodInfo* method            (8 bytes)
//
// Il2CppClass 的 vtable 在结构体末尾 (inline 变长数组)
// 我们通过扫描 klass 内存中匹配 {origPtr, methodInfo*} 的对来定位并 patch

static bool makePageWritable(void *addr) {
    long pageSize = sysconf(_SC_PAGESIZE);
    uintptr_t page = (uintptr_t)addr & ~(uintptr_t)(pageSize - 1);
    return mprotect((void *)page, (size_t)pageSize, PROT_READ | PROT_WRITE | PROT_EXEC) == 0;
}

// 扫描 Il2CppClass 内存，找到 vtable 中匹配的 VirtualInvokeData 并 patch
static int patchClassVtable(Il2CppClass *klass, const MethodInfo *targetMethod,
                            void *origPtr, void *newPtr) {
    if (!klass || !targetMethod || !origPtr || !newPtr) return 0;

    // 确保 klass 所在页面可写
    makePageWritable(klass);

    int patched = 0;
    void **scan = (void **)klass;

    // VirtualInvokeData = { methodPointer, MethodInfo* } = 16 bytes on 64-bit
    // 扫描 klass 结构体，查找 origPtr 后面紧跟 targetMethod 的位置
    // Il2CppClass 固定部分约 200-400 bytes，vtable 在末尾
    // 保守扫描 512 个指针宽度 (4096 bytes)
    for (int i = 0; i < 512; i++) {
        if (scan[i] == origPtr && (i + 1) < 512 && scan[i + 1] == (void *)targetMethod) {
            scan[i] = newPtr;
            patched++;
            LOGH("[VTABLE] Patched at klass+%d (slot %d)", (int)(i * sizeof(void *)), i / 2);
        }
    }

    return patched;
}

// ==================== Inline Code Patching (ARM64) ====================
//
// methodPointer 替换和 vtable patching 都可能因为 il2cpp 的分派机制而失效
// (例如: thunk、interface dispatch、编译器内联等)
//
// Inline code patching 直接修改目标函数入口的机器码，写入跳转到 hook 的指令
// 这是最可靠的方式: 无论 il2cpp 如何分派方法调用，最终都会执行到函数入口
//
// ARM64 trampoline (16 bytes):
//   LDR X16, [PC, #8]    ; 加载 8 字节后的 hook 地址到 X16
//   BR X16                ; 跳转到 X16 (hook 函数)
//   .quad hook_address    ; hook 函数地址 (8 bytes)
//
// 调用原始函数的方式: 临时恢复原始指令 -> 调用 -> 重新写入 trampoline
// (不用 bridge，因为 ARM64 的 PC-relative 指令如 ADRP 在 bridge 地址执行会崩溃)
// 注意: 非线程安全，但对于单线程游戏逻辑足够

struct InlineHookInfo {
    void *targetFn;      // 目标函数地址
    uint8_t saved[16];   // 保存的原始 16 bytes
    void *hookFn;        // hook 函数地址
    bool installed;      // 是否已安装
};

static InlineHookInfo g_inlineHooks[8] = {};

static bool installInlineHook(void *targetFn, void *hookFn, void **origFn, int idx) {
    if (!targetFn || !hookFn || !origFn || idx < 0 || idx >= 8) return false;

    // 1. 保存原始 16 bytes
    memcpy(g_inlineHooks[idx].saved, targetFn, 16);
    g_inlineHooks[idx].targetFn = targetFn;
    g_inlineHooks[idx].hookFn = hookFn;

    // 2. 修改目标函数页面为可写
    long pageSize = sysconf(_SC_PAGESIZE);
    uintptr_t page = (uintptr_t)targetFn & ~(uintptr_t)(pageSize - 1);
    if (mprotect((void *)page, (size_t)pageSize, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        LOGH("[INLINE] mprotect target page %p failed: %s", (void *)page, strerror(errno));
        return false;
    }

    // 3. 写入 trampoline 到目标函数入口
    const uint32_t ldrInst = 0x58000050; // LDR X16, [PC, #8]
    const uint32_t brInst  = 0xD61F0200; // BR X16
    uint64_t hookAddr = (uint64_t)hookFn;
    memcpy(targetFn, &ldrInst, 4);
    memcpy((uint8_t *)targetFn + 4, &brInst, 4);
    memcpy((uint8_t *)targetFn + 8, &hookAddr, 8);
    __builtin___clear_cache((char *)targetFn, (char *)targetFn + 16);

    // 4. orig_fn 指向目标函数本身 (调用前临时恢复原始指令)
    *origFn = targetFn;
    g_inlineHooks[idx].installed = true;

    LOGH("[INLINE] Patched %p -> %p (idx=%d)", targetFn, hookFn, idx);
    return true;
}

// 临时取消 inline patch (恢复原始指令)
static void unpatchInline(int idx) {
    if (idx < 0 || idx >= 8 || !g_inlineHooks[idx].installed) return;
    memcpy(g_inlineHooks[idx].targetFn, g_inlineHooks[idx].saved, 16);
    __builtin___clear_cache((char *)g_inlineHooks[idx].targetFn,
                            (char *)g_inlineHooks[idx].targetFn + 16);
}

// 重新写入 inline patch (trampoline)
static void repatchInline(int idx) {
    if (idx < 0 || idx >= 8 || !g_inlineHooks[idx].installed) return;
    const uint32_t ldrInst = 0x58000050;
    const uint32_t brInst  = 0xD61F0200;
    uint64_t hookAddr = (uint64_t)g_inlineHooks[idx].hookFn;
    memcpy(g_inlineHooks[idx].targetFn, &ldrInst, 4);
    memcpy((uint8_t *)g_inlineHooks[idx].targetFn + 4, &brInst, 4);
    memcpy((uint8_t *)g_inlineHooks[idx].targetFn + 8, &hookAddr, 8);
    __builtin___clear_cache((char *)g_inlineHooks[idx].targetFn,
                            (char *)g_inlineHooks[idx].targetFn + 16);
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

// 在指定 klass 上执行 hook: 替换 methodPointer + patch vtable
static bool hookOnClass(Il2CppClass *klass, const HookEntry &entry) {
    for (int k = 0; entry.argCounts[k] >= 0; k++) {
        auto method = il2cpp_class_get_method_from_name(klass, entry.method, entry.argCounts[k]);
        if (!method) continue;

        void *origPtr = (void *)method->methodPointer;

        // 1. 替换 MethodInfo.methodPointer
        const_cast<MethodInfo *>(method)->methodPointer = (Il2CppMethodPointer)entry.fake_fn;
        *entry.orig_fn = origPtr;

        // 2. Patch vtable 中的副本
        int vtablePatched = patchClassVtable(klass, method, origPtr, entry.fake_fn);

        auto ns = il2cpp_class_get_namespace(klass);
        auto name = il2cpp_class_get_name(klass);
        LOGH("[%s OK] %s.%s.%s(argCount=%d) @ %p -> %p vtable_patched=%d",
             entry.tag, ns ? ns : "", name ? name : "", entry.method,
             entry.argCounts[k], origPtr, entry.fake_fn, vtablePatched);
        return true;
    }
    return false;
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

            if (hookOnClass(klass, entry)) return true;

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

            if (hookOnClass(klass, entry)) return true;

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
                    void *origPtr = (void *)method->methodPointer;

                    // 替换 MethodInfo.methodPointer
                    const_cast<MethodInfo *>(method)->methodPointer = (Il2CppMethodPointer)entry.fake_fn;
                    *entry.orig_fn = origPtr;

                    // Patch vtable
                    int vtablePatched = patchClassVtable(klass, method, origPtr, entry.fake_fn);

                    auto ns = il2cpp_class_get_namespace(klass);
                    auto name = il2cpp_class_get_name(klass);
                    LOGH("[%s OK-RVA] %s.%s @ %p -> %p vtable_patched=%d [RVA=0x%llx]",
                         entry.tag, ns ? ns : "", name ? name : "?",
                         origPtr, entry.fake_fn, vtablePatched,
                         (unsigned long long)entry.rva);
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
        { "H1", "net",    "CSRoleLoginRes",       "unpack",               {3, 4, -1},       0x25692C8, (void *)hook_H1, (void **)&orig_H1 },
        { "H2", "net",    "CSGuideFuncOpenedRes", "unpack",               {3, 4, -1},       0x24BFE58, (void *)hook_H2, (void **)&orig_H2 },
        { "H3", "S6Game", "FuncOpenMgr",          "CheckOpenList",        {1, 0, -1},       0x1FE5368, (void *)hook_H3, (void **)&orig_H3 },
        { "H4", "S6Game", "FuncOpenMgr",          "ReqFuncOpenData",      {0, -1},          0x1FE5AC8, (void *)hook_H4, (void **)&orig_H4 },
        { "H5", "S6Game", "FuncOpenMgr",          "HandleNotifyFuncOpened", {2, 1, -1},     0x1FE5264, (void *)hook_H5, (void **)&orig_H5 },
        { "H6", "S6Game", "FuncOpenMgr",          "CheckFuncOpen",        {2, -1},          0x1FE5B60, (void *)hook_H6, (void **)&orig_H6 },
        { "H7", "S6Game", "FuncOpenNetMgr",       "NotifySysOpenCfg",     {2, -1},          0x1EB2F10, (void *)hook_H7, (void **)&orig_H7 },
        { "H8", "S6Game", "FuncOpenMgr",          "Init",                 {0, -1},          0x1FE4FD4, (void *)hook_H8, (void **)&orig_H8 },
    };

    int ok = 0;
    for (int i = 0; i < 8; i++) {
        if (hookMethodMulti(entries[i])) {
            ok++;
        }
    }

    LOGH("Registered %d/8 hooks via methodPointer+vtable (g_forceFuncOpen=%d). Inline patching DISABLED.", ok, g_forceFuncOpen ? 1 : 0);

    // ==================== Inline Code Patching ====================
    // DISABLED: inline patching 直接修改目标函数入口机器码
    // 实测 methodPointer+vtable 已经足够让 hook 触发 (H1/H6 均正常触发)
    // inline patching 在多线程环境下 unpatch/repatch 不安全，可能导致:
    //   1. 函数执行被破坏 (另一个线程执行到半修改的指令)
    //   2. 游戏逻辑卡住 (某些函数的 prologue 被破坏后恢复不完整)
    // 如果确认 methodPointer+vtable 不够，可以设置 g_useInlinePatch=true
    static bool g_useInlinePatch = false;
    if (g_useInlinePatch) {
        for (int i = 0; i < 8; i++) {
            void *targetFn = *entries[i].orig_fn;
            if (!targetFn) {
                LOGH("[INLINE-%s] SKIP: orig_fn is null (method not found)", entries[i].tag);
                continue;
            }
            if (installInlineHook(targetFn, entries[i].fake_fn, entries[i].orig_fn, i)) {
                LOGH("[INLINE-%s] OK at %p (actual methodPointer)", entries[i].tag, targetFn);
            } else {
                LOGH("[INLINE-%s] FAILED at %p", entries[i].tag, targetFn);
            }
        }
    }

    LOGH("All hooks installed (methodPointer+vtable, inline=%s). g_forceFuncOpen=%d. Waiting for game login...",
         g_useInlinePatch ? "ON" : "OFF", g_forceFuncOpen ? 1 : 0);
}
