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
#include "r_local.h"

#define MAX_PARTICLES			2048	// default max # of particles at one
										//  time
#define ABSOLUTE_MIN_PARTICLES	512		// no fewer than this no matter what's
										//  on the command line

int		ramp1[8] = {0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61};
int		ramp2[8] = {0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x66};
int		ramp3[8] = {0x6d, 0x6b, 6, 5, 4, 3};

particle_t	*active_particles, *free_particles;

particle_t	*particles;
int			r_numparticles;

vec3_t			r_pright, r_pup, r_ppn;
fixed16_t       r_pright_fixed[ 3 ], r_pup_fixed[ 3 ], r_ppn_fixed[ 3 ], r_porigin_fixed[3];
fixed16_t       pxcenter_fixed, pycenter_fixed;

#define FIXED16_DIE( f ) ((int)((f) * ((float)0x200)))
#define FIXED16_ORIGIN( f ) ((int)((f) * ((float)0x100)))
#define FIXED16_VELOCITY( f ) ((int)((f) * ((float)0x100)))
#define FIXED16_VELOCITYF( i ) ((int)((i)<<8))
#define FIXED16_RAMPF( i ) ((int)((i)<<8))

#define FIXED16_ADDVELOCITY( i, time ) ( ( (i)*(time) ) >> 9 );
#define FIXED16_ADDVELOCITYV( i, val ) ( ( (i)*(val) ) >> 8 );

/*
===============
R_InitParticles
===============
*/
void R_InitParticles (void)
{
	int		i;

	i = COM_CheckParm ("-particles");

	if (i)
	{
		r_numparticles = (int)(Q_atoi(com_argv[i+1]));
		if (r_numparticles < ABSOLUTE_MIN_PARTICLES)
			r_numparticles = ABSOLUTE_MIN_PARTICLES;
	}
	else
	{
		r_numparticles = MAX_PARTICLES;
	}

	particles = (particle_t *)
			Hunk_AllocName (r_numparticles * sizeof(particle_t), "particles");
}

#ifdef QUAKE2
void R_DarkFieldParticles (entity_t *ent)
{
	int			i, j, k;
	particle_t	*p;
	float		vel;
	vec3_t		dir;
	vec3_t		org;

	org[0] = ent->origin[0];
	org[1] = ent->origin[1];
	org[2] = ent->origin[2];
	for (i=-16 ; i<16 ; i+=8)
		for (j=-16 ; j<16 ; j+=8)
			for (k=0 ; k<32 ; k+=8)
			{
				if (!free_particles)
					return;
				p = free_particles;
				free_particles = p->next;
				p->next = active_particles;
				active_particles = p;
		
				p->die = cl.time + 0.2 + (rand()&7) * 0.02;
				p->color = 150 + rand()%6;
				p->type = pt_slowgrav;
				
				dir[0] = j*8;
				dir[1] = i*8;
				dir[2] = k*8;
	
				p->org[0] = org[0] + i + (rand()&3);
				p->org[1] = org[1] + j + (rand()&3);
				p->org[2] = org[2] + k + (rand()&3);
	
				VectorNormalize (dir);						
				vel = 50 + (rand()&63);
				VectorScale (dir, vel, p->vel);
			}
}
#endif


/*
===============
R_EntityParticles
===============
*/

#define NUMVERTEXNORMALS	162
extern	float	r_avertexnormals[NUMVERTEXNORMALS][3];
vec3_t	avelocities[NUMVERTEXNORMALS];
float	beamlength = 16;
vec3_t	avelocity = {23, 7, 3};
float	partstep = 0.01;
float	timescale = 0.01;

