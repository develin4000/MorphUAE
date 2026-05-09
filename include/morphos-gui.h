/*
->========================================<-
->= MorphUAE - © Copyright 2026 OnyxSoft =<-
->========================================<-
->= Version  : 1.0                       =<-
->= File     : morphos-gui.h             =<-
->= Author   : Stefan Blixth             =<-
->= Compiled : 2026-05-09                =<-
->========================================<-
*/

#ifndef MORPHOSGUI_H_
#define MORPHOSGUI_H_

//extern static Object *app          = NULL;  // MUI-Application object
//extern static Object *win_settings = NULL;         // Window object


static Object *but_use = NULL;
static Object *but_save = NULL;
static Object *but_cancel = NULL;

static Object *but_gen_reset = NULL;
static Object *but_ocs_reset = NULL;
static Object *but_ecs_reset = NULL;
static Object *but_aga_reset = NULL;
static Object *but_cus_reset = NULL;

static Object *but_gen_machine = NULL;
static Object *but_gen_sound = NULL;
static Object *but_gen_channels = NULL;
static Object *but_gen_frequency = NULL;
static Object *but_gen_bits = NULL;
static Object *but_gen_joy0 = NULL;
static Object *but_gen_joy1 = NULL;
static Object *but_gen_floppy = NULL;
static Object *but_gen_blitter = NULL;
static Object *but_gen_sprite = NULL;
static Object *but_gen_resetmode = NULL;

static Object *but_ocs_chipmem = NULL;
static Object *but_ocs_fastmem = NULL;

static Object *but_ecs_mode = NULL;
static Object *but_ecs_chipmem = NULL;
static Object *but_ecs_fastmem = NULL;
static Object *but_ecs_zorromem = NULL;

static Object *but_aga_fastmem = NULL;
static Object *but_aga_zorromem = NULL;

static Object *but_cus_cpu = NULL;
static Object *but_cus_speed = NULL;
static Object *but_cus_jit = NULL;
static Object *but_cus_chipset = NULL;
static Object *but_cus_chipmem = NULL;
static Object *but_cus_fastmem = NULL;
static Object *but_cus_zorromem = NULL;

static const char *Pages[]   = { "General", "OCS", "ECS", "AGA", "Custom", "About", NULL };

// Enumerated Objects...
enum
{
   ID_MENU_ABOUT = 1,
   ID_BUT_USE,
   ID_BUT_SAVE,
   ID_BUT_CANCEL,
   ID_BUT_GEN_RESET,
   ID_BUT_OCS_RESET,
   ID_BUT_ECS_RESET,
   ID_BUT_AGA_RESET,
   ID_BUT_CUS_RESET,
   ID_PRFS_OCS_KICKSTART,
   ID_PRFS_ECS_KICKSTART,
   ID_PRFS_AGA_KICKSTART,
   ID_PRFS_CUS_KICKSTART,
   ID_PRFS_GEN_MACHINE,
   ID_PRFS_GEN_SOUND,
   ID_PRFS_GEN_CHANNELS,
   ID_PRFS_GEN_FREQUENCY,
   ID_PRFS_GEN_BITS,
   ID_PRFS_GEN_JOY0,
   ID_PRFS_GEN_JOY1,
   ID_PRFS_GEN_FLOPPY,
   ID_PRFS_GEN_BLITTER,
   ID_PRFS_GEN_SPRITE,
   ID_PRFS_GEN_RESETMODE,
   ID_PRFS_OCS_CHIPMEM,
   ID_PRFS_OCS_FASTMEM,
   ID_PRFS_ECS_MODE,
   ID_PRFS_ECS_CHIPMEM,
   ID_PRFS_ECS_FASTMEM,
   ID_PRFS_ECS_ZORROMEM,
   ID_PRFS_AGA_FASTMEM,
   ID_PRFS_AGA_ZORROMEM,
   ID_PRFS_CUS_CPU,
   ID_PRFS_CUS_SPEED,
   ID_PRFS_CUS_JIT,
   ID_PRFS_CUS_CHIPSET,
   ID_PRFS_CUS_CHIPMEM,
   ID_PRFS_CUS_FASTMEM,
   ID_PRFS_CUS_ZORROMEM,
};


