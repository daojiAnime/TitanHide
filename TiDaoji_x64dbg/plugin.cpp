#include "plugin.h"
#include <windows.h>
#include <stdio.h>
#include <string>
#include "../TiDaoji/TiDaoji.h"
#include "resource.h"

static DWORD pid = 0;
static bool hidden = false;
static std::string driverName = "TiDaoji";

// Default: all known hide bits (BIT1..BIT12) for VMP-oriented sessions.
static constexpr ULONG kDefaultHideType = 0xFFFu;

static ULONG GetTiDaojiOptions()
{
    duint options = 0;
    if(!BridgeSettingGetUint("TiDaoji", "Options", &options))
        options = kDefaultHideType;
    return (ULONG)options;
}

static void LogWin32Error(const char* what)
{
    const DWORD err = GetLastError();
    char* msg = nullptr;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, err, 0, (LPSTR)&msg, 0, nullptr);
    if(msg)
    {
        // trim trailing CR/LF
        size_t n = strlen(msg);
        while(n && (msg[n - 1] == '\r' || msg[n - 1] == '\n' || msg[n - 1] == ' '))
            msg[--n] = '\0';
        _plugin_logprintf("[" PLUGIN_NAME "] %s: Win32=%lu (%s)\n", what, err, msg);
        LocalFree(msg);
    }
    else
        _plugin_logprintf("[" PLUGIN_NAME "] %s: Win32=%lu\n", what, err);
}

static void DescribeType(ULONG type)
{
    _plugin_logprintf("[" PLUGIN_NAME "] Type=0x%08X bits:", type);
    struct { ULONG bit; const char* name; } table[] =
    {
        { HideProcessDebugFlags, "ProcessDebugFlags" },
        { HideProcessDebugPort, "ProcessDebugPort" },
        { HideProcessDebugObjectHandle, "ProcessDebugObjectHandle" },
        { HideDebugObject, "DebugObject" },
        { HideSystemDebuggerInformation, "SystemDebuggerInformation" },
        { HideNtClose, "NtClose" },
        { HideThreadHideFromDebugger, "ThreadHideFromDebugger" },
        { HideNtGetContextThread, "NtGetContextThread" },
        { HideNtSetContextThread, "NtSetContextThread" },
        { HideNtSystemDebugControl, "NtSystemDebugControl" },
        { HideNtSystemVMInformation, "NtSystemVMInformation" },
        { HideNtTerminateProcess, "NtTerminateProcess" },
    };
    bool any = false;
    for(const auto& e : table)
    {
        if(type & e.bit)
        {
            _plugin_logprintf(" %s", e.name);
            any = true;
        }
    }
    if(!any)
        _plugin_logputs(" (none)");
    else
        _plugin_logputs("");
}

static void LogOpenFail(const char* path, DWORD err)
{
    // Actionable hints (log panel is primary feedback in x64dbg)
    const char* hint = "start TiDaoji.sys first";
    if(err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND)
        hint = "device missing: KDU -dse 0 -> sc start TiDaoji -> -dse 6";
    else if(err == ERROR_ACCESS_DENIED)
        hint = "access denied: run x64dbg as Administrator";
    else if(err == ERROR_TOO_MANY_POSTS) // 298 — often seen when service stopped / bad link
        hint = "Win32=298: driver STOPPED or stale; sc query TiDaoji, then L1 restart";
    _plugin_logprintf("[" PLUGIN_NAME "] open %s FAILED Win32=%lu - %s\n", path, err, hint);
    _plugin_logputs("[" PLUGIN_NAME "] Menu: Plugins -> TiDaoji -> Control Panel");
}

static bool OpenDevice(HANDLE* out)
{
    *out = INVALID_HANDLE_VALUE;
    auto path = "\\\\.\\" + driverName;
    HANDLE h = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
    if(h == INVALID_HANDLE_VALUE)
    {
        LogOpenFail(path.c_str(), GetLastError());
        return false;
    }
    *out = h;
    return true;
}

static bool TiDaojiCall(HIDE_COMMAND Command)
{
    HANDLE hDevice = INVALID_HANDLE_VALUE;
    if(!OpenDevice(&hDevice))
        return false;

    HIDE_INFO HideInfo;
    HideInfo.Command = Command;
    HideInfo.Pid = pid;
    HideInfo.Type = GetTiDaojiOptions();
    DWORD written = 0;
    bool result = false;
    if(WriteFile(hDevice, &HideInfo, sizeof(HIDE_INFO), &written, 0))
    {
        if(Command == UnhideAll)
            _plugin_logputs("[" PLUGIN_NAME "] UnhideAll written");
        else
            _plugin_logprintf("[" PLUGIN_NAME "] PID %u (0x%X) %shidden (Type=0x%08X)\n",
                              pid, pid, Command == UnhidePid ? "un" : "", HideInfo.Type);
        result = true;
    }
    else
    {
        _plugin_logputs("[" PLUGIN_NAME "] WriteFile failed");
        LogWin32Error("WriteFile");
    }
    CloseHandle(hDevice);
    return result;
}

