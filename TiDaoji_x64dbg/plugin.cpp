// TiDaoji x64dbg plugin - C-style, no std::string, /MD CRT (match host plugins).
// Auto-hide on SYSTEMBP is OFF by default (set TiDaoji/AutoHide=1 to enable).
#include "plugin.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "../TiDaoji/TiDaoji.h"
#include "resource.h"

static DWORD g_pid = 0;
static int g_hidden = 0;
static char g_driverName[64] = "TiDaoji";
static constexpr ULONG kDefaultHideType = 0xFFFu;

static ULONG GetTypeMask()
{
    duint options = 0;
    if(!BridgeSettingGetUint("TiDaoji", "Options", &options))
        options = kDefaultHideType;
    return (ULONG)options;
}

static int AutoHideEnabled()
{
    duint v = 0;
    if(!BridgeSettingGetUint("TiDaoji", "AutoHide", &v))
        return 0; // default OFF - safer
    return v ? 1 : 0;
}

static bool OpenDevice(HANDLE* out)
{
    char path[96];
    _snprintf_s(path, _TRUNCATE, "\\\\.\\%s", g_driverName);
    HANDLE h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
    if(h == INVALID_HANDLE_VALUE)
    {
        DWORD err = GetLastError();
        _plugin_logprintf("[TiDaoji] open %s failed Win32=%lu (sc start TiDaoji / L1 KDU)\n", path, err);
        *out = INVALID_HANDLE_VALUE;
        return false;
    }
    *out = h;
    return true;
}

static bool DeviceWrite(HIDE_COMMAND cmd, DWORD pid, ULONG type)
{
    HANDLE h = INVALID_HANDLE_VALUE;
    if(!OpenDevice(&h))
        return false;
    HIDE_INFO hi;
    hi.Command = cmd;
    hi.Pid = pid;
    hi.Type = type;
    DWORD written = 0;
    BOOL ok = WriteFile(h, &hi, sizeof(hi), &written, 0);
    if(!ok)
        _plugin_logprintf("[TiDaoji] WriteFile failed Win32=%lu\n", GetLastError());
    else
        _plugin_logprintf("[TiDaoji] cmd=%d pid=%lu type=0x%08X written=%lu\n",
                          (int)cmd, (unsigned long)pid, type, written);
    CloseHandle(h);
    return ok ? true : false;
}

static void NotePid(DWORD p)
{
    if(p != g_pid)
    {
        g_hidden = 0;
        g_pid = p;
    }
}

static bool cbHide(int argc, char* argv[])
{
    (void)argc;
    (void)argv;
    if(!g_pid)
    {
        _plugin_logputs("[TiDaoji] no debuggee PID");
        return false;
    }
    if(DeviceWrite(HidePid, g_pid, GetTypeMask()))
    {
        g_hidden = 1;
        // PEB hide only from command/menu path, not SYSTEMBP
        DbgCmdExecDirect("hide");
        return true;
    }
    return false;
}

static bool cbUnhide(int argc, char* argv[])
{
    (void)argc;
    (void)argv;
    if(!g_pid)
        return false;
    if(DeviceWrite(UnhidePid, g_pid, GetTypeMask()))
    {
        g_hidden = 0;
        return true;
    }
    return false;
}

static bool cbUnhideAll(int argc, char* argv[])
{
    (void)argc;
    (void)argv;
    if(DeviceWrite(UnhideAll, 0, 0))
    {
        g_hidden = 0;
        return true;
    }
    return false;
}

static bool cbSoftUnload(int argc, char* argv[])
{
    (void)argc;
    (void)argv;
    return DeviceWrite(SoftUnload, 0, 0);
}

static bool cbStatus(int argc, char* argv[])
{
    (void)argc;
    (void)argv;
    HANDLE h = INVALID_HANDLE_VALUE;
    _plugin_logprintf("[TiDaoji] driver=%s pid=%lu hidden=%d type=0x%08X autoHide=%d\n",
                      g_driverName, (unsigned long)g_pid, g_hidden, GetTypeMask(), AutoHideEnabled());
    if(OpenDevice(&h))
    {
        _plugin_logputs("[TiDaoji] device OPEN ok");
        CloseHandle(h);
    }
    return true;
}

static bool cbOptions(int argc, char* argv[])
{
    if(argc < 2)
    {
        _plugin_logprintf("[TiDaoji] Type=0x%08X\n", GetTypeMask());
        return true;
    }
    duint options = DbgValFromString(argv[1]);
    BridgeSettingSetUint("TiDaoji", "Options", options & 0xffffffff);
    _plugin_logprintf("[TiDaoji] Type set 0x%08X\n", GetTypeMask());
    return true;
}

static bool cbName(int argc, char* argv[])
{
    if(argc < 2)
    {
        _plugin_logprintf("[TiDaoji] DriverName=%s\n", g_driverName);
        return true;
    }
    strncpy_s(g_driverName, argv[1], _TRUNCATE);
    BridgeSettingSet("TiDaoji", "DriverName", g_driverName);
    _plugin_logprintf("[TiDaoji] DriverName set %s\n", g_driverName);
    return true;
}

