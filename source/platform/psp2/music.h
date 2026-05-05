extern int              music_init(void);
extern void             music_deinit(void);
extern void             music_update(void);
extern int              music_start_play(char *fname, int pos);
extern void             music_pause(void);
extern void             music_resume(void);
extern void             music_stop(void);
extern volatile int     music_job_started;
extern int              music_volume;
extern qboolean         music_paused;

#include <psp2/audioout.h> 
#define PLATFORM_VOLUME_MAX SCE_AUDIO_OUT_MAX_VOL
