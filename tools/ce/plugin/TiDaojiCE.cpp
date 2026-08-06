// TiDaoji native Cheat Engine plugin (x86 + x64).
// Same role as x64dbg TiDaoji.dp64: menu + auto hide via kernel device.
// NOT PG-safe. Lab only.
//
// Exports (stdcall): CEPlugin_GetVersion / InitializePlugin / DisablePlugin
// Build: tools/ce/plugin/build_ce_plugin.bat

#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "cepluginsdk.h"
#include "../../../TiDaoji/TiDaoji.h"
#include "../../../TiDaoji/user_client.h"

static ExportedFunctions g_ef;
static int g_pluginId = -1;
static int g_menuHide = -1;
static int g_menuUnhide = -1;
static int g_menuStatus = -1;
static int g_menuSoft = -1;
static int g_menuToggleAuto = -1;
static int g_processWatch = -1;

static TiDaojiUserSettings g_settings;
static char g_iniPath[MAX_PATH] = "";
static volatile LONG g_autoHide = 1; // default on
static HINSTANCE g_hInst = nullptr;

static void LoadSettings()
{
    TiDaojiUserSettingsInit(&g_settings);
    g_settings.Type = 0xFFFu;
    if(g_iniPath[0])
        TiDaojiUserSettingsLoad(&g_settings, g_iniPath);
    if(g_iniPath[0])
    {
        g_autoHide = GetPrivateProfileIntA("TiDaoji", "AutoHide", 1, g_iniPath) ? 1 : 0;
    }
}

static void SaveSettings()
{
    if(!g_iniPath[0])
        return;
    TiDaojiUserSettingsSave(&g_settings, g_iniPath);
    WritePrivateProfileStringA("TiDaoji", "AutoHide", g_autoHide ? "1" : "0", g_iniPath);
}

static DWORD CurrentPid()
{
    if(g_ef.OpenedProcessID && *g_ef.OpenedProcessID)
        return *g_ef.OpenedProcessID;
    return 0;
}

static void Msg(const char* s)
{
    if(g_ef.ShowMessage)
        g_ef.ShowMessage((char*)s);
    else
        MessageBoxA(nullptr, s, "TiDaoji CE", MB_OK);
}

static bool DeviceCall(HIDE_COMMAND cmd, DWORD pid)
{
    DWORD err = 0;
    if(!TiDaojiUserCall(&g_settings, pid, cmd, &err))
    {
        char w32[200];
        TiDaojiFormatWin32(err, w32, sizeof(w32));
        char buf[320];
        sprintf_s(buf, "TiDaoji: command failed pid=%lu %s", (unsigned long)pid, w32);
        Msg(buf);
        return false;
    }
    return true;
}

static void __stdcall OnMenuHide(void)
{
    DWORD pid = CurrentPid();
    if(!pid)
    {
        Msg("TiDaoji: no opened process (select a process in CE first)");
        return;
    }
    if(DeviceCall(HidePid, pid))
    {
        char buf[160];
        sprintf_s(buf, "TiDaoji: HidePid OK pid=%lu type=0x%08X", (unsigned long)pid, g_settings.Type);
        Msg(buf);
    }
}

static void __stdcall OnMenuUnhide(void)
{
    DWORD pid = CurrentPid();
    if(!pid)
    {
        Msg("TiDaoji: no opened process");
        return;
    }
    if(DeviceCall(UnhidePid, pid))
    {
        char buf[120];
        sprintf_s(buf, "TiDaoji: UnhidePid OK pid=%lu", (unsigned long)pid);
        Msg(buf);
    }
}

static void __stdcall OnMenuStatus(void)
{
    char path[160];
    TiDaojiDevicePath(&g_settings, path, sizeof(path));
    HANDLE h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    char buf[280];
    if(h == INVALID_HANDLE_VALUE)
    {
        char w32[160];
        TiDaojiFormatWin32(GetLastError(), w32, sizeof(w32));
        sprintf_s(buf, "TiDaoji: %s OPEN FAIL %s (start TiDaoji.sys)", path, w32);
    }
    else
    {
        CloseHandle(h);
        sprintf_s(buf, "TiDaoji: %s OPEN OK  type=0x%08X autoHide=%d pid=%lu",
                  path, g_settings.Type, (int)g_autoHide, (unsigned long)CurrentPid());
    }
    Msg(buf);
}

static void __stdcall OnMenuSoftUnload(void)
{
    HWND owner = nullptr;
    if(g_ef.GetMainWindowHandle)
        owner = (HWND)g_ef.GetMainWindowHandle();
    if(MessageBoxA(owner,
                   "SoftUnload tears down InfinityHook / device. Continue?",
                   "TiDaoji CE", MB_ICONWARNING | MB_YESNO) != IDYES)
        return;
    if(DeviceCall(SoftUnload, 0))
        Msg("TiDaoji: SoftUnload OK");
}

