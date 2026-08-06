#include "pluginmain.h"
#include "plugin.h"

int pluginHandle;
HWND hwndDlg;
int hMenu;
int hMenuDisasm;
int hMenuDump;
int hMenuStack;
HINSTANCE g_hInst;

enum
{
    MENU_HIDE = 1,
    MENU_UNHIDE,
    MENU_STATUS,
    MENU_SETTINGS,
    MENU_HELP,
};

PLUG_EXPORT bool pluginit(PLUG_INITSTRUCT* initStruct)
{
    initStruct->pluginVersion = PLUGIN_VERSION;
    initStruct->sdkVersion = PLUG_SDKVERSION;
    strncpy_s(initStruct->pluginName, PLUGIN_NAME, _TRUNCATE);
    pluginHandle = initStruct->pluginHandle;
    g_hInst = GetModuleHandleA("TiDaoji.dp64");
    if(!g_hInst)
        g_hInst = GetModuleHandleA("TiDaoji.dp32");
    TiDaojiInit(initStruct);
    return true;
}

PLUG_EXPORT bool plugstop()
{
    TiDaojiStop();
    return true;
}

PLUG_EXPORT void plugsetup(PLUG_SETUPSTRUCT* setupStruct)
{
    hwndDlg = setupStruct->hwndDlg;
    hMenu = setupStruct->hMenu;
    _plugin_menuaddentry(hMenu, MENU_HIDE, "&Hide debuggee");
    _plugin_menuaddentry(hMenu, MENU_UNHIDE, "&Unhide debuggee");
    _plugin_menuaddentry(hMenu, MENU_STATUS, "S&tatus");
    _plugin_menuaddentry(hMenu, MENU_SETTINGS, "Se&ttings...");
    _plugin_menuaddentry(hMenu, MENU_HELP, "&Help");
}

PLUG_EXPORT void CBMENUENTRY(CBTYPE cbType, PLUG_CB_MENUENTRY* info)
{
    (void)cbType;
    switch(info->hEntry)
    {
    case MENU_HIDE:
        DbgCmdExecDirect("TiDaoji");
        break;
    case MENU_UNHIDE:
        DbgCmdExecDirect("TiDaojiUnhide");
        break;
    case MENU_STATUS:
        DbgCmdExecDirect("TiDaojiStatus");
        break;
    case MENU_SETTINGS:
        TiDaojiShowSettings();
        break;
    case MENU_HELP:
        DbgCmdExecDirect("TiDaojiHelp");
        break;
    default:
        break;
    }
}
