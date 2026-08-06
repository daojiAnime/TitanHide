#ifndef _TEST_H
#define _TEST_H

#include "pluginmain.h"

#define PLUGIN_NAME "TiDaoji"
// PR4: help/status/unhideall + better errors; keep in sync with README plugin section
#define PLUGIN_VERSION 2

void TiDaojiInit(PLUG_INITSTRUCT* initStruct);
void TiDaojiStop();

#endif // _TEST_H
