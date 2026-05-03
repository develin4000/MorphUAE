 /*
  * UAE - The Un*x Amiga Emulator
  *
  * GUI interface (to be done).
  * Calls AREXX interface.
  *
  * Copyright 1996 Bernd Schmidt, Samuel Devulder
  * Copyright 2004-2006 Richard Drummond
  */

#include "sysconfig.h"
#include "sysdeps.h"

#include <intuition/intuition.h>
#include <libraries/asl.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/asl.h>
#include <dos/dosextens.h>

/****************************************************************************/


/****************************************************************************/


/* File dialog types */
#define FILEDIALOG_INSERT_DF0    0
#define FILEDIALOG_INSERT_DF1    1
#define FILEDIALOG_INSERT_DF2    2
#define FILEDIALOG_INSERT_DF3    3
#define FILEDIALOG_LOAD_STATE    4
#define FILEDIALOG_SAVE_STATE    5
#define FILEDIALOG_MAX           6

#define FILEDIALOG_DRIVE(x) ((x)-FILEDIALOG_INSERT_DF0)

/* For remembering last directory used in file requesters */

//
#ifndef USEDEBUG
 #define USEDEBUG
#endif

#ifdef USEDEBUG
   #include <clib/debug_protos.h>
   #define debug_print(args...) { KPrintF((CONST_STRPTR)args); }
#else
   #define debug_print(...)
#endif
//
extern void update_led_status(int led, int on);

static char *last_floppy_dir;
static char *last_savestate_dir;

static void free_last_floppy_dir (void)
{
   debug_print("%s (%d)\n", __func__, __LINE__);
    if (last_floppy_dir) {
	free (last_floppy_dir);
	last_floppy_dir = 0;
    }
}

static void free_last_savestate_dir (void)
{
   debug_print("%s (%d)\n", __func__, __LINE__);
    if (last_savestate_dir) {
	free (last_savestate_dir);
	last_savestate_dir = 0;
    }
}

static const char *get_last_floppy_dir (void)
{
   debug_print("%s (%d)\n", __func__, __LINE__);
    if (!last_floppy_dir) {
	static int done = 0;
	unsigned int len;

	if (!done) {
	    done = 1;
	    atexit (free_last_floppy_dir);
	}

	last_floppy_dir = my_strdup (prefs_get_attr ("floppy_path"));
    }
    return last_floppy_dir;
}

static const char *get_last_savestate_dir (void)
{
   debug_print("%s (%d)\n", __func__, __LINE__);
    if (!last_savestate_dir) {
	static int done = 0;
	unsigned int len;

	if (!done) {
	    done = 1;
	    atexit (free_last_savestate_dir);
	}

	last_savestate_dir = my_strdup (prefs_get_attr ("savestate_path"));
    }
    return last_savestate_dir;
}

static void set_last_floppy_dir (const char *path)
{
   debug_print("%s (%d)\n", __func__, __LINE__);
    if (last_floppy_dir) {
	free (last_floppy_dir);
	last_floppy_dir = 0;
    }

    if (path) {
	unsigned int len = strlen (path);
	if (len) {
	    last_floppy_dir = malloc (len + 1);
	    if (last_floppy_dir)
		strcpy (last_floppy_dir, path);
	}
    }
}

static void set_last_savestate_dir (const char *path)
{
   debug_print("%s (%d)\n", __func__, __LINE__);
    if (last_savestate_dir) {
	free (last_savestate_dir);
	last_savestate_dir = 0;
    }

    if (path) {
	unsigned int len = strlen (path);
	if (len) {
	    last_savestate_dir = malloc (len + 1);
	    if (last_savestate_dir)
		strcpy (last_savestate_dir, path);
	}
    }
}

static void do_file_dialog (unsigned int type)
{
   debug_print("%s (%d)\n", __func__, __LINE__);
    return;
}

/****************************************************************************/

void gui_init (void)
{
   debug_print("%s (%d)\n", __func__, __LINE__);
}

/****************************************************************************/

static int have_rexx = 0;

int gui_open (void)
{
   debug_print("%s (%d)\n", __func__, __LINE__);
   return -1;
}

/****************************************************************************/

void gui_exit (void)
{
   debug_print("%s (%d)\n", __func__, __LINE__);
}

/****************************************************************************/

int gui_update (void)
{
   debug_print("%s (%d)\n", __func__, __LINE__);
    return 0;
}

/****************************************************************************/

void gui_led (int led, int on)
{
   update_led_status(led, on); // Call to gfx-morphos...
   //debug_print("%s (%d) : LED-%d - %d\n", __func__, __LINE__, led, on);
}

/****************************************************************************/

void gui_filename (int num, const char *name)
{
   debug_print("%s (%d) - %d : %s\n", __func__, __LINE__, num, name);
}

/****************************************************************************/

void gui_handle_events (void)
{
   //debug_print("%s (%d)\n", __func__, __LINE__);
}

/****************************************************************************/

void gui_notify_state (int state)
{
   debug_print("%s (%d)\n", __func__, __LINE__);
}

/****************************************************************************/

void gui_hd_led (int led)
{
   //debug_print("%s (%d)\n", __func__, __LINE__);
}

/****************************************************************************/

void gui_cd_led (int led)
{
   //debug_print("%s (%d)\n", __func__, __LINE__);
}

/****************************************************************************/

void gui_fps (int fps, int idle)
{
   //debug_print("%s (%d) - FPS=%d - Idle=%d\n", __func__, __LINE__, fps, idle);
}

/****************************************************************************/

void gui_display (int shortcut)
{
   debug_print("%s (%d) - shortcut = %d\n", __func__, __LINE__, shortcut);
}

/****************************************************************************/

void gui_message (const char *format,...)
{
   //MUI_Request(NULL, NULL, 0L, "Error Message", "Ok", "MorphUAE needs a valid ROM file to be able to work\nPlease adjust the settings!");
   debug_print("%s (%d)\n", __func__, __LINE__);
}
