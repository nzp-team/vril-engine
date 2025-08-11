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
// world.c -- world query functions

#include "nzportable_def.h"

/*#ifdef PSP_VFPU
#include <pspmath.h>
#endif*/

/*

entities never clip against themselves, or their owner

line of sight checks trace->crosscontent, but bullets don't

*/


typedef struct
{
	vec3_t		boxmins, boxmaxs;// enclose the test object along entire move
	float		*mins, *maxs;	// size of the moving object
	vec3_t		mins2, maxs2;	// size when clipping against mosnters
	float		*start, *end;
	trace_t		trace;
	int			type;
	edict_t		*passedict;
} moveclip_t;


int SV_HullPointContents (hull_t *hull, int num, vec3_t p);

/*
===============================================================================

HULL BOXES

===============================================================================
*/


static	hull_t		box_hull;
static	dclipnode_t	box_clipnodes[6];
static	mplane_t	box_planes[6];

/*
===================
SV_InitBoxHull

Set up the planes and clipnodes so that the six floats of a bounding box
can just be stored out and get a proper hull_t structure.
===================
*/
void SV_InitBoxHull (void)
{
	int		i;
	int		side;

	box_hull.clipnodes = box_clipnodes;
	box_hull.planes = box_planes;
	box_hull.firstclipnode = 0;
	box_hull.lastclipnode = 5;

	for (i=0 ; i<6 ; i++)
	{
		box_clipnodes[i].planenum = i;

		side = i&1;

		box_clipnodes[i].children[side] = CONTENTS_EMPTY;
		if (i != 5)
			box_clipnodes[i].children[side^1] = i + 1;
		else
			box_clipnodes[i].children[side^1] = CONTENTS_SOLID;

		box_planes[i].type = i>>1;
		box_planes[i].normal[i>>1] = 1;
	}

}


/*
===================
SV_HullForBox

To keep everything totally uniform, bounding boxes are turned into small
BSP trees instead of being compared directly.
===================
*/
hull_t	*SV_HullForBox (vec3_t mins, vec3_t maxs)
{
	box_planes[0].dist = maxs[0];
	box_planes[1].dist = mins[0];
	box_planes[2].dist = maxs[1];
	box_planes[3].dist = mins[1];
	box_planes[4].dist = maxs[2];
	box_planes[5].dist = mins[2];

	return &box_hull;
}

/*
================
SV_HullForEntity

Returns a hull that can be used for testing or clipping an object of mins/maxs
size.
Offset is filled in to contain the adjustment that must be added to the
testing object's origin to get a point to use with the returned hull.
================
*/
hull_t *SV_HullForEntity (edict_t *ent, vec3_t mins, vec3_t maxs, vec3_t offset, edict_t *move_ent)
{
	model_t		*model;
	vec3_t		size;
	vec3_t		hullmins, hullmaxs;
	hull_t		*hull;

// decide which clipping hull to use, based on the size
	if (ent->v.solid == SOLID_BSP)
	{	// explicit hulls in the BSP model
		if (ent->v.movetype != MOVETYPE_PUSH)
			Sys_Error ("SOLID_BSP without MOVETYPE_PUSH");

		model = sv.models[ (int)ent->v.modelindex ];

		if (!model || model->type != mod_brush)
			Sys_Error ("MOVETYPE_PUSH with a non bsp model");

		VectorSubtract (maxs, mins, size);

		{
			if (model->bspversion == HL_BSPVERSION)
			{
				if (size[0] < 3)
				{
					hull = &model->hulls[0]; // 0x0x0
				}
				else if (size[0] <= 32)
				{
					if (size[2] < 54) // pick the nearest of 36 or 72
						hull = &model->hulls[3]; // 32x32x36
					else
						hull = &model->hulls[1]; // 32x32x72
				}
				else
				{
					hull = &model->hulls[2]; // 64x64x64
				}
			}
			else
			{
				if (size[0] < 3)
					hull = &model->hulls[0];
				else if (size[0] <= 32)
					hull = &model->hulls[1];
				else if (size[0] <= 32 && size[2] <= 28)  // Crouch
					hull = &model->hulls[3];
				else
					hull = &model->hulls[2];
			}
         }
// calculate an offset value to center the origin
		VectorSubtract (hull->clip_mins, mins, offset);
		VectorAdd (offset, ent->v.origin, offset);
	}
	else
	{	// create a temp hull from bounding box sizes

		VectorSubtract (ent->v.mins, maxs, hullmins);
		VectorSubtract (ent->v.maxs, mins, hullmaxs);
		hull = SV_HullForBox (hullmins, hullmaxs);

		VectorCopy (ent->v.origin, offset);
	}


	return hull;
}

