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
// d_scan.c
//
// Portable C scan-level rasterization code, all pixel depths.

#include "../../nzportable_def.h"
#include "r_local.h"
#include "d_local.h"

unsigned char	*r_turb_pbase, *r_turb_pdest;
fixed16_t		r_turb_s, r_turb_t, r_turb_sstep, r_turb_tstep;
int				*r_turb_turb;
int				r_turb_spancount;

void D_DrawTurbulent8Span (void);


/*
=============
D_WarpScreen

// this performs a slight compression of the screen at the same time as
// the sine warp, to keep the edges from wrapping
=============
*/
void D_WarpScreen (void)
{
	int		w, h;
	int		u,v;
	byte	*dest;
	int		*turb;
	int		*col;
	byte	**row;
	byte	*rowptr[MAXHEIGHT+(AMP2*2)];
	int		column[MAXWIDTH+(AMP2*2)];
	float	wratio, hratio;

	w = r_refdef.vrect.width;
	h = r_refdef.vrect.height;

	wratio = w / (float)scr_vrect.width;
	hratio = h / (float)scr_vrect.height;

	for (v=0 ; v<scr_vrect.height+AMP2*2 ; v++)
	{
		rowptr[v] = d_viewbuffer + (r_refdef.vrect.y * screenwidth) +
				 (screenwidth * (int)((float)v * hratio * h / (h + AMP2 * 2)));
	}

	for (u=0 ; u<scr_vrect.width+AMP2*2 ; u++)
	{
		column[u] = r_refdef.vrect.x +
				(int)((float)u * wratio * w / (w + AMP2 * 2));
	}

	turb = intsintable + ((int)(cl.time*SPEED)&(CYCLE-1));
	dest = vid.buffer + scr_vrect.y * vid.rowbytes + scr_vrect.x;

	for (v=0 ; v<scr_vrect.height ; v++, dest += vid.rowbytes)
	{
		col = &column[turb[v]];
		row = &rowptr[v];

		for (u=0 ; u<scr_vrect.width ; u+=4)
		{
			dest[u+0] = row[turb[u+0]][col[u+0]];
			dest[u+1] = row[turb[u+1]][col[u+1]];
			dest[u+2] = row[turb[u+2]][col[u+2]];
			dest[u+3] = row[turb[u+3]][col[u+3]];
		}
	}
}


#if	!id386

/*
=============
D_DrawTurbulent8Span
=============
*/
void D_DrawTurbulent8Span (void)
{
	int		sturb, tturb;

	do
	{
		sturb = ((r_turb_s + r_turb_turb[(r_turb_t>>16)&(CYCLE-1)])>>16)&63;
		tturb = ((r_turb_t + r_turb_turb[(r_turb_s>>16)&(CYCLE-1)])>>16)&63;
		*r_turb_pdest++ = *(r_turb_pbase + (tturb<<6) + sturb);
		r_turb_s += r_turb_sstep;
		r_turb_t += r_turb_tstep;
	} while (--r_turb_spancount > 0);
}

#endif	// !id386


