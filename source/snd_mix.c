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
// snd_mix.c -- portable code to mix sounds for snd_dma.c

#include "nzportable_def.h"

#define DWORD	unsigned long

#define	PAINTBUFFER_SIZE	512
portable_samplepair_t paintbuffer[PAINTBUFFER_SIZE];
int		snd_scaletable[32][256];
int 	*snd_p, snd_linear_count, snd_vol;
short	*snd_out;

void Snd_WriteLinearBlastStereo16 (void);

void Snd_WriteLinearBlastStereo16 (void)
{
	int	i, val;

	for (i=0 ; i<snd_linear_count ; i+=2)
	{
		val = (snd_p[i]*snd_vol)>>8;
		if (val > 0x7fff)
			snd_out[i] = 0x7fff;
		else if (val < (short)0x8000)
			snd_out[i] = (short)0x8000;
		else
			snd_out[i] = val;

		val = (snd_p[i+1]*snd_vol)>>8;
		if (val > 0x7fff)
			snd_out[i+1] = 0x7fff;
		else if (val < (short)0x8000)
			snd_out[i+1] = (short)0x8000;
		else
			snd_out[i+1] = val;
	}
}

void S_TransferStereo16 (int endtime)
{
	int		lpos, lpaintedtime;
	DWORD	*pbuf;

	snd_vol = volume.value*256;

	snd_p = (int *) paintbuffer;
	lpaintedtime = paintedtime;

	{
		pbuf = (DWORD *)shm->buffer;
	}

	while (lpaintedtime < endtime)
	{
	// handle recirculating buffer issues
		lpos = lpaintedtime & ((shm->samples>>1)-1);

		snd_out = (short *) pbuf + (lpos<<1);

		snd_linear_count = (shm->samples>>1) - lpos;
		if (lpaintedtime + snd_linear_count > endtime)
			snd_linear_count = endtime - lpaintedtime;

		snd_linear_count <<= 1;

	// write a linear blast of samples
		Snd_WriteLinearBlastStereo16 ();

		snd_p += snd_linear_count;
		lpaintedtime += (snd_linear_count>>1);
	}
}

void S_TransferPaintBuffer(int endtime)
{
	int 	out_idx, count, out_mask, *p, step, val, snd_vol;
	DWORD	*pbuf;

	if (shm->samplebits == 16 && shm->channels == 2)
	{
		S_TransferStereo16 (endtime);
		return;
	}

	p = (int *) paintbuffer;
	count = (endtime - paintedtime) * shm->channels;
	out_mask = shm->samples - 1;
	out_idx = paintedtime * shm->channels & out_mask;
	step = 3 - shm->channels;
	snd_vol = volume.value*256;


	{
		pbuf = (DWORD *)shm->buffer;
	}

	if (shm->samplebits == 16)
	{
		short *out = (short *) pbuf;
		while (count--)
		{
			val = (*p * snd_vol) >> 8;
			p+= step;
			if (val > 0x7fff)
				val = 0x7fff;
			else if (val < (short)0x8000)
				val = (short)0x8000;
			out[out_idx] = val;
			out_idx = (out_idx + 1) & out_mask;
		}
	}
	else if (shm->samplebits == 8)
	{
		unsigned char *out = (unsigned char *) pbuf;
		while (count--)
		{
			val = (*p * snd_vol) >> 8;
			p+= step;
			if (val > 0x7fff)
				val = 0x7fff;
			else if (val < (short)0x8000)
				val = (short)0x8000;
			out[out_idx] = (val>>8) + 128;
			out_idx = (out_idx + 1) & out_mask;
		}
	}

}


/*
===============================================================================

CHANNEL MIXING

===============================================================================
*/

void SND_PaintChannelFrom8 (channel_t *ch, sfxcache_t *sc, int count, int offset);
void SND_PaintChannelFrom16 (channel_t *ch, sfxcache_t *sc, int count, int offset);

int SND_ChannelSamplesRemaining(const channel_t *ch, const sfxcache_t *sc)
{
	unsigned int step = ch->step ? ch->step : SOUND_FRACONE;
	long long remaining = ((long long)(sc->length - ch->pos) * SOUND_FRACONE) - ch->pos_frac;
	if (remaining <= 0)
		return 0;
	if (step == SOUND_FRACONE)
		return sc->length - ch->pos;
	return (int)((remaining + step - 1) / step);
}

