#ifndef __TOUCH__
#define __TOUCH__

//Touchscreen mode identifiers
#define TMODE_TOUCHPAD 1
#define TMODE_SETTINGS 2

void Touch_TouchpadTap();
void Touch_ProcessTap();
void Touch_DrawOverlay();
void Touch_Init();
void Touch_Update();

#endif
