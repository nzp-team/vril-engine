extern int              music_init(void);
extern void             music_deinit(void);
extern int              music_start_play(char *fname, int pos);
extern void             music_pause(void);
extern void             music_stop(void);
extern void             music_resume(void);
extern volatile int     music_job_started;
extern int              music_volume;
extern qboolean         music_paused;

#define PLATFORM_VOLUME_MAX 1
