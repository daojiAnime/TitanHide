// TiDaoji x64dbg plugin v7
// Critical: do NOT export CBCREATEPROCESS/CBSYSTEMBREAKPOINT/etc.
// Register callbacks via _plugin_registercallback in plugsetup only.
// pluginit: no Bridge API. /MD CRT. No std::string.
#include "plugin.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "../TiDaoji/TiDaoji.h"
#include "resource.h"

static DWORD g_pid = 0;
static int g_hidden = 0;
static char g_driverName[64] = "TiDaoji";
static const ULONG kDefaultType = 0xFFFu;

static ULONG GetTypeMask()
{
    duint options = 0;
    if(!BridgeSettingGetUint("TiDaoji", "Options", &options))
        options = kDefaultType;
    return (ULONG)options;
}

static int AutoHideOn()
{
    duint v = 0;
    if(!BridgeSettingGetUint("TiDaoji", "AutoHide", &v))
        return 0;
    return v ? 1 : 0;
}

static bool OpenDev(HANDLE* out)
{
    char path[96];
    _snprintf_s(path, _TRUNCATE, "\\\\.\\%s", g_driverName);
    HANDLE h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if(h == INVALID_HANDLE_VALUE)
    {
        _plugin_logprintf("[TiDaoji] open %s fail Win32=%lu\n", path, GetLastError());
        *out = INVALID_HANDLE_VALUE;
        return false;
    }
    *out = h;
    return true;
}

static bool DevWrite(HIDE_COMMAND cmd, DWORD pid, ULONG type)
{
    HANDLE h = INVALID_HANDLE_VALUE;
    if(!OpenDev(&h))
        return false;
    HIDE_INFO hi = {};
    hi.Command = cmd;
    hi.Pid = pid;
    hi.Type = type;
    DWORD n = 0;
    BOOL ok = WriteFile(h, &hi, sizeof(hi), &n, nullptr);
    if(!ok)
        _plugin_logprintf("[TiDaoji] WriteFile fail Win32=%lu\n", GetLastError());
    else
        _plugin_logprintf("[TiDaoji] ok cmd=%d pid=%lu type=0x%X\n", (int)cmd, (unsigned long)pid, type);
    CloseHandle(h);
    return ok == TRUE;
}

static void NotePid(DWORD p)
{
    if(p != g_pid)
    {
        g_hidden = 0;
        g_pid = p;
    }
}

// --- commands ---
static bool cbHide(int, char**)
{
    if(!g_pid)
    {
        _plugin_logputs("[TiDaoji] no PID");
        return false;
    }
    if(DevWrite(HidePid, g_pid, GetTypeMask()))
    {
        g_hidden = 1;
        DbgCmdExecDirect("hide");
        return true;
    }
    return false;
}

static bool cbUnhide(int, char**)
{
    if(!g_pid)
        return false;
    if(DevWrite(UnhidePid, g_pid, GetTypeMask()))
    {
        g_hidden = 0;
        return true;
    }
    return false;
}

static bool cbUnhideAll(int, char**)
{
    if(DevWrite(UnhideAll, 0, 0))
    {
        g_hidden = 0;
        return true;
    }
    return false;
}

static bool cbSoft(int, char**)
{
    return DevWrite(SoftUnload, 0, 0);
}

static bool cbStatus(int, char**)
{
    HANDLE h = INVALID_HANDLE_VALUE;
    _plugin_logprintf("[TiDaoji] driver=%s pid=%lu hidden=%d type=0x%X auto=%d\n",
                      g_driverName, (unsigned long)g_pid, g_hidden, GetTypeMask(), AutoHideOn());
    if(OpenDev(&h))
    {
        _plugin_logputs("[TiDaoji] device OPEN ok");
        CloseHandle(h);
    }
    return true;
}

static bool cbOptions(int argc, char** argv)
{
    if(argc < 2)
    {
        _plugin_logprintf("[TiDaoji] Type=0x%08X\n", GetTypeMask());
        return true;
    }
    BridgeSettingSetUint("TiDaoji", "Options", DbgValFromString(argv[1]) & 0xffffffff);
    return true;
}

