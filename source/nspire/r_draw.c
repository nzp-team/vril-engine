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

// r_draw.c

#include "../nzportable_def.h"
#include "r_local.h"
#include "d_local.h"	// FIXME: shouldn't need to include this

#define MAXLEFTCLIPEDGES		100

// !!! if these are changed, they must be changed in asm_draw.h too !!!
#define FULLY_CLIPPED_CACHED	0x80000000
#define FRAMECOUNT_MASK			0x7FFFFFFF

unsigned int	cacheoffset;

int			c_faceclip;					// number of faces clipped

zpointdesc_t	r_zpointdesc;

polydesc_t		r_polydesc;



clipplane_t	*entity_clipplanes;
clipplane_t	view_clipplanes[4];
clipplane_t	world_clipplanes[16];

clipplane_fixed_t	view_clipplanes_fixed[4];

medge_t			*r_pedge;

qboolean		r_leftclipped, r_rightclipped;
static qboolean	makeleftedge, makerightedge;
qboolean		r_nearzionly;

int		sintable[SIN_BUFFER_SIZE];
int		intsintable[SIN_BUFFER_SIZE];

mvertex_t	r_leftenter, r_leftexit;
mvertex_t	r_rightenter, r_rightexit;

typedef struct
{
	float	u,v;
	int		ceilv;
} evert_t;

int				r_emitted;
float			r_nearzi;

#if !PARTIALY_FIXED_TRANSFORM
float			r_u1, r_v1, r_lzi1;
#else
fixed16_t f16_r_u1, f16_r_v1, f16_r_lzi1;
float r_lzi1;
#endif
int				r_ceilv1;

qboolean	r_lastvertvalid;


#if	!id386

/*
================
R_EmitEdge
================
*/

#define PARTIALY_FIXED_TRANSFORM_DOTP( v1, v2 ) ( ( ( llmull_s0( v1[ 0 ], v2[ 0 ] ) + llmull_s0( v1[ 1 ], v2[ 1 ] ) + llmull_s0( v1[ 2 ], v2[ 2 ] ) ) + ( 1 << 27 ) ) >> 28 )

qboolean b_cached_pv_fixed_dirty_v0 = false;
qboolean b_cached_pv_fixed_dirty_v1 = false;
fixed16_t rgf16_cached_pv0_fixed[ 3 ], rgf16_cached_pv1_fixed[ 3 ];


