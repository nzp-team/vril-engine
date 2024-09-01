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
// snd_dma.c -- main control for any streaming sound output device

#include "quakedef.h"



void S_Play_f(void);
void S_PlayVol_f(void);
void S_SoundList_f(void);
void S_Update_();
void S_StopAllSounds(qboolean clear);
void S_StopAllSoundsC_f(void);
void S_VolumeDown_f (void); // Baker 3.60 - from JoeQuake 0.15
void S_VolumeUp_f (void);  // Baker 3.60 - from JoeQuake 0.15

// =======================================================================
// Internal sound data & structures
// =======================================================================

channel_t   channels[MAX_CHANNELS];
int			total_channels;

int				snd_blocked = 0;
static qboolean	snd_ambient = 1;
qboolean		snd_initialized = false;

// pointer should go away
volatile dma_t  *shm = 0;
volatile dma_t sn;

vec3_t		listener_origin;
vec3_t		listener_forward;
vec3_t		listener_right;
vec3_t		listener_up;
vec_t		sound_nominal_clip_dist=1500.0; // JPG - changed this from 1000 to 15000 (I'm 99% sure that's what it was in 1.06)

int			soundtime;		// sample PAIRS
int   		paintedtime; 	// sample PAIRS


sfx_t		*known_sfx;		// hunk allocated [MAX_SFX]
int			num_sfx;

sfx_t		*ambient_sfx[NUM_AMBIENTS];

int 		desired_speed = 11025;
int 		desired_bits = 16;

int sound_started=0;

cvar_t bgmvolume = {"bgmvolume", "1", true};
cvar_t bgmtype = {"bgmtype", "cd", true};   // cd or none
cvar_t volume = {"volume", "0.7", true};

cvar_t nosound = {"nosound", "0"};
cvar_t precache = {"precache", "1"};
cvar_t loadas8bit = {"loadas8bit", "0"};
cvar_t bgmbuffer = {"bgmbuffer", "4096"};
cvar_t ambient_level = {"ambient_level", "0.3", true}; // Baker 3.60 - Save to config
cvar_t ambient_fade = {"ambient_fade", "100"};
cvar_t snd_noextraupdate = {"snd_noextraupdate", "0"};
cvar_t snd_show = {"snd_show", "0"};
cvar_t _snd_mixahead = {"_snd_mixahead", "0.1", true};

// ====================================================================
// User-setable variables
// ====================================================================

// Fake dma is a synchronous faking of the DMA progress used for
// isolating performance in the renderer.  The fakedma_updates is
// number of times S_Update() is called per second.

qboolean fakedma = false;
int fakedma_updates = 15;


void S_AmbientOff (void)
{
	snd_ambient = false;
}


void S_AmbientOn (void)
{
	snd_ambient = true;
}


void S_SoundInfo_f(void)
{
	if (!sound_started || !shm)
	{
		Con_Printf ("sound system not started\n");
		return;
	}

    Con_Printf("%5d stereo\n", shm->channels - 1);
    Con_Printf("%5d samples\n", shm->samples);
    Con_Printf("%5d samplepos\n", shm->samplepos);
    Con_Printf("%5d samplebits\n", shm->samplebits);
    Con_Printf("%5d submission_chunk\n", shm->submission_chunk);
    Con_Printf("%5d speed\n", shm->speed);
    Con_Printf("0x%x dma buffer\n", shm->buffer);
	Con_Printf("%5d total_channels\n", total_channels);
}


/*
================
S_Startup
================
*/

void S_Startup (void)
{
	int		rc;

	if (!snd_initialized)
		return;

	if (!fakedma)
	{
		if (!(rc = SNDDMA_Init()))
		{
			Con_Printf("S_Startup: SNDDMA_Init failed.\n");
			sound_started = 0;
			return;
		}
	}

	sound_started = 1;
}


/*
================
S_Init
================
*/
void CDAudioSetVolume (void);
void S_Init (void)
{
	if (COM_CheckParm("-nosound"))
		return;

	Con_Printf("\nSound Initialization\n");

	if (COM_CheckParm("-simsound"))
		fakedma = true;

	Cmd_AddCommand("play", S_Play_f);
	Cmd_AddCommand("playvol", S_PlayVol_f);
	Cmd_AddCommand("stopsound", S_StopAllSoundsC_f);
	Cmd_AddCommand("soundlist", S_SoundList_f);
	Cmd_AddCommand("soundinfo", S_SoundInfo_f);
	Cmd_AddCommand ("volumedown", S_VolumeDown_f); // Baker 3.60 - from JoeQuake 0.15
	Cmd_AddCommand ("volumeup", S_VolumeUp_f); // Baker 3.60 - from JoeQuake 0.15

	Cvar_RegisterVariable(&nosound);
	Cvar_RegisterVariable(&volume);
	Cvar_RegisterVariable(&precache);
	Cvar_RegisterVariable(&loadas8bit);
	Cvar_RegisterVariable(&bgmvolume);
	Cvar_RegisterVariable(&bgmbuffer);
	Cvar_RegisterVariable(&ambient_level);
	Cvar_RegisterVariable(&ambient_fade);
	Cvar_RegisterVariable(&snd_noextraupdate);
	Cvar_RegisterVariable(&snd_show);
	Cvar_RegisterVariable(&_snd_mixahead);

	//if (host_parms.memsize < 0x800000)
	{
		Cvar_Set ("loadas8bit", "1");
		Con_Printf ("loading all sounds as 8bit\n");
	}


	snd_initialized = true;

	S_Startup ();

	SND_InitScaletable ();

	known_sfx = Hunk_AllocName (MAX_SFX*sizeof(sfx_t), "sfx_t");
	num_sfx = 0;

// create a piece of DMA memory

	if (fakedma)
	{
		shm = (void *) Hunk_AllocName(sizeof(*shm), "shm");
		shm->splitbuffer = 0;
		shm->samplebits = 16;
		shm->speed = 22050;
		shm->channels = 2;
		shm->samples = 32768;
		shm->samplepos = 0;
		shm->soundalive = true;
		shm->gamealive = true;
		shm->submission_chunk = 1;
		shm->buffer = Hunk_AllocName(1<<16, "shmbuf");
	}

	Con_Printf ("Sound sampling rate: %i Hz\n", shm->speed);

	// provides a tick sound until washed clean

//	if (shm->buffer)
//		shm->buffer[4] = shm->buffer[5] = 0x7f;	// force a pop for debugging

	S_StopAllSounds (true);
}


// =======================================================================
// Shutdown sound engine
// =======================================================================

void S_Shutdown(void)
{
	if (!sound_started)
		return;

	if (shm)
		shm->gamealive = 0;

	shm = 0;
	sound_started = 0;

	if (!fakedma)
		SNDDMA_Shutdown();
}


// =======================================================================
// Load a sound
// =======================================================================

/*
==================
S_FindName

==================
*/
sfx_t *S_FindName (char *name)
{
	int		i;
	sfx_t	*sfx;

	if (!name)
		Sys_Error ("S_FindName: NULL\n");

	if (strlen(name) >= MAX_QPATH)
		Sys_Error ("Sound name too long: %s", name);

// see if already loaded
	for (i=0 ; i < num_sfx ; i++)
		if (!strcmp(known_sfx[i].name, name))
			return &known_sfx[i];

	if (num_sfx == MAX_SFX)
		Sys_Error ("S_FindName: out of sfx_t");

	sfx = &known_sfx[i];
	strcpy (sfx->name, name);

	num_sfx++;

	return sfx;
}


/*
==================
S_TouchSound

==================
*/
void S_TouchSound (char *name)
{
	sfx_t	*sfx;

	if (!sound_started)
		return;

	sfx = S_FindName (name);
	Cache_Check (&sfx->cache);
}

/*
==================
S_PrecacheSound

==================
*/
sfx_t *S_PrecacheSound (char *name)
{
	sfx_t	*sfx;

	if (!sound_started || nosound.value)
		return NULL;

	sfx = S_FindName (name);

// cache it in
	if (precache.value)
		S_LoadSound (sfx);

	return sfx;
}


//=============================================================================

/*
=================
SND_PickChannel
=================
*/
channel_t *SND_PickChannel(int entnum, int entchannel)
{
    int ch_idx;
    int first_to_die;
    int life_left;

// Check for replacement sound, or find the best one to replace
    first_to_die = -1;
    life_left = 0x7fffffff;
    for (ch_idx=NUM_AMBIENTS ; ch_idx < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS ; ch_idx++)
    {
		if (entchannel != 0		// channel 0 never overrides
		&& channels[ch_idx].entnum == entnum
		&& (channels[ch_idx].entchannel == entchannel || entchannel == -1) )
		{	// always override sound from same entity
			first_to_die = ch_idx;
			break;
		}

		// don't let monster sounds override player sounds
		if (channels[ch_idx].entnum == cl.viewentity && entnum != cl.viewentity && channels[ch_idx].sfx)
			continue;

		if (channels[ch_idx].end - paintedtime < life_left)
		{
			life_left = channels[ch_idx].end - paintedtime;
			first_to_die = ch_idx;
		}
   }

	if (first_to_die == -1)
		return NULL;

	if (channels[first_to_die].sfx)
		channels[first_to_die].sfx = NULL;

    return &channels[first_to_die];
}

/*
=================
SND_Spatialize
=================
*/
//
// cypress -- sound dot product square root LUTs
//
float SND_InverseSpatializationRScaleLUT[126] = { 
	0.0f, 0.12247448713915896f, 0.1732050807568878f, 0.21213203435596434f, 
	0.2449489742783179f, 0.2738612787525832f, 0.30000000000000016f, 0.32403703492039315f, 
	0.3464101615137756f, 0.3674234614174769f, 0.38729833462074187f, 0.4062019202317982f, 
	0.4242640687119287f, 0.44158804331639256f, 0.4582575694955842f, 0.4743416490252571f, 
	0.4898979485566358f, 0.5049752469181041f, 0.5196152422706635f, 0.5338539126015658f, 
	0.5477225575051664f, 0.5612486080160914f, 0.5744562646538032f, 0.5873670062235368f, 
	0.6000000000000003f, 0.6123724356957948f, 0.62449979983984f, 0.6363961030678931f, 
	0.6480740698407863f, 0.6595452979136462f, 0.6708203932499373f, 0.6819090848492931f, 
	0.6928203230275513f, 0.7035623639735148f, 0.7141428428542853f, 0.7245688373094723f, 
	0.7348469228349538f, 0.7449832212875673f, 0.7549834435270754f, 0.764852927038918f, 
	0.7745966692414837f, 0.7842193570679065f, 0.7937253933193775f, 0.8031189202104508f, 
	0.8124038404635964f, 0.8215838362577496f, 0.8306623862918079f, 0.8396427811873336f, 
	0.8485281374238574f, 0.8573214099741128f, 0.866025403784439f, 0.8746427842267954f, 
	0.8831760866327851f, 0.8916277250063508f, 0.9000000000000004f, 0.9082951062292479f, 
	0.9165151389911684f, 0.9246621004453469f, 0.9327379053088819f, 0.9407443861113394f, 
	0.9486832980505142f, 0.9565563234854499f, 0.9643650760992959f, 0.9721111047611795f, 
	0.9797958971132716f, 0.9874208829065754f, 0.9949874371066203f, 1.0024968827881715f, 
	1.009950493836208f, 1.0173494974687907f, 1.0246950765959602f, 1.0319883720275151f, 
	1.0392304845413267f, 1.0464224768228179f, 1.0535653752852743f, 1.0606601717798216f, 
	1.0677078252031316f, 1.0747092630102342f, 1.081665382639197f, 1.0885770528538625f, 
	1.0954451150103326f, 1.1022703842524304f, 1.109053650640942f, 1.1157956802210702f, 
	1.1224972160321829f, 1.1291589790636218f, 1.135781669160055f, 1.1423659658795866f, 
	1.1489125293076061f, 1.1554220008291347f, 1.1618950038622256f, 1.1683321445547927f, 
	1.1747340124470735f, 1.181101181101772f, 1.1874342087037921f, 1.1937336386313326f, 
	1.2000000000000004f, 1.2062338081814823f, 1.2124355652982146f, 1.2186057606953946f, 
	1.2247448713915896f, 1.2308533625091176f, 1.2369316876852987f, 1.242980289465606f, 
	1.24899959967968f, 1.2549900398011138f, 1.2609520212918497f, 1.2668859459319932f, 
	1.272792206135786f, 1.2786711852544426f, 1.2845232578665133f, 1.2903487900563946f, 
	1.2961481396815726f, 1.301921656629154f, 1.3076696830622025f, 1.3133925536563702f, 
	1.3190905958272925f, 1.3247641299491775f, 1.3304134695650076f, 1.3360389215887394f, 
	1.3416407864998743f, 1.3472193585307484f, 1.3527749258468689f, 1.358307770720613f, 
	1.363818169698586f, 1.3693063937629157f
};

float SND_InverseSpatializationLScaleLUT[126] = { 
	1.4142135623730951f, 1.408900280360537f, 1.40356688476182f, 1.3982131454109563f, 
	1.3928388277184118f, 1.3874436925511606f, 1.3820274961085253f, 1.3765899897936205f, 
	1.3711309200802089f, 1.3656500283747661f, 1.3601470508735443f, 1.3546217184144067f, 
	1.349073756323204f, 1.34350288425444f, 1.3379088160259651f, 1.3322912594474228f, 
	1.3266499161421599f, 1.3209844813622904f, 1.3152946437965904f, 1.3095800853708794f, 
	1.3038404810405297f, 1.2980754985747167f, 1.2922847983320085f, 1.2864680330268607f, 
	1.2806248474865696f, 1.2747548783981961f, 1.268857754044952f, 1.2629330940315087f, 
	1.2569805089976533f, 1.2509996003196802f, 1.2449899597988732f, 1.2389511693363866f, 
	1.232882800593795f, 1.2267844146385294f, 1.22065556157337f, 1.2144957801491119f, 
	1.208304597359457f, 1.2020815280171306f, 1.1958260743101397f, 1.1895377253370318f, 
	1.183215956619923f, 1.1768602295939816f, 1.1704699910719623f, 1.1640446726822813f, 
	1.1575836902790222f, 1.1510864433221335f, 1.1445523142259595f, 1.137980667674104f, 
	1.1313708498984758f, 1.124722187920199f, 1.1180339887498945f, 1.111305538544643f, 
	1.1045361017187258f, 1.097724920005007f, 1.090871211463571f, 1.0839741694339398f, 
	1.0770329614269005f, 1.0700467279516344f, 1.0630145812734646f, 1.0559356040971435f, 
	1.0488088481701512f, 1.0416333327999825f, 1.0344080432788596f, 1.0271319292087064f, 
	1.0198039027185566f, 1.012422836565829f, 1.0049875621120887f, 0.9974968671629998f, 
	0.9899494936611661f, 0.9823441352194247f, 0.974679434480896f, 0.9669539802906855f, 
	0.9591663046625435f, 0.951314879522022f, 0.94339811320566f, 0.9354143466934849f, 
	0.9273618495495699f, 0.9192388155425113f, 0.9110433579144295f, 0.902773504263389f, 
	0.8944271909999154f, 0.886002257333467f, 0.8774964387392117f, 0.8689073598491378f, 
	0.8602325267042622f, 0.8514693182963196f, 0.8426149773176353f, 0.8336666000266528f, 
	0.8246211251235316f, 0.8154753215150039f, 0.8062257748298544f, 0.7968688725254608f, 
	0.7874007874011805f, 0.7778174593052016f, 0.7681145747868602f, 0.7582875444051543f, 
	0.7483314773547876f, 0.7382411530116693f, 0.728010988928051f, 0.7176350047203655f, 
	0.7071067811865467f, 0.6964194138592051f, 0.6855654600401035f, 0.6745368781616012f, 
	0.663324958071079f, 0.651920240520264f, 0.6403124237432839f, 0.6284902544988258f, 
	0.6164414002968966f, 0.6041522986797276f, 0.5916079783099606f, 0.5787918451395102f, 
	0.5656854249492369f, 0.5522680508593619f, 0.5385164807134492f, 0.5244044240850745f, 
	0.5099019513592772f, 0.4949747468305819f, 0.4795831523312705f, 0.46368092477478373f, 
	0.4472135954999564f, 0.4301162633521297f, 0.41231056256176435f, 0.39370039370058874f, 
	0.3741657386773922f, 0.35355339059327173f
};

void SND_Spatialize(channel_t *ch)
{
    vec_t dot;
    vec_t dist;
    vec_t lscale, rscale, scale;
    vec3_t source_vec;
	sfx_t *snd;

	// anything coming from the view entity will always be full volume
	// cypress -- added full volume for no attenuation.
	if (ch->entnum == cl.viewentity || ch->dist_mult == 0)
	{
		ch->leftvol = ch->master_vol;
		ch->rightvol = ch->master_vol;
		return;
	}

// calculate stereo seperation and distance attenuation

	snd = ch->sfx;
	VectorSubtract(ch->origin, listener_origin, source_vec);

	dist = VectorNormalize(source_vec) * ch->dist_mult;

	dot = DotProduct(listener_right, source_vec);

	int dot_index = (dot + 1) * 63;
	rscale = SND_InverseSpatializationRScaleLUT[dot_index];
	lscale = SND_InverseSpatializationLScaleLUT[dot_index];

// add in distance effect
	scale = (1.0 - dist) * rscale;
	ch->rightvol = (int) (ch->master_vol * scale);
	if (ch->rightvol < 0)
		ch->rightvol = 0;

	scale = (1.0 - dist) * lscale;
	ch->leftvol = (int) (ch->master_vol * scale);
	if (ch->leftvol < 0)
		ch->leftvol = 0;
}


// =======================================================================
// Start a sound effect
// =======================================================================

void S_StartSound(int entnum, int entchannel, sfx_t *sfx, vec3_t origin, float fvol, float attenuation)
{
	channel_t *target_chan, *check;
	sfxcache_t	*sc;
	int		vol;
	int		ch_idx;
	int		skip;

	if (!sound_started)
		return;

	if (!sfx)
		return;

	if (nosound.value)
		return;

	vol = fvol*255;

// pick a channel to play on
	target_chan = SND_PickChannel(entnum, entchannel);
	if (!target_chan)
		return;

// spatialize
	memset (target_chan, 0, sizeof(*target_chan));
	VectorCopy(origin, target_chan->origin);
	target_chan->dist_mult = attenuation / sound_nominal_clip_dist;
	target_chan->master_vol = vol;
	target_chan->entnum = entnum;
	target_chan->entchannel = entchannel;
	SND_Spatialize(target_chan);

	if (!target_chan->leftvol && !target_chan->rightvol)
		return;		// not audible at all

// new channel
	if (!(sc = S_LoadSound (sfx)))
	{
		target_chan->sfx = NULL;
		return;		// couldn't load the sound's data
	}

	target_chan->sfx = sfx;
	target_chan->pos = 0.0;
    target_chan->end = paintedtime + sc->length;

// if an identical sound has also been started this frame, offset the pos
// a bit to keep it from just making the first one louder
	check = &channels[NUM_AMBIENTS];
    for (ch_idx=NUM_AMBIENTS ; ch_idx < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS ; ch_idx++, check++)
    {
		if (check == target_chan)
			continue;
		if (check->sfx == sfx && !check->pos)
		{
			skip = rand () % (int)(0.1*shm->speed);
			if (skip >= target_chan->end)
				skip = target_chan->end - 1;
			target_chan->pos += skip;
			target_chan->end -= skip;
			break;
		}
	}
}

void S_StopSound(int entnum, int entchannel)
{
	int i;

	for (i=0 ; i<MAX_DYNAMIC_CHANNELS ; i++)
	{
		if (channels[i].entnum == entnum && channels[i].entchannel == entchannel)
		{
			channels[i].end = 0;
			channels[i].sfx = NULL;
			return;
		}
	}
}

void S_StopAllSounds(qboolean clear)
{
	int		i;

	if (!sound_started)
		return;

	total_channels = MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS;	// no statics

	for (i=0 ; i<MAX_CHANNELS ; i++)
		if (channels[i].sfx)
			channels[i].sfx = NULL;

	memset(channels, 0, MAX_CHANNELS * sizeof(channel_t));

	if (clear)
		S_ClearBuffer ();
}

void S_StopAllSoundsC_f (void)
{
	S_StopAllSounds (true);
}

void S_ClearBuffer (void)
{
	int		clear;

	if (!sound_started || !shm || !shm->buffer)
		return;

	if (shm->samplebits == 8)
		clear = 0x80;
	else
		clear = 0;

	{
		memset(shm->buffer, clear, shm->samples * shm->samplebits/8);
	}
}


/*
=================
S_StaticSound
=================
*/
void S_StaticSound (sfx_t *sfx, vec3_t origin, float vol, float attenuation)
{
	channel_t	*ss;
	sfxcache_t		*sc;

	if (!sfx)
		return;

	if (total_channels == MAX_CHANNELS)
	{
		Con_Printf ("total_channels == MAX_CHANNELS\n");
		return;
	}

	ss = &channels[total_channels];
	total_channels++;

	if (!(sc = S_LoadSound (sfx)))
		return;

	if (sc->loopstart == -1)
	{
		Con_Printf ("Sound %s not looped\n", sfx->name);
		return;
	}

	ss->sfx = sfx;
	VectorCopy (origin, ss->origin);
	ss->master_vol = vol;
	ss->dist_mult = (attenuation/64) / sound_nominal_clip_dist;
    ss->end = paintedtime + sc->length;

	SND_Spatialize (ss);
}


//=============================================================================

/*
===================
S_UpdateAmbientSounds
===================
*/
void S_UpdateAmbientSounds (void)
{
	mleaf_t		*l;
	float		vol;
	int			ambient_channel;
	channel_t	*chan;

	if (!snd_ambient)
		return;

// calc ambient sound levels
	if (!cl.worldmodel)
		return;

	l = Mod_PointInLeaf (listener_origin, cl.worldmodel);
	if (!l || !ambient_level.value)
	{
		for (ambient_channel = 0 ; ambient_channel< NUM_AMBIENTS ; ambient_channel++)
			channels[ambient_channel].sfx = NULL;
		return;
	}

	for (ambient_channel = 0 ; ambient_channel< NUM_AMBIENTS ; ambient_channel++)
	{
		chan = &channels[ambient_channel];
		chan->sfx = ambient_sfx[ambient_channel];

		vol = ambient_level.value * l->ambient_sound_level[ambient_channel];
		if (vol < 8)
			vol = 0;

	// don't adjust volume too fast
		if (chan->master_vol < vol)
		{
			chan->master_vol += host_frametime * ambient_fade.value;
			if (chan->master_vol > vol)
				chan->master_vol = vol;
		}
		else if (chan->master_vol > vol)
		{
			chan->master_vol -= host_frametime * ambient_fade.value;
			if (chan->master_vol < vol)
				chan->master_vol = vol;
		}

		chan->leftvol = chan->rightvol = chan->master_vol;
	}
}


/*
============
S_Update

Called once each time through the main loop
============
*/
void S_Update(vec3_t origin, vec3_t forward, vec3_t right, vec3_t up)
{
	int			i, j;
	int			total;
	channel_t	*ch;
	channel_t	*combine;

	if (!sound_started || (snd_blocked > 0))
		return;

	VectorCopy(origin, listener_origin);
	VectorCopy(forward, listener_forward);
	VectorCopy(right, listener_right);
	VectorCopy(up, listener_up);

// update general area ambient sound sources
	S_UpdateAmbientSounds ();

	combine = NULL;

// update spatialization for static and dynamic sounds
	ch = channels+NUM_AMBIENTS;
	for (i=NUM_AMBIENTS ; i<total_channels; i++, ch++)
	{
		if (!ch->sfx)
			continue;
		SND_Spatialize(ch);         // respatialize channel
		if (!ch->leftvol && !ch->rightvol)
			continue;

	// try to combine static sounds with a previous channel of the same
	// sound effect so we don't mix five torches every frame

		if (i >= MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS)
		{
		// see if it can just use the last one
			if (combine && combine->sfx == ch->sfx)
			{
				combine->leftvol += ch->leftvol;
				combine->rightvol += ch->rightvol;
				ch->leftvol = ch->rightvol = 0;
				continue;
			}
		// search for one
			combine = channels+MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS;
			for (j=MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS ; j<i; j++, combine++)
				if (combine->sfx == ch->sfx)
					break;

			if (j == total_channels)
			{
				combine = NULL;
			}
			else
			{
				if (combine != ch)
				{
					combine->leftvol += ch->leftvol;
					combine->rightvol += ch->rightvol;
					ch->leftvol = ch->rightvol = 0;
				}
				continue;
			}
		}
	}

// debugging output
	if (snd_show.value)
	{
		total = 0;
		ch = channels;
		for (i=0 ; i<total_channels; i++, ch++)
			if (ch->sfx && (ch->leftvol || ch->rightvol) )
			{
				//Con_Printf ("%3i %3i %s\n", ch->leftvol, ch->rightvol, ch->sfx->name);
				total++;
			}

		Con_Printf ("----(%i)----\n", total);
	}

// mix some sound
	S_Update_();
}

void GetSoundtime(void)
{
	int		samplepos;
	static	int		buffers;
	static	int		oldsamplepos;
	int		fullsamples;


	fullsamples = shm->samples / shm->channels;

// it is possible to miscount buffers if it has wrapped twice between
// calls to S_Update.  Oh well.
	samplepos = SNDDMA_GetDMAPos();

	if (samplepos < oldsamplepos)
	{
		buffers++;					// buffer wrapped

		if (paintedtime > 0x40000000)
		{	// time to chop things off to avoid 32 bit limits
			buffers = 0;
			paintedtime = fullsamples;
			S_StopAllSounds (true);
		}
	}
	oldsamplepos = samplepos;

	soundtime = buffers*fullsamples + samplepos/shm->channels;
}

void S_ExtraUpdate (void)
{
	if (snd_noextraupdate.value)
		return;		// don't pollute timings

	S_Update_();
}

void S_Update_(void)
{
	unsigned        endtime;
	int				samps;

	if (!sound_started || (snd_blocked > 0))
		return;

// Updates DMA time
	GetSoundtime();

// check to make sure that we haven't overshot
	if (paintedtime < soundtime)
	{
		//Con_Printf ("S_Update_ : overflow\n");
		paintedtime = soundtime;
	}

	//OutputDebugString(va("paintedtime: %i, soundtime: %i\n", paintedtime, soundtime));

// mix ahead of current position
	endtime = soundtime + _snd_mixahead.value * shm->speed;
	samps = shm->samples >> (shm->channels-1);
	if (endtime - soundtime > samps)
		endtime = soundtime + samps;

	S_PaintChannels (endtime);

	SNDDMA_Submit ();
}

/*
===============================================================================

console functions

===============================================================================
*/

void S_Play_f(void)
{
	static int hash=345;
	int 	i;
	char name[256];
	sfx_t	*sfx;

	i = 1;
	while (i<Cmd_Argc())
	{
		if (!strrchr(Cmd_Argv(i), '.'))
		{
			strcpy(name, Cmd_Argv(i));
			strlcat (name, ".wav", sizeof(name));
		}
		else
			strcpy(name, Cmd_Argv(i));
		sfx = S_PrecacheSound(name);
		S_StartSound(hash++, 0, sfx, listener_origin, 1.0, 1.0);
		i++;
	}
}

void S_PlayVol_f(void)
{
	static int hash=543;
	int i;
	float vol;
	char name[256];
	sfx_t	*sfx;

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("Usage: playvol <filename>\n");
		return;
	}

	i = 1;
	while (i<Cmd_Argc())
	{
		if (!strrchr(Cmd_Argv(i), '.'))
		{
			strcpy(name, Cmd_Argv(i));
			strlcat (name, ".wav", sizeof(name));
		}
		else
			strcpy(name, Cmd_Argv(i));
		sfx = S_PrecacheSound(name);
		vol = atof(Cmd_Argv(i+1));
		S_StartSound(hash++, 0, sfx, listener_origin, vol, 1.0);
		i+=2;
	}
}

void S_SoundList_f(void)
{
	int		i;
	sfx_t	*sfx;
	sfxcache_t	*sc;
	int		size, total;

	total = 0;
	for (sfx=known_sfx, i=0 ; i<num_sfx ; i++, sfx++)
	{
		if (!(sc = Cache_Check (&sfx->cache)))
			continue;
		size = sc->length*sc->width*(sc->stereo+1);
		total += size;
		if (sc->loopstart >= 0)
			Con_Printf ("L");
		else
			Con_Printf (" ");
		Con_Printf("(%2db) %6i : %s\n",sc->width*8,  size, sfx->name);
	}
	Con_Printf ("Total resident: %i\n", total);
}

qboolean	volume_changed;

void S_VolumeDown_f (void)
{
	//S_LocalSound ("misc/menu3.wav");
	volume.value -= 0.1;
	volume.value = bound(0, volume.value, 1);
	//Cvar_SetValueByRef (&volume, volume.value);
	volume_changed = true;
}

void S_VolumeUp_f (void)
{
	//S_LocalSound ("misc/menu3.wav");
	volume.value += 0.1;
	volume.value = bound(0, volume.value, 1);
	//Cvar_SetValueByRef (&volume, volume.value);
	volume_changed = true;
}

void S_LocalSound (char *sound)
{
	sfx_t	*sfx;

	if (nosound.value)
		return;
	if (!sound_started)
		return;

	if (!(sfx = S_PrecacheSound (sound)))
	{
		Con_Printf ("S_LocalSound: can't cache %s\n", sound);
		return;
	}
	S_StartSound (cl.viewentity, -1, sfx, vec3_origin, 1, 1);
}


void S_ClearPrecache (void)
{
}


void S_BeginPrecaching (void)
{
}


void S_EndPrecaching (void)
{
}

