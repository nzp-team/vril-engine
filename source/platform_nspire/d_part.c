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
// d_part.c: software driver module for drawing particles

#include "../nzportable_def.h"
#include "d_local.h"


/*
==============
D_EndParticles
==============
*/
void D_EndParticles (void)
{
// not used by software driver
}


/*
==============
D_StartParticles
==============
*/
void D_StartParticles (void)
{
// not used by software driver
}


#if	!id386

/*
==============
D_DrawParticle
==============
*/
#define PARTICLE_FIXED_TRANSFORM_DOTP( v1, v2 ) ( ( llmull_s0( v1[ 0 ], v2[ 0 ] ) + llmull_s0( v1[ 1 ], v2[ 1 ] ) + llmull_s0( v1[ 2 ], v2[ 2 ] ) + ( 1 << 15 ) ) >> 16 )

void D_DrawParticle (particle_t *pparticle)
{
#if !FIXED_POINT_PARTICLES
	vec3_t	local, transformed;
	float	zi;
#else
	fixed16_t f16_zi, rgf16_local[ 3 ], rgf16_transformed[ 3 ];
	int local_screenwidth = screenwidth;
	int local_d_zwidth = d_zwidth;
#endif
	byte	*pdest;
	short	*pz;
	int		i, izi, pix, count, u, v, color;

// transform point
#if !FIXED_POINT_PARTICLES
	VectorSubtract (pparticle->org, r_origin, local);

	transformed[0] = DotProduct(local, r_pright);
	transformed[1] = DotProduct(local, r_pup);
	transformed[2] = DotProduct(local, r_ppn);		

	if (transformed[2] < PARTICLE_Z_CLIP)
		return;

// project the point
// FIXME: preadjust xcenter and ycenter
	zi = 1.0 / transformed[2];
	u = (int)(xcenter + zi * transformed[0] + 0.5);
	v = (int)(ycenter - zi * transformed[1] + 0.5);
	color = pparticle->color;
#else
	rgf16_local[ 0 ] = pparticle->rgf16_org[ 0 ] - r_porigin_fixed[ 0 ];
	rgf16_local[ 1 ] = pparticle->rgf16_org[ 1 ] - r_porigin_fixed[ 1 ];
	rgf16_local[ 2 ] = pparticle->rgf16_org[ 2 ] - r_porigin_fixed[ 2 ];
	
	rgf16_transformed[ 0 ] = PARTICLE_FIXED_TRANSFORM_DOTP( rgf16_local, r_pright_fixed );
	rgf16_transformed[ 1 ] = PARTICLE_FIXED_TRANSFORM_DOTP( rgf16_local, r_pup_fixed );
	rgf16_transformed[ 2 ] = PARTICLE_FIXED_TRANSFORM_DOTP( rgf16_local, r_ppn_fixed );
	if( rgf16_transformed[ 2 ] < ((int)PARTICLE_Z_CLIP)<<8 )
	{
		return;
	}
	f16_zi = ( 0xffffffffU / rgf16_transformed[ 2 ] );
	u = (int)(( pxcenter_fixed + (int)( ( (long long)rgf16_transformed[ 0 ] * f16_zi ) >> 24 ) )>>8);
	v = (int)(( pycenter_fixed - (int)( ( (long long)rgf16_transformed[ 1 ] * f16_zi ) >> 24 ) )>>8);
	color = pparticle->f16_color;
#endif

	if ((v > d_vrectbottom_particle) || 
		(u > d_vrectright_particle) ||
		(v < d_vrecty) ||
		(u < d_vrectx))
	{
		return;
	}

	pz = d_pzbuffer + (d_zwidth * v) + u;
	pdest = d_viewbuffer + d_scantable[v] + u;
#if !FIXED_POINT_PARTICLES
	izi = (int)(zi * 0x8000);
#else
	izi = f16_zi >> 9;
#endif
	pix = izi >> d_pix_shift;

	if (pix < d_pix_min)
		pix = d_pix_min;
	else if (pix > d_pix_max)
		pix = d_pix_max;

	switch (pix)
	{
	case 1:
		count = 1 << d_y_aspect_shift;

		for ( ; count ; count--, pz += local_d_zwidth, pdest += local_screenwidth )
		{
			if (pz[0] <= izi)
			{
				pz[0] = izi;
				pdest[0] = color;
			}
		}
		break;

	case 2:
		count = 2 << d_y_aspect_shift;

		for ( ; count ; count--, pz += local_d_zwidth, pdest += local_screenwidth )
		{
			if (pz[0] <= izi)
			{
				pz[0] = izi;
				pdest[0] = color;
			}

			if (pz[1] <= izi)
			{
				pz[1] = izi;
				pdest[1] = color;
			}
		}
		break;

	case 3:
		count = 3 << d_y_aspect_shift;

		for ( ; count ; count--, pz += local_d_zwidth, pdest += local_screenwidth )
		{
			if (pz[0] <= izi)
			{
				pz[0] = izi;
				pdest[0] = color;
			}

			if (pz[1] <= izi)
			{
				pz[1] = izi;
				pdest[1] = color;
			}

			if (pz[2] <= izi)
			{
				pz[2] = izi;
				pdest[2] = color;
			}
		}
		break;

	case 4:
		count = 4 << d_y_aspect_shift;

		for ( ; count ; count--, pz += local_d_zwidth, pdest += local_screenwidth )
		{
			if (pz[0] <= izi)
			{
				pz[0] = izi;
				pdest[0] = color;
			}

			if (pz[1] <= izi)
			{
				pz[1] = izi;
				pdest[1] = color;
			}

			if (pz[2] <= izi)
			{
				pz[2] = izi;
				pdest[2] = color;
			}

			if (pz[3] <= izi)
			{
				pz[3] = izi;
				pdest[3] = color;
			}
		}
		break;

	default:
		count = pix << d_y_aspect_shift;

		for ( ; count ; count--, pz += local_d_zwidth, pdest += local_screenwidth )
		{
			for (i=0 ; i<pix ; i++)
			{
				if (pz[i] <= izi)
				{
					pz[i] = izi;
					pdest[i] = color;
				}
			}
		}
		break;
	}
}

#endif	// !id386