static bool cbName(int argc, char** argv)
{
    if(argc < 2)
    {
        _plugin_logprintf("[TiDaoji] name=%s\n", g_driverName);
        return true;
    }
    strncpy_s(g_driverName, argv[1], _TRUNCATE);
    BridgeSettingSet("TiDaoji", "DriverName", g_driverName);
    return true;
}

static bool cbAuto(int argc, char** argv)
{
    if(argc < 2)
    {
        _plugin_logprintf("[TiDaoji] AutoHide=%d\n", AutoHideOn());
        return true;
    }
    BridgeSettingSetUint("TiDaoji", "AutoHide", DbgValFromString(argv[1]) ? 1 : 0);
    return true;
}

static bool cbHelp(int, char**)
{
    _plugin_logputs("[TiDaoji] Panel|Hide|Unhide|Status|AutoHide 0|1|SoftUnload");
    _plugin_logputs("[TiDaoji] AutoHide default 0. Need TiDaoji.sys running.");
    return true;
}

// --- panel ---
struct Chk { int id; ULONG bit; };
static const Chk kChk[] = {
    { IDC_CHK_PROCESSDEBUGFLAGS, HideProcessDebugFlags },
    { IDC_CHK_PROCESSDEBUGPORT, HideProcessDebugPort },
    { IDC_CHK_PROCESSDEBUGOBJECTHANDLE, HideProcessDebugObjectHandle },
    { IDC_CHK_DEBUGOBJECT, HideDebugObject },
    { IDC_CHK_SYSTEMDEBUGGERINFO, HideSystemDebuggerInformation },
    { IDC_CHK_NTCLOSE, HideNtClose },
    { IDC_CHK_THREADHIDEFROMDBG, HideThreadHideFromDebugger },
    { IDC_CHK_NTGETCONTEXTTHREAD, HideNtGetContextThread },
    { IDC_CHK_NTSETCONTEXTTHREAD, HideNtSetContextThread },
    { IDC_CHK_NTSYSTEMDEBUGCONTROL, HideNtSystemDebugControl },
    { IDC_CHK_NTSYSTEMVMINFO, HideNtSystemVMInformation },
    { IDC_CHK_NTTERMINATEPROCESS, HideNtTerminateProcess },
};

static void Refresh(HWND d)
{
    char name[64] = {};
    GetDlgItemTextA(d, IDC_EDT_DRIVER, name, 64);
    if(!name[0])
        strcpy_s(name, "TiDaoji");
    char path[96];
    _snprintf_s(path, _TRUNCATE, "\\\\.\\%s", name);
    HANDLE h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    char st[200];
    if(h == INVALID_HANDLE_VALUE)
        _snprintf_s(st, _TRUNCATE, "Status: OPEN FAIL %s err=%lu", path, GetLastError());
    else
    {
        CloseHandle(h);
        _snprintf_s(st, _TRUNCATE, "Status: OPEN OK %s", path);
    }
    SetDlgItemTextA(d, IDC_LBL_STATUS, st);
    char pb[100];
    if(g_pid)
        _snprintf_s(pb, _TRUNCATE, "PID %lu hidden=%d", (unsigned long)g_pid, g_hidden);
    else
        strcpy_s(pb, "PID (none)");
    SetDlgItemTextA(d, IDC_LBL_PID, pb);
}