void R_EntityParticles (entity_t *ent)
{
	int			count;
	int			i;
	particle_t	*p;
	float		angle;
	float		sr, sp, sy, cr, cp, cy;
	vec3_t		forward;
	float		dist;
	
	dist = 64;
	count = 50;

if (!avelocities[0][0])
{
for (i=0 ; i<NUMVERTEXNORMALS*3 ; i++)
avelocities[0][i] = (rand()&255) * 0.01;
}


	for (i=0 ; i<NUMVERTEXNORMALS ; i++)
	{
		angle = cl.time * avelocities[i][0];
		sy = sin(angle);
		cy = cos(angle);
		angle = cl.time * avelocities[i][1];
		sp = sin(angle);
		cp = cos(angle);
		angle = cl.time * avelocities[i][2];
		sr = sin(angle);
		cr = cos(angle);
	
		forward[0] = cp*cy;
		forward[1] = cp*sy;
		forward[2] = -sp;

		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

#if !FIXED_POINT_PARTICLES
		p->die = cl.time + 0.01;
		p->color = 0x6f;
		p->type = pt_explode;
		
		p->org[0] = ent->origin[0] + r_avertexnormals[i][0]*dist + forward[0]*beamlength;			
		p->org[1] = ent->origin[1] + r_avertexnormals[i][1]*dist + forward[1]*beamlength;			
		p->org[2] = ent->origin[2] + r_avertexnormals[i][2]*dist + forward[2]*beamlength;			
#else
		p->f16_die = cl.time + 0.01;
		p->f16_color = 0x6f;
		p->type = pt_explode;
		
		p->rgf16_org[0] = FIXED16_ORIGIN( ent->origin[0] + r_avertexnormals[i][0]*dist + forward[0]*beamlength );
		p->rgf16_org[1] = FIXED16_ORIGIN( ent->origin[1] + r_avertexnormals[i][1]*dist + forward[1]*beamlength );
		p->rgf16_org[2] = FIXED16_ORIGIN( ent->origin[2] + r_avertexnormals[i][2]*dist + forward[2]*beamlength );
#endif
	}
}


/*
===============
R_ClearParticles
===============
*/
void R_ClearParticles (void)
{
	int		i;
	
	free_particles = &particles[0];
	active_particles = NULL;

	for (i=0 ;i<r_numparticles ; i++)
		particles[i].next = &particles[i+1];
	particles[r_numparticles-1].next = NULL;
}


void R_ReadPointFile_f (void)
{
	FILE	*f;
	vec3_t	org;
	int		r;
	int		c;
	particle_t	*p;
	char	name[MAX_OSPATH];
	
	sprintf (name,"maps/%s.pts", sv.name);

	COM_FOpenFile (name, &f);
	if (!f)
	{
		Con_Printf ("couldn't open %s\n", name);
		return;
	}
	
	Con_Printf ("Reading %s...\n", name);
	c = 0;
	for ( ;; )
	{
		r = fscanf (f,"%f %f %f\n", &org[0], &org[1], &org[2]);
		if (r != 3)
			break;
		c++;
		
		if (!free_particles)
		{
			Con_Printf ("Not enough free particles\n");
			break;
		}
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;
		
#if !FIXED_POINT_PARTICLES
		p->die = 99999;
		p->color = (-c)&15;
		p->type = pt_static;
		VectorCopy (vec3_origin, p->vel);
		VectorCopy (org, p->org);
#else
		p->f16_die = FIXED16_DIE( 99999 );
		p->f16_color = (-c)&15;
		p->type = pt_static;
		p->rgf16_vel[ 0 ] = FIXED16_VELOCITY( vec3_origin[ 0 ] );
		p->rgf16_vel[ 1 ] = FIXED16_VELOCITY( vec3_origin[ 1 ] );
		p->rgf16_vel[ 2 ] = FIXED16_VELOCITY( vec3_origin[ 2 ] );
		p->rgf16_org[ 0 ] = FIXED16_VELOCITY( org[ 0 ] );
		p->rgf16_org[ 1 ] = FIXED16_VELOCITY( org[ 1 ] );
		p->rgf16_org[ 2 ] = FIXED16_VELOCITY( org[ 2 ] );
#endif
	}

	fclose (f);
	Con_Printf ("%i points read\n", c);
}

/*
===============
R_ParseParticleEffect

Parse an effect out of the server message
===============
*/
void R_ParseParticleEffect (void)
{
	vec3_t		org, dir;
	int			i, count, msgcount, color;
	
	for (i=0 ; i<3 ; i++)
		org[i] = MSG_ReadCoord ();
	for (i=0 ; i<3 ; i++)
		dir[i] = MSG_ReadChar () * (1.0/16);
	msgcount = MSG_ReadByte ();
	color = MSG_ReadByte ();

if (msgcount == 255)
	count = 1024;
else
	count = msgcount;
	
	R_RunParticleEffect (org, dir, color, count);
}
	
/*
===============
R_ParticleExplosion

===============
*/
void R_ParticleExplosion (vec3_t org)
{
	int			i, j;
	particle_t	*p;
	
	for (i=0 ; i<1024 ; i++)
	{
		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

#if !FIXED_POINT_PARTICLES
		p->die = cl.time + 5;
		p->color = ramp1[0];
		p->ramp = rand()&3;
#else
		p->f16_die = FIXED16_DIE( 5 );
		p->f16_color = ramp1[ 0 ];
		p->f16_ramp = FIXED16_RAMPF(rand()&3);
#endif
		if (i & 1)
		{
			p->type = pt_explode;
			for (j=0 ; j<3 ; j++)
			{
#if !FIXED_POINT_PARTICLES
				p->org[j] = org[j] + ((rand()%32)-16);
				p->vel[j] = (rand()%512)-256;
#else
				p->rgf16_org[j] = FIXED16_ORIGIN( org[j] + ((rand()%32)-16) );
				p->rgf16_vel[j] = FIXED16_VELOCITYF((rand()%512)-256);
#endif
			}
		}
		else
		{
			p->type = pt_explode2;
			for (j=0 ; j<3 ; j++)
			{
#if !FIXED_POINT_PARTICLES
				p->org[j] = org[j] + ((rand()%32)-16);
				p->vel[j] = (rand()%512)-256;
#else
				p->rgf16_org[j] = FIXED16_ORIGIN( org[j] + ((rand()%32)-16) );
				p->rgf16_vel[j] = FIXED16_VELOCITYF((rand()%512)-256);
#endif
			}
		}
	}
}

/*
===============
R_ParticleExplosion2

===============
*/
void R_ParticleExplosion2 (vec3_t org, int colorStart, int colorLength)
{
	int			i, j;
	particle_t	*p;
	int			colorMod = 0;

	for (i=0; i<512; i++)
	{
		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

#if !FIXED_POINT_PARTICLES
		p->die = cl.time + 0.3;
		p->color = colorStart + (colorMod % colorLength);
#else
		p->f16_die = FIXED16_DIE( 0.3 );
		p->f16_color = colorStart + (colorMod % colorLength);
#endif
		colorMod++;

		p->type = pt_blob;
		for (j=0 ; j<3 ; j++)
		{
#if !FIXED_POINT_PARTICLES
			p->org[j] = org[j] + ((rand()%32)-16);
			p->vel[j] = (rand()%512)-256;
#else
			p->rgf16_org[j] = FIXED16_ORIGIN( org[j] + ((rand()%32)-16) );
			p->rgf16_vel[j] = FIXED16_VELOCITYF((rand()%512)-256);
#endif
		}
	}
}

/*
===============
R_BlobExplosion

===============
*/
void R_BlobExplosion (vec3_t org)
{
	int			i, j;
	particle_t	*p;
	
	for (i=0 ; i<1024 ; i++)
	{
		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

#if !FIXED_POINT_PARTICLES
		p->die = cl.time + 1 + (rand()&8)*0.05;
#else
		p->f16_die = FIXED16_DIE(1 + (rand()&8)*0.05);
#endif

		if (i & 1)
		{
			p->type = pt_blob;
#if !FIXED_POINT_PARTICLES
			p->color = 66 + rand()%6;
			for (j=0 ; j<3 ; j++)
			{
				p->org[j] = org[j] + ((rand()%32)-16);
				p->vel[j] = (rand()%512)-256;
			}
#else
			p->f16_color = 66 + rand()%6;
			for (j=0 ; j<3 ; j++)
			{
				p->rgf16_org[j] = FIXED16_ORIGIN( org[j] + ((rand()%32)-16) );
				p->rgf16_vel[j] = FIXED16_VELOCITYF((rand()%512)-256);
			}
#endif
		}
		else
		{
#if !FIXED_POINT_PARTICLES
			p->type = pt_blob2;
			p->color = 150 + rand()%6;
			for (j=0 ; j<3 ; j++)
			{
				p->org[j] = org[j] + ((rand()%32)-16);
				p->vel[j] = (rand()%512)-256;
			}
#else
			p->type = pt_blob2;
			p->f16_color = 150 + rand()%6;
			for (j=0 ; j<3 ; j++)
			{
				p->rgf16_org[j] = FIXED16_ORIGIN( org[j] + ((rand()%32)-16) );
				p->rgf16_vel[j] = FIXED16_VELOCITYF((rand()%512)-256);
			}
#endif
		}
	}
}