/*
=============
Turbulent8
=============
*/
void Turbulent8 (espan_t *pspan)
{
	int				count;
	fixed16_t		snext, tnext;
	float			sdivz, tdivz, zi, z, du, dv, spancountminus1;
	float			sdivz16stepu, tdivz16stepu, zi16stepu;
	
	r_turb_turb = sintable + ((int)(cl.time*SPEED)&(CYCLE-1));

	r_turb_sstep = 0;	// keep compiler happy
	r_turb_tstep = 0;	// ditto

	r_turb_pbase = (unsigned char *)cacheblock;

	sdivz16stepu = d_sdivzstepu * 16;
	tdivz16stepu = d_tdivzstepu * 16;
	zi16stepu = d_zistepu * 16;

	do
	{
		r_turb_pdest = (unsigned char *)((byte *)d_viewbuffer +
				(screenwidth * pspan->v) + pspan->u);

		count = pspan->count;

	// calculate the initial s/z, t/z, 1/z, s, and t and clamp
		du = (float)pspan->u;
		dv = (float)pspan->v;

		sdivz = d_sdivzorigin + dv*d_sdivzstepv + du*d_sdivzstepu;
		tdivz = d_tdivzorigin + dv*d_tdivzstepv + du*d_tdivzstepu;
		zi = d_ziorigin + dv*d_zistepv + du*d_zistepu;
		z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point

		r_turb_s = (int)(sdivz * z) + sadjust;
		if (r_turb_s > bbextents)
			r_turb_s = bbextents;
		else if (r_turb_s < 0)
			r_turb_s = 0;

		r_turb_t = (int)(tdivz * z) + tadjust;
		if (r_turb_t > bbextentt)
			r_turb_t = bbextentt;
		else if (r_turb_t < 0)
			r_turb_t = 0;

		do
		{
		// calculate s and t at the far end of the span
			if (count >= 16)
				r_turb_spancount = 16;
			else
				r_turb_spancount = count;

			count -= r_turb_spancount;

			if (count)
			{
			// calculate s/z, t/z, zi->fixed s and t at far end of span,
			// calculate s and t steps across span by shifting
				sdivz += sdivz16stepu;
				tdivz += tdivz16stepu;
				zi += zi16stepu;
				z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point

				snext = (int)(sdivz * z) + sadjust;
				if (snext > bbextents)
					snext = bbextents;
				else if (snext < 16)
					snext = 16;	// prevent round-off error on <0 steps from
								//  from causing overstepping & running off the
								//  edge of the texture

				tnext = (int)(tdivz * z) + tadjust;
				if (tnext > bbextentt)
					tnext = bbextentt;
				else if (tnext < 16)
					tnext = 16;	// guard against round-off error on <0 steps

				r_turb_sstep = (snext - r_turb_s) >> 4;
				r_turb_tstep = (tnext - r_turb_t) >> 4;
			}
			else
			{
			// calculate s/z, t/z, zi->fixed s and t at last pixel in span (so
			// can't step off polygon), clamp, calculate s and t steps across
			// span by division, biasing steps low so we don't run off the
			// texture
				spancountminus1 = (float)(r_turb_spancount - 1);
				sdivz += d_sdivzstepu * spancountminus1;
				tdivz += d_tdivzstepu * spancountminus1;
				zi += d_zistepu * spancountminus1;
				z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point
				snext = (int)(sdivz * z) + sadjust;
				if (snext > bbextents)
					snext = bbextents;
				else if (snext < 16)
					snext = 16;	// prevent round-off error on <0 steps from
								//  from causing overstepping & running off the
								//  edge of the texture

				tnext = (int)(tdivz * z) + tadjust;
				if (tnext > bbextentt)
					tnext = bbextentt;
				else if (tnext < 16)
					tnext = 16;	// guard against round-off error on <0 steps

				if (r_turb_spancount > 1)
				{
					r_turb_sstep = (snext - r_turb_s) / (r_turb_spancount - 1);
					r_turb_tstep = (tnext - r_turb_t) / (r_turb_spancount - 1);
				}
			}

			r_turb_s = r_turb_s & ((CYCLE<<16)-1);
			r_turb_t = r_turb_t & ((CYCLE<<16)-1);

			D_DrawTurbulent8Span ();

			r_turb_s = snext;
			r_turb_t = tnext;

		} while (count > 0);

	} while ((pspan = pspan->pnext) != NULL);
}


#if	!id386