void R_EmitEdge (mvertex_t *pv0, mvertex_t *pv1)
{
	edge_t	*edge, *pcheck;
	int		u_check;
	float	*world;
#if PARTIALY_FIXED_TRANSFORM
	fixed16_t rgf16_world[ 3 ], rgf16_local[ 3 ], rgf16_transformed[ 3 ], f16_lzi0;
	fixed16_t f16_u0, f16_u1, f16_v0, f16_v1, f16_lzi1;
	long long f32_ptrans;
	float t0, t1, t2, t3;
	float lzi0;
	fixed16_t f16_u, f16_u_step;
#else
	vec3_t	local, transformed;
	float	u, u_step;
	float	scale, lzi0, u0, v0, tmp;
#endif
	int		v, v2, ceilv0;
	int		side;

	if (r_lastvertvalid)
	{
#if PARTIALY_FIXED_TRANSFORM
		f16_u0 = f16_r_u1;
		f16_v0 = f16_r_v1;
		f16_lzi0 = f16_r_lzi1;
		ceilv0 = r_ceilv1;
#else
		u0 = r_u1;
		v0 = r_v1;
		lzi0 = r_lzi1;
		ceilv0 = r_ceilv1;
#endif
	}
	else
	{
		world = &pv0->position[0];
	
	// transform and project
#if PARTIALY_FIXED_TRANSFORM
		/*FIXED_FLOATTOFIXED( world[ 0 ], rgf16_world[ 0 ], 16 );
		FIXED_FLOATTOFIXED( world[ 1 ], rgf16_world[ 1 ], 16 );
		FIXED_FLOATTOFIXED( world[ 2 ], rgf16_world[ 2 ], 16 );*/
		rgf16_world[ 0 ] = rgf16_cached_pv0_fixed[ 0 ];
		rgf16_world[ 1 ] = rgf16_cached_pv0_fixed[ 1 ];
		rgf16_world[ 2 ] = rgf16_cached_pv0_fixed[ 2 ];
		VectorSubtract (rgf16_world, rgf16_modelorg, rgf16_local);
		rgf16_transformed[ 0 ] = PARTIALY_FIXED_TRANSFORM_DOTP( rgf16_local, rgf16_vright );
		rgf16_transformed[ 1 ] = PARTIALY_FIXED_TRANSFORM_DOTP( rgf16_local, rgf16_vup );
		rgf16_transformed[ 2 ] = PARTIALY_FIXED_TRANSFORM_DOTP( rgf16_local, rgf16_vpn );

		if( rgf16_transformed[ 2 ] < 0x18000 )
		{
			rgf16_transformed[ 2 ] = 0x18000;
		}

		f16_lzi0 = udiv_s31_32( rgf16_transformed[ 2 ], 2 );

		f32_ptrans = ( ( ( long long )f16_xscale ) * rgf16_transformed[ 0 ] ) >> 16;
		if( rgf16_transformed[ 0 ] < 0 )
		{
			f32_ptrans = udiv_64_32( (-f32_ptrans) << 16, rgf16_transformed[ 2 ] );
			f16_u0 = -( int )f32_ptrans;
		}
		else
		{
			f32_ptrans = udiv_64_32( f32_ptrans << 16, rgf16_transformed[ 2 ] );
			f16_u0 = ( int )f32_ptrans;
		}
		f16_u0 += f16_xcenter;
		if (f16_u0 < r_refdef.f16_vrectx_adj)
		{
			f16_u0 = r_refdef.f16_vrectx_adj;
		}
		if (f16_u0 > r_refdef.f16_vrectright_adj)
		{
			f16_u0 = r_refdef.f16_vrectright_adj;
		}



		f32_ptrans = ( ( ( long long )f16_yscale ) * rgf16_transformed[ 1 ] ) >> 16;
		if( rgf16_transformed[ 1 ] < 0 )
		{
			f32_ptrans = udiv_64_32( (-f32_ptrans) << 16, rgf16_transformed[ 2 ] );
			f16_v0 = -( int )f32_ptrans;
		}
		else
		{
			f32_ptrans = udiv_64_32( f32_ptrans << 16, rgf16_transformed[ 2 ] );
			f16_v0 = ( int )f32_ptrans;
		}
		f16_v0 = f16_ycenter - f16_v0;
		if (f16_v0 < r_refdef.f16_vrecty_adj)
		{
			f16_v0 = r_refdef.f16_vrecty_adj;
		}
		if (f16_v0 > r_refdef.f16_vrectbottom_adj)
		{
			f16_v0 = r_refdef.f16_vrectbottom_adj;
		}

		
		FIXED_FIXEDTOFLOAT( f16_lzi0, lzi0, 16 );
		/*FIXED_FIXEDTOFLOAT( f16_u0, u0, 16 );
		FIXED_FIXEDTOFLOAT( f16_v0, v0, 16 ); */
		ceilv0 = ( f16_v0 + 0xffff ) >> 16;
		/*
		FIXED_FIXEDTOFLOAT( f16_lzi0, t0, 16 );
		FIXED_FIXEDTOFLOAT( f16_u0, t1, 16 );
		FIXED_FIXEDTOFLOAT( f16_v0, t2, 16 );
		t3 = ( f16_v0 + 0xffff ) >> 16;
		*/

#else
		VectorSubtract (world, modelorg, local);
		TransformVector (local, transformed);
	
		if (transformed[2] < (float)NEAR_CLIP)
			transformed[2] = (float)NEAR_CLIP;
	
		lzi0 = 1.0f / transformed[2];
	
	// FIXME: build x/yscale into transform?
		scale = xscale * lzi0;
		u0 = (xcenter + scale*transformed[0]);
		if (u0 < r_refdef.fvrectx_adj)
			u0 = r_refdef.fvrectx_adj;
		if (u0 > r_refdef.fvrectright_adj)
			u0 = r_refdef.fvrectright_adj;
	
		scale = yscale * lzi0;
		v0 = (ycenter - scale*transformed[1]);
		if (v0 < r_refdef.fvrecty_adj)
			v0 = r_refdef.fvrecty_adj;
		if (v0 > r_refdef.fvrectbottom_adj)
			v0 = r_refdef.fvrectbottom_adj;
	
		tmp = v0;
		test_float32_shift( &tmp, 16 );
		ceilv0 = (int) tmp;
		ceilv0 = ( ceilv0 + 0xffff ) >> 16;
		/*if( ceilv0 != ceilf( v0 ) )
		{
			ceilv0 = ceilv0;
		}*/
/*
		if( fabs( t0 - lzi0 ) / fabs( ( float )lzi0 ) > 0.001 )
		{
			t0 = t0;
		}
		if( fabs( t1 - u0 ) > 0.1 )
		{
			t1 = t1;
		}
		if( fabs( t2 - v0 ) > 0.1 )
		{
			t2 = t2;
		}
		if( fabs( t3 - ceilv0 ) > 0.1 )
		{
			t3 = t3;
		}
*/
#endif
	}

	world = &pv1->position[0];
#if PARTIALY_FIXED_TRANSFORM
	/*
	FIXED_FLOATTOFIXED( world[ 0 ], rgf16_world[ 0 ], 16 );
	FIXED_FLOATTOFIXED( world[ 1 ], rgf16_world[ 1 ], 16 );
	FIXED_FLOATTOFIXED( world[ 2 ], rgf16_world[ 2 ], 16 );
	*/
	rgf16_world[ 0 ] = rgf16_cached_pv1_fixed[ 0 ];
	rgf16_world[ 1 ] = rgf16_cached_pv1_fixed[ 1 ];
	rgf16_world[ 2 ] = rgf16_cached_pv1_fixed[ 2 ];

	VectorSubtract (rgf16_world, rgf16_modelorg, rgf16_local);
	rgf16_transformed[ 0 ] = PARTIALY_FIXED_TRANSFORM_DOTP( rgf16_local, rgf16_vright );
	rgf16_transformed[ 1 ] = PARTIALY_FIXED_TRANSFORM_DOTP( rgf16_local, rgf16_vup );
	rgf16_transformed[ 2 ] = PARTIALY_FIXED_TRANSFORM_DOTP( rgf16_local, rgf16_vpn );

	if( rgf16_transformed[ 2 ] < 0x18000 )
	{
		rgf16_transformed[ 2 ] = 0x18000;
	}

	f16_lzi1 = udiv_s31_32( rgf16_transformed[ 2 ], 2 );

	f32_ptrans = ( ( ( long long )f16_xscale ) * rgf16_transformed[ 0 ] ) >> 16;
	if( rgf16_transformed[ 0 ] < 0 )
	{
		f32_ptrans = udiv_64_32( (-f32_ptrans) << 16, rgf16_transformed[ 2 ] );
		f16_u1 = -( int )f32_ptrans;
	}
	else
	{
		f32_ptrans = udiv_64_32( f32_ptrans << 16, rgf16_transformed[ 2 ] );
		f16_u1 = ( int )f32_ptrans;
	}
	f16_u1 += f16_xcenter;
	if (f16_u1 < r_refdef.f16_vrectx_adj)
	{
		f16_u1 = r_refdef.f16_vrectx_adj;
	}
	if (f16_u1 > r_refdef.f16_vrectright_adj)
	{
		f16_u1 = r_refdef.f16_vrectright_adj;
	}

	f32_ptrans = ( ( ( long long )f16_yscale ) * rgf16_transformed[ 1 ] ) >> 16;
	if( rgf16_transformed[ 1 ] < 0 )
	{
		f32_ptrans = udiv_64_32( (-f32_ptrans) << 16, rgf16_transformed[ 2 ] );
		f16_v1 = -( int )f32_ptrans;
	}
	else
	{
		f32_ptrans = udiv_64_32( f32_ptrans << 16, rgf16_transformed[ 2 ] );
		f16_v1 = ( int )f32_ptrans;
	}
	f16_v1 = f16_ycenter - f16_v1;
	if (f16_v1 < r_refdef.f16_vrecty_adj)
	{
		f16_v1 = r_refdef.f16_vrecty_adj;
	}
	if (f16_v1 > r_refdef.f16_vrectbottom_adj)
	{
		f16_v1 = r_refdef.f16_vrectbottom_adj;
	}
	
	
	
	/*
	FIXED_FIXEDTOFLOAT( f16_u1, r_u1, 16 );
	FIXED_FIXEDTOFLOAT( f16_v1, r_v1, 16 );
	*/
	f16_r_u1 = f16_u1;
	f16_r_v1 = f16_v1;
	f16_r_lzi1 = f16_lzi1;

	if( f16_r_lzi1 > f16_lzi0 )
	{
		f16_lzi0 = f16_r_lzi1;
	}
	FIXED_FIXEDTOFLOAT( f16_lzi0, lzi0, 16 );

	if (lzi0 > r_nearzi)	// for mipmap finding
		r_nearzi = lzi0;

	if (r_nearzionly)
		return;

	r_emitted = 1;

	r_ceilv1 = ( f16_v1 + 0xffff ) >> 16;
#else
// transform and project
	VectorSubtract (world, modelorg, local);
	TransformVector (local, transformed);

	if (transformed[2] < (float)NEAR_CLIP)
		transformed[2] = (float)NEAR_CLIP;

	r_lzi1 = 1.0f / transformed[2];

	scale = xscale * r_lzi1;
	r_u1 = (xcenter + scale*transformed[0]);
	if (r_u1 < r_refdef.fvrectx_adj)
		r_u1 = r_refdef.fvrectx_adj;
	if (r_u1 > r_refdef.fvrectright_adj)
		r_u1 = r_refdef.fvrectright_adj;

	scale = yscale * r_lzi1;
	r_v1 = (ycenter - scale*transformed[1]);
	if (r_v1 < r_refdef.fvrecty_adj)
		r_v1 = r_refdef.fvrecty_adj;
	if (r_v1 > r_refdef.fvrectbottom_adj)
		r_v1 = r_refdef.fvrectbottom_adj;

	if (r_lzi1 > lzi0)
		lzi0 = r_lzi1;

	if (lzi0 > r_nearzi)	// for mipmap finding
		r_nearzi = lzi0;

// for right edges, all we want is the effect on 1/z
	if (r_nearzionly)
		return;

	r_emitted = 1;

	
	tmp = r_v1;
	test_float32_shift( &tmp, 16 );
	r_ceilv1 = (int) tmp;
	r_ceilv1 = ( r_ceilv1 + 0xffff ) >> 16;
#endif

// create the edge
	if (ceilv0 == r_ceilv1)
	{
	// we cache unclipped horizontal edges as fully clipped
		if (cacheoffset != 0x7FFFFFFF)
		{
			cacheoffset = FULLY_CLIPPED_CACHED |
					(r_framecount & FRAMECOUNT_MASK);
		}

		return;		// horizontal edge
	}

	side = ceilv0 > r_ceilv1;

	edge = edge_p++;

	edge->owner = r_pedge;

	edge->nearzi = lzi0;

	if (side == 0)
	{
	// trailing edge (go from p1 to p2)
		v = ceilv0;
		v2 = r_ceilv1 - 1;

		edge->surfs[0] = surface_p - surfaces;
		edge->surfs[1] = 0;

#if !PARTIALY_FIXED_TRANSFORM
		u_step = ((r_u1 - u0) / (r_v1 - v0));
		u = u0 + ((float)v - v0) * u_step;
#else
		{
			fixed16_t d;
			d = ( f16_r_u1 - f16_u0 );
			if( d < 0 )
			{
				f16_u_step = udiv_64_32( ((long long)-d)<<16, ( f16_r_v1 - f16_v0 ) );
				f16_u_step = -f16_u_step;
			}
			else
			{
				f16_u_step = udiv_64_32( ((long long)d)<<16, ( f16_r_v1 - f16_v0 ) );
			}
			f16_u = f16_u0 + llmull_s16(((v<<16)-f16_v0), f16_u_step );
		}
#endif
	}
	else
	{
	// leading edge (go from p2 to p1)
		v2 = ceilv0 - 1;
		v = r_ceilv1;

		edge->surfs[0] = 0;
		edge->surfs[1] = surface_p - surfaces;

#if !PARTIALY_FIXED_TRANSFORM
		u_step = ((u0 - r_u1) / (v0 - r_v1));
		u = r_u1 + ((float)v - r_v1) * u_step;
#else
		{
			fixed16_t d;
			d = ( f16_u0 - f16_r_u1 );
			if( d < 0 )
			{
				f16_u_step = udiv_64_32( ((long long)-d)<<16, ( f16_v0 - f16_r_v1 ) );
				f16_u_step = -f16_u_step;
			}
			else
			{
				f16_u_step = udiv_64_32( ((long long)d)<<16, ( f16_v0 - f16_r_v1 ) );
			}
			f16_u = f16_u1 + llmull_s16(((v<<16)-f16_v1), f16_u_step );
		}
#endif
	}

#if !PARTIALY_FIXED_TRANSFORM
	edge->u_step = u_step*0x100000;
	edge->u = u*0x100000 + 0xFFFFF;
#else
	edge->u_step = f16_u_step << 4;
	edge->u = ( f16_u << 4 ) + 0xFFFFF;
#endif

	if( edge->u_step == 0x80000000 && v2 != v )
	{
		edge = edge;
	}
	if( edge->u < 0 )
	{
		edge = edge;
	}

// we need to do this to avoid stepping off the edges if a very nearly
// horizontal edge is less than epsilon above a scan, and numeric error causes
// it to incorrectly extend to the scan, and the extension of the line goes off
// the edge of the screen
// FIXME: is this actually needed?
	if (edge->u < r_refdef.vrect_x_adj_shift20)
		edge->u = r_refdef.vrect_x_adj_shift20;
	if (edge->u > r_refdef.vrectright_adj_shift20)
		edge->u = r_refdef.vrectright_adj_shift20;

//
// sort the edge in normally
//
	u_check = edge->u;
	if (edge->surfs[0])
		u_check++;	// sort trailers after leaders

	if (!newedges[v] || newedges[v]->u >= u_check)
	{
		edge->next = newedges[v];
		newedges[v] = edge;
	}
	else
	{
		pcheck = newedges[v];
		while (pcheck->next && pcheck->next->u < u_check)
			pcheck = pcheck->next;
		edge->next = pcheck->next;
		pcheck->next = edge;
	}

	edge->nextremove = removeedges[v2];
	removeedges[v2] = edge;
}


