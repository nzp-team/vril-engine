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

#include <vitaGL.h>
#include "../../nzportable_def.h"
#include "neon_mathfun.h"
#include "r_local.h"

#define DEFAULT_NUM_PARTICLES	1024	// default max # of particles at one time
#define ABSOLUTE_MIN_PARTICLES	512		// no fewer than this no matter what's on the command line
#define ABSOLUTE_MAX_PARTICLES	8192

static int ramp1[8] = {0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61};
static int ramp2[8] = {0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x66};
static int ramp3[6] = {0x6d, 0x6b, 0x06, 0x05, 0x04, 0x03};

particle_t	*active_particles, *free_particles;

particle_t	*particles;
int			r_numparticles;

vec3_t			r_pright, r_pup, r_ppn;

extern float d_8to32ftable[];

/*
===============
R_InitParticles
===============
*/
void R_InitParticles (void)
{
	int		i;

	if ((i = COM_CheckParm ("-particles"))  && i+1 < com_argc)
	{
		r_numparticles = (int)(atoi(com_argv[i+1]));
		if (r_numparticles < ABSOLUTE_MIN_PARTICLES) r_numparticles = ABSOLUTE_MIN_PARTICLES;
		else if (r_numparticles > ABSOLUTE_MAX_PARTICLES) r_numparticles = ABSOLUTE_MAX_PARTICLES;
	}
	else
	{
		r_numparticles = DEFAULT_NUM_PARTICLES;
	}

	particles = (particle_t *)
			Hunk_AllocName (r_numparticles * sizeof(particle_t), "particles");
}


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
	int			i;
	particle_t	*p;
	float		angle;
	float		sr, sp, sy, cr, cp, cy;
	vec3_t		forward;
	float		dist;
	
	dist = 64;

	if (!avelocities[0][0])
	{
		for (i = 0 ; i < NUMVERTEXNORMALS; i++)
		{
			avelocities[i][0] = (rand() & 255) * 0.01;
			avelocities[i][1] = (rand() & 255) * 0.01;
			avelocities[i][2] = (rand() & 255) * 0.01;
		}
	}	


	for (i=0 ; i<NUMVERTEXNORMALS ; i++)
	{
		
		v4sf src = {
			cl.time * avelocities[i][0],
			cl.time * avelocities[i][1],
			cl.time * avelocities[i][2],
			0
		};
		
		v4sf sins, coss;
		sincos_ps(src, &sins, &coss);
		
		sy = sins[0];
		cy = coss[0];
		sp = sins[1];
		cp = coss[2];
		sr = sins[2];
		cr = coss[2];
	
		forward[0] = cp*cy;
		forward[1] = cp*sy;
		forward[2] = -sp;

		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = cl.time + 0.01;
		p->color = 0x6f;
		p->type = pt_explode;
		
		p->org[0] = ent->origin[0] + r_avertexnormals[i][0]*dist + forward[0]*beamlength;			
		p->org[1] = ent->origin[1] + r_avertexnormals[i][1]*dist + forward[1]*beamlength;			
		p->org[2] = ent->origin[2] + r_avertexnormals[i][2]*dist + forward[2]*beamlength;			
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

/*
===============
R_ReadPointFile_f
===============
*/
void R_ReadPointFile_f (void)
{
	FILE	*f;
	vec3_t	org;
	int		r;
	int		c;
	particle_t	*p;
	char	name[MAX_OSPATH];
	
	snprintf(name, sizeof(name), "maps/%s.pts", sv.name);

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
		r = fscanf (f, "%f %f %f\n", &org[0], &org[1], &org[2]);
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
		
		p->die = 99999;
		p->color = (-c)&15;
		p->type = pt_static;
		VectorCopy (vec3_origin, p->vel);
		VectorCopy (org, p->org);
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
		
		p->color = ramp1[0];
		p->ramp = rand()&3;
		if (i & 1)
		{
			p->type = pt_explode;
			p->die = cl.time + (8 - p->ramp) / 10.0f;
		}
		else
		{
			p->type = pt_explode2;
			p->die = cl.time + (8 - p->ramp) / 15.0f;
		}
		for (j=0 ; j<3 ; j++)
		{
			p->org[j] = org[j] + ((rand()%32)-16);
			p->vel[j] = (rand()%512)-256;
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

		p->die = cl.time + 0.3;
		p->color = colorStart + (colorMod % colorLength);
		colorMod++;

		p->type = pt_blob;
		for (j=0 ; j<3 ; j++)
		{
			p->org[j] = org[j] + ((rand()%32)-16);
			p->vel[j] = (rand()%512)-256;
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

		p->die = cl.time + 1 + (rand()&8)*0.05;

		if (i & 1)
		{
			p->type = pt_blob;
			p->color = 66 + rand()%6;
			for (j=0 ; j<3 ; j++)
			{
				p->org[j] = org[j] + ((rand()%32)-16);
				p->vel[j] = (rand()%512)-256;
			}
		}
		else
		{
			p->type = pt_blob2;
			p->color = 150 + rand()%6;
			for (j=0 ; j<3 ; j++)
			{
				p->org[j] = org[j] + ((rand()%32)-16);
				p->vel[j] = (rand()%512)-256;
			}
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
			p->color = ramp1[0];
			p->ramp = rand()&3;
			if (i & 1)
			{
				p->type = pt_explode;
				p->die = cl.time + (8 - p->ramp) / 10.0f;
			}
			else
			{
				p->type = pt_explode2;
				p->die = cl.time + (8 - p->ramp) / 15.0f;
			}
			for (j=0 ; j<3 ; j++)
			{
				p->org[j] = org[j] + ((rand()%32)-16);
				p->vel[j] = (rand()%512)-256;
			}
		}
		else
		{
			p->die = cl.time + 0.1*(rand()%5);
			p->color = (color&~7) + (rand()&7);
			p->type = pt_slowgrav;
			for (j=0 ; j<3 ; j++)
			{
				p->org[j] = org[j] + ((rand()&15)-8);
				p->vel[j] = dir[j]*15;// + (rand()%300)-150;
			}
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
		
				p->die = cl.time + 2 + (rand()&31) * 0.02;
				p->color = 224 + (rand()&7);
				p->type = pt_slowgrav;
				
				dir[0] = j*8 + (rand()&7);
				dir[1] = i*8 + (rand()&7);
				dir[2] = 256;
	
				p->org[0] = org[0] + dir[0];
				p->org[1] = org[1] + dir[1];
				p->org[2] = org[2] + (rand()&63);
	
				VectorNormalize (dir);						
				vel = 50 + (rand()&63);
				VectorScale (dir, vel, p->vel);
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
		
				p->die = cl.time + 0.2 + (rand()&7) * 0.02;
				p->color = 7 + (rand()&7);
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

/*
===============
R_RocketTrail

FIXME -- rename function and use #defined types instead of numbers
===============
*/
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
		
		VectorCopy (vec3_origin, p->vel);
		p->die = cl.time + 2;
		
		switch (type)
		{
			case 0:	// rocket trail
				p->ramp = (rand()&3);
				if (p->ramp >= 6) p->ramp -= 6;
				p->die = cl.time + (6 - p->ramp) / 5.0f;
				p->color = ramp3[(int)p->ramp];
				p->type = pt_fire;
				for (j=0 ; j<3 ; j++)
					p->org[j] = start[j] + ((rand()%6)-3);
				break;

			case 1:	// smoke trail
				p->ramp = (rand()&2) + 2;
				p->die = cl.time + (6 - p->ramp) / 5.0f;
				p->color = ramp3[(int)p->ramp];
				p->type = pt_fire;
				for (j=0 ; j<3 ; j++)
					p->org[j] = start[j] + ((rand()%6)-3);
				break;

			case 2:	// blood
				p->type = pt_grav;
				p->color = 67 + (rand()&3);
				for (j=0 ; j<3 ; j++)
					p->org[j] = start[j] + ((rand()%6)-3);
				break;

			case 3:
			case 5:	// tracer
				p->die = cl.time + 0.5;
				p->type = pt_static;
				if (type == 3)
					p->color = 52 + ((tracercount&4)<<1);
				else
					p->color = 230 + ((tracercount&4)<<1);
			
				tracercount++;

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
				break;

			case 4:	// slight blood
				p->type = pt_grav;
				p->color = 67 + (rand()&3);
				for (j=0 ; j<3 ; j++)
					p->org[j] = start[j] + ((rand()%6)-3);
				len -= 3;
				break;

			case 6:	// voor trail
				p->color = 9*16 + 8 + (rand()&3);
				p->type = pt_static;
				p->die = cl.time + 0.3;
				for (j=0 ; j<3 ; j++)
					p->org[j] = start[j] + ((rand()&15)-8);
				break;
		}
		

		VectorAdd (start, vec, start);
	}
}

/*
===============
CL_RunParticles -- johnfitz -- all the particle behavior, separated from R_DrawParticles
===============
*/
void CL_RunParticles (void)
{
	particle_t		*p, *kill;
	int				i;
	float			time1, time2, time3, dvel, frametime, scale, grav;
	extern	cvar_t	sv_gravity;

	frametime = cl.time - cl.oldtime;
	time3 = frametime * 15;
	time2 = frametime * 10;
	time1 = frametime * 5;
	grav = frametime * sv_gravity.value * 0.05;
	dvel = 4*frametime;

	for ( ;; )
	{
		kill = active_particles;
		if (kill && kill->die < cl.time)
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
			if (kill && kill->die < cl.time)
			{
				p->next = kill->next;
				kill->next = free_particles;
				free_particles = kill;
				continue;
			}
			break;
		}

		p->org[0] += p->vel[0]*frametime;
		p->org[1] += p->vel[1]*frametime;
		p->org[2] += p->vel[2]*frametime;

		switch (p->type)
		{
		case pt_static:
			break;
		case pt_fire:
			p->ramp += time1;
			if (p->ramp >= 6)
				p->die = cl.time - 0.01f;
			else
				p->color = ramp3[(int)p->ramp];
			p->vel[2] += grav;
			break;

		case pt_explode:
			p->ramp += time2;
			if (p->ramp >=8)
				p->die = cl.time - 0.01f;
			else
				p->color = ramp1[(int)p->ramp];
			for (i=0 ; i<3 ; i++)
				p->vel[i] += p->vel[i]*dvel;
			p->vel[2] -= grav;
			break;

		case pt_explode2:
			p->ramp += time3;
			if (p->ramp >=8)
				p->die = cl.time - 0.01f;
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
		case pt_slowgrav:
			p->vel[2] -= grav;
			break;
		}
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
	vec3_t			up, right, p_up, p_right, p_upright; //johnfitz -- p_ vectors
	GLubyte		color[4], *c; //johnfitz -- particle transparency
	float			alpha; //johnfitz -- particle transparency
	float			grav;
	int				i;
	float			time2, time3;
	float			time1;
	float			dvel;
	float			frametime;
	float*			pPos = gVertexBuffer;
	float*			pColor = gColorBuffer;
	float*			pUV = gTexCoordBuffer;
	float			colors[3];
	
	float			scale;
	
    GL_Bind(particletexture);
	
	
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable (GL_BLEND);
	GL_EnableState(GL_MODULATE);
	glDepthMask (GL_FALSE); //johnfitz -- fix for particle z-buffer bug
	GL_EnableState(GL_COLOR_ARRAY);
	
	//->glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

	VectorScale (vup, 1.50, up);
	VectorScale (vright, 1.50, right);
	
	int num_vertices = 0;
	for (p=active_particles ; p ; p=p->next)
	{
		num_vertices += 3;

		// hack a scale up to keep particles from disapearing
		scale = (p->org[0] - r_origin[0])*vpn[0] + (p->org[1] - r_origin[1])*vpn[1]
			+ (p->org[2] - r_origin[2])*vpn[2];
		if (scale < 20)
			scale = 1.08f; //johnfitz -- added .08 to be consistent
		else
			scale = 1 + scale * 0.004;
		
		scale *= 1.27;
		
		c = (GLubyte *) &d_8to24table[(int)p->color];
		colors[0] = ((float)(c[0])) / 255.0f;
		colors[1] = ((float)(c[1])) / 255.0f;
		colors[2] = ((float)(c[2])) / 255.0f;
		*gColorBuffer++ = colors[0];
		*gColorBuffer++ = colors[1];
		*gColorBuffer++ = colors[2];
		*gColorBuffer++ = 1.0f;
		*gTexCoordBuffer++ = 0.0f;
		*gTexCoordBuffer++ = 0.0f;
		*gVertexBuffer++ = p->org[0];
		*gVertexBuffer++ = p->org[1];
		*gVertexBuffer++ = p->org[2];

		*gColorBuffer++ = colors[0];
		*gColorBuffer++ = colors[1];
		*gColorBuffer++ = colors[2];
		*gColorBuffer++ = 1.0f;
		*gTexCoordBuffer++ = 1.0f;
		*gTexCoordBuffer++ = 0.0f;
		VectorMA (p->org, scale, up, p_up);
		*gVertexBuffer++ = (p_up[0]);
		*gVertexBuffer++ = (p_up[1]);
		*gVertexBuffer++ = (p_up[2]);

		*gColorBuffer++ = colors[0];
		*gColorBuffer++ = colors[1];
		*gColorBuffer++ = colors[2];
		*gColorBuffer++ = 1.0f;
		*gTexCoordBuffer++ = 0.0;
		*gTexCoordBuffer++ = 1.0;
		VectorMA (p->org, scale, right, p_right);
		*gVertexBuffer++ = (p_right[0]);
		*gVertexBuffer++ = (p_right[1]);
		*gVertexBuffer++ = (p_right[2]);
	}

	vglVertexAttribPointerMapped(0, pPos);
	vglVertexAttribPointerMapped(1, pUV);
	vglVertexAttribPointerMapped(2, pColor);
	GL_DrawPolygon(GL_TRIANGLES, num_vertices);
	GL_DisableState(GL_COLOR_ARRAY);
	//->glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glDepthMask (GL_TRUE);
	glDisable (GL_BLEND);
	GL_EnableState(GL_REPLACE);
	Platform_Graphics_Color(1,1,1,1);
}