static void SND_PaintChannelResampled(channel_t *ch, sfxcache_t *sc, int count, int offset)
{
	int i, next, a, b, sample;
	unsigned int fraction;
	const signed char *samples8 = (const signed char *)sc->data;
	const short *samples16 = (const short *)sc->data;

	for (i = 0; i < count; i++)
	{
		next = ch->pos + 1;
		if (next >= sc->length)
			next = sc->loopstart >= 0 ? sc->loopstart : ch->pos;
		a = sc->width == 1 ? samples8[ch->pos] * 256 : samples16[ch->pos];
		b = sc->width == 1 ? samples8[next] * 256 : samples16[next];
		sample = a + ((b - a) * (int)(ch->pos_frac >> 8)) / 256;
		paintbuffer[offset + i].left += (sample * ch->leftvol) >> 8;
		paintbuffer[offset + i].right += (sample * ch->rightvol) >> 8;
		fraction = ch->pos_frac + ch->step;
		ch->pos += fraction >> SOUND_FRACBITS;
		ch->pos_frac = fraction & (SOUND_FRACONE - 1);
	}
}

void S_PaintChannels(int endtime)
{
	int		i, end, ltime, count;
	channel_t *ch;
	sfxcache_t	*sc;

	while (paintedtime < endtime)
	{
	// if paintbuffer is smaller than DMA buffer
		end = endtime;
		if (endtime - paintedtime > PAINTBUFFER_SIZE)
			end = paintedtime + PAINTBUFFER_SIZE;

	// clear the paint buffer
		memset(paintbuffer, 0, (end - paintedtime) * sizeof(portable_samplepair_t));

	// paint in the channels.
		ch = channels;
		for (i=0; i<total_channels ; i++, ch++)
		{
			if (!ch->sfx)
				continue;
			if (!(sc = S_LoadSound (ch->sfx)))
				continue;

			ltime = paintedtime;

			while (ltime < end)
			{	// paint up to end
				if (ch->end < end)
					count = ch->end - ltime;
				else
					count = end - ltime;

				if (count > 0)
				{
					if (!ch->leftvol && !ch->rightvol)
					{
						unsigned int step = ch->step ? ch->step : SOUND_FRACONE;
						long long advance = ch->pos_frac + (long long)step * count;
						ch->pos += advance >> SOUND_FRACBITS;
						ch->pos_frac = advance & (SOUND_FRACONE - 1);
					}
					else if (ch->step && ch->step != SOUND_FRACONE)
						SND_PaintChannelResampled(ch, sc, count, ltime - paintedtime);
					else if (sc->width == 1)
						SND_PaintChannelFrom8(ch, sc, count, ltime - paintedtime);
					else
						SND_PaintChannelFrom16(ch, sc, count, ltime - paintedtime);

					ltime += count;
				}

			// if at end of loop, restart
				if (ltime >= ch->end)
				{
					if (sc->loopstart >= 0 && sc->loopstart < sc->length)
					{
						if (ch->pos >= sc->length)
							ch->pos = sc->loopstart + (ch->pos - sc->length) % (sc->length - sc->loopstart);
						else
						{
							ch->pos = sc->loopstart;
							ch->pos_frac = 0;
						}
						ch->end = ltime + SND_ChannelSamplesRemaining(ch, sc);
					}
					else
					{	// channel just stopped
						ch->sfx = NULL;
						break;
					}
				}
			}

		}

	// transfer out according to DMA format
		S_TransferPaintBuffer(end);
		paintedtime = end;
	}
}

void SND_InitScaletable (void)
{
	int		i, j;

	for (i=0 ; i<32 ; i++)
		for (j=0 ; j<256 ; j++)
			snd_scaletable[i][j] = ((signed char)j) * i * 8;
}


void SND_PaintChannelFrom8 (channel_t *ch, sfxcache_t *sc, int count, int offset)
{
	int 		i, data, *lscale, *rscale;
	unsigned char *sfx;

	if (ch->leftvol > 255)
		ch->leftvol = 255;
	if (ch->rightvol > 255)
		ch->rightvol = 255;

	lscale = snd_scaletable[ch->leftvol >> 3];
	rscale = snd_scaletable[ch->rightvol >> 3];
	sfx = (unsigned char *)sc->data + ch->pos;

	for (i=0 ; i<count ; i++)
	{
		data = sfx[i];
		paintbuffer[offset + i].left += lscale[data];
		paintbuffer[offset + i].right += rscale[data];
	}

	ch->pos += count;
}


void SND_PaintChannelFrom16 (channel_t *ch, sfxcache_t *sc, int count, int offset)
{
	int data, left, right, leftvol, rightvol;
	signed short *sfx;
	int	i;

	leftvol = ch->leftvol;
	rightvol = ch->rightvol;
	sfx = (signed short *)sc->data + ch->pos;

	for (i=0 ; i<count ; i++)
	{
		data = sfx[i];
		left = (data * leftvol) >> 8;
		right = (data * rightvol) >> 8;
		paintbuffer[offset + i].left += left;
		paintbuffer[offset + i].right += right;
	}

	ch->pos += count;
}
