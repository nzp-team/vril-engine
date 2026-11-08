/*
 * Copyright (C) 2026 NZ:P Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <windows.h>
#include <hal/debug.h>
#include <hal/video.h>
#include <hal/xbox.h>
#include <nxdk/mount.h>
#include <stdio.h>
#include <string.h>

static int CopyFileNative(const char *source, const char *target)
{
    static unsigned char buffer[65536];
    HANDLE input = CreateFile(source, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    HANDLE output = INVALID_HANDLE_VALUE;
    DWORD remaining = 0;
    int success = input != INVALID_HANDLE_VALUE;
    if (success) {
        remaining = GetFileSize(input, NULL);
        output = CreateFile(target, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        success = remaining != INVALID_FILE_SIZE && output != INVALID_HANDLE_VALUE;
    }
    while (success && remaining) {
        DWORD read, written, amount = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
        success = ReadFile(input, buffer, amount, &read, NULL) && read == amount &&
            WriteFile(output, buffer, read, &written, NULL) && written == read;
        if (success) remaining -= read;
    }
    if (!success) debugPrint("Copy failed: %s -> %s (%lu)\n", source, target, GetLastError());
    if (input != INVALID_HANDLE_VALUE) CloseHandle(input);
    if (output != INVALID_HANDLE_VALUE) CloseHandle(output);
    return success;
}

static int CopyDirectory(const char *source, const char *target)
{
    char pattern[256], from[256], to[256];
    WIN32_FIND_DATA info;
    HANDLE handle;
    int success = 1;
    CreateDirectory(target, NULL);
    snprintf(pattern, sizeof(pattern), "%s\\*", source);
    handle = FindFirstFile(pattern, &info);
    if (handle == INVALID_HANDLE_VALUE) { debugPrint("Directory failed: %s (%lu)\n", source, GetLastError()); return 0; }
    do {
        if (!strcmp(info.cFileName, ".") || !strcmp(info.cFileName, "..")) continue;
        if (snprintf(from, sizeof(from), "%s\\%s", source, info.cFileName) >= sizeof(from) ||
            snprintf(to, sizeof(to), "%s\\%s", target, info.cFileName) >= sizeof(to)) {
            success = 0; break;
        }
        if (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            success = CopyDirectory(from, to);
        else
            success = CopyFileNative(from, to);
        if (!success) break;
    } while (FindNextFile(handle, &info));
    FindClose(handle);
    return success;
}

int main(void)
{
    XVideoSetMode(640, 480, 32, REFRESH_DEFAULT);
    if (!nxMountDrive('E', "\\Device\\Harddisk0\\Partition1")) goto failed;
    CreateDirectory("E:\\Games", NULL);
    CreateDirectory("E:\\Games\\nzportable", NULL);
    DeleteFile("E:\\Games\\nzportable\\capture.bmp");
    DeleteFile("E:\\Games\\nzportable\\nzp\\condebug.log");
    if (!CopyDirectory("D:\\nzp", "E:\\Games\\nzportable\\nzp") ||
        !CopyFileNative("D:\\vril.xbe", "E:\\Games\\nzportable\\default.xbe") ||
        !CopyFileNative("D:\\setup.ini", "E:\\Games\\nzportable\\setup.ini")) goto failed;
    XLaunchXBE("E:\\Games\\nzportable\\default.xbe");
failed:
    debugPrint("Xbox test installation or launch failed.\n");
    for (;;) Sleep(1000);
}
