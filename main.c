 /*
  * UAE - The Un*x Amiga Emulator
  *
  * Main program
  *
  * Copyright 1995 Ed Hanway
  * Copyright 1995, 1996, 1997 Bernd Schmidt
  * Copyright 2006-2007 Richard Drummond
  */

#include "sysconfig.h"
#include "sysdeps.h"
#include <assert.h>

#include "options.h"
#include "thread.h"
#include "uae.h"
#include "audio.h"
#include "events.h"
#include "memory.h"
#include "custom.h"
#include "serial.h"
#include "newcpu.h"
#include "disk.h"
#include "debug.h"
#include "xwin.h"
#include "drawing.h"
#include "inputdevice.h"
#include "keybuf.h"
#include "gui.h"
#include "zfile.h"
#include "autoconf.h"
#include "traps.h"
#include "osemu.h"
#include "filesys.h"
#include "picasso96.h"
#include "bsdsocket.h"
#include "uaeexe.h"
#include "native2amiga.h"
#include "scsidev.h"
#include "akiko.h"
#include "savestate.h"
#include "hrtimer.h"
#include "sleep.h"
#include "version.h"

#include <workbench/workbench.h>
#include <workbench/startup.h>

#ifdef USEDEBUG
   #include <clib/debug_protos.h>
   #define debug_print(args...) { KPrintF((CONST_STRPTR)args); }
#else
   #define debug_print(...)
#endif

struct uae_prefs currprefs, changed_prefs;

static int restart_program;
static char restart_config[256];
static char optionsfile[256];

int cloanto_rom = 0;

int rom_ok = 1;
int cfgtype = UAE_CFGTYPE_DEFAULT;
int usechksum = UAE_CHKSUM_ON;
int usefullscreen = UAE_FULLSCREEN_OFF;
int usearosrom = UAE_AROSROM_NO;
int useoverscan = UAE_OVERSCAN_OFF;
int useoldconfig = UAE_OLDCONFIG_OFF;

int log_scsi;

struct gui_info gui_data;


int uae_get_cfgtype(void)
{
    return cfgtype;
}

void uae_set_cfgtype(int value)
{
    cfgtype = value;
}

void uae_set_usearosrom(int value)
{
   usearosrom = value;
}

int uae_get_usearosrom(void)
{
   return usearosrom;
}

void uae_set_use_checksum(int value)
{
    usechksum = value;
}

int uae_get_use_checksum(void)
{
    return usechksum;
}

void uae_set_use_fullscreen(int value)
{
    usefullscreen = value;
}

int uae_get_use_fullscreen(void)
{
    return usefullscreen;
}

void uae_set_overscan(int value)
{
   useoverscan = value;
}

int uae_get_overscan(void)
{
   return useoverscan;
}

void uae_set_oldconfig(int value)
{
   useoldconfig = value;
}

int uae_get_oldconfig(void)
{
   return useoldconfig;
}

int uae_get_kick_status(void)
{
   return rom_ok;
}

void uae_set_kick_status(int value)
{
    debug_print("%s (%d) Memory Call SerKickStatus = %d\n", __func__, __LINE__, value);
    rom_ok = value;
}

/*
 * Random prefs-related junk that needs to go elsewhere.
 */

void fixup_prefs_dimensions (struct uae_prefs *prefs)
{
    if (prefs->gfx_width_fs < 320)
	prefs->gfx_width_fs = 320;
    if (prefs->gfx_height_fs < 200)
	prefs->gfx_height_fs = 200;
    if (prefs->gfx_height_fs > 1280)
	prefs->gfx_height_fs = 1280;
    prefs->gfx_width_fs += 7;
    prefs->gfx_width_fs &= ~7;
    if (prefs->gfx_width_win < 320)
	prefs->gfx_width_win = 320;
    if (prefs->gfx_height_win < 200)
	prefs->gfx_height_win = 200;
    if (prefs->gfx_height_win > 1280)
	prefs->gfx_height_win = 1280;
    prefs->gfx_width_win += 7;
    prefs->gfx_width_win &= ~7;
}

