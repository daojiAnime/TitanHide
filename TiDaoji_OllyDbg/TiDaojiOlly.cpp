#include <windows.h>
#include <stdio.h>
#include "../TiDaoji/user_client.h"
#include "pebhider.h"

// OllyDbg definitions (no full SDK required for these exports)
#define PLUGIN_VERSION1 110
#define PLUGIN_VERSION2 0x2010001
#define PP_MAIN 3
#define PP_TERMINATED 2

static HINSTANCE g_hInst = nullptr;
static char g_iniPath[MAX_PATH] = "";
static TiDaojiUserSettings g_settings;
static DWORD g_processId = 0;
static bool g_settingsLoaded = false;
static bool g_reportedOpenFail = false;

static void EnsureSettings()
{
    if(g_settingsLoaded)
        return;
    if(!g_iniPath[0])
        TiDaojiIniPathFromModule(g_hInst ? g_hInst : GetModuleHandleA(nullptr), "TiDaojiOlly", g_iniPath, sizeof(g_iniPath));
    TiDaojiUserSettingsLoad(&g_settings, g_iniPath);
    g_settingsLoaded = true;
    char msg[320];
    sprintf_s(msg, "[TIDAOJI][OLLY] settings DriverName=%s Type=0x%08X ini=%s\n",
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
        sprintf_s(msg, "[TIDAOJI][OLLY] cmd=%d pid=%lu ok\n", (int)command, processId);
        OutputDebugStringA(msg);
        return;
    }
    char detail[192];
    TiDaojiFormatWin32(err, detail, sizeof(detail));
    char msg[320];
    sprintf_s(msg, "[TIDAOJI][OLLY] cmd=%d pid=%lu FAIL %s device=\\\\.\\%s\n",
              (int)command, processId, detail, g_settings.DriverName);
    OutputDebugStringA(msg);
    if(!g_reportedOpenFail)
    {
        g_reportedOpenFail = true;
        char box[384];
        sprintf_s(box,
                  "TiDaoji call failed.\n%s\nDevice \\\\.\\%s\nEdit %s [TiDaoji] DriverName/Type\nSee DSU runbook (profile A).",
                  detail, g_settings.DriverName, g_iniPath);
        MessageBoxA(nullptr, box, "TiDaoji Olly", MB_ICONWARNING | MB_SYSTEMMODAL);
    }
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID)
{
    if(fdwReason == DLL_PROCESS_ATTACH)
    {
        g_hInst = hinstDLL;
        DisableThreadLibraryCalls(hinstDLL);
        TiDaojiIniPathFromModule(hinstDLL, "TiDaojiOlly", g_iniPath, sizeof(g_iniPath));
    }
    return TRUE;
}

// --- OllyDbg1 ---
extern "C" __declspec(dllexport) int _ODBG_Plugindata(char name[32])
{
    strcpy_s(name, 32, "TiDaoji");
    return PLUGIN_VERSION1;
}

extern "C" __declspec(dllexport) int _ODBG_Plugininit(int ollyVersion, HWND hwndDlg, unsigned long* features)
{
    UNREFERENCED_PARAMETER(hwndDlg);
    UNREFERENCED_PARAMETER(features);
    if(ollyVersion < PLUGIN_VERSION1)
        return -1;
    EnsureSettings();
    OutputDebugStringA("[TIDAOJI][OLLY] Plugininit v2 — NOT PG-safe; no dual-IH with CR\n");
    return 0;
}

extern "C" __declspec(dllexport) void _ODBG_Pluginmainloop(DEBUG_EVENT* DebugEvent)
{
    static bool pebHidden = false;
    static HANDLE hProcess = nullptr;
    if(!DebugEvent)
        return;
    switch(DebugEvent->dwDebugEventCode)
    {
    case CREATE_PROCESS_DEBUG_EVENT:
        hProcess = DebugEvent->u.CreateProcessInfo.hProcess;
        g_processId = DebugEvent->dwProcessId;
        g_reportedOpenFail = false; // allow one dialog per session
        TiDaojiCall(g_processId, HidePid);
        pebHidden = false;
        break;

    case EXCEPTION_DEBUG_EVENT:
        if(DebugEvent->u.Exception.ExceptionRecord.ExceptionCode == STATUS_BREAKPOINT)
        {
            if(!pebHidden && hProcess)
            {
                HidePEB(hProcess, true);
                pebHidden = true;
            }
        }
        break;

    case EXIT_PROCESS_DEBUG_EVENT:
        if(DebugEvent->dwProcessId == g_processId)
            TiDaojiCall(g_processId, UnhidePid);
        break;
    }
}

extern "C" __declspec(dllexport) int _ODBG_Pausedex(int reason, int extdata, void* reg, DEBUG_EVENT* DebugEvent)
{
    UNREFERENCED_PARAMETER(extdata);
    UNREFERENCED_PARAMETER(reg);
    UNREFERENCED_PARAMETER(DebugEvent);
    if((reason & PP_MAIN) == PP_TERMINATED)
        TiDaojiCall(g_processId, UnhidePid);
    return 0;
}

// --- OllyDbg2 ---
extern "C" __declspec(dllexport) void _ODBG2_Pluginmainloop(DEBUG_EVENT* DebugEvent)
{
    _ODBG_Pluginmainloop(DebugEvent);
}

extern "C" __declspec(dllexport) int _ODBG2_Pluginquery(int ollyVersion, unsigned long* features,
                                                        wchar_t pluginname[32], wchar_t pluginversion[32])
{
    UNREFERENCED_PARAMETER(ollyVersion);
    UNREFERENCED_PARAMETER(features);
    wcscpy_s(pluginname, 32, L"TiDaoji");
    wcscpy_s(pluginversion, 32, L"2.0");
    return PLUGIN_VERSION2;
}
