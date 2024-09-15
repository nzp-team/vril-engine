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

#include "quakedef.h"

#ifdef _3DS
extern bool new3ds_flag;
#elif __WII__
#include <ogc/lwp_watchdog.h>
#include <wiiuse/wpad.h>
#include <ctype.h>
#endif // _3DS, __WII__

#define PR_MAX_TEMPSTRING 2048	// 2001-10-25 Enhanced temp string handling by Maddes
#define	RETURN_EDICT(e) (((int *)pr_globals)[OFS_RETURN] = EDICT_TO_PROG(e))

/*
===============================================================================
						BUILT-IN FUNCTIONS
===============================================================================
*/
char	pr_varstring_temp[PR_MAX_TEMPSTRING];	// 2001-10-25 Enhanced temp string handling by Maddes
char *PF_VarString (int	first)
{
	int		i;
// 2001-10-25 Enhanced temp string handling by Maddes  start
	int		maxlen;
	char	*add;

	pr_varstring_temp[0] = 0;
	for (i=first ; i < pr_argc ; i++)
	{
		maxlen = PR_MAX_TEMPSTRING - strlen(pr_varstring_temp) - 1;	// -1 is EndOfString
		add = G_STRING((OFS_PARM0+i*3));
		if (maxlen > strlen(add))
		{
			strcat (pr_varstring_temp, add);
		}
		else
		{
			strncat (pr_varstring_temp, add, maxlen);
			pr_varstring_temp[PR_MAX_TEMPSTRING-1] = 0;
			break;	// can stop here
		}
	}
	return pr_varstring_temp;
// 2001-10-25 Enhanced temp string handling by Maddes  end
}


/*
=================
PF_errror

This is a TERMINAL error, which will kill off the entire server.
Dumps self.

error(value)
=================
*/
void PF_error (void)
{
	char	*s;
	edict_t	*ed;

	s = PF_VarString(0);
	Con_Printf ("======SERVER ERROR in %s:\n%s\n"
	,pr_strings + pr_xfunction->s_name,s);
	ed = PROG_TO_EDICT(pr_global_struct->self);
	ED_Print (ed);

	Host_Error ("Program error");
}

/*
=================
PF_objerror

Dumps out self, then an error message.  The program is aborted and self is
removed, but the level can continue.

objerror(value)
=================
*/
void PF_objerror (void)
{
	char	*s;
	edict_t	*ed;

	s = PF_VarString(0);
	Con_Printf ("======OBJECT ERROR in %s:\n%s\n"
	,pr_strings + pr_xfunction->s_name,s);
	ed = PROG_TO_EDICT(pr_global_struct->self);
	ED_Print (ed);
	ED_Free (ed);

	Host_Error ("Program error");
}



/*
==============
PF_makevectors

Writes new values for v_forward, v_up, and v_right based on angles
makevectors(vector)
==============
*/
void PF_makevectors (void)
{
	AngleVectors (G_VECTOR(OFS_PARM0), pr_global_struct->v_forward, pr_global_struct->v_right, pr_global_struct->v_up);
}

/*
=================
PF_setorigin

This is the only valid way to move an object without using the physics of the world (setting velocity and waiting).  Directly changing origin will not set internal links correctly, so clipping would be messed up.  This should be called when an object is spawned, and then only if it is teleported.

setorigin (entity, origin)
=================
*/
void PF_setorigin (void)
{
	edict_t	*e;
	float	*org;

	e = G_EDICT(OFS_PARM0);
	org = G_VECTOR(OFS_PARM1);
	VectorCopy (org, e->v.origin);
	SV_LinkEdict (e, false);
}


void SetMinMaxSize (edict_t *e, float *min, float *max, qboolean rotate)
{
	float	*angles;
	vec3_t	rmin, rmax;
	float	bounds[2][3];
	float	xvector[2], yvector[2];
	float	a;
	vec3_t	base, transformed;
	int		i, j, k, l;

	for (i=0 ; i<3 ; i++)
		if (min[i] > max[i])
			PR_RunError ("backwards mins/maxs");

	rotate = false;		// FIXME: implement rotation properly again

	if (!rotate)
	{
		VectorCopy (min, rmin);
		VectorCopy (max, rmax);
	}
	else
	{
	// find min / max for rotations
		angles = e->v.angles;

		a = angles[1]/180 * M_PI;

		xvector[0] = cosf(a);
		xvector[1] = sinf(a);
		yvector[0] = -sinf(a);
		yvector[1] = cosf(a);

		VectorCopy (min, bounds[0]);
		VectorCopy (max, bounds[1]);

		rmin[0] = rmin[1] = rmin[2] = 9999;
		rmax[0] = rmax[1] = rmax[2] = -9999;

		for (i=0 ; i<= 1 ; i++)
		{
			base[0] = bounds[i][0];
			for (j=0 ; j<= 1 ; j++)
			{
				base[1] = bounds[j][1];
				for (k=0 ; k<= 1 ; k++)
				{
					base[2] = bounds[k][2];

				// transform the point
					transformed[0] = xvector[0]*base[0] + yvector[0]*base[1];
					transformed[1] = xvector[1]*base[0] + yvector[1]*base[1];
					transformed[2] = base[2];

					for (l=0 ; l<3 ; l++)
					{
						if (transformed[l] < rmin[l])
							rmin[l] = transformed[l];
						if (transformed[l] > rmax[l])
							rmax[l] = transformed[l];
					}
				}
			}
		}
	}

// set derived values
	VectorCopy (rmin, e->v.mins);
	VectorCopy (rmax, e->v.maxs);
	VectorSubtract (max, min, e->v.size);

	SV_LinkEdict (e, false);
}

/*
=================
PF_setsize

the size box is rotated by the current angle

setsize (entity, minvector, maxvector)
=================
*/
void PF_setsize (void)
{
	edict_t	*e;
	float	*min, *max;

	e = G_EDICT(OFS_PARM0);
	min = G_VECTOR(OFS_PARM1);
	max = G_VECTOR(OFS_PARM2);
	SetMinMaxSize (e, min, max, false);
}


/*
=================
PF_setmodel

setmodel(entity, model)
=================
*/
void PF_setmodel (void)
{
	edict_t	*e;
	char	*m, **check;
	model_t	*mod;
	int		i;

	e = G_EDICT(OFS_PARM0);
	m = G_STRING(OFS_PARM1);

// check to see if model was properly precached
	for (i=0, check = sv.model_precache ; *check ; i++, check++)
		if (!strcmp(*check, m))
			break;

	if (!*check)
		PR_RunError ("no precache: %s\n", m);


	e->v.model = m - pr_strings;
	e->v.modelindex = i; //SV_ModelIndex (m);

	mod = sv.models[ (int)e->v.modelindex];  // Mod_ForName (m, true);

	if (mod)
		SetMinMaxSize (e, mod->mins, mod->maxs, true);
	else
		SetMinMaxSize (e, vec3_origin, vec3_origin, true);
}

/*
=================
PF_bprint
broadcast print to everyone on server
bprint(style, value)
=================
*/
void PF_bprint (void)
{
	// 
	float style = G_FLOAT(OFS_PARM0);
	char *s = PF_VarString(1);
	SV_BroadcastPrintf ("%s", s);
}

/*
=================
PF_sprint

single print to a specific client

sprint(clientent, value)
=================
*/
void PF_sprint (void)
{
	char		*s;
	client_t	*client;
	int			entnum;

	entnum = G_EDICTNUM(OFS_PARM0);
	s = PF_VarString(1);

	if (entnum < 1 || entnum > svs.maxclients)
	{
		Con_Printf ("tried to sprint to a non-client\n");
		return;
	}

	client = &svs.clients[entnum-1];

	MSG_WriteChar (&client->message,svc_print);
	MSG_WriteString (&client->message, s );
}


/*
=================
PF_centerprint

single print to a specific client

centerprint(clientent, value)
=================
*/
void PF_centerprint (void)
{
	char		*s;
	client_t	*client;
	int			entnum;

	entnum = G_EDICTNUM(OFS_PARM0);
	s = PF_VarString(1);

	if (entnum < 1 || entnum > svs.maxclients)
	{
		Con_Printf ("tried to sprint to a non-client\n");
		return;
	}

	client = &svs.clients[entnum-1];

	MSG_WriteChar (&client->message,svc_centerprint);
	MSG_WriteString (&client->message, s );
}

/*
=================
PF_useprint

Print a text depending on what it is fed with

useprint(entity client, float type, float cost, float weapon)
=================
*/
void PF_useprint (void)
{
	client_t	*client;
	int			entnum, type, cost, weapon;

	entnum = G_EDICTNUM(OFS_PARM0);
	type = G_FLOAT(OFS_PARM1);
	cost = G_FLOAT(OFS_PARM2);
	weapon = G_FLOAT(OFS_PARM3);


	if (entnum < 1 || entnum > svs.maxclients)
	{
		Con_Printf ("tried to sprint to a non-client\n");
		return;
	}

	client = &svs.clients[entnum-1];

	MSG_WriteByte (&client->message,svc_useprint);
	MSG_WriteByte (&client->message,type);
	MSG_WriteShort (&client->message,cost);
	MSG_WriteByte (&client->message,weapon);
	//MSG_WriteString (&client->message, s );
}


/*
=================
PF_normalize

vector normalize(vector)
=================
*/
void PF_normalize (void)
{
	float	*value1;
	vec3_t	newvalue;
	float	new;

	value1 = G_VECTOR(OFS_PARM0);

	new = value1[0] * value1[0] + value1[1] * value1[1] + value1[2]*value1[2];
	new = sqrtf(new);

	if (new == 0)
		newvalue[0] = newvalue[1] = newvalue[2] = 0;
	else
	{
		new = 1/new;
		newvalue[0] = value1[0] * new;
		newvalue[1] = value1[1] * new;
		newvalue[2] = value1[2] * new;
	}

	VectorCopy (newvalue, G_VECTOR(OFS_RETURN));
}

/*
=================
PF_vlen

scalar vlen(vector)
=================
*/
void PF_vlen (void)
{
	float	*value1;
	float	new;

	value1 = G_VECTOR(OFS_PARM0);

	new = value1[0] * value1[0] + value1[1] * value1[1] + value1[2]*value1[2];
	new = sqrtf(new);

	G_FLOAT(OFS_RETURN) = new;
}

/*
=================
PF_vectoyaw

float vectoyaw(vector)
=================
*/
void PF_vectoyaw (void)
{
	float	*value1;
	float	yaw;

	value1 = G_VECTOR(OFS_PARM0);

	if (value1[1] == 0 && value1[0] == 0)
		yaw = 0;
	else
	{
		yaw = (int) (atan2f(value1[1], value1[0]) * 180 / M_PI);
		if (yaw < 0)
			yaw += 360;
	}

	G_FLOAT(OFS_RETURN) = yaw;
}


/*
=================
PF_vectoangles

vector vectoangles(vector)
=================
*/
void PF_vectoangles (void)
{
	float	*value1;
	float	forward;
	float	yaw, pitch;

	value1 = G_VECTOR(OFS_PARM0);

	if (value1[1] == 0 && value1[0] == 0)
	{
		yaw = 0;
		if (value1[2] > 0)
			pitch = 90;
		else
			pitch = 270;
	}
	else
	{
		yaw = (int) (atan2f(value1[1], value1[0]) * 180 / M_PI);
		if (yaw < 0)
			yaw += 360;

		forward = sqrtf (value1[0]*value1[0] + value1[1]*value1[1]);
		pitch = (int) (atan2f(value1[2], forward) * 180 / M_PI);
		if (pitch < 0)
			pitch += 360;
	}

	G_FLOAT(OFS_RETURN+0) = pitch;
	G_FLOAT(OFS_RETURN+1) = yaw;
	G_FLOAT(OFS_RETURN+2) = 0;
}

/*
=================
PF_Random

Returns a number from 0<= num < 1

random()
=================
*/
void PF_random (void)
{
	float		num;

	num = (rand ()&0x7fff) / ((float)0x7fff);

	G_FLOAT(OFS_RETURN) = num;
}

/*
=================
PF_particle

particle(origin, color, count)
=================
*/
void PF_particle (void)
{
	float		*org, *dir;
	float		color;
	float		count;

	org = G_VECTOR(OFS_PARM0);
	dir = G_VECTOR(OFS_PARM1);
	color = G_FLOAT(OFS_PARM2);
	count = G_FLOAT(OFS_PARM3);
	SV_StartParticle (org, dir, color, count);
}


/*
=================
PF_ambientsound

=================
*/
void PF_ambientsound (void)
{
	char		**check;
	char		*samp;
	float		*pos;
	float 		vol, attenuation;
	int			i, soundnum;

	pos = G_VECTOR (OFS_PARM0);
	samp = G_STRING(OFS_PARM1);
	vol = G_FLOAT(OFS_PARM2);
	attenuation = G_FLOAT(OFS_PARM3);

// check to see if samp was properly precached
	for (soundnum=0, check = sv.sound_precache ; *check ; check++, soundnum++)
		if (!strcmp(*check,samp))
			break;

	if (!*check)
	{
		Con_Printf ("no precache: %s\n", samp);
		return;
	}

// add an svc_spawnambient command to the level signon packet

	MSG_WriteByte (&sv.signon,svc_spawnstaticsound);
	for (i=0 ; i<3 ; i++)
		MSG_WriteCoord(&sv.signon, pos[i]);

	MSG_WriteByte (&sv.signon, soundnum);

	MSG_WriteByte (&sv.signon, vol*255);
	MSG_WriteByte (&sv.signon, attenuation*64);

}

/*
=================
PF_sound

Each entity can have eight independant sound sources, like voice,
weapon, feet, etc.

Channel 0 is an auto-allocate channel, the others override anything
allready running on that entity/channel pair.

An attenuation of 0 will play full volume everywhere in the level.
Larger attenuations will drop off.

=================
*/
void PF_sound (void)
{
	char		*sample;
	int			channel;
	edict_t		*entity;
	int 		volume;
	float attenuation;

	entity = G_EDICT(OFS_PARM0);
	channel = G_FLOAT(OFS_PARM1);
	sample = G_STRING(OFS_PARM2);
	volume = G_FLOAT(OFS_PARM3) * 255;
	attenuation = G_FLOAT(OFS_PARM4);

	if (volume < 0 || volume > 255)
		Sys_Error ("SV_StartSound: volume = %i", volume);

	if (attenuation < 0 || attenuation > 4)
		Sys_Error ("SV_StartSound: attenuation = %f", attenuation);

	if (channel < 0 || channel > 7)
		Sys_Error ("SV_StartSound: channel = %i", channel);

	SV_StartSound (entity, channel, sample, volume, attenuation);
}

/*
=================
PF_break

break()
=================
*/
void PF_break (void)
{
Con_Printf ("break statement\n");
*(int *)-4 = 0;	// dump to debugger
//	PR_RunError ("break statement");
}

/*
=================
PF_traceline

Used for use tracing and shot targeting
Traces are blocked by bbox and exact bsp entityes, and also slide box entities
if the tryents flag is set.

traceline (vector1, vector2, tryents)
=================
*/
void PF_traceline (void)
{
	float	*v1, *v2;
	trace_t	trace;
	int		nomonsters;
	edict_t	*ent;

	v1 = G_VECTOR(OFS_PARM0);
	v2 = G_VECTOR(OFS_PARM1);
	nomonsters = G_FLOAT(OFS_PARM2);
	ent = G_EDICT(OFS_PARM3);

	trace = SV_Move (v1, vec3_origin, vec3_origin, v2, nomonsters, ent);

	pr_global_struct->trace_allsolid = trace.allsolid;
	pr_global_struct->trace_startsolid = trace.startsolid;
	pr_global_struct->trace_fraction = trace.fraction;
	pr_global_struct->trace_inwater = trace.inwater;
	pr_global_struct->trace_inopen = trace.inopen;
	VectorCopy (trace.endpos, pr_global_struct->trace_endpos);
	VectorCopy (trace.plane.normal, pr_global_struct->trace_plane_normal);
	pr_global_struct->trace_plane_dist =  trace.plane.dist;
	if (trace.ent)
		pr_global_struct->trace_ent = EDICT_TO_PROG(trace.ent);
	else
		pr_global_struct->trace_ent = EDICT_TO_PROG(sv.edicts);
}


extern trace_t SV_Trace_Toss (edict_t *ent, edict_t *ignore);

void PF_TraceToss (void)
{
	trace_t	trace;
	edict_t	*ent, *ignore;

	ent = G_EDICT(OFS_PARM0);
	ignore = G_EDICT(OFS_PARM1);

	trace = SV_Trace_Toss (ent, ignore);

	pr_global_struct->trace_allsolid = trace.allsolid;
	pr_global_struct->trace_startsolid = trace.startsolid;
	pr_global_struct->trace_fraction = trace.fraction;
	pr_global_struct->trace_inwater = trace.inwater;
	pr_global_struct->trace_inopen = trace.inopen;
	VectorCopy (trace.endpos, pr_global_struct->trace_endpos);
	VectorCopy (trace.plane.normal, pr_global_struct->trace_plane_normal);
	pr_global_struct->trace_plane_dist =  trace.plane.dist;
	if (trace.ent)
		pr_global_struct->trace_ent = EDICT_TO_PROG(trace.ent);
	else
		pr_global_struct->trace_ent = EDICT_TO_PROG(sv.edicts);
}

int TraceMove(vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int type, edict_t *ent)//engine-sides
{
	if(start[0] == end[0] && start[1] == end[1] && start[2] == end[2])
	{
		return 1;
	}
	vec3_t forward, up;
	float HorDist;
	vec3_t HorGoal;
	vec3_t tempHorGoal;

	up[0] = 0; up[1] = 0; up[2] = 1;
	HorGoal[0] = end[0]; HorGoal[1] = end[1]; HorGoal[2] = start[2];

	VectorSubtract(HorGoal,start,forward);
	HorDist = VectorLength(forward);
	VectorNormalize(forward);

	vec3_t CurrentPos;

	VectorCopy(start,CurrentPos);
	VectorCopy(HorGoal,tempHorGoal);

	float CurrentDist = 0;//2d distance from initial 3d positionvector

	trace_t trace1, trace2;
	float tempDist;
	vec3_t tempVec;
	vec3_t tempVec2;
	float i;
	int STEPSIZEB = 18;//other declaration isn't declared yet
	float SLOPELEN = 10.4;//18/tan(60) = 10.4, the the length of the triangle formed by the max walkable slope of 60 degrees.
 	int skip = 0;
	int LoopBreak = 0;

	while(CurrentDist < HorDist)
	{
		if(LoopBreak > 20)//was 50, decreased this quite a bit. now it's 260 meters
		{
			//Con_Printf("AI Warning: There is a ledge that is greater than 650 meters.\n");
			return -1;
		}

		trace1 = SV_Move(CurrentPos, mins, maxs, tempHorGoal, MOVE_NOMONSTERS, ent);

		VectorSubtract(tempHorGoal,CurrentPos,tempVec);
		tempDist = trace1.fraction * VectorLength(tempVec);
		//Check if we fell along the path
		for(i = (maxs[0] * 1); i < tempDist; i += (maxs[0] * 1))
		{
			VectorScale(forward,i,tempVec);
			VectorAdd(tempVec,CurrentPos,tempVec);
			VectorScale(up,-500,tempVec2);//500 inches is about 13 meters
			VectorAdd(tempVec,tempVec2,tempVec2);
			trace2 = SV_Move(tempVec, mins, maxs, tempVec2, MOVE_NOMONSTERS, ent);
			if(trace2.fraction > 0)
			{
				VectorScale(up,trace2.fraction * -100,tempVec2);
				VectorAdd(tempVec,tempVec2,CurrentPos);
				VectorAdd(tempHorGoal,tempVec2,tempHorGoal);
				skip = 1;
				CurrentDist += i;
				if(trace2.fraction == 1)
				{
					//We fell the full 13 meters!, we need to be careful here,
					//because if we're checking over the void, then we could be stuck in an infinite loop and crash the game
					//So we're going to keep track of how many times we fall 13 meters
					LoopBreak++;
				}
				else
				{
					LoopBreak = 0;
				}
				break;
			}
		}
		//If we fell at any location along path, then we don't try to step up
		if(skip == 1)
		{
			trace2.fraction = 0;
			skip = 0;
			continue;
		}
		//We need to advance it as much as possible along path before step up
		if(trace1.fraction > 0 && trace1.fraction < 1)
		{
			VectorCopy(trace1.endpos,CurrentPos);
			trace1.fraction = 0;
		}
		//Check step up
		if(trace1.fraction < 1)
		{
			VectorScale(up,STEPSIZEB,tempVec2);
			VectorAdd(CurrentPos,tempVec2,tempVec);
			VectorAdd(tempHorGoal,tempVec2,tempVec2);
			trace2 = SV_Move(tempVec, mins, maxs, tempVec2, MOVE_NOMONSTERS, ent);
			//10.4 is minimum length for a slope of 60 degrees, we need to at least advance this much to know the surface is walkable
			VectorSubtract(tempVec2,tempVec,tempVec2);
			if(trace2.fraction > (trace1.fraction + (SLOPELEN/VectorLength(tempVec2))) || trace2.fraction == 1)
			{
				VectorCopy(tempVec,CurrentPos);
				tempHorGoal[2] = CurrentPos[2];
				continue;
			}
			else
			{
				return 0;//stepping up didn't advance so we've hit a wall, we failed
			}
		}
		if(trace1.fraction == 1)//we've made it horizontally to our goal... so check if we've made it vertically...
		{
			if((end[2] - tempHorGoal[2] < STEPSIZEB) && (end[2] - tempHorGoal[2]) > -1 * STEPSIZEB)
				return 1;
			else return 0;
		}
	}
	return 0;
}


void PF_tracemove(void)//progs side
{
	float   *start, *end, *mins, *maxs;
	int      nomonsters;
	edict_t   *ent;

	start = G_VECTOR(OFS_PARM0);
	mins = G_VECTOR(OFS_PARM1);
	maxs = G_VECTOR(OFS_PARM2);
	end = G_VECTOR(OFS_PARM3);
	nomonsters = G_FLOAT(OFS_PARM4);
	ent = G_EDICT(OFS_PARM5);

	// Con_DPrintf ("TraceMove start, ");
	G_INT(OFS_RETURN) = TraceMove(start, mins, maxs, end,nomonsters,ent);
	// Con_DPrintf ("TM end\n");
	return;
}

void PF_tracebox (void)
{
   float   *v1, *v2, *mins, *maxs;
   trace_t   trace;
   int      nomonsters;
   edict_t   *ent;

   v1 = G_VECTOR(OFS_PARM0);
   mins = G_VECTOR(OFS_PARM1);
   maxs = G_VECTOR(OFS_PARM2);
   v2 = G_VECTOR(OFS_PARM3);
   nomonsters = G_FLOAT(OFS_PARM4);
   ent = G_EDICT(OFS_PARM5);

   trace = SV_Move (v1, mins, maxs, v2, nomonsters, ent);

   pr_global_struct->trace_allsolid = trace.allsolid;
   pr_global_struct->trace_startsolid = trace.startsolid;
   pr_global_struct->trace_fraction = trace.fraction;
   pr_global_struct->trace_inwater = trace.inwater;
   pr_global_struct->trace_inopen = trace.inopen;
   VectorCopy (trace.endpos, pr_global_struct->trace_endpos);
   VectorCopy (trace.plane.normal, pr_global_struct->trace_plane_normal);
   pr_global_struct->trace_plane_dist =  trace.plane.dist;
   if (trace.ent)
      pr_global_struct->trace_ent = EDICT_TO_PROG(trace.ent);
   else
      pr_global_struct->trace_ent = EDICT_TO_PROG(sv.edicts);
}
/*
=================
PF_checkpos

Returns true if the given entity can move to the given position from it's
current position by walking or rolling.
FIXME: make work...
scalar checkpos (entity, vector)
=================
*/
void PF_checkpos (void)
{
}

//============================================================================

byte	checkpvs[MAX_MAP_LEAFS/8];

int PF_newcheckclient (int check)
{
	int		i;
	byte	*pvs;
	edict_t	*ent;
	mleaf_t	*leaf;
	vec3_t	org;

// cycle to the next one

	if (check < 1)
		check = 1;
	if (check > svs.maxclients)
		check = svs.maxclients;

	if (check == svs.maxclients)
		i = 1;
	else
		i = check + 1;

	for ( ;  ; i++)
	{
		if (i == svs.maxclients+1)
			i = 1;

		ent = EDICT_NUM(i);

		if (i == check)
			break;	// didn't find anything else

		if (ent->free)
			continue;
		if (ent->v.health <= 0)
			continue;
		if ((int)ent->v.flags & FL_NOTARGET)
			continue;

	// anything that is a client, or has a client as an enemy
		break;
	}

// get the PVS for the entity
	VectorAdd (ent->v.origin, ent->v.view_ofs, org);
	leaf = Mod_PointInLeaf (org, sv.worldmodel);
	pvs = Mod_LeafPVS (leaf, sv.worldmodel);

#ifdef __PSP__
	memcpy_vfpu(checkpvs, pvs, (sv.worldmodel->numleafs+7)>>3 );
#else
	memcpy(checkpvs, pvs, (sv.worldmodel->numleafs+7)>>3 );
#endif // __PSP__

	return i;
}

/*
=================
PF_checkclient

Returns a client (or object that has a client enemy) that would be a
valid target.

If there are more than one valid options, they are cycled each frame

If (self.origin + self.viewofs) is not in the PVS of the current target,
it is not returned at all.

name checkclient ()
=================
*/
#define	MAX_CHECK	16
int c_invis, c_notvis;
void PF_checkclient (void)
{
	edict_t	*ent, *self;
	mleaf_t	*leaf;
	int		l;
	vec3_t	view;

// find a new check if on a new frame
	if (sv.time - sv.lastchecktime >= 0.1)
	{
		sv.lastcheck = PF_newcheckclient (sv.lastcheck);
		sv.lastchecktime = sv.time;
	}

// return check if it might be visible
	ent = EDICT_NUM(sv.lastcheck);
	if (ent->free || ent->v.health <= 0)
	{
		RETURN_EDICT(sv.edicts);
		return;
	}

// if current entity can't possibly see the check entity, return 0
	self = PROG_TO_EDICT(pr_global_struct->self);
	VectorAdd (self->v.origin, self->v.view_ofs, view);
	leaf = Mod_PointInLeaf (view, sv.worldmodel);
	l = (leaf - sv.worldmodel->leafs) - 1;
	if ( (l<0) || !(checkpvs[l>>3] & (1<<(l&7)) ) )
	{
c_notvis++;
		RETURN_EDICT(sv.edicts);
		return;
	}

// might be able to see it
c_invis++;
	RETURN_EDICT(ent);
}

//============================================================================


/*
=================
PF_stuffcmd

Sends text over to the client's execution buffer

stuffcmd (clientent, value)
=================
*/
void PF_stuffcmd (void)
{
	int		entnum;
	char	*str;
	client_t	*old;

	entnum = G_EDICTNUM(OFS_PARM0);
	if (entnum < 1 || entnum > svs.maxclients)
		PR_RunError ("Parm 0 not a client");
	str = G_STRING(OFS_PARM1);

	old = host_client;
	host_client = &svs.clients[entnum-1];
	Host_ClientCommands ("%s", str);
	host_client = old;
}

/*
=================
PF_localcmd

Sends text over to the client's execution buffer

localcmd (string)
=================
*/
void PF_localcmd (void)
{
	char	*str;

	str = G_STRING(OFS_PARM0);
	Cbuf_AddText (str);
}

/*
=================
PF_cvar

float cvar (string)
=================
*/
void PF_cvar (void)
{
	char	*str;

	str = G_STRING(OFS_PARM0);

	G_FLOAT(OFS_RETURN) = Cvar_VariableValue (str);
}

/*
=================
PF_cvar_set

float cvar (string)
=================
*/
void PF_cvar_set (void)
{
	char	*var, *val;

	var = G_STRING(OFS_PARM0);
	val = G_STRING(OFS_PARM1);

	Cvar_Set (var, val);
}

/*
=================
PF_findradius

Returns a chain of entities that have origins within a spherical area

findradius (origin, radius)
=================
*/
void PF_findradius (void)
{
	edict_t	*ent, *chain;
	float	rad;
	float	*org;
	vec3_t	eorg;
	int		i, j;

	chain = (edict_t *)sv.edicts;

	org = G_VECTOR(OFS_PARM0);
	
	rad = G_FLOAT(OFS_PARM1);
	rad *= rad;

	ent = NEXT_EDICT(sv.edicts);
	for (i=1 ; i<sv.num_edicts ; i++, ent = NEXT_EDICT(ent))
	{
		if (ent->free)
			continue;
		if (ent->v.solid == SOLID_NOT)
			continue;
		for (j=0 ; j<3 ; j++)
			eorg[j] = org[j] - (ent->v.origin[j] + (ent->v.mins[j] + ent->v.maxs[j])*0.5);

		if (DotProduct(eorg, eorg) > rad)
			continue;

		ent->v.chain = EDICT_TO_PROG(chain);
		chain = ent;
	}

	RETURN_EDICT(chain);
}