/*
================
R_ClipEdge
================
*/



#define RECURSIVE_WORLD_NODE_FIXED_DOTP( v1, v2 ) ( ( ( llmull_s0( v1[ 0 ], v2[ 0 ] ) + llmull_s0( v1[ 1 ], v2[ 1 ] ) + llmull_s0( v1[ 2 ], v2[ 2 ] ) ) + ( 1 << 27 ) ) >> 28 )

void test_float32_shift( float *f, int shift )
{
	int i_float;
	int i_e;

	i_float = *(int*)f;
	i_e = ( i_float >> 23 ) & 0xff;
	i_e += shift;
	if( i_e > 0xfe )
	{
		i_e = 0xfe;
	}
	else if( i_e < 0x1 )
	{
		i_e = 0x1;
	}
	i_float = ( i_float & 0x807fffff ) | ( i_e << 23 );
	*f = *( float * )&i_float;
}

void R_ClipEdge (mvertex_t *pv0, mvertex_t *pv1, clipplane_t *clip)
{
	int i;
	fixed16_t   f16_d0, f16_d1;
	float		d0, d1, f;
	mvertex_t	clipvert;
#if PARTIALY_FIXED_TRANSFORM
	fixed32_t f32_f, tmp1, tmp2;
#endif

#if PARTIALY_FIXED_TRANSFORM
	if( b_cached_pv_fixed_dirty_v0 )
	{
		for( i = 0; i < 3; i++ )
		{
			float f0;
			f0 = pv0->position[ i ];
			test_float32_shift( &f0, 16 );
			rgf16_cached_pv0_fixed[ i ] = f0;
		}
		b_cached_pv_fixed_dirty_v0 = false;
	}
	if( b_cached_pv_fixed_dirty_v1 )
	{
		for( i = 0; i < 3; i++ )
		{
			float f1;
			f1 = pv1->position[ i ];
			test_float32_shift( &f1, 16 );
			rgf16_cached_pv1_fixed[ i ] = f1;
		}
		b_cached_pv_fixed_dirty_v1 = false;
	}
#endif
	if (clip)
	{
#if RECURSIVE_WORLD_NODE_CLIP_FIXED && !PARTIALY_FIXED_TRANSFORM
		if( b_cached_pv_fixed_dirty_v0 )
		{
			for( i = 0; i < 3; i++ )
			{
				float f0;
				f0 = pv0->position[ i ];
				test_float32_shift( &f0, 16 );
				rgf16_cached_pv0_fixed[ i ] = f0;
			}
			b_cached_pv_fixed_dirty_v0 = false;
		}
		if( b_cached_pv_fixed_dirty_v1 )
		{
			for( i = 0; i < 3; i++ )
			{
				float f1;
				f1 = pv1->position[ i ];
				test_float32_shift( &f1, 16 );
				rgf16_cached_pv1_fixed[ i ] = f1;
			}
			b_cached_pv_fixed_dirty_v1 = false;
		}

#endif
		do
		{
#if RECURSIVE_WORLD_NODE_CLIP_FIXED
			{
				float td0, td1;
				clipplane_fixed_t *ps_fclipplane;
				ps_fclipplane = &view_clipplanes_fixed[ clip->idx ];
				f16_d0 = RECURSIVE_WORLD_NODE_FIXED_DOTP( ps_fclipplane->normal, rgf16_cached_pv0_fixed );
				f16_d1 = RECURSIVE_WORLD_NODE_FIXED_DOTP( ps_fclipplane->normal, rgf16_cached_pv1_fixed );
				f16_d0 = f16_d0 - ps_fclipplane->f16_dist;
				f16_d1 = f16_d1 - ps_fclipplane->f16_dist;
			}

#else
			d0 = DotProduct (pv0->position, clip->normal) - clip->dist;
			d1 = DotProduct (pv1->position, clip->normal) - clip->dist;
#endif

#if !RECURSIVE_WORLD_NODE_CLIP_FIXED
			if (d0 >= 0)
#else
			if ( ( (*(int*)&f16_d0) & 0x80000000 ) == 0 )
#endif
			{
			// point 0 is unclipped
#if !RECURSIVE_WORLD_NODE_CLIP_FIXED
				if (d1 >= 0)
#else
				if ( ( (*(int*)&f16_d1) & 0x80000000 ) == 0 )
#endif
				{
				// both points are unclipped
					continue;
				}

			// only point 1 is clipped

			// we don't cache clipped edges
				cacheoffset = 0x7FFFFFFF;

#if	RECURSIVE_WORLD_NODE_CLIP_FIXED
				/* just do it at full precision */
				f32_f = udiv_64_32( ( long long )f16_d0 << 32, f16_d0 - f16_d1 );
				rgf16_cached_pv1_fixed[ 0 ] = rgf16_cached_pv0_fixed[ 0 ] + ( mul_64_32_r64( f32_f, ( rgf16_cached_pv1_fixed[ 0 ] - rgf16_cached_pv0_fixed[ 0 ] ) ) >> 32 );
				rgf16_cached_pv1_fixed[ 1 ] = rgf16_cached_pv0_fixed[ 1 ] + ( mul_64_32_r64( f32_f, ( rgf16_cached_pv1_fixed[ 1 ] - rgf16_cached_pv0_fixed[ 1 ] ) ) >> 32 );
				rgf16_cached_pv1_fixed[ 2 ] = rgf16_cached_pv0_fixed[ 2 ] + ( mul_64_32_r64( f32_f, ( rgf16_cached_pv1_fixed[ 2 ] - rgf16_cached_pv0_fixed[ 2 ] ) ) >> 32 );
#else
				b_cached_pv_fixed_dirty_v1 = true;
				f = d0 / (d0 - d1);
				clipvert.position[0] = pv0->position[0] +
						f * (pv1->position[0] - pv0->position[0]);
				clipvert.position[1] = pv0->position[1] +
						f * (pv1->position[1] - pv0->position[1]);
				clipvert.position[2] = pv0->position[2] +
						f * (pv1->position[2] - pv0->position[2]);
#endif

				if (clip->leftedge)
				{
					r_leftclipped = true;
					/*r_leftexit = clipvert;*/
					FIXED_FIXEDTOFLOAT( rgf16_cached_pv1_fixed[ 0 ], r_leftexit.position[ 0 ], 16 );
					FIXED_FIXEDTOFLOAT( rgf16_cached_pv1_fixed[ 1 ], r_leftexit.position[ 1 ], 16 );
					FIXED_FIXEDTOFLOAT( rgf16_cached_pv1_fixed[ 2 ], r_leftexit.position[ 2 ], 16 );
				}
				else if (clip->rightedge)
				{
					r_rightclipped = true;
					/*r_rightexit = clipvert;*/
					FIXED_FIXEDTOFLOAT( rgf16_cached_pv1_fixed[ 0 ], r_rightexit.position[ 0 ], 16 );
					FIXED_FIXEDTOFLOAT( rgf16_cached_pv1_fixed[ 1 ], r_rightexit.position[ 1 ], 16 );
					FIXED_FIXEDTOFLOAT( rgf16_cached_pv1_fixed[ 2 ], r_rightexit.position[ 2 ], 16 );
				}

				R_ClipEdge (pv0, &clipvert, clip->next);
				return;
			}
			else
			{
			// point 0 is clipped
#if !RECURSIVE_WORLD_NODE_CLIP_FIXED
				if (d1 < 0 )
#else
				if( (*(int*)&f16_d1) & 0x80000000 )
#endif
				{
				// both points are clipped
				// we do cache fully clipped edges
					if (!r_leftclipped)
						cacheoffset = FULLY_CLIPPED_CACHED |
								(r_framecount & FRAMECOUNT_MASK);
					return;
				}

			// only point 0 is clipped
				r_lastvertvalid = false;

			// we don't cache partially clipped edges
				cacheoffset = 0x7FFFFFFF;

#if	RECURSIVE_WORLD_NODE_CLIP_FIXED
				f32_f = udiv_64_32( ( long long )f16_d1 << 32, f16_d1 - f16_d0 );
				rgf16_cached_pv0_fixed[ 0 ] = rgf16_cached_pv1_fixed[ 0 ] + ( mul_64_32_r64( f32_f, ( rgf16_cached_pv0_fixed[ 0 ] - rgf16_cached_pv1_fixed[ 0 ] ) ) >> 32 );
				rgf16_cached_pv0_fixed[ 1 ] = rgf16_cached_pv1_fixed[ 1 ] + ( mul_64_32_r64( f32_f, ( rgf16_cached_pv0_fixed[ 1 ] - rgf16_cached_pv1_fixed[ 1 ] ) ) >> 32 );
				rgf16_cached_pv0_fixed[ 2 ] = rgf16_cached_pv1_fixed[ 2 ] + ( mul_64_32_r64( f32_f, ( rgf16_cached_pv0_fixed[ 2 ] - rgf16_cached_pv1_fixed[ 2 ] ) ) >> 32 );
#else
				b_cached_pv_fixed_dirty_v0 = true;
				f = d0 / (d0 - d1);
				clipvert.position[0] = pv0->position[0] +
						f * (pv1->position[0] - pv0->position[0]);
				clipvert.position[1] = pv0->position[1] +
						f * (pv1->position[1] - pv0->position[1]);
				clipvert.position[2] = pv0->position[2] +
						f * (pv1->position[2] - pv0->position[2]);
#endif

				if (clip->leftedge)
				{
					r_leftclipped = true;
					/*r_leftenter = clipvert;*/
					FIXED_FIXEDTOFLOAT( rgf16_cached_pv0_fixed[ 0 ], r_leftenter.position[ 0 ], 16 );
					FIXED_FIXEDTOFLOAT( rgf16_cached_pv0_fixed[ 1 ], r_leftenter.position[ 1 ], 16 );
					FIXED_FIXEDTOFLOAT( rgf16_cached_pv0_fixed[ 2 ], r_leftenter.position[ 2 ], 16 );
				}
				else if (clip->rightedge)
				{
					r_rightclipped = true;
/*					r_rightenter = clipvert;*/
					FIXED_FIXEDTOFLOAT( rgf16_cached_pv0_fixed[ 0 ], r_rightenter.position[ 0 ], 16 );
					FIXED_FIXEDTOFLOAT( rgf16_cached_pv0_fixed[ 1 ], r_rightenter.position[ 1 ], 16 );
					FIXED_FIXEDTOFLOAT( rgf16_cached_pv0_fixed[ 2 ], r_rightenter.position[ 2 ], 16 );
				}

				R_ClipEdge (&clipvert, pv1, clip->next);
				return;
			}
		} while ((clip = clip->next) != NULL);
	}

// add the edge
	{
		R_EmitEdge (pv0, pv1);
	}
}