/*
===============
R_RunParticleEffect

===============
*/
void R_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count)
{
	int			i, j;
	particle_t	*p;
	
	for (i=0 ; i<count ; i++)
	{
		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		if (count == 1024)
		{	// rocket explosion
#if !FIXED_POINT_PARTICLES
			p->die = cl.time + 5;
			p->color = ramp1[0];
			p->ramp = rand()&3;
			if (i & 1)
			{
				p->type = pt_explode;
				for (j=0 ; j<3 ; j++)
				{
					p->org[j] = org[j] + ((rand()%32)-16);
					p->vel[j] = (rand()%512)-256;
				}
			}
			else
			{
				p->type = pt_explode2;
				for (j=0 ; j<3 ; j++)
				{
					p->org[j] = org[j] + ((rand()%32)-16);
					p->vel[j] = (rand()%512)-256;
				}
			}
#else
			p->f16_die = FIXED16_DIE( 5 );
			p->f16_color = ramp1[0];
			p->f16_ramp = FIXED16_RAMPF(rand()&3);
			if (i & 1)
			{
				p->type = pt_explode;
				for (j=0 ; j<3 ; j++)
				{
					p->rgf16_org[j] = FIXED16_ORIGIN( org[j] + ((rand()%32)-16) );
					p->rgf16_vel[j] = FIXED16_VELOCITYF((rand()%512)-256);
				}
			}
			else
			{
				p->type = pt_explode2;
				for (j=0 ; j<3 ; j++)
				{
					p->rgf16_org[j] = FIXED16_ORIGIN( org[j] + ((rand()%32)-16) );
					p->rgf16_vel[j] = FIXED16_VELOCITYF((rand()%512)-256);
				}
			}
#endif
		}
		else
		{
#if !FIXED_POINT_PARTICLES
			p->die = cl.time + 0.1*(rand()%5);
			p->color = (color&~7) + (rand()&7);
			p->type = pt_slowgrav;
			for (j=0 ; j<3 ; j++)
			{
				p->org[j] = org[j] + ((rand()&15)-8);
				p->vel[j] = dir[j]*15;// + (rand()%300)-150;
			}
#else
			p->f16_die = FIXED16_DIE( 0.1*(rand()%5) );
			p->f16_color = (color&~7) + (rand()&7);
			p->type = pt_slowgrav;
			for (j=0 ; j<3 ; j++)
			{
				p->rgf16_org[j] = FIXED16_ORIGIN( org[j] + ((rand()&15)-8) );
				p->rgf16_vel[j] = FIXED16_VELOCITY(dir[j]*15);
			}
#endif
		}
	}
}


/*
===============
R_LavaSplash

===============
*/
void R_LavaSplash (vec3_t org)
{
	int			i, j, k;
	particle_t	*p;
	float		vel;
	vec3_t		dir;

	for (i=-16 ; i<16 ; i++)
		for (j=-16 ; j<16 ; j++)
			for (k=0 ; k<1 ; k++)
			{
				if (!free_particles)
					return;
				p = free_particles;
				free_particles = p->next;
				p->next = active_particles;
				active_particles = p;
#if !FIXED_POINT_PARTICLES
				p->die = cl.time + 2 + (rand()&31) * 0.02;
				p->color = 224 + (rand()&7);
#else
				p->f16_die = FIXED16_DIE( 2 + (rand()&31) * 0.02 );
				p->f16_color = 224 + (rand()&7);
#endif
				p->type = pt_slowgrav;
				
				dir[0] = j*8 + (rand()&7);
				dir[1] = i*8 + (rand()&7);
				dir[2] = 256;
	
#if !FIXED_POINT_PARTICLES
				p->org[0] = org[0] + dir[0];
				p->org[1] = org[1] + dir[1];
				p->org[2] = org[2] + (rand()&63);
#else
				p->rgf16_org[0] = FIXED16_ORIGIN( org[0] + dir[0] );
				p->rgf16_org[1] = FIXED16_ORIGIN( org[1] + dir[1] );
				p->rgf16_org[2] = FIXED16_ORIGIN( org[2] + (rand()&63) );
#endif
	
				VectorNormalize (dir);
				vel = 50 + (rand()&63);
#if !FIXED_POINT_PARTICLES
				VectorScale (dir, vel, p->vel);
#else
				{
					vec3_t v_tempvel;
					VectorScale (dir, vel, v_tempvel);
					p->rgf16_vel[ 0 ] = FIXED16_VELOCITY( v_tempvel[ 0 ] );
					p->rgf16_vel[ 1 ] = FIXED16_VELOCITY( v_tempvel[ 1 ] );
					p->rgf16_vel[ 2 ] = FIXED16_VELOCITY( v_tempvel[ 2 ] );
				}
#endif
			}
}