/*
=========
PF_dprint
=========
*/
void PF_dprint (void)
{
	Con_DPrintf ("%s",PF_VarString(0));
}

char	pr_string_temp[PR_MAX_TEMPSTRING];	// 2001-10-25 Enhanced temp string handling by Maddes

void PF_ftos (void)
{
	float	v;
	v = G_FLOAT(OFS_PARM0);

	if (v == (int)v)
		sprintf (pr_string_temp, "%d",(int)v);
	else
		sprintf (pr_string_temp, "%5.1f",v);
	G_INT(OFS_RETURN) = pr_string_temp - pr_strings;
}

void PF_fabs (void)
{
	float	v;
	v = G_FLOAT(OFS_PARM0);
	G_FLOAT(OFS_RETURN) = fabsf(v);
}

void PF_vtos (void)
{
	sprintf (pr_string_temp, "'%5.1f %5.1f %5.1f'", G_VECTOR(OFS_PARM0)[0], G_VECTOR(OFS_PARM0)[1], G_VECTOR(OFS_PARM0)[2]);
	G_INT(OFS_RETURN) = pr_string_temp - pr_strings;
}

void PF_etos (void)
{
	sprintf (pr_string_temp, "entity %i", G_EDICTNUM(OFS_PARM0));
	G_INT(OFS_RETURN) = pr_string_temp - pr_strings;
}


void PF_Spawn (void)
{
	edict_t	*ed;
	ed = ED_Alloc();
	RETURN_EDICT(ed);
}

void PF_Remove (void)
{
	edict_t	*ed;

	ed = G_EDICT(OFS_PARM0);
	ED_Free (ed);
}

// 2001-09-20 QuakeC string manipulation by FrikaC/Maddes  start
/*
=================
PF_strzone

string strzone (string)
=================
*/
void PF_strzone (void)
{
	char *m, *p;
	m = G_STRING(OFS_PARM0);
	p = Z_Malloc(strlen(m) + 1);
	strcpy(p, m);

	G_INT(OFS_RETURN) = p - pr_strings;
}

/*
=================
PF_strunzone

string strunzone (string)
=================
*/
void PF_strunzone (void)
{
	Z_Free(G_STRING(OFS_PARM0));
	G_INT(OFS_PARM0) = OFS_NULL; // empty the def
};

/*
=================
PF_strtrim

string strtrim (string)
=================
*/
void PF_strtrim (void)
{
	int		offset, length;
	int		maxoffset;		// 2001-10-25 Enhanced temp string handling by Maddes
	char	*str;
	char 	*end;

	str = G_STRING(OFS_PARM0);

	// figure out the new start
	while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') {
		offset++;
		str++;
	}

	// figure out the new end.
	end = str + strlen (str);
	while (end > str && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' || end[-1] == '\r'))
		end--;

	length = end - str;

	if (offset < 0)
		offset = 0;
// 2001-10-25 Enhanced temp string handling by Maddes  start
	if (length >= PR_MAX_TEMPSTRING)
		length = PR_MAX_TEMPSTRING-1;
// 2001-10-25 Enhanced temp string handling by Maddes  end
	if (length < 0)
		length = 0;

	//str += offset;
	strncpy(pr_string_temp, str, length);
	pr_string_temp[length] = 0;

	G_INT(OFS_RETURN) = pr_string_temp - pr_strings;
};

/*
=================
PF_strlen

float strlen (string)
=================
*/
void PF_strlen (void)
{
	char *p = G_STRING(OFS_PARM0);
	G_FLOAT(OFS_RETURN) = strlen(p);
}

/*
=================
PF_strcat

string strcat (string, string)
=================
*/

void PF_strcat (void)
{
	char *s1, *s2;
	int		maxlen;	// 2001-10-25 Enhanced temp string handling by Maddes

	s1 = G_STRING(OFS_PARM0);
	s2 = PF_VarString(1);

// 2001-10-25 Enhanced temp string handling by Maddes  start
	pr_string_temp[0] = 0;
	if (strlen(s1) < PR_MAX_TEMPSTRING)
	{
		strcpy(pr_string_temp, s1);
	}
	else
	{
		strncpy(pr_string_temp, s1, PR_MAX_TEMPSTRING);
		pr_string_temp[PR_MAX_TEMPSTRING-1] = 0;
	}

	maxlen = PR_MAX_TEMPSTRING - strlen(pr_string_temp) - 1;	// -1 is EndOfString
	if (maxlen > 0)
	{
		if (maxlen > strlen(s2))
		{
			strcat (pr_string_temp, s2);
		}
		else
		{
			strncat (pr_string_temp, s2, maxlen);
			pr_string_temp[PR_MAX_TEMPSTRING-1] = 0;
		}
	}
// 2001-10-25 Enhanced temp string handling by Maddes  end

	G_INT(OFS_RETURN) = pr_string_temp - pr_strings;
}

/*
=================
PF_substring

string substring (string, float, float)
=================
*/
void PF_substring (void)
{
	int		offset, length;
	int		maxoffset;		// 2001-10-25 Enhanced temp string handling by Maddes
	char	*p;

	p = G_STRING(OFS_PARM0);
	offset = (int)G_FLOAT(OFS_PARM1); // for some reason, Quake doesn't like G_INT
	length = (int)G_FLOAT(OFS_PARM2);

	// cap values
	maxoffset = strlen(p);
	if (offset > maxoffset)
	{
		offset = maxoffset;
	}
	if (offset < 0)
		offset = 0;
// 2001-10-25 Enhanced temp string handling by Maddes  start
	if (length >= PR_MAX_TEMPSTRING)
		length = PR_MAX_TEMPSTRING-1;
// 2001-10-25 Enhanced temp string handling by Maddes  end
	if (length < 0)
		length = 0;

	p += offset;
	strncpy(pr_string_temp, p, length);
	pr_string_temp[length]=0;

	G_INT(OFS_RETURN) = pr_string_temp - pr_strings;
}

/*
=================
PF_stof

float stof (string)
=================
*/
// thanks Zoid, taken from QuakeWorld
void PF_stof (void)
{
	char	*s;

	s = G_STRING(OFS_PARM0);
	G_FLOAT(OFS_RETURN) = atof(s);
}

/*
=================
PF_strtolower

string strtolower (string)
=================
*/
void PF_strtolower(void)
{
	char *s;

	s = G_STRING(OFS_PARM0);

	pr_string_temp[0] = 0;
	if (strlen(s) < PR_MAX_TEMPSTRING)
	{
		strcpy(pr_string_temp, s);
	}
	else
	{
		strncpy(pr_string_temp, s, PR_MAX_TEMPSTRING);
		pr_string_temp[PR_MAX_TEMPSTRING-1] = 0;
	}

	for(int i = 0; i < strlen(s); i++)
  		pr_string_temp[i] = tolower(pr_string_temp[i]);

	G_INT(OFS_RETURN) = pr_string_temp - pr_strings;
}

/*
=================
PF_crc16

float crc16 (float, string)
=================
*/
void PF_crc16(void)
{
	int insens = G_FLOAT(OFS_PARM0);
	char *s = G_STRING(OFS_PARM1);

	G_FLOAT(OFS_RETURN) = (unsigned short) ((insens ? CRC_Block_CaseInsensitive : CRC_Block) ((unsigned char *) s, strlen(s)));
}

/*
=================
PF_stov

vector stov (string)
=================
*/
void PF_stov (void)
{
	char *v;
	int i;
	vec3_t d;

	v = G_STRING(OFS_PARM0);

	for (i=0; i<3; i++)
	{
		while(v && (v[0] == ' ' || v[0] == '\'')) //skip unneeded data
			v++;
		d[i] = atof(v);
		while (v && v[0] != ' ') // skip to next space
			v++;
	}
	VectorCopy (d, G_VECTOR(OFS_RETURN));
}

/*
=================
PF_Set_Zombie

void Set_Zombie (entity)
=================
*/
void PF_Set_Zombie (void)
{
	//int entitynum = G_EDICT(OFS_PARM0);
}


/*
=================
Main_Waypoint functin

This is where the magic happens
=================
*/

#ifdef __PSP__
#define MaxZombies 12
#elif _3DS
#define MaxZombies 18
#elif __WII__
#define MaxZombies 24
#endif //__PSP__, _3DS, __WII__


#define WAYPOINT_SET_NONE 	0
#define WAYPOINT_SET_OPEN 	1
#define WAYPOINT_SET_CLOSED	2


char waypoint_set[MAX_WAYPOINTS]; // waypoint_set[i] contains the set identifier for the i-th waypoint
unsigned short openset_waypoints[MAX_WAYPOINTS]; // List of waypoints currently in the open set sorted by heuristic cost (index 0 contains lowest cost waypoint)
unsigned short openset_length; // Current length of the open set
zombie_ai zombie_list[MaxZombies];


//
// Debugs prints the current sorted list of waypoints in the open set
//
void sv_way_print_sorted_open_set() {
	Con_Printf("Sorted open-set F-scores: ");
	for(int i = 0; i < openset_length; i++) {
		Con_Printf("%.0f, ",waypoints[openset_waypoints[i]].f_score);
	}
	Con_Printf("\n");
}



// 
// Removes a waypoint from a set, if it belongs to it. 
//
void sv_way_remove_way_from_set(char set, int waypoint_idx) {
	// If the waypoint doesn't belong to the current set, stop
	if(waypoint_set[waypoint_idx] != set) {
		return;
	}
	// If removing from open set, also remove from open-set sorted list
	if(set == WAYPOINT_SET_OPEN) {
		for(int i = 0; i < openset_length; i++) {
			if(openset_waypoints[i] == waypoint_idx) {
				// Shift down all openset entries above this index
				for(int j = i; j < openset_length - 1; j++) {
					openset_waypoints[j] = openset_waypoints[j+1];
				}
				openset_length -= 1;
				break;
			}
		}
	}
	waypoint_set[waypoint_idx] = WAYPOINT_SET_NONE;
}


//
// Debug method to verify that `openset` and `opensetRef` remain synchronized
//
void sv_way_compare_open_set_lists() {
	// Count the number of waypoints in the open set
	int n_openset_waypoints = 0;
	for(int i = 0; i < n_waypoints; i++) {
		if(waypoint_set[i] == WAYPOINT_SET_OPEN) {
			n_openset_waypoints += 1;
		}
	}

	if(n_openset_waypoints != openset_length) {
		Con_Printf("%i%i%i\n", n_openset_waypoints, openset_length);
	}
}

//
// Adds a waypoint to a set. If adding to open-set, also adds to the binary-sorted
// list of open-set waypoints.
//
void sv_way_add_way_to_set(char set, int waypoint_idx) {
	// If waypoint already belongs to the set, stop
	if(waypoint_set[waypoint_idx] == set) {
		return;
	}

	// If waypoint belongs to another set, remove it
	if(waypoint_set[waypoint_idx] != WAYPOINT_SET_NONE) {
		sv_way_remove_way_from_set(waypoint_set[waypoint_idx], waypoint_idx);
	}

	// Special logic for waypoint open-set
	if(set == WAYPOINT_SET_OPEN) {
		int min = -1;
		int max = openset_length;
		int test;
		float way_f_score = waypoints[waypoint_idx].f_score;
		float test_f_score;

		// Binary insert into the open set
		while(max > min) {
			if(max - min == 1) {
				// Shift elements up in the sorted openset_waypoints list
				for(int i = openset_length; i > max ; i--) {
					openset_waypoints[i] = openset_waypoints[i-1];
				}
				openset_waypoints[max] = waypoint_idx;
				openset_length += 1;
				// sv_way_print_sorted_open_set(); // For debug only
				break;
			}
			test = (int)((min + max)/2);
			test_f_score = waypoints[openset_waypoints[test]].f_score;
			if(way_f_score > test_f_score) {
				min = test;
			}
			else if(way_f_score < test_f_score) {
				max = test;
			}
			else if(way_f_score == test_f_score) {
				max = test;
				min = test - 1;
			}
		}
	}

	// Assign the waypoint to the set
	waypoint_set[waypoint_idx] = set;
}

//
// Returns the waypoint with the lowest F-score from the open-set, or -1 if the open-set is empty.
//
int sv_way_get_lowest_f_score_openset_waypoint() {
	if(openset_length > 0) {
		return openset_waypoints[0];
	}
	return -1;
}

//
// Return `true` if a set contains 0 waypoints, `false` otherwise
//
qboolean sv_way_is_set_empty(char set) {
	// Special case for openset
	if(set == WAYPOINT_SET_OPEN) {
		return (openset_length == 0);
	}

	// Check if any waypoints belong to this set
	for (int i = 0; i < n_waypoints; i++) {
		if(waypoint_set[i] == set) {
			return false;
		}
	}
	return true;
}

//
// Return `true` if waypoint `waypoint_idx` belongs to set `set`
//
qboolean sv_way_in_set(char set, int waypoint_idx) {
	return (waypoint_set[waypoint_idx] == set);
}

// 
// Compute A* heuristic between two waypoints
//
float sv_way_heuristic_cost_estimate(int waypoint_idx_a, int waypoint_idx_b) {
	// Compute distance squared between:
	return VectorDistanceSquared(waypoints[waypoint_idx_a].origin, waypoints[waypoint_idx_b].origin);
}


// Global array in which to store pathfinding results
int process_list[MAX_WAYPOINTS];
int process_list_length;

// 
// Follows the path found by `Pathfind()` invocation, storing result path i global `process_list`
//
void sv_way_reconstruct_path(int start_node, int current_node) {
	process_list_length = 0;

	// loop through the waypoints on the path
	while (current_node >= 0) {
		//Con_DPrintf("\nreconstruct_path: current = %i, waypoints[current].came_from = %i\n", current, waypoints[current].came_from);
		// Add the current waypoint to the path list
		process_list[process_list_length] = current_node;
		process_list_length++;

		if (current_node == start_node) {
			break;
		}
		current_node = waypoints[current_node].came_from;
	}
}


