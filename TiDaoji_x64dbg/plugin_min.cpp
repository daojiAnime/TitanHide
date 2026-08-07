// Absolute minimum x64dbg plugin - no BridgeSetting, no CB_* debug events, no dialog.
// Used to verify load safety against host crash.
#include <windows.h>
#include <stdio.h>
#include "pluginsdk/bridgemain.h"
#include "pluginsdk/_plugins.h"

#ifdef _WIN64
#pragma comment(lib, "pluginsdk/x64dbg.lib")
#pragma comment(lib, "pluginsdk/x64bridge.lib")
#else
#pragma comment(lib, "pluginsdk/x32dbg.lib")
#pragma comment(lib, "pluginsdk/x32bridge.lib")
#endif

#define PLUG_EXPORT extern "C" __declspec(dllexport)
#define PLUGIN_NAME "TiDaoji"

static int g_pluginHandle = 0;
static HINSTANCE g_hInst = nullptr;

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID)
{
    if(reason == DLL_PROCESS_ATTACH)
    {
        g_hInst = h;
        DisableThreadLibraryCalls(h);
    }
    return TRUE;
}

PLUG_EXPORT bool pluginit(PLUG_INITSTRUCT* initStruct)
{
    if(!initStruct)
        return false;
    initStruct->pluginVersion = 6;
    initStruct->sdkVersion = PLUG_SDKVERSION;
    strncpy_s(initStruct->pluginName, PLUGIN_NAME, _TRUNCATE);
    g_pluginHandle = initStruct->pluginHandle;
    // NO bridge API calls here
    return true;
}

PLUG_EXPORT bool plugstop()
{
    return true;
}

PLUG_EXPORT void plugsetup(PLUG_SETUPSTRUCT* setupStruct)
{
    if(!setupStruct || !setupStruct->hMenu)
        return;
    _plugin_menuaddentry(setupStruct->hMenu, 1, "TiDaoji (min load test)");
    // delay log until setup (bridge fully up)
    _plugin_logputs("[TiDaoji] MIN plugin loaded OK (no auto-callbacks)");
}

PLUG_EXPORT void CBMENUENTRY(CBTYPE, PLUG_CB_MENUENTRY* info)
{
    if(!info)
        return;
    if(info->hEntry == 1)
        _plugin_logputs("[TiDaoji] menu click OK - full features next if stable");
}