static void fixup_prefs_joysticks (struct uae_prefs *prefs)
{
    int joy_count = inputdevice_get_device_total (IDTYPE_JOYSTICK);

    /* If either port is configured to use a non-existent joystick, try
     * to use a sensible alternative.
     */
    if (prefs->jport0 >= JSEM_JOYS && prefs->jport0 < JSEM_MICE) {
	if (prefs->jport0 - JSEM_JOYS >= joy_count)
	    prefs->jport0 = (prefs->jport1 != JSEM_MICE) ? JSEM_MICE : JSEM_NONE;
    }
    if (prefs->jport1 >= JSEM_JOYS && prefs->jport1 < JSEM_MICE) {
	if (prefs->jport1 - JSEM_JOYS >= joy_count)
	    prefs->jport1 = (prefs->jport0 != JSEM_KBDLAYOUT) ? JSEM_KBDLAYOUT : JSEM_NONE;
    }
}

static void fix_options (void)
{
    int err = 0;

debug_print("%s (%d)\n", __func__, __LINE__);

    if ((currprefs.chipmem_size & (currprefs.chipmem_size - 1)) != 0
	|| currprefs.chipmem_size < 0x40000
	|| currprefs.chipmem_size > 0x800000)
    {
	currprefs.chipmem_size = 0x200000;
	write_log ("Unsupported chipmem size!\n");
	err = 1;
    }
    if (currprefs.chipmem_size > 0x80000)
	currprefs.chipset_mask |= CSMASK_ECS_AGNUS;

    if ((currprefs.fastmem_size & (currprefs.fastmem_size - 1)) != 0
	|| (currprefs.fastmem_size != 0 && (currprefs.fastmem_size < 0x100000 || currprefs.fastmem_size > 0x800000)))
    {
	currprefs.fastmem_size = 0;
	write_log ("Unsupported fastmem size!\n");
	err = 1;
    }
    if ((currprefs.gfxmem_size & (currprefs.gfxmem_size - 1)) != 0
	|| (currprefs.gfxmem_size != 0 && (currprefs.gfxmem_size < 0x100000 || currprefs.gfxmem_size > 0x2000000)))
    {
	write_log ("Unsupported graphics card memory size %lx!\n", currprefs.gfxmem_size);
	currprefs.gfxmem_size = 0;
	err = 1;
    }
    if ((currprefs.z3fastmem_size & (currprefs.z3fastmem_size - 1)) != 0
	|| (currprefs.z3fastmem_size != 0 && (currprefs.z3fastmem_size < 0x100000 || currprefs.z3fastmem_size > 0x20000000)))
    {
	currprefs.z3fastmem_size = 0;
	write_log ("Unsupported Zorro III fastmem size!\n");
	err = 1;
    }
    if (currprefs.address_space_24 && (currprefs.gfxmem_size != 0 || currprefs.z3fastmem_size != 0)) {
	currprefs.z3fastmem_size = currprefs.gfxmem_size = 0;
	write_log ("Can't use a graphics card or Zorro III fastmem when using a 24 bit\n"
		 "address space - sorry.\n");
    }
    if (currprefs.bogomem_size != 0 && currprefs.bogomem_size != 0x80000 && currprefs.bogomem_size != 0x100000 && currprefs.bogomem_size != 0x1C0000)
    {
	currprefs.bogomem_size = 0;
	write_log ("Unsupported bogomem size!\n");
	err = 1;
    }

    if (currprefs.chipmem_size > 0x200000 && currprefs.fastmem_size != 0) {
	write_log ("You can't use fastmem and more than 2MB chip at the same time!\n");
	currprefs.fastmem_size = 0;
	err = 1;
    }
#if 0
    if (currprefs.m68k_speed < -1 || currprefs.m68k_speed > 20) {
	write_log ("Bad value for -w parameter: must be -1, 0, or within 1..20.\n");
	currprefs.m68k_speed = 4;
	err = 1;
    }
#endif

    if (currprefs.produce_sound < 0 || currprefs.produce_sound > 3) {
	write_log ("Bad value for -S parameter: enable value must be within 0..3\n");
	currprefs.produce_sound = 0;
	err = 1;
    }

    if (currprefs.comptrustbyte < 0 || currprefs.comptrustbyte > 3) {
	write_log ("Bad value for comptrustbyte parameter: value must be within 0..2\n");
	currprefs.comptrustbyte = 1;
	err = 1;
    }
    if (currprefs.comptrustword < 0 || currprefs.comptrustword > 3) {
	write_log ("Bad value for comptrustword parameter: value must be within 0..2\n");
	currprefs.comptrustword = 1;
	err = 1;
    }
    if (currprefs.comptrustlong < 0 || currprefs.comptrustlong > 3) {
	write_log ("Bad value for comptrustlong parameter: value must be within 0..2\n");
	currprefs.comptrustlong = 1;
	err = 1;
    }
    if (currprefs.compoptim < 0 || currprefs.compoptim > 1) {
	write_log ("Bad value for comp_optimize parameter: value must be within 0..1\n");
	currprefs.compoptim = 0;
	err = 1;
    }
#ifdef JIT_DEBUG
    if (currprefs.complog < 0 || currprefs.complog > 1) {
	write_log ("Bad value for comp_log parameter: value must be within 0..1\n");
	currprefs.complog = 0;
	err = 1;
    }
    if (currprefs.complogcompiled < 0 || currprefs.complogcompiled > 1) {
	write_log ("Bad value for comp_log_compiled parameter: value must be within 0..1\n");
	currprefs.complogcompiled = 0;
	err = 1;
    }
#endif
    if (currprefs.comptestconsistency < 0 || currprefs.comptestconsistency > 1) {
	write_log ("Bad value for comp_test_consistency parameter: value must be within 0..1\n");
	currprefs.comptestconsistency = 0;
	err = 1;
    }
    if (currprefs.comp_hardflush < 0 || currprefs.comp_hardflush > 1) {
	write_log ("Bad value for comp_hardflush parameter: value must be within 0..1\n");
	currprefs.comp_hardflush = 1;
	err = 1;
    }
    if (currprefs.comp_constjump < 0 || currprefs.comp_constjump > 1) {
	write_log ("Bad value for comp_constjump parameter: value must be within 0..1\n");
	currprefs.comp_constjump = 1;
	err = 1;
    }
    if (currprefs.cachesize < 0 || currprefs.cachesize > 16384) {
	write_log ("Bad value for cachesize parameter: value must be within 0..16384\n");
	currprefs.cachesize = 0;
	err = 1;
    }

    if (currprefs.cpu_level < 2 && currprefs.z3fastmem_size > 0) {
	write_log ("Z3 fast memory can't be used with a 68000/68010 emulation. It\n"
		 "requires a 68020 emulation. Turning off Z3 fast memory.\n");
	currprefs.z3fastmem_size = 0;
	err = 1;
    }
    if (currprefs.gfxmem_size > 0 && (currprefs.cpu_level < 2 || currprefs.address_space_24)) {
	write_log ("Picasso96 can't be used with a 68000/68010 or 68EC020 emulation. It\n"
		 "requires a 68020 emulation. Turning off Picasso96.\n");
	currprefs.gfxmem_size = 0;
	err = 1;
    }
#ifndef BSDSOCKET
    if (currprefs.socket_emu) {
	write_log ("Compile-time option of BSDSOCKET was not enabled.  You can't use bsd-socket emulation.\n");
	currprefs.socket_emu = 0;
	err = 1;
    }
#endif

    if (currprefs.nr_floppies < 0 || currprefs.nr_floppies > 4) {
	write_log ("Invalid number of floppies.  Using 4.\n");
	currprefs.nr_floppies = 4;
	currprefs.dfxtype[0] = 0;
	currprefs.dfxtype[1] = 0;
	currprefs.dfxtype[2] = 0;
	currprefs.dfxtype[3] = 0;
	err = 1;
    }

    if (currprefs.floppy_speed > 0 && currprefs.floppy_speed < 10) {
	currprefs.floppy_speed = 100;
    }
    if (currprefs.input_mouse_speed < 1 || currprefs.input_mouse_speed > 1000) {
	currprefs.input_mouse_speed = 100;
    }

    if (currprefs.collision_level < 0 || currprefs.collision_level > 3) {
	write_log ("Invalid collision support level.  Using 1.\n");
	currprefs.collision_level = 1;
	err = 1;
    }
    fixup_prefs_dimensions (&currprefs);

#ifdef CPU_68000_ONLY
    currprefs.cpu_level = 0;
#endif


#if !defined (BSDSOCKET)
    currprefs.socket_emu = 0;
#endif
#if !defined (SCSIEMU)
    currprefs.scsi = 0;
#endif

    fixup_prefs_joysticks (&currprefs);

    if (err)
	write_log ("Please use \"uae -h\" to get usage information.\n");
}