static void NotePid(DWORD newPid)
{
    if(newPid != pid)
    {
        if(hidden && pid != 0)
            _plugin_logprintf("[" PLUGIN_NAME "] debuggee PID %u -> %u (local hide flag cleared; re-run TiDaoji)\n",
                              pid, newPid);
        hidden = false;
        pid = newPid;
    }
}

static bool cbTiDaojiPanel(int argc, char* argv[])
{
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);
    TiDaojiShowPanel();
    return true;
}

static bool cbTiDaojiHelp(int argc, char* argv[])
{
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);
    _plugin_logputs("[" PLUGIN_NAME "] commands:");
    _plugin_logputs("  TiDaojiPanel         - Control Panel UI (status + Hide + Type)");
    _plugin_logputs("  TiDaoji              - HidePid for current debuggee (re-apply OK)");
    _plugin_logputs("  TiDaojiUnhide        - UnhidePid for current debuggee");
    _plugin_logputs("  TiDaojiUnhideAll     - UnhideAll (driver table clear)");
    _plugin_logputs("  TiDaojiSoftUnload    - L2/L3: SoftUnload (no sc stop; SCM may still list service)");
    _plugin_logputs("  TiDaojiOptions [n]   - get/set Type bitmask (BridgeSetting TiDaoji/Options)");
    _plugin_logputs("  TiDaojiName [svc]    - get/set device/service name (default TiDaoji)");
    _plugin_logputs("  TiDaojiStatus        - driver open probe + session state");
    _plugin_logputs("  TiDaojiHelp          - this text");
    _plugin_logputs(" Menu: Plugins -> TiDaoji -> Control Panel...");
    _plugin_logputs(" Auto: SYSTEMBP -> TiDaoji; stop debug -> TiDaojiUnhide");
    _plugin_logputs(" Requires kernel TiDaoji.sys RUNNING. NOT PG-safe. No dual-IH with CR.");
    return true;
}

static bool cbTiDaojiStatus(int argc, char* argv[])
{
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);
    _plugin_logprintf("[" PLUGIN_NAME "] driverName=%s pid=%u (0x%X) hidden_flag=%d\n",
                      driverName.c_str(), pid, pid, hidden ? 1 : 0);
    DescribeType(GetTiDaojiOptions());
    HANDLE h = INVALID_HANDLE_VALUE;
    if(OpenDevice(&h))
    {
        _plugin_logprintf("[" PLUGIN_NAME "] device \\\\.\\%s OPEN ok\n", driverName.c_str());
        CloseHandle(h);
    }
    return true;
}

static bool cbTiDaoji(int argc, char* argv[])
{
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);
    if(pid == 0)
    {
        _plugin_logputs("[" PLUGIN_NAME "] no debuggee PID yet (attach/create first)");
        return false;
    }
    _plugin_logprintf("[" PLUGIN_NAME "] HidePid PID %u (0x%X)\n", pid, pid);
    if(TiDaojiCall(HidePid))
    {
        // Safe PEB hide from command path (user typed TiDaoji / menu) — not from SYSTEMBP
        DbgCmdExecDirect("hide");
        hidden = true;
        return true;
    }
    return false;
}

static bool cbTiDaojiUnhide(int argc, char* argv[])
{
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);
    if(pid == 0)
    {
        _plugin_logputs("[" PLUGIN_NAME "] no debuggee PID");
        return false;
    }
    // Always attempt UnhidePid (driver may still track PID after restart of plugin state)
    _plugin_logprintf("[" PLUGIN_NAME "] UnhidePid PID %u (0x%X)\n", pid, pid);
    if(TiDaojiCall(UnhidePid))
    {
        hidden = false;
        return true;
    }
    return false;
}

static bool cbTiDaojiUnhideAll(int argc, char* argv[])
{
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);
    if(TiDaojiCall(UnhideAll))
    {
        hidden = false;
        return true;
    }
    return false;
}

static bool cbTiDaojiSoftUnload(int argc, char* argv[])
{
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);
    _plugin_logputs("[" PLUGIN_NAME "] SoftUnload (L2/L3 teardown)");
    if(TiDaojiCall(SoftUnload))
    {
        hidden = false;
        return true;
    }
    return false;
}

