//
// hook_trace.h
// 追踪 RoleLoginRes 解码与 FuncOpenMgr 菜单功能激活流程
//

#ifndef HOOK_TRACE_H
#define HOOK_TRACE_H

#include <stdint.h>

// 在 il2cpp_dump 完成后调用，注册所有 hook
void register_trace_hooks();

#endif // HOOK_TRACE_H
