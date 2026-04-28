extern int  mp3_init(void);
extern void mp3_deinit(void);
extern void mp3_update(void);
extern int  mp3_start_play(char *fname, int pos);
extern void mp3_stop(void);
extern volatile int  mp3_job_started;
extern int  mp3_volume;
