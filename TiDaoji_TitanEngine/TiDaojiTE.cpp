#include <windows.h>
#include <stdio.h>
#include "TitanEngine/TitanEngine.h"
#include "../TiDaoji/user_client.h"

#ifdef _WIN64
#pragma comment(lib, "TitanEngine/TitanEngine_x64.lib")
#else
#pragma comment(lib, "TitanEngine/TitanEngine_x86.lib")
#endif

static HINSTANCE g_hInst = nullptr;
static char g_iniPath[MAX_PATH] = "";
static TiDaojiUserSettings g_settings;
static bool g_settingsLoaded = false;
static bool g_reportedOpenFail = false;
static DWORD g_processId = 0;

static void EnsureSettings()
{
    if(g_settingsLoaded)
        return;
    if(!g_iniPath[0])
        TiDaojiIniPathFromModule(g_hInst ? g_hInst : GetModuleHandleA(nullptr), "TiDaojiTE", g_iniPath, sizeof(g_iniPath));
    TiDaojiUserSettingsLoad(&g_settings, g_iniPath);
    g_settingsLoaded = true;
    char msg[320];
    sprintf_s(msg, "[TIDAOJI][TE] settings DriverName=%s Type=0x%08X ini=%s\n",
              g_settings.DriverName, g_settings.Type, g_iniPath);
    OutputDebugStringA(msg);
}

static void TiDaojiCall(DWORD processId, HIDE_COMMAND command)
{
    EnsureSettings();
    DWORD err = 0;
    if(TiDaojiUserCall(&g_settings, processId, command, &err))
    {
        char msg[160];
        sprintf_s(msg, "[TIDAOJI][TE] cmd=%d pid=%lu ok\n", (int)command, processId);
        OutputDebugStringA(msg);
        return;
    }
    char detail[192];
    TiDaojiFormatWin32(err, detail, sizeof(detail));
    char msg[320];
    sprintf_s(msg, "[TIDAOJI][TE] cmd=%d pid=%lu FAIL %s\n", (int)command, processId, detail);
    OutputDebugStringA(msg);
    if(!g_reportedOpenFail)
    {
        g_reportedOpenFail = true;
        char box[400];
        sprintf_s(box,
                  "TiDaoji TitanEngine plugin failed.\n%s\n\\\\.\\%s\nIni: %s\n[TiDaoji] DriverName / Type\nNOT PG-safe; no dual-IH with CR.",
                  detail, g_settings.DriverName, g_iniPath);
        MessageBoxA(nullptr, box, "TiDaoji TE", MB_ICONWARNING | MB_SYSTEMMODAL);
    }
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID)
{
    if(fdwReason == DLL_PROCESS_ATTACH)
    {
        g_hInst = hinstDLL;
        DisableThreadLibraryCalls(hinstDLL);
        TiDaojiIniPathFromModule(hinstDLL, "TiDaojiTE", g_iniPath, sizeof(g_iniPath));
    }
    return TRUE;
}

extern "C" __declspec(dllexport) bool TitanRegisterPlugin(char* szPluginName, LPDWORD titanPluginMajorVersion, LPDWORD titanPluginMinorVersion)
{
    strcpy_s(szPluginName, 64, "TiDaoji");
    *titanPluginMajorVersion = 2;
    *titanPluginMinorVersion = 0;
    EnsureSettings();
    OutputDebugStringA("[TIDAOJI][TE] TitanRegisterPlugin 2.0\n");
    return true;
}

extern "C" __declspec(dllexport) void TitanDebuggingCallBack(LPDEBUG_EVENT debugEvent, int CallReason)
{
    static bool pebHidden = false;
    static HANDLE hProcess = nullptr;

    switch(CallReason)
    {
    case UE_PLUGIN_CALL_REASON_EXCEPTION:
        if(!debugEvent)
            break;
        switch(debugEvent->dwDebugEventCode)
        {
        case CREATE_PROCESS_DEBUG_EVENT:
            hProcess = debugEvent->u.CreateProcessInfo.hProcess;
            g_processId = debugEvent->dwProcessId;
            g_reportedOpenFail = false;
            TiDaojiCall(g_processId, HidePid);
            pebHidden = false;
            break;

        case EXCEPTION_DEBUG_EVENT:
            if(debugEvent->u.Exception.ExceptionRecord.ExceptionCode == STATUS_BREAKPOINT)
            {
                if(!pebHidden && hProcess)
                {
                    HideDebugger(hProcess, UE_HIDE_PEBONLY);
                    pebHidden = true;
                }
            }
            break;

        case EXIT_PROCESS_DEBUG_EVENT:
            if(debugEvent->dwProcessId == g_processId)
                TiDaojiCall(g_processId, UnhidePid);
            break;
        }
        break;

    case UE_PLUGIN_CALL_REASON_POSTDEBUG:
        if(g_processId)
            TiDaojiCall(g_processId, UnhidePid);
        break;
    }
}
