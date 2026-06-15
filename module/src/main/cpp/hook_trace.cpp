//
// hook_trace.cpp
// 追踪 RoleLoginRes 解码与 FuncOpenMgr 菜单功能激活流程
//
// 用法: adb logcat -s HookTrace
//
// RVA 来源: hotupdate/csharp/dump.cs
// 字段偏移来源: IDA Pro 逆向分析 + Frida JS 版验证
//

#include "hook_trace.h"
#include "il2cpp_dump.h"
#include "log.h"

#include <dobby.h>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstring>

// ==================== RVA 配置 (来自 dump.cs) ====================

struct HookRVA {
    uint64_t rva;
    const char *name;
};

static const HookRVA RVA_TABLE[] = {
    { 0x251E144, "net.CSRoleLoginRes.unpack" },           // H1
    { 0x24770A0, "net.CSGuideFuncOpenedRes.unpack" },     // H2
    { 0x1FB21A0, "S6Game.FuncOpenMgr.CheckOpenList" },    // H3
    { 0x1FB2900, "S6Game.FuncOpenMgr.ReqFuncOpenData" },  // H4
    { 0x1FB209C, "S6Game.FuncOpenMgr.HandleNotifyFuncOpened" }, // H5
    { 0x1FB29A0, "S6Game.FuncOpenMgr.CheckFuncOpen" },    // H6
    { 0x1E858B8, "S6Game.FuncOpenNetMgr.NotifySysOpenCfg" }, // H7
};

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
// ARM64 calling convention: x0=self, x1=srcBuf, x2=cutVer, x3=stack
typedef void (*H1_Fn)(void *self, void *srcBuf, uint32_t cutVer, void *stack);
static H1_Fn orig_H1 = nullptr;

static void hook_H1(void *self, void *srcBuf, uint32_t cutVer, void *stack) {
    orig_H1(self, srcBuf, cutVer, stack);
    if (!shouldLog(0)) return;
    // CSRoleLoginRes 字段布局:
    //   +0x10: CSRoleData RoleData (值类型, 内嵌)
    //   +0x18: ProtoResult Result (int32 枚举)
    //   +0x20: uint LastLogoutTime
    //   +0x24: byte WearShape
    //   +0x30: int32 RegionID
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
    // CSGuideFuncOpenedRes 字段布局:
    //   +0x10: int32 FuncOpenedCnt
    //   +0x18: uint[] FuncOpenedList
    auto cnt = safeReadS32((const uint8_t *)self + 0x10);
    auto listPtr = safeReadPtr((const uint8_t *)self + 0x18);
    uint32_t buf[200];
    int32_t arrLen = 0;
    readUintArray(listPtr, &arrLen, buf, 200);
    // 格式化输出
    char out[1024];
    int pos = 0;
    pos += snprintf(out + pos, sizeof(out) - pos, "FuncOpenedCnt=%d List=[", cnt);
    int printCount = arrLen > 50 ? 50 : arrLen;
    for (int i = 0; i < printCount; i++) {
        pos += snprintf(out + pos, sizeof(out) - pos, "%u,", buf[i]);
    }
    if (arrLen > 50) pos += snprintf(out + pos, sizeof(out) - pos, "...(%d more)", arrLen - 50);
    if (printCount > 0) pos--; // remove trailing comma
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
    // FuncOpenMgr 字段布局:
    //   +0x10: bool m_initOpenList (byte)
    //   +0x18: List<uint>* m_openFuncList
    auto initOpenList = safeReadU8((const uint8_t *)self + 0x10) != 0;
    auto openFuncListPtr = safeReadPtr((const uint8_t *)self + 0x18);
    int32_t listSize = 0;
    uint32_t listBuf[200];
    readUintList(openFuncListPtr, &listSize, listBuf, 200);

    // svrData (CSGuideFuncOpenedRes): +0x10=cnt, +0x18=uint[]
    char svrInfo[512] = "<null>";
    if (svrData) {
        auto svrCnt = safeReadS32((const uint8_t *)svrData + 0x10);
        auto svrListPtr = safeReadPtr((const uint8_t *)svrData + 0x18);
        int32_t svrArrLen = 0;
        uint32_t svrBuf[200];
        readUintArray(svrListPtr, &svrArrLen, svrBuf, 200);
        int pos = 0;
        pos += snprintf(svrInfo, sizeof(svrInfo), "FuncOpenedCnt=%d List=[", svrCnt);
        int printCount = svrArrLen > 30 ? 30 : svrArrLen;
        for (int i = 0; i < printCount; i++) {
            pos += snprintf(svrInfo + pos, sizeof(svrInfo) - pos, "%u,", svrBuf[i]);
        }
        if (printCount > 0) pos--;
        snprintf(svrInfo + pos, sizeof(svrInfo) - pos, "]");
    }

    // 格式化 m_openFuncList
    char listOut[512];
    int lpos = 0;
    lpos += snprintf(listOut, sizeof(listOut), "size=%d [", listSize);
    int printCount = listSize > 50 ? 50 : listSize;
    for (int i = 0; i < printCount; i++) {
        lpos += snprintf(listOut + lpos, sizeof(listOut) - lpos, "%u,", listBuf[i]);
    }
    if (printCount > 0) lpos--;
    snprintf(listOut + lpos, sizeof(listOut) - lpos, "]");

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
    // CSPkg.Head at msg+0x10, Cmd at head+0x12 (uint16 BE)
    uint16_t cmd = 0;
    auto head = safeReadPtr((const uint8_t *)msg + 0x10);
    if (head) {
        memcpy(&cmd, (const uint8_t *)head + 0x12, 2);
    }
    LOGH("[H5] FuncOpenMgr.HandleNotifyFuncOpened: result=%d msg.Cmd=0x%04X(%u)",
         result, cmd, cmd);
}

