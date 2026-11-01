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

#include "../../nzportable_def.h"
#include <kernel.h>
#include <timer.h>
#include <timer_alarm.h>
#include <stdlib.h>
#include <stdint.h>

extern char _ftext[], _etext[];
static unsigned int *profile_samples;
static unsigned int profile_low, profile_count, profile_outside;
static int profile_alarm = -1;
static volatile int profile_active;

static u64 Sys_PS2Sample(s32 id, u64 scheduled, u64 actual, void *arg, void *pc_value)
{
	unsigned int pc = (uintptr_t)pc_value & 0x0fffffff;
	(void)id;
	(void)scheduled;
	(void)actual;
	(void)arg;
	if (!profile_active)
		return 0;
	if (pc >= profile_low && ((pc - profile_low) >> 7) < profile_count)
		profile_samples[(pc - profile_low) >> 7]++;
	else
		profile_outside++;
	profile_alarm = iSetTimerAlarm(kBUSCLK / 1000, Sys_PS2Sample, NULL);
	return 0;
}

static void Sys_PS2ProfileStop(void)
{
	FILE *file;
	unsigned int header[4];
	if (!profile_active)
		return;
	profile_active = 0;
	if (profile_alarm >= 0)
		ReleaseTimerAlarm(profile_alarm);
	profile_alarm = -1;
	header[0] = profile_low;
	header[1] = 7;
	header[2] = profile_count;
	header[3] = profile_outside;
	file = fopen("mass:/APPS/NZPORTABLE/cpu-profile.bin", "wb");
	if (file) {
		fwrite(header, sizeof(header), 1, file);
		fwrite(profile_samples, sizeof(*profile_samples), profile_count, file);
		fclose(file);
		Con_Printf("CPU samples saved to cpu-profile.bin\n");
	}
	free(profile_samples);
	profile_samples = NULL;
}

static void Sys_PS2ProfileStart(void)
{
	if (profile_active)
		return;
	profile_low = (uintptr_t)_ftext & 0x0fffffff;
	profile_count = (((uintptr_t)_etext & 0x0fffffff) - profile_low + 127) >> 7;
	profile_samples = calloc(profile_count, sizeof(*profile_samples));
	if (!profile_samples) {
		Con_Printf("Not enough memory for CPU samples\n");
		return;
	}
	profile_outside = 0;
	profile_active = 1;
	profile_alarm = SetTimerAlarm(kBUSCLK / 1000, Sys_PS2Sample, NULL);
	if (profile_alarm < 0) {
		profile_active = 0;
		free(profile_samples);
		profile_samples = NULL;
	}
}

void Sys_PS2ProfileInit(void)
{
	Cmd_AddCommand("ps2_cpu_start", Sys_PS2ProfileStart);
	Cmd_AddCommand("ps2_cpu_stop", Sys_PS2ProfileStop);
}