#endif	// !id386


/*
================
R_EmitCachedEdge
================
*/
void R_EmitCachedEdge (void)
{
	edge_t		*pedge_t;

	pedge_t = (edge_t *)((unsigned long)r_edges + r_pedge->cachededgeoffset);

	if (!pedge_t->surfs[0])
		pedge_t->surfs[0] = surface_p - surfaces;
	else
		pedge_t->surfs[1] = surface_p - surfaces;

	if (pedge_t->nearzi > r_nearzi)	// for mipmap finding
		r_nearzi = pedge_t->nearzi;

	r_emitted = 1;
}


/*
================
R_RenderFace
================
*/

#define NSPIRE_CHEAP_ZICACHE 1

#if NSPIRE_CHEAP_ZICACHE
struct {
	int i_valid_framecount;
	mplane_t *ps_pplane;
	float f_d_zistepu;
	float f_d_zistepv;
	float f_d_ziorigin;
} s_r_render_face_cached_plane = { 0, 0, 0, 0, 0 };
#endif

void R_RenderFace (msurface_t *fa, int clipflags)
{
	int			i, lindex;
	unsigned	mask;
	mplane_t	*pplane;
	float		distinv;
	vec3_t		p_normal;
	medge_t		*pedges, tedge;
	clipplane_t	*pclip;

// skip out if no more surfs
	if ((surface_p) >= surf_max)
	{
		r_outofsurfaces++;
		return;
	}

// ditto if not enough edges left, or switch to auxedges if possible
	if ((edge_p + fa->numedges + 4) >= edge_max)
	{
		r_outofedges += fa->numedges;
		return;
	}

	c_faceclip++;

// set up clip planes
	pclip = NULL;

	for (i=3, mask = 0x08 ; i>=0 ; i--, mask >>= 1)
	{
		if (clipflags & mask)
		{
			view_clipplanes[i].next = pclip;
			pclip = &view_clipplanes[i];
		}
	}

// push the edges through
	r_emitted = 0;
	r_nearzi = 0;
	r_nearzionly = false;
	makeleftedge = makerightedge = false;
	pedges = currententity->model->edges;
	r_lastvertvalid = false;

	for (i=0 ; i<fa->numedges ; i++)
	{
		lindex = currententity->model->surfedges[fa->firstedge + i];

#if RECURSIVE_WORLD_NODE_CLIP_FIXED
		b_cached_pv_fixed_dirty_v0 = true;
		b_cached_pv_fixed_dirty_v1 = true;
#endif
		if (lindex > 0)
		{
			r_pedge = &pedges[lindex];

		// if the edge is cached, we can just reuse the edge
			if (!insubmodel)
			{
				if (r_pedge->cachededgeoffset & FULLY_CLIPPED_CACHED)
				{
					if ((r_pedge->cachededgeoffset & FRAMECOUNT_MASK) ==
						r_framecount)
					{
						r_lastvertvalid = false;
						continue;
					}
				}
				else
				{
					if ((((unsigned long)edge_p - (unsigned long)r_edges) >
						 r_pedge->cachededgeoffset) &&
						(((edge_t *)((unsigned long)r_edges +
						 r_pedge->cachededgeoffset))->owner == r_pedge))
					{
						R_EmitCachedEdge ();
						r_lastvertvalid = false;
						continue;
					}
				}
			}

		// assume it's cacheable
			cacheoffset = (byte *)edge_p - (byte *)r_edges;
			r_leftclipped = r_rightclipped = false;
			R_ClipEdge (&r_pcurrentvertbase[r_pedge->v[0]],
						&r_pcurrentvertbase[r_pedge->v[1]],
						pclip);
			r_pedge->cachededgeoffset = cacheoffset;

			if (r_leftclipped)
				makeleftedge = true;
			if (r_rightclipped)
				makerightedge = true;
			r_lastvertvalid = true;
		}
		else
		{
			lindex = -lindex;
			r_pedge = &pedges[lindex];
		// if the edge is cached, we can just reuse the edge
			if (!insubmodel)
			{
				if (r_pedge->cachededgeoffset & FULLY_CLIPPED_CACHED)
				{
					if ((r_pedge->cachededgeoffset & FRAMECOUNT_MASK) ==
						r_framecount)
					{
						r_lastvertvalid = false;
						continue;
					}
				}
				else
				{
				// it's cached if the cached edge is valid and is owned
				// by this medge_t
					if ((((unsigned long)edge_p - (unsigned long)r_edges) >
						 r_pedge->cachededgeoffset) &&
						(((edge_t *)((unsigned long)r_edges +
						 r_pedge->cachededgeoffset))->owner == r_pedge))
					{
						R_EmitCachedEdge ();
						r_lastvertvalid = false;
						continue;
					}
				}
			}

		// assume it's cacheable
			cacheoffset = (byte *)edge_p - (byte *)r_edges;
			r_leftclipped = r_rightclipped = false;
			R_ClipEdge (&r_pcurrentvertbase[r_pedge->v[1]],
						&r_pcurrentvertbase[r_pedge->v[0]],
						pclip);
			r_pedge->cachededgeoffset = cacheoffset;

			if (r_leftclipped)
				makeleftedge = true;
			if (r_rightclipped)
				makerightedge = true;
			r_lastvertvalid = true;
		}
	}

// if there was a clip off the left edge, add that edge too
// FIXME: faster to do in screen space?
// FIXME: share clipped edges?

		if( lindex == 9149 )
		{
			lindex = lindex;
		}

	if (makeleftedge)
	{
#if RECURSIVE_WORLD_NODE_CLIP_FIXED
		b_cached_pv_fixed_dirty_v0 = b_cached_pv_fixed_dirty_v1 = true;
#endif
		r_pedge = &tedge;
		r_lastvertvalid = false;
		R_ClipEdge (&r_leftexit, &r_leftenter, pclip->next);
	}

// if there was a clip off the right edge, get the right r_nearzi
	if (makerightedge)
	{
#if RECURSIVE_WORLD_NODE_CLIP_FIXED
		b_cached_pv_fixed_dirty_v0 = b_cached_pv_fixed_dirty_v1 = true;
#endif
		r_pedge = &tedge;
		r_lastvertvalid = false;
		r_nearzionly = true;
		R_ClipEdge (&r_rightexit, &r_rightenter, view_clipplanes[1].next);
	}

// if no edges made it out, return without posting the surface
	if (!r_emitted)
		return;

	r_polycount++;

	surface_p->data = (void *)fa;
	surface_p->nearzi = r_nearzi;
	surface_p->flags = fa->flags;
	surface_p->insubmodel = insubmodel;
	surface_p->spanstate = 0;
	surface_p->entity = currententity;
	surface_p->key = r_currentkey++;
	surface_p->spans = NULL;

	pplane = fa->plane;
#if NSPIRE_CHEAP_ZICACHE
	if( s_r_render_face_cached_plane.i_valid_framecount == r_framecount && s_r_render_face_cached_plane.ps_pplane == pplane )
	{
		surface_p->d_zistepu = s_r_render_face_cached_plane.f_d_zistepu;
		surface_p->d_zistepv = s_r_render_face_cached_plane.f_d_zistepv;
		surface_p->d_ziorigin = s_r_render_face_cached_plane.f_d_ziorigin;
	}
	else
	{
#endif
	// FIXME: cache this?
		TransformVector (pplane->normal, p_normal);
	// FIXME: cache this?
		distinv = 1.0f / (pplane->dist - DotProduct (modelorg, pplane->normal));

		surface_p->d_zistepu = p_normal[0] * xscaleinv * distinv;
		surface_p->d_zistepv = -p_normal[1] * yscaleinv * distinv;
		surface_p->d_ziorigin = p_normal[2] * distinv -
				xcenter * surface_p->d_zistepu -
				ycenter * surface_p->d_zistepv;

		s_r_render_face_cached_plane.ps_pplane = pplane;
		s_r_render_face_cached_plane.f_d_zistepu = surface_p->d_zistepu;
		s_r_render_face_cached_plane.f_d_zistepv = surface_p->d_zistepv;
		s_r_render_face_cached_plane.f_d_ziorigin = surface_p->d_ziorigin;
		s_r_render_face_cached_plane.i_valid_framecount = r_framecount;
#if NSPIRE_CHEAP_ZICACHE
	}
#endif

//JDC	VectorCopy (r_worldmodelorg, surface_p->modelorg);
	surface_p++;
}


