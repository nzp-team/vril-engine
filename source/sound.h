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
// sound.h -- client sound i/o functions

#ifndef __SOUND__
#define __SOUND__

#define DEFAULT_SOUND_PACKET_VOLUME 255
#define DEFAULT_SOUND_PACKET_ATTENUATION 1.0

// !!! if this is changed, it much be changed in asm_i386.h too !!!
typedef struct
{
	int left;
	int right;
} portable_samplepair_t;

typedef struct sfx_s
{
	char 	name[MAX_QPATH];
	cache_user_t	cache;
} sfx_t;

// !!! if this is changed, it much be changed in asm_i386.h too !!!
typedef struct
{
	int 	length;
	int 	loopstart;
	int 	speed;
	int 	width;
	int 	stereo;
	byte	data[1];		// variable sized
} sfxcache_t;

typedef struct
{
	qboolean		gamealive;
	qboolean		soundalive;
	qboolean		splitbuffer;
	int				channels;
	int				samples;				// mono samples in buffer
	int				submission_chunk;		// don't mix less than this #
	int				samplepos;				// in mono samples
	int				samplebits;
	int				speed;
	unsigned char	*buffer;
} dma_t;

// !!! if this is changed, it much be changed in asm_i386.h too !!!
typedef struct
{
	sfx_t	*sfx;			// sfx number
	int		leftvol;		// 0-255 volume
	int		rightvol;		// 0-255 volume
	int		end;			// end time in global paintsamples
	int 	pos;			// sample position in sfx
	int		looping;		// where to loop, -1 = no looping
	int		entnum;			// to allow overriding a specific sound
	int		entchannel;		//
	vec3_t	origin;			// origin of sound effect
	vec_t	dist_mult;		// distance multiplier (attenuation/clipK)
	int		master_vol;		// 0-255 master volume
} channel_t;

typedef struct
{
	int		rate;
	int		width;
	int		channels;
	int		loopstart;
	int		samples;
	int		dataofs;		// chunk starts this many bytes from file start
} wavinfo_t;

void S_Init (void);
void S_Startup (void);
void S_Shutdown (void);
void S_StartSound (int entnum, int entchannel, sfx_t *sfx, vec3_t origin, float fvol,  float attenuation);
void S_StaticSound (sfx_t *sfx, vec3_t origin, float vol, float attenuation);
void S_StopSound (int entnum, int entchannel);
void S_StopAllSounds(qboolean clear);
void S_ClearBuffer (void);
void S_Update (vec3_t origin, vec3_t v_forward, vec3_t v_right, vec3_t v_up);
void S_ExtraUpdate (void);

sfx_t *S_PrecacheSound (char *sample);
void S_TouchSound (char *sample);
void S_ClearPrecache (void);
void S_BeginPrecaching (void);
void S_EndPrecaching (void);
void S_PaintChannels(int endtime);
void S_InitPaintChannels (void);

// picks a channel based on priorities, empty slots, number of channels
channel_t *SND_PickChannel(int entnum, int entchannel);

// spatializes a channel
void SND_Spatialize(channel_t *ch);

// initializes cycling through a DMA buffer and returns information on it
qboolean SNDDMA_Init(void);

// gets the current DMA position
int SNDDMA_GetDMAPos(void);

// shutdown the DMA xfer.
void SNDDMA_Shutdown(void);

// ====================================================================
// User-setable variables
// ====================================================================

#define	MAX_CHANNELS			128
#define	MAX_DYNAMIC_CHANNELS	8

#define	MAX_SFX		512


extern	channel_t   channels[MAX_CHANNELS];
// 0 to MAX_DYNAMIC_CHANNELS-1	= normal entity sounds
// MAX_DYNAMIC_CHANNELS to MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS -1 = water, etc
// MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS to total_channels = static sounds

extern	int			total_channels;

//
// Fake dma is a synchronous faking of the DMA progress used for
// isolating performance in the renderer.  The fakedma_updates is
// number of times S_Update() is called per second.
//

extern qboolean 		fakedma;
extern int 			fakedma_updates;
extern int		paintedtime;
extern vec3_t listener_origin;
extern vec3_t listener_forward;
extern vec3_t listener_right;
extern vec3_t listener_up;
extern volatile dma_t *shm;
extern volatile dma_t sn;
extern vec_t sound_nominal_clip_dist;

extern	cvar_t loadas8bit;
extern	cvar_t bgmvolume;
extern	cvar_t bgmtype; 
extern	cvar_t volume;

extern qboolean	snd_initialized;

extern int		snd_blocked;

void S_LocalSound (char *s);
sfxcache_t *S_LoadSound (sfx_t *s);

wavinfo_t GetWavinfo (char *name, byte *wav, int wavlength);

void SND_InitScaletable (void);
void SNDDMA_Submit(void);

void S_AmbientOff (void);
void S_AmbientOn (void);

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
	1.363818169698586f, 1.3693063937629157f, 1.3747727084867525f, 1.3802173741842267f, 
	1.3856406460551023f, 1.391042774324356f, 1.3964240043768947f, 1.4017845768876191f, 
	1.4071247279470294f, 1.412444689182554f
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
	0.3741657386773922f, 0.35355339059327173f, 0.3316624790355378f, 0.30822070014844644f, 
	0.2828427124746164f, 0.2549509756796363f, 0.2236067977499756f, 0.187082869338693f, 
	0.14142135623730406f, 0.07071067811864379f
};

#endif
