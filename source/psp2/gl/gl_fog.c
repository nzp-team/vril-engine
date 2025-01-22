/*

Fogging system based on FitzQuake's implementation
Now with Quakespasm bits thrown into it!

*/

#include "../../quakedef.h"

//==============================================================================
//
//  GLOBAL FOG
//
//==============================================================================

#define DEFAULT_DENSITY 1.0
#define DEFAULT_GRAY 0.3

float density = 1.0;
float fog_density_gl;

float 		fog_start;
float 		fog_end;
float 		fog_red;
float 		fog_green;
float 		fog_blue;

float old_density;
float old_start;
float old_end;
float old_red;
float old_green;
float old_blue;

float fade_time; //duration of fade
float fade_done; //time when fade will be done

/*
=============
Fog_Update

update internal variables
=============
*/
void Fog_Update (float start, float end, float red, float green, float blue, float time)
{

}

/*
=============
Fog_ParseServerMessage

handle an SVC_FOG message from server
=============
*/
void Fog_ParseServerMessage (void)
{

}

/*
=============
Fog_FogCommand_f

handle the 'fog' console command
=============
*/
void Fog_FogCommand_f (void)
{

}

/*
=============
Fog_ParseWorldspawn

called at map load
=============
*/
void Fog_ParseWorldspawn (void)
{

}

/*
=============
Fog_GetColor

calculates fog color for this frame, taking into account fade times
=============
*/
float *Fog_GetColor (void)
{

}

/*
=============
Fog_GetDensity

returns current density of fog

=============
*/
float Fog_GetDensity (void)
{

}

/*
=============
Fog_SetupFrame

called at the beginning of each frame
=============
*/
void Fog_SetupFrame (void)
{

}

/*
=============
Fog_SetColorForSkyS
Start called before drawing flat-colored sky
Crow_bar*
=============
*/
void Fog_SetColorForSkyS (void)
{

}

/*
=============
Fog_GetStart
returns current start of fog
=============
*/
float Fog_GetStart (void)
{

}

/*
=============
Fog_GetEnd
returns current end of fog
=============
*/
float Fog_GetEnd (void)
{

}

/*
=============
Fog_SetColorForSky
End called before drawing flat-colored sky
Crow_bar*
=============
*/
void Fog_SetColorForSkyE (void)
{

}

/*
=============
Fog_EnableGFog

called before drawing stuff that should be fogged
=============
*/
void Fog_EnableGFog (void)
{

}

/*
=============
Fog_DisableGFog

called after drawing stuff that should be fogged
=============
*/
void Fog_DisableGFog (void)
{

}

/*
=============
Fog_SetColorForSky

called before drawing flat-colored sky
=============
*/
/*
void Fog_SetColorForSky (void)
{
	float c[3];
	float f, d;

	if (fade_done > cl.time)
	{
		f = (fade_done - cl.time) / fade_time;
		d = f * old_density + (1.0 - f) * fog_density;
		c[0] = f * old_red + (1.0 - f) * fog_red;
		c[1] = f * old_green + (1.0 - f) * fog_green;
		c[2] = f * old_blue + (1.0 - f) * fog_blue;
	}
	else
	{
		d = fog_density;
		c[0] = fog_red;
		c[1] = fog_green;
		c[2] = fog_blue;
	}

	if (d > 0)
		glColor3fv (c);
}
*/
//==============================================================================
//
//  VOLUMETRIC FOG
//
//==============================================================================
/*

void Fog_DrawVFog (void){}
void Fog_MarkModels (void){}
*/
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
}