/*
================
R_RenderBmodelFace
================
*/
void R_RenderBmodelFace (bedge_t *pedges, msurface_t *psurf)
{
	int			i;
	unsigned	mask;
	mplane_t	*pplane;
	float		distinv;
	vec3_t		p_normal;
	medge_t		tedge;
	clipplane_t	*pclip;

// skip out if no more surfs
	if (surface_p >= surf_max)
	{
		r_outofsurfaces++;
		return;
	}

// ditto if not enough edges left, or switch to auxedges if possible
	if ((edge_p + psurf->numedges + 4) >= edge_max)
	{
		r_outofedges += psurf->numedges;
		return;
	}

	c_faceclip++;

// this is a dummy to give the caching mechanism someplace to write to
	r_pedge = &tedge;

// set up clip planes
	pclip = NULL;

	for (i=3, mask = 0x08 ; i>=0 ; i--, mask >>= 1)
	{
		if (r_clipflags & mask)
		{
			view_clipplanes[i].next = pclip;
			pclip = &view_clipplanes[i];
		}
	}

// push the edges through
	r_emitted = 0;
	r_nearzi = 0;
	r_nearzionly = false;
	makeleftedge = makerightedge = false;
// FIXME: keep clipped bmodel edges in clockwise order so last vertex caching
// can be used?
	r_lastvertvalid = false;

	for ( ; pedges ; pedges = pedges->pnext)
	{
#if RECURSIVE_WORLD_NODE_CLIP_FIXED
		b_cached_pv_fixed_dirty_v0 = true;
		b_cached_pv_fixed_dirty_v1 = true;
#endif

		r_leftclipped = r_rightclipped = false;
		R_ClipEdge (pedges->v[0], pedges->v[1], pclip);

		if (r_leftclipped)
			makeleftedge = true;
		if (r_rightclipped)
			makerightedge = true;
	}

// if there was a clip off the left edge, add that edge too
// FIXME: faster to do in screen space?
// FIXME: share clipped edges?
	if (makeleftedge)
	{
#if RECURSIVE_WORLD_NODE_CLIP_FIXED
		b_cached_pv_fixed_dirty_v0 = true;
		b_cached_pv_fixed_dirty_v1 = true;
#endif

		r_pedge = &tedge;
		R_ClipEdge (&r_leftexit, &r_leftenter, pclip->next);
	}

// if there was a clip off the right edge, get the right r_nearzi
	if (makerightedge)
	{
#if RECURSIVE_WORLD_NODE_CLIP_FIXED
		b_cached_pv_fixed_dirty_v0 = true;
		b_cached_pv_fixed_dirty_v1 = true;
#endif

		r_pedge = &tedge;
		r_nearzionly = true;
		R_ClipEdge (&r_rightexit, &r_rightenter, view_clipplanes[1].next);
	}

// if no edges made it out, return without posting the surface
	if (!r_emitted)
		return;

	r_polycount++;

	surface_p->data = (void *)psurf;
	surface_p->nearzi = r_nearzi;
	surface_p->flags = psurf->flags;
	surface_p->insubmodel = true;
	surface_p->spanstate = 0;
	surface_p->entity = currententity;
	surface_p->key = r_currentbkey;
	surface_p->spans = NULL;

	pplane = psurf->plane;
// FIXME: cache this?
	TransformVector (pplane->normal, p_normal);
// FIXME: cache this?
	distinv = 1.0f / (pplane->dist - DotProduct (modelorg, pplane->normal));

	surface_p->d_zistepu = p_normal[0] * xscaleinv * distinv;
	surface_p->d_zistepv = -p_normal[1] * yscaleinv * distinv;
	surface_p->d_ziorigin = p_normal[2] * distinv -
			xcenter * surface_p->d_zistepu -
			ycenter * surface_p->d_zistepv;

//JDC	VectorCopy (r_worldmodelorg, surface_p->modelorg);
	surface_p++;
}


