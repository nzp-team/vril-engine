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

int
music_init(void);
void
music_deinit(void);
void
music_update(void);
int
music_start_play(char * fname, int pos);
void
music_pause(void);
void
music_resume(void);
void
music_stop(void);

extern volatile int music_job_started;
extern int music_volume;
extern qboolean music_paused;

#define PLATFORM_VOLUME_MAX 32767
