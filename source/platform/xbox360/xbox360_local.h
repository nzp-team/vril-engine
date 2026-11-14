#pragma once

int
X360_GraphicsInit(void);
int
X360_GraphicsSwap(int vsync);
void
X360_GraphicsShutdown(void);
const char *
X360_GraphicsError(void);
void
X360_AudioUpdate(void);
void
X360_NetworkPoll(void);
void
X360_GraphicsShowError(const char * error);