/*
===============
R_TeleportSplash

===============
*/
void R_TeleportSplash (vec3_t org)
{
	int			i, j, k;
	particle_t	*p;
	float		vel;
	vec3_t		dir;

	for (i=-16 ; i<16 ; i+=4)
		for (j=-16 ; j<16 ; j+=4)
			for (k=-24 ; k<32 ; k+=4)
			{
				if (!free_particles)
					return;
				p = free_particles;
				free_particles = p->next;
				p->next = active_particles;
				active_particles = p;
		
#if !FIXED_POINT_PARTICLES
				p->die = cl.time + 0.2 + (rand()&7) * 0.02;
				p->color = 7 + (rand()&7);
#else
				p->f16_die = FIXED16_DIE( 0.2 + (rand()&7) * 0.02 );
				p->f16_color = 7 + (rand()&7);
#endif
				p->type = pt_slowgrav;
				
				dir[0] = j*8;
				dir[1] = i*8;
				dir[2] = k*8;
	
#if !FIXED_POINT_PARTICLES
				p->org[0] = org[0] + i + (rand()&3);
				p->org[1] = org[1] + j + (rand()&3);
				p->org[2] = org[2] + k + (rand()&3);
#else
				p->rgf16_org[0] = FIXED16_ORIGIN( org[0] + i + (rand()&3) );
				p->rgf16_org[1] = FIXED16_ORIGIN( org[1] + j + (rand()&3) );
				p->rgf16_org[2] = FIXED16_ORIGIN( org[2] + k + (rand()&3) );
#endif
	
				VectorNormalize (dir);						
				vel = 50 + (rand()&63);

#if !FIXED_POINT_PARTICLES
				VectorScale (dir, vel, p->vel);
#else
				{
					vec3_t v_tempvel;
					VectorScale (dir, vel, v_tempvel);
					p->rgf16_vel[ 0 ] = FIXED16_VELOCITY( v_tempvel[ 0 ] );
					p->rgf16_vel[ 1 ] = FIXED16_VELOCITY( v_tempvel[ 1 ] );
					p->rgf16_vel[ 2 ] = FIXED16_VELOCITY( v_tempvel[ 2 ] );
				}
#endif
			}
}

