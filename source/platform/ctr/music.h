extern int  music_init(void);
extern void music_deinit(void);
extern void music_update(void);
extern int  music_start_play(char *fname, int pos);
extern void music_stop(void);
extern volatile int  music_job_started;
extern int  music_volume;

#define PLATFORM_VOLUME_MAX 60