static bool cbTiDaojiOptions(int argc, char* argv[])
{
    if(argc < 2)
    {
        DescribeType(GetTiDaojiOptions());
    }
    else
    {
        duint options = DbgValFromString(argv[1]);
        BridgeSettingSetUint("TiDaoji", "Options", options & 0xffffffff);
        DescribeType(GetTiDaojiOptions());
        if(hidden && pid != 0)
            TiDaojiCall(HidePid);
    }
    return true;
}

static bool cbTiDaojiName(int argc, char* argv[])
{
    if(argc < 2)
    {
        _plugin_logprintf("[" PLUGIN_NAME "] DriverName='%s' -> \\\\.\\%s\n",
                          driverName.c_str(), driverName.c_str());
    }
    else
    {
        driverName = argv[1];
        BridgeSettingSet("TiDaoji", "DriverName", driverName.c_str());
        _plugin_logprintf("[" PLUGIN_NAME "] DriverName set to '%s'\n", driverName.c_str());
    }
    return true;
}

// Kernel hide without DbgCmdExecDirect("hide") — re-entrancy from SYSTEMBP
// callbacks has historically crashed x64dbg (command queue re-enter).
void TiDaojiHideKernelOnly()
{
    if(pid == 0)
        return;
    if(TiDaojiCall(HidePid))
        hidden = true;
}

void TiDaojiUnhideKernelOnly()
{
    if(pid == 0)
        return;
    if(TiDaojiCall(UnhidePid))
        hidden = false;
}

PLUG_EXPORT void CBCREATEPROCESS(CBTYPE cbType, PLUG_CB_CREATEPROCESS* info)
{
    UNREFERENCED_PARAMETER(cbType);
    __try
    {
        if(info && info->fdProcessInfo)
            NotePid(info->fdProcessInfo->dwProcessId);
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        _plugin_logputs("[" PLUGIN_NAME "] CBCREATEPROCESS exception");
    }
}

PLUG_EXPORT void CBATTACH(CBTYPE cbType, PLUG_CB_ATTACH* info)
{
    UNREFERENCED_PARAMETER(cbType);
    __try
    {
        if(info)
            NotePid(info->dwProcessId);
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        _plugin_logputs("[" PLUGIN_NAME "] CBATTACH exception");
    }
}

PLUG_EXPORT void CBSYSTEMBREAKPOINT(CBTYPE cbType, PLUG_CB_SYSTEMBREAKPOINT* info)
{
    UNREFERENCED_PARAMETER(cbType);
    UNREFERENCED_PARAMETER(info);
    // Do NOT call DbgCmdExec here — only kernel WriteFile path.
    __try
    {
        TiDaojiHideKernelOnly();
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        _plugin_logputs("[" PLUGIN_NAME "] CBSYSTEMBREAKPOINT exception");
    }
}

PLUG_EXPORT void CBSTOPDEBUG(CBTYPE cbType, PLUG_CB_STOPDEBUG* info)
{
    UNREFERENCED_PARAMETER(cbType);
    UNREFERENCED_PARAMETER(info);
    __try
    {
        TiDaojiUnhideKernelOnly();
        pid = 0;
        hidden = false;
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        _plugin_logputs("[" PLUGIN_NAME "] CBSTOPDEBUG exception");
    }
}

// --- Control Panel (Win32 dialog; x64dbg pattern: DialogBox + GuiGetWindowHandle) ---

extern HINSTANCE g_hInst;

struct CheckEntry { int id; ULONG bit; };
static const CheckEntry kCheckMap[] = {
    { IDC_CHK_PROCESSDEBUGFLAGS,        HideProcessDebugFlags },
    { IDC_CHK_PROCESSDEBUGPORT,         HideProcessDebugPort },
    { IDC_CHK_PROCESSDEBUGOBJECTHANDLE, HideProcessDebugObjectHandle },
    { IDC_CHK_DEBUGOBJECT,              HideDebugObject },
    { IDC_CHK_SYSTEMDEBUGGERINFO,       HideSystemDebuggerInformation },
    { IDC_CHK_NTCLOSE,                  HideNtClose },
    { IDC_CHK_THREADHIDEFROMDBG,        HideThreadHideFromDebugger },
    { IDC_CHK_NTGETCONTEXTTHREAD,       HideNtGetContextThread },
    { IDC_CHK_NTSETCONTEXTTHREAD,       HideNtSetContextThread },
    { IDC_CHK_NTSYSTEMDEBUGCONTROL,     HideNtSystemDebugControl },
    { IDC_CHK_NTSYSTEMVMINFO,           HideNtSystemVMInformation },
    { IDC_CHK_NTTERMINATEPROCESS,       HideNtTerminateProcess },
};