// 
// Performs pathfinding algorithm, storing results in global 
// 
// start_way -- Start waypoint index in global waypoints array
// end_way -- End waypoint index in global waypoints array
//
int sv_way_pathfind(int start_way, int end_way) {
	int current;
	float tentative_g_score, tentative_f_score;
	int i;
	// -------------–-------------–-------------–-------------–
	// Clear the path data for all waypoints
	// -------------–-------------–-------------–-------------–
	for (i = 0; i < n_waypoints; i++) {
		waypoint_set[i] = WAYPOINT_SET_NONE;
		waypoints[i].f_score = 0;
		waypoints[i].g_score = 0;
		waypoints[i].came_from = -1;
	}
	openset_length = 0;
	// -------------–-------------–-------------–-------------–

	// Cost from start along best known path.
	waypoints[start_way].g_score = 0; 
	// Estimated total cost from start to goal through y
	waypoints[start_way].f_score = waypoints[start_way].g_score + sv_way_heuristic_cost_estimate(start_way, end_way);

	// The set of tentative nodes to be evaluated, initially containing the start node
	sv_way_add_way_to_set(WAYPOINT_SET_OPEN, start_way);
	
	while (!sv_way_is_set_empty(WAYPOINT_SET_OPEN)) {
		current = sv_way_get_lowest_f_score_openset_waypoint();

		//Con_DPrintf("Pathfind current: %i, f_score: %f, g_score: %f\n", current, waypoints[current].f_score, waypoints[current].g_score);
		if (current == end_way) {
			sv_way_reconstruct_path(start_way, end_way);
			return 1;
		}
		sv_way_remove_way_from_set(WAYPOINT_SET_OPEN, current);
		sv_way_add_way_to_set(WAYPOINT_SET_CLOSED, current);

		// Add each neighbor to the open set
		for (i = 0;i < 8; i++) {
			int neighbor_waypoint_idx = waypoints[current].target[i];

			// Skip unused neighbor slots
			if (neighbor_waypoint_idx < 0) {
				break;
			}

			// Check if waypoint is enabled (e.g. door waypoints)
			if (!waypoints[neighbor_waypoint_idx].open) {
				//if (waypoints[current].target_id[i])
					//Con_DPrintf("Pathfind for: %i, waypoints[waypoints[current].target_id[i]].open = %i, current = %i\n", waypoints[current].target_id[i], waypoints[waypoints[current].target_id[i]].open, current);
				continue;
			}

			// If this waypoint is already in the closed set, skip it
			if (sv_way_in_set(WAYPOINT_SET_CLOSED, neighbor_waypoint_idx)) {
				continue;
			}
			tentative_g_score = waypoints[current].g_score + waypoints[current].dist[i];
			tentative_f_score = tentative_g_score + sv_way_heuristic_cost_estimate(neighbor_waypoint_idx, end_way);

			if (sv_way_in_set(WAYPOINT_SET_OPEN, neighbor_waypoint_idx)) {
				if(tentative_f_score < waypoints[neighbor_waypoint_idx].f_score) {
					waypoints[neighbor_waypoint_idx].g_score = tentative_g_score;
					waypoints[neighbor_waypoint_idx].f_score = tentative_f_score;
					waypoints[neighbor_waypoint_idx].came_from = current;
					// The score has been updated, remove and re-insert into its new location in the sorted open-set
					sv_way_remove_way_from_set(WAYPOINT_SET_OPEN, neighbor_waypoint_idx);
					sv_way_add_way_to_set(WAYPOINT_SET_OPEN, neighbor_waypoint_idx);
				}
			}
			else {
				waypoints[neighbor_waypoint_idx].g_score = tentative_g_score;
				waypoints[neighbor_waypoint_idx].f_score = tentative_f_score;
				waypoints[neighbor_waypoint_idx].came_from = current;
				sv_way_add_way_to_set(WAYPOINT_SET_OPEN, neighbor_waypoint_idx);
			}
		}
	}
	return 0;
}

/*
=================
Get_Waypoint_Near

vector Get_Waypoint_Near (entity)
=================
*/

void Get_Waypoint_Near (void) {
	float best_dist;
	float dist;
	int i, best;
	trace_t   trace;
	edict_t   *ent;

	best = 0;
	Con_DPrintf("Starting Get_Waypoint_Near\n");
	ent = G_EDICT(OFS_PARM0);
	best_dist = 1000000000;
	dist = 0;

	for (i = 0; i < MAX_WAYPOINTS; i++) {
		if (waypoints[i].open) {
			dist = VecLength2(waypoints[i].origin, ent->v.origin);
			if(dist < best_dist) {
				trace = SV_Move (ent->v.origin, vec3_origin, vec3_origin, waypoints[i].origin, 1, ent);

				//Con_DPrintf("Waypoint: %i, distance: %f, fraction: %f\n", i, dist, trace.fraction);
				if (trace.fraction >= 1) {
					best_dist = dist;
					best = i;
				}
			}
		}
	}
	Con_DPrintf("'%5.1f %5.1f %5.1f', %f is %f, (%i, %i)\n", waypoints[best].origin[0],waypoints[best].origin[1], waypoints[best].origin[2], best_dist, dist, i, best);
	VectorCopy (waypoints[best].origin, G_VECTOR(OFS_RETURN));
}

/*
=================
Open_Waypoint

void Open_Waypoint (string, string, string, string, string, string, string, string)
=================
*/
void Open_Waypoint (void) {
	int i;
	char *p = G_STRING(OFS_PARM0);

	//Con_DPrintf("Open_Waypoint\n");
	for (i = 0; i < MAX_WAYPOINTS; i++) {
		//no need to open without tag
		if (waypoints[i].special[0]) {
			if (!strcmp(p, waypoints[i].special)) {
				waypoints[i].open = 1;
				//Con_DPrintf("Open_Waypoint: %i, opened\n", i);
			}
			else {	
				continue;
			}
		}
	}
	//if (t == 0)
	//{
		//Con_DPrintf("Open_Waypoint: no waypoints opened\n");
	//}
}

/*
=================
Close_Waypoint

void Close_Waypoint (string, string, string, string, string, string, string, string)

cypress - basically a carbon copy of open_waypoint lol
=================
*/
void Close_Waypoint (void) {
	int i;
	char *p = G_STRING(OFS_PARM0);

	for (i = 0; i < MAX_WAYPOINTS; i++) {
		//no need to open without tag
		if (waypoints[i].special[0]) {
			if (!strcmp(p, waypoints[i].special)) {
				waypoints[i].open = 0;
			}
			else {
				continue;
			}
		}
	}
}

/*
=================
Do_Pathfind

float Do_Pathfind (entity zombie, entity target)
=================
*/
// #define MEASURE_PF_PERF
float max_waypoint_distance = 750;
short closest_waypoints[MAX_EDICTS]; 



// 
// Returns true iff we can tracebox from (start + [0,0,ofs]) to (end + [0,0,ofs])
//

// Dynamic hull sizes for hit detection cause chaos on movement code. Treat all AI ents as same size as player hull for movement
vec3_t ai_hull_mins = {-16, -16, -36};
vec3_t ai_hull_maxs = { 16,  16,  40};

qboolean ofs_tracebox(vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int type, edict_t *ignore_ent) {
	trace_t trace;
	vec3_t start_ofs;
	vec3_t end_ofs;
	VectorCopy(start, start_ofs);
	VectorCopy(end, end_ofs);
	start_ofs[2] += 8; // Move 8qu up to work better on uneven terrain
	end_ofs[2] += 8;
	trace = SV_Move(start_ofs, mins, maxs, end_ofs, type, ignore_ent);
	return (trace.fraction >= 1);
}





//
// Returns the clsoest waypoint to an entity that the entity can walk to
// Sorts all waypoints by distance, returns first waypoint we can tracebox to
//
int get_closest_waypoint(int entnum) {
	edict_t *ent = EDICT_NUM(entnum);

	vec3_t ent_mins;
	vec3_t ent_maxs;
	// VectorMin(ent->v.mins, ai_hull_mins, ent_mins);
	// VectorMax(ent->v.maxs, ai_hull_maxs, ent_maxs);
	VectorCopy(ai_hull_mins, ent_mins);
	VectorCopy(ai_hull_maxs, ent_maxs);

	// Get all waypoint indices sorted by distance to ent
	argsort_entry_t waypoint_sort_values[MAX_WAYPOINTS];
	for(int i = 0; i < n_waypoints; i++) {
		waypoint_sort_values[i].index = i;
		waypoint_sort_values[i].value = VectorDistanceSquared(waypoints[i].origin, ent->v.origin);
	}
	qsort(waypoint_sort_values, n_waypoints, sizeof(argsort_entry_t), argsort_comparator);

	

	int best_waypoint_idx = -1;
	// Sweep through waypoints from closest to farthest, stop when we can tracebox to one
	for(int i = 0; i < n_waypoints; i++) {
		int waypoint_idx = waypoint_sort_values[i].index;

		if(ofs_tracebox(ent->v.origin, ent_mins, ent_maxs, waypoints[waypoint_idx].origin, MOVE_NOMONSTERS, ent)) {
			best_waypoint_idx = waypoint_idx;
			break;
		}
	}

	return best_waypoint_idx;
}



void Do_Pathfind (void) {
	#ifdef MEASURE_PF_PERF
	u64 t1, t2;
	sceRtcGetCurrentTick(&t1);
	#endif

	int i, s;
	trace_t   trace;

	Con_DPrintf("====================\n");
	Con_DPrintf("Starting Do_Pathfind\n");
	Con_DPrintf("====================\n");

	int zombie_entnum = G_EDICTNUM(OFS_PARM0);
	int target_entnum = G_EDICTNUM(OFS_PARM1);
	edict_t * zombie = G_EDICT(OFS_PARM0);
	edict_t * ent = G_EDICT(OFS_PARM1);

	if(developer.value == 3) {
		Con_Printf("Finding start waypoint\n");
	}
	int start_waypoint = get_closest_waypoint(zombie_entnum);
	if(developer.value == 3) {
		Con_Printf("Finding goal waypoint\n");
	}
	int goal_waypoint = get_closest_waypoint(target_entnum);

	if(start_waypoint == -1 || goal_waypoint == -1) {
		Con_DPrintf("Pathfind failure. Invalid start or goal waypoint. (Start: %d, Goal: %d)\n", start_waypoint, goal_waypoint);
		G_FLOAT(OFS_RETURN) = 0;
		return;
	}

	Con_DPrintf("\tStarting waypoint: %i, Ending waypoint: %i\n", start_waypoint, goal_waypoint);
	if (sv_way_pathfind(start_waypoint, goal_waypoint)) {

		// --------------------------------------------------------------------
		// Debug print zombie path
		// --------------------------------------------------------------------
		if(developer.value == 3) {
			Con_Printf("\tPrinting zombie (%d) (%d --> %d) path: [", zombie_entnum, start_waypoint, goal_waypoint);
			for(i = process_list_length - 1; i >= 0; i--) {
				Con_Printf("%d, ", process_list[i]);
			}
			Con_Printf("]\n");

			Con_Printf("\tWaypoint path distances: [");
			for(i = process_list_length - 1; i >= 0; i--) {
				float waypoint_dist = VectorDistanceSquared(zombie->v.origin, waypoints[process_list[i]].origin);
				Con_Printf("%.2f, ", waypoint_dist);
			}
			Con_Printf("]\n");

			Con_Printf("\tWaypoint path traceboxes: [");
			for(i = process_list_length - 1; i >= 0; i--) {
				int waypoint_tracebox_result = ofs_tracebox(zombie->v.origin, ai_hull_mins, ai_hull_maxs, waypoints[process_list[i]].origin, MOVE_NOMONSTERS, ent);
				Con_Printf("%d, ", waypoint_tracebox_result);
			}
			Con_Printf("]\n");
		}
		
		// --------------------------------------------------------------------

		int zombie_slot = -1;
		int free_slot = -1;

		for(i = 0; i < MaxZombies; i++) {
			// If we see any free slots, keep track of it, we might need it
			if(free_slot == -1 && !zombie_list[i].zombienum) {
				free_slot = i;
			}
			else if(zombie_entnum == zombie_list[i].zombienum) { 
				zombie_slot = i;
				break;
			}
		}

		// If this zombie ent doesn't have a slot, take the free slot we saw
		if(zombie_slot == -1 && free_slot != -1) {
			zombie_slot = free_slot;
		}
		if(zombie_slot != -1) {
			// Claim the slot
			zombie_list[zombie_slot].zombienum = zombie_entnum;
			for (s = 0; s < process_list_length; s++) {
				zombie_list[zombie_slot].pathlist[s] = process_list[s];
			}
			zombie_list[zombie_slot].pathlist_length = process_list_length;

#ifdef MEASURE_PF_PERF
			sceRtcGetCurrentTick(&t2);
			double elapsed = (t2 - t1) * 0.000001;
			Con_Printf("PF time: %f\n", elapsed);
#endif

			// If there is only one waypoint on the path, we are already at the player's waypoint
			if(zombie_list[zombie_slot].pathlist_length == 1) {
				Con_DPrintf("\tWe are at player's waypoint already!\n");
				G_FLOAT(OFS_RETURN) = -1;
			} 
			else {
				Con_DPrintf("\tPath found!\n");
				G_FLOAT(OFS_RETURN) = 1;
			}
			return;
		}
	}

#ifdef MEASURE_PF_PERF
	sceRtcGetCurrentTick(&t2);
	double elapsed = (t2 - t1) * 0.000001;
	Con_Printf("PF time: %f\n", elapsed);
#endif

	Con_DPrintf("Pathfind failure. Goal waypoint not reachable.\n");
	G_FLOAT(OFS_RETURN) = 0;
}

//
// Returns distance (squared) between point q and the line segment (a,b)
//
// https://www.desmos.com/calculator/pwabcrtil0
//
float dist_to_line_segment(vec3_t a, vec3_t b, vec3_t q) {

	vec3_t ab;
	VectorSubtract(b,a,ab); // ab = b - a
	vec3_t aq;
	VectorSubtract(q,a,aq); // aq = q - a

	float aq_dot_ab = DotProduct(aq,ab);
	float ab_dot_ab = DotProduct(ab,ab);

	// Compute fraction along line segment (a,b) closest to point q
	float t = aq_dot_ab / ab_dot_ab;
	
	// If t < 0, return distance to point a
	if(t < 0) {
		return VectorDistanceSquared(q,a);
	}
	// If t > 1, return distance to point b
	if(t > 1) {
		return VectorDistanceSquared(q,b);
	}

	// Otherwise, return distance to point on a,b at fraction t
	vec3_t point_on_ab;
	VectorLerp(a, t, b, point_on_ab);
	return VectorDistanceSquared(q, point_on_ab);
}