/*
================
R_RenderPoly
================
*/
void R_RenderPoly (msurface_t *fa, int clipflags)
{
	int			i, lindex, lnumverts, s_axis, t_axis;
	float		dist, lastdist, lzi, scale, u, v, frac;
	unsigned	mask;
	vec3_t		local, transformed;
	clipplane_t	*pclip;
	medge_t		*pedges;
	mplane_t	*pplane;
	mvertex_t	verts[2][100];	//FIXME: do real number
	polyvert_t	pverts[100];	//FIXME: do real number, safely
	int			vertpage, newverts, newpage, lastvert;
	qboolean	visible;

// FIXME: clean this up and make it faster
// FIXME: guard against running out of vertices

	s_axis = t_axis = 0;	// keep compiler happy

// set up clip planes
	pclip = NULL;

	for (i=3, mask = 0x08 ; i>=0 ; i--, mask >>= 1)
	{
		if (clipflags & mask)
		{
			view_clipplanes[i].next = pclip;
			pclip = &view_clipplanes[i];
		}
	}

// reconstruct the polygon
// FIXME: these should be precalculated and loaded off disk
	pedges = currententity->model->edges;
	lnumverts = fa->numedges;
	vertpage = 0;

	for (i=0 ; i<lnumverts ; i++)
	{
		lindex = currententity->model->surfedges[fa->firstedge + i];

		if (lindex > 0)
		{
			r_pedge = &pedges[lindex];
			verts[0][i] = r_pcurrentvertbase[r_pedge->v[0]];
		}
		else
		{
			r_pedge = &pedges[-lindex];
			verts[0][i] = r_pcurrentvertbase[r_pedge->v[1]];
		}
	}

// clip the polygon, done if not visible
	while (pclip)
	{
		lastvert = lnumverts - 1;
		lastdist = DotProduct (verts[vertpage][lastvert].position,
							   pclip->normal) - pclip->dist;

		visible = false;
		newverts = 0;
		newpage = vertpage ^ 1;

		for (i=0 ; i<lnumverts ; i++)
		{
			dist = DotProduct (verts[vertpage][i].position, pclip->normal) -
					pclip->dist;

			if ((lastdist > 0) != (dist > 0))
			{
				frac = dist / (dist - lastdist);
				verts[newpage][newverts].position[0] =
						verts[vertpage][i].position[0] +
						((verts[vertpage][lastvert].position[0] -
						  verts[vertpage][i].position[0]) * frac);
				verts[newpage][newverts].position[1] =
						verts[vertpage][i].position[1] +
						((verts[vertpage][lastvert].position[1] -
						  verts[vertpage][i].position[1]) * frac);
				verts[newpage][newverts].position[2] =
						verts[vertpage][i].position[2] +
						((verts[vertpage][lastvert].position[2] -
						  verts[vertpage][i].position[2]) * frac);
				newverts++;
			}

			if (dist >= 0)
			{
				verts[newpage][newverts] = verts[vertpage][i];
				newverts++;
				visible = true;
			}

			lastvert = i;
			lastdist = dist;
		}

		if (!visible || (newverts < 3))
			return;

		lnumverts = newverts;
		vertpage ^= 1;
		pclip = pclip->next;
	}

// transform and project, remembering the z values at the vertices and
// r_nearzi, and extract the s and t coordinates at the vertices
	pplane = fa->plane;
	switch (pplane->type)
	{
	case PLANE_X:
	case PLANE_ANYX:
		s_axis = 1;
		t_axis = 2;
		break;
	case PLANE_Y:
	case PLANE_ANYY:
		s_axis = 0;
		t_axis = 2;
		break;
	case PLANE_Z:
	case PLANE_ANYZ:
		s_axis = 0;
		t_axis = 1;
		break;
	}

	r_nearzi = 0;

	for (i=0 ; i<lnumverts ; i++)
	{
	// transform and project
		VectorSubtract (verts[vertpage][i].position, modelorg, local);
		TransformVector (local, transformed);

		if (transformed[2] < (float)NEAR_CLIP)
			transformed[2] = (float)NEAR_CLIP;

		lzi = 1.0f / transformed[2];

		if (lzi > r_nearzi)	// for mipmap finding
			r_nearzi = lzi;

	// FIXME: build x/yscale into transform?
		scale = xscale * lzi;
		u = (xcenter + scale*transformed[0]);
		if (u < r_refdef.fvrectx_adj)
			u = r_refdef.fvrectx_adj;
		if (u > r_refdef.fvrectright_adj)
			u = r_refdef.fvrectright_adj;

		scale = yscale * lzi;
		v = (ycenter - scale*transformed[1]);
		if (v < r_refdef.fvrecty_adj)
			v = r_refdef.fvrecty_adj;
		if (v > r_refdef.fvrectbottom_adj)
			v = r_refdef.fvrectbottom_adj;

		pverts[i].u = u;
		pverts[i].v = v;
		pverts[i].zi = lzi;
		pverts[i].s = verts[vertpage][i].position[s_axis];
		pverts[i].t = verts[vertpage][i].position[t_axis];
	}

// build the polygon descriptor, including fa, r_nearzi, and u, v, s, t, and z
// for each vertex
	r_polydesc.numverts = lnumverts;
	r_polydesc.nearzi = r_nearzi;
	r_polydesc.pcurrentface = fa;
	r_polydesc.pverts = pverts;

// draw the polygon
	D_DrawPoly ();
}


/*
================
R_ZDrawSubmodelPolys
================
*/
void R_ZDrawSubmodelPolys (model_t *pmodel)
{
	int			i, numsurfaces;
	msurface_t	*psurf;
	float		dot;
	mplane_t	*pplane;

	psurf = &pmodel->surfaces[pmodel->firstmodelsurface];
	numsurfaces = pmodel->nummodelsurfaces;

	for (i=0 ; i<numsurfaces ; i++, psurf++)
	{
	// find which side of the node we are on
		pplane = psurf->plane;

		dot = DotProduct (modelorg, pplane->normal) - pplane->dist;

	// draw the polygon
		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
			(!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
		{
		// FIXME: use bounding-box-based frustum clipping info?
			R_RenderPoly (psurf, 15);
		}
	}
}