/*
=============
D_DrawSpans8
=============
*/
#if 0
void D_DrawSpans8 (espan_t *pspan)
{
	int				count, spancount;
	unsigned char	*pbase, *pdest;
	fixed16_t		s, t, snext, tnext, sstep, tstep;
	float			sdivz, tdivz, zi, z, du, dv, spancountminus1;
	float			sdivz8stepu, tdivz8stepu, zi8stepu;

	sstep = 0;	// keep compiler happy
	tstep = 0;	// ditto

	pbase = (unsigned char *)cacheblock;

	sdivz8stepu = d_sdivzstepu * 8;
	tdivz8stepu = d_tdivzstepu * 8;
	zi8stepu = d_zistepu * 8;

	do
	{
		pdest = (unsigned char *)((byte *)d_viewbuffer +
				(screenwidth * pspan->v) + pspan->u);

		count = pspan->count;

	// calculate the initial s/z, t/z, 1/z, s, and t and clamp
		du = (float)pspan->u;
		dv = (float)pspan->v;

		sdivz = d_sdivzorigin + dv*d_sdivzstepv + du*d_sdivzstepu;
		tdivz = d_tdivzorigin + dv*d_tdivzstepv + du*d_tdivzstepu;
		zi = d_ziorigin + dv*d_zistepv + du*d_zistepu;
		z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point

		s = (int)(sdivz * z) + sadjust;
		if (s > bbextents)
			s = bbextents;
		else if (s < 0)
			s = 0;

		t = (int)(tdivz * z) + tadjust;
		if (t > bbextentt)
			t = bbextentt;
		else if (t < 0)
			t = 0;

		do
		{
		// calculate s and t at the far end of the span
			if (count >= 8)
				spancount = 8;
			else
				spancount = count;

			count -= spancount;

			if (count)
			{
			// calculate s/z, t/z, zi->fixed s and t at far end of span,
			// calculate s and t steps across span by shifting
				sdivz += sdivz8stepu;
				tdivz += tdivz8stepu;
				zi += zi8stepu;
				z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point

				snext = (int)(sdivz * z) + sadjust;
				if (snext > bbextents)
					snext = bbextents;
				else if (snext < 8)
					snext = 8;	// prevent round-off error on <0 steps from
								//  from causing overstepping & running off the
								//  edge of the texture

				tnext = (int)(tdivz * z) + tadjust;
				if (tnext > bbextentt)
					tnext = bbextentt;
				else if (tnext < 8)
					tnext = 8;	// guard against round-off error on <0 steps

				sstep = (snext - s) >> 3;
				tstep = (tnext - t) >> 3;
			}
			else
			{
			// calculate s/z, t/z, zi->fixed s and t at last pixel in span (so
			// can't step off polygon), clamp, calculate s and t steps across
			// span by division, biasing steps low so we don't run off the
			// texture
				spancountminus1 = (float)(spancount - 1);
				sdivz += d_sdivzstepu * spancountminus1;
				tdivz += d_tdivzstepu * spancountminus1;
				zi += d_zistepu * spancountminus1;
				z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point
				snext = (int)(sdivz * z) + sadjust;
				if (snext > bbextents)
					snext = bbextents;
				else if (snext < 8)
					snext = 8;	// prevent round-off error on <0 steps from
								//  from causing overstepping & running off the
								//  edge of the texture

				tnext = (int)(tdivz * z) + tadjust;
				if (tnext > bbextentt)
					tnext = bbextentt;
				else if (tnext < 8)
					tnext = 8;	// guard against round-off error on <0 steps

				if (spancount > 1)
				{
					sstep = (snext - s) / (spancount - 1);
					tstep = (tnext - t) / (spancount - 1);
				}
			}

			do
			{
				*pdest++ = *(pbase + (s >> 16) + (t >> 16) * cachewidth);
				s += sstep;
				t += tstep;
			} while (--spancount > 0);

			s = snext;
			t = tnext;

		} while (count > 0);

	} while ((pspan = pspan->pnext) != NULL);
}
#else

extern void test_float32_shift( float *f, int shift );

#define D_DrawSpans8_fw_STEP							\
do {													\
	*pdest++ = *(pbase + (f16_ps >> 16) + curr_t );		\
	f16_ps += f16_ps_step;								\
	curr_t += tstep;									\
	tfrac += tstep_frac;								\
	if( tfrac & 0x10000 )								\
	{													\
		curr_t += i_cachewidth;							\
		tfrac &= 0xffff;								\
	}													\
} while( 0 )


extern void draw_span_nspire_fw_c( draw_span8_nspire_t *ps_dset );
extern void draw_span_nspire_bw_c( draw_span8_nspire_t *ps_dset );


