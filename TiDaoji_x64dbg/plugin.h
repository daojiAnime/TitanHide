#pragma once
#include "pluginmain.h"

#define PLUGIN_NAME "TiDaoji"
#define PLUGIN_VERSION 7

void TiDaojiInit(PLUG_INITSTRUCT* initStruct);
void TiDaojiSetup(); // real init after bridge ready
void TiDaojiStop();
void TiDaojiShowPanel();
void TiDaojiHideKernelOnly();
void TiDaojiUnhideKernelOnly();
