#include "pluginmain.h"
#include "plugin.h"
#include <windows.h>
#include <string.h>

int pluginHandle = 0;
HWND hwndDlg = nullptr;
int hMenu = 0;
int hMenuDisasm = 0;
int hMenuDump = 0;
int hMenuStack = 0;
HINSTANCE g_hInst = nullptr;

enum
{
    MENU_PANEL = 1,
    MENU_HIDE,
    MENU_UNHIDE,
    MENU_STATUS,
    MENU_HELP,
};

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD reason, LPVOID)
{
    if(reason == DLL_PROCESS_ATTACH)
    {
        g_hInst = hinstDLL;
        DisableThreadLibraryCalls(hinstDLL);
    }
    return TRUE;
}

PLUG_EXPORT bool pluginit(PLUG_INITSTRUCT* initStruct)
{
    if(!initStruct)
        return false;
    initStruct->pluginVersion = PLUGIN_VERSION;
    initStruct->sdkVersion = PLUG_SDKVERSION;
    strncpy_s(initStruct->pluginName, PLUGIN_NAME, _TRUNCATE);
    pluginHandle = initStruct->pluginHandle;
    if(!g_hInst)
    {
        GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCSTR)(void*)&pluginit,
            &g_hInst);
    }
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
    if(!setupStruct)
        return;
    hwndDlg = setupStruct->hwndDlg;
    hMenu = setupStruct->hMenu;
    if(!hMenu)
        return;
    _plugin_menuaddentry(hMenu, MENU_PANEL, "&Control Panel...");
    _plugin_menuaddseparator(hMenu);
    _plugin_menuaddentry(hMenu, MENU_HIDE, "&Hide debuggee");
    _plugin_menuaddentry(hMenu, MENU_UNHIDE, "&Unhide debuggee");
    _plugin_menuaddentry(hMenu, MENU_STATUS, "S&tatus");
    _plugin_menuaddentry(hMenu, MENU_HELP, "&Help");
}

PLUG_EXPORT void CBMENUENTRY(CBTYPE, PLUG_CB_MENUENTRY* info)
{
    if(!info)
        return;
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