static void __stdcall OnMenuToggleAuto(void)
{
    g_autoHide = g_autoHide ? 0 : 1;
    SaveSettings();
    char buf[80];
    sprintf_s(buf, "TiDaoji: AutoHide on process open = %s", g_autoHide ? "ON" : "OFF");
    Msg(buf);
}

// Process create/destroy — separate thread; no ShowMessage.
static void __stdcall OnProcessWatch(ULONG processid, ULONG peprocess, BOOL Created)
{
    UNREFERENCED_PARAMETER(peprocess);
    if(!Created || !g_autoHide)
        return;
    if(!processid)
        return;
    // Only hide if CE is attached to this pid (or about to be)
    DWORD opened = CurrentPid();
    if(opened != 0 && opened != processid)
        return;
    DeviceCall(HidePid, processid);
}

// Also: when CE switches process via OpenProcess API path — menu Hide is enough.
// Register a second approach: ptProcesswatcherEvent covers create; user can Hide after open.

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if(reason == DLL_PROCESS_ATTACH)
    {
        g_hInst = hModule;
        DisableThreadLibraryCalls(hModule);
        TiDaojiIniPathFromModule(hModule, "TiDaojiCE", g_iniPath, sizeof(g_iniPath));
        LoadSettings();
    }
    return TRUE;
}

BOOL __stdcall CEPlugin_GetVersion(PPluginVersion pv, int sizeofpluginversion)
{
    UNREFERENCED_PARAMETER(sizeofpluginversion);
    static char name[] = "TiDaoji (InfinityHook hide) - NOT PG-safe";
    pv->version = CESDK_VERSION;
    pv->pluginname = name;
    return TRUE;
}

BOOL __stdcall CEPlugin_InitializePlugin(PExportedFunctions ef, int pluginid)
{
    g_pluginId = pluginid;
    g_ef = *ef;
    if(g_ef.sizeofExportedFunctions != sizeof(ExportedFunctions))
    {
        // Soft mismatch: still try (CE versions vary); warn only
        // return FALSE would refuse load on minor SDK drift
    }

    LoadSettings();

    MAINMENUPLUGIN_INIT m;

    m.name = "TiDaoji: Hide opened process";
    m.callbackroutine = OnMenuHide;
    m.shortcut = "Ctrl+Shift+H";
    g_menuHide = g_ef.RegisterFunction(pluginid, ptMainMenu, &m);

    m.name = "TiDaoji: Unhide opened process";
    m.callbackroutine = OnMenuUnhide;
    m.shortcut = "Ctrl+Shift+U";
    g_menuUnhide = g_ef.RegisterFunction(pluginid, ptMainMenu, &m);

    m.name = "TiDaoji: Driver status";
    m.callbackroutine = OnMenuStatus;
    m.shortcut = nullptr;
    g_menuStatus = g_ef.RegisterFunction(pluginid, ptMainMenu, &m);

    m.name = "TiDaoji: SoftUnload";
    m.callbackroutine = OnMenuSoftUnload;
    m.shortcut = nullptr;
    g_menuSoft = g_ef.RegisterFunction(pluginid, ptMainMenu, &m);

    m.name = "TiDaoji: Toggle AutoHide";
    m.callbackroutine = OnMenuToggleAuto;
    m.shortcut = nullptr;
    g_menuToggleAuto = g_ef.RegisterFunction(pluginid, ptMainMenu, &m);

    PROCESSWATCHERPLUGIN_INIT pw;
    pw.callbackroutine = OnProcessWatch;
    g_processWatch = g_ef.RegisterFunction(pluginid, ptProcesswatcherEvent, &pw);

    if(g_menuHide == -1)
        return FALSE;

    char hello[200];
    sprintf_s(hello, "TiDaoji CE plugin enabled (type=0x%08X autoHide=%d). Device \\\\.\\%s",
              g_settings.Type, (int)g_autoHide, g_settings.DriverName);
    Msg(hello);
    return TRUE;
}

BOOL __stdcall CEPlugin_DisablePlugin(void)
{
    SaveSettings();
    if(g_ef.UnregisterFunction)
    {
        if(g_menuHide != -1)
            g_ef.UnregisterFunction(g_pluginId, g_menuHide);
        if(g_menuUnhide != -1)
            g_ef.UnregisterFunction(g_pluginId, g_menuUnhide);
        if(g_menuStatus != -1)
            g_ef.UnregisterFunction(g_pluginId, g_menuStatus);
        if(g_menuSoft != -1)
            g_ef.UnregisterFunction(g_pluginId, g_menuSoft);
        if(g_menuToggleAuto != -1)
            g_ef.UnregisterFunction(g_pluginId, g_menuToggleAuto);
        if(g_processWatch != -1)
            g_ef.UnregisterFunction(g_pluginId, g_processWatch);
    }
    return TRUE;
}
