/*

Fogging system based on FitzQuake's implementation
Now with Quakespasm bits thrown into it!
Modified for Wii :)

*/

#include "../../quakedef.h"

//==============================================================================
//
//  GLOBAL FOG
//
//==============================================================================
#define DEFAULT_GRAY 0.3

float fog_start;
float fog_end;
float fog_red;
float fog_green;
float fog_blue;

float old_start;
float old_end;
float old_red;
float old_green;
float old_blue;

float fade_time; //duration of fade
float fade_done; //time when fade will be done

extern Mtx44 perspective;
GXColor BLACK = {0, 0, 0, 255};

/*
=============
Fog_Update

update internal variables
=============
*/
void Fog_Update (float start, float end, float red, float green, float blue, float time)
{
	//save previous settings for fade
	if (time > 0)
	{
		//check for a fade in progress
		if (fade_done > cl.time)
		{
			float f;

			f = (fade_done - cl.time) / fade_time;
			old_start = f * old_start + (85.0 - f) * fog_start;
			old_end = f * old_end + (85.0 - f) * fog_end;
			old_red = f * old_red + (85.0 - f) * fog_red;
			old_green = f * old_green + (85.0 - f) * fog_green;
			old_blue = f * old_blue + (85.0 - f) * fog_blue;
		}
		else
		{
			old_start = fog_start;
			old_end = fog_end;
			old_red = fog_red;
			old_green = fog_green;
			old_blue = fog_blue;
		}
	}

	fog_start = start;
	fog_end = end;
	fog_red = red;
	fog_green = green;
	fog_blue = blue;
	fade_time = time;
	fade_done = cl.time + time;
}

/*
=============
Fog_ParseServerMessage

handle an SVC_FOG message from server
=============
*/
void Fog_ParseServerMessage (void)
{
	float start, end, red, green, blue, time;

	start = MSG_ReadByte();
	end = MSG_ReadByte();
	red = MSG_ReadByte();
	green = MSG_ReadByte();
	blue = MSG_ReadByte();
	time = MSG_ReadShort();
	
	//Con_Printf("updating fog values");

	Fog_Update (start, end, red, green, blue, time);
}

/*
=============
Fog_FogCommand_f

handle the 'fog' console command
=============
*/
void Fog_FogCommand_f (void)
{
	switch (Cmd_Argc())
	{
	default:
	case 1:
		Con_Printf("usage:\n");
		Con_Printf("   fog <fade>\n");
		Con_Printf("   fog <start> <end>\n");
		Con_Printf("   fog <red> <green> <blue>\n");
		Con_Printf("   fog <fade> <red> <green> <blue>\n");
		Con_Printf("   fog <start> <end> <red> <green> <blue>\n");
		Con_Printf("   fog <start> <end> <red> <green> <blue> <fade>\n");
		Con_Printf("current values:\n");
		Con_Printf("   \"start\" is \"%f\"\n", fog_start);
		Con_Printf("   \"end\" is \"%f\"\n", fog_end);
		Con_Printf("   \"red\" is \"%f\"\n", fog_red);
		Con_Printf("   \"green\" is \"%f\"\n", fog_green);
		Con_Printf("   \"blue\" is \"%f\"\n", fog_blue);
		Con_Printf("   \"fade\" is \"%f\"\n", fade_time);
		break;
	case 2: //TEST
		Fog_Update(fog_start,
				   fog_end,
				   fog_red,
				   fog_green,
				   fog_blue,
				   atof(Cmd_Argv(1)));
		break;
	case 3:
		Fog_Update(atof(Cmd_Argv(1)),
				   atof(Cmd_Argv(2)),
				   fog_red,
				   fog_green,
				   fog_blue,
				   0.0);
		break;
	case 4:
		Fog_Update(fog_start,
				   fog_end,
				   CLAMP(0.0, atof(Cmd_Argv(1)), 85.0),
				   CLAMP(0.0, atof(Cmd_Argv(2)), 85.0),
				   CLAMP(0.0, atof(Cmd_Argv(3)), 85.0),
				   0.0);
		break;
	case 5: //TEST
		Fog_Update(fog_start,
				   fog_end,
				   CLAMP(0.0, atof(Cmd_Argv(1)), 85.0),
				   CLAMP(0.0, atof(Cmd_Argv(2)), 85.0),
				   CLAMP(0.0, atof(Cmd_Argv(3)), 85.0),
				   atof(Cmd_Argv(4)));
		break;
	case 6:
		Fog_Update(atof(Cmd_Argv(1)),
				   atof(Cmd_Argv(2)),
				   CLAMP(0.0, atof(Cmd_Argv(3)), 85.0),
				   CLAMP(0.0, atof(Cmd_Argv(4)), 85.0),
				   CLAMP(0.0, atof(Cmd_Argv(5)), 85.0),
				   0.0);
		break;
	case 7:
		Fog_Update(atof(Cmd_Argv(1)),
				   atof(Cmd_Argv(2)),
				   CLAMP(0.0, atof(Cmd_Argv(3)), 85.0),
				   CLAMP(0.0, atof(Cmd_Argv(4)), 85.0),
				   CLAMP(0.0, atof(Cmd_Argv(5)), 85.0),
				   atof(Cmd_Argv(6)));
		break;
	}
}

/*
=============
Fog_ParseWorldspawn

called at map load
=============
*/
void Fog_ParseWorldspawn (void)
{
	char key[128], value[4096];
	char *data;

	//initially no fog
	fog_start = 0;
	old_start = 0;

	fog_end = 4000;
	old_end = 4000;

	fog_red = 0.0;
	old_red = 0.0;

	fog_green = 0.0;
	old_green = 0.0;

	fog_blue = 0.0;
	old_blue = 0.0;

	fade_time = 0.0;
	fade_done = 0.0;

	data = COM_Parse(cl.worldmodel->entities);
	if (!data)
		return; // error
	if (com_token[0] != '{')
		return; // error
	while (1)
	{
		data = COM_Parse(data);
		if (!data)
			return; // error
		if (com_token[0] == '}')
			break; // end of worldspawn
		if (com_token[0] == '_')
			strcpy(key, com_token + 1);
		else
			strcpy(key, com_token);
		while (key[strlen(key)-1] == ' ') // remove trailing spaces
			key[strlen(key)-1] = 0;
		data = COM_Parse(data);
		if (!data)
			return; // error
		strcpy(value, com_token);

		if (!strcmp("fog", key))
		{
			sscanf(value, "%f %f %f %f %f", &fog_start, &fog_end, &fog_red, &fog_green, &fog_blue);
		}
	}
}

/* Deduce the projection type (perspective vs orthogonal) and the values of the
 * near and far clipping plane from the projection matrix.
 * Note that the formulas for computing "near" and "far" are only valid for
 * matrices created by opengx or by the gu* family of GX functions. OpenGL
 * books use different formulas.
 */
static void get_projection_info(float *near, float *far)
{
    float A, B;

    A = perspective[2][2];
    B = perspective[3][2];

    if (perspective[3][3] == 0) {
        *near = B / (A - 1.0);
    } else {
        *near = (B + 1.0) / A;
    }
    *far = B / A;
}

/*
=============
Fog_EnableGFog

called before drawing stuff that should be fogged
=============
*/
GXColor color = {110, 110, 117, 255};
void Fog_EnableGFog (void)
{
	float near, far;
	float end;
	float r, g, b;
	
	if (fog_red > 85)
		fog_red = 85;
	if (fog_green > 85)
		fog_green = 85;
	if (fog_blue > 85)
		fog_blue = 85;
	
	r = fog_red*3;
	g = fog_green*3;
	b = fog_blue*3;
	
	GXColor FogColor = {r, g, b, 255}; //alpha what>?
	
	get_projection_info (&near, &far);
	
	end = fog_end/2.2;
	
	if (end < 500)
		end = 500;
		
	//Con_Printf("enabled fog: e%f r%f g%f b%f\n", end, r, g, b);
	if (r == 0 && g == 0 && b == 0 && end == 500)
		GX_SetFog(GX_FOG_NONE, 0.0F, 1.0F, 0.0F, 1.0F, color);
	else
		GX_SetFog(GX_FOG_EXP2 /*GX_FOG_LIN*/, 0.0F, end, near, far, FogColor);
}

/*
=============
Fog_DisableGFog

called after drawing stuff that should be fogged
=============
*/
void Fog_DisableGFog (void)
{
	GX_SetFog(GX_FOG_NONE, 0.0F, 1.0F, 0.0F, 1.0F, color);
}

//==============================================================================
//
//  INIT
//
//==============================================================================

/*
=============
Fog_NewMap

called whenever a map is loaded
=============
*/

void Fog_NewMap (void)
{
	Fog_ParseWorldspawn (); //for global fog
}

/*
=============
Fog_Init

called when quake initializes
=============
*/
void Fog_Init (void)
{
	Cmd_AddCommand ("fog",Fog_FogCommand_f);

	//set up global fog
	fog_start = 0;
	fog_end = 650;
	fog_red = 108;
	fog_green = 132;
	fog_blue = 147;
	fade_time = 1;
	fade_time = 1;

	//glFogi(GL_FOG_MODE, GL_LINEAR);
	//GX_SetFog(GX_FOG_LIN, 0.0F, 1.0F, 0.1F, 1.0F, BLACK);	
}