static INT_PTR CALLBACK PanelProc(HWND d, UINT m, WPARAM w, LPARAM)
{
    if(m == WM_INITDIALOG)
    {
        ULONG t = GetTypeMask();
        for(size_t i = 0; i < sizeof(kChk) / sizeof(kChk[0]); i++)
            if(t & kChk[i].bit)
                CheckDlgButton(d, kChk[i].id, BST_CHECKED);
        SetDlgItemTextA(d, IDC_EDT_DRIVER, g_driverName);
        Refresh(d);
        return TRUE;
    }
    if(m == WM_COMMAND)
    {
        switch(LOWORD(w))
        {
        case IDC_BTN_SELALL:
            for(size_t i = 0; i < sizeof(kChk) / sizeof(kChk[0]); i++)
                CheckDlgButton(d, kChk[i].id, BST_CHECKED);
            return TRUE;
        case IDC_BTN_SELNONE:
            for(size_t i = 0; i < sizeof(kChk) / sizeof(kChk[0]); i++)
                CheckDlgButton(d, kChk[i].id, BST_UNCHECKED);
            return TRUE;
        case IDC_BTN_REFRESH:
            Refresh(d);
            return TRUE;
        case IDC_BTN_APPLY:
        {
            char name[64] = {};
            GetDlgItemTextA(d, IDC_EDT_DRIVER, name, 64);
            if(name[0])
            {
                strcpy_s(g_driverName, name);
                BridgeSettingSet("TiDaoji", "DriverName", g_driverName);
            }
            ULONG t = 0;
            for(size_t i = 0; i < sizeof(kChk) / sizeof(kChk[0]); i++)
                if(IsDlgButtonChecked(d, kChk[i].id) == BST_CHECKED)
                    t |= kChk[i].bit;
            BridgeSettingSetUint("TiDaoji", "Options", t);
            Refresh(d);
            return TRUE;
        }
        case IDC_BTN_HIDE:
            if(!g_pid)
                MessageBoxA(d, "No debuggee", "TiDaoji", MB_OK);
            else
            {
                char name[64] = {};
                GetDlgItemTextA(d, IDC_EDT_DRIVER, name, 64);
                if(name[0])
                    strcpy_s(g_driverName, name);
                ULONG t = 0;
                for(size_t i = 0; i < sizeof(kChk) / sizeof(kChk[0]); i++)
                    if(IsDlgButtonChecked(d, kChk[i].id) == BST_CHECKED)
                        t |= kChk[i].bit;
                BridgeSettingSetUint("TiDaoji", "Options", t);
                if(DevWrite(HidePid, g_pid, t))
                {
                    g_hidden = 1;
                    DbgCmdExecDirect("hide");
                }
            }
            Refresh(d);
            return TRUE;
        case IDC_BTN_UNHIDE:
            if(g_pid && DevWrite(UnhidePid, g_pid, GetTypeMask()))
                g_hidden = 0;
            Refresh(d);
            return TRUE;
        case IDC_BTN_UNHIDEALL:
            if(DevWrite(UnhideAll, 0, 0))
                g_hidden = 0;
            Refresh(d);
            return TRUE;
        case IDC_BTN_SOFTUNLOAD:
            if(MessageBoxA(d, "SoftUnload?", "TiDaoji", MB_YESNO | MB_ICONWARNING) == IDYES)
                DevWrite(SoftUnload, 0, 0);
            Refresh(d);
            return TRUE;
        case IDC_BTN_CLOSE:
        case IDCANCEL:
            EndDialog(d, 0);
            return TRUE;
        }
    }
    return FALSE;
}

void TiDaojiShowPanel()
{
    if(!g_hInst)
        return;
    HWND p = hwndDlg ? hwndDlg : GuiGetWindowHandle();
    DialogBoxParamA(g_hInst, MAKEINTRESOURCEA(IDD_PANEL), p, PanelProc, 0);
}

void TiDaojiHideKernelOnly()
{
    if(g_pid && DevWrite(HidePid, g_pid, GetTypeMask()))
        g_hidden = 1;
}

void TiDaojiUnhideKernelOnly()
{
    if(g_pid && DevWrite(UnhidePid, g_pid, GetTypeMask()))
        g_hidden = 0;
}

static bool cbPanel(int, char**)
{
    TiDaojiShowPanel();
    return true;
}

// Registered via _plugin_registercallback — NOT as PE exports
static void cbCreateProcess(CBTYPE, void* callbackInfo)
{
    auto* info = (PLUG_CB_CREATEPROCESS*)callbackInfo;
    if(info && info->fdProcessInfo)
        NotePid(info->fdProcessInfo->dwProcessId);
}

