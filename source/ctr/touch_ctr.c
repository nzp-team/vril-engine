/*
Copyright (C) 2015 Felipe Izzo

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

//FIX ME: load all hardcoded values from file

#include "../quakedef.h"

#include <3ds.h>
#include "touch_ctr.h"

u16* touchOverlay;
char lastKey = 0;
int tmode;
u16* tfb;
touchPosition oldtouch, touch;
u64 tick;

u64 lastTap = 0;

int shiftToggle = 0;

void Touch_Init(){
  tmode = TMODE_TOUCHPAD; //Start in touchpad Mode
  tfb = (u16*)gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);

  //Load overlay files from sdmc for easier testing
  FILE *texture = fopen("touchOverlay.bin", "rb");
  if(!texture)
    Sys_Error("Could not open touchpadOverlay.bin\n");
  fseek(texture, 0, SEEK_END);
  int size = ftell(texture);
  fseek(texture, 0, SEEK_SET);
  touchOverlay = malloc(size);
  fread(touchOverlay, 1, size, texture);
  fclose(texture);
}

void Touch_DrawOverlay()
{
  int x, y;
  for(x=0; x<320; x++){
    for(y=0; y<240;y++){
      tfb[(x*240 + (239 - y))] = touchOverlay[(y*320 + x)];
    }
  }
}

void Touch_Update(){
  if(lastKey){
    Key_Event(lastKey, false);
    lastKey = 0;
  }

  if(hidKeysDown() & KEY_TOUCH){
    hidTouchRead(&touch);
    tick = Sys_FloatTime();
  }

  if(hidKeysUp() & KEY_TOUCH){
      Touch_ProcessTap();
  }
}

void Touch_ProcessTap()
{
  if(touch.px > 268 && touch.py > 14 && touch.py < 226)
    Touch_SideBarTap();
}

void Touch_SideBarTap()
{
  uint16_t y = (touch.py - 14)/42;
  lastKey = K_AUX9 + y;
  Key_Event(lastKey, true);
}