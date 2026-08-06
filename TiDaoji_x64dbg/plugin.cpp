#include "plugin.h"
#include <windows.h>
#include <stdio.h>
#include <string>
#include "../TiDaoji/TiDaoji.h"

static DWORD pid = 0;
static bool hidden = false;
static std::string driverName = "TiDaoji";

// Default: all known hide bits (BIT1..BIT11) for VMP-oriented sessions.
static constexpr ULONG kDefaultHideType = 0x7FFu;

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

static bool OpenDevice(HANDLE* out)
{
    *out = INVALID_HANDLE_VALUE;
    auto path = "\\\\.\\" + driverName;
    HANDLE h = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
    if(h == INVALID_HANDLE_VALUE)
    {
        _plugin_logprintf("[" PLUGIN_NAME "] open \\\\.\\%s failed (is service started? name via TiDaojiName)\n",
                          driverName.c_str());
        LogWin32Error("CreateFile");
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

static bool cbTiDaojiHelp(int argc, char* argv[])
{
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);
    _plugin_logputs("[" PLUGIN_NAME "] commands:");
    _plugin_logputs("  TiDaoji              — HidePid for current debuggee (re-apply OK)");
    _plugin_logputs("  TiDaojiUnhide        — UnhidePid for current debuggee");
    _plugin_logputs("  TiDaojiUnhideAll     — UnhideAll (driver table clear)");
    _plugin_logputs("  TiDaojiOptions [n]   — get/set Type bitmask (BridgeSetting TiDaoji/Options)");
    _plugin_logputs("  TiDaojiName [svc]    — get/set device/service name (default TiDaoji)");
    _plugin_logputs("  TiDaojiStatus        — driver open probe + session state");
    _plugin_logputs("  TiDaojiHelp          — this text");
    _plugin_logputs(" Auto: SYSTEMBP -> TiDaoji; stop debug -> TiDaojiUnhide");
    _plugin_logputs(" Requires kernel TiDaoji.sys (InfinityHook). NOT PG-safe. No dual-IH with CR.");
    _plugin_logputs(" Runbook: docs/2026-08-07-tidaoji-dsu-profile-a-runbook.md");
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

PLUG_EXPORT void CBCREATEPROCESS(CBTYPE cbType, PLUG_CB_CREATEPROCESS* info)
{
    UNREFERENCED_PARAMETER(cbType);
    NotePid(info->fdProcessInfo->dwProcessId);
}

PLUG_EXPORT void CBATTACH(CBTYPE cbType, PLUG_CB_ATTACH* info)
{
    UNREFERENCED_PARAMETER(cbType);
    NotePid(info->dwProcessId);
}

PLUG_EXPORT void CBSYSTEMBREAKPOINT(CBTYPE cbType, PLUG_CB_SYSTEMBREAKPOINT* info)
{
    UNREFERENCED_PARAMETER(cbType);
    UNREFERENCED_PARAMETER(info);
    char* argv = "TiDaoji";
    cbTiDaoji(1, &argv);
}

PLUG_EXPORT void CBSTOPDEBUG(CBTYPE cbType, PLUG_CB_STOPDEBUG* info)
{
    UNREFERENCED_PARAMETER(cbType);
    UNREFERENCED_PARAMETER(info);
    char* argv = "TiDaojiUnhide";
    cbTiDaojiUnhide(1, &argv);
}

void TiDaojiInit(PLUG_INITSTRUCT* initStruct)
{
    UNREFERENCED_PARAMETER(initStruct);
    char setting[MAX_SETTING_SIZE] = "";
    BridgeSettingGet("TiDaoji", "DriverName", setting);
    if(setting[0] != '\0')
        driverName = setting;

    // Seed default options once if missing
    duint options = 0;
    if(!BridgeSettingGetUint("TiDaoji", "Options", &options))
        BridgeSettingSetUint("TiDaoji", "Options", kDefaultHideType);

    _plugin_registercommand(pluginHandle, "TiDaoji", cbTiDaoji, true);
    _plugin_registercommand(pluginHandle, "TiDaojiUnhide", cbTiDaojiUnhide, true);
    _plugin_registercommand(pluginHandle, "TiDaojiUnhideAll", cbTiDaojiUnhideAll, false);
    _plugin_registercommand(pluginHandle, "TiDaojiOptions", cbTiDaojiOptions, false);
    _plugin_registercommand(pluginHandle, "TiDaojiName", cbTiDaojiName, false);
    _plugin_registercommand(pluginHandle, "TiDaojiStatus", cbTiDaojiStatus, false);
    _plugin_registercommand(pluginHandle, "TiDaojiHelp", cbTiDaojiHelp, false);

    _plugin_logprintf("[" PLUGIN_NAME "] loaded v%d — TiDaojiHelp; device \\\\.\\%s\n",
                      PLUGIN_VERSION, driverName.c_str());
}

void TiDaojiStop()
{
    _plugin_unregistercommand(pluginHandle, "TiDaojiHelp");
    _plugin_unregistercommand(pluginHandle, "TiDaojiStatus");
    _plugin_unregistercommand(pluginHandle, "TiDaojiOptions");
    _plugin_unregistercommand(pluginHandle, "TiDaojiName");
    _plugin_unregistercommand(pluginHandle, "TiDaojiUnhideAll");
    _plugin_unregistercommand(pluginHandle, "TiDaojiUnhide");
    _plugin_unregistercommand(pluginHandle, "TiDaoji");
}
