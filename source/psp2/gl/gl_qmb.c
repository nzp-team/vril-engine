#include "../../quakedef.h"


//#define	DEFAULT_NUM_PARTICLES		8192
#define	ABSOLUTE_MIN_PARTICLES      64
#define	ABSOLUTE_MAX_PARTICLES      6144

int decal_blood1, decal_blood2, decal_blood3, decal_q3blood, decal_burn, decal_mark, decal_glow;

typedef	enum
{
	p_spark,
	p_rayspark,
	p_raysmoke,
	p_smoke,
	p_fire,
	p_fire2,
	p_bubble,
	p_gunblast,
	p_chunk,
	p_shockwave,
	p_inferno_flame,
	p_inferno_trail,
	p_sparkray,
	p_staticbubble,
	p_trailpart,
	p_dpsmoke,
	p_dpfire,
	p_teleflare,
	p_blood1,
	p_blood2,
	p_blood3,
	p_bloodcloud,
	p_flame,
	p_lavatrail,
	p_bubble2,
	p_rain,
	p_streak,
	p_streaktrail,
	p_streakwave,
	p_lightningbeam,
	p_glow,
	p_alphatrail,//R00k
	p_torch_flame,
	p_flare,
	p_dot,
	p_muzzleflash,
	p_muzzleflash2,
	p_muzzleflash3,
	p_q3flame,
	num_particletypes
} part_type_t;

typedef	enum
{
	pm_static,
	pm_normal,
	pm_bounce,
	pm_die,
	pm_nophysics,
	pm_float,
	pm_rain,
	pm_streak,
	pm_streakwave,
} part_move_t;

typedef	enum
{
	ptex_none,
	ptex_smoke,
	ptex_generic,
	ptex_dpsmoke,
	ptex_blood1,
	ptex_blood2,
	ptex_blood3,
	ptex_lightning,
	ptex_flame,
	ptex_muzzleflash,
	ptex_muzzleflash2,
	ptex_muzzleflash3,
	ptex_bloodcloud,
	ptex_q3flame,
	num_particletextures
} part_tex_t;

typedef	enum
{
	pd_spark,
	pd_sparkray,
	pd_billboard,
    pd_billboard_vel,
	pd_hide,
	pd_beam,
    pd_q3flame,
	pd_q3gunshot,
	pd_q3teleport
} part_draw_t;

typedef	struct particle_type_s
{
	particle_t			*start;
	part_type_t			id;
	part_draw_t			drawtype;
	int					SrcBlend;
	int					DstBlend;
	part_tex_t			texture;
	float				startalpha;
	float				grav;
	float				accel;
	part_move_t 		move;
	float				custom;
} particle_type_t;

#define	MAX_PTEX_COMPONENTS		8

typedef struct particle_texture_s
{
	int		texnum;
	int		components;
	float	coords[MAX_PTEX_COMPONENTS][4];
} particle_texture_t;

static	float	sint[7] = {0.000000, 0.781832, 0.974928, 0.433884, -0.433884, -0.974928, -0.781832};
static	float	cost[7] = {1.000000, 0.623490, -0.222521, -0.900969, -0.900969, -0.222521, 0.623490};

static			particle_t			*particles, *free_particles, active_particles;
static			particle_type_t		particle_types[num_particletypes];//R00k
static	int		particle_type_index[num_particletypes];
static			particle_texture_t	particle_textures[num_particletextures];

int				lightning_texture;//Vult
float			varray_vertex[16];//Vult
void			R_CalcBeamVerts (float *vert, vec3_t org1, vec3_t org2, float width);//Vult

vec3_t	NULLVEC = {0,0,0};//r00k

static	int		r_numparticles;
static	vec3_t	zerodir = {22, 22, 22};
static	int		particle_count = 0;
static	float	particle_time;
qboolean		qmb_initialized = false;
int				particle_mode = 0;	// 0: classic (default), 1: QMB, 2: mixed

cvar_t	r_particle_count	= {"r_particle_count", "1024", true};
cvar_t	r_bounceparticles	= {"r_bounceparticles", "1", true};
cvar_t	r_decal_blood		= {"r_decal_blood", "1", true};
cvar_t	r_decal_bullets	    = {"r_decal_bullets","1", true};
cvar_t	r_decal_sparks		= {"r_decal_sparks","1", true};
cvar_t	r_decal_explosions	= {"r_decal_explosions","1", true};

