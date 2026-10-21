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

#include "../nzportable_def.h"

#include <ctype.h>

#ifdef __WII__
#include <ogc/lwp_watchdog.h>
#include <wiiuse/wpad.h>
#include <ctype.h>
#endif // __WII__

#define PR_MAX_TEMPSTRING 2048	// 2001-10-25 Enhanced temp string handling by Maddes
#define	RETURN_EDICT(e) (((int *)pr_globals)[OFS_RETURN] = EDICT_TO_PROG(e))

extern cvar_t sv_maxai;
extern cvar_t sv_spoofmonth;

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
		if (maxlen > (int)strlen(add))
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
    Sys_BeginErrorReport();
	Con_Printf ("======SERVER ERROR in %s:\n%s\n"
	,PR_GetString(pr_xfunction->s_name),s);
    Con_Printf("\n--- QC stack trace ---\n");
    PR_StackTrace();
    Con_Printf("\n--- self ---\n");
	ed = PROG_TO_EDICT(pr_global_struct->self);
	ED_Print (ed);

	Host_Error ("Program error (server): %s", s);
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
    Sys_BeginErrorReport();
	Con_Printf ("======OBJECT ERROR in %s:\n%s\n"
	,PR_GetString(pr_xfunction->s_name),s);
    Con_Printf("\n--- QC stack trace ---\n");
    PR_StackTrace();
    Con_Printf("\n--- self ---\n");
	ed = PROG_TO_EDICT(pr_global_struct->self);
	ED_Print (ed);
	ED_Free (ed);

	Host_Error ("Program error (object): %s", s);
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

		a = angles[1]/180 * (float)M_PI;

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


	e->v.model = PR_SetString(m);
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
	s = PF_VarString(2);

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

useprint(entity client, float index, float cost)
=================
*/
void PF_useprint (void)
{
	client_t	*client;
	int			entnum, index, cost;

	entnum = G_EDICTNUM(OFS_PARM0);
	index = G_FLOAT(OFS_PARM1);
	cost = G_FLOAT(OFS_PARM2);


	if (entnum < 1 || entnum > svs.maxclients)
	{
		Con_Printf ("tried to sprint to a non-client\n");
		return;
	}

	client = &svs.clients[entnum-1];

	MSG_WriteByte (&client->message,svc_useprint);
	MSG_WriteByte (&client->message,index);
	MSG_WriteShort (&client->message,cost);
}

static char *pr_useprint_strings[256];
static byte pr_useprint_colors[256][3];
static qboolean pr_useprint_registered[256];

void PR_ClearRegisteredUseprints (void)
{
	int index;

	for (index = 0; index < 256; index++)
	{
		if (pr_useprint_strings[index])
			Z_Free (pr_useprint_strings[index]);
		pr_useprint_strings[index] = NULL;
	}
	memset (pr_useprint_registered, 0, sizeof(pr_useprint_registered));
}

static void PR_WriteUseprintDefinition (client_t *client, int index)
{
	MSG_WriteByte (&client->message, svc_registeruseprint);
	MSG_WriteByte (&client->message, index);
	MSG_WriteString (&client->message, pr_useprint_strings[index]);
	MSG_WriteByte (&client->message, pr_useprint_colors[index][0]);
	MSG_WriteByte (&client->message, pr_useprint_colors[index][1]);
	MSG_WriteByte (&client->message, pr_useprint_colors[index][2]);
}

void PR_SendRegisteredUseprints (client_t *client)
{
	int index;

	for (index = 0; index < 256; index++)
		if (pr_useprint_registered[index])
			PR_WriteUseprintDefinition (client, index);
}