/*
===============================================================================

ENTITY AREA CHECKING

===============================================================================
*/

typedef struct areanode_s
{
	int		axis;		// -1 = leaf node
	float	dist;
	struct areanode_s	*children[2];
	link_t	trigger_edicts;
	link_t	solid_edicts;
} areanode_t;

#define	AREA_DEPTH	4
#define	AREA_NODES	32

static	areanode_t	sv_areanodes[AREA_NODES];
static	int			sv_numareanodes;

/*
===============
SV_CreateAreaNode

===============
*/
areanode_t *SV_CreateAreaNode (int depth, vec3_t mins, vec3_t maxs)
{
	areanode_t	*anode;
	vec3_t		size;
	vec3_t		mins1, maxs1, mins2, maxs2;

	anode = &sv_areanodes[sv_numareanodes];
	sv_numareanodes++;

	ClearLink (&anode->trigger_edicts);
	ClearLink (&anode->solid_edicts);

	if (depth == AREA_DEPTH)
	{
		anode->axis = -1;
		anode->children[0] = anode->children[1] = NULL;
		return anode;
	}

	VectorSubtract (maxs, mins, size);
	if (size[0] > size[1])
		anode->axis = 0;
	else
		anode->axis = 1;

	anode->dist = 0.5f * (maxs[anode->axis] + mins[anode->axis]);
	VectorCopy (mins, mins1);
	VectorCopy (mins, mins2);
	VectorCopy (maxs, maxs1);
	VectorCopy (maxs, maxs2);

	maxs1[anode->axis] = mins2[anode->axis] = anode->dist;

	anode->children[0] = SV_CreateAreaNode (depth+1, mins2, maxs2);
	anode->children[1] = SV_CreateAreaNode (depth+1, mins1, maxs1);

	return anode;
}

/*
===============
SV_ClearWorld

===============
*/
void SV_ClearWorld (void)
{
	SV_InitBoxHull ();

	memset (sv_areanodes, 0, sizeof(sv_areanodes));
	sv_numareanodes = 0;
	SV_CreateAreaNode (0, sv.worldmodel->mins, sv.worldmodel->maxs);
}


/*
===============
SV_UnlinkEdict

===============
*/
void SV_UnlinkEdict (edict_t *ent)
{
	if (!ent->area.prev)
		return;		// not linked in anywhere
	RemoveLink (&ent->area);
	ent->area.prev = ent->area.next = NULL;
}

/*
====================
SV_AreaTriggerEdicts

Spike -- just builds a list of entities within the area, rather than walking
them and risking the list getting corrupt.
====================
*/
static void
SV_AreaTriggerEdicts ( edict_t *ent, areanode_t *node, edict_t **list, int *listcount, const int listspace )
{
	link_t		*l, *next;
	edict_t		*touch;

// touch linked edicts
	for (l = node->trigger_edicts.next ; l != &node->trigger_edicts ; l = next)
	{
		next = l->next;
		touch = EDICT_FROM_AREA(l);
		if (touch == ent)
			continue;
		if (!touch->v.touch || touch->v.solid != SOLID_TRIGGER)
			continue;
		if (ent->v.absmin[0] > touch->v.absmax[0]
		|| ent->v.absmin[1] > touch->v.absmax[1]
		|| ent->v.absmin[2] > touch->v.absmax[2]
		|| ent->v.absmax[0] < touch->v.absmin[0]
		|| ent->v.absmax[1] < touch->v.absmin[1]
		|| ent->v.absmax[2] < touch->v.absmin[2] )
			continue;

		if (*listcount == listspace)
			return; // should never happen

		list[*listcount] = touch;
		(*listcount)++;
	}

// recurse down both sides
	if (node->axis == -1)
		return;

	if ( ent->v.absmax[node->axis] > node->dist )
		SV_AreaTriggerEdicts ( ent, node->children[0], list, listcount, listspace );
	if ( ent->v.absmin[node->axis] < node->dist )
		SV_AreaTriggerEdicts ( ent, node->children[1], list, listcount, listspace );
}

