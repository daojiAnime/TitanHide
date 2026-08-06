#include "pluginmain.h"
#include "plugin.h"

int pluginHandle;
HWND hwndDlg;
int hMenu;
int hMenuDisasm;
int hMenuDump;
int hMenuStack;
HINSTANCE g_hInst;

#define MENU_SETTINGS 1

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
    _plugin_menuaddentry(hMenu, MENU_SETTINGS, "&Settings...");
}

PLUG_EXPORT void CBMENUENTRY(CBTYPE cbType, PLUG_CB_MENUENTRY* info)
{
    (void)cbType;
    if(info->hEntry == MENU_SETTINGS)
        TiDaojiShowSettings();
}