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
    auto ret = orig_H1(self, srcBuf, cutVer, stack);
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
    auto ret = orig_H2(self, srcBuf, cutVer, stack);
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

// 强制 CheckFuncOpen 返回 true，绕过功能开放检查
// 当 m_openFuncList 为空时 (FuncOpenMgr 未初始化)，所有功能检查都返回 false
// 导致游戏卡在登录界面无法进入，强制返回 true 可以绕过此限制
static bool g_forceFuncOpen = true;

static uint8_t hook_H6(void *self, uint32_t funcType, uint8_t showTips) {
    auto ret = orig_H6(self, funcType, showTips);
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

// --- H8: FuncOpenMgr.Init(self) ---
typedef void (*H8_Fn)(void *self);
static H8_Fn orig_H8 = nullptr;

static void hook_H8(void *self) {
    orig_H8(self);
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
// methodPointer 替换和 vtable patching 对 AOT 编译的直接调用无效
// (il2cpp AOT 编译器会将虚方法调用去虚化为直接调用，绕过 vtable)
//
// Inline code patching 直接修改目标函数入口的机器码，写入跳转到 hook 的指令
// 这是最可靠的方式: 无论 il2cpp 如何分派方法调用，最终都会执行到函数入口
//
// ARM64 trampoline (16 bytes) 写入目标函数入口:
//   LDR X16, [PC, #8]    ; 加载 8 字节后的 hook 地址到 X16
//   BR X16                ; 跳转到 X16 (hook 函数)
//   .quad hook_address    ; hook 函数地址 (8 bytes)
//
// 调用原始函数的方式: 通过 bridge (跳板) 执行保存的原始指令
// bridge 中复制了原始函数的前 4 条指令 (16 bytes)，并修复 PC-relative 指令
// 然后跳转回原始函数 +16 继续执行。这样 inline patch 永远不需要临时恢复，
// 是线程安全的。

struct InlineHookInfo {
    void *targetFn;      // 目标函数地址
    uint8_t saved[16];   // 保存的原始 16 bytes (4 条 ARM64 指令)
    void *hookFn;        // hook 函数地址
    void *bridge;        // bridge 跳板地址 (调用原始函数用)
    bool installed;      // 是否已安装
};

static InlineHookInfo g_inlineHooks[8] = {};

// ==================== ARM64 Bridge (跳板) ====================
//
// bridge 是一段可执行内存，包含:
//   1. 原始函数的前 4 条指令 (16 bytes)，PC-relative 指令已修复
//   2. 跳转回原始函数 +16 的指令
//
// hook 回调通过调用 bridge 来执行原始函数，无需 unpatch/repatch

// Bridge 内存池 (简单实现: 分配多个 4K 页面)
static uint8_t *g_bridgePool = nullptr;
static int g_bridgePoolOffset = 0;
static const int BRIDGE_POOL_SIZE = 65536; // 64KB, 足够 8 个 hook

// 分配 bridge 内存
static uint8_t *allocBridge() {
    if (!g_bridgePool) {
        g_bridgePool = (uint8_t *)mmap(nullptr, BRIDGE_POOL_SIZE,
                                        PROT_READ | PROT_WRITE | PROT_EXEC,
                                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (g_bridgePool == MAP_FAILED) {
            g_bridgePool = nullptr;
            LOGH("[BRIDGE] mmap failed: %s", strerror(errno));
            return nullptr;
        }
        LOGH("[BRIDGE] pool allocated at %p (%d bytes)", g_bridgePool, BRIDGE_POOL_SIZE);
    }
    if (g_bridgePoolOffset + 128 > BRIDGE_POOL_SIZE) {
        LOGH("[BRIDGE] pool exhausted");
        return nullptr;
    }
    uint8_t *p = g_bridgePool + g_bridgePoolOffset;
    g_bridgePoolOffset += 128; // 每个 bridge 最多 128 bytes (间接跳转替代可能需要更多空间)
    return p;
}

// 修复 ARM64 ADRP 指令的 PC-relative 偏移
// ADRP 格式: [31:24]=1x01_0000 [23:5]=immhi [4:0]=Rd
// imm = SignExtend(immhi:immlo) << 12
// Result = (PC & ~0xFFF) + imm
static bool fixupADRP(uint32_t *insn, uintptr_t origPC, uintptr_t newPC) {
    uint32_t v = *insn;
    if ((v & 0x9F000000) != 0x90000000) return false; // 不是 ADRP

    int64_t immhi = (v >> 5) & 0x7FFFF;
    int64_t immlo = (v >> 29) & 0x3;
    int64_t imm = (immhi << 2) | immlo;
    if (imm & 0x100000) imm -= 0x200000; // 符号扩展 21 位

    // 原始 ADRP 的目标地址
    uint64_t target = (origPC & ~(uint64_t)0xFFF) + (imm << 12);

    // 新的偏移
    int64_t newImm = ((int64_t)target - (int64_t)(newPC & ~(uint64_t)0xFFF)) >> 12;
    if (newImm < -0x100000 || newImm > 0x0FFFFF) {
        LOGH("[BRIDGE] ADRP fixup out of range: newImm=%lld", (long long)newImm);
        return false; // 超出 ADRP 的 21 位范围
    }

    uint32_t newImmLo = (uint32_t)(newImm & 0x3);
    uint32_t newImmHi = (uint32_t)((newImm >> 2) & 0x7FFFF);
    *insn = (v & 0x9F00001F) | (newImmLo << 29) | (newImmHi << 5);
    return true;
}

// 修复 ARM64 B/BL 指令的 PC-relative 偏移
static bool fixupBBL(uint32_t *insn, uintptr_t origPC, uintptr_t newPC) {
    uint32_t v = *insn;
    bool isBL = (v & 0xFC000000) == 0x94000000;
    bool isB  = (v & 0xFC000000) == 0x14000000;
    if (!isB && !isBL) return false;

    int64_t offset = v & 0x3FFFFFF;
    if (offset & 0x2000000) offset -= 0x4000000; // 符号扩展 26 位

    int64_t target = origPC + (offset << 2);
    int64_t newOffset = (target - (int64_t)newPC) >> 2;
    if (newOffset < -0x2000000 || newOffset > 0x1FFFFFF) {
        LOGH("[BRIDGE] B/BL fixup out of range");
        return false;
    }

    *insn = (v & 0xFC000000) | ((uint32_t)newOffset & 0x3FFFFFF);
    return true;
}

// 修复 ARM64 LDR (literal) 指令的 PC-relative 偏移
static bool fixupLDRLiteral(uint32_t *insn, uintptr_t origPC, uintptr_t newPC) {
    uint32_t v = *insn;
    // LDR Wt, label: 0x18xxxxxx  (opc=00)
    // LDR Xt, label: 0x58xxxxxx  (opc=01, 64-bit)
    // LDRSW Xt, label: 0x98xxxxxx (opc=10)
    bool isLDR = (v & 0x3B000000) == 0x18000000;
    if (!isLDR) return false;

    int64_t offset = v & 0x7FFFF;
    if (offset & 0x40000) offset -= 0x80000; // 符号扩展 19 位

    int64_t target = origPC + (offset << 2);
    int64_t newOffset = (target - (int64_t)newPC) >> 2;
    if (newOffset < -0x40000 || newOffset > 0x3FFFF) {
        LOGH("[BRIDGE] LDR literal fixup out of range");
        return false;
    }

    *insn = (v & 0xFF000000) | ((uint32_t)newOffset & 0x7FFFF);
    return true;
}

// 修复 ARM64 条件分支指令 (CBZ/CBNZ/B.cond) 的 PC-relative 偏移
// CBZ:  0x34000000 | (imm19 << 5) | Rt  (32-bit)
// CBNZ: 0x35000000 | (imm19 << 5) | Rt  (32-bit)
// CBZ:  0xB4000000 | (imm19 << 5) | Rt  (64-bit)
// CBNZ: 0xB5000000 | (imm19 << 5) | Rt  (64-bit)
// B.cond: 0x54000000 | (imm19 << 5) | cond
static bool fixupCondBranch(uint32_t *insn, uintptr_t origPC, uintptr_t newPC) {
    uint32_t v = *insn;
    bool isCBZ32  = (v & 0x7E000000) == 0x34000000;
    bool isCBNZ32 = (v & 0x7E000000) == 0x35000000;
    bool isCBZ64  = (v & 0x7E000000) == 0xB4000000;
    bool isCBNZ64 = (v & 0x7E000000) == 0xB5000000;
    bool isBcond  = (v & 0xFF000000) == 0x54000000;
    if (!isCBZ32 && !isCBNZ32 && !isCBZ64 && !isCBNZ64 && !isBcond) return false;

    int64_t offset = (v >> 5) & 0x7FFFF;
    if (offset & 0x40000) offset -= 0x80000; // 符号扩展 19 位

    int64_t target = origPC + (offset << 2);
    int64_t newOffset = (target - (int64_t)newPC) >> 2;
    if (newOffset < -0x40000 || newOffset > 0x3FFFF) {
        return false; // 超出 19 位范围
    }

    *insn = (v & 0xFF00001F) | (((uint32_t)newOffset & 0x7FFFF) << 5);
    return true;
}

// 获取条件分支指令的目标地址，返回 0 表示不是条件分支
static uint64_t getCondBranchTarget(uint32_t insn, uintptr_t origPC) {
    bool isCBZ32  = (insn & 0x7E000000) == 0x34000000;
    bool isCBNZ32 = (insn & 0x7E000000) == 0x35000000;
    bool isCBZ64  = (insn & 0x7E000000) == 0xB4000000;
    bool isCBNZ64 = (insn & 0x7E000000) == 0xB5000000;
    bool isBcond  = (insn & 0xFF000000) == 0x54000000;
    if (!isCBZ32 && !isCBNZ32 && !isCBZ64 && !isCBNZ64 && !isBcond) return 0;

    int64_t offset = (insn >> 5) & 0x7FFFF;
    if (offset & 0x40000) offset -= 0x80000;
    return origPC + (offset << 2);
}

// 反转条件分支指令的条件 (CBZ<->CBNZ, 其他不变)
static uint32_t invertCondBranch(uint32_t insn) {
    // CBZ 32-bit: 0x34 <-> CBNZ 32-bit: 0x35
    if ((insn & 0x7E000000) == 0x34000000)
        return (insn ^ 0x01000000); // bit 24 翻转
    // CBZ 64-bit: 0xB4 <-> CBNZ 64-bit: 0xB5
    if ((insn & 0x7E000000) == 0xB4000000)
        return (insn ^ 0x01000000); // bit 24 翻转
    // B.cond: 无法简单反转，返回 0 表示不支持
    return 0;
}

// 创建 bridge: 复制原始指令并修复 PC-relative，然后跳回原函数+16
// 对于超出修复范围的 PC-relative 指令，用间接跳转替代
// 两遍扫描: 第一遍计算每条指令展开后的大小，第二遍生成代码
struct BridgeInsn {
    uint32_t orig;        // 原始指令
    int expandedSize;     // 展开后的大小 (4 或 12 或 16 或 20 或 24)
    uint8_t code[24];     // 展开后的代码
    bool isBL;            // 是否是 BL (需要特殊处理 LR)
    uint64_t blTarget;    // BL 的目标地址
    uint64_t blReturnIdx; // BL 返回后应执行的指令索引
    int internalBranch;   // B/BL/CondBranch 目标在 patched 区域内的指令索引 (-1=不是)
    bool isCondBranchIndirect; // 条件分支超范围，需要反转+间接跳转
    uint64_t condBranchTarget; // 条件分支的目标地址
};

static void *createBridge(void *targetFn, const uint8_t *saved) {
    uint8_t *bridge = allocBridge();
    if (!bridge) return nullptr;

    uintptr_t origPC = (uintptr_t)targetFn;
    BridgeInsn insns[4] = {};

    // 第一遍: 分析每条指令并生成代码，计算展开大小
    for (int i = 0; i < 4; i++) {
        uint32_t insn;
        memcpy(&insn, saved + i * 4, 4);
        insns[i].orig = insn;
        insns[i].isBL = false;
        insns[i].internalBranch = -1;
        insns[i].isCondBranchIndirect = false;
        insns[i].condBranchTarget = 0;
        uintptr_t insnOrigPC = origPC + i * 4;

        if ((insn & 0x9F000000) == 0x90000000) {
            // ADRP
            uint32_t fixed = insn;
            uintptr_t insnNewPC = 0; // 先用 0，第二遍再修复
            if (fixupADRP(&fixed, insnOrigPC, insnNewPC)) {
                memcpy(insns[i].code, &fixed, 4);
                insns[i].expandedSize = 4;
            } else {
                // 超出范围，用 LDR Xd, [PC, #4]; .quad target (12 bytes)
                int64_t immhi = (insn >> 5) & 0x7FFFF;
                int64_t immlo = (insn >> 29) & 0x3;
                int64_t imm = (immhi << 2) | immlo;
                if (imm & 0x100000) imm -= 0x200000;
                uint64_t target = (insnOrigPC & ~(uint64_t)0xFFF) + (imm << 12);
                uint32_t rd = insn & 0x1F;
                uint32_t ldrXd = 0x58000020 | rd; // LDR Xd, label (imm19=1, PC+4)
                memcpy(insns[i].code, &ldrXd, 4);
                memcpy(insns[i].code + 4, &target, 8);
                insns[i].expandedSize = 12;
                LOGH("[BRIDGE] ADRP out of range at insn %d, replaced with LDR X%d + .quad", i, rd);
            }
        } else if ((insn & 0xFC000000) == 0x14000000) {
            // B (unconditional branch)
            int64_t offset = insn & 0x3FFFFFF;
            if (offset & 0x2000000) offset -= 0x4000000;
            uint64_t target = insnOrigPC + (offset << 2);

            // 检查目标是否在 patched 区域内 [origPC, origPC+16)
            if (target >= origPC && target < origPC + 16) {
                int targetIdx = (int)((target - origPC) / 4);
                insns[i].internalBranch = targetIdx;
                insns[i].expandedSize = 4; // 第二遍生成 B 指令跳到 bridge 内对应位置
                LOGH("[BRIDGE] B at insn %d targets patched insn %d, will redirect within bridge", i, targetIdx);
            } else {
                uint32_t fixed = insn;
                if (fixupBBL(&fixed, insnOrigPC, 0)) {
                    memcpy(insns[i].code, &fixed, 4);
                    insns[i].expandedSize = 4;
                } else {
                    // 超出范围，用间接跳转: LDR X16, [PC, #8]; BR X16; .quad target (16 bytes)
                    const uint32_t ldrX16 = 0x58000050;
                    const uint32_t brX16 = 0xD61F0200;
                    memcpy(insns[i].code, &ldrX16, 4);
                    memcpy(insns[i].code + 4, &brX16, 4);
                    memcpy(insns[i].code + 8, &target, 8);
                    insns[i].expandedSize = 16;
                    LOGH("[BRIDGE] B out of range at insn %d, replaced with indirect branch", i);
                }
            }
        } else if ((insn & 0xFC000000) == 0x94000000) {
            // BL (branch with link) - 需要特殊处理 LR
            insns[i].isBL = true;
            int64_t offset = insn & 0x3FFFFFF;
            if (offset & 0x2000000) offset -= 0x4000000;
            insns[i].blTarget = insnOrigPC + (offset << 2);

            // 检查目标是否在 patched 区域内
            if (insns[i].blTarget >= origPC && insns[i].blTarget < origPC + 16) {
                int targetIdx = (int)((insns[i].blTarget - origPC) / 4);
                insns[i].internalBranch = targetIdx;
                insns[i].isBL = false; // 不需要特殊 LR 处理，直接 B 即可
                insns[i].expandedSize = 4;
                LOGH("[BRIDGE] BL at insn %d targets patched insn %d, will redirect within bridge", i, targetIdx);
            } else {
                insns[i].expandedSize = 24; // 固定 24 bytes: 3条指令 + 2个.quad
            }
        } else if ((insn & 0x3B000000) == 0x18000000) {
            // LDR (literal)
            uint32_t fixed = insn;
            if (fixupLDRLiteral(&fixed, insnOrigPC, 0)) {
                memcpy(insns[i].code, &fixed, 4);
                insns[i].expandedSize = 4;
            } else {
                // 超出范围，用间接加载: LDR X16, [PC, #8]; LDR Xt, [X16]; .quad target (16 bytes)
                uint32_t rt = insn & 0x1F;
                int64_t offset = insn & 0x7FFFF;
                if (offset & 0x40000) offset -= 0x80000;
                uint64_t target = insnOrigPC + (offset << 2);
                const uint32_t ldrX16 = 0x58000050;
                uint32_t ldrXtX16 = 0xF9400200 | rt;
                memcpy(insns[i].code, &ldrX16, 4);
                memcpy(insns[i].code + 4, &ldrXtX16, 4);
                memcpy(insns[i].code + 8, &target, 8);
                insns[i].expandedSize = 16;
                LOGH("[BRIDGE] LDR literal out of range at insn %d, replaced with indirect load", i);
            }
        } else {
            // 检查是否是条件分支 (CBZ/CBNZ/B.cond)
            uint64_t condTarget = getCondBranchTarget(insn, insnOrigPC);
            if (condTarget != 0) {
                // 条件分支指令
                if (condTarget >= origPC && condTarget < origPC + 16) {
                    // 目标在 patched 区域内，重定向到 bridge 中对应位置
                    int targetIdx = (int)((condTarget - origPC) / 4);
                    insns[i].internalBranch = targetIdx;
                    insns[i].expandedSize = 4;
                    LOGH("[BRIDGE] CondBranch at insn %d targets patched insn %d, will redirect within bridge", i, targetIdx);
                } else {
                    // 目标在 patched 区域外，尝试修复偏移
                    uint32_t fixed = insn;
                    if (fixupCondBranch(&fixed, insnOrigPC, 0)) {
                        // 先用 origPC 估算，第二遍再精确修复
                        memcpy(insns[i].code, &fixed, 4);
                        insns[i].expandedSize = 4;
                    } else {
                        // 超出修复范围，用反转条件 + 间接跳转替代
                        // CBZ target -> CBNZ skip; LDR X16, [PC, #8]; BR X16; .quad target
                        // CBNZ target -> CBZ skip; LDR X16, [PC, #8]; BR X16; .quad target
                        // B.cond target -> 无法反转，暂不支持
                        uint32_t inverted = invertCondBranch(insn);
                        if (inverted != 0) {
                            // 标记为条件分支间接跳转，第二遍生成代码
                            insns[i].isCondBranchIndirect = true;
                            insns[i].condBranchTarget = condTarget;
                            insns[i].expandedSize = 20; // CBNZ + LDR + BR + .quad
                            LOGH("[BRIDGE] CondBranch at insn %d out of range, will use inverted + indirect branch", i);
                        } else {
                            // B.cond 超出范围，无法处理，直接复制（会导致错误跳转）
                            memcpy(insns[i].code, &insn, 4);
                            insns[i].expandedSize = 4;
                            LOGH("[BRIDGE] WARNING: B.cond at insn %d out of range, cannot fixup!", i);
                        }
                    }
                }
            } else {
                // 非 PC-relative 指令，直接复制
                memcpy(insns[i].code, &insn, 4);
                insns[i].expandedSize = 4;
            }
        }
    }

    // 计算每条指令在 bridge 中的偏移
    int offsets[5] = {}; // offsets[4] = 所有指令之后的位置
    offsets[0] = 0;
    for (int i = 0; i < 4; i++) {
        offsets[i + 1] = offsets[i] + insns[i].expandedSize;
    }

    // 第二遍: 生成最终代码
    int pos = 0;
    for (int i = 0; i < 4; i++) {
        if (insns[i].isBL) {
            // BL 展开为:
            //   LDR X16, [PC, #12]    ; 加载 target (pos+12)
            //   LDR LR, [PC, #16]     ; 加载 return_addr (pos+20)
            //   BR X16                ; 跳转到 target
            //   .quad target          ; 8 bytes (pos+12~19)
            //   .quad return_addr     ; 8 bytes (pos+20~27)
            // return_addr = 下一条指令在 bridge 中的地址
            uint64_t returnAddr = (uint64_t)bridge + offsets[i + 1];
            uint32_t ldrX16 = 0x58000070; // LDR X16, label (imm19=3, PC+12)
            uint32_t ldrLR = 0x5800009E;  // LDR LR (X30), label (imm19=4, PC+16)
            const uint32_t brX16 = 0xD61F0200; // BR X16
            memcpy(bridge + pos, &ldrX16, 4); pos += 4;
            memcpy(bridge + pos, &ldrLR, 4); pos += 4;
            memcpy(bridge + pos, &brX16, 4); pos += 4;
            memcpy(bridge + pos, &insns[i].blTarget, 8); pos += 8;
            memcpy(bridge + pos, &returnAddr, 8); pos += 8;
            LOGH("[BRIDGE] BL at insn %d, replaced with LDR X16 + LDR LR + BR X16 (target=%p return=%p)",
                 i, (void *)insns[i].blTarget, (void *)returnAddr);
        } else {
            // 非 BL 指令: 需要修复 ADRP/B 的 PC-relative 偏移
            uint32_t insn;
            memcpy(&insn, saved + i * 4, 4);
            uintptr_t insnOrigPC = origPC + i * 4;
            uintptr_t insnNewPC = (uintptr_t)bridge + pos;

            if (insns[i].expandedSize == 4) {
                // 4 字节指令
                if (insns[i].internalBranch >= 0) {
                    // B/BL/CondBranch 目标在 patched 区域内，重定向到 bridge 中对应指令
                    int targetIdx = insns[i].internalBranch;
                    int64_t branchOffset = (offsets[targetIdx] - pos) / 4;
                    uint64_t condTarget = getCondBranchTarget(insn, insnOrigPC);
                    if (condTarget != 0) {
                        // 条件分支: 修复偏移，保持条件不变
                        uint32_t fixed = insn;
                        fixed = (fixed & 0xFF00001F) | (((uint32_t)branchOffset & 0x7FFFF) << 5);
                        memcpy(bridge + pos, &fixed, 4);
                        LOGH("[BRIDGE] Internal CondBranch at insn %d -> bridge insn %d (offset=%lld)",
                             i, targetIdx, (long long)branchOffset);
                    } else {
                        // 无条件 B
                        uint32_t bInsn = 0x14000000 | (uint32_t)(branchOffset & 0x3FFFFFF);
                        memcpy(bridge + pos, &bInsn, 4);
                        LOGH("[BRIDGE] Internal branch at insn %d -> bridge insn %d (offset=%lld)",
                             i, targetIdx, (long long)branchOffset);
                    }
                } else {
                    // 可能需要修复 PC-relative
                    uint32_t fixed = insn;
                    if ((insn & 0x9F000000) == 0x90000000) {
                        fixupADRP(&fixed, insnOrigPC, insnNewPC);
                    } else if ((insn & 0xFC000000) == 0x14000000) {
                        fixupBBL(&fixed, insnOrigPC, insnNewPC);
                    } else if ((insn & 0x3B000000) == 0x18000000) {
                        fixupLDRLiteral(&fixed, insnOrigPC, insnNewPC);
                    } else if (getCondBranchTarget(insn, insnOrigPC) != 0) {
                        fixupCondBranch(&fixed, insnOrigPC, insnNewPC);
                    }
                    memcpy(bridge + pos, &fixed, 4);
                }
            } else if (insns[i].isCondBranchIndirect) {
                // 条件分支超范围: 反转条件 + 间接跳转
                // CBZ target -> CBNZ skip; LDR X16, [PC, #8]; BR X16; .quad target
                // CBNZ target -> CBZ skip; LDR X16, [PC, #8]; BR X16; .quad target
                uint32_t inverted = invertCondBranch(insn);
                // 反转条件分支跳过间接跳转，跳到下一条 bridge 指令 (offsets[i+1])
                int64_t skipOffset = (offsets[i + 1] - pos) / 4;
                uint32_t invertedBranch = (inverted & 0xFF00001F) | (((uint32_t)skipOffset & 0x7FFFF) << 5);
                const uint32_t ldrX16 = 0x58000050; // LDR X16, [PC, #8]
                const uint32_t brX16 = 0xD61F0200;  // BR X16
                memcpy(bridge + pos, &invertedBranch, 4);
                memcpy(bridge + pos + 4, &ldrX16, 4);
                memcpy(bridge + pos + 8, &brX16, 4);
                memcpy(bridge + pos + 12, &insns[i].condBranchTarget, 8);
                LOGH("[BRIDGE] CondBranch indirect at insn %d, skip offset=%lld, target=%p",
                     i, (long long)skipOffset, (void *)insns[i].condBranchTarget);
            } else {
                // 展开后的指令，PC-relative 已经通过间接方式处理
                // ADRP 展开: LDR Xd, [PC, #4]; .quad target
                memcpy(bridge + pos, insns[i].code, insns[i].expandedSize);
            }
            pos += insns[i].expandedSize;
        }
    }

    // 追加跳转回原始函数 +16:
    //   LDR X16, [PC, #8]
    //   BR X16
    //   .quad (targetFn + 16)
    const uint32_t ldrInst = 0x58000050; // LDR X16, [PC, #8]
    const uint32_t brInst  = 0xD61F0200; // BR X16
    uint64_t retAddr = (uint64_t)targetFn + 16;
    memcpy(bridge + pos, &ldrInst, 4);
    memcpy(bridge + pos + 4, &brInst, 4);
    memcpy(bridge + pos + 8, &retAddr, 8);
    pos += 16;

    __builtin___clear_cache((char *)bridge, (char *)bridge + pos);
    return bridge;
}

static bool installInlineHook(void *targetFn, void *hookFn, void **origFn, int idx) {
    if (!targetFn || !hookFn || !origFn || idx < 0 || idx >= 8) return false;

    // 1. 保存原始 16 bytes
    memcpy(g_inlineHooks[idx].saved, targetFn, 16);
    g_inlineHooks[idx].targetFn = targetFn;
    g_inlineHooks[idx].hookFn = hookFn;

    // 2. 创建 bridge (跳板) 用于调用原始函数
    void *bridge = createBridge(targetFn, g_inlineHooks[idx].saved);
    if (!bridge) {
        LOGH("[INLINE] Bridge creation failed for idx=%d, skipping inline hook", idx);
        return false;
    }
    g_inlineHooks[idx].bridge = bridge;

    // 3. 修改目标函数页面为可写
    long pageSize = sysconf(_SC_PAGESIZE);
    uintptr_t page = (uintptr_t)targetFn & ~(uintptr_t)(pageSize - 1);
    if (mprotect((void *)page, (size_t)pageSize, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        LOGH("[INLINE] mprotect target page %p failed: %s", (void *)page, strerror(errno));
        return false;
    }

    // 4. 写入 trampoline 到目标函数入口
    const uint32_t ldrInst = 0x58000050; // LDR X16, [PC, #8]
    const uint32_t brInst  = 0xD61F0200; // BR X16
    uint64_t hookAddr = (uint64_t)hookFn;
    memcpy(targetFn, &ldrInst, 4);
    memcpy((uint8_t *)targetFn + 4, &brInst, 4);
    memcpy((uint8_t *)targetFn + 8, &hookAddr, 8);
    __builtin___clear_cache((char *)targetFn, (char *)targetFn + 16);

    // 5. orig_fn 指向 bridge (线程安全，无需 unpatch/repatch)
    *origFn = bridge;
    g_inlineHooks[idx].installed = true;

    LOGH("[INLINE] Patched %p -> %p bridge=%p (idx=%d)", targetFn, hookFn, bridge, idx);
    return true;
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
        { "H1", "net",    "CSRoleLoginRes",       "unpack",               {3, 4, -1},       0x25691F4, (void *)hook_H1, (void **)&orig_H1 },
        { "H2", "net",    "CSGuideFuncOpenedRes", "unpack",               {3, 4, -1},       0x24BFD84, (void *)hook_H2, (void **)&orig_H2 },
        { "H3", "S6Game", "FuncOpenMgr",          "CheckOpenList",        {1, 0, -1},       0x1FE57CC, (void *)hook_H3, (void **)&orig_H3 },
        { "H4", "S6Game", "FuncOpenMgr",          "ReqFuncOpenData",      {0, -1},          0x1FE5F2C, (void *)hook_H4, (void **)&orig_H4 },
        { "H5", "S6Game", "FuncOpenMgr",          "HandleNotifyFuncOpened", {2, 1, -1},     0x1FE56C8, (void *)hook_H5, (void **)&orig_H5 },
        { "H6", "S6Game", "FuncOpenMgr",          "CheckFuncOpen",        {2, -1},          0x1FE5FC4, (void *)hook_H6, (void **)&orig_H6 },
        { "H7", "S6Game", "FuncOpenNetMgr",       "NotifySysOpenCfg",     {2, -1},          0x1EB3374, (void *)hook_H7, (void **)&orig_H7 },
        { "H8", "S6Game", "FuncOpenMgr",          "Init",                 {0, -1},          0x1FE5598, (void *)hook_H8, (void **)&orig_H8 },
    };

    int ok = 0;
    for (int i = 0; i < 8; i++) {
        if (hookMethodMulti(entries[i])) {
            ok++;
        }
    }

    LOGH("Registered %d/8 hooks via methodPointer+vtable (g_forceFuncOpen=%d). Now installing inline hooks with bridge...", ok, g_forceFuncOpen ? 1 : 0);

    // ==================== Inline Code Patching (with Bridge) ====================
    // methodPointer+vtable 对 AOT 编译的直接调用无效 (il2cpp 去虚化)
    // 必须用 inline patching 修改函数入口机器码才能拦截所有调用
    // 使用 bridge (跳板) 替代 unpatch/repatch，线程安全:
    //   - bridge 复制原始指令并修复 PC-relative，跳回原函数+16
    //   - hook 回调通过 bridge 调用原始函数，无需临时恢复指令
    //   - inline patch 永远不被移除，多线程安全
    for (int i = 0; i < 8; i++) {
        void *targetFn = *entries[i].orig_fn;
        if (!targetFn) {
            LOGH("[INLINE-%s] SKIP: orig_fn is null (method not found)", entries[i].tag);
            continue;
        }
        if (installInlineHook(targetFn, entries[i].fake_fn, entries[i].orig_fn, i)) {
            LOGH("[INLINE-%s] OK at %p bridge=%p", entries[i].tag, targetFn, *entries[i].orig_fn);
        } else {
            LOGH("[INLINE-%s] FAILED at %p (bridge creation failed, hook may not trigger for direct calls)", entries[i].tag, targetFn);
        }
    }

    LOGH("All hooks installed (methodPointer+vtable+inline-bridge). g_forceFuncOpen=%d. Waiting for game login...",
         g_forceFuncOpen ? 1 : 0);
}
