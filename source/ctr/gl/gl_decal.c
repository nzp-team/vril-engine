
#include "../../quakedef.h"

void R_SpawnDecal (vec3_t center, vec3_t normal, vec3_t tangent, int tex, int size, int isbsp)
{
// naievil -- fixme
/*
    int		a;
	float	width, height, depth, d, one_over_w, one_over_h;
	vec3_t	binormal, test = {0.5, 0.5, 0.5};
	decal_t	*dec;

	if (!qmb_initialized)
		return;

	// allocate decal
	if (!free_decals)
		return;

	dec           = free_decals;
	free_decals   = dec->next;
	dec->next     = active_decals;
	active_decals = dec;

	VectorNormalize (test);
	CrossProduct (normal, test, tangent);

	VectorCopy (center, dec->origin);
	VectorCopy (tangent, dec->tangent);
	VectorCopy (normal, dec->normal);
	VectorNormalize (tangent);
	VectorNormalize (normal);
	CrossProduct (normal, tangent, binormal);
	VectorNormalize (binormal);

	width          = RandomMinMax (size * 0.5, size);
	height         = width;
	depth          = width * 0.5;
	dec->radius    = fmax(fmax(width, height), depth);
	dec->starttime = cl.time;
	dec->bspdecal  = isbsp;
    dec->die       = (isbsp ? 0 : cl.time + r_decaltime.value);
	dec->texture   = tex;

	// Calculate boundary planes
	d = DotProduct (center, tangent);
	VectorCopy (tangent, leftPlane.normal);
	leftPlane.dist  = -(width * 0.5 - d);
	VectorNegate (tangent, tangent);
	VectorCopy (tangent, rightPlane.normal);
	VectorNegate (tangent, tangent);
	rightPlane.dist = -(width * 0.5 + d);

	d = DotProduct (center, binormal);
	VectorCopy (binormal, bottomPlane.normal);
	bottomPlane.dist = -(height * 0.5 - d);
	VectorNegate (binormal, binormal);
	VectorCopy (binormal, topPlane.normal);
	VectorNegate (binormal, binormal);
	topPlane.dist    = -(height * 0.5 + d);

	d = DotProduct (center, normal);
	VectorCopy (normal, backPlane.normal);
	backPlane.dist  = -(depth - d);
	VectorNegate (normal, normal);
	VectorCopy (normal, frontPlane.normal);
	VectorNegate (normal, normal);
	frontPlane.dist = -(depth + d);

	// Begin with empty mesh
	dec->vertexCount   = 0;
	dec->triangleCount = 0;

	// Clip decal to bsp
	DecalWalkBsp_R (dec, cl.worldmodel->nodes);

	// This happens when a decal is to far from any surface or the surface is to steeply sloped
	if (dec->triangleCount == 0)
	{	// deallocate decal
		active_decals = dec->next;
		dec->next   = free_decals;
		free_decals = dec;
		return;
	}

	// Assign texture mapping coordinates
	one_over_w = 1.0F / width;
	one_over_h = 1.0F / height;
	for (a = 0 ; a < dec->vertexCount ; a++)
	{
		float	s, t;
		vec3_t	v;

		VectorSubtract (dec->vertexArray[a], center, v);
		s = DotProduct (v, tangent) * one_over_w + 0.5F;
		t = DotProduct (v, binormal) * one_over_h + 0.5F;
		dec->texcoordArray[a][0] = s;
		dec->texcoordArray[a][1] = t;
	}
*/
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