void R_RocketTrail (vec3_t start, vec3_t end, int type)
{
	vec3_t		vec;
	float		len;
	int			j;
	particle_t	*p;
	int			dec;
	static int	tracercount;

	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);
	if (type < 128)
		dec = 3;
	else
	{
		dec = 1;
		type -= 128;
	}

	while (len > 0)
	{
		len -= dec;

		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;
		
#if !FIXED_POINT_PARTICLES
		VectorCopy (vec3_origin, p->vel);
		p->die = cl.time + 2;
#else
		p->rgf16_vel[ 0 ] = p->rgf16_vel[ 1 ] = p->rgf16_vel[ 2 ] = 0;
		p->f16_die = FIXED16_DIE( 2 );
#endif
		switch (type)
		{
			case 0:	// rocket trail
#if !FIXED_POINT_PARTICLES
				p->ramp = (rand()&3);
				p->color = ramp3[(int)p->ramp];
				p->type = pt_fire;
				for (j=0 ; j<3 ; j++)
					p->org[j] = start[j] + ((rand()%6)-3);
#else
				p->f16_ramp = FIXED16_RAMPF((rand()&3));
				p->f16_color = ramp3[(int)(p->f16_ramp)>>8];
				p->type = pt_fire;
				for (j=0 ; j<3 ; j++)
					p->rgf16_org[j] = FIXED16_ORIGIN( start[j] + ((rand()%6)-3) );
#endif
				break;

			case 1:	// smoke smoke
#if !FIXED_POINT_PARTICLES
				p->ramp = (rand()&3) + 2;
				p->color = ramp3[(int)p->ramp];
				p->type = pt_fire;
				for (j=0 ; j<3 ; j++)
					p->org[j] = start[j] + ((rand()%6)-3);
#else
				p->f16_ramp = FIXED16_RAMPF((rand()&3) + 2);
				p->f16_color = ramp3[(int)(p->f16_ramp)>>8];
				p->type = pt_fire;
				for (j=0 ; j<3 ; j++)
					p->rgf16_org[j] = FIXED16_ORIGIN( start[j] + ((rand()%6)-3) );
#endif
				break;

			case 2:	// blood
#if !FIXED_POINT_PARTICLES
				p->type = pt_grav;
				p->color = 67 + (rand()&3);
				for (j=0 ; j<3 ; j++)
					p->org[j] = start[j] + ((rand()%6)-3);
#else
				p->type = pt_grav;
				p->f16_color = 67 + (rand()&3);
				for (j=0 ; j<3 ; j++)
					p->rgf16_org[j] = FIXED16_ORIGIN( start[j] + ((rand()%6)-3) );
#endif
				break;

			case 3:
			case 5:	// tracer
#if !FIXED_POINT_PARTICLES
				p->die = cl.time + 0.5;
				p->type = pt_static;
				if (type == 3)
					p->color = 52 + ((tracercount&4)<<1);
				else
					p->color = 230 + ((tracercount&4)<<1);
#else
				p->f16_die = FIXED16_DIE( 0.5 );
				p->type = pt_static;
				if (type == 3)
					p->f16_color = 52 + ((tracercount&4)<<1);
				else
					p->f16_color = 230 + ((tracercount&4)<<1);

#endif
			
				tracercount++;

#if !FIXED_POINT_PARTICLES
				VectorCopy (start, p->org);
				if (tracercount & 1)
				{
					p->vel[0] = 30*vec[1];
					p->vel[1] = 30*-vec[0];
				}
				else
				{
					p->vel[0] = 30*-vec[1];
					p->vel[1] = 30*vec[0];
				}
#else
				p->rgf16_org[ 0 ] = FIXED16_ORIGIN( start[ 0 ] );
				p->rgf16_org[ 1 ] = FIXED16_ORIGIN( start[ 1 ] );
				p->rgf16_org[ 2 ] = FIXED16_ORIGIN( start[ 2 ] );
				if (tracercount & 1)
				{
					p->rgf16_vel[0] = FIXED16_VELOCITY( 30*vec[1] );
					p->rgf16_vel[1] = FIXED16_VELOCITY( 30*-vec[0] );
				}
				else
				{
					p->rgf16_vel[0] = FIXED16_VELOCITY( 30*-vec[1] );
					p->rgf16_vel[1] = FIXED16_VELOCITY( 30*vec[0] );
				}
#endif
				break;

			case 4:	// slight blood
				p->type = pt_grav;
#if !FIXED_POINT_PARTICLES
				p->color = 67 + (rand()&3);
				for (j=0 ; j<3 ; j++)
					p->org[j] = start[j] + ((rand()%6)-3);
#else
				p->f16_color = 67 + (rand()&3);
				for (j=0 ; j<3 ; j++)
					p->rgf16_org[j] = FIXED16_ORIGIN( start[j] + ((rand()%6)-3) );
#endif
				len -= 3;
				break;

			case 6:	// voor trail
#if !FIXED_POINT_PARTICLES
				p->color = 9*16 + 8 + (rand()&3);
				p->type = pt_static;
				p->die = cl.time + 0.3;
				for (j=0 ; j<3 ; j++)
					p->org[j] = start[j] + ((rand()&15)-8);
#else
				p->f16_color = 9*16 + 8 + (rand()&3);
				p->type = pt_static;
				p->f16_die = FIXED16_DIE( 0.3 );
				for (j=0 ; j<3 ; j++)
					p->rgf16_org[j] = FIXED16_ORIGIN( start[j] + ((rand()&15)-8) );
#endif
				break;
		}
		
		VectorAdd (start, vec, start);
	}
}