static void SetAllChecks(HWND hDlg, BOOL state)
{
    for(const auto& e : kCheckMap)
        CheckDlgButton(hDlg, e.id, state ? BST_CHECKED : BST_UNCHECKED);
}

static void SyncDriverNameFromUi(HWND hDlg)
{
    char name[128] = "";
    GetDlgItemTextA(hDlg, IDC_EDT_DRIVER, name, sizeof(name));
    if(name[0])
    {
        driverName = name;
        BridgeSettingSet("TiDaoji", "DriverName", driverName.c_str());
    }
}

static ULONG CollectTypeFromUi(HWND hDlg)
{
    ULONG opts = 0;
    for(const auto& e : kCheckMap)
    {
        if(IsDlgButtonChecked(hDlg, e.id) == BST_CHECKED)
            opts |= e.bit;
    }
    return opts;
}

static void RefreshPanel(HWND hDlg)
{
    char name[128] = "";
    GetDlgItemTextA(hDlg, IDC_EDT_DRIVER, name, sizeof(name));
    if(name[0] == '\0')
        strncpy_s(name, "TiDaoji", _TRUNCATE);

    char path[160];
    snprintf(path, sizeof path, "\\\\.\\%s", name);
    HANDLE h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
    char status[280];
    if(h == INVALID_HANDLE_VALUE)
    {
        const DWORD err = GetLastError();
        snprintf(status, sizeof status,
                 "Status: OPEN FAIL  %s  Win32=%lu  (sc query TiDaoji; L1 KDU then sc start)",
                 path, err);
    }
    else
    {
        CloseHandle(h);
        snprintf(status, sizeof status, "Status: OPEN OK  %s  Type will apply on Hide", path);
    }
    SetDlgItemTextA(hDlg, IDC_LBL_STATUS, status);

    char pbuf[160];
    if(pid)
        snprintf(pbuf, sizeof pbuf, "PID: %u (0x%X)  %s", pid, pid, hidden ? "[session: hidden]" : "[session: not hidden]");
    else
        snprintf(pbuf, sizeof pbuf, "PID: (no debuggee — attach/open first)");
    SetDlgItemTextA(hDlg, IDC_LBL_PID, pbuf);
}