#ifndef DONT_PARSE_CMDLINE

void usage (void)
{
debug_print("%s (%d)\n", __func__, __LINE__);
    cfgfile_show_usage ();
}

static void show_version (void)
{
debug_print("%s (%d)\n", __func__, __LINE__);
    write_log (UAE_VERSION_STRING "\n");
    write_log ("Build date: " __DATE__ " " __TIME__ "\n");
}

static void show_version_full (void)
{
debug_print("%s (%d)\n", __func__, __LINE__);
    write_log ("\n");
    show_version ();
    write_log ("\nCopyright 2003-2007 Richard Drummond and contributors.\n");
    write_log ("Based on source code from:\n");
    write_log ("UAE    - copyright 1995-2002 Bernd Schmidt;\n");
    write_log ("WinUAE - copyright 1999-2007 Toni Wilen.\n");
    write_log ("See the source code for a full list of contributors.\n");

    write_log ("This is free software; see the file COPYING for copying conditions.  There is NO\n");
    write_log ("warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n");
}

static void parse_cmdline (int argc, char **argv)
{
    int i;
debug_print("%s (%d)\n", __func__, __LINE__);

    for (i = 1; i < argc; i++) {
	if (strcmp (argv[i], "-cfgparam") == 0) {
	    if (i + 1 < argc)
		i++;
	} else if (strncmp (argv[i], "-config=", 8) == 0) {

	    free_mountinfo (currprefs.mountinfo);

	    if (cfgfile_load (&currprefs, argv[i] + 8, 0))
		strcpy (optionsfile, argv[i] + 8);
	}
	/* Check for new-style "-f xxx" argument, where xxx is config-file */
	else if (strcmp (argv[i], "-f") == 0) {
	    if (i + 1 == argc) {
		write_log ("Missing argument for '-f' option.\n");
	    } else {

		free_mountinfo (currprefs.mountinfo);

		if (cfgfile_load (&currprefs, argv[++i], 0))
		    strcpy (optionsfile, argv[i]);
	    }
	} else if (strcmp (argv[i], "-s") == 0) {
	    if (i + 1 == argc)
		write_log ("Missing argument for '-s' option.\n");
	    else
		cfgfile_parse_line (&currprefs, argv[++i], 0);
	} else if (strcmp (argv[i], "-h") == 0 || strcmp (argv[i], "-help") == 0) {
	    usage ();
	    exit (0);
	} else if (strcmp (argv[i], "-version") == 0) {
	    show_version_full ();
	    exit (0);
	} else if (strcmp (argv[i], "-scsilog") == 0) {
	    log_scsi = 1;
   } else if (strcmp (argv[i], "-ocs") == 0) {
       uae_set_cfgtype(UAE_CFGTYPE_OCS);
   } else if (strcmp (argv[i], "-ecs") == 0) {
        uae_set_cfgtype(UAE_CFGTYPE_ECS);
   } else if (strcmp (argv[i], "-aga") == 0) {
        uae_set_cfgtype(UAE_CFGTYPE_AGA);
   } else if (strcmp (argv[i], "-custom") == 0) {
        uae_set_cfgtype(UAE_CFGTYPE_CUS);
   } else if (strcmp (argv[i], "-nochecksum") == 0) {
       uae_set_use_checksum(UAE_CHKSUM_OFF);
   } else if (strcmp (argv[i], "-fullscreen") == 0) {
       uae_set_use_fullscreen(UAE_FULLSCREEN_ON);
   } else if (strcmp (argv[i], "-overscan") == 0) {
       uae_set_overscan(UAE_OVERSCAN_ON);
   } else if (strcmp (argv[i], "-oldconf") == 0) {
       uae_set_oldconfig(UAE_OLDCONFIG_ON);
	} else {
	    if (argv[i][0] == '-' && argv[i][1] != '\0') {
		const char *arg = argv[i] + 2;
		int extra_arg = *arg == '\0';
		if (extra_arg)
		    arg = i + 1 < argc ? argv[i + 1] : 0;
		if (parse_cmdline_option (&currprefs, argv[i][1], (char*)arg) && extra_arg)
		    i++;
	    }
	}
    }
}
#endif

