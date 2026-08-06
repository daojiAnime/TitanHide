#include "plugin.h"
#include <windows.h>
#include <stdio.h>
#include <string>
#include "../TiDaoji/TiDaoji.h"

static DWORD pid = 0;
static bool hidden = false;
static std::string driverName = "TiDaoji";

static ULONG GetTiDaojiOptions()
{
    duint options = 0;
    if (!BridgeSettingGetUint("TiDaoji", "Options", &options))
        options = 0xffffffff;
    return (ULONG)options;
}

static bool TiDaojiCall(HIDE_COMMAND Command)
{
    auto path = "\\\\.\\" + driverName;
    HANDLE hDevice = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
    if (hDevice == INVALID_HANDLE_VALUE)
    {
        _plugin_logputs("[" PLUGIN_NAME "] Could not open TiDaoji handle (wrong driver name?)");
        return false;
    }
    HIDE_INFO HideInfo;
    HideInfo.Command = Command;
    HideInfo.Pid = pid;
    HideInfo.Type = GetTiDaojiOptions();
    DWORD written = 0;
    auto result = false;
    if (WriteFile(hDevice, &HideInfo, sizeof(HIDE_INFO), &written, 0))
    {
        _plugin_logprintf("[" PLUGIN_NAME "] Process %shidden!\n", Command == UnhidePid ? "un" : "");
        result = true;
    }
    else
    {
        _plugin_logputs("[" PLUGIN_NAME "] WriteFile error...");
    }
    CloseHandle(hDevice);
    return result;
}

static bool cbTiDaoji(int argc, char* argv[])
{
    if (!hidden)
    {
        _plugin_logprintf("[" PLUGIN_NAME "] Hiding PID %X (%ud)\n", pid, pid);
        if (TiDaojiCall(HidePid))
        {
            DbgCmdExecDirect("hide");
            hidden = true;
        }
    }
    return hidden;
}

static bool cbTiDaojiUnhide(int argc, char* argv[])
{
    if (hidden)
    {
        _plugin_logprintf("[" PLUGIN_NAME "] Unhiding PID %X (%ud)\n", pid, pid);
        if (TiDaojiCall(UnhidePid))
            hidden = false;
    }
    return !hidden;
}

static bool cbTiDaojiOptions(int argc, char* argv[])
{
    if (argc < 2)
    {
        _plugin_logprintf("[" PLUGIN_NAME "] Options: 0x%08X\n", GetTiDaojiOptions());
    }
    else
    {
        duint options = DbgValFromString(argv[1]);
        BridgeSettingSetUint("TiDaoji", "Options", options & 0xffffffff);
        if (hidden)
            TiDaojiCall(HidePid);
        _plugin_logprintf("[" PLUGIN_NAME "] New options: 0x%08X\n", GetTiDaojiOptions());
    }
    return true;
}

static bool cbTiDaojiName(int argc, char* argv[])
{
    if (argc < 2)
    {
        _plugin_logprintf("[" PLUGIN_NAME "] Current driver name: '%s'\n", driverName.c_str());
    }
    else
    {
        driverName = argv[1];
        BridgeSettingSet("TiDaoji", "DriverName", driverName.c_str());
        _plugin_logprintf("[" PLUGIN_NAME "] New driver name: '%s'\n", driverName.c_str());
    }
    return true;
}

PLUG_EXPORT void CBCREATEPROCESS(CBTYPE cbType, PLUG_CB_CREATEPROCESS* info)
{
    pid = info->fdProcessInfo->dwProcessId;
}

PLUG_EXPORT void CBATTACH(CBTYPE cbType, PLUG_CB_ATTACH* info)
{
    pid = info->dwProcessId;
}

PLUG_EXPORT void CBSYSTEMBREAKPOINT(CBTYPE cbType, PLUG_CB_SYSTEMBREAKPOINT* info)
{
    char* argv = "TiDaoji";
    cbTiDaoji(1, &argv);
}

PLUG_EXPORT void CBSTOPDEBUG(CBTYPE cbType, PLUG_CB_STOPDEBUG* info)
{
    char* argv = "TiDaojiUnhide";
    cbTiDaojiUnhide(1, &argv);
}

void TiDaojiInit(PLUG_INITSTRUCT* initStruct)
{
    char setting[MAX_SETTING_SIZE] = "";
    BridgeSettingGet("TiDaoji", "DriverName", setting);
    if (setting[0] != '\0')
    {
        driverName = setting;
    }

    _plugin_registercommand(pluginHandle, "TiDaoji", cbTiDaoji, true);
    _plugin_registercommand(pluginHandle, "TiDaojiUnhide", cbTiDaojiUnhide, true);
    _plugin_registercommand(pluginHandle, "TiDaojiOptions", cbTiDaojiOptions, false);
    _plugin_registercommand(pluginHandle, "TiDaojiName", cbTiDaojiName, false);
}

void TiDaojiStop()
{
    _plugin_unregistercommand(pluginHandle, "TiDaojiOptions");
    _plugin_unregistercommand(pluginHandle, "TiDaojiName");
    _plugin_unregistercommand(pluginHandle, "TiDaojiUnhide");
    _plugin_unregistercommand(pluginHandle, "TiDaoji");
}