static bool cbAutoHide(int argc, char* argv[])
{
    if(argc < 2)
    {
        _plugin_logprintf("[TiDaoji] AutoHide=%d (1=on SYSTEMBP)\n", AutoHideEnabled());
        return true;
    }
    duint v = DbgValFromString(argv[1]) ? 1 : 0;
    BridgeSettingSetUint("TiDaoji", "AutoHide", v);
    _plugin_logprintf("[TiDaoji] AutoHide=%llu\n", (unsigned long long)v);
    return true;
}

static bool cbHelp(int argc, char* argv[])
{
    (void)argc;
    (void)argv;
    _plugin_logputs("[TiDaoji] TiDaojiPanel | TiDaoji | TiDaojiUnhide | TiDaojiStatus");
    _plugin_logputs("[TiDaoji] TiDaojiAutoHide 0|1  (default 0; SYSTEMBP auto hide)");
    _plugin_logputs("[TiDaoji] Need TiDaoji.sys RUNNING. NOT PG-safe.");
    return true;
}

// --- Control panel ---
struct CheckEntry { int id; ULONG bit; };
static const CheckEntry kChecks[] = {
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

static void RefreshPanel(HWND hDlg)
{
    char path[96];
    char name[64];
    GetDlgItemTextA(hDlg, IDC_EDT_DRIVER, name, sizeof(name));
    if(!name[0])
        strncpy_s(name, "TiDaoji", _TRUNCATE);
    _snprintf_s(path, _TRUNCATE, "\\\\.\\%s", name);
    HANDLE h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
    char status[256];
    if(h == INVALID_HANDLE_VALUE)
        _snprintf_s(status, _TRUNCATE, "Status: OPEN FAIL %s Win32=%lu", path, GetLastError());
    else
    {
        CloseHandle(h);
        _snprintf_s(status, _TRUNCATE, "Status: OPEN OK %s", path);
    }
    SetDlgItemTextA(hDlg, IDC_LBL_STATUS, status);

    char pbuf[128];
    if(g_pid)
        _snprintf_s(pbuf, _TRUNCATE, "PID: %lu (0x%lX) hidden=%d",
                    (unsigned long)g_pid, (unsigned long)g_pid, g_hidden);
    else
        _snprintf_s(pbuf, _TRUNCATE, "PID: (no debuggee)");
    SetDlgItemTextA(hDlg, IDC_LBL_PID, pbuf);
}

static INT_PTR CALLBACK PanelProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM)
{
    switch(msg)
    {
    case WM_INITDIALOG:
    {
        ULONG opts = GetTypeMask();
        for(size_t i = 0; i < sizeof(kChecks) / sizeof(kChecks[0]); i++)
        {
            if(opts & kChecks[i].bit)
                CheckDlgButton(hDlg, kChecks[i].id, BST_CHECKED);
        }
        SetDlgItemTextA(hDlg, IDC_EDT_DRIVER, g_driverName);
        RefreshPanel(hDlg);
        return TRUE;
    }
    case WM_COMMAND:
        switch(LOWORD(wParam))
        {
        case IDC_BTN_SELALL:
            for(size_t i = 0; i < sizeof(kChecks) / sizeof(kChecks[0]); i++)
                CheckDlgButton(hDlg, kChecks[i].id, BST_CHECKED);
            return TRUE;
        case IDC_BTN_SELNONE:
            for(size_t i = 0; i < sizeof(kChecks) / sizeof(kChecks[0]); i++)
                CheckDlgButton(hDlg, kChecks[i].id, BST_UNCHECKED);
            return TRUE;
        case IDC_BTN_REFRESH:
            RefreshPanel(hDlg);
            return TRUE;
        case IDC_BTN_APPLY:
        {
            char name[64];
            GetDlgItemTextA(hDlg, IDC_EDT_DRIVER, name, sizeof(name));
            if(name[0])
            {
                strncpy_s(g_driverName, name, _TRUNCATE);
                BridgeSettingSet("TiDaoji", "DriverName", g_driverName);
            }
            ULONG opts = 0;
            for(size_t i = 0; i < sizeof(kChecks) / sizeof(kChecks[0]); i++)
            {
                if(IsDlgButtonChecked(hDlg, kChecks[i].id) == BST_CHECKED)
                    opts |= kChecks[i].bit;
            }
            BridgeSettingSetUint("TiDaoji", "Options", opts);
            RefreshPanel(hDlg);
            return TRUE;
        }
        case IDC_BTN_HIDE:
            if(!g_pid)
                MessageBoxA(hDlg, "No debuggee.", "TiDaoji", MB_OK | MB_ICONWARNING);
            else
            {
                char name[64];
                GetDlgItemTextA(hDlg, IDC_EDT_DRIVER, name, sizeof(name));
                if(name[0])
                    strncpy_s(g_driverName, name, _TRUNCATE);
                ULONG opts = 0;
                for(size_t i = 0; i < sizeof(kChecks) / sizeof(kChecks[0]); i++)
                {
                    if(IsDlgButtonChecked(hDlg, kChecks[i].id) == BST_CHECKED)
                        opts |= kChecks[i].bit;
                }
                BridgeSettingSetUint("TiDaoji", "Options", opts);
                if(DeviceWrite(HidePid, g_pid, opts))
                {
                    g_hidden = 1;
                    DbgCmdExecDirect("hide");
                }
            }
            RefreshPanel(hDlg);
            return TRUE;
        case IDC_BTN_UNHIDE:
            if(g_pid && DeviceWrite(UnhidePid, g_pid, GetTypeMask()))
                g_hidden = 0;
            RefreshPanel(hDlg);
            return TRUE;
        case IDC_BTN_UNHIDEALL:
            if(DeviceWrite(UnhideAll, 0, 0))
                g_hidden = 0;
            RefreshPanel(hDlg);
            return TRUE;
        case IDC_BTN_SOFTUNLOAD:
            if(MessageBoxA(hDlg, "SoftUnload InfinityHook device?", "TiDaoji",
                           MB_YESNO | MB_ICONWARNING) == IDYES)
            {
                DeviceWrite(SoftUnload, 0, 0);
                g_hidden = 0;
            }
            RefreshPanel(hDlg);
            return TRUE;
        case IDC_BTN_CLOSE:
        case IDCANCEL:
            EndDialog(hDlg, 0);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static void ShowPanelGui()
{
    if(!g_hInst)
        return;
    HWND parent = hwndDlg ? hwndDlg : GuiGetWindowHandle();
    DialogBoxParamA(g_hInst, MAKEINTRESOURCEA(IDD_PANEL), parent, PanelProc, 0);
}

void TiDaojiShowPanel()
{
    ShowPanelGui();
}

void TiDaojiHideKernelOnly()
{
    if(g_pid)
    {
        if(DeviceWrite(HidePid, g_pid, GetTypeMask()))
            g_hidden = 1;
    }
}

void TiDaojiUnhideKernelOnly()
{
    if(g_pid)
    {
        if(DeviceWrite(UnhidePid, g_pid, GetTypeMask()))
            g_hidden = 0;
    }
}

static bool cbPanel(int argc, char* argv[])
{
    (void)argc;
    (void)argv;
    TiDaojiShowPanel();
    return true;
}

PLUG_EXPORT void CBCREATEPROCESS(CBTYPE, PLUG_CB_CREATEPROCESS* info)
{
    if(info && info->fdProcessInfo)
        NotePid(info->fdProcessInfo->dwProcessId);
}

PLUG_EXPORT void CBATTACH(CBTYPE, PLUG_CB_ATTACH* info)
{
    if(info)
        NotePid(info->dwProcessId);
}

PLUG_EXPORT void CBSYSTEMBREAKPOINT(CBTYPE, PLUG_CB_SYSTEMBREAKPOINT*)
{
    // Default OFF. No DbgCmdExec here.
    if(AutoHideEnabled())
        TiDaojiHideKernelOnly();
}

PLUG_EXPORT void CBSTOPDEBUG(CBTYPE, PLUG_CB_STOPDEBUG*)
{
    if(AutoHideEnabled() && g_pid)
        TiDaojiUnhideKernelOnly();
    g_pid = 0;
    g_hidden = 0;
}

void TiDaojiInit(PLUG_INITSTRUCT*)
{
    char setting[MAX_SETTING_SIZE] = "";
    if(BridgeSettingGet("TiDaoji", "DriverName", setting) && setting[0])
        strncpy_s(g_driverName, setting, _TRUNCATE);

    duint options = 0;
    if(!BridgeSettingGetUint("TiDaoji", "Options", &options))
        BridgeSettingSetUint("TiDaoji", "Options", kDefaultHideType);

    duint ah = 0;
    if(!BridgeSettingGetUint("TiDaoji", "AutoHide", &ah))
        BridgeSettingSetUint("TiDaoji", "AutoHide", 0);

    _plugin_registercommand(pluginHandle, "TiDaoji", cbHide, true);
    _plugin_registercommand(pluginHandle, "TiDaojiUnhide", cbUnhide, true);
    _plugin_registercommand(pluginHandle, "TiDaojiUnhideAll", cbUnhideAll, false);
    _plugin_registercommand(pluginHandle, "TiDaojiSoftUnload", cbSoftUnload, false);
    _plugin_registercommand(pluginHandle, "TiDaojiOptions", cbOptions, false);
    _plugin_registercommand(pluginHandle, "TiDaojiName", cbName, false);
    _plugin_registercommand(pluginHandle, "TiDaojiStatus", cbStatus, false);
    _plugin_registercommand(pluginHandle, "TiDaojiAutoHide", cbAutoHide, false);
    _plugin_registercommand(pluginHandle, "TiDaojiHelp", cbHelp, false);
    _plugin_registercommand(pluginHandle, "TiDaojiPanel", cbPanel, false);

    _plugin_logprintf("[TiDaoji] v%d loaded (/MD). AutoHide=%d device \\\\.\\%s\n",
                      PLUGIN_VERSION, AutoHideEnabled(), g_driverName);
}

void TiDaojiStop()
{
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
