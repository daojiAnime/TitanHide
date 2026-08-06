#ifndef _SSDT_H
#define _SSDT_H

#include "_global.h"
#include "hooklib.h"

// GetFunctionAddress：生产只读（InfinityHook 原件解析）。
// Hook/Unhook：默认实现为 no-op（见 ssdt.cpp）；仅 TIDAOJI_ALLOW_SSDT_FALLBACK 启用真实写。
class SSDT
{
public:
    static PVOID GetFunctionAddress(const char* apiname);
    static HOOK Hook(const char* apiname, void* newfunc);
    static void Hook(HOOK hHook);
    static void Unhook(HOOK hHook, bool free = false);
};

#endif