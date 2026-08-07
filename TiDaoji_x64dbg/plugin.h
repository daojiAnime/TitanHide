#ifndef _TIDAOJI_PLUGIN_H
#define _TIDAOJI_PLUGIN_H

#include "pluginmain.h"

// Product rename only; structure matches official TitanHide plugin.h
#define PLUGIN_NAME "TiDaoji"
#define PLUGIN_VERSION 1

void TiDaojiInit(PLUG_INITSTRUCT* initStruct);
void TiDaojiStop();

#endif // _TIDAOJI_PLUGIN_H
