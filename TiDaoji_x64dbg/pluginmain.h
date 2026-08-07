#pragma once

// Minimal includes only — full SDK template (Yara/TitanEngine/capstone) can
// crash x64dbg at load due to static init / CRT. TiDaoji only needs bridge + plugins.
#include "pluginsdk/bridgemain.h"
#include "pluginsdk/_plugins.h"

#ifdef _WIN64
#pragma comment(lib, "pluginsdk/x64dbg.lib")
#pragma comment(lib, "pluginsdk/x64bridge.lib")
#else
#pragma comment(lib, "pluginsdk/x32dbg.lib")
#pragma comment(lib, "pluginsdk/x32bridge.lib")
#endif

#define Cmd(x) DbgCmdExecDirect(x)
#define PLUG_EXPORT extern "C" __declspec(dllexport)

extern int pluginHandle;
extern HWND hwndDlg;
extern int hMenu;
extern int hMenuDisasm;
extern int hMenuDump;
extern int hMenuStack;
extern HINSTANCE g_hInst;