/*
=================
Get_Next_Waypoint This function will return the next waypoint in zombies path and then remove it from the list

vector Get_Next_Waypoint (entity)
=================
*/
void Get_Next_Waypoint (void) {
	int entnum;
	edict_t *ent;
	// vec3_t move;
	vec3_t start;
	// vec3_t mins;
	// vec3_t maxs;

	// Initialize to world origin
	// VectorCopy(vec3_origin, move);

	entnum = G_EDICTNUM(OFS_PARM0);
	ent = G_EDICT(OFS_PARM0);
	VectorCopy(G_VECTOR(OFS_PARM1), start);
	// VectorCopy(G_VECTOR(OFS_PARM2), mins);
	// VectorCopy(G_VECTOR(OFS_PARM3), maxs);


	edict_t *goal_ent = PROG_TO_EDICT(ent->v.enemy);
	vec3_t goal;
	VectorCopy(goal_ent->v.origin, goal);

	if(developer.value == 3){
		Con_Printf("Get_Next_Waypoint for ent %d\n", entnum);
		Con_Printf("\tEnt origin: (%f, %f, %f)\n", ent->v.origin[0], ent->v.origin[1], ent->v.origin[2]);
		Con_Printf("\tSearch start origin: (%f, %f, %f)\n", start[0], start[1], start[2]);
	}

	int zombie_idx = -1;
	for (int i = 0; i < MaxZombies; i++) {
		if(entnum == zombie_list[i].zombienum) {
			zombie_idx = i;
			break;
		}
	}

	// If we didn't find the ent in our list of data, stop. Return the enemy ent's origin
	if(zombie_idx == -1) {
		if(developer.value == 3){
			Con_Printf("Warning: no pathing data found for ent %d.\n", entnum);
		}
		VectorCopy(goal, G_VECTOR(OFS_RETURN));
		return;
	}


	if(developer.value == 3){
		// Print path (stored in reverse order from zombie to target ent)
		Con_Printf("\tpath before: [");
		for(int i = zombie_list[zombie_idx].pathlist_length - 1; i >= 0; i--) {
			Con_Printf(" %d,", zombie_list[zombie_idx].pathlist[i]);
		}
		Con_Printf("]\n");
	}


	// if(developer.value == 3){
	// 	float dist;
	// 	if(zombie_list[zombie_idx].pathlist_length > 0) {
	// 		int first_waypoint_idx = zombie_list[zombie_idx].pathlist[zombie_list[zombie_idx].pathlist_length - 1];
	// 		dist = VectorDistanceSquared(ent->v.origin, waypoints[first_waypoint_idx].origin);
	// 		Con_Printf("\tDist squared to first waypoint (%d): %.2f\n", first_waypoint_idx, dist);
	// 		Con_Printf("\t\tEnt pos: (%.2f, %.2f, %.2f)\n", ent->v.origin[0], ent->v.origin[1], ent->v.origin[2]);
	// 		Con_Printf("\t\tFirst waypoint pos: (%.2f, %.2f, %.2f)\n", waypoints[first_waypoint_idx].origin[0], waypoints[first_waypoint_idx].origin[1], waypoints[first_waypoint_idx].origin[2]);
	// 	}
	// 	dist = VectorDistanceSquared(ent->v.origin, goal_ent->v.origin);
	// 	Con_Printf("\tDist squared to goal ent: %.2f\n", dist);
	// }


	// Check if our path is now empty.
	// If it's empty, we have no more waypoints to chase, follow the enemy entity.
	if(zombie_list[zombie_idx].pathlist_length < 1) {
		if(developer.value == 3){
			Con_Printf("\tZombie path length: %d, returning enemy origin.\n", zombie_list[zombie_idx].pathlist_length);
		}
		// The zombie's path is empty, return the enemy origin
		VectorCopy(goal, G_VECTOR(OFS_RETURN));
		return;
	}


	// ---------------–---------------–---------------–---------------–
	// 
	// There is an unfortunate edge case in the following situation:
	// 
	// On uneven terrain, tracebox may fail for the true closest waypoint, 
	// yielding a nonoptimal path we instead go for a waypoint farther than 
	// the one we should've gone for.
	// 
	// In some instances, this causes us to run away from the optimal path
	// to some start waypoint, only to run through back through the point
	// we were originally standing on.
	//
	// To attempt to catch this edge case, check the distance from where we are
	// standing to the closest point on each edge along the waypoint path, 
	// to see if we are already somewhere along the path.
	// if so, skip waypoints up to the point we are standing.
	// 
	// ---------------–---------------–---------------–---------------–
	float dist_threshold = 400; // Max distance squared to path
	// --
	float best_edge_idx = -2; // -2 = None, -1 = Closest to edge connecting final waypoint and goal
	float best_edge_dist = INFINITY;


	for(int i = zombie_list[zombie_idx].pathlist_length - 1; i >= 0; i--) {
		float dist;
		if(i > 0) {
			dist = dist_to_line_segment(waypoints[zombie_list[zombie_idx].pathlist[i]].origin, waypoints[zombie_list[zombie_idx].pathlist[i-1]].origin, start);
		}
		// If on i == 0, endpoint of edge is the goal position
		else {
			dist = dist_to_line_segment(waypoints[zombie_list[zombie_idx].pathlist[i]].origin, goal, start);
		}
		if(dist < best_edge_dist) {
			best_edge_idx = i;
			best_edge_dist = dist;
		}
	}

	// If we are within the threshold of a waypoint edge, drop all waypoints up to and including the start waypoint for that edge
	if(best_edge_dist <= dist_threshold) {
		zombie_list[zombie_idx].pathlist_length = best_edge_idx;
	}

	if(developer.value == 3){
		// Print path (stored in reverse order from zombie to target ent)
		Con_Printf("\tpath after pruning: [");
		for(int i = zombie_list[zombie_idx].pathlist_length - 1; i >= 0; i--) {
			Con_Printf(" %d,", zombie_list[zombie_idx].pathlist[i]);
		}
		Con_Printf("]\n");
	}

	// ---------------–---------------–---------------–---------------–


	// ---------------–---------------–---------------–---------------–
	// FIXME - Check if we are already somewhere along the path
	// Check distance to each line segment
	// If distance < 40qu, we're going to consider ourselves already on that edge, and skip the initial waypoints



	// ---------------–---------------–---------------–---------------–
	// Check to see if we can walk directly to any waypoints farther
	// along the path.
	// ---------------–---------------–---------------–---------------–
	vec3_t ent_mins;
	vec3_t ent_maxs;
	VectorCopy(ai_hull_mins, ent_mins);
	VectorCopy(ai_hull_maxs, ent_maxs);


	// Get the index of the farthest waypoint we can walk to in the path:
	int farthest_walkable_path_node_idx = -2; // -2 means no waypoints were walkable, -1 means we can walk to goal ent position
	for(int i = zombie_list[zombie_idx].pathlist_length - 1; i >= 0; i--) {
		if(ofs_tracebox(start, ent_mins, ent_maxs, waypoints[zombie_list[zombie_idx].pathlist[i]].origin, MOVE_NOMONSTERS, ent)) {
			farthest_walkable_path_node_idx = i;
			continue;
		}
		break;
	}

	// If we were able to walk all the way to the final waypoint, check if we can walk to the goal entity position
	if(farthest_walkable_path_node_idx == 0) {
		if(ofs_tracebox(start, ent_mins, ent_maxs, goal, MOVE_NOMONSTERS, ent)) {
			farthest_walkable_path_node_idx = -1;
		}
	}


	// If weren't able to walk to any waypoints, return first waypoint in path
	if(farthest_walkable_path_node_idx == -2) {
		int waypoint_idx = zombie_list[zombie_idx].pathlist[zombie_list[zombie_idx].pathlist_length - 1];

		// Remove first waypoint from path
		zombie_list[zombie_idx].pathlist_length -= 1;


		if(developer.value == 3){
			Con_Printf("\tReturning walk to first path node. (path node: %d, waypoint: %d)\n", (zombie_list[zombie_idx].pathlist_length - 1) + 1, waypoint_idx);
			Con_Printf("\tpath after: [");
			for(int i = zombie_list[zombie_idx].pathlist_length - 1; i >= 0; i--) {
				Con_Printf(" %d,", zombie_list[zombie_idx].pathlist[i]);
			}
			Con_Printf("]\n");
		}


		VectorCopy(waypoints[waypoint_idx].origin, G_VECTOR(OFS_RETURN));
		return;
	}

	// If we were able to walk all the way to goal entity, return that point, clear the path
	if(farthest_walkable_path_node_idx == -1) {
		if(developer.value == 3){
			Con_Printf("\tReturning can walk to goal. (path node: %d)\n", farthest_walkable_path_node_idx);
		}
		VectorCopy(goal, G_VECTOR(OFS_RETURN));
		// Remove all nodes from the path
		zombie_list[zombie_idx].pathlist_length = 0;
		return;
	}


	if(developer.value == 3){
		Con_Printf("Farthest walkable path node: %d (waypoint: %d)\n",
			(zombie_list[zombie_idx].pathlist_length - 1) - farthest_walkable_path_node_idx,
			zombie_list[zombie_idx].pathlist[farthest_walkable_path_node_idx]
		);
	}

	// Otherwise, we were able to walk to at least one node. 
	// Binary search

	// Perform a binary search along the edge from cur to next
	int edge_start_waypoint_idx;
	int edge_end_waypoint_idx;
	vec3_t edge_start;
	vec3_t edge_end;

	if(farthest_walkable_path_node_idx > 0) {
		edge_start_waypoint_idx = zombie_list[zombie_idx].pathlist[farthest_walkable_path_node_idx];
		edge_end_waypoint_idx = zombie_list[zombie_idx].pathlist[farthest_walkable_path_node_idx - 1];
		if(developer.value == 3){
			Con_Printf("\tPerforming binary search between waypoint %d (%d in path, can walk: 1) and %d (%d in path, can walk: 0)\n", 
				edge_start_waypoint_idx, (zombie_list[zombie_idx].pathlist_length - 1) - farthest_walkable_path_node_idx,
				edge_end_waypoint_idx, ((zombie_list[zombie_idx].pathlist_length - 1) - farthest_walkable_path_node_idx) + 1
			);
		}
		VectorCopy(waypoints[edge_start_waypoint_idx].origin, edge_start);
		VectorCopy(waypoints[edge_end_waypoint_idx].origin, edge_end);
	}
	else {
		edge_start_waypoint_idx = zombie_list[zombie_idx].pathlist[farthest_walkable_path_node_idx];
		edge_end_waypoint_idx = -1;

		if(developer.value == 3){
			Con_Printf("\tPerforming binary search between waypoint %d (%d in path, can walk: 1) and goal ent pos\n", 
				edge_start_waypoint_idx, (zombie_list[zombie_idx].pathlist_length - 1) - farthest_walkable_path_node_idx
			);
		}
		VectorCopy(waypoints[edge_start_waypoint_idx].origin, edge_start);
		VectorCopy(goal, edge_end);
	}


	int n_iters = 3;
	int cur_frac_numerator = 1;
	float cur_frac;

	vec3_t cur_point;
	vec3_t best_point;
	VectorCopy(edge_start, best_point);
	float best_point_frac = 0;

	for(int i = 0; i < n_iters; i++) {
		// Calculate the number in [0,1] corresponding to how far along the edge we are checking
		cur_frac = ((float) cur_frac_numerator) / (2 << i);
		if(developer.value == 3){
			Con_Printf("\tBinary search iter: %d/%d, frac: %f\n", i, n_iters, cur_frac);
		}
		VectorLerp(edge_start, cur_frac, edge_end, cur_point);

		// Check if we can walk from the ent's current location directly to `cur_point`
		if(ofs_tracebox(start, ent_mins, ent_maxs, cur_point, MOVE_NOMONSTERS, ent)) {
			cur_frac_numerator = (cur_frac_numerator * 2) + 1;
			best_point_frac = cur_frac;
			VectorCopy(cur_point, best_point);
		}
		else {
			cur_frac_numerator = (cur_frac_numerator * 2) - 1;
		}
	}

	if(developer.value == 3){
		Con_Printf("\tpath after binary search: (%f x between waypoints (%d,%d), then [", 
			best_point_frac, 
			edge_start_waypoint_idx, 
			edge_end_waypoint_idx
		);
		for(int i = farthest_walkable_path_node_idx - 1; i >= 0; i--) {
			Con_Printf(" %d,", zombie_list[zombie_idx].pathlist[i]);
		}
		Con_Printf("]\n");
	}
	// Remove all points up to and including `farthest_walkable_path_node_idx` from the path
	zombie_list[zombie_idx].pathlist_length = farthest_walkable_path_node_idx;



	// ------------------------------------------------------------------------
	// If we're already incredibly close to the goal point along the path
	//
	// Get_Next_Waypoint should've returned somewhere farther along the path,
	// but is running into tricky edge cases regarding tracebox. 
	// For this case, force-advance to the next waypoint / goal along the path
	// ------------------------------------------------------------------------
	if(VectorDistanceSquared(start,best_point) < 64) {
		// If trying to walk to the next waypoint already, skip a waypoint on the path
		if(best_point_frac >= 1.0) {
			zombie_list[zombie_idx].pathlist_length -= 1;
		}

		// If we have at least one waypoint, walk directly to it, pop from path
		if(zombie_list[zombie_idx].pathlist_length > 0) {
			int waypoint_idx = zombie_list[zombie_idx].pathlist[zombie_list[zombie_idx].pathlist_length - 1];
			VectorCopy(waypoints[waypoint_idx].origin, best_point);
			zombie_list[zombie_idx].pathlist_length -= 1;
		}
		// If we have no waypoints on the path, walk to goal, clear the path
		else {
			zombie_list[zombie_idx].pathlist_length = 0;
			VectorCopy(goal, best_point);
		}

		if(developer.value == 3) {
			Con_Printf("\tForce-truncated path to %d waypoints.\n", zombie_list[zombie_idx].pathlist_length);
		}
	}
	// ------------------------------------------------------------------------


	if(developer.value == 3){
		Con_Printf("\tfinal path [");
		for(int i = zombie_list[zombie_idx].pathlist_length - 1; i >= 0; i--) {
			Con_Printf(" %d,", zombie_list[zombie_idx].pathlist[i]);
		}
		Con_Printf("]\n");

		Con_Printf("\tFinal best point: (%f, %f, %f)\n", best_point[0], best_point[1], best_point[2]);
	}

	VectorCopy(best_point, G_VECTOR(OFS_RETURN));
	return;
}




/*
=================
Get_First_Waypoint This function will return the waypoint waypoint in zombies path and then remove it from the list

vector Get_First_Waypoint (entity)
=================
*/
void Get_First_Waypoint (void) {
	// TODO - Remove `Get_First_Waypoint`, replace references with `Get_Next_Waypoint`
	Get_Next_Waypoint();
}


// 2001-09-20 QuakeC file access by FrikaC/Maddes  start
/*
=================
PF_fopen

float fopen (string,float)
=================
*/
void PF_fopen (void)
{
	char *p = G_STRING(OFS_PARM0);
	char *ftemp;
	int fmode = G_FLOAT(OFS_PARM1);
	int h = 0, fsize = 0;

	switch (fmode)
	{
		case 0: // read
			Sys_FileOpenRead (va("%s/%s",com_gamedir, p), &h);
			// protection against crash on Wii
			if(h <= 0) {
				G_FLOAT(OFS_RETURN) = -1;
				return;
			}
			G_FLOAT(OFS_RETURN) = (float) h;
			return;
		case 1: // append -- this is nasty
			// copy whole file into the zone
			fsize = Sys_FileOpenRead(va("%s/%s",com_gamedir, p), &h);
			if (h == -1)
			{
				h = Sys_FileOpenWrite(va("%s/%s",com_gamedir, p));
				G_FLOAT(OFS_RETURN) = (float) h;
				return;
			}
			ftemp = Z_Malloc(fsize + 1);
			Sys_FileRead(h, ftemp, fsize);
			Sys_FileClose(h);
			// spit it back out
			h = Sys_FileOpenWrite(va("%s/%s",com_gamedir, p));
			Sys_FileWrite(h, ftemp, fsize);
			Z_Free(ftemp); // free it from memory
			G_FLOAT(OFS_RETURN) = (float) h;  // return still open handle
			return;
		default: // write
			h = Sys_FileOpenWrite (va("%s/%s", com_gamedir, p));
			G_FLOAT(OFS_RETURN) = (float) h;
			return;
	}
}

/*
=================
PF_fclose

void fclose (float)
=================
*/
void PF_fclose (void)
{
	int h = (int)G_FLOAT(OFS_PARM0);
	if (h > 0) { // stop crashing on Wii HW
		Sys_FileClose(h);
	}
}

/*
=================
PF_fgets

string fgets (float)
=================
*/
void PF_fgets (void)
{
	// reads one line (up to a \n) into a string
	int		h;
	int		i;
	int		count;
	char	buffer;

	h = (int)G_FLOAT(OFS_PARM0);

	count = Sys_FileRead(h, &buffer, 1);
	
	if (count && buffer == '\r')	// carriage return
	{
		count = Sys_FileRead(h, &buffer, 1);	// skip
	}
	if (!count)	// EndOfFile
	{
		G_INT(OFS_RETURN) = OFS_NULL;	// void string
		return;
	}

	i = 0;
	while (count && buffer != '\n')
	{
		if (i < PR_MAX_TEMPSTRING-1)	// no place for character in temp string
		{
			pr_string_temp[i++] = buffer;
		}

		// read next character
		count = Sys_FileRead(h, &buffer, 1);
		if (count && buffer == '\r')	// carriage return
		{
			count = Sys_FileRead(h, &buffer, 1);	// skip
		}
	};
	pr_string_temp[i] = 0;

	G_INT(OFS_RETURN) = pr_string_temp - pr_strings;
}

/*
=================
PF_fputs

void fputs (float,string)
=================
*/
void PF_fputs (void)
{
	// writes to file, like bprint
	float handle = G_FLOAT(OFS_PARM0);
	char *str = PF_VarString(1);
	Sys_FileWrite (handle, str, strlen(str));
}
// 2001-09-20 QuakeC file access by FrikaC/Maddes  end

// entity (entity start, .string field, string match) find = #5;
void PF_Find (void)
{
	int		e;
	int		f;
	char	*s, *t;
	edict_t	*ed;

	e = G_EDICTNUM(OFS_PARM0);
	f = G_INT(OFS_PARM1);
	s = G_STRING(OFS_PARM2);
	if (!s)
		PR_RunError ("PF_Find: bad search string");

	for (e++ ; e < sv.num_edicts ; e++)
	{
		ed = EDICT_NUM(e);
		if (ed->free)
			continue;
		t = E_STRING(ed,f);
		if (!t)
			continue;
		if (!strcmp(t,s))
		{
			RETURN_EDICT(ed);
			return;
		}
	}

	RETURN_EDICT(sv.edicts);
}

// entity (entity start, .float field, float match) findfloat = #98;
void PF_FindFloat (void)
{
	int		e;
	int		f;
	float	s, t;
	edict_t	*ed;

	e = G_EDICTNUM(OFS_PARM0);
	f = G_INT(OFS_PARM1);
	s = G_FLOAT(OFS_PARM2);
	if (!s)
		PR_RunError ("PF_FindFloat: bad search float");

	for (e++ ; e < sv.num_edicts ; e++)
	{
		ed = EDICT_NUM(e);
		if (ed->free)
			continue;
		t = E_FLOAT(ed,f);
		if (!t)
			continue;
		if (t == s)
		{
			RETURN_EDICT(ed);
			return;
		}
	}

	RETURN_EDICT(sv.edicts);
}

void PR_CheckEmptyString (char *s)
{
	if (s[0] <= ' ')
		PR_RunError ("Bad string");
}

void PF_precache_file (void)
{	// precache_file is only used to copy files with qcc, it does nothing
	G_INT(OFS_RETURN) = G_INT(OFS_PARM0);
}

void PF_precache_sound (void)
{
	char	*s;
	int		i;

	if (sv.state != ss_loading)
		PR_RunError ("PF_Precache_*: Precache can only be done in spawn functions");

	s = G_STRING(OFS_PARM0);
	G_INT(OFS_RETURN) = G_INT(OFS_PARM0);
	PR_CheckEmptyString (s);

	for (i=0 ; i<MAX_SOUNDS ; i++)
	{
		if (!sv.sound_precache[i])
		{
			sv.sound_precache[i] = s;
			return;
		}
		if (!strcmp(sv.sound_precache[i], s))
			return;
	}
	PR_RunError ("PF_precache_sound: overflow");
}

void PF_precache_model (void)
{
	char	*s;
	int		i;

	if (sv.state != ss_loading)
		PR_RunError ("PF_Precache_*: Precache can only be done in spawn functions");

	s = G_STRING(OFS_PARM0);
	G_INT(OFS_RETURN) = G_INT(OFS_PARM0);
	PR_CheckEmptyString (s);

	for (i=0 ; i<MAX_MODELS ; i++)
	{
		if (!sv.model_precache[i])
		{
			sv.model_precache[i] = s;
			sv.models[i] = Mod_ForName (s, true);
			return;
		}
		if (!strcmp(sv.model_precache[i], s))
			return;
	}
	PR_RunError ("PF_precache_model: overflow");
}


void PF_coredump (void)
{
	ED_PrintEdicts ();
}

void PF_traceon (void)
{
	pr_trace = true;
}

void PF_traceoff (void)
{
	pr_trace = false;
}

void PF_eprint (void)
{
	ED_PrintNum (G_EDICTNUM(OFS_PARM0));
}

/*
===============
PF_walkmove

float(float yaw, float dist) walkmove
===============
*/
void PF_walkmove (void)
{
	edict_t	*ent;
	float	yaw, dist;
	vec3_t	move;
	dfunction_t	*oldf;
	int 	oldself;

	ent = PROG_TO_EDICT(pr_global_struct->self);
	yaw = G_FLOAT(OFS_PARM0);
	dist = G_FLOAT(OFS_PARM1);

	if ( !( (int)ent->v.flags & (FL_ONGROUND|FL_FLY|FL_SWIM) ) )
	{
		G_FLOAT(OFS_RETURN) = 0;
		return;
	}

	yaw = yaw*M_PI*2 / 360;

	move[0] = cosf(yaw)*dist;
	move[1] = sinf(yaw)*dist;
	move[2] = 0;

// save program state, because SV_movestep may call other progs
	oldf = pr_xfunction;
	oldself = pr_global_struct->self;

	G_FLOAT(OFS_RETURN) = SV_movestep(ent, move, true);

	//if(!strcmp(pr_strings + ent->v.classname, "ai_zombie"))
	//{
	//	VectorCopy(ent->v.origin,PROG_TO_EDICT(ent->v.head)->v.origin);
	//	VectorCopy(ent->v.origin,PROG_TO_EDICT(ent->v.rarm)->v.origin);
	//	VectorCopy(ent->v.origin,PROG_TO_EDICT(ent->v.larm)->v.origin);
	//}

// restore program state
	pr_xfunction = oldf;
	pr_global_struct->self = oldself;
}

/*
===============
PF_droptofloor

void() droptofloor
===============
*/
void PF_droptofloor (void)
{
	edict_t		*ent;
	vec3_t		end;
	trace_t		trace;

	ent = PROG_TO_EDICT(pr_global_struct->self);

	VectorCopy (ent->v.origin, end);
	end[2] -= 256;

	trace = SV_Move (ent->v.origin, ent->v.mins, ent->v.maxs, end, false, ent);

	if (trace.fraction == 1 || trace.allsolid)
		G_FLOAT(OFS_RETURN) = 0;
	else
	{
		VectorCopy (trace.endpos, ent->v.origin);
		SV_LinkEdict (ent, false);
		ent->v.flags = (int)ent->v.flags | FL_ONGROUND;
		ent->v.groundentity = EDICT_TO_PROG(trace.ent);
		G_FLOAT(OFS_RETURN) = 1;
	}
}

/*
===============
PF_lightstyle

void(float style, string value) lightstyle
===============
*/
void PF_lightstyle (void)
{
	int		style;
	char	*val;
	client_t	*client;
	int			j;

	style = G_FLOAT(OFS_PARM0);
	val = G_STRING(OFS_PARM1);

// change the string in sv
	sv.lightstyles[style] = val;

// send message to all clients on this server
	if (sv.state != ss_active)
		return;

	for (j=0, client = svs.clients ; j<svs.maxclients ; j++, client++)
		if (client->active || client->spawned)
		{
			MSG_WriteChar (&client->message, svc_lightstyle);
			MSG_WriteChar (&client->message,style);
			MSG_WriteString (&client->message, val);
		}
}

void PF_rint (void)
{
	float	f;
	f = G_FLOAT(OFS_PARM0);
	if (f > 0)
		G_FLOAT(OFS_RETURN) = (int)(f + 0.5);
	else
		G_FLOAT(OFS_RETURN) = (int)(f - 0.5);
}
void PF_floor (void)
{
	G_FLOAT(OFS_RETURN) = floorf(G_FLOAT(OFS_PARM0));
}
void PF_ceil (void)
{
	G_FLOAT(OFS_RETURN) = ceilf(G_FLOAT(OFS_PARM0));
}


/*
=============
PF_checkbottom
=============
*/
void PF_checkbottom (void)
{
	edict_t	*ent;

	ent = G_EDICT(OFS_PARM0);

	G_FLOAT(OFS_RETURN) = SV_CheckBottom (ent);
}

/*
=============
PF_pointcontents
=============
*/
void PF_pointcontents (void)
{
	float	*v;

	v = G_VECTOR(OFS_PARM0);

	G_FLOAT(OFS_RETURN) = SV_PointContents (v);
}

/*
=============
PF_nextent

entity nextent(entity)
=============
*/
void PF_nextent (void)
{
	int		i;
	edict_t	*ent;

	i = G_EDICTNUM(OFS_PARM0);
	while (1)
	{
		i++;
		if (i == sv.num_edicts)
		{
			RETURN_EDICT(sv.edicts);
			return;
		}
		ent = EDICT_NUM(i);
		if (!ent->free)
		{
			RETURN_EDICT(ent);
			return;
		}
	}
}

/*
=============
PF_aim

Pick a vector for the player to shoot along
vector aim(entity, missilespeed)
=============
*/
cvar_t	sv_aim = {"sv_aim", "0.93"};
void PF_aim (void)
{
	edict_t	*ent, *check, *bestent;
	vec3_t	start, dir, end, bestdir, tempv;
	int		i, j;
	trace_t	tr;
	float	dist, bestdist;
	float	speed;

	ent = G_EDICT(OFS_PARM0);
	speed = G_FLOAT(OFS_PARM1);
	VectorAdd(ent->v.origin, ent->v.view_ofs, tempv);
	VectorCopy (tempv, start);
	start[2] += 20;

// try sending a trace straight
	VectorCopy (pr_global_struct->v_forward, dir);
	VectorMA (start, 2048, dir, end);
	tr = SV_Move (start, vec3_origin, vec3_origin, end, false, ent);
	if (tr.ent && tr.ent->v.takedamage == DAMAGE_AIM
	&& (!teamplay.value || ent->v.team <=0 || ent->v.team != tr.ent->v.team) )
	{
		VectorCopy (pr_global_struct->v_forward, G_VECTOR(OFS_RETURN));
		return;
	}


// try all possible entities
	VectorCopy (dir, bestdir);
	bestdist = sv_aim.value;
	// dr_mabuse1981: PSP controls sucks ass
	bestent = NULL;

	check = NEXT_EDICT(sv.edicts);
	for (i=1 ; i<sv.num_edicts ; i++, check = NEXT_EDICT(check) )
	{
		if (check->v.takedamage != DAMAGE_AIM)
			continue;
		if (check == ent)
			continue;
		if (teamplay.value && ent->v.team > 0 && ent->v.team == check->v.team)
			continue;	// don't aim at teammate
		for (j=0 ; j<3 ; j++)
			end[j] = check->v.origin[j]
			+ 0.5*(check->v.mins[j] + check->v.maxs[j]);
		VectorSubtract (end, start, dir);
		VectorNormalize (dir);
		dist = DotProduct (dir, pr_global_struct->v_forward);
		if (dist < bestdist)
			continue;	// to far to turn
		tr = SV_Move (start, vec3_origin, vec3_origin, end, false, ent);
		if (tr.ent == check)
		{	// can shoot at this one
			bestdist = dist;
			bestent = check;
		}
	}

	if (bestent)
	{
		VectorSubtract (bestent->v.origin, ent->v.origin, dir);
// dr_mabuse1981: bullets shouldnt go offset anymore (fix from Quartal engine)
//		dist = DotProduct (dir, pr_global_struct->v_forward);
//		VectorScale (pr_global_struct->v_forward, dist, end);
//		end[2] = dir[2];
		end[0] = dir[0];
		end[1] = dir[1];
		end[2] = dir[2];
		VectorNormalize (end);
		VectorCopy (end, G_VECTOR(OFS_RETURN));
	}
	else
	{
		VectorCopy (bestdir, G_VECTOR(OFS_RETURN));
	}
}

