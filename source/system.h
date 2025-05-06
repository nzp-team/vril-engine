/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2023 NZ:P Team

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

void Sys_PrintError(const char *function_name, const char *source_file, int line_number, char *message, ...);

#define Sys_Error(message, ...) Sys_PrintError(__func__, __FILE__, __LINE__, message, ##__VA_ARGS__)