static void parse_cmdline_and_init_file (int argc, char **argv)
{
    char *home;
debug_print("%s (%d)\n", __func__, __LINE__);
    strcpy (optionsfile, "");

#ifdef OPTIONS_IN_HOME
    home = getenv ("HOME");
    if (home != NULL && strlen (home) < 240)
    {
	strcpy (optionsfile, home);
	strcat (optionsfile, "/");
    }
#endif

    strcat (optionsfile, OPTIONSFILENAME);

    if (! cfgfile_load (&currprefs, optionsfile, 0)) {
//	write_log ("failed to load config '%s'\n", optionsfile);
#ifdef OPTIONS_IN_HOME
	/* sam: if not found in $HOME then look in current directory */
	char *saved_path = strdup (optionsfile);
	strcpy (optionsfile, OPTIONSFILENAME);
	if (! cfgfile_load (&currprefs, optionsfile, 0) ) {
	    /* If not in current dir either, change path back to home
	     * directory - so that a GUI can save a new config file there */
	    strcpy (optionsfile, saved_path);
	}

	free (saved_path);
#endif
    }
    fix_options ();

    parse_cmdline (argc, argv);

    fix_options ();
}

/*
 * Save the currently loaded configuration.
 */
void uae_save_config (void)
{
    FILE *f;
    char tmp[257];
debug_print("%s (%d)\n", __func__, __LINE__);
    /* Back up the old file.  */
    strcpy (tmp, optionsfile);
    strcat (tmp, "~");
    write_log ("Backing-up config file '%s' to '%s'\n", optionsfile, tmp);
    rename (optionsfile, tmp);

    write_log ("Writing new config file '%s'\n", optionsfile);
    f = fopen (optionsfile, "w");
    if (f == NULL) {
	gui_message ("Error saving configuration file.!\n"); // FIXME - better error msg.
	return;
    }

    // FIXME  - either fix this nonsense, or only allow config to be saved when emulator is stopped.
    if (uae_get_state () == UAE_STATE_STOPPED)
	save_options (f, &changed_prefs, 0);
    else
	save_options (f, &currprefs, 0);

    fclose (f);
}


