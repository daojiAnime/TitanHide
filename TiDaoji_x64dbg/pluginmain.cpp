#include "pluginmain.h"
#include "plugin.h"
#include <windows.h>

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

// x64dbg docs: save HINSTANCE in DllMain for DialogBox resources
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID)
{
    if(fdwReason == DLL_PROCESS_ATTACH)
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
        // Fallback if loader skipped DllMain attach edge cases
        GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&pluginit),
            &g_hInst);
    }
    __try
    {
        TiDaojiInit(initStruct);
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        return false; // refuse load rather than bring down the host
    }
    return true;
}

PLUG_EXPORT bool plugstop()
{
    __try
    {
        TiDaojiStop();
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
    }
    return true;
}

PLUG_EXPORT void plugsetup(PLUG_SETUPSTRUCT* setupStruct)
{
    if(!setupStruct)
        return;
    hwndDlg = setupStruct->hwndDlg;
    hMenu = setupStruct->hMenu;
    hMenuDisasm = setupStruct->hMenuDisasm;
    hMenuDump = setupStruct->hMenuDump;
    hMenuStack = setupStruct->hMenuStack;

    // Keep menu setup simple — no nested state
    if(hMenu)
    {
        _plugin_menuaddentry(hMenu, MENU_PANEL, "&Control Panel...");
        _plugin_menuaddseparator(hMenu);
        _plugin_menuaddentry(hMenu, MENU_HIDE, "&Hide debuggee");
        _plugin_menuaddentry(hMenu, MENU_UNHIDE, "&Unhide debuggee");
        _plugin_menuaddentry(hMenu, MENU_STATUS, "S&tatus (log)");
        _plugin_menuaddentry(hMenu, MENU_HELP, "&Help");
    }
}

PLUG_EXPORT void CBMENUENTRY(CBTYPE cbType, PLUG_CB_MENUENTRY* info)
{
    (void)cbType;
    if(!info)
        return;
    __try
    {
        switch(info->hEntry)
        {
        case MENU_PANEL:
            // Always run dialog on GUI thread (safe if menu already GUI)
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
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        _plugin_logputs("[TiDaoji] CBMENUENTRY exception swallowed");
    }
}
