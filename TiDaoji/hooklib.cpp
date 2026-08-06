#include "hooklib.h"
#include "log.h"

// 生产 hide 不走 hooklib；默认冻结写路径，与 SSDT::Hook 一致。
// 仅 TIDAOJI_ALLOW_SSDT_FALLBACK 时编译真实 cave 补丁。
#ifndef TIDAOJI_ALLOW_SSDT_FALLBACK

HOOK Hooklib::Hook(PVOID api, void* newfunc)
{
    UNREFERENCED_PARAMETER(api);
    UNREFERENCED_PARAMETER(newfunc);
    Log("[TIDAOJI] Hooklib::Hook frozen (IH production)\r\n");
    return 0;
}

bool Hooklib::Hook(HOOK hook)
{
    UNREFERENCED_PARAMETER(hook);
    Log("[TIDAOJI] Hooklib::Hook(HOOK) frozen (IH production)\r\n");
    return false;
}

bool Hooklib::Unhook(HOOK hook, bool free)
{
    UNREFERENCED_PARAMETER(hook);
    UNREFERENCED_PARAMETER(free);
    Log("[TIDAOJI] Hooklib::Unhook frozen (IH production)\r\n");
    return false;
}

#else // TIDAOJI_ALLOW_SSDT_FALLBACK

static HOOK hook_internal(ULONG_PTR addr, void* newfunc)
{
    //allocate structure
    HOOK hook = (HOOK)RtlAllocateMemory(true, sizeof(HOOKSTRUCT));
    //set hooking address
    hook->addr = addr;
    //set hooking opcode
#ifdef _WIN64
    hook->hook.mov = 0xB848;
#else
    hook->hook.mov = 0xB8;
#endif
    hook->hook.addr = (ULONG_PTR)newfunc;
    hook->hook.push = 0x50;
    hook->hook.ret = 0xc3;
    //set original data
    RtlCopyMemory(&hook->orig, (const void*)addr, sizeof(HOOKOPCODES));
    if(!NT_SUCCESS(RtlSuperCopyMemory((void*)addr, &hook->hook, sizeof(HOOKOPCODES))))
    {
        RtlFreeMemory(hook);
        return 0;
    }
    return hook;
}

HOOK Hooklib::Hook(PVOID api, void* newfunc)
{
    ULONG_PTR addr = (ULONG_PTR)api;
    if(!addr)
        return 0;
    Log("[TIDAOJI] hook(0x%p, 0x%p)\r\n", addr, newfunc);
    return hook_internal(addr, newfunc);
}

bool Hooklib::Hook(HOOK hook)
{
    if(!hook)
        return false;
    return (NT_SUCCESS(RtlSuperCopyMemory((void*)hook->addr, &hook->hook, sizeof(HOOKOPCODES))));
}

bool Hooklib::Unhook(HOOK hook, bool free)
{
    if(!hook || !hook->addr)
        return false;
    if(NT_SUCCESS(RtlSuperCopyMemory((void*)hook->addr, hook->orig, sizeof(HOOKOPCODES))))
    {
        if(free)
            RtlFreeMemory(hook);
        return true;
    }
    return false;
}

#endif // TIDAOJI_ALLOW_SSDT_FALLBACK