/*
==============
PF_changeyaw

This was a major timewaster in progs, so it was converted to C
==============
*/
void PF_changeyaw (void)
{
	edict_t		*ent;
	float		ideal, current, move, speed;

	ent = PROG_TO_EDICT(pr_global_struct->self);
	current = anglemod( ent->v.angles[1] );
	ideal = ent->v.ideal_yaw;
	speed = ent->v.yaw_speed;

	if (current == ideal)
		return;
	move = ideal - current;
	if (ideal > current)
	{
		if (move >= 180)
			move = move - 360;
	}
	else
	{
		if (move <= -180)
			move = move + 360;
	}
	if (move > 0)
	{
		if (move > speed)
			move = speed;
	}
	else
	{
		if (move < -speed)
			move = -speed;
	}

	ent->v.angles[1] = anglemod (current + move);
}

/*
==============
PF_GetSoundLen

Get the lenght of the sound (useful for things like radio)
==============
*/
void PF_GetSoundLen (void)
{

	char	*name;

	name = G_STRING(OFS_PARM0);

    char	namebuffer[256];
	byte	*data;
	wavinfo_t	info;
	byte	stackbuf[1*1024];		// avoid dirtying the cache heap

//Con_Printf ("S_LoadSound: %x\n", (int)stackbuf);
// load it in
    Q_strcpy(namebuffer, "");
    Q_strcat(namebuffer, name);

	data = COM_LoadStackFile(namebuffer, stackbuf, sizeof(stackbuf));

	if (!data)
	{
		Con_Printf ("Couldn't load %s\n", namebuffer);
		G_FLOAT(OFS_RETURN) = -1;
		return;
	}

	info = GetWavinfo (name, data, com_filesize);
	if (info.channels != 1)
	{
		Con_Printf ("%s is a stereo sample\n",name);
		G_FLOAT(OFS_RETURN) = -1;
		return;
	}

	G_FLOAT(OFS_RETURN) = (float)info.samples/(float)info.rate;
}

/*
==============
PF_changepitch
==============
*/
void PF_changepitch (void)
{
	float	ideal, current, move, speed;
	edict_t	*ent;
	eval_t	*val;

	ent = G_EDICT(OFS_PARM0);
	current = anglemod(ent->v.angles[0]);
	if ((val = GETEDICTFIELDVALUE(ent, eval_idealpitch)))
	{
		ideal = val->_float;
	}
	else
	{
		PR_RunError ("PF_changepitch: .float idealpitch and .float pitch_speed must be defined to use changepitch");
		return;
	}

	if ((val = GETEDICTFIELDVALUE(ent, eval_pitch_speed)))
	{
		speed = val->_float;
	}
	else
	{
		PR_RunError ("PF_changepitch: .float idealpitch and .float pitch_speed must be defined to use changepitch");
		return;
	}

	if (current == ideal)
		return;

	move = ideal - current;
	if (ideal > current)
	{
		if (move >= 180)
			move = move - 360;
	}
	else
	{
		if (move <= -180)
			move = move + 360;
	}

	if (move > 0)
	{
		if (move > speed)
			move = speed;
	}
	else
	{
		if (move < -speed)
			move = -speed;
	}

	ent->v.angles[0] = anglemod (current + move);
}

/*
===============================================================================

MESSAGE WRITING

===============================================================================
*/

#define	MSG_BROADCAST	0		// unreliable to all
#define	MSG_ONE			1		// reliable to one (msg_entity)
#define	MSG_ALL			2		// reliable to all
#define	MSG_INIT		3		// write to the init string

sizebuf_t *WriteDest (void)
{
	int		entnum;
	int		dest;
	edict_t	*ent;

	dest = G_FLOAT(OFS_PARM0);
	switch (dest)
	{
	case MSG_BROADCAST:
		return &sv.datagram;

	case MSG_ONE:
		ent = PROG_TO_EDICT(pr_global_struct->msg_entity);
		entnum = NUM_FOR_EDICT(ent);
		if (entnum < 1 || entnum > svs.maxclients)
			PR_RunError ("WriteDest: not a client");
		return &svs.clients[entnum-1].message;

	case MSG_ALL:
		return &sv.reliable_datagram;

	case MSG_INIT:
		return &sv.signon;

	default:
		PR_RunError ("WriteDest: bad destination");
		break;
	}

	return NULL;
}

void PF_WriteByte (void)
{
	MSG_WriteByte (WriteDest(), G_FLOAT(OFS_PARM1));
}

void PF_WriteChar (void)
{
	MSG_WriteChar (WriteDest(), G_FLOAT(OFS_PARM1));
}

void PF_WriteShort (void)
{
	MSG_WriteShort (WriteDest(), G_FLOAT(OFS_PARM1));
}

void PF_WriteLong (void)
{
	MSG_WriteLong (WriteDest(), G_FLOAT(OFS_PARM1));
}

void PF_WriteAngle (void)
{
	MSG_WriteAngle (WriteDest(), G_FLOAT(OFS_PARM1));
}

void PF_WriteCoord (void)
{
	MSG_WriteCoord (WriteDest(), G_FLOAT(OFS_PARM1));
}

void PF_WriteString (void)
{
	MSG_WriteString (WriteDest(), G_STRING(OFS_PARM1));
}


void PF_WriteEntity (void)
{
	MSG_WriteShort (WriteDest(), G_EDICTNUM(OFS_PARM1));
}

//=============================================================================

int SV_ModelIndex (char *name);

void PF_makestatic (void)
{
	edict_t	*ent;
	int		i;

	ent = G_EDICT(OFS_PARM0);

	MSG_WriteByte (&sv.signon,svc_spawnstatic);

	MSG_WriteShort (&sv.signon, SV_ModelIndex(pr_strings + ent->v.model));

	MSG_WriteByte (&sv.signon, ent->v.frame);
	MSG_WriteByte (&sv.signon, ent->v.colormap);
	MSG_WriteByte (&sv.signon, ent->v.skin);
	for (i=0 ; i<3 ; i++)
	{
		MSG_WriteCoord(&sv.signon, ent->v.origin[i]);
		MSG_WriteAngle(&sv.signon, ent->v.angles[i]);
	}

// throw the entity away now
	ED_Free (ent);
}

//=============================================================================

/*
==============
PF_setspawnparms
==============
*/
void PF_setspawnparms (void)
{
	edict_t	*ent;
	int		i;
	client_t	*client;

	ent = G_EDICT(OFS_PARM0);
	i = NUM_FOR_EDICT(ent);
	if (i < 1 || i > svs.maxclients)
		PR_RunError ("Entity is not a client");

	// copy spawn parms out of the client_t
	client = svs.clients + (i-1);

	for (i=0 ; i< NUM_SPAWN_PARMS ; i++)
		(&pr_global_struct->parm1)[i] = client->spawn_parms[i];
}

/*
==============
PF_changelevel
==============
*/
void PF_changelevel (void)
{
	char	*s;

// make sure we don't issue two changelevels
	if (svs.changelevel_issued)
		return;
	svs.changelevel_issued = true;

	s = G_STRING(OFS_PARM0);
	Cbuf_AddText (va("changelevel %s\n",s));
}



void PF_sin (void)
{
	G_FLOAT(OFS_RETURN) = sinf(G_FLOAT(OFS_PARM0));
}

void PF_cos (void)
{
	G_FLOAT(OFS_RETURN) = cosf(G_FLOAT(OFS_PARM0));
}

void PF_sqrt (void)
{
	G_FLOAT(OFS_RETURN) = sqrtf(G_FLOAT(OFS_PARM0));
}

void PF_Fixme (void)
{
	//PR_RunError ("unimplemented bulitin");
	Con_DPrintf("unimplemented bulitin");
}

/*
=================
PF_SongEgg

plays designated easter egg track

songegg(trackname)
=================
*/
void PF_SongEgg (void)
{
	char *s;
	
	s = G_STRING(OFS_PARM0);
	Cbuf_AddText (va("cd playstring %s 0\n",s));
}

/*
=================
PF_MaxAmmo

activates max ammo text in HUD

nzp_maxammo()
=================
*/
void PF_MaxAmmo(void)
{
	MSG_WriteByte(&sv.reliable_datagram, svc_maxammo);
}

/*
=================
PF_GrenadePulse

pulses grenade crosshair

grenade_pulse()
=================
*/
void PF_GrenadePulse(void)
{
	client_t	*client;
	int			entnum;

	entnum = G_EDICTNUM(OFS_PARM0);

	if (entnum < 1 || entnum > svs.maxclients)
		return;

	client = &svs.clients[entnum-1];
	MSG_WriteByte (&client->message, svc_pulse);
}

/*
=================
PF_SetDoubleTapVersion

Server tells client which HUD icon
to draw for Double-Tap (damage buff
v.s. just rate of fire enhancement).

nzp_setdoubletapver()
=================
*/
void PF_SetDoubleTapVersion(void)
{
	client_t	*client;
	int			entnum;
	int 		state;

	entnum = G_EDICTNUM(OFS_PARM0);
	state = G_FLOAT(OFS_PARM1);

	if (entnum < 1 || entnum > svs.maxclients)
		return;

	client = &svs.clients[entnum-1];
	MSG_WriteByte (&client->message, svc_doubletap);
	MSG_WriteByte (&client->message, state);
}

/*
=================
PF_ScreenFlash

Server tells client to flash on screen
for a short (but specified) moment.

nzp_screenflash(target, color, duration, type)
=================
*/
void PF_ScreenFlash(void)
{
	client_t	*client;
	int			entnum;
	int 		color, duration, type;

	entnum = G_EDICTNUM(OFS_PARM0);
	color = G_FLOAT(OFS_PARM1);
	duration = G_FLOAT(OFS_PARM2);
	type = G_FLOAT(OFS_PARM3);

	// Specified world, or something. Send to everyone.
	if (entnum < 1 || entnum > svs.maxclients) {
		MSG_WriteByte(&sv.reliable_datagram, svc_screenflash);
		MSG_WriteByte(&sv.reliable_datagram, color);
		MSG_WriteByte(&sv.reliable_datagram, duration);
		MSG_WriteByte(&sv.reliable_datagram, type);
	} 
	// Send to specific user
	else {
		client = &svs.clients[entnum-1];
		MSG_WriteByte (&client->message, svc_screenflash);
		MSG_WriteByte (&client->message, color);
		MSG_WriteByte (&client->message, duration);
		MSG_WriteByte (&client->message, type);
	}
}

/*
=================
PF_LockViewmodel

Server tells client to lock their
viewmodel in place, if applicable.

nzp_lockviewmodel()
=================
*/
void PF_LockViewmodel(void)
{
	client_t	*client;
	int			entnum;
	int 		state;

	entnum = G_EDICTNUM(OFS_PARM0);
	state = G_FLOAT(OFS_PARM1);

	if (entnum < 1 || entnum > svs.maxclients)
		return;

	client = &svs.clients[entnum-1];
	MSG_WriteByte (&client->message, svc_lockviewmodel);
	MSG_WriteByte (&client->message, state);
}

/*
=================
PF_Rumble

Server tells client to rumble their
GamePad.

nzp_rumble()
=================
*/
void PF_Rumble(void)
{
	client_t	*client;
	int			entnum;
	int 		low_frequency;
	int 		high_frequency;
	int 		duration;

	entnum = G_EDICTNUM(OFS_PARM0);
	low_frequency = G_FLOAT(OFS_PARM1);
	high_frequency = G_FLOAT(OFS_PARM2);
	duration = G_FLOAT(OFS_PARM3);

	if (entnum < 1 || entnum > svs.maxclients)
		return;

	client = &svs.clients[entnum-1];
	MSG_WriteByte (&client->message, svc_rumble);
	MSG_WriteShort (&client->message, low_frequency);
	MSG_WriteShort (&client->message, high_frequency);
	MSG_WriteShort (&client->message, duration);
}

/*
=================
PF_BettyPrompt

draws status on hud on
how to use bouncing
betty.

nzp_bettyprompt()
=================
*/
void PF_BettyPrompt(void)
{
	client_t	*client;
	int			entnum;

	entnum = G_EDICTNUM(OFS_PARM0);

	if (entnum < 1 || entnum > svs.maxclients)
		return;

	client = &svs.clients[entnum-1];
	MSG_WriteByte (&client->message, svc_bettyprompt);
}

/*
=================
PF_SetPlayerName

sends the name string to
the client, avoids making
a protocol extension and
spamming strings.

nzp_setplayername()
=================
*/
void PF_SetPlayerName(void)
{
	client_t	*client;
	int			entnum;
	char* 		s;

	entnum = G_EDICTNUM(OFS_PARM0);
	s = G_STRING(OFS_PARM1);

	if (entnum < 1 || entnum > svs.maxclients)
		return;

	client = &svs.clients[entnum-1];
	MSG_WriteByte (&client->message, svc_playername);
	MSG_WriteString (&client->message, s);
}

/*
=================
PF_MaxZombies

Returns the total number of zombies
the platform can have out at once.

nzp_maxai()
=================
*/
void PF_MaxZombies(void)
{
#ifdef _3DS
	if (new3ds_flag)
		G_FLOAT(OFS_RETURN) = MaxZombies;
	else
		G_FLOAT(OFS_RETURN) = 12;
#else
	G_FLOAT(OFS_RETURN) = MaxZombies;
#endif
}

/*
=================
PF_achievement

unlocks the achievement number for entity

achievement(clientent, value)
=================
*/
void PF_achievement (void)
{
	int		ach;
	client_t	*client;
	int			entnum;

	entnum = G_EDICTNUM(OFS_PARM0);
	ach = G_FLOAT(OFS_PARM1);

	if (entnum < 1 || entnum > svs.maxclients)
	{
		Con_DPrintf ("tried to unlock ach to a non-client\n");
		return;
	}

	//Con_Printf (va("Achievement? %i\n", ach));	// JPG
	client = &svs.clients[entnum-1];

	MSG_WriteByte (&client->message,svc_achievement);
	MSG_WriteByte (&client->message, ach);
}


/*
=================
PF_updateLimb

updates zombies limb

PF_updateLimb(zombieent, value. limbent)
=================
*/
void PF_updateLimb (void)
{
	int		limb;
	int		zombieent, limbent;

	zombieent = G_EDICTNUM(OFS_PARM0);
	limb = G_FLOAT(OFS_PARM1);
	limbent = G_EDICTNUM(OFS_PARM2);
	MSG_WriteByte (&sv.reliable_datagram,   svc_limbupdate);
	MSG_WriteByte (&sv.reliable_datagram,  limb);
	MSG_WriteShort (&sv.reliable_datagram,  zombieent);
	MSG_WriteShort (&sv.reliable_datagram,  limbent);
}