void D_DrawSpans8 (espan_t *pspan)
{
	int				count, spancount, i_cachewidth = cachewidth, i_right_to_left = 0;
	unsigned char	*pbase, *pdest;
	fixed16_t		s, t, snext, tnext, sstep, tstep;
#if !CALCG_FIXED
	float			sdivz, tdivz, zi, z, du, dv, spancountminus1, base_sdivz, base_tdivz, base_iz;
	float			sdivz8stepu, tdivz8stepu, zi8stepu;
	float			f_end_zi, f_end_sdivz, f_end_tdivz, f_end_z;
#else
	int i_du, i_dv;
	fixed32_t		f32_sdivz, f32_tdivz, f32_zi, f32_spancountminus1, f32_base_sdivz, f32_base_tdivz, f32_base_iz;
	fixed16_t       f16_z, f16_zend, f16_zi, f16_end_zi;
	fixed32_t		f32_end_zi, f32_end_sdivz, f32_end_tdivz, f32_end_z;
#endif
	fixed16_t       f16_sstart, f16_tstart, f16_ps, f16_pt, f16_psnext, f16_ptnext, f16_steps, f16_stept, f16_snext, f16_tnext, f16_sstep, f16_tstep, f16_stepp, f16_p, f16_pnext;
	draw_span8_nspire_t s_dset;

	sstep = 0;	// keep compiler happy
	tstep = 0;	// ditto
	f16_sstep = f16_tstep = 0;

	pbase = (unsigned char *)cacheblock;

#if !CALCG_FIXED
	sdivz8stepu = d_sdivzstepu * 8;
	tdivz8stepu = d_tdivzstepu * 8;
	zi8stepu = d_zistepu * 8;
#else
#endif

	s_dset.pbase = pbase;
	s_dset.i_cachewidth = i_cachewidth;
	s_dset.i_bbextents = bbextents;
	s_dset.i_bbextentt = bbextentt;

	do
	{
#if !CALCG_FIXED
		float f_uend;
#else
		float f_zi, f_ziend, f_z, f_zend;
		int i_uend;
#endif
		fixed16_t f16_send, f16_tend;

		pdest = (unsigned char *)((byte *)d_viewbuffer +
				(screenwidth * pspan->v) + pspan->u);

		count = pspan->count;

	// calculate the initial s/z, t/z, 1/z, s, and t and clamp
#if !CALCG_FIXED
		du = (float)pspan->u;
		dv = (float)pspan->v;

		base_sdivz = d_sdivzorigin + dv*d_sdivzstepv;
		base_tdivz = d_tdivzorigin + dv*d_tdivzstepv;
		base_iz = d_ziorigin + dv*d_zistepv;

		sdivz = base_sdivz + du*d_sdivzstepu;
		tdivz = base_tdivz + du*d_tdivzstepu;
		zi = base_iz + du*d_zistepu;
		z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point

		s = (int)(sdivz * z) + sadjust;
		if (s > bbextents)
			s = bbextents;
		else if (s < 0)
			s = 0;

		t = (int)(tdivz * z) + tadjust;
		if (t > bbextentt)
			t = bbextentt;
		else if (t < 0)
			t = 0;
#else
		i_du = pspan->u;
		i_dv = pspan->v;

		f32_base_sdivz = s_spans8_var_package.f32_sdivzorigin + mul_64_32_r64(s_spans8_var_package.f32_sdivzstepv,i_dv);
		f32_base_tdivz = s_spans8_var_package.f32_tdivzorigin + mul_64_32_r64(s_spans8_var_package.f32_tdivzstepv,i_dv);
		f32_base_iz = s_spans8_var_package.f32_ziorigin + mul_64_32_r64(s_spans8_var_package.f32_zistepv,i_dv);

		f32_sdivz = f32_base_sdivz + mul_64_32_r64(s_spans8_var_package.f32_sdivzstepu,i_du);
		f32_tdivz = f32_base_tdivz + mul_64_32_r64(s_spans8_var_package.f32_tdivzstepu,i_du);
		f32_zi = f32_base_iz + mul_64_32_r64(s_spans8_var_package.f32_zistepu,i_du);

		if( f32_zi >= 0x100000000LL )
		{
			f16_zi = 0x7fffffff;
		}
		else if( f32_zi <= 0x20004 )
		{
			f16_zi = 0x10002;
		}
		else
		{
			f16_zi = f32_zi >> 1;
		}
		f16_z = udiv_s31_32( f16_zi, 0x10000 );
		if( f16_z < 0x10000 )
		{
			f16_z = 0x10000;
		}

		s = ((int)( mul_64_32_r64(f32_sdivz, f16_z) >> 32 )) + sadjust;
		if (s > bbextents)
			s = bbextents;
		else if (s < 8)
			s = 8;

		t = ((int)( mul_64_32_r64(f32_tdivz, f16_z) >> 32 )) + tadjust;
		if (t > bbextentt)
			t = bbextentt;
		else if (t < 8)
			t = 8;
#endif

		if( count > 1 )
		{
			int i_count_reciproc = udiv_fast_32_32_incorrect( ( count - 1 ), 0x1000000 );
#if !CALCG_FIXED
			f_uend = (float)( pspan->u + pspan->count - 1 );
			f_end_sdivz = base_sdivz + f_uend*d_sdivzstepu;
			f_end_tdivz = base_tdivz + f_uend*d_tdivzstepu;
			f_end_zi = base_iz + f_uend*d_zistepu;
			f_end_z = ( float )0x10000 / f_end_zi;
			f16_send = ((int)(f_end_sdivz * f_end_z )) + sadjust;
			f16_tend = ((int)(f_end_tdivz * f_end_z )) + tadjust;
#else

			i_uend = ( pspan->u + count - 1 );
			f32_end_sdivz = f32_base_sdivz + mul_64_32_r64(s_spans8_var_package.f32_sdivzstepu,i_uend);;
			f32_end_tdivz = f32_base_tdivz + mul_64_32_r64(s_spans8_var_package.f32_tdivzstepu,i_uend);;
			f32_end_zi = f32_base_iz + mul_64_32_r64(s_spans8_var_package.f32_zistepu,i_uend); /* do we need f32, maybe just let it overflow ? */

			if( f32_end_zi >= 0x100000000LL )
			{
				f16_end_zi = 0x7fffffff;
			}
			else if( f32_zi <= 0x20004 )
			{
				f16_end_zi = 0x10002;
			}
			else
			{
				f16_end_zi = f32_end_zi >> 1;
			}
			f16_zend = udiv_s31_32( f16_end_zi, 0x10000 );
			if( f16_zend < 0x10000 )
			{
				f16_zend = 0x10000;
			}
			f16_send = ((int)( (mul_64_32_r64( f32_end_sdivz, f16_zend)) >> 32 )) + sadjust;
			f16_tend = ((int)( (mul_64_32_r64( f32_end_tdivz, f16_zend)) >> 32 )) + tadjust;

#endif
			if (f16_send > s_dset.i_bbextents)
				f16_send = s_dset.i_bbextents;
			if (f16_send < 8)
				f16_send = 8;
			if (f16_tend > s_dset.i_bbextentt)
				f16_tend = s_dset.i_bbextentt;
			if (f16_tend < 8)
				f16_tend = 8;

			if( f16_z > f16_zend )
			{
				i_right_to_left = 1;
				pdest += pspan->count - 1;
			}
			else
			{
				i_right_to_left = 0;
			}
			s_dset.pdest = pdest;
			s_dset.count = count;

			if( !i_right_to_left )
			{
				f16_steps = llmull_s24( ( f16_send - s ), i_count_reciproc );
				f16_steps -= f16_steps >> 31;
				f16_stept = llmull_s24( ( f16_tend - t ), i_count_reciproc );
				f16_stept -= f16_stept >> 31;
				f16_ps = f16_sstart = s;
				f16_pt = f16_tstart = t;
#if !CALCG_FIXED
				f16_p = ( f_end_z * ( float )0x10000 ) / z;
#else
				f16_p = udiv_64_32( ( ( long long )f16_zend << 16 ), f16_z );
#endif
				s_dset.f16_sstart = f16_sstart;
				s_dset.f16_ps = f16_ps;
				s_dset.f16_psend = f16_send;
				s_dset.f16_steps = f16_steps;
				s_dset.f16_tstart = f16_tstart;
				s_dset.f16_pt = f16_pt;
				s_dset.f16_ptend = f16_tend;
				s_dset.f16_stept = f16_stept;
			}
			else
			{
				f16_steps = llmull_s24( ( s - f16_send ), i_count_reciproc );
				f16_steps -= f16_steps >> 31;
				f16_stept = llmull_s24( ( t - f16_tend ), i_count_reciproc );
				f16_stept -= f16_stept >> 31;

				f16_ps = f16_sstart = f16_send;
				f16_pt = f16_tstart = f16_tend;
#if !CALCG_FIXED
				f16_p = ( f_end_z * ( float )0x10000 ) / z;
#else
				f16_p = udiv_64_32( ( long long )f16_z << 16, f16_zend );
#endif
				s_dset.f16_sstart = f16_sstart;
				s_dset.f16_ps = f16_ps;
				s_dset.f16_psend = s;
				s_dset.f16_steps = f16_steps;
				s_dset.f16_tstart = f16_tstart;
				s_dset.f16_pt = f16_pt;
				s_dset.f16_ptend = t;
				s_dset.f16_stept = f16_stept;
			}

			if( f16_p < 0x100 )
			{
				f16_p = 0x100;
			}
			else if( f16_p > 0xffffff )
			{
				f16_p = 0xffffff;
			}
			f16_stepp = llmull_s21(( 0x10000 - f16_p ), i_count_reciproc );
			s_dset.f16_stepp = f16_stepp + 1;
			s_dset.f16_p = f16_p;
		}
		else
		{
			f16_ps = s;
			f16_pt = t;
		}


		if( !i_right_to_left )
		{
			if( count > 1 )
			{
				draw_span_nspire_fw_c( &s_dset );
			}
			else
			{
				*pdest++ = *(pbase + (f16_ps >> 16) + (f16_pt >> 16) * i_cachewidth);
			}
#endif
		}
		else
		{
			if( count > 1 )
			{
				draw_span_nspire_bw_c( &s_dset );
			}
			else
			{
				*pdest++ = *(pbase + (f16_ps >> 16) + (f16_pt >> 16) * i_cachewidth);
			}
		}
	} while ((pspan = pspan->pnext) != NULL);
}
#endif