// --- H6: FuncOpenMgr.CheckFuncOpen(self, funcType, showTips) -> bool ---
// ARM64: x0=self, x1=funcType(uint32), x2=showTips(bool), ret=w0(uint8/bool)
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

// ==================== Hook 注册 ====================

struct HookEntry {
    uint64_t rva;
    const char *name;
    void *fake_fn;
    void **orig_fn;
};

void register_trace_hooks() {
    auto base = get_il2cpp_base();
    if (!base) {
        LOGH("ERROR: il2cpp_base is 0, cannot register hooks");
        return;
    }

    LOGH("il2cpp_base=0x%" PRIx64, base);

    HookEntry entries[] = {
        { RVA_TABLE[0].rva, RVA_TABLE[0].name, (void *)hook_H1, (void **)&orig_H1 },
        { RVA_TABLE[1].rva, RVA_TABLE[1].name, (void *)hook_H2, (void **)&orig_H2 },
        { RVA_TABLE[2].rva, RVA_TABLE[2].name, (void *)hook_H3, (void **)&orig_H3 },
        { RVA_TABLE[3].rva, RVA_TABLE[3].name, (void *)hook_H4, (void **)&orig_H4 },
        { RVA_TABLE[4].rva, RVA_TABLE[4].name, (void *)hook_H5, (void **)&orig_H5 },
        { RVA_TABLE[5].rva, RVA_TABLE[5].name, (void *)hook_H6, (void **)&orig_H6 },
        { RVA_TABLE[6].rva, RVA_TABLE[6].name, (void *)hook_H7, (void **)&orig_H7 },
    };

    int ok = 0;
    for (int i = 0; i < 7; i++) {
        auto target = (void *)(base + entries[i].rva);
        int ret = DobbyHook(target, entries[i].fake_fn, entries[i].orig_fn);
        if (ret == 0) {
            LOGH("[H%d] OK %s @ %p", i + 1, entries[i].name, target);
            ok++;
        } else {
            LOGH("[H%d] FAIL %s @ %p (DobbyHook ret=%d)", i + 1, entries[i].name, target, ret);
        }
    }

    LOGH("Registered %d/7 hooks. Waiting for game login...", ok);
}
