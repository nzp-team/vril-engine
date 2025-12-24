/*
    This file comes from the source code of Red Viper:
    see https://github.com/skyfloogle/red-viper/blob/master/include/cpp.h
*/

#pragma once

#ifdef __3DS__
#include <3ds/types.h>
#include <3ds/services/hid.h>

Result cppInit(void);
void cppExit(void);
bool cppGetConnected(void);
void cppCircleRead(circlePosition *pos);
u32 cppKeysHeld(void);
u32 cppKeysDown(void);
u32 cppKeysUp(void);
u8 cppBatteryLevel(void);
#endif