static INT_PTR CALLBACK PanelDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM)
{
    switch(msg)
    {
    case WM_INITDIALOG:
    {
        ULONG opts = GetTiDaojiOptions();
        for(const auto& e : kCheckMap)
        {
            if(opts & e.bit)
                CheckDlgButton(hDlg, e.id, BST_CHECKED);
        }
        SetDlgItemTextA(hDlg, IDC_EDT_DRIVER, driverName.c_str());
        RefreshPanel(hDlg);
        return TRUE;
    }
    case WM_COMMAND:
        switch(LOWORD(wParam))
        {
        case IDC_BTN_SELALL:
            SetAllChecks(hDlg, TRUE);
            return TRUE;
        case IDC_BTN_SELNONE:
            SetAllChecks(hDlg, FALSE);
            return TRUE;
        case IDC_BTN_REFRESH:
            SyncDriverNameFromUi(hDlg);
            RefreshPanel(hDlg);
            return TRUE;
        case IDC_BTN_APPLY:
        {
            SyncDriverNameFromUi(hDlg);
            const ULONG opts = CollectTypeFromUi(hDlg);
            BridgeSettingSetUint("TiDaoji", "Options", opts);
            _plugin_logprintf("[" PLUGIN_NAME "] Type=0x%08X driver=%s\n", opts, driverName.c_str());
            DescribeType(opts);
            RefreshPanel(hDlg);
            return TRUE;
        }
        case IDC_BTN_HIDE:
            SyncDriverNameFromUi(hDlg);
            BridgeSettingSetUint("TiDaoji", "Options", CollectTypeFromUi(hDlg));
            if(pid == 0)
                MessageBoxA(hDlg, "No debuggee PID. Open/attach a process first.", PLUGIN_NAME, MB_ICONWARNING);
            else if(TiDaojiCall(HidePid))
            {
                // PEB hide via GUI thread (avoid nested cmd during modal dialog)
                GuiExecuteOnGuiThread(+[]() { DbgCmdExecDirect("hide"); });
                hidden = true;
            }
            RefreshPanel(hDlg);
            return TRUE;
        case IDC_BTN_UNHIDE:
            SyncDriverNameFromUi(hDlg);
            if(pid)
            {
                TiDaojiCall(UnhidePid);
                hidden = false;
            }
            RefreshPanel(hDlg);
            return TRUE;
        case IDC_BTN_UNHIDEALL:
            SyncDriverNameFromUi(hDlg);
            TiDaojiCall(UnhideAll);
            hidden = false;
            RefreshPanel(hDlg);
            return TRUE;
        case IDC_BTN_SOFTUNLOAD:
            if(MessageBoxA(hDlg,
                           "SoftUnload tears down InfinityHook / device (L2/L3).\nContinue?",
                           PLUGIN_NAME, MB_ICONWARNING | MB_YESNO) == IDYES)
            {
                SyncDriverNameFromUi(hDlg);
                TiDaojiCall(SoftUnload);
                hidden = false;
                RefreshPanel(hDlg);
            }
            return TRUE;
        case IDC_BTN_CLOSE:
        case IDCANCEL:
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static void ShowPanelOnGui()
{
    if(!g_hInst)
    {
        GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&ShowPanelOnGui),
            &g_hInst);
    }
    if(!g_hInst)
    {
        _plugin_logputs("[" PLUGIN_NAME "] UI: HINSTANCE null — rebuild plugin with DllMain");
        return;
    }
    HWND parent = hwndDlg ? hwndDlg : GuiGetWindowHandle();
    INT_PTR r = DialogBoxParamA(g_hInst, MAKEINTRESOURCEA(IDD_PANEL), parent, PanelDlgProc, 0);
    if(r == -1)
    {
        _plugin_logprintf("[" PLUGIN_NAME "] DialogBox failed Win32=%lu (resource missing?)\n",
                          GetLastError());
    }
}

void TiDaojiShowPanel()
{
    // Ensure dialog runs on GUI thread (x64dbg GuiExecuteOnGuiThread)
    // Menu already on GUI thread usually; GuiExecute is still safe.
    HWND main = GuiGetWindowHandle();
    if(main && GetCurrentThreadId() != GetWindowThreadProcessId(main, nullptr))
        GuiExecuteOnGuiThread(+[]() { ShowPanelOnGui(); });
    else
        ShowPanelOnGui();
}

void TiDaojiInit(PLUG_INITSTRUCT* initStruct)
{
    UNREFERENCED_PARAMETER(initStruct);
    char setting[MAX_SETTING_SIZE] = "";
    BridgeSettingGet("TiDaoji", "DriverName", setting);
    if(setting[0] != '\0')
        driverName = setting;

    duint options = 0;
    if(!BridgeSettingGetUint("TiDaoji", "Options", &options))
        BridgeSettingSetUint("TiDaoji", "Options", kDefaultHideType);

    _plugin_registercommand(pluginHandle, "TiDaoji", cbTiDaoji, true);
    _plugin_registercommand(pluginHandle, "TiDaojiUnhide", cbTiDaojiUnhide, true);
    _plugin_registercommand(pluginHandle, "TiDaojiUnhideAll", cbTiDaojiUnhideAll, false);
    _plugin_registercommand(pluginHandle, "TiDaojiSoftUnload", cbTiDaojiSoftUnload, false);
    _plugin_registercommand(pluginHandle, "TiDaojiOptions", cbTiDaojiOptions, false);
    _plugin_registercommand(pluginHandle, "TiDaojiName", cbTiDaojiName, false);
    _plugin_registercommand(pluginHandle, "TiDaojiStatus", cbTiDaojiStatus, false);
    _plugin_registercommand(pluginHandle, "TiDaojiHelp", cbTiDaojiHelp, false);
    _plugin_registercommand(pluginHandle, "TiDaojiPanel", cbTiDaojiPanel, false);

    _plugin_logprintf("[" PLUGIN_NAME "] loaded v%d — Plugins menu: Control Panel; device \\\\.\\%s\n",
                      PLUGIN_VERSION, driverName.c_str());
}

void TiDaojiStop()
{
    _plugin_unregistercommand(pluginHandle, "TiDaojiPanel");
    _plugin_unregistercommand(pluginHandle, "TiDaojiHelp");
    _plugin_unregistercommand(pluginHandle, "TiDaojiStatus");
    _plugin_unregistercommand(pluginHandle, "TiDaojiOptions");
    _plugin_unregistercommand(pluginHandle, "TiDaojiName");
    _plugin_unregistercommand(pluginHandle, "TiDaojiSoftUnload");
    _plugin_unregistercommand(pluginHandle, "TiDaojiUnhideAll");
    _plugin_unregistercommand(pluginHandle, "TiDaojiUnhide");
    _plugin_unregistercommand(pluginHandle, "TiDaoji");
}