/*
 * A first cut at better state management...
 */

static int uae_state;
static int uae_target_state;

int uae_get_state (void)
{
   debug_print("%s (%d)\n", __func__, __LINE__);
   return uae_state;
}

static void set_state (int state)
{
   debug_print("%s (%d)\n", __func__, __LINE__);
   uae_state = state;
   gui_notify_state (state);
   graphics_notify_state (state);
}

int uae_state_change_pending (void)
{
//debug_print("%s (%d)\n", __func__, __LINE__);
   return uae_state != uae_target_state;
}

void uae_start (void)
{
   debug_print("%s (%d)\n", __func__, __LINE__);
   uae_target_state = UAE_STATE_COLD_START;
}

void uae_pause (void)
{
   debug_print("%s (%d)\n", __func__, __LINE__);
   if (uae_target_state == UAE_STATE_RUNNING)
      uae_target_state = UAE_STATE_PAUSED;
}

void uae_resume (void)
{
   debug_print("%s (%d)\n", __func__, __LINE__);
   if (uae_target_state == UAE_STATE_PAUSED)
      uae_target_state = UAE_STATE_RUNNING;
}

void uae_quit (void)
{
   debug_print("%s (%d)\n", __func__, __LINE__);
   if (uae_target_state != UAE_STATE_QUITTING)
   {
      uae_target_state = UAE_STATE_QUITTING;
   }
}

void uae_stop (void)
{
   debug_print("%s (%d)\n", __func__, __LINE__);
   if (uae_target_state != UAE_STATE_QUITTING && uae_target_state != UAE_STATE_STOPPED)
   {
      uae_target_state = UAE_STATE_STOPPED;
      restart_config[0] = 0;
   }
}

void uae_reset (int hard_reset)
{
debug_print("%s (%d)\n", __func__, __LINE__);
    switch (uae_target_state) {
	case UAE_STATE_QUITTING:
	case UAE_STATE_STOPPED:
	case UAE_STATE_COLD_START:
	case UAE_STATE_WARM_START:
	    /* Do nothing */
	    break;
	default:
	    uae_target_state = hard_reset ? UAE_STATE_COLD_START : UAE_STATE_WARM_START;
    }
}