static void cbAttach(CBTYPE, void* callbackInfo)
{
    auto* info = (PLUG_CB_ATTACH*)callbackInfo;
    if(info)
        NotePid(info->dwProcessId);
}

static void cbSystemBp(CBTYPE, void*)
{
    if(AutoHideOn())
        TiDaojiHideKernelOnly();
}

static void cbStopDebug(CBTYPE, void*)
{
    if(AutoHideOn() && g_pid)
        TiDaojiUnhideKernelOnly();
    g_pid = 0;
    g_hidden = 0;
}

void TiDaojiInit(PLUG_INITSTRUCT*)
{
    // Intentionally empty of Bridge calls — plugsetup does real init.
}

void TiDaojiSetup()
{
    char setting[MAX_SETTING_SIZE] = "";
    if(BridgeSettingGet("TiDaoji", "DriverName", setting) && setting[0])
        strncpy_s(g_driverName, setting, _TRUNCATE);

    duint options = 0;
    if(!BridgeSettingGetUint("TiDaoji", "Options", &options))
        BridgeSettingSetUint("TiDaoji", "Options", kDefaultType);

    duint ah = 0;
    if(!BridgeSettingGetUint("TiDaoji", "AutoHide", &ah))
        BridgeSettingSetUint("TiDaoji", "AutoHide", 0);

    _plugin_registercommand(pluginHandle, "TiDaoji", cbHide, true);
    _plugin_registercommand(pluginHandle, "TiDaojiUnhide", cbUnhide, true);
    _plugin_registercommand(pluginHandle, "TiDaojiUnhideAll", cbUnhideAll, false);
    _plugin_registercommand(pluginHandle, "TiDaojiSoftUnload", cbSoft, false);
    _plugin_registercommand(pluginHandle, "TiDaojiOptions", cbOptions, false);
    _plugin_registercommand(pluginHandle, "TiDaojiName", cbName, false);
    _plugin_registercommand(pluginHandle, "TiDaojiStatus", cbStatus, false);
    _plugin_registercommand(pluginHandle, "TiDaojiAutoHide", cbAuto, false);
    _plugin_registercommand(pluginHandle, "TiDaojiHelp", cbHelp, false);
    _plugin_registercommand(pluginHandle, "TiDaojiPanel", cbPanel, false);

    _plugin_registercallback(pluginHandle, CB_CREATEPROCESS, cbCreateProcess);
    _plugin_registercallback(pluginHandle, CB_ATTACH, cbAttach);
    _plugin_registercallback(pluginHandle, CB_SYSTEMBREAKPOINT, cbSystemBp);
    _plugin_registercallback(pluginHandle, CB_STOPDEBUG, cbStopDebug);

    _plugin_logprintf("[TiDaoji] v%d ready. AutoHide=%d \\\\.\\%s\n",
                      PLUGIN_VERSION, AutoHideOn(), g_driverName);
}

void TiDaojiStop()
{
    _plugin_unregistercallback(pluginHandle, CB_CREATEPROCESS);
    _plugin_unregistercallback(pluginHandle, CB_ATTACH);
    _plugin_unregistercallback(pluginHandle, CB_SYSTEMBREAKPOINT);
    _plugin_unregistercallback(pluginHandle, CB_STOPDEBUG);

    _plugin_unregistercommand(pluginHandle, "TiDaojiPanel");
    _plugin_unregistercommand(pluginHandle, "TiDaojiHelp");
    _plugin_unregistercommand(pluginHandle, "TiDaojiAutoHide");
    _plugin_unregistercommand(pluginHandle, "TiDaojiStatus");
    _plugin_unregistercommand(pluginHandle, "TiDaojiOptions");
    _plugin_unregistercommand(pluginHandle, "TiDaojiName");
    _plugin_unregistercommand(pluginHandle, "TiDaojiSoftUnload");
    _plugin_unregistercommand(pluginHandle, "TiDaojiUnhideAll");
    _plugin_unregistercommand(pluginHandle, "TiDaojiUnhide");
    _plugin_unregistercommand(pluginHandle, "TiDaoji");
}
