#ifndef _TIDAOJI_PLUGIN_H
#define _TIDAOJI_PLUGIN_H

#include "pluginmain.h"

#define PLUGIN_NAME "TiDaoji"
// v4: control panel UI + deployable error hints
#define PLUGIN_VERSION 5

void TiDaojiInit(PLUG_INITSTRUCT* initStruct);
void TiDaojiStop();
void TiDaojiShowPanel(); // control panel (primary UI)
// kernel-only hide (no DbgCmdExec — safe from SYSTEMBP callbacks)
void TiDaojiHideKernelOnly();
void TiDaojiUnhideKernelOnly();

#endif
