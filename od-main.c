/*
 * E-UAE - The portable Amiga emulator
 *
 * Copyright 2004-2006 Richard Drummond
 *
 * Start-up and support functions for Amiga target
 */

#include "sysconfig.h"
#include "sysdeps.h"

#include "options.h"
#include "uae.h"
#include "xwin.h"
#include "debug.h"

#include "signal.h"

#include "version.h"

#define  __USE_BASETYPE__
#include <proto/exec.h>
#undef   __USE_BASETYPE__
#include <exec/execbase.h>


/* Get compiler/libc to enlarge stack to this size - if possible */
#if defined __PPC__ || defined __ppc__ || defined POWERPC || defined __POWERPC__
# define MIN_STACK_SIZE  (64 * 1024)
#endif

#if defined __libnix__ || defined __ixemul__
/* libnix requires that we link against the swapstack.o module */
unsigned int __stack = MIN_STACK_SIZE;
#endif

struct Device *TimerBase;

//Version tag string for AmigaOS version command
//Not perfect: format of date supposed to be: dd.MM.yyyy, but that format is not available
//at compile time. Could be resolved using a clever define in the makefile...
char* AMIGAOS_VERSION_TAG = "$VER: " UAE_VERSION_STRING " (" __DATE__ ")";

static void free_libs (void)
{

}

static void init_libs (void)
{
    atexit (free_libs);

    TimerBase = (struct Device *) FindName(&SysBase->DeviceList, "timer.device");
}

static int fromWB;
static FILE *logfile;

/*
 * Amiga-specific main entry
 */
int main (int argc, char *argv[])
{
    fromWB = argc == 0;

    if (fromWB)
	set_logfile ("T:E-UAE.log");

    init_libs ();

    real_main (argc, argv);

    if (fromWB)
	set_logfile (0);

    return 0;
}

/*
 * Handle CTRL-C signals
 */
static RETSIGTYPE sigbrkhandler(int foo)
{
#ifdef DEBUGGER
    activate_debugger ();
#endif
}

#undef HAVE_SIGACTION

void setup_brkhandler (void)
{
#ifdef HAVE_SIGACTION
    struct sigaction sa;
    sa.sa_handler = (void*)sigbrkhandler;
    sa.sa_flags = 0;
    sa.sa_flags = SA_RESTART;
    sigemptyset (&sa.sa_mask);
    sigaction (SIGINT, &sa, NULL);
#else
    signal (SIGINT,sigbrkhandler);
#endif
}


/*
 * Handle target-specific cfgfile options
 */
void target_save_options (FILE *f, const struct uae_prefs *p)
{
}

int target_parse_option (struct uae_prefs *p, const char *option, const char *value)
{
    return 0;
}

void target_default_options (struct uae_prefs *p)
{
}