/* This needs to be rethought */
void uae_restart (int opengui, char *cfgfile)
{
   debug_print("%s (%d)\n", __func__, __LINE__);
   uae_stop ();
   restart_program = opengui > 0 ? 1 : (opengui == 0 ? 2 : 3);
   restart_config[0] = 0;
   if (cfgfile)
      strcpy (restart_config, cfgfile);
}


/*
 * Early initialization of emulator, parsing of command-line options,
 * and loading of config files, etc.
 *
 * TODO: Need better cohesion! Break this sucker up!
 */
static int do_preinit_machine (int argc, char **argv)
{
   debug_print("%s (%d)\n", __func__, __LINE__);
   if (! graphics_setup (argc, argv))
   {
      exit (1);
   }

   if (restart_config[0])
   {
      free_mountinfo (currprefs.mountinfo);

      default_prefs (&currprefs, 0);
      fix_options ();
   }

   rtarea_init ();
   hardfile_install ();

   if (restart_config[0])
      parse_cmdline_and_init_file (argc, argv);
   else
      currprefs = changed_prefs;

   uae_inithrtimer ();

   machdep_init ();

   if (! audio_setup ())
   {
      write_log ("Sound driver unavailable: Sound output disabled\n");
      currprefs.produce_sound = 0;
   }
   inputdevice_init ();

   return 1;
}

/*
 * Initialization of emulator proper
 */
static int do_init_machine (void)
{
   debug_print("%s (%d)\n", __func__, __LINE__);
   if (!(( currprefs.cpu_level >= 2 ) && ( currprefs.address_space_24 == 0 ) && ( currprefs.cachesize )))
      canbang = 0;

   savestate_init ();
#ifdef SCSIEMU
   scsidev_install ();
#endif

   /* Install resident module to get 8MB chipmem, if requested */
   rtarea_setup ();
   keybuf_init (); /* Must come after init_joystick */

   expansion_init ();

   memory_init ();
   uae_set_kick_status(1);
   memory_reset ();

   filesys_install ();

   bsdlib_install ();
   emulib_install ();
   uaeexe_install ();
   native2amiga_install ();

   if (custom_init ())
   { /* Must come after memory_init */
#ifdef SERIAL_PORT
      serial_init ();
#endif
      DISK_init ();

      reset_frame_rate_hack ();
      init_m68k(); /* must come after reset_frame_rate_hack (); */

      gui_update ();

      if (graphics_init ())
      {
         setup_brkhandler ();

         if (currprefs.start_debugger && debuggable ())
            activate_debugger ();

         if (sound_available && currprefs.produce_sound > 1 && ! audio_init ())
         {
            write_log ("Sound driver unavailable: Sound output disabled\n");
            currprefs.produce_sound = 0;
         }

      return 1;
      }
   }
   return 0;
}

/*
 * Helper for reset method
 */
static void reset_all_systems (void)
{
   debug_print("%s (%d)\n", __func__, __LINE__);
   init_eventtab ();
   uae_set_kick_status(1);
   memory_reset ();
#ifdef BSDSOCKET
   bsdlib_reset ();
#endif

   filesys_reset ();
   filesys_start_threads ();
   hardfile_reset ();

#ifdef SCSIEMU
   scsidev_reset ();
   scsidev_start_threads ();
#endif
}

/*
 * Reset emulator
 */
static void do_reset_machine (int hardreset)
{
   debug_print("%s (%d)\n", __func__, __LINE__);
   if (savestate_state == STATE_RESTORE)
      restore_state (savestate_fname);
   else if (savestate_state == STATE_REWIND)
      savestate_rewind ();

   /* following three lines must not be reordered or
    * fastram state restore breaks
    */
   reset_all_systems ();
   customreset ();
   m68k_reset ();
   if (hardreset)
   {
      memset (chipmemory, 0, allocated_chipmem);
      write_log ("chipmem cleared\n");
   }

   /* We may have been restoring state, but we're done now.  */
   if (savestate_state == STATE_RESTORE || savestate_state == STATE_REWIND)
   {
      map_overlay (1);
      fill_prefetch_slow (&regs); /* compatibility with old state saves */
   }
   savestate_restore_finish ();

   fill_prefetch_slow (&regs);
   if (currprefs.produce_sound == 0)
      eventtab[ev_audio].active = 0;
   handle_active_events ();

   inputdevice_updateconfig (&currprefs);
}

