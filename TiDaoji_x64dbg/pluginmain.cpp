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
    MENU_PANEL = 1,
    MENU_HIDE,
    MENU_UNHIDE,
    MENU_STATUS,
    MENU_HELP,
};

// Prefer hModule from pluginit path via GetModuleHandle; fallback names.
static HINSTANCE ResolvePluginModule()
{
    HINSTANCE h = GetModuleHandleA("TiDaoji.dp64");
    if(!h)
        h = GetModuleHandleA("TiDaoji.dp32");
    return h;
}

PLUG_EXPORT bool pluginit(PLUG_INITSTRUCT* initStruct)
{
    initStruct->pluginVersion = PLUGIN_VERSION;
    initStruct->sdkVersion = PLUG_SDKVERSION;
    strncpy_s(initStruct->pluginName, PLUGIN_NAME, _TRUNCATE);
    pluginHandle = initStruct->pluginHandle;
    g_hInst = ResolvePluginModule();
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
    // Primary UI first — user finds Control Panel under Plugins -> TiDaoji
    _plugin_menuaddentry(hMenu, MENU_PANEL, "&Control Panel...");
    _plugin_menuaddseparator(hMenu);
    _plugin_menuaddentry(hMenu, MENU_HIDE, "&Hide debuggee");
    _plugin_menuaddentry(hMenu, MENU_UNHIDE, "&Unhide debuggee");
    _plugin_menuaddentry(hMenu, MENU_STATUS, "S&tatus (log)");
    _plugin_menuaddentry(hMenu, MENU_HELP, "&Help");
}

PLUG_EXPORT void CBMENUENTRY(CBTYPE cbType, PLUG_CB_MENUENTRY* info)
{
    (void)cbType;
    switch(info->hEntry)
    {
    case MENU_PANEL:
        TiDaojiShowPanel();
        break;
    case MENU_HIDE:
        DbgCmdExecDirect("TiDaoji");
        break;
    case MENU_UNHIDE:
        DbgCmdExecDirect("TiDaojiUnhide");
        break;
    case MENU_STATUS:
        DbgCmdExecDirect("TiDaojiStatus");
        break;
    case MENU_HELP:
        DbgCmdExecDirect("TiDaojiHelp");
        break;
    default:
        break;
    }
}