void PF_useprint_send (void)
{
	client_t *client;
	char *text, *text_copy;
	int entnum, index;
	float *color;

	entnum = G_EDICTNUM(OFS_PARM0);
	index = G_FLOAT(OFS_PARM1);
	color = G_VECTOR(OFS_PARM3);

	if (entnum < 1 || entnum > svs.maxclients)
	{
		Con_Printf ("useprint_send: not a client\n");
		return;
	}
	if (index < 0 || index > 255)
	{
		Con_Printf ("useprint_send: index %i out of range\n", index);
		return;
	}

	client = &svs.clients[entnum-1];
	text = G_STRING(OFS_PARM2);
	text_copy = Z_Strdup(text);
	if (pr_useprint_strings[index])
		Z_Free(pr_useprint_strings[index]);
	pr_useprint_strings[index] = text_copy;
	pr_useprint_colors[index][0] = CLAMP(0, color[0] * 255, 255);
	pr_useprint_colors[index][1] = CLAMP(0, color[1] * 255, 255);
	pr_useprint_colors[index][2] = CLAMP(0, color[2] * 255, 255);
	pr_useprint_registered[index] = true;

	if (client->spawned)
		PR_WriteUseprintDefinition (client, index);
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
		yaw = (int) (atan2f(value1[1], value1[0]) * 180 / (float)M_PI);
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
		yaw = (int) (atan2f(value1[1], value1[0]) * 180 / (float)M_PI);
		if (yaw < 0)
			yaw += 360;

		forward = sqrtf (value1[0]*value1[0] + value1[1]*value1[1]);
		pitch = (int) (atan2f(value1[2], forward) * 180 / (float)M_PI);
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

	num = (rand ()&0x7fff) / 32768.0f;

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

	MSG_WriteShort (&sv.signon, soundnum);

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
	float speed, soundflags;
	int speedpct, flags;

	entity = G_EDICT(OFS_PARM0);
	channel = G_FLOAT(OFS_PARM1);
	sample = G_STRING(OFS_PARM2);
	volume = G_FLOAT(OFS_PARM3) * 255;
	attenuation = G_FLOAT(OFS_PARM4);

	// Sound flags and pitch shifting from FTE, as optional parms
	speed = pr_argc > 5 ? G_FLOAT(OFS_PARM5) : 0;

	if (speed == 0)
		speed = DEFAULT_SOUND_PACKET_SPEED;

	if (!isfinite(speed) || speed < 1 || speed > 255)
		PR_RunError ("sound: speedpct must be between 1 and 255 (or 0 for normal)");

	speedpct = (int)(speed + 0.5f);
	soundflags = pr_argc > 6 ? G_FLOAT(OFS_PARM6) : 0;

	if (!isfinite(soundflags) || soundflags < 0 || soundflags > 65535)
		PR_RunError ("sound: invalid flags");
	
	flags = (int)soundflags & (SOUNDFLAG_FOLLOW | SOUNDFLAG_NODISTANCE);

	if (volume < 0 || volume > 255)
		Sys_Error ("volume = %i", volume);

	if (attenuation < 0 || attenuation > 4)
		Sys_Error ("attenuation = %f", (double)attenuation);

	if (channel < 0 || channel > 7)
		Sys_Error ("channel = %i", channel);

	SV_StartSound (entity, channel, sample, volume, attenuation, speedpct, flags);
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
abort();	// dump to debugger
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

static int TraceMoveInternal(vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int type, edict_t *ent, qboolean sliding, float *remaining_distance)
{
	vec3_t position, next, up, down, delta;
	float dx = end[0] - start[0];
	float dy = end[1] - start[1];
	float distance = sqrtf(dx * dx + dy * dy);
	int steps;
	trace_t trace;
	qboolean slid = false;

	if (!isfinite(distance) || distance > 65536)
		return 0;

	float stride = sliding ? 8 : 4;
	steps = (int)ceilf(distance / (stride * 0.5f)) + 8;
	VectorCopy(start, position);
	position[2] += 0.03125f;

	for (int i = 0; i < steps; i++) {
		VectorSubtract(end, position, delta);
		delta[2] = 0;
		distance = VectorNormalize(delta);

		if (distance > stride * 4 && !(i & 7)) {
			VectorCopy(end, next);
			next[2] = position[2];
			trace = SV_Move(position, mins, maxs, next, type, ent);

			if (!trace.startsolid && !trace.allsolid && (!slid || trace.fraction == 1)) {
				float advance = floorf((distance * trace.fraction - stride) / stride) * stride;

				while (advance >= stride * 4) {
					VectorMA(position, advance, delta, next);

					vec3_t support_start, support_end;
					VectorCopy(position, support_start);
					VectorCopy(next, support_end);
					support_start[2] += mins[2] - 0.125f;
					support_end[2] = support_start[2];
					trace = SV_Move(support_start, vec3_origin, vec3_origin, support_end, MOVE_NOMONSTERS, ent);

					if (trace.allsolid) {
						VectorCopy(next, position);
						VectorSubtract(end, position, delta);
						delta[2] = 0;
						distance = VectorNormalize(delta);
						break;
					}

					advance = floorf(advance * 0.5f / stride) * stride;
				}
			}
		}

		if (remaining_distance)
			*remaining_distance = distance;

		VectorMA(position, fminf(stride, distance), delta, next);
		trace = SV_Move(position, mins, maxs, next, type, ent);

		if (trace.startsolid || trace.allsolid)
			return 0;

		if (trace.fraction < 1) {
			trace_t blocked = trace;
			VectorCopy(position, up);
			up[2] += 20;
			trace = SV_Move(position, mins, maxs, up, type, ent);

			if (!trace.startsolid && !trace.allsolid) {
				VectorCopy(trace.endpos, up);

				vec3_t step;
				VectorCopy(next, step);
				step[2] = up[2];
				trace = SV_Move(up, mins, maxs, step, type, ent);
			}

			if (trace.startsolid || trace.allsolid || trace.fraction < 1) {
				trace = blocked;

				float remaining_x = end[0] - trace.endpos[0];
				float remaining_y = end[1] - trace.endpos[1];

				if (!sliding && remaining_x * remaining_x + remaining_y * remaining_y >= 16)
					return 0;

				for (int bump = 0; sliding && bump < 4 && trace.fraction < 1; bump++) {
					vec3_t slide, remaining;
					slid = true;
					VectorSubtract(next, trace.endpos, remaining);

					float into = DotProduct(remaining, trace.plane.normal);
					VectorMA(remaining, -into, trace.plane.normal, remaining);
					VectorAdd(trace.endpos, remaining, slide);
					trace = SV_Move(trace.endpos, mins, maxs, slide, type, ent);

					if (trace.startsolid || trace.allsolid)
						return 0;

					VectorCopy(slide, next);
				}
			}
		}

		VectorCopy(trace.endpos, next);
		VectorCopy(next, down);
		down[2] -= fmaxf(512, start[2] - end[2] + 20);
		trace = SV_Move(next, mins, maxs, down, type, ent);

		if (trace.startsolid || trace.allsolid || trace.fraction == 1 || trace.plane.normal[2] <= 0.7f)
			return 0;

		VectorSubtract(trace.endpos, position, delta);
		VectorCopy(trace.endpos, position);
		dx = end[0] - position[0];
		dy = end[1] - position[1];

		if (dx * dx + dy * dy < 16 && fabsf(position[2] - end[2]) < 20)
			return 1;

		if (DotProduct(delta, delta) < 0.0001f)
			return 0;
	}

	return 0;
}

int TraceMove(vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int type, edict_t *ent)
{
	return TraceMoveInternal(start, mins, maxs, end, type, ent, true, NULL);
}

static edict_t *waypoint_origin_actor;
static signed char waypoint_origin_state[MAX_WAYPOINTS];
static vec3_t waypoint_origins[MAX_WAYPOINTS];

static qboolean WaypointGroundOriginInternal(int waypoint, edict_t *ent, vec3_t position)
{
	vec3_t anchor, down;
	trace_t trace;

	VectorCopy(waypoints[waypoint].origin, anchor);
	anchor[2] += 0.03125f;
	trace = SV_Move(anchor, ai_hull_mins, ai_hull_maxs, anchor, MOVE_NOMONSTERS, ent);

	if (!trace.startsolid && !trace.allsolid) {
		VectorCopy(anchor, down);
		down[2] -= 64;
		trace = SV_Move(anchor, ai_hull_mins, ai_hull_maxs, down, MOVE_NOMONSTERS, ent);

		if (!trace.startsolid && !trace.allsolid && trace.fraction < 1 && trace.plane.normal[2] > 0.7f && fabsf(trace.endpos[2] - waypoints[waypoint].origin[2]) < 50) {
			VectorCopy(trace.endpos, position);
		} else {
			VectorCopy(anchor, position);
		}

		return true;
	}

	VectorCopy(anchor, down);
	down[2] -= 128;
	trace = SV_Move(anchor, vec3_origin, vec3_origin, down, MOVE_NOMONSTERS, ent);

	if (!trace.startsolid && !trace.allsolid && trace.fraction < 1 && trace.plane.normal[2] > 0.7f && fabsf(trace.endpos[2] - ai_hull_mins[2] - waypoints[waypoint].origin[2]) < 50) {
		VectorCopy(trace.endpos, anchor);
		anchor[2] -= ai_hull_mins[2];
	}

	for (int height = 0; height <= 20; height++) {
		for (int offset = 0; offset < 9; offset++) {
			vec3_t candidate;
			VectorCopy(anchor, candidate);

			if (offset) {
				int index = offset - 1;

				if (index >= 4)
					index++;
				candidate[0] += (index / 3 - 1) * 0.125f;
				candidate[1] += (index % 3 - 1) * 0.125f;
			}

			candidate[2] += height;
			trace = SV_Move(candidate, ai_hull_mins, ai_hull_maxs, candidate, MOVE_NOMONSTERS, ent);

			if (trace.startsolid || trace.allsolid)
				continue;

			VectorCopy(candidate, down);
			down[2] -= 64;
			trace = SV_Move(candidate, ai_hull_mins, ai_hull_maxs, down, MOVE_NOMONSTERS, ent);

			if (!trace.startsolid && !trace.allsolid && trace.fraction < 1 && trace.plane.normal[2] > 0.7f && fabsf(trace.endpos[2] - waypoints[waypoint].origin[2]) < 50) {
				VectorCopy(trace.endpos, position);
				return true;
			}
		}
	}

	for (int radius = 1; radius < 16; radius++) {
		for (int x = -3; x <= 3; x++) {
			for (int y = -3; y <= 3; y++) {
				if (x * x + y * y != radius)
					continue;

				for (int height = 0; height <= 20; height++) {
					vec3_t candidate;
					VectorCopy(anchor, candidate);
					candidate[0] += x;
					candidate[1] += y;
					candidate[2] += height;
					trace = SV_Move(candidate, ai_hull_mins, ai_hull_maxs, candidate, MOVE_NOMONSTERS, ent);

					if (trace.startsolid || trace.allsolid)
						continue;

					VectorCopy(candidate, down);
					down[2] -= 64;
					trace = SV_Move(candidate, ai_hull_mins, ai_hull_maxs, down, MOVE_NOMONSTERS, ent);

					if (!trace.startsolid && !trace.allsolid && trace.fraction < 1 && trace.plane.normal[2] > 0.7f && fabsf(trace.endpos[2] - waypoints[waypoint].origin[2]) < 50) {
						VectorCopy(trace.endpos, position);
						return true;
					}
				}
			}
		}
	}

	return false;
}

qboolean WaypointGroundOrigin(int waypoint, edict_t *ent, vec3_t position)
{
	if (!waypoint_origin_actor || waypoint_origin_actor != ent)
		return WaypointGroundOriginInternal(waypoint, ent, position);

	if (waypoint_origin_state[waypoint] < 0) {
		waypoint_origin_state[waypoint] = WaypointGroundOriginInternal(waypoint, ent, waypoint_origins[waypoint]);
	}

	if (waypoint_origin_state[waypoint]) {
		VectorCopy(waypoint_origins[waypoint], position);
		return true;
	}

	return false;
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
	if (ent->v.movetype == MOVETYPE_WALK && NUM_FOR_EDICT(ent) > svs.maxclients) {
		mins = ai_hull_mins;
		maxs = ai_hull_maxs;
	}

	// Con_DPrintf ("TraceMove start, ");
	G_FLOAT(OFS_RETURN) = TraceMove(start, mins, maxs, end,nomonsters,ent);
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
	if (vid_headless && !Q_strcasecmp(str, "quit\n"))
	{
		Sys_Quit();
		return;
	}
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

void PF_strstrofs(void)
{
	const char *text = G_STRING(OFS_PARM0);
	const char *match = G_STRING(OFS_PARM1);
	int start = (int)G_FLOAT(OFS_PARM2);
	const char *found;
	if (start < 0) start = 0;
	if ((size_t)start > strlen(text)) {
		G_FLOAT(OFS_RETURN) = -1;
		return;
	}
	found = strstr(text + start, match);
	G_FLOAT(OFS_RETURN) = found ? (float)(found - text) : -1;
}

/*
=================
PF_cvar_set

float cvar (string)
=================
*/

void PF_cvar_string (void)
{
	G_INT(OFS_RETURN) = PR_SetString((char *)Cvar_VariableString(G_STRING(OFS_PARM0)));
}

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
	const char *classname = pr_argc > 2 ? G_STRING(OFS_PARM2) : NULL;
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
		if (classname && classname[0] && strcmp(PR_GetString(ent->v.classname), classname))
			continue;
		for (j=0 ; j<3 ; j++)
			eorg[j] = org[j] - (ent->v.origin[j] + (ent->v.mins[j] + ent->v.maxs[j])*0.5f);

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
	if (sys_testmode.value > 0)
		return;

	Con_DPrintf ("%s",PF_VarString(0));
}

/* FTE/DarkPlaces console output, used by SSQC test harnesses. */
void PF_print (void)
{
	Con_Printf("%s", PF_VarString(0));
}

void PF_gettime (void)
{
	G_FLOAT(OFS_RETURN) = (float)Sys_FloatTime();
}

#define PR_SPRINTF_BUFFERS 64
static char pr_sprintf_buffers[PR_SPRINTF_BUFFERS][PR_MAX_TEMPSTRING];
static unsigned int pr_sprintf_buffer_index;

static void PF_sprintf_append(char **out, size_t *remaining, const char *text)
{
	size_t length;

	if (*remaining <= 1)
		return;
	length = strlen(text);
	if (length >= *remaining)
		length = *remaining - 1;
	memcpy(*out, text, length);
	*out += length;
	*remaining -= length;
	**out = 0;
}

/*
 * DP_QC_SPRINTF.  QC arguments occupy three globals apiece regardless of
 * their type.  This covers the scalar/string conversions used by NZ:P QC,
 * while preserving normal printf flags, widths, and precisions.
 */
void PF_sprintf (void)
{
	const char *format = G_STRING(OFS_PARM0);
	char *result = pr_sprintf_buffers[pr_sprintf_buffer_index++ % PR_SPRINTF_BUFFERS];
	char *out = result;
	size_t remaining = PR_MAX_TEMPSTRING;
	int argument = 1;

	result[0] = 0;
	while (*format && remaining > 1)
	{
		char conversion[32];
		char rendered[512];
		char *spec;
		char code;
		qboolean integer_argument = false;

		if (*format != '%')
		{
			*out++ = *format++;
			*out = 0;
			--remaining;
			continue;
		}
		if (format[1] == '%')
		{
			*out++ = '%';
			*out = 0;
			remaining--;
			format += 2;
			continue;
		}

		spec = conversion;
		*spec++ = *format++;
		while (*format && strchr("-+ #0", *format) && spec < conversion + sizeof(conversion) - 3)
			*spec++ = *format++;
		while (*format && isdigit((unsigned char)*format) && spec < conversion + sizeof(conversion) - 3)
			*spec++ = *format++;
		if (*format == '.' && spec < conversion + sizeof(conversion) - 3)
		{
			*spec++ = *format++;
			while (*format && isdigit((unsigned char)*format) && spec < conversion + sizeof(conversion) - 3)
				*spec++ = *format++;
		}
		if (*format == 'l')
		{
			integer_argument = true;
			format++;
		}
		else if (*format == 'h')
			format++;
		code = *format ? *format++ : 0;
		if (!code || argument >= pr_argc)
			break;

		if (code == 's' || code == 'S')
		{
			*spec++ = 's';
			*spec = 0;
			snprintf(rendered, sizeof(rendered), conversion,
				G_STRING(OFS_PARM0 + argument * 3));
		}
		else if (code == 'd' || code == 'c' || code == 'x' || code == 'X')
		{
			*spec++ = code;
			*spec = 0;
			snprintf(rendered, sizeof(rendered), conversion,
				integer_argument ? G_INT(OFS_PARM0 + argument * 3) :
				(int)G_FLOAT(OFS_PARM0 + argument * 3));
		}
		else if (strchr("eEfFgG", code))
		{
			*spec++ = code;
			*spec = 0;
			snprintf(rendered, sizeof(rendered), conversion,
				(double)G_FLOAT(OFS_PARM0 + argument * 3));
		}
		else
			snprintf(rendered, sizeof(rendered), "%%%c", code);

		PF_sprintf_append(&out, &remaining, rendered);
		argument++;
	}
	G_INT(OFS_RETURN) = PR_SetString(result);
}

char	pr_string_temp[PR_MAX_TEMPSTRING];	// 2001-10-25 Enhanced temp string handling by Maddes

void PF_ftos (void)
{
	float	v;
	v = G_FLOAT(OFS_PARM0);

	if (v == (int)v)
		sprintf (pr_string_temp, "%d",(int)v);
	else
		sprintf (pr_string_temp, "%5.1f",(double)v);
	G_INT(OFS_RETURN) = PR_SetString(pr_string_temp);
}

void PF_fabs (void)
{
	float	v;
	v = G_FLOAT(OFS_PARM0);
	G_FLOAT(OFS_RETURN) = fabsf(v);
}

void PF_vtos (void)
{
	sprintf (pr_string_temp, "'%5.1f %5.1f %5.1f'",
		(double)G_VECTOR(OFS_PARM0)[0], (double)G_VECTOR(OFS_PARM0)[1], (double)G_VECTOR(OFS_PARM0)[2]);
	G_INT(OFS_RETURN) = PR_SetString(pr_string_temp);
}

void PF_etos (void)
{
	sprintf (pr_string_temp, "entity %i", G_EDICTNUM(OFS_PARM0));
	G_INT(OFS_RETURN) = PR_SetString(pr_string_temp);
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
	G_INT(OFS_RETURN) = PR_SetString(PR_ZoneString(G_STRING(OFS_PARM0)));
}

/*
=================
PF_strunzone

string strunzone (string)
=================
*/
void PF_strunzone (void)
{
	PR_UnzoneString(G_STRING(OFS_PARM0));
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
	int		length;
	char	*str;
	char 	*end;

	str = G_STRING(OFS_PARM0);

	// figure out the new start
	while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') {
		str++;
	}

	// figure out the new end.
	end = str + strlen (str);
	while (end > str && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' || end[-1] == '\r'))
		end--;

	length = end - str;

// 2001-10-25 Enhanced temp string handling by Maddes  start
	if (length >= PR_MAX_TEMPSTRING)
		length = PR_MAX_TEMPSTRING-1;
// 2001-10-25 Enhanced temp string handling by Maddes  end
	if (length < 0)
		length = 0;

	//str += offset;
	memmove(pr_string_temp, str, length);
	pr_string_temp[length] = 0;

	G_INT(OFS_RETURN) = PR_SetString(pr_string_temp);
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
	char	result[PR_MAX_TEMPSTRING];
	size_t	s1len, s2len;

	s1 = G_STRING(OFS_PARM0);
	s2 = PF_VarString(1);

	s1len = strlen(s1);
	if (s1len >= sizeof(result))
		s1len = sizeof(result) - 1;
	memcpy(result, s1, s1len);

	s2len = strlen(s2);
	if (s2len > sizeof(result) - s1len - 1)
		s2len = sizeof(result) - s1len - 1;
	memcpy(result + s1len, s2, s2len);
	result[s1len + s2len] = 0;

	memcpy(pr_string_temp, result, s1len + s2len + 1);

	G_INT(OFS_RETURN) = PR_SetString(pr_string_temp);
}

/*
=================
PF_substring

string substring (string, float, float)
=================
*/
void PF_substring (void)
{
	int		offset, length, copy_length;
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
	copy_length = maxoffset - offset;
	if (copy_length > length)
		copy_length = length;
	memmove(pr_string_temp, p, copy_length);
	pr_string_temp[copy_length] = 0;

	G_INT(OFS_RETURN) = PR_SetString(pr_string_temp);
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
	int len;

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
	len = strlen(s);
	for(int i = 0; i < len; i++)
	{
  		if (pr_string_temp[i] >= 'A' && pr_string_temp[i] <= 'Z')
		    pr_string_temp[i] = pr_string_temp[i] - 'a'-'A'; 
	}

	G_INT(OFS_RETURN) = PR_SetString(pr_string_temp);
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

#define WAYPOINT_SET_NONE 	0
#define WAYPOINT_SET_OPEN 	1
#define WAYPOINT_SET_CLOSED	2


char waypoint_set[MAX_WAYPOINTS]; // waypoint_set[i] contains the set identifier for the i-th waypoint
unsigned short openset_waypoints[MAX_WAYPOINTS]; // List of waypoints currently in the open set sorted by heuristic cost (index 0 contains lowest cost waypoint)
unsigned short openset_length; // Current length of the open set
zombie_ai zombie_list[MAX_AI_COUNT];
static edict_t *pathfind_actor;
static signed char pathfind_walkable[MAX_WAYPOINTS][8];
static float *pathfind_entries;
static float path_entry_cost[MAX_WAYPOINTS];
static vec3_t path_entry_point[MAX_WAYPOINTS], path_entry_start[MAX_WAYPOINTS];
static float path_entry_distance[MAX_WAYPOINTS];
static vec3_t path_entry_basis;
static qboolean path_entry_basis_valid;

typedef struct {
	edict_t *entity;
	vec3_t origin, angles, mins, maxs, absmin, absmax;
	float modelindex, owner, flags, size_x, movetype;
} pathfind_brush_state_t;

static pathfind_brush_state_t pathfind_brushes[MAX_EDICTS];
static vec3_t pathfind_node_origins[MAX_WAYPOINTS];
static int pathfind_node_links[MAX_WAYPOINTS][8];
static int pathfind_brush_count, pathfind_node_count;
static qboolean pathfind_geometry_valid, pathfind_sized_actor;
static unsigned int pathfind_geometry_revision, pathfind_gate_revision;
static int pathfind_node_open[MAX_WAYPOINTS];

typedef struct {
	int entnum;
	vec3_t start, goal;
	unsigned int revision;
	float until;
	qboolean valid;
} pathfind_direct_state_t;

static pathfind_direct_state_t pathfind_direct[MAX_AI_COUNT];

static struct {
	qboolean valid;
	vec3_t origin;
	unsigned int geometry_revision, gate_revision;
	int waypoint;
} pathfind_goals[8];
static unsigned int pathfind_goal_index;

static void PathfindPrepareGeometry(edict_t *actor)
{
	edict_t *owner = PROG_TO_EDICT(actor->v.owner);
	qboolean share = !Test_Pathfind_Active() && actor->v.solid != SOLID_BSP && (owner == sv.edicts || owner->v.solid != SOLID_BSP);
	qboolean sized = actor->v.size[0] != 0;
	qboolean reuse = share && pathfind_geometry_valid && sized == pathfind_sized_actor && n_waypoints == pathfind_node_count;
	int count = 0;

	for (int i = 1; i < sv.num_edicts; i++) {
		edict_t *ent = EDICT_NUM(i);

		if (ent->free || !ent->area.prev || ent->v.solid != SOLID_BSP)
			continue;

		pathfind_brush_state_t state;
		memset(&state, 0, sizeof(state));
		state.entity = ent;
		VectorCopy(ent->v.origin, state.origin);
		VectorCopy(ent->v.angles, state.angles);
		VectorCopy(ent->v.mins, state.mins);
		VectorCopy(ent->v.maxs, state.maxs);
		VectorCopy(ent->v.absmin, state.absmin);
		VectorCopy(ent->v.absmax, state.absmax);
		state.modelindex = ent->v.modelindex;
		state.owner = ent->v.owner;
		state.flags = ent->v.flags;
		state.size_x = ent->v.size[0];
		state.movetype = ent->v.movetype;

		if (ent->v.owner && PROG_TO_EDICT(ent->v.owner) == actor) {
			share = reuse = false;
		}

		if (memcmp(&pathfind_brushes[count], &state, sizeof(state))) {
			pathfind_brushes[count] = state;
			reuse = false;
		}
		count++;
	}

	if (count != pathfind_brush_count)
		reuse = false;

	for (int i = 0; i < n_waypoints; i++) {
		if (pathfind_node_open[i] != waypoints[i].open) {
			pathfind_node_open[i] = waypoints[i].open;
			pathfind_gate_revision++;
		}

		if (memcmp(pathfind_node_origins[i], waypoints[i].origin, sizeof(vec3_t)) || memcmp(pathfind_node_links[i], waypoints[i].target, sizeof(pathfind_node_links[i]))) {
			VectorCopy(waypoints[i].origin, pathfind_node_origins[i]);
			memcpy(pathfind_node_links[i], waypoints[i].target, sizeof(pathfind_node_links[i]));
			reuse = false;
		}
	}

	if (!reuse) {
		pathfind_geometry_revision++;
		memset(waypoint_origin_state, -1, sizeof(waypoint_origin_state));
		memset(pathfind_walkable, -1, sizeof(pathfind_walkable));
	}

	pathfind_brush_count = count;
	pathfind_node_count = n_waypoints;
	pathfind_sized_actor = sized;
	pathfind_geometry_valid = share;
}

void PR_ResetWaypointState (void)
{
	pathfind_geometry_valid = false;
	memset(pathfind_goals, 0, sizeof(pathfind_goals));
	pathfind_goal_index = 0;
	waypoint_origin_actor = NULL;
	memset (waypoint_set, 0, sizeof(waypoint_set));
	memset (openset_waypoints, 0, sizeof(openset_waypoints));
	openset_length = 0;
	memset (zombie_list, 0, sizeof(zombie_list));
	pathfind_actor = NULL;
	pathfind_entries = NULL;
}

void PR_ClearEntityPath (int entnum)
{
	pathfind_direct_state_t *direct = &pathfind_direct[entnum % MAX_AI_COUNT];
	if (direct->entnum == entnum)
		direct->valid = false;

	for (int i = 0; i < MAX_AI_COUNT; i++) {
		if (zombie_list[i].zombienum == entnum) {
			memset (&zombie_list[i], 0, sizeof(zombie_list[i]));
		}
	}
}


//
// Debugs prints the current sorted list of waypoints in the open set
//
void sv_way_print_sorted_open_set() {
	Con_Printf("Sorted open-set F-scores: ");
	for(int i = 0; i < openset_length; i++) {
		Con_Printf("%.0f, ",(double)waypoints[openset_waypoints[i]].f_score);
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
	return VecLength2(waypoints[waypoint_idx_a].origin, waypoints[waypoint_idx_b].origin);
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
static int sv_way_pathfind_internal(int start_way, int end_way) {
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

	if (pathfind_entries) {
		for (i = 0; i < n_waypoints; i++) {
			if (pathfind_entries[i] < 0)
				continue;
			waypoints[i].g_score = pathfind_entries[i];
			waypoints[i].f_score = pathfind_entries[i] + sv_way_heuristic_cost_estimate(i, end_way);
			sv_way_add_way_to_set(WAYPOINT_SET_OPEN, i);
		}
	} else {
		waypoints[start_way].g_score = 0;
		waypoints[start_way].f_score = sv_way_heuristic_cost_estimate(start_way, end_way);
		sv_way_add_way_to_set(WAYPOINT_SET_OPEN, start_way);
	}

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

			if (sv_way_in_set(WAYPOINT_SET_OPEN, neighbor_waypoint_idx) && !(tentative_f_score < waypoints[neighbor_waypoint_idx].f_score))
				continue;

			if (pathfind_actor && pathfind_walkable[current][i] == 0)
				continue;

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

int sv_way_pathfind(int start_way, int end_way)
{
	while (sv_way_pathfind_internal(start_way, end_way)) {
		if (!pathfind_actor)
			return 1;

		qboolean valid = true;

		for (int i = process_list_length - 1; i > 0; i--) {
			int node = process_list[i];
			int next = process_list[i - 1];
			int edge = 0;

			while (edge < 8 && waypoints[node].target[edge] != next)
				edge++;

			if (edge == 8)
				return 0;

			if (pathfind_walkable[node][edge] < 0) {
				vec3_t origin, end;
				pathfind_walkable[node][edge] = WaypointGroundOrigin(node, pathfind_actor, origin)
					&& WaypointGroundOrigin(next, pathfind_actor, end)
					&& TraceMove(origin, ai_hull_mins, ai_hull_maxs, end, MOVE_NOMONSTERS, pathfind_actor);
			}

			if (!pathfind_walkable[node][edge]) {
				valid = false;
				break;
			}
		}

		if (valid)
			return 1;
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
	Con_DPrintf("'%5.1f %5.1f %5.1f', %f is %f, (%i, %i)\n",
		(double)waypoints[best].origin[0], (double)waypoints[best].origin[1], (double)waypoints[best].origin[2],
		(double)best_dist, (double)dist, i, best);
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



// 
// Returns true iff we can tracebox from (start + [0,0,ofs]) to (end + [0,0,ofs])
//

// Dynamic hull sizes for hit detection cause chaos on movement code. Treat all AI ents as same size as player hull for movement
vec3_t ai_hull_mins = {-16, -16, -32};
vec3_t ai_hull_maxs = { 16,  16,  40};

static qboolean PathfindDirectAllowed(edict_t *ent)
{
	eval_t *retry = GETEDICTFIELDVALUE(ent, eval_path_direct_retry_time);
	return !retry || pr_global_struct->time >= retry->_float;
}

qboolean TraceMoveToGoal(vec3_t start, vec3_t goal, edict_t *ent, vec3_t result)
{
	vec3_t delta;
	float distance, remaining_distance = -1;

	VectorCopy(goal, result);

	if (TraceMoveInternal(start, ai_hull_mins, ai_hull_maxs, goal, MOVE_NOMONSTERS, ent, false, &remaining_distance))
		return true;

	VectorSubtract(goal, start, delta);
	distance = sqrtf(delta[0] * delta[0] + delta[1] * delta[1]);

	if (distance <= 48)
		return fabsf(delta[2]) < 20;

	result[0] -= delta[0] * 48 / distance;
	result[1] -= delta[1] * 48 / distance;

	if (remaining_distance > 64)
		return false;

	return TraceMoveInternal(start, ai_hull_mins, ai_hull_maxs, result, MOVE_NOMONSTERS, ent, false, NULL);
}

static qboolean PathfindDirectToGoal(vec3_t start, vec3_t goal, edict_t *ent, vec3_t result)
{
	int entnum = NUM_FOR_EDICT(ent);
	pathfind_direct_state_t *direct = &pathfind_direct[entnum % MAX_AI_COUNT];
	qboolean prepared = false;
	VectorCopy(goal, result);

	if (direct->valid && direct->entnum == entnum && pr_global_struct->time < direct->until && VectorCompare(start, direct->start) && VectorCompare(goal, direct->goal)) {
		PathfindPrepareGeometry(ent);
		prepared = true;
		if (direct->revision == pathfind_geometry_revision)
			return false;
	}

	direct->valid = false;
	if (TraceMoveToGoal(start, goal, ent, result))
		return true;

	if (!prepared)
		PathfindPrepareGeometry(ent);

	direct->entnum = entnum;
	VectorCopy(start, direct->start);
	VectorCopy(goal, direct->goal);
	direct->revision = pathfind_geometry_revision;
	direct->until = pr_global_struct->time + 0.5f;
	direct->valid = true;
	return false;
}


void PF_ai_land(void)
{
	edict_t *ent = G_EDICT(OFS_PARM0);
	vec3_t origin, candidate, down;
	VectorCopy(G_VECTOR(OFS_PARM1), origin);
	VectorCopy(origin, G_VECTOR(OFS_RETURN));

	for (int radius = 0; radius <= 1; radius++) {
		for (int x = -radius; x <= radius; x++) {
			for (int y = -radius; y <= radius; y++) {
				if (radius && !x && !y)
					continue;

				for (int height = 0; height <= 20; height += 4) {
					VectorCopy(origin, candidate);
					candidate[0] += x * 8;
					candidate[1] += y * 8;
					candidate[2] += height + 0.03125f;

					trace_t line = SV_Move(origin, vec3_origin, vec3_origin, candidate, MOVE_NOMONSTERS, ent);

					if (line.startsolid || line.allsolid || line.fraction < 1)
						continue;

					VectorCopy(candidate, down);
					down[2] -= 64;

					trace_t ground = SV_Move(candidate, ai_hull_mins, ai_hull_maxs, down, MOVE_NOMONSTERS, ent);

					if (ground.startsolid || ground.allsolid || ground.fraction == 1 || ground.plane.normal[2] <= 0.7f)
						continue;

					VectorCopy(ground.endpos, G_VECTOR(OFS_RETURN));
					return;
				}
			}
		}
	}
}

static void PathfindTargetOrigin(edict_t *target, vec3_t origin)
{
	VectorCopy(target->v.origin, origin);

	if (((int)target->v.flags & FL_ONGROUND) || strcmp(PR_GetString(target->v.classname), "player"))
		return;

	vec3_t down;
	VectorCopy(origin, down);
	down[2] -= 256;
	trace_t ground = SV_Move(origin, target->v.mins, target->v.maxs, down, MOVE_NOMONSTERS, target);

	if (ground.startsolid || ground.allsolid || ground.fraction == 1 || ground.plane.normal[2] <= 0.7f)
		return;

	VectorCopy(ground.endpos, origin);
	origin[2] += target->v.mins[2] - ai_hull_mins[2];
}

void PF_pathfind_target_origin(void)
{
	PathfindTargetOrigin(G_EDICT(OFS_PARM0), G_VECTOR(OFS_RETURN));
}

void PF_can_walk_to_enemy(void)
{
	edict_t *ent = G_EDICT(OFS_PARM0);
	edict_t *enemy = PROG_TO_EDICT(ent->v.enemy);
	vec3_t approach, goal;
	G_FLOAT(OFS_RETURN) = 0;

	if (enemy == sv.edicts || enemy->free || !PathfindDirectAllowed(ent))
		return;

	PathfindTargetOrigin(enemy, goal);
	G_FLOAT(OFS_RETURN) = PathfindDirectToGoal(ent->v.origin, goal, ent, approach);
}


qboolean ofs_tracebox(vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int type, edict_t *ignore_ent) {
	trace_t trace;
	vec3_t start_ofs;
	vec3_t end_ofs;
	VectorCopy(start, start_ofs);
	VectorCopy(end, end_ofs);
	start_ofs[2] += 8; // Move 8qu up to work better on uneven terrain
	end_ofs[2] += 8;
	trace = SV_Move(start_ofs, mins, maxs, end_ofs, type, ignore_ent);
	return (!trace.startsolid && !trace.allsolid && trace.fraction >= 1);
}





//
// Returns the clsoest waypoint to an entity that the entity can walk to
// Sorts all waypoints by distance, returns first waypoint we can tracebox to
//
static int get_closest_waypoint(vec3_t goal, edict_t *actor) {
	const int waypoint_count = n_waypoints;

	if (waypoint_count <= 0)
		return -1;

	if (waypoint_count > MAX_WAYPOINTS)
		Sys_Error("Invalid waypoint count: %d", waypoint_count);

	if (pathfind_geometry_valid) {
		for (int i = 0; i < 8; i++) {
			if (pathfind_goals[i].valid && pathfind_goals[i].geometry_revision == pathfind_geometry_revision && pathfind_goals[i].gate_revision == pathfind_gate_revision && VectorCompare(pathfind_goals[i].origin, goal))
				return pathfind_goals[i].waypoint;
		}
	}

	qboolean connected[MAX_WAYPOINTS] = {0};

	for (int i = 0; i < waypoint_count; i++) {
		for (int link = 0; link < 8; link++) {
			int target = waypoints[i].target[link];

			if (waypoints[i].used && target >= 0 && target < waypoint_count && waypoints[target].used) {
				connected[i] = true;
				connected[target] = true;
			}
		}
	}

	// Get all waypoint indices sorted by distance to ent
	argsort_entry_t waypoint_sort_values[MAX_WAYPOINTS];

	for(int i = 0; i < waypoint_count; i++) {
		waypoint_sort_values[i].index = i;
		waypoint_sort_values[i].value = VectorDistanceSquared(waypoints[i].origin, goal);
	}

	if (waypoint_count > 1)
		qsort(waypoint_sort_values, waypoint_count, sizeof(argsort_entry_t), argsort_comparator);

	int best_waypoint_idx = -1;
	// Sweep through waypoints from closest to farthest, stop when we can tracebox to one
	for(int i = 0; i < waypoint_count; i++) {
		int waypoint_idx = waypoint_sort_values[i].index;

		if (!waypoints[waypoint_idx].used || !waypoints[waypoint_idx].open || (waypoint_count > 1 && !connected[waypoint_idx])) {
			continue;
		}

		vec3_t approach, origin;

		if (!WaypointGroundOrigin(waypoint_idx, actor, origin))
			continue;

		if (TraceMoveToGoal(origin, goal, actor, approach)) {
			best_waypoint_idx = waypoint_idx;
			break;
		}
	}

	if (pathfind_geometry_valid) {
		int slot = pathfind_goal_index++ % 8;
		pathfind_goals[slot].valid = true;
		VectorCopy(goal, pathfind_goals[slot].origin);
		pathfind_goals[slot].geometry_revision = pathfind_geometry_revision;
		pathfind_goals[slot].gate_revision = pathfind_gate_revision;
		pathfind_goals[slot].waypoint = best_waypoint_idx;
	}

	return best_waypoint_idx;
}



static qboolean PathLookaheadGround(edict_t *ent, vec3_t candidate);

static int PathFindEntries(edict_t *actor, int goal)
{
	static short heads[MAX_WAYPOINTS], sources[MAX_WAYPOINTS * 8], links[MAX_WAYPOINTS * 8];
	short queue[MAX_WAYPOINTS];
	qboolean reachable[MAX_WAYPOINTS] = {0};
	argsort_entry_t nodes[MAX_WAYPOINTS], edges[8];
	int edge_start[8], edge_end[8];
	float edge_fraction[8];
	int edge_count = 0, link_count = 0, count = 0;
	float basis_distance = -1;
	path_entry_basis_valid = false;

	if (n_waypoints <= 0 || n_waypoints > MAX_WAYPOINTS || goal < 0 || goal >= n_waypoints)
		return 0;

	for (int i = 0; i < n_waypoints; i++) {
		heads[i] = -1;
		path_entry_cost[i] = -1;
		path_entry_distance[i] = -1;
		VectorCopy(actor->v.origin, path_entry_start[i]);
		nodes[i].index = i;
		nodes[i].value = VectorDistanceSquared(actor->v.origin, waypoints[i].origin);
	}

	for (int i = 0; i < n_waypoints; i++) {
		if (!waypoints[i].used || !waypoints[i].open)
			continue;

		for (int j = 0; j < 8; j++) {
			int next = waypoints[i].target[j];

			if (next < 0)
				break;

			if (next >= n_waypoints || !waypoints[next].used || !waypoints[next].open)
				continue;

			sources[link_count] = i;
			links[link_count] = heads[next];
			heads[next] = link_count;
			link_count++;
		}
	}

	int tail = 1;
	queue[0] = goal;
	reachable[goal] = true;

	for (int head = 0; head < tail; head++) {
		for (int edge = heads[queue[head]]; edge >= 0; edge = links[edge]) {
			int node = sources[edge];

			if (!reachable[node]) {
				reachable[node] = true;
				queue[tail++] = node;
			}
		}
	}

	qsort(nodes, n_waypoints, sizeof(nodes[0]), argsort_comparator);

	int checked = 0;

	for (int i = 0; i < n_waypoints && checked < 8 && count < 4; i++) {
		int node = nodes[i].index;
		vec3_t origin;

		if (!reachable[node] || !waypoints[node].used || !waypoints[node].open)
			continue;

		checked++;

		if (WaypointGroundOrigin(node, actor, origin) && TraceMove(actor->v.origin, ai_hull_mins, ai_hull_maxs, origin, MOVE_NOMONSTERS, actor)) {
			path_entry_cost[node] = VecLength2(actor->v.origin, origin);
			VectorCopy(origin, path_entry_point[node]);
			count++;

			if (path_entry_cost[node] < 8)
				return count;
		}
	}

	for (int i = 0; i < n_waypoints; i++) {
		if (!waypoints[i].used || !waypoints[i].open)
			continue;

		for (int j = 0; j < 8; j++) {
			int next = waypoints[i].target[j];

			if (next < 0)
				break;

			if (next >= n_waypoints || !reachable[next] || !waypoints[next].used || !waypoints[next].open || fabsf(waypoints[next].origin[2] - waypoints[i].origin[2]) >= 8)
				continue;

			vec3_t delta, offset, point;
			VectorSubtract(waypoints[next].origin, waypoints[i].origin, delta);
			delta[2] = 0;

			float length_squared = DotProduct(delta, delta);

			if (length_squared < 1)
				continue;

			VectorSubtract(actor->v.origin, waypoints[i].origin, offset);

			float fraction = DotProduct(offset, delta) / length_squared;

			if (fraction <= 0.05f || fraction >= 0.95f)
				continue;

			VectorMA(waypoints[i].origin, fraction, delta, point);

			float distance = VectorDistanceSquared(actor->v.origin, point);

			if (distance > 16384)
				continue;

			int slot = edge_count < 8 ? edge_count : 7;

			while (slot > 0 && distance < edges[slot - 1].value)
				slot--;

			if (slot == 7 && edge_count == 8 && distance >= edges[7].value)
				continue;

			for (int k = edge_count < 8 ? edge_count : 7; k > slot; k--) {
				edges[k] = edges[k - 1];
				edge_start[k] = edge_start[k - 1];
				edge_end[k] = edge_end[k - 1];
				edge_fraction[k] = edge_fraction[k - 1];
			}

			edges[slot].value = distance;
			edge_start[slot] = i;
			edge_end[slot] = next;
			edge_fraction[slot] = fraction;

			if (edge_count < 8)
				edge_count++;
		}
	}

	for (int i = 0; i < edge_count; i++) {
		vec3_t start, end, point;
		int node = edge_end[i];

		if (!WaypointGroundOrigin(edge_start[i], actor, start) || !WaypointGroundOrigin(node, actor, end))
			continue;

		VectorLerp(start, edge_fraction[i], end, point);

		if (!PathLookaheadGround(actor, point) || (path_entry_cost[node] < 0 && !TraceMoveInternal(point, ai_hull_mins, ai_hull_maxs, end, MOVE_NOMONSTERS, actor, false, NULL)))
			continue;

		float entry_distance = VectorDistanceSquared(actor->v.origin, point);

		if (basis_distance < 0 || entry_distance < basis_distance) {
			basis_distance = entry_distance;
			path_entry_basis_valid = true;
			VectorCopy(point, path_entry_basis);
		}

		if (path_entry_distance[node] < 0 || entry_distance < path_entry_distance[node]) {
			path_entry_distance[node] = entry_distance;
			VectorCopy(start, path_entry_start[node]);
		}

		float cost = VecLength2(actor->v.origin, point) + VecLength2(point, end);

		if (path_entry_cost[node] < 0 || cost < path_entry_cost[node]) {
			if (path_entry_cost[node] < 0)
				count++;
			path_entry_cost[node] = cost;
			VectorCopy(point, path_entry_point[node]);
		}
	}

	return count;
}

static qboolean PathfindCanKeepRoute(edict_t *actor, edict_t *target, int goal)
{
	if (!pathfind_geometry_valid || goal < 0 || !actor->v.goalentity || actor->v.goalentity == actor->v.enemy)
		return false;

	int entnum = NUM_FOR_EDICT(actor);
	edict_t *anchor = PROG_TO_EDICT(actor->v.goalentity);

	for (int i = 0; i < MAX_AI_COUNT; i++) {
		zombie_ai *path = &zombie_list[i];

		if (path->zombienum != entnum)
			continue;

		if (!path->segment_valid || path->goal_waypoint != goal || path->targetnum != NUM_FOR_EDICT(target))
			return false;

		if (path->geometry_revision != pathfind_geometry_revision || path->gate_revision != pathfind_gate_revision)
			return false;

		if (VectorDistanceSquared(anchor->v.origin, path->segment_end) >= 1)
			return false;

		vec3_t delta, offset, point;
		VectorSubtract(path->segment_end, path->segment_start, delta);
		VectorSubtract(actor->v.origin, path->segment_start, offset);
		float length_squared = DotProduct(delta, delta);
		float fraction = length_squared > 0 ? fmaxf(0, fminf(DotProduct(offset, delta) / length_squared, 1)) : 0;
		VectorMA(path->segment_start, fraction, delta, point);
		float distance_squared = VectorDistanceSquared(actor->v.origin, point);

		if (distance_squared > 4096)
			return false;

		return distance_squared < 1 || TraceMoveInternal(actor->v.origin, ai_hull_mins, ai_hull_maxs, point, MOVE_NOMONSTERS, actor, false, NULL);
	}

	return false;
}

void Do_Pathfind (void) {

#ifdef MEASURE_PF_PERF

	u64 t1, t2;
	sceRtcGetCurrentTick(&t1);

#endif // MEASURE_PF_PERF

	int i, s;

	Con_DPrintf("====================\n");
	Con_DPrintf("Starting Do_Pathfind\n");
	Con_DPrintf("====================\n");

	int zombie_entnum = G_EDICTNUM(OFS_PARM0);
	edict_t * zombie = G_EDICT(OFS_PARM0);
	edict_t * ent = G_EDICT(OFS_PARM1);
	vec3_t approach, goal;
	PathfindTargetOrigin(ent, goal);
	waypoint_origin_actor = zombie;
	qboolean direct_allowed = PathfindDirectAllowed(zombie);

	if (direct_allowed && PathfindDirectToGoal(zombie->v.origin, goal, zombie, approach)) {
		G_FLOAT(OFS_RETURN) = -1;
		waypoint_origin_actor = NULL;
		return;
	}

	if (!direct_allowed)
		PathfindPrepareGeometry(zombie);

	if(developer.value == 3) {
		Con_Printf("Finding start waypoint\n");
	}

	int start_waypoint = -1;

	if(developer.value == 3) {
		Con_Printf("Finding goal waypoint\n");
	}

	int goal_waypoint = get_closest_waypoint(goal, zombie);

	if (pr_argc > 2 && G_FLOAT(OFS_PARM2) && PathfindCanKeepRoute(zombie, ent, goal_waypoint)) {
		G_FLOAT(OFS_RETURN) = 2;
		waypoint_origin_actor = NULL;
		return;
	}

	if (goal_waypoint == -1 || !PathFindEntries(zombie, goal_waypoint)) {
		Con_DPrintf("Pathfind failure. Invalid start or goal waypoint. (Start: %d, Goal: %d)\n", start_waypoint, goal_waypoint);
		G_FLOAT(OFS_RETURN) = 0;
		waypoint_origin_actor = NULL;
		return;
	}

	Con_DPrintf("\tStarting waypoint: %i, Ending waypoint: %i\n", start_waypoint, goal_waypoint);
	pathfind_actor = zombie;
	pathfind_entries = path_entry_cost;

	int path_found = sv_way_pathfind(-1, goal_waypoint);
	pathfind_entries = NULL;
	pathfind_actor = NULL;

	if (path_found)
		start_waypoint = process_list[process_list_length - 1];

	if (path_found) {

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
				Con_Printf("%.2f, ", (double)waypoint_dist);
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

		for(i = 0; i < MAX_AI_COUNT; i++) {
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
			zombie_list[zombie_slot].segment_valid = false;
			zombie_list[zombie_slot].goal_waypoint = goal_waypoint;
			zombie_list[zombie_slot].targetnum = NUM_FOR_EDICT(ent);
			zombie_list[zombie_slot].geometry_revision = pathfind_geometry_revision;
			zombie_list[zombie_slot].gate_revision = pathfind_gate_revision;
			VectorCopy(path_entry_point[start_waypoint], zombie_list[zombie_slot].entry_point);
			VectorCopy(path_entry_start[start_waypoint], zombie_list[zombie_slot].entry_start);

			if (path_entry_basis_valid) {
				vec3_t direction;
				VectorSubtract(waypoints[start_waypoint].origin, zombie->v.origin, direction);
				direction[2] = 0;

				if (VectorNormalize(direction) > 0)
					VectorMA(path_entry_basis, -128, direction, zombie_list[zombie_slot].entry_start);
			}

			vec3_t entry_end;
			zombie_list[zombie_slot].entry_pending = WaypointGroundOrigin(start_waypoint, zombie, entry_end) && VectorDistanceSquared(entry_end, zombie_list[zombie_slot].entry_point) >= 64;

#ifdef MEASURE_PF_PERF
			sceRtcGetCurrentTick(&t2);
			double elapsed = (t2 - t1) * 0.000001;
			Con_Printf("PF time: %f\n", elapsed);
#endif

			Con_DPrintf("\tPath found!\n");
			G_FLOAT(OFS_RETURN) = 1;

			waypoint_origin_actor = NULL;
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
	waypoint_origin_actor = NULL;
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
static void PathSetSegment(zombie_ai *path, vec3_t end, qboolean first_point)
{
	if (first_point || !path->segment_valid) {
		VectorCopy(path->entry_start, path->segment_start);
	} else {
		VectorCopy(path->segment_end, path->segment_start);
	}

	VectorCopy(end, path->segment_end);
	path->segment_valid = true;
}

static void Get_Next_Waypoint_Internal(qboolean first_point)
{
	int entnum = G_EDICTNUM(OFS_PARM0);
	edict_t *ent = G_EDICT(OFS_PARM0);
	edict_t *goal = PROG_TO_EDICT(ent->v.enemy);
	vec3_t start, goal_origin;
	zombie_ai *path = NULL;

	VectorCopy(G_VECTOR(OFS_PARM1), start);
	PathfindTargetOrigin(goal, goal_origin);
	VectorCopy(goal_origin, G_VECTOR(OFS_RETURN));

	for (int i = 0; i < MAX_AI_COUNT; i++) {
		if (zombie_list[i].zombienum == entnum) {
			path = &zombie_list[i];
			break;
		}
	}

	if (!path || path->pathlist_length < 1)
		return;

	vec3_t approach;
	qboolean direct_allowed = PathfindDirectAllowed(ent);
	qboolean at_start = false;

	if (direct_allowed && PathfindDirectToGoal(start, goal_origin, ent, approach)) {
		VectorCopy(approach, G_VECTOR(OFS_RETURN));
		path->pathlist_length = 0;
		PathSetSegment(path, G_VECTOR(OFS_RETURN), first_point);
		return;
	}

	while (path->pathlist_length > 1) {
		vec3_t delta, origin;

		if (!WaypointGroundOrigin(path->pathlist[path->pathlist_length - 1], ent, origin))
			break;

		VectorSubtract(origin, start, delta);

		if (delta[0] * delta[0] + delta[1] * delta[1] > 64 || fabsf(delta[2]) >= 20)
			break;

		path->pathlist_length--;
		at_start = true;
	}

	int adjacent = path->pathlist_length - (first_point && !at_start ? 2 : 1);

	if (adjacent < 0)
		adjacent = 0;

	if (first_point && path->entry_pending) {
		path->entry_pending = false;

		if (VectorDistanceSquared(start, path->entry_point) >= 64) {
			VectorCopy(path->entry_point, G_VECTOR(OFS_RETURN));
			PathSetSegment(path, G_VECTOR(OFS_RETURN), true);
			return;
		}
	}

	int shortcut = first_point && !at_start ? path->pathlist_length - 1 : adjacent;
	int first = 0;

	for (int i = path->pathlist_length - 1; i >= 0; i--) {
		int node = path->pathlist[i];

		if (node < 0 || node >= n_waypoints || !waypoints[node].open) {
			first = i + 1;
			break;
		}
	}

	for (int i = first; i < path->pathlist_length; i++) {
		int waypoint = path->pathlist[i];
		vec3_t origin;

		if (!WaypointGroundOrigin(waypoint, ent, origin))
			continue;

		if (i < shortcut && VectorDistanceSquared(start, origin) > 36864)
			continue;

		if (TraceMoveInternal(start, ai_hull_mins, ai_hull_maxs, origin, MOVE_NOMONSTERS, ent, i == adjacent, NULL)) {
			VectorCopy(origin, G_VECTOR(OFS_RETURN));
			path->pathlist_length = i;
			PathSetSegment(path, G_VECTOR(OFS_RETURN), first_point);
			return;
		}
	}

	if (!WaypointGroundOrigin(path->pathlist[path->pathlist_length - 1], ent, G_VECTOR(OFS_RETURN))) {
		VectorCopy(waypoints[path->pathlist[path->pathlist_length - 1]].origin, G_VECTOR(OFS_RETURN));
	}

	path->pathlist_length--;
	PathSetSegment(path, G_VECTOR(OFS_RETURN), first_point);
}


static qboolean PathLookaheadGround(edict_t *ent, vec3_t candidate)
{
	vec3_t up, down;
	VectorCopy(candidate, up);
	VectorCopy(candidate, down);
	up[2] += 8;
	down[2] -= 20;

	trace_t ground = SV_Move(up, ai_hull_mins, ai_hull_maxs, down, MOVE_NOMONSTERS, ent);

	if (ground.startsolid || ground.allsolid || ground.fraction == 1 || ground.plane.normal[2] <= 0.7f || fabsf(ground.endpos[2] - ent->v.origin[2]) >= 8)
		return false;

	VectorCopy(ground.endpos, candidate);
	return TraceMoveInternal(ent->v.origin, ai_hull_mins, ai_hull_maxs, candidate, MOVE_NOMONSTERS, ent, false, NULL);
}

void PF_pathfind_follow(void)
{
	edict_t *ent = G_EDICT(OFS_PARM0);
	vec3_t anchor, delta, points[6], candidate;
	float distance = G_FLOAT(OFS_PARM2);
	zombie_ai *path = NULL;

	VectorCopy(G_VECTOR(OFS_PARM1), anchor);
	VectorCopy(anchor, G_VECTOR(OFS_RETURN));

	if (!isfinite(distance) || distance <= 0 || ent->free)
		return;

	distance = fminf(distance, 192);
	VectorSubtract(anchor, ent->v.origin, delta);

	if (fabsf(delta[2]) >= 8)
		return;

	int entnum = NUM_FOR_EDICT(ent);

	for (int i = 0; i < MAX_AI_COUNT; i++) {
		if (zombie_list[i].zombienum == entnum) {
			path = &zombie_list[i];
			break;
		}
	}

	if (!path || !path->segment_valid)
		return;

	VectorCopy(ent->v.origin, points[0]);

	float lateral = 0;

	if (VectorDistanceSquared(anchor, path->segment_end) < 1 && fabsf(path->segment_start[2] - anchor[2]) < 8) {
		vec3_t offset;
		VectorSubtract(anchor, path->segment_start, delta);
		delta[2] = 0;

		float length_squared = DotProduct(delta, delta);

		if (length_squared > 1) {
			VectorSubtract(ent->v.origin, path->segment_start, offset);

			float fraction = fmaxf(0, fminf(DotProduct(offset, delta) / length_squared, 1));
			VectorMA(path->segment_start, fraction, delta, candidate);
			candidate[2] = anchor[2];

			if (VectorDistanceSquared(ent->v.origin, candidate) < 4096) {
				VectorCopy(candidate, points[0]);

				float length = sqrtf(length_squared);
				lateral = fmaxf(-48, fminf((offset[1] * delta[0] - offset[0] * delta[1]) / length, 48));
			}
		}
	}

	VectorCopy(anchor, points[1]);

	int count = 2;

	for (int i = path->pathlist_length - 1; i >= 0 && count < 6; i--) {
		int waypoint = path->pathlist[i];

		if (waypoint < 0 || waypoint >= n_waypoints || !waypoints[waypoint].open || !WaypointGroundOrigin(waypoint, ent, candidate) || fabsf(candidate[2] - anchor[2]) >= 8)
			break;

		VectorCopy(candidate, points[count]);
		count++;
	}

	if (count == 2) {
		float remaining = VecLength2(ent->v.origin, anchor);
		float approach = path->pathlist_length == 0 ? 256 : 96;
		lateral *= fmaxf(0, fminf((remaining - 64) / approach, 1));
	}

	int segment = 0;
	qboolean found = false;

	for (int attempt = 0; attempt < 4; attempt++, distance *= 0.5f) {
		float remaining = distance;

		for (segment = 1; segment < count; segment++) {
			VectorSubtract(points[segment], points[segment - 1], delta);

			float length = VectorNormalize(delta);

			if (remaining <= length || segment == count - 1) {
				VectorMA(points[segment - 1], fminf(remaining, length), delta, candidate);
				break;
			}

			remaining -= length;
		}

		vec3_t center, side;
		VectorCopy(candidate, center);
		VectorSubtract(points[segment], points[segment - 1], delta);
		delta[2] = 0;
		VectorNormalize(delta);
		side[0] = -delta[1];
		side[1] = delta[0];
		side[2] = 0;

		if (attempt == 0 && fabsf(lateral) >= 2) {
			for (int trial = 0; trial < 2; trial++) {
				VectorMA(center, lateral * (trial == 0 ? 1 : 0.5f), side, candidate);

				if (PathLookaheadGround(ent, candidate)) {
					found = true;
					break;
				}
			}

			if (found)
				break;
		}

		VectorCopy(center, candidate);

		if (PathLookaheadGround(ent, candidate)) {
			found = true;
			break;
		}
	}

	if (!found)
		return;

	VectorCopy(candidate, G_VECTOR(OFS_RETURN));

	edict_t *goal = PROG_TO_EDICT(ent->v.goalentity);

	if (goal == sv.edicts || goal == PROG_TO_EDICT(ent->v.enemy) || VectorDistanceSquared(goal->v.origin, anchor) >= 1 || !path->segment_valid)
		return;

	for (int i = 2; i <= segment && path->pathlist_length > 0; i++) {
		vec3_t offset;
		VectorSubtract(path->segment_end, path->segment_start, delta);
		delta[2] = 0;
		VectorSubtract(ent->v.origin, path->segment_end, offset);

		qboolean passed = DotProduct(delta, delta) >= 1 && DotProduct(offset, delta) >= -8 * VectorLength(delta) && dist_to_line_segment(path->segment_start, path->segment_end, ent->v.origin) <= 4096;
		VectorSubtract(points[i], path->segment_end, delta);
		delta[2] = 0;

		if (!passed && (DotProduct(delta, delta) < 1 || DotProduct(offset, delta) < 8 * VectorLength(delta) || dist_to_line_segment(path->segment_end, points[i], ent->v.origin) > 4096))
			break;

		PathSetSegment(path, points[i], false);
		path->pathlist_length--;
		VectorCopy(points[i], goal->v.origin);
		SV_LinkEdict(goal, false);
	}
}


void Get_Next_Waypoint(void)
{
	Get_Next_Waypoint_Internal(false);
}



/*
=================
Get_First_Waypoint This function will return the waypoint waypoint in zombies path and then remove it from the list

vector Get_First_Waypoint (entity)
=================
*/
void Get_First_Waypoint (void) {
	// TODO - Remove `Get_First_Waypoint`, replace references with `Get_Next_Waypoint`
	Get_Next_Waypoint_Internal(true);
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
	G_INT(OFS_RETURN) = OFS_NULL;

	count = Sys_FileRead(h, &buffer, 1);
	
	// protection against crashes
	if(!count)
		return;
	
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

	G_INT(OFS_RETURN) = PR_SetString(pr_string_temp);
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

char* PR_CopyToZoneIfTempString (char *s)
{
	// shpuld: if parm0 address == temp string address then we're trying to precache temp string.
	// trying to precache temp string potentially breaks future precaches, so copy contents to zone.
	if (s == pr_string_temp)
	{
		return PR_ZoneString(s);
	}
	return s;
}

void PF_precache_file (void)
{	// precache_file is only used to copy files with qcc, it does nothing
	G_INT(OFS_RETURN) = G_INT(OFS_PARM0);
}

void PF_precache_sound (void)
{
	char	*s;
	int		i;

	s = G_STRING(OFS_PARM0);
	G_INT(OFS_RETURN) = G_INT(OFS_PARM0);
	PR_CheckEmptyString (s);
	
	for (i=0 ; i<MAX_SOUNDS ; i++)
	{
		if (!sv.sound_precache[i])
		{
			s = PR_CopyToZoneIfTempString(s);
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

	s = G_STRING(OFS_PARM0);
	G_INT(OFS_RETURN) = G_INT(OFS_PARM0);
	PR_CheckEmptyString (s);
	
	for (i=0 ; i<MAX_MODELS ; i++)
	{
		if (!sv.model_precache[i])
		{
			s = PR_CopyToZoneIfTempString(s);
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

	yaw = yaw*((float)M_PI / 180);

	move[0] = cosf(yaw)*dist;
	move[1] = sinf(yaw)*dist;
	move[2] = 0;

// save program state, because SV_movestep may call other progs
	oldf = pr_xfunction;
	oldself = pr_global_struct->self;

	G_FLOAT(OFS_RETURN) = SV_movestep(ent, move, true);

	//if(!strcmp(PR_GetString(ent->v.classname,) "ai_zombie"))
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
	if (ent == sv.edicts)
	{
		G_FLOAT(OFS_RETURN) = 0;
		return;
	}

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
		G_FLOAT(OFS_RETURN) = (int)(f + 0.5f);
	else
		G_FLOAT(OFS_RETURN) = (int)(f - 0.5f);
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

	ent = G_EDICT(OFS_PARM0);
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
			+ 0.5f*(check->v.mins[j] + check->v.maxs[j]);
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

#ifdef NO_SOUND_PROCESSING
	G_FLOAT(OFS_RETURN) = 0.0;
	return;
#endif

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
	qboolean extended;

	ent = G_EDICT(OFS_PARM0);

	extended = (ent->v.scale != 0 && ent->v.scale != 1) || ent->v.effects;
	MSG_WriteByte (&sv.signon, extended ? svc_spawnstatic2 : svc_spawnstatic);

	MSG_WriteShort (&sv.signon, SV_ModelIndex(PR_GetString(ent->v.model)));

	MSG_WriteByte (&sv.signon, ent->v.frame);
	MSG_WriteByte (&sv.signon, ent->v.colormap);
	MSG_WriteByte (&sv.signon, ent->v.skin);
	for (i=0 ; i<3 ; i++)
	{
		MSG_WriteCoord(&sv.signon, ent->v.origin[i]);
		MSG_WriteAngle(&sv.signon, ent->v.angles[i]);
	}

	if (extended)
	{
		MSG_WriteByte (&sv.signon, ENTSCALE_ENCODE(ent->v.scale));
		MSG_WriteShort (&sv.signon, ent->v.effects);
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
void PF_HUDToast(void)
{
	const char *text = G_STRING(OFS_PARM0);
	if (!*text) return;
	MSG_WriteByte(&sv.reliable_datagram, svc_hudtoast);
	MSG_WriteString(&sv.reliable_datagram, (char *)text);
}

#define MAX_HUD_CONFIGS 35
static char pr_hudconfig[MAX_HUD_CONFIGS][64];
static qboolean pr_hudconfig_set[MAX_HUD_CONFIGS];

void PR_ClearHUDConfig(void)
{
	memset(pr_hudconfig_set, 0, sizeof(pr_hudconfig_set));
}

void PF_HUDConfig(void)
{
	int index = G_FLOAT(OFS_PARM0);
	const char *value = G_STRING(OFS_PARM1);
	if (index < 0 || index >= MAX_HUD_CONFIGS || strlen(value) >= sizeof(pr_hudconfig[0]))
		PR_RunError("Invalid HUD configuration");
	snprintf(pr_hudconfig[index], sizeof(pr_hudconfig[index]), "%s", value);
	pr_hudconfig_set[index] = true;
}

void PR_SendHUDConfig(client_t *client)
{
	int index;
	for (index = 0; index < MAX_HUD_CONFIGS; index++) {
		if (!pr_hudconfig_set[index]) continue;
		MSG_WriteByte(&client->message, svc_hudconfig);
		MSG_WriteByte(&client->message, index);
		MSG_WriteString(&client->message, pr_hudconfig[index]);
	}
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

void PF_SetClientMode(void)
{
	int 		state;

	state = G_FLOAT(OFS_PARM0);

	MSG_WriteByte(&sv.reliable_datagram, svc_gamemode);
	MSG_WriteByte(&sv.reliable_datagram, state);
}

void PF_SetRoundColor(void)
{
	float *color;

	color = G_VECTOR(OFS_PARM0);
	MSG_WriteByte(&sv.reliable_datagram, svc_roundcolor);
	MSG_WriteCoord(&sv.reliable_datagram, color[0]);
	MSG_WriteCoord(&sv.reliable_datagram, color[1]);
	MSG_WriteCoord(&sv.reliable_datagram, color[2]);
}

void PF_GetMonthOfYear(void)
{
	if (sv_spoofmonth.value) {
		G_FLOAT(OFS_RETURN) = sv_spoofmonth.value;
		return;
	}

	// Get month using time.h
	time_t raw_time = time(NULL);
	struct tm *time_info = localtime(&raw_time);
	int current_month = time_info->tm_mon + 1;
	G_FLOAT(OFS_RETURN) = current_month;
}

void PF_SetPerkOrientation(void)
{
	int 		orientation;

	orientation = G_FLOAT(OFS_PARM0);

	MSG_WriteByte(&sv.reliable_datagram, svc_perkorientation);
	MSG_WriteByte(&sv.reliable_datagram, orientation);
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
	int max_ai = CLAMP(1, sv_maxai.value, MAX_AI_COUNT);
	G_FLOAT(OFS_RETURN) = (float)max_ai;
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
	static char buffers[8][PR_MAX_TEMPSTRING];
	static unsigned int buffer_index;
	char *buffer = buffers[buffer_index++ % 8];

	strncpy(buffer, Cmd_Argv(G_FLOAT(OFS_PARM0)), PR_MAX_TEMPSTRING - 1);
	buffer[PR_MAX_TEMPSTRING - 1] = 0;
	G_INT(OFS_RETURN) = PR_SetString(buffer);
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

	{ 105, "cvar_free", PF_cvar_free },		// 2001-09-18 New BuiltIn Function: cvar_free() by Maddes

	{ 106, "NVS_InitSVCMsg", PF_NVS_InitSVCMsg },	// 2000-05-02 NVS SVC by Maddes

	{ 107, "WriteFloat", PF_WriteFloat },	// 2001-09-16 New BuiltIn Function: WriteFloat() by Maddes

	{ 108, "etof", PF_etof },	// 2001-09-25 New BuiltIn Function: etof() by Maddes

	{ 109, "ftoe", PF_ftoe },	// 2001-09-25 New BuiltIn Function: ftoe() by Maddes
*/

  	{ 103, "cvar_string", PF_cvar_string },

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

	{ 221, "strstrofs", PF_strstrofs },

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

  { 339, "print", PF_print },
  { 519, "gettime", PF_gettime },
  { 627, "sprintf", PF_sprintf },

  { 500, "songegg", PF_SongEgg },
  {	501, "nzp_poweruptoast", PF_HUDToast },
  { 502, "grenade_pulse", PF_GrenadePulse },
  { 503, "nzp_maxai", PF_MaxZombies },
  { 504, "nzp_bettyprompt", PF_BettyPrompt },
  { 505, "nzp_setplayername", PF_SetPlayerName },
  { 506, "nzp_setdoubletapver", PF_SetDoubleTapVersion },
  { 507, "nzp_screenflash", PF_ScreenFlash },
  { 508, "nzp_lockviewmodel", PF_LockViewmodel },
  { 509, "nzp_rumble", PF_Rumble },
  { 510, "nzp_setclientmode", PF_SetClientMode },
  { 511, "nzp_setroundcolor", PF_SetRoundColor },
  { 512, "nzp_getmonthofyear", PF_GetMonthOfYear },
  { 513, "nzp_setperkorientation", PF_SetPerkOrientation },
  { 514, "useprint_send", PF_useprint_send },
  { 515, "nzp_hudconfig", PF_HUDConfig },
  { 516, "nzp_pathfind_test", PF_pathfind_test },
  { 517, "nzp_pathfind_origin", PF_pathfind_origin },
  { 518, "nzp_pathfind_clear", PF_pathfind_clear },
	{ 520, "nzp_can_walk_to_enemy", PF_can_walk_to_enemy },
	{ 521, "nzp_pathfind_follow", PF_pathfind_follow },
	{ 522, "nzp_ai_land", PF_ai_land },
	{ 523, "nzp_pathfind_target_origin", PF_pathfind_target_origin },

  { 600, "json_parse", PF_json_parse },
  { 601, "json_parse_file", PF_json_parse_file },
  { 602, "json_free", PF_json_free },
  { 603, "json_get_value_type", PF_json_get_value_type },
  { 604, "json_get_integer", PF_json_get_integer },
  { 605, "json_get_float", PF_json_get_float },
  { 606, "json_get_string", PF_json_get_string },
  { 607, "json_find_object_child", PF_json_find_object_child },
  { 608, "json_get_length", PF_json_get_length },
  { 609, "json_get_child_at_index", PF_json_get_child_at_index },
  { 610, "json_get_name", PF_json_get_name },
  { 611, "json_get_error_code", PF_json_get_error_code },
  { 612, "json_get_error", PF_json_get_error },
  { 613, "json_get_error_position", PF_json_get_error_position },


// 2001-11-15 DarkPlaces general builtin functions by Lord Havoc  end

};

int pr_ebfs_numbuiltins = sizeof(pr_ebfs_builtins)/sizeof(pr_ebfs_builtins[0]);
// 2001-09-14 Enhanced BuiltIn Function System (EBFS) by Maddes  end