/*
===============
R_DrawParticles
===============
*/
extern	cvar_t	sv_gravity;

void R_DrawParticles (void)
{
	particle_t		*p, *kill;
	float			grav;
	int				i;
	float			time2, time3;
	float			time1;
	float			dvel;
	float			frametime;
#if FIXED_POINT_PARTICLES
	fixed16_t f16_frametime, f16_time1, f16_time2, f16_time3, f16_dvel, f16_grav;
#endif
#ifdef GLQUAKE
	vec3_t			up, right;
	float			scale;

    GL_Bind(particletexture);
	glEnable (GL_BLEND);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glBegin (GL_TRIANGLES);

	VectorScale (vup, 1.5, up);
	VectorScale (vright, 1.5, right);
#else
	D_StartParticles ();

	VectorScale (vright, xscaleshrink, r_pright);
	VectorScale (vup, yscaleshrink, r_pup);
	VectorCopy (vpn, r_ppn);
#if FIXED_POINT_PARTICLES
	for( i = 0; i < 3; i++ )
	{
		r_pright_fixed[ i ] = (int)( r_pright[ i ] * ( float )( 1 << 16 ) );
		r_pup_fixed[ i ] = (int)( r_pup[ i ] * ( float )( 1 << 16 ) );
		r_ppn_fixed[ i ] = (int)( r_ppn[ i ] * ( float )( 1 << 16 ) );
		r_porigin_fixed[ i ] = ( int )( r_origin[ i ] * ( float )( 1 << 8 ) );
	}
	pxcenter_fixed = ( (int)( xcenter * 256.0f ) + 128 );
	pycenter_fixed = ( (int)( ycenter * 256.0f ) + 128 );
#endif

#endif
	frametime = cl.time - cl.oldtime;
	time3 = frametime * 15;
	time2 = frametime * 10; // 15;
	time1 = frametime * 5;
	grav = frametime * sv_gravity.value * 0.05;
	dvel = 4*frametime;
	
#if FIXED_POINT_PARTICLES
	f16_frametime = FIXED16_DIE( frametime );
	f16_time1 = FIXED16_DIE( time1 );
	f16_time2 = FIXED16_DIE( time2 );
	f16_time3 = FIXED16_DIE( time3 );
	f16_grav = FIXED16_VELOCITY( grav );
	f16_dvel = FIXED16_VELOCITY( dvel );

	for (p=active_particles ; p ; p=p->next)
	{
		p->f16_die -= f16_frametime;
	}
#endif

	for ( ;; ) 
	{
		kill = active_particles;
#if !FIXED_POINT_PARTICLES
		if (kill && kill->die < cl.time)
#else
		if ( kill && kill->f16_die < 0)
#endif
		{
			active_particles = kill->next;
			kill->next = free_particles;
			free_particles = kill;
			continue;
		}
		break;
	}

	for (p=active_particles ; p ; p=p->next)
	{
		for ( ;; )
		{
			kill = p->next;
#if !FIXED_POINT_PARTICLES
			if (kill && kill->die < cl.time)
#else
			if ( kill && kill->f16_die < 0)
#endif
			{
				p->next = kill->next;
				kill->next = free_particles;
				free_particles = kill;
				continue;
			}
			break;
		}

#ifdef GLQUAKE
		// hack a scale up to keep particles from disapearing
		scale = (p->org[0] - r_origin[0])*vpn[0] + (p->org[1] - r_origin[1])*vpn[1]
			+ (p->org[2] - r_origin[2])*vpn[2];
		if (scale < 20)
			scale = 1;
		else
			scale = 1 + scale * 0.004;
		glColor3ubv ((byte *)&d_8to24table[(int)p->color]);
		glTexCoord2f (0,0);
		glVertex3fv (p->org);
		glTexCoord2f (1,0);
		glVertex3f (p->org[0] + up[0]*scale, p->org[1] + up[1]*scale, p->org[2] + up[2]*scale);
		glTexCoord2f (0,1);
		glVertex3f (p->org[0] + right[0]*scale, p->org[1] + right[1]*scale, p->org[2] + right[2]*scale);
#else
		D_DrawParticle (p);
#endif

#if !FIXED_POINT_PARTICLES
		p->org[0] += p->vel[0]*frametime;
		p->org[1] += p->vel[1]*frametime;
		p->org[2] += p->vel[2]*frametime;
#else
		p->rgf16_org[0] += FIXED16_ADDVELOCITY( p->rgf16_vel[ 0 ], f16_frametime );
		p->rgf16_org[1] += FIXED16_ADDVELOCITY( p->rgf16_vel[ 1 ], f16_frametime );
		p->rgf16_org[2] += FIXED16_ADDVELOCITY( p->rgf16_vel[ 2 ], f16_frametime );
#endif
		
		switch (p->type)
		{
		case pt_static:
			break;
		case pt_fire:
#if !FIXED_POINT_PARTICLES
			p->ramp += time1;
			if (p->ramp >= 6)
				p->die = -1;
			else
				p->color = ramp3[(int)p->ramp];
			p->vel[2] += grav;
#else
			p->f16_ramp += f16_time1;
			if (p->f16_ramp >= (6<<8))
				p->f16_die = -1;
			else
				p->f16_color = ramp3[(int)(p->f16_ramp>>8)];
			p->rgf16_vel[2] += f16_grav;
#endif
			break;

		case pt_explode:
#if !FIXED_POINT_PARTICLES
			p->ramp += time2;
			if (p->ramp >=8)
				p->die = -1;
			else
				p->color = ramp1[(int)p->ramp];
			for (i=0 ; i<3 ; i++)
				p->vel[i] += p->vel[i]*dvel;
			p->vel[2] -= grav;
#else
			p->f16_ramp += f16_time2;
			if (p->f16_ramp >=8<<8)
				p->f16_die = -1;
			else
				p->f16_color = ramp1[(int)(p->f16_ramp>>8)];
			for (i=0 ; i<3 ; i++)
				p->rgf16_vel[i] += FIXED16_ADDVELOCITYV(p->rgf16_vel[i],f16_dvel);
			p->rgf16_vel[2] -= f16_grav;
#endif
			break;

#if !FIXED_POINT_PARTICLES
		case pt_explode2:
			p->ramp += time3;
			if (p->ramp >=8)
				p->die = -1;
			else
				p->color = ramp2[(int)p->ramp];
			for (i=0 ; i<3 ; i++)
				p->vel[i] -= p->vel[i]*frametime;
			p->vel[2] -= grav;
			break;

		case pt_blob:
			for (i=0 ; i<3 ; i++)
				p->vel[i] += p->vel[i]*dvel;
			p->vel[2] -= grav;
			break;

		case pt_blob2:
			for (i=0 ; i<2 ; i++)
				p->vel[i] -= p->vel[i]*dvel;
			p->vel[2] -= grav;
			break;

		case pt_grav:
#ifdef QUAKE2
			p->vel[2] -= grav * 20;
			break;
#endif
		case pt_slowgrav:
			p->vel[2] -= grav;
			break;
#else
		case pt_explode2:
			p->f16_ramp += f16_time3;
			if (p->f16_ramp >=8<<8)
				p->f16_die = -1;
			else
				p->f16_color = ramp2[(int)p->f16_ramp>>8];
			for (i=0 ; i<3 ; i++)
				p->rgf16_vel[i] -= FIXED16_ADDVELOCITY( p->rgf16_vel[i], f16_frametime );
			p->rgf16_vel[2] -= f16_grav;
			break;

		case pt_blob:
			for (i=0 ; i<3 ; i++)
				p->rgf16_vel[i] += FIXED16_ADDVELOCITYV(p->rgf16_vel[i],f16_dvel);;
			p->rgf16_vel[2] -= f16_grav;
			break;

		case pt_blob2:
			for (i=0 ; i<2 ; i++)
				p->rgf16_vel[i] -= FIXED16_ADDVELOCITYV(p->rgf16_vel[i],f16_dvel);;
			p->rgf16_vel[2] -= f16_grav;
			break;

		case pt_grav:
#ifdef QUAKE2
			p->vel[2] -= grav * 20;
			break;
#endif
		case pt_slowgrav:
			p->rgf16_vel[2] -= f16_grav;
			break;
#endif
		}
	}

#ifdef GLQUAKE
	glEnd ();
	glDisable (GL_BLEND);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
#else
	D_EndParticles ();
#endif
}