// 2001-09-14 Enhanced BuiltIn Function System (EBFS) by Maddes  start
/*
=================
PF_builtin_find

float builtin_find (string)
=================
*/
void PF_builtin_find (void)
{
	int		j;
	float	funcno;
	char	*funcname;

	funcno = 0;
	funcname = G_STRING(OFS_PARM0);

	// search function name
	for ( j=1 ; j < pr_ebfs_numbuiltins ; j++)
	{
		if ((pr_ebfs_builtins[j].funcname) && (!(Q_strcasecmp(funcname,pr_ebfs_builtins[j].funcname))))
		{
			break;	// found
		}
	}

	if (j < pr_ebfs_numbuiltins)
	{
		funcno = pr_ebfs_builtins[j].funcno;
	}

	G_FLOAT(OFS_RETURN) = funcno;
}

/*
=================
PF_tokenize

float tokenize (string) = #441
=================
*/
//KRIMZON_SV_PARSECLIENTCOMMAND added both of these
// refined to work on psp on 2017-DEC-09
void PF_tokenize (void)
{
	char *m = G_STRING(OFS_PARM0);
	Cmd_TokenizeString(m);
	G_FLOAT(OFS_RETURN) = Cmd_Argc();
};

/*
=================
PF_argv

string argv (float num) = #442
=================
*/
void PF_ArgV  (void)
{
	char tempc[256];
	strcpy(tempc, Cmd_Argv(G_FLOAT(OFS_PARM0)));
	G_INT(OFS_RETURN) = tempc - pr_strings;
}


// 2001-09-14 Enhanced BuiltIn Function System (EBFS) by Maddes  end
builtin_t *pr_builtins;
int pr_numbuiltins;
// 2001-09-14 Enhanced BuiltIn Function System (EBFS) by Maddes  start
// for builtin function definitions see Quake Standards Group at http://www.quakesrc.org/
ebfs_builtin_t pr_ebfs_builtins[] =
{
	{   0, NULL, PF_Fixme },		// has to be first entry as it is needed for initialization in PR_LoadProgs()
	{   1, "makevectors", PF_makevectors },	// void(entity e)	makevectors 		= #1;
	{   2, "setorigin", PF_setorigin },		// void(entity e, vector o) setorigin	= #2;
	{   3, "setmodel", PF_setmodel },		// void(entity e, string m) setmodel	= #3;
	{   4, "setsize", PF_setsize },			// void(entity e, vector min, vector max) setsize = #4;
//	{   5, "fixme", PF_Fixme },				// void(entity e, vector min, vector max) setabssize = #5;
	{   6, "break", PF_break },				// void() break						= #6;
	{   7, "random", PF_random },			// float() random						= #7;
	{   8, "sound", PF_sound },				// void(entity e, float chan, string samp) sound = #8;
	{   9, "normalize", PF_normalize },		// vector(vector v) normalize			= #9;
	{  10, "error", PF_error },				// void(string e) error				= #10;
	{  11, "objerror", PF_objerror },		// void(string e) objerror				= #11;
	{  12, "vlen", PF_vlen },				// float(vector v) vlen				= #12;
	{  13, "vectoyaw", PF_vectoyaw },		// float(vector v) vectoyaw		= #13;
	{  14, "spawn", PF_Spawn },				// entity() spawn						= #14;
	{  15, "remove", PF_Remove },			// void(entity e) remove				= #15;
	{  16, "traceline", PF_traceline },		// float(vector v1, vector v2, float tryents) traceline = #16;
	{  17, "checkclient", PF_checkclient },	// entity() clientlist					= #17;
	{  18, "find", PF_Find },				// entity(entity start, .string fld, string match) find = #18;
	{  19, "precache_sound", PF_precache_sound },	// void(string s) precache_sound		= #19;
	{  20, "precache_model", PF_precache_model },	// void(string s) precache_model		= #20;
	{  21, "stuffcmd", PF_stuffcmd },		// void(entity client, string s)stuffcmd = #21;
	{  22, "findradius", PF_findradius },	// entity(vector org, float rad) findradius = #22;
	{  23, "bprint", PF_bprint },			// void(string s) bprint				= #23;
	{  24, "sprint", PF_sprint },			// void(entity client, string s) sprint = #24;
	{  25, "dprint", PF_dprint },			// void(string s) dprint				= #25;
	{  26, "ftos", PF_ftos },				// void(string s) ftos				= #26;
	{  27, "vtos", PF_vtos },				// void(string s) vtos				= #27;
	{  28, "coredump", PF_coredump },
	{  29, "traceon", PF_traceon },
	{  30, "traceoff", PF_traceoff },
	{  31, "eprint", PF_eprint },			// void(entity e) debug print an entire entity
	{  32, "walkmove", PF_walkmove },		// float(float yaw, float dist) walkmove
	{  33, "updateLimb", PF_updateLimb },
	{  34, "droptofloor", PF_droptofloor },
	{  35, "lightstyle", PF_lightstyle },
	{  36, "rint", PF_rint },
	{  37, "floor", PF_floor },
	{  38, "ceil", PF_ceil },
//	{  39, "fixme", PF_Fixme },
	{  40, "checkbottom", PF_checkbottom },
	{  41, "pointcontents", PF_pointcontents },
//	{  42, "fixme", PF_Fixme },
	{  43, "fabs", PF_fabs },
	{  44, "aim", PF_aim },
	{  45, "cvar", PF_cvar },
	{  46, "localcmd", PF_localcmd },
	{  47, "nextent", PF_nextent },
	{  48, "particle", PF_particle },
	{  49, "ChangeYaw", PF_changeyaw },
	{  50, "getSoundLen", PF_GetSoundLen },
	{  51, "vectoangles", PF_vectoangles },

	{  52, "WriteByte", PF_WriteByte },
	{  53, "WriteChar", PF_WriteChar },
	{  54, "WriteShort", PF_WriteShort },
	{  55, "WriteLong", PF_WriteLong },
	{  56, "WriteCoord", PF_WriteCoord },
	{  57, "WriteAngle", PF_WriteAngle },
	{  58, "WriteString", PF_WriteString },
	{  59, "WriteEntity", PF_WriteEntity },

	{  60, "sin", PF_sin },
	{  61, "cos", PF_cos },
	{  62, "sqrt", PF_sqrt },
	{  65, "etos", PF_etos },

	{  67, "movetogoal", SV_MoveToGoal },
	{  68, "precache_file", PF_precache_file },
	{  69, "makestatic", PF_makestatic },

	{  70, "changelevel", PF_changelevel },
	{  71, "movetoorigin", SV_MoveToOrigin },

	{  72, "cvar_set", PF_cvar_set },
	{  73, "centerprint", PF_centerprint },

	{  74, "ambientsound", PF_ambientsound },

	{  75, "precache_model2", PF_precache_model },
	{  76, "precache_sound2", PF_precache_sound },	// precache_sound2 is different only for qcc
	{  77, "precache_file2", PF_precache_file },

	{  78, "setspawnparms", PF_setspawnparms },
	{  79, "achievement", PF_achievement },

	{  81, "stof", PF_stof },	// 2001-09-20 QuakeC string manipulation by FrikaC/Maddes

	{  82, "Set_Zombie", PF_Set_Zombie },	//set zombies stats
	{  83, "Get_Waypoint_Near", Get_Waypoint_Near }, //returns the cords for closest waypoint
	{  84, "Do_Pathfind", Do_Pathfind }, //starts the magic
	{  85, "Open_Waypoint", Open_Waypoint }, //Opens a waypoint
	{  86, "Get_Next_Waypoint", Get_Next_Waypoint }, //Get next waypoint
	{  87, "useprint", PF_useprint }, //Print with button
	{  88, "Get_First_Waypoint", Get_First_Waypoint },//Get the first waypoint in the list
	{  89, "Close_Waypoint", Close_Waypoint }, //Closes a waypoint

// 2001-11-15 DarkPlaces general builtin functions by Lord Havoc  start
// not implemented yet


	{  90, "tracebox", PF_tracebox },
	{  99, "tracemove", PF_tracemove },//blubs improved tracebox
/*	{  91, "randomvec", PF_randomvec },
	{  92, "getlight", PF_GetLight },	// not implemented yet
	{  93, "cvar_create", PF_cvar_create },		// 2001-09-18 New BuiltIn Function: cvar_create() by Maddes
	{  94, "fmin", PF_fmin },
	{  95, "fmax", PF_fmax },
	{  96, "fbound", PF_fbound },
	{  97, "fpow", PF_fpow },
	*/{  98, "findfloat", PF_FindFloat },/*
	{ PR_DEFAULT_FUNCNO_EXTENSION_FIND, "extension_find", PF_extension_find },	// 2001-10-20 Extension System by Lord Havoc/Maddes
	{   0, "registercvar", PF_cvar_create },	// 0 indicates that this entry is just for remapping (because of name change)
	{   0, "checkextension", PF_extension_find },
*/
// 2001-11-15 DarkPlaces general builtin functions by Lord Havoc  end

	{ PR_DEFAULT_FUNCNO_BUILTIN_FIND, "builtin_find", PF_builtin_find },	// 2001-09-14 Enhanced BuiltIn Function System (EBFS) by Maddes

// not implemented yet
/*
	{ 101, "cmd_find", PF_cmd_find },		// 2001-09-16 New BuiltIn Function: cmd_find() by Maddes

	{ 102, "cvar_find", PF_cvar_find },		// 2001-09-16 New BuiltIn Function: cvar_find() by Maddes

	{ 103, "cvar_string", PF_cvar_string },	// 2001-09-16 New BuiltIn Function: cvar_string() by Maddes

	{ 105, "cvar_free", PF_cvar_free },		// 2001-09-18 New BuiltIn Function: cvar_free() by Maddes

	{ 106, "NVS_InitSVCMsg", PF_NVS_InitSVCMsg },	// 2000-05-02 NVS SVC by Maddes

	{ 107, "WriteFloat", PF_WriteFloat },	// 2001-09-16 New BuiltIn Function: WriteFloat() by Maddes

	{ 108, "etof", PF_etof },	// 2001-09-25 New BuiltIn Function: etof() by Maddes

	{ 109, "ftoe", PF_ftoe },	// 2001-09-25 New BuiltIn Function: ftoe() by Maddes
*/

// 2001-09-20 QuakeC file access by FrikaC/Maddes  start
// not implemented yet

	{ 110, "fopen", PF_fopen },
	{ 111, "fclose", PF_fclose },
	{ 112, "fgets", PF_fgets },
	{ 113, "fputs", PF_fputs },
	{   0, "open", PF_fopen },		// 0 indicates that this entry is just for remapping (because of name and number change)
	{   0, "close", PF_fclose },
	{   0, "read", PF_fgets },
	{   0, "write", PF_fputs },

// 2001-09-20 QuakeC file access by FrikaC/Maddes  end

// 2001-09-20 QuakeC string manipulation by FrikaC/Maddes  start
// not implemented yet

	{ 114, "strlen", PF_strlen },
	{ 115, "strcat", PF_strcat },
	{ 116, "substring", PF_substring },
	{ 117, "stov", PF_stov },
	{ 118, "strzone", PF_strzone },
	{ 119, "strunzone", PF_strunzone },
	{ 120, "strtrim", PF_strtrim },
	{   0, "zone", PF_strzone },		// 0 indicates that this entry is just for remapping (because of name and number change)
	{   0, "unzone", PF_strunzone },

// 2001-09-20 QuakeC string manipulation by FrikaC/Maddes  end

// 2001-11-15 DarkPlaces general builtin functions by Lord Havoc  start
// not implemented yet
/*
	{ 400, "copyentity", PF_... },
	{ 401, "setcolor", PF_... },
	{ 402, "findchain", PF_... },
	{ 403, "findchainfloat", PF_... },
	{ 404, "effect", PF_... },
	{ 405, "te_blood", PF_... },
	{ 406, "te_bloodshower", PF_... },
	{ 407, "te_explosionrgb", PF_... },
	{ 408, "te_particlecube", PF_... },
	{ 409, "te_particlerain", PF_... },
	{ 410, "te_particlesnow", PF_... },
	{ 411, "te_spark", PF_... },
	{ 412, "te_gunshotquad", PF_... },
	{ 413, "te_spikequad", PF_... },
	{ 414, "te_superspikequad", PF_... },
	{ 415, "te_explosionquad", PF_... },
	{ 416, "te_smallflash", PF_... }
	{ 417, "te_customflash", PF_... },
	{ 418, "te_gunshot", PF_... },
	{ 419, "te_spike", PF_... },
	{ 420, "te_superspike", PF_... },
	{ 421, "te_explosion", PF_... },
	{ 422, "te_tarexplosion", PF_... },
	{ 423, "te_wizspike", PF_... },
	{ 424, "te_knightspike", PF_... },
	{ 425, "te_lavasplash", PF_... }
	{ 426, "te_teleport", PF_... },
	{ 427, "te_explosion2", PF_... },
	{ 428, "te_lightning1", PF_... },
	{ 429, "te_lightning2", PF_... },
	{ 430, "te_lightning3", PF_... },
	{ 431, "te_beam", PF_... },
	{ 432, "vectorvectors", PF_... },*/
  { 441, "tokenize", PF_tokenize },
  { 442, "argv", PF_ArgV },
  { 480, "strtolower", PF_strtolower },
  { 494, "crc16", PF_crc16 },

  { 500, "songegg", PF_SongEgg },
  {	501, "nzp_maxammo", PF_MaxAmmo },
  { 502, "grenade_pulse", PF_GrenadePulse },
  { 503, "nzp_maxai", PF_MaxZombies },
  { 504, "nzp_bettyprompt", PF_BettyPrompt },
  { 505, "nzp_setplayername", PF_SetPlayerName },
  { 506, "nzp_setdoubletapver", PF_SetDoubleTapVersion },
  { 507, "nzp_screenflash", PF_ScreenFlash },
  { 508, "nzp_lockviewmodel", PF_LockViewmodel },
  { 509, "nzp_rumble", PF_Rumble }

// 2001-11-15 DarkPlaces general builtin functions by Lord Havoc  end

};

int pr_ebfs_numbuiltins = sizeof(pr_ebfs_builtins)/sizeof(pr_ebfs_builtins[0]);
// 2001-09-14 Enhanced BuiltIn Function System (EBFS) by Maddes  end