int	decals_enabled;

void QMB_InitParticles (void)
{
    return;
}

void QMB_ClearParticles (void)
{
    return;
}

void QMB_DrawParticles (void)
{
    return;
}

void QMB_Q3TorchFlame (vec3_t org, float size)
{
    return;
}

void QMB_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count)
{
    return;
}

void QMB_RocketTrail (vec3_t start, vec3_t end, trail_type_t type)
{
    return;
}

void QMB_BlobExplosion (vec3_t org)
{
    return;
}

void QMB_ParticleExplosion (vec3_t org)
{
    return;
}

void QMB_LavaSplash (vec3_t org)
{
    return;
}

void QMB_TeleportSplash (vec3_t org)
{
    return;
}

void QMB_InfernoFlame (vec3_t org)
{
    return;
}

void QMB_StaticBubble (entity_t *ent)
{
    return;
}

void QMB_ColorMappedExplosion (vec3_t org, int colorStart, int colorLength)
{
    return;
}

void QMB_TorchFlame (vec3_t org)
{
    return;
}

void QMB_FlameGt (vec3_t org, float size, float time)
{
    return;
}

void QMB_BigTorchFlame (vec3_t org)
{
    return;
}

void QMB_ShamblerCharge (vec3_t org)
{
    return;
}

void QMB_LightningBeam (vec3_t start, vec3_t end)
{
    return;
}

void QMB_EntityParticles (entity_t *ent)
{
    return;
}

void QMB_MuzzleFlash (vec3_t org)
{
    return;
}

void QMB_MuzzleFlashLG (vec3_t org)
{
    return;
}

void QMB_Q3Gunshot (vec3_t org, int skinnum, float alpha)
{
    return;
}

void QMB_Q3Teleport (vec3_t org, float alpha)
{
    return;
}

//Revamped by blubs
void R_SpawnDecalStatic (vec3_t org, int tex, int size)
{
	/*
	int		i;
	float	frac, bestfrac;
	vec3_t	tangent, v, bestorg, normal, bestnormal, org2;
	vec3_t tempVec;

	if (!qmb_initialized)
		return;

	VectorClear (bestorg);
	VectorClear (bestnormal);
	VectorClear(tempVec);

	bestfrac = 10;
	for (i = 0 ; i < 26 ; i++)
	{
		//Reference: i = 0: check straight up, i = 1: check straight down
			//1 < i < 10: Check sideways in increments of 45 degrees
			//9 < i < 18: Check angled 45 degrees down in increments of 45 degrees
			//17 < i : Check angled 45 degrees up in increments of 45 degrees
		org2[0] = (((((i - 2) % 8) < 2) ||  (((i - 2) % 8) == 7)) ? 1 : 0 ) + ((((i - 2) % 8) > 2 &&  ((i - 2) % 8) < 6) ? -1 : 0 );
		org2[1] = ((((i - 2) % 8) > 0 &&  ((i - 2) % 8) < 4) ? 1 : 0 ) + ((((i - 2) % 8) > 4 && ((i - 2) % 8) < 7) ? -1 : 0 );
		org2[2] = ((i == 0) ? 1 : 0) + ((i == 1) ? -1 : 0) + (((i > 9) && (i < 18)) ? 1 : 0) + ((i > 17) ? -1 : 0);

		VectorCopy(org,tempVec);
		VectorMA(tempVec, -0.1,org2,tempVec);

		VectorMA (org, 20, org2, org2);
		TraceLineN (tempVec, org2, v, normal);

		VectorSubtract(org2,tempVec,org2);//goal
		VectorSubtract(v,tempVec,tempVec);//collision

		if(VectorLength(org2) == 0)
			return;

		frac = VectorLength(tempVec) / VectorLength(org2);

		if(frac < 1 && frac < bestfrac)
		{
			bestfrac = frac;
			VectorCopy(v,bestorg);
			VectorCopy(normal, bestnormal);
			CrossProduct(normal,bestnormal,tangent);
		}
	}

	if (bestfrac < 1)
		R_SpawnDecal (bestorg, bestnormal, tangent, tex, size, 0);
	*/
}