/*
Copyright (C) 1996-1997 Id Software, Inc.

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
// sys.h -- non-portable functions

#define PSP_MODEL_PHAT		0
#define PSP_MODEL_SLIM 		1
#define PSP_MODEL_BRITE     2
#define PSP_MODEL_GO        3
#define PSP_MODEL_STREET    4
#define PSP_MODEL_PSVITA 	5
#define PSP_MODEL_EMULATED  6
#define PSP_MODEL_UNKNOWN   100

extern int psp_system_model;

//
// file IO
//

// returns the file size
// return -1 if file is not present
// the file should be in BINARY mode for stupid OSs that care
int Sys_FileOpenRead (char *path, int *hndl);

int Sys_FileOpenWrite (char *path);
void Sys_FileClose (int handle);
void Sys_FileSeek (int handle, int position);
int Sys_FileRead (int handle, void *dest, int count);
int Sys_FileWrite (int handle, void *data, int count);
int	Sys_FileTime (char *path);
void Sys_mkdir (char *path);

//
// memory protection
//
void Sys_MakeCodeWriteable (unsigned long startaddr, unsigned long length);

//
// system IO
//
void Sys_DebugLog(char *file, char *fmt, ...);

void Sys_PrintSystemInfo(void);

void Sys_SystemError(char *error);
// an error will cause the entire program to exit

void Sys_Printf (char *fmt, ...);
// send text to the console

void Sys_Quit (void);

void M_Exit_f (void);

double Sys_FloatTime (void);

char *Sys_ConsoleInput (void);

void Sys_Sleep (void);
// called to yield for a little bit so as
// not to hog cpu when paused or debugging

void Sys_SendKeyEvents (void);
// Perform Key_Event () callbacks until the input que is empty

void Sys_LowFPPrecision (void);
void Sys_HighFPPrecision (void);
void Sys_SetFPCW (void);

// returns psp model
int Sys_GetPSPModel(void);
bool Sys_HasRightStick(void);

void Sys_CaptureScreenshot(void);