static char *cyc_gen_machine[]   = { "OCS", "ECS", "AGA", "Custom", NULL };
static char *cyc_gen_sound[]   = { "None", "Interrupts", "Normal", "Exact", NULL };
static char *cyc_gen_channels[]   = { "Mono", "Stereo", "Mixed", NULL };
static char *cyc_gen_frequency[]   = { "11025 Hz", "22050 Hz", "44100 Hz", "48000 Hz", NULL };
static char *cyc_gen_bits[]   = { "8", "16", NULL };
static char *cyc_gen_joy0[]   = { "Mouse", "Joy0","Joy1", "Kbd1", "Kbd2", "Kbd3", NULL };
static char *cyc_gen_joy1[]   = { "Mouse", "Joy0","Joy1", "Kbd1", "Kbd2", "Kbd3", NULL };
static char *cyc_gen_floppy[]  = { "Slow", "Normal", "Max", NULL };
static char *cyc_gen_blitter[]  = { "On", "Off", NULL };
static char *cyc_gen_sprite[]  = { "Sprite & Playfield", "None", "Sprites only", "Full",  NULL };
static char *cyc_gen_resetmode[]  = { "Soft", "Hard", NULL };

APTR cyc_ocs_kickstart;
static Object *ocs_kickstart_str = NULL;  // (Path string)

static char *cyc_ocs_chipmem[]  = { "0.5 Mb", "1 Mb", NULL };
static char *cyc_ocs_fastmem[]  = { "0 Mb", "1 Mb", "2 Mb", "4 Mb", "8 Mb", NULL };

APTR cyc_ecs_kickstart;
static Object *ecs_kickstart_str = NULL;  // (Path string)

static char *cyc_ecs_mode[]  = { "ECS Agnus", "ECS Denise", "Full ECS", NULL };
static char *cyc_ecs_chipmem[]  = { "1 Mb", "2 Mb", NULL };
static char *cyc_ecs_fastmem[]  = { "0 Mb", "1 Mb", "2 Mb", "4 Mb", "8 Mb", NULL };
static char *cyc_ecs_zorromem[]  = { "0 Mb", "1 Mb", "2 Mb", "4 Mb", "8 Mb", "16 Mb", "32 Mb", "64 Mb", "128 Mb", "256 Mb", NULL };

APTR cyc_aga_kickstart;
static Object *aga_kickstart_str = NULL;  // (Path string)

//static char *cyc_aga_chipmem[]  = { "1 Mb", "2 Mb", NULL };
static char *cyc_aga_fastmem[]  = { "0 Mb", "1 Mb", "2 Mb", "4 Mb", "8 Mb", NULL };
static char *cyc_aga_zorromem[]  = { "0 Mb", "1 Mb", "2 Mb", "4 Mb", "8 Mb", "16 Mb", "32 Mb", "64 Mb", "128 Mb", "256 Mb", NULL };

APTR cyc_cus_kickstart;
static Object *cus_kickstart_str = NULL;  // (Path string)
static char *cyc_cus_cpu[]  = { "68000", "68010", "68EC020", "68020", "68040", "68060", NULL };
static char *cyc_cus_speed[]  = { "Max", "Real", NULL };
static char *cyc_cus_chipset[]  = { "OCS", "ECS", "AGA",  NULL };
static char *cyc_cus_jit[]  = { "Off", "On - 4 Mb", "On - 8 Mb", "On - 16 Mb", NULL };
static char *cyc_cus_chipmem[]  = { "0.5 Mb", "1 Mb", "2 Mb", NULL };
static char *cyc_cus_fastmem[]  = { "0 Mb", "1 Mb", "2 Mb", "4 Mb", "8 Mb", NULL };
static char *cyc_cus_zorromem[]  = { "0 Mb", "1 Mb", "2 Mb", "4 Mb", "8 Mb", "16 Mb", "32 Mb", "64 Mb", "128 Mb", "256 Mb", NULL };

static const char about_text[] = "\33c\
\n\33bMorphUAE\n\n\33nThe Amiga emulator for MorphOS by Stefan Blixth, OnyxSoft\n\n\
This software is based on work previous done by :\n\n\
Richard Drummond, E-UAE\n\
Bernd Schmidt, original UAE\n\
Toni Wilen, WinUAE\n\
";


#endif /* MORPHOSGUI_H_ */