#if	!id386

/*
=============
D_DrawZSpans
=============
*/

#define CHEAP_NSPIRE_ZSPANS 1

void D_DrawZSpans (espan_t *pspan)
{
	int				count, doublecount, izistep;
	int				izi;
	short			*pdest;
	unsigned		ltemp;
#if !CALCG_FIXED
	double			zi;
	float			du, dv;
#else
	fixed32_t		f32_zi;
	fixed16_t		i_du, i_dv;
#endif

// FIXME: check for clamping/range problems
// we count on FP exceptions being turned off to avoid range problems
#if !CALCG_FIXED
	izistep = (int)(d_zistepu * 0x8000 * 0x10000);
#else
	izistep = s_spans8_var_package.f32_zistepu >> 1;
#endif

	do
	{
		pdest = d_pzbuffer + (d_zwidth * pspan->v) + pspan->u;

		count = pspan->count;

	// calculate the initial 1/z
#if !CALCG_FIXED
		du = (float)pspan->u;
		dv = (float)pspan->v;

		zi = d_ziorigin + dv*d_zistepv + du*d_zistepu;
	// we count on FP exceptions being turned off to avoid range problems
		izi = (int)(zi * 0x8000 * 0x10000);
#else
		i_du = pspan->u;
		i_dv = pspan->v;

		f32_zi = s_spans8_var_package.f32_ziorigin + mul_64_32_r64(s_spans8_var_package.f32_zistepv,i_dv) + mul_64_32_r64(s_spans8_var_package.f32_zistepu,i_du);
		izi = f32_zi >> 1;
#endif

#if !CHEAP_NSPIRE_ZSPANS
		if ((long)pdest & 0x02)
		{
			*pdest++ = (short)(izi >> 16);
			izi += izistep;
			count--;
		}

		if ((doublecount = count >> 1) > 0)
		{
			do
			{
				ltemp = izi >> 16;
				izi += izistep;
				ltemp |= izi & 0xFFFF0000;
				izi += izistep;
				*(int *)pdest = ltemp;
				pdest += 2;
			} while (--doublecount > 0);
		}

		if (count & 1)
			*pdest = (short)(izi >> 16);
#else
		do
		{
			switch( count )
			{
			default:
			case 8:
				ltemp = izi >> 16;
				izi += izistep;
				*pdest = ltemp;
				pdest++;
				/* fall through */
			case 7:
				ltemp = izi >> 16;
				izi += izistep;
				*pdest = ltemp;
				pdest++;
				/* fall through */
			case 6:
				ltemp = izi >> 16;
				izi += izistep;
				*pdest = ltemp;
				pdest++;
				/* fall through */
			case 5:
				ltemp = izi >> 16;
				izi += izistep;
				*pdest = ltemp;
				pdest++;
				/* fall through */
			case 4:
				ltemp = izi >> 16;
				izi += izistep;
				*pdest = ltemp;
				pdest++;
				/* fall through */
			case 3:
				ltemp = izi >> 16;
				izi += izistep;
				*pdest = ltemp;
				pdest++;
				/* fall through */
			case 2:
				ltemp = izi >> 16;
				izi += izistep;
				*pdest = ltemp;
				pdest++;
				/* fall through */
			case 1:
				ltemp = izi >> 16;
				izi += izistep;
				*pdest = ltemp;
				pdest++;
				/* fall through */
			case 0:
				;
			}
			count -= 8;
		} while( count > 0 );
#endif
	} while ((pspan = pspan->pnext) != NULL);
}

#endif