/*
 * Run emulator
 */
static void do_run_machine (void)
{
   debug_print("%s (%d)\n", __func__, __LINE__);
   if (uae_get_kick_status())
      m68k_go (1);
}

/*
 * Exit emulator
 */
static void do_exit_machine (void)
{
   debug_print("%s (%d)\n", __func__, __LINE__);
   graphics_leave ();
   inputdevice_close ();

   compemu_cleanup();

#ifdef SCSIEMU
   scsidev_exit ();
#endif
   DISK_free ();

   audio_close ();
   dump_counts ();
#ifdef SERIAL_PORT
   serial_exit ();
#endif
#ifdef CD32
   akiko_free ();
#endif
   gui_exit ();

   expansion_cleanup ();

   filesys_cleanup ();
   hardfile_cleanup ();

   savestate_free ();

   memory_cleanup ();
   cfgfile_addcfgparam (0);
}



/*
 * Here's where all the action takes place!
 */
void real_main (int argc, char **argv)
{
   debug_print("%s (%d)\n", __func__, __LINE__);
   //show_version ();

   currprefs.mountinfo = changed_prefs.mountinfo = &options_mountinfo;

   restart_program = 1;

   strcat (restart_config, OPTIONSFILENAME);

   /* Initial state is stopped */
   uae_target_state = UAE_STATE_STOPPED;

   while (uae_target_state != UAE_STATE_QUITTING)
   {
      set_state (uae_target_state);
      do_preinit_machine (argc, argv);
      changed_prefs = currprefs;

      restart_program = 0;

      if (uae_target_state == UAE_STATE_QUITTING)
         break;

      uae_target_state = UAE_STATE_COLD_START;

      /* Start emulator proper. */
      if (!do_init_machine ())
         break;

      while (uae_target_state != UAE_STATE_QUITTING && uae_target_state != UAE_STATE_STOPPED)
      {
         /* Reset */
         set_state (uae_target_state);
         do_reset_machine (uae_state == UAE_STATE_COLD_START);
         uae_msleep (1000);
         /* Running */
         uae_target_state = UAE_STATE_RUNNING;

         /*
          * Main Loop
         */
         do
         {
            set_state (uae_target_state);

            /* Run emulator. */
            do_run_machine ();

            if (uae_target_state == UAE_STATE_PAUSED) 
            {
               /* Paused */
               set_state (uae_target_state);

               audio_pause ();

               /* While UAE is paused we have to handle
                * input events, etc. ourselves.
               */
               do
               {
                  gui_handle_events ();
                  handle_events ();

                  /* Manually pump input device */
                  inputdevicefunc_keyboard.read ();
                  inputdevicefunc_mouse.read ();
                  inputdevicefunc_joystick.read ();
                  inputdevice_handle_inputcode ();

                  /* Don't busy wait. */
                  uae_msleep (10);

               } while (!uae_state_change_pending ());

               audio_resume ();
            }

         } while (uae_target_state == UAE_STATE_RUNNING);

         /*
          * End of Main Loop
          *
          * We're no longer running or paused.
         */

         set_inhibit_frame (IHF_QUIT_PROGRAM);

         /* Ensure any cached changes to virtual filesystem are flushed before
          * resetting or exitting. */
         filesys_prepare_reset ();

      } /* while (!QUITTING && !STOPPED) */

      do_exit_machine ();

      /* TODO: This stuff is a hack. What we need to do is
       * check whether a config GUI is available. If not,
       * then quit.
      */
      restart_program = 3;
   }
   zfile_exit ();
}

#ifndef NO_MAIN_IN_MAIN_C
int main (int argc, char **argv)
{
   debug_print("%s (%d)\n", __func__, __LINE__);
   //gui_init (argc, argv);
   gui_init ();

   real_main (argc, argv);
   return 0;
}
#endif

#ifdef SINGLEFILE
uae_u8 singlefile_config[50000] = { "_CONFIG_STARTS_HERE" };
uae_u8 singlefile_data[1500000] = { "_DATA_STARTS_HERE" };
#endif