/*
====================
SV_TouchLinks

ericw -- copy the touching edicts to an array (on the hunk) so we can avoid
iteating the trigger_edicts linked list while calling PR_ExecuteProgram
which could potentially corrupt the list while it's being iterated.
Based on code from Spike.
====================
*/
void SV_TouchLinks (edict_t *ent)
{
	edict_t		**list;
	edict_t		*touch;
	int		old_self, old_other;
	int		i, listcount;
	int		mark;
	
	mark = Hunk_LowMark ();
	list = (edict_t **) Hunk_Alloc (sv.num_edicts*sizeof(edict_t *));
	
	listcount = 0;
	SV_AreaTriggerEdicts (ent, sv_areanodes, list, &listcount, sv.num_edicts);

	for (i = 0; i < listcount; i++)
	{
		touch = list[i];
	// re-validate in case of PR_ExecuteProgram having side effects that make
	// edicts later in the list no longer touch
		if (touch == ent)
			continue;
		if (!touch->v.touch || touch->v.solid != SOLID_TRIGGER)
			continue;
		if (ent->v.absmin[0] > touch->v.absmax[0]
		|| ent->v.absmin[1] > touch->v.absmax[1]
		|| ent->v.absmin[2] > touch->v.absmax[2]
		|| ent->v.absmax[0] < touch->v.absmin[0]
		|| ent->v.absmax[1] < touch->v.absmin[1]
		|| ent->v.absmax[2] < touch->v.absmin[2] )
			continue;
		old_self = pr_global_struct->self;
		old_other = pr_global_struct->other;

		pr_global_struct->self = EDICT_TO_PROG(touch);
		pr_global_struct->other = EDICT_TO_PROG(ent);
		pr_global_struct->time = sv.time;
		PR_ExecuteProgram (touch->v.touch);

		pr_global_struct->self = old_self;
		pr_global_struct->other = old_other;
	}

// free hunk-allocated edicts array
	Hunk_FreeToLowMark (mark);
}


/*
===============
SV_FindTouchedLeafs

===============
*/
void SV_FindTouchedLeafs (edict_t *ent, mnode_t *node)
{
	mplane_t	*splitplane;
	mleaf_t		*leaf;
	int			sides;
	int			leafnum;

	if (node->contents == CONTENTS_SOLID)
		return;

// add an efrag if the node is a leaf

	if ( node->contents < 0)
	{
		if (ent->num_leafs == MAX_ENT_LEAFS)
			return;

		leaf = (mleaf_t *)node;
		leafnum = leaf - sv.worldmodel->leafs - 1;

		ent->leafnums[ent->num_leafs] = leafnum;
		ent->num_leafs++;
		return;
	}

// NODE_MIXED

	splitplane = node->plane;
	sides = BOX_ON_PLANE_SIDE(ent->v.absmin, ent->v.absmax, splitplane);

// recurse down the contacted sides
	if (sides & 1)
		SV_FindTouchedLeafs (ent, node->children[0]);

	if (sides & 2)
		SV_FindTouchedLeafs (ent, node->children[1]);
}

