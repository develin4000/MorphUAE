 /*
  * UAE - The Un*x Amiga Emulator
  *
  * Miscellaneous machine dependent support functions and definitions
  *
  * Copyright 1996 Bernd Schmidt
  * Copyright 2003-2005 Richard Drummond
  */

#include "sysconfig.h"
#include "sysdeps.h"

#include "options.h"
#include "sleep.h"
#include "rpt.h"
#include "m68k.h"

//#ifndef HAVE_SYNC
# define sync()
//#endif

frame_time_t timebase;


# include <proto/timer.h>

static frame_time_t machdep_morphos_gettimebase (void)
{
    UQUAD cputime;

    frame_time_t result = ReadCPUClock (&cputime);

    return result;
}


/*
 * Calibrate PPC timebase frequency the hard way...
 * This is still dumb and horribly inefficient.
 */
static frame_time_t machdep_calibrate_timebase (void)
{
    const int num_loops = 5;
    frame_time_t last_time;
    frame_time_t best_time;
    int i;

    write_log ("Calibrating timebase...\n");
    flush_log ();

    sync ();
    last_time = read_processor_time ();
    for (i = 0; i < num_loops; i++)
	uae_msleep (1000);
    best_time = read_processor_time () - last_time;

    return best_time / num_loops;
}

int machdep_inithrtimer (void)
{
    static int done = 0;

    if (!done) {
	    timebase = machdep_morphos_gettimebase ();

	if (!timebase)
	    timebase = machdep_calibrate_timebase ();

	write_log ("Timebase frequency: %.6f MHz\n", timebase / 1e6);

	done = 1;
    }
    return 1;
}

frame_time_t machdep_gethrtimebase (void)
{
    return timebase;
}

void machdep_init (void)
{
}

/*
 * Handle processor-specific cfgfile options
 */
void machdep_save_options (FILE *f, const struct uae_prefs *p)
{
    cfgfile_write (f, MACHDEP_NAME ".use_tbc=%s\n", p->use_processor_clock ? "yes" : "no");
}

int machdep_parse_option (struct uae_prefs *p, const char *option, const char *value)
{
    return cfgfile_yesno (option, value, "use_tbc", &p->use_processor_clock);
}

void machdep_default_options (struct uae_prefs *p)
{
    p->use_processor_clock = 1;
}
