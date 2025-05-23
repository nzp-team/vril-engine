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

int CDAudio_Init(void);
void CDAudio_Play(byte track, qboolean looping);
#ifndef __3DS__
void CDAudio_PlayFromString(char* track_name, qboolean looping);
#endif // __3DS__
void CDAudio_Stop(void);
void CDAudio_Pause(void);
void CDAudio_Resume(void);
void CDAudio_Shutdown(void);
void CDAudio_Update(void);
#ifdef __PSP__
void CDAudio_Next(void);
void CDAudio_Prev(void);
void CDAudio_PrintMusicList(void);
void CDAudio_Track(char* trackname);
#endif // __PSP__