/*
===============
SV_LinkEdict

===============
*/
void SV_LinkEdict (edict_t *ent, qboolean touch_triggers)
{
	areanode_t	*node;

	if (ent->area.prev)
		SV_UnlinkEdict (ent);	// unlink from old position

	if (ent == sv.edicts)
		return;		// don't add the world

	if (ent->free)
		return;

// set the abs box
	VectorAdd (ent->v.origin, ent->v.mins, ent->v.absmin);
	VectorAdd (ent->v.origin, ent->v.maxs, ent->v.absmax);

//
// to make items easier to pick up and allow them to be grabbed off
// of shelves, the abs sizes are expanded
//
	if ((int)ent->v.flags & FL_ITEM)
	{
		ent->v.absmin[0] -= 15;
		ent->v.absmin[1] -= 15;
		ent->v.absmax[0] += 15;
		ent->v.absmax[1] += 15;
	}
	else
	{	// because movement is clipped an epsilon away from an actual edge,
		// we must fully check even when bounding boxes don't quite touch
		ent->v.absmin[0] -= 1;
		ent->v.absmin[1] -= 1;
		ent->v.absmin[2] -= 1;
		ent->v.absmax[0] += 1;
		ent->v.absmax[1] += 1;
		ent->v.absmax[2] += 1;
	}

// link to PVS leafs
	ent->num_leafs = 0;
	if (ent->v.modelindex)
		SV_FindTouchedLeafs (ent, sv.worldmodel->nodes);

	if (ent->v.solid == SOLID_NOT)
		return;

// find the first node that the ent's box crosses
	node = sv_areanodes;
	while (1)
	{
		if (node->axis == -1)
			break;
		if (ent->v.absmin[node->axis] > node->dist)
			node = node->children[0];
		else if (ent->v.absmax[node->axis] < node->dist)
			node = node->children[1];
		else
			break;		// crosses the node
	}

// link it in

	if (ent->v.solid == SOLID_TRIGGER)
		InsertLinkBefore (&ent->area, &node->trigger_edicts);
	else
		InsertLinkBefore (&ent->area, &node->solid_edicts);

// if touch_triggers, touch all entities at this node and decend for more
	if (touch_triggers)
		SV_TouchLinks ( ent );
}



/*
===============================================================================

POINT TESTING IN HULLS

===============================================================================
*/

#if	!id386

/*
==================
SV_HullPointContents

==================
*/
int SV_HullPointContents (hull_t *hull, int num, vec3_t p)
{
	float		d;
	dclipnode_t	*node;
	mplane_t	*plane;

	while (num >= 0)
	{
		if (num < hull->firstclipnode || num > hull->lastclipnode)
			Sys_Error ("bad node number");

		node = hull->clipnodes + num;
		plane = hull->planes + node->planenum;

		if (plane->type < 3)
			d = p[plane->type] - plane->dist;
		else
			d = DotProduct (plane->normal, p) - plane->dist;
		if (d < 0)
			num = node->children[1];
		else
			num = node->children[0];
	}

	return num;
}

#endif	// !id386


/*
==================
SV_PointContents

==================
*/
int SV_PointContents (vec3_t p)
{
	int		cont;

	cont = SV_HullPointContents (&sv.worldmodel->hulls[0], 0, p);
	if (cont <= CONTENTS_CURRENT_0 && cont >= CONTENTS_CURRENT_DOWN)
		cont = CONTENTS_WATER;
	return cont;
}

int SV_TruePointContents (vec3_t p)
{
	return SV_HullPointContents (&sv.worldmodel->hulls[0], 0, p);
}

//===========================================================================

/*
============
SV_TestEntityPosition

This could be a lot more efficient...
============
*/
edict_t	*SV_TestEntityPosition (edict_t *ent)
{
	trace_t	trace;

	trace = SV_Move (ent->v.origin, ent->v.mins, ent->v.maxs, ent->v.origin, 0, ent);

	if (trace.startsolid)
		return sv.edicts;

	return NULL;
}


/*
===============================================================================

LINE TESTING IN HULLS

===============================================================================
*/

enum
{
	rht_solid,
	rht_empty,
	rht_impact
};
struct rhtctx_s
{
	unsigned int hitcontents;
	vec3_t start, end;
	dclipnode_t	*clipnodes;
	mplane_t	*planes;
};

// 1/32 epsilon to keep floating point happy
#define	DIST_EPSILON	(0.03125)

/*
==================
Q1BSP_RecursiveHullTrace
This does the core traceline/tracebox logic.
This version is from FTE and attempts to be more numerically stable than vanilla.
This is achieved by recursing at the actual decision points instead of vanilla's habit of vanilla's habit of using points that are outside of the child's volume.
It also uses itself to test solidity on the other side of the node, which ensures consistent precision.
The actual collision point is (still) biased by an epsilon, so the end point shouldn't be inside walls either way.
FTE's version 'should' be more compatible with vanilla than DP's (which doesn't take care with allsolid).
ezQuake also has a version of this logic, but I trust mine more.
==================
*/
static int Q1BSP_RecursiveHullTrace (struct rhtctx_s *ctx, int num, float p1f, float p2f, vec3_t p1, vec3_t p2, trace_t *trace)
{
	dclipnode_t	*node;
	mplane_t	*plane;
	float		t1, t2;
	vec3_t		mid;
	int			side;
	float		midf;
	int rht;

reenter:

	if (num < 0)
	{
		/*hit a leaf*/
		if (num == CONTENTS_SOLID)
		{
			if (trace->allsolid)
				trace->startsolid = true;
			return rht_solid;
		}
		else
		{
			trace->allsolid = false;
			if (num == CONTENTS_EMPTY)
				trace->inopen = true;
			else if (num != CONTENTS_SOLID)
				trace->inwater = true;
			return rht_empty;
		}
	}

	/*its a node*/

	/*get the node info*/
	node = ctx->clipnodes + num;
	plane = ctx->planes + node->planenum;

	if (plane->type < 3)
	{
		t1 = p1[plane->type] - plane->dist;
		t2 = p2[plane->type] - plane->dist;
	}
	else
	{
		t1 = DoublePrecisionDotProduct (plane->normal, p1) - (double)plane->dist;
		t2 = DoublePrecisionDotProduct (plane->normal, p2) - (double)plane->dist;
	}

	/*if its completely on one side, resume on that side*/
	if (t1 >= 0 && t2 >= 0)
	{
		//return Q1BSP_RecursiveHullTrace (hull, node->children[0], p1f, p2f, p1, p2, trace);
		num = node->children[0];
		goto reenter;
	}
	if (t1 < 0 && t2 < 0)
	{
		//return Q1BSP_RecursiveHullTrace (hull, node->children[1], p1f, p2f, p1, p2, trace);
		num = node->children[1];
		goto reenter;
	}

	if (plane->type < 3)
	{
		t1 = ctx->start[plane->type] - plane->dist;
		t2 = ctx->end[plane->type] - plane->dist;
	}
	else
	{
		t1 = DotProduct (plane->normal, ctx->start) - plane->dist;
		t2 = DotProduct (plane->normal, ctx->end) - plane->dist;
	}

	side = t1 < 0;

	midf = t1 / (t1 - t2);
	if (midf < p1f) midf = p1f;
	if (midf > p2f) midf = p2f;
	VectorInterpolate(ctx->start, midf, ctx->end, mid);

	rht = Q1BSP_RecursiveHullTrace(ctx, node->children[side], p1f, midf, p1, mid, trace);
	if (rht != rht_empty && !trace->allsolid)
		return rht;
	rht = Q1BSP_RecursiveHullTrace(ctx, node->children[side^1], midf, p2f, mid, p2, trace);
	if (rht != rht_solid)
		return rht;

	if (side)
	{
		/*we impacted the back of the node, so flip the plane*/
		trace->plane.dist = -plane->dist;
		VectorNegate(plane->normal, trace->plane.normal);
		midf = (t1 + (float)DIST_EPSILON) / (t1 - t2);
	}
	else
	{
		/*we impacted the front of the node*/
		trace->plane.dist = plane->dist;
		VectorCopy(plane->normal, trace->plane.normal);
		midf = (t1 - (float)DIST_EPSILON) / (t1 - t2);
	}

	t1 = DoublePrecisionDotProduct (trace->plane.normal, ctx->start) - (double)trace->plane.dist;
	t2 = DoublePrecisionDotProduct (trace->plane.normal, ctx->end) - (double)trace->plane.dist;
	midf = (t1 - (float)DIST_EPSILON) / (t1 - t2);

	midf = CLAMP(0, midf, 1);
	trace->fraction = midf;
	VectorCopy (mid, trace->endpos);
	VectorInterpolate(ctx->start, midf, ctx->end, trace->endpos);

	return rht_impact;
}

/*
==================
SV_RecursiveHullCheck

==================
*/
qboolean SV_RecursiveHullCheck (hull_t *hull, int num, vec3_t p1, vec3_t p2, trace_t *trace)
{
	if (p1[0] == p2[0] && p1[1] == p2[1] && p1[2] == p2[2])
	{
		/*points cannot cross planes, so do it faster*/
		int c = SV_HullPointContents(hull, hull->firstclipnode, p1);
		//trace->contents = c;

		if (c != CONTENTS_SOLID)
		{
			trace->allsolid = false;
			if (num == CONTENTS_EMPTY)
				trace->inopen = true;
			else
				trace->inwater = true;
		}
		else
			trace->startsolid = true;
		return true;
	}
	else
	{
		struct rhtctx_s ctx;
		VectorCopy(p1, ctx.start);
		VectorCopy(p2, ctx.end);
		ctx.clipnodes = hull->clipnodes;
		ctx.planes = hull->planes;
		return Q1BSP_RecursiveHullTrace(&ctx, hull->firstclipnode, 0, 1, p1, p2, trace) != rht_impact;
	}
}

/*
==================
SV_WorldTransformAABB
==================
*/
void SV_WorldTransformAABB( matrix4x4 transform, const vec3_t mins, const vec3_t maxs, vec3_t outmins, vec3_t outmaxs )
{
	vec3_t	p1, p2;
	matrix4x4	itransform;
	int	i;

	if( !outmins || !outmaxs ) return;

	Matrix4x4_Invert_Simple( itransform, transform );

	outmins[0] = outmins[1] = outmins[2] =  999999;
	outmaxs[0] = outmaxs[1] = outmaxs[2] = -999999;

	// compute a full bounding box
	for( i = 0; i < 8; i++ )
	{
		p1[0] = ( i & 1 ) ? mins[0] : maxs[0];
		p1[1] = ( i & 2 ) ? mins[1] : maxs[1];
		p1[2] = ( i & 4 ) ? mins[2] : maxs[2];

		p2[0] = DotProduct( p1, itransform[0] );
		p2[1] = DotProduct( p1, itransform[1] );
		p2[2] = DotProduct( p1, itransform[2] );

		if( p2[0] < outmins[0] ) outmins[0] = p2[0];
		if( p2[0] > outmaxs[0] ) outmaxs[0] = p2[0];
		if( p2[1] < outmins[1] ) outmins[1] = p2[1];
		if( p2[1] > outmaxs[1] ) outmaxs[1] = p2[1];
		if( p2[2] < outmins[2] ) outmins[2] = p2[2];
		if( p2[2] > outmaxs[2] ) outmaxs[2] = p2[2];
	}

	// sanity check
	for( i = 0; i < 3; i++ )
	{
		if( outmins[i] > outmaxs[i] )
		{
			Sys_Error("backwards mins/maxs\n");
			outmins[0] = outmins[1] = outmins[2] = 0;
			outmaxs[0] = outmaxs[1] = outmaxs[2] = 0;
			return;
		}
	}
}


/*
==================
SV_ClipMoveToEntity

Handles selection or creation of a clipping hull, and offseting (and
eventually rotation) of the end points
==================
*/
trace_t SV_ClipMoveToEntity (edict_t *ent, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, edict_t *move_ent )
{
	trace_t		trace;
	matrix4x4	matrix;
	vec3_t		offset, temp;
	vec3_t		start_l, end_l;
	hull_t		*hull;
	int         j;
    qboolean    transform_bbox = true;

// fill in a default trace
	memset (&trace, 0, sizeof(trace_t));
	VectorCopy (end, trace.endpos);
	trace.fraction = 1;
	trace.allsolid = true;

// get the clipping hull
	hull = SV_HullForEntity (ent, mins, maxs, offset, move_ent);

	// keep untransformed bbox less than 45 degress or train on subtransit.bsp will stop working

	if(( check_angles( ent->v.angles[0] ) || check_angles( ent->v.angles[2] )) && (mins[0] || mins[1] || mins[2]))
		transform_bbox = true;
	else
	    transform_bbox = false;

	// rotate start and end into the models frame of reference
	if (ent->v.solid == SOLID_BSP && (ent->v.angles[0] || ent->v.angles[1] || ent->v.angles[2]))
	{
		vec3_t	out_mins, out_maxs;

		if( transform_bbox )
			Matrix4x4_CreateFromEntity( matrix, ent->v.angles, ent->v.origin, 1.0f );
		else
		    Matrix4x4_CreateFromEntity( matrix, ent->v.angles, offset, 1.0f );

		Matrix4x4_VectorITransform( matrix, start, start_l );
		Matrix4x4_VectorITransform( matrix, end, end_l );

		if( transform_bbox )
		{
			SV_WorldTransformAABB( matrix, mins, maxs, out_mins, out_maxs );
			VectorSubtract( hull->clip_mins, out_mins, offset ); // calc new local offset

			for( j = 0; j < 3; j++ )
			{
				if( start_l[j] >= 0.0f )
					start_l[j] -= offset[j];
				else start_l[j] += offset[j];
				if( end_l[j] >= 0.0f )
					end_l[j] -= offset[j];
				else end_l[j] += offset[j];
			}
		}
	}
	else
	{
	    VectorSubtract (start, offset, start_l);
	    VectorSubtract (end, offset, end_l);
	}


	// trace a line through the apropriate clipping hull
	//(hull_t *hull, int num, vec3_t p1, vec3_t p2, trace_t *trace)
	SV_RecursiveHullCheck (hull, hull->firstclipnode, start_l, end_l, &trace);

	if( trace.fraction != 1.0f )
	{
		// compute endpos (generic case)
		VectorLerp( start, trace.fraction, end, trace.endpos );

		if(ent->v.solid == SOLID_BSP && (ent->v.angles[0] || ent->v.angles[1] || ent->v.angles[2]))
		{
			// transform plane
			VectorCopy( trace.plane.normal, temp );
			Matrix4x4_TransformPositivePlane( matrix, temp, trace.plane.dist, trace.plane.normal, &trace.plane.dist );
		}
		else
		{
			trace.plane.dist = DotProduct( trace.endpos, trace.plane.normal );
		}
	}

	if( trace.fraction < 1.0f || trace.startsolid )
		trace.ent = ent;

	return trace;
}

//===========================================================================

/*
====================
SV_ClipToLinks

Mins and maxs enclose the entire area swept by the move
====================
*/
void SV_ClipToLinks ( areanode_t *node, moveclip_t *clip )
{
	link_t		*l, *next;
	edict_t		*touch;
	trace_t		trace;

// touch linked edicts
	for (l = node->solid_edicts.next ; l != &node->solid_edicts ; l = next)
	{
		next = l->next;
		touch = EDICT_FROM_AREA(l);
		if (touch->v.solid == SOLID_NOT)
			continue;
		if (touch == clip->passedict)
			continue;
		if (touch->v.solid == SOLID_TRIGGER)
			Sys_Error ("Trigger in clipping list");

		if (clip->type == MOVE_NOMONSTERS && touch->v.solid != SOLID_BSP)
			continue;

		if (clip->boxmins[0] > touch->v.absmax[0]
		|| clip->boxmins[1] > touch->v.absmax[1]
		|| clip->boxmins[2] > touch->v.absmax[2]
		|| clip->boxmaxs[0] < touch->v.absmin[0]
		|| clip->boxmaxs[1] < touch->v.absmin[1]
		|| clip->boxmaxs[2] < touch->v.absmin[2] )
			continue;

		if (clip->passedict && clip->passedict->v.size[0] && !touch->v.size[0])
			continue;	// points never interact

	// might intersect, so do an exact clip
		if (clip->trace.allsolid)
			return;
		if (clip->passedict)
		{
		 	if (PROG_TO_EDICT(touch->v.owner) == clip->passedict)
				continue;	// don't clip against own missiles
			if (PROG_TO_EDICT(clip->passedict->v.owner) == touch)
				continue;	// don't clip against owner
		}

		if ((int)touch->v.flags & FL_MONSTER)
			trace = SV_ClipMoveToEntity (touch, clip->start, clip->mins2, clip->maxs2, clip->end, touch);
		else
			trace = SV_ClipMoveToEntity (touch, clip->start, clip->mins, clip->maxs, clip->end, touch);

		if (trace.allsolid || trace.startsolid ||
		trace.fraction < clip->trace.fraction)
		{
			trace.ent = touch;
			if (clip->trace.startsolid)
			{
				clip->trace = trace;
				clip->trace.startsolid = true;
			}
			else
				clip->trace = trace;
		}
		else if (trace.startsolid)
			clip->trace.startsolid = true;
	}

// recurse down both sides
	if (node->axis == -1)
		return;

	if ( clip->boxmaxs[node->axis] > node->dist )
		SV_ClipToLinks ( node->children[0], clip );
	if ( clip->boxmins[node->axis] < node->dist )
		SV_ClipToLinks ( node->children[1], clip );
}


/*
==================
SV_MoveBounds
==================
*/
void SV_MoveBounds (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, vec3_t boxmins, vec3_t boxmaxs)
{
#if 0
// debug to test against everything
boxmins[0] = boxmins[1] = boxmins[2] = -9999;
boxmaxs[0] = boxmaxs[1] = boxmaxs[2] = 9999;
#else
	int		i;

	for (i=0 ; i<3 ; i++)
	{
		if (end[i] > start[i])
		{
			boxmins[i] = start[i] + mins[i] - 1;
			boxmaxs[i] = end[i] + maxs[i] + 1;
		}
		else
		{
			boxmins[i] = end[i] + mins[i] - 1;
			boxmaxs[i] = start[i] + maxs[i] + 1;
		}
	}
#endif
}

/*
==================
SV_Move
==================
*/
trace_t SV_Move (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int type, edict_t *passedict)
{
	moveclip_t	clip;
	int			i;

	memset ( &clip, 0, sizeof ( moveclip_t ) );

	clip.start = start;
	clip.end = end;
	clip.mins = mins;
	clip.maxs = maxs;
	clip.type = type;
	clip.passedict = passedict;

// clip to world
	clip.trace = SV_ClipMoveToEntity( sv.edicts, start, mins, maxs, end, passedict);

	if (type == MOVE_MISSILE)
	{
		for (i=0 ; i<3 ; i++)
		{
			clip.mins2[i] = -15;
			clip.maxs2[i] = 15;
		}
	}
	else
	{
		VectorCopy (mins, clip.mins2);
		VectorCopy (maxs, clip.maxs2);
	}

// create the bounding box of the entire move
	SV_MoveBounds ( start, clip.mins2, clip.maxs2, end, clip.boxmins, clip.boxmaxs );

// clip to entities
	SV_ClipToLinks ( sv_areanodes, &clip );

	return clip.trace;
}

