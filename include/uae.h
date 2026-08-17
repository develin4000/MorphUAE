 /*
  * UAE - The Un*x Amiga Emulator
  *
  * Prototypes for main.c
  *
  * Copyright 1996 Bernd Schmidt
  * Copyright 2006-2007 Richard Drummond
  */

extern void real_main (int, char **);
extern void usage (void);

extern void sleep_millis (int ms);
extern void sleep_millis_busy (int ms);

extern void uae_start (void);
extern void uae_pause (void);
extern void uae_resume (void);
extern void uae_reset (int);
extern void uae_quit (void);
extern void uae_stop (void);
extern void uae_restart (int, char*);
extern void uae_save_config (void);

extern int uae_get_kick_status(void);
extern void uae_set_kick_status(int);
extern void uae_set_cfgtype(int);
extern int uae_get_cfgtype(void);

extern void uae_set_use_checksum(int);
extern int uae_get_use_checksum(void);

extern void uae_set_use_fullscreen(int);
extern int uae_get_use_fullscreen(void);

extern void uae_set_usearosrom(int);
extern int uae_get_usearosrom(void);

extern void uae_set_overscan(int);
extern int uae_get_overscan(void);

extern void uae_set_doublebuffer(int);
extern int uae_get_doublebuffer(void);

extern void uae_set_oldconfig(int);
extern int uae_get_oldconfig(void);

#define UAE_CHKSUM_ON       0
#define UAE_CHKSUM_OFF      1

#define UAE_FULLSCREEN_OFF  0
#define UAE_FULLSCREEN_ON   1

#define UAE_CFGTYPE_OCS     0
#define UAE_CFGTYPE_ECS     1
#define UAE_CFGTYPE_AGA     2
#define UAE_CFGTYPE_CUS     3
#define UAE_CFGTYPE_DEFAULT 4

#define UAE_AROSROM_NO      0
#define UAE_AROSROM_YES     1

#define UAE_OVERSCAN_OFF    0
#define UAE_OVERSCAN_ON     1

#define UAE_DOUBLEBUFFER_ON 1

#define UAE_OLDCONFIG_OFF   0
#define UAE_OLDCONFIG_ON    1


extern void setup_brkhandler (void);

#define UAE_STATE_STOPPED    0
#define UAE_STATE_RUNNING    1
#define UAE_STATE_PAUSED     2
#define UAE_STATE_COLD_START 3
#define UAE_STATE_WARM_START 4
#define UAE_STATE_QUITTING   5

int uae_get_state (void);
int uae_state_change_pending (void);
