/*
  * UAE - The Un*x Amiga Emulator
  *
  * Amiga interface
  *
  * Copyright 1996,1997,1998 Samuel Devulder.
  * Copyright 2003-2007 Richard Drummond
  *
  * MorphOS (MUI) Magic User Interface - MorphUAE
  * Copyright 2025-2026 Stefan Blixth, OnyxSoft
  *
  * Camera SVG Vector :
  * (c) Diemen Design
  * https://www.svgrepo.com/svg/443599/camera
  *
  * Joystick SVG Vector
  * (c) Bootstrap
  * https://www.svgrepo.com/svg/344949/joystick
  *
  * Arrow Clockwise SVG Vector
  * (c) Bootstrap
  * https://www.svgrepo.com/svg/344397/arrow-clockwise
  *
  * Arrows Fullscreen SVG Vector
  * (c) Bootstrap
  * https://www.svgrepo.com/svg/344427/arrows-fullscreen
  *
  * Resume SVG Vector
  * (c) radix-ui
  * https://www.svgrepo.com/svg/361583/resume
  */

#include "sysconfig.h"
#include "sysdeps.h"

/****************************************************************************/

#include <exec/execbase.h>
#include <exec/memory.h>

#include <dos/dos.h>
#include <dos/dosextens.h>

#include <graphics/gfxbase.h>
#include <graphics/displayinfo.h>

#include <libraries/asl.h>
#include <intuition/pointerclass.h>
#include <libraries/gadtools.h>

/****************************************************************************/

# include <proto/intuition.h>
# include <proto/graphics.h>
# include <proto/layers.h>
# include <proto/exec.h>
# include <proto/dos.h>
# include <proto/asl.h>
# include <proto/icon.h>
# include <proto/utility.h>
# include <proto/muimaster.h>
# include <libraries/mui.h>
# include <proto/utility.h>

#include <proto/cybergraphics.h>
#include <cybergraphx/cybergraphics.h>

#include <proto/multimedia.h>
#include <classes/multimedia/video.h>
#include <classes/multimedia/metadata.h>
#include <proto/png.h>

/****************************************************************************/

#include <ctype.h>
#include <signal.h>

/****************************************************************************/

#include "uae.h"
#include "options.h"
#include "custom.h"
#include "xwin.h"
#include "drawing.h"
#include "inputdevice.h"
#include "keyboard.h"
#include "keybuf.h"
#include "gui.h"
#include "debug.h"
#include "hotkeys.h"
#include "version.h"

#include <gfx-icons.h>
#include <gfx-logo.h>
#include <morphos-gui.h>

// MCC Classes
#include <mui/Rawimage_mcc.h>
#include <LEDmcc.h>

#define VERS     "1.0"
#define DATE     __AMIGADATE__
#define VSTRING  VERS" ("DATE") "

#define ABOUTSTR "\33c \n \33bMorphUAE " VSTRING " \n\n \33nThe Amiga emulator for MorphOS by Stefan Blixth, OnyxSoft \n\n This software is based on work previous done by : \n\n Richard Drummond, E-UAE \n\ Bernd Schmidt, original UAE \n\ Toni Wilen, WinUAE"

/****************************************************************************/

extern xcolnr xcolors[4096];
//extern struct uae_prefs currprefs;
/****************************************************************************/
/*
 * prototypes & global vars
 */

struct IntuitionBase *IntuitionBase = NULL;
struct GfxBase       *GfxBase = NULL;
struct Library       *IconBase = NULL;
struct Library       *UtilityBase = NULL;
struct Library       *LayersBase = NULL;
struct Library       *AslBase = NULL;
struct Library       *CyberGfxBase = NULL;
struct Library       *MUIMasterBase = NULL;
struct Library       *RawFilterBase = NULL;
struct Library       *MemoryStreamBase = NULL;
struct Library       *FileOutputBase = NULL;

struct vidbuf_description *tmp_gfxinfo;
int tmp_line_no;
int tmp_first_line;
int tmp_last_line;
BOOL uae_restarted = FALSE;
int valid_kick = 0;

static int init_colors(void);
static int dummy_lock (struct vidbuf_description *gfxinfo);
static void dummy_unlock (struct vidbuf_description *gfxinfo);
static void flush_clear_screen_gfxlib (struct vidbuf_description *gfxinfo);
static void dummy_flush_screen (struct vidbuf_description *gfxinfo, int first_line, int last_line);

static void flush_line_cgx (struct vidbuf_description *gfxinfo, int line_no);
static void flush_block_cgx (struct vidbuf_description *gfxinfo, int first_line, int last_line);

#ifdef USEDEBUG
   #include <clib/debug_protos.h>
   #define debug_print(args...) { KPrintF((CONST_STRPTR)args); }
#else
   #define debug_print(...)
#endif

#define MyKeyString(contents, maxlen, controlchar, id)\
   StringObject,\
      StringFrame,\
      MUIA_ControlChar    , controlchar,\
      MUIA_String_MaxLen  , maxlen,\
      MUIA_String_Contents, contents,\
      MUIA_ObjectID, id,\
      MUIA_UserData, id,\
      End

#define MyPopString(contents, id)\
   StringObject,\
      StringFrame,\
      MUIA_String_Contents, contents,\
      MUIA_ObjectID, id,\
      MUIA_UserData, id,\
      End

#define NOPROF __attribute__((no_instrument_function))

#define MUI_HOOK(n, y, z) \
   static LONG n##_GATE(void); \
   static LONG n##_GATE2(struct Hook *h, y, z); \
   static const struct EmulLibEntry n = { TRAP_LIB, 0, (void (*)(void))n##_GATE }; \
   static LONG NOPROF n##_GATE(void) { return (n##_GATE2((void *)REG_A0, (void *)REG_A2, (void *)REG_A1)); } \
   static const struct Hook n##_hook = { { 0, 0}, (void *)&n, (void *)&n##_GATE2 }; \
   static LONG n##_GATE2(struct Hook *h, y, z)

#ifndef MUIA_Window_Frontdrop
   #define MUIA_Window_Frontdrop 0x80426411
#endif


/* Compiler specific stuff */

#define REG(x)

#ifndef DISPATCHER
#define DISPATCHER(Name) \
static ULONG Name##_Dispatcher(void); \
struct EmulLibEntry GATE ##Name##_Dispatcher = { TRAP_LIB, 0, (void (*)(void)) Name##_Dispatcher }; \
static ULONG Name##_Dispatcher(void) { struct IClass *cl=(struct IClass*)REG_A0; Msg msg=(Msg)REG_A1; Object *obj=(Object*)REG_A2;
#define DISPATCHER_REF(Name) &GATE##Name##_Dispatcher
#define DISPATCHER_END }
#endif

#ifndef MAKE_ID
 #define MAKE_ID(a,b,c,d) ((ULONG) (a)<<24 | (ULONG) (b)<<16 | (ULONG) (c)<<8 | (ULONG) (d))
#endif

// Global variables and defines...
// Size and Style defines...

static Object *app             = NULL;  // MUI-Application object
static Object *win_main        = NULL;  // MUI-Window object
static Object *win_settings    = NULL;  // Window object
static Object *obj_rendermcc   = NULL;  // Render object
struct Object *btn_settings    = NULL;
struct Object *btn_camera      = NULL;
struct Object *btn_reset       = NULL;
struct Object *btn_eject       = NULL;
struct Object *btn_fullscreen  = NULL;
struct Object *btn_pauseresume = NULL;
struct Object *grp_toolbar     = NULL;
struct Object *ctm_reset       = NULL;
struct Object *ctm_eject       = NULL;

struct DiskObject *morphuae_icon = NULL;

#define DEFAULT_GFX_WIDTH 640
#define DEFAULT_GFX_HEIGHT 512


struct RenderData
{
   BOOL Active;
   BOOL InitOK;
   BOOL showpointer;
   BOOL FullScreen;
   BOOL ToolBar;
   struct Window *window;
   struct Screen *screen, *ogscreen;
   ULONG modeid;

   /* Events */
   struct MUI_EventHandlerNode eh;

   struct BitMap *BitMap;
   uae_u8 *Buffer;
   int XOffset,YOffset;
   unsigned long render_state;
   WORD   WinWidth;
   WORD   WinHeight;
   WORD   ScrWidth;
   WORD   ScrHeight;
   int    Depth;
   WORD   MouseX;
   WORD   MouseY;
   WORD   OldX;
   WORD   OldY;
};


#define SERIALNUMBER            (1)
#define TAGBASE_DEVELIN         (TAG_USER | (SERIALNUMBER<<16))
#define MUIV_HKTriggerQuit      (TAGBASE_DEVELIN | 0x0001)
#define MUIV_HKTriggerResetS    (TAGBASE_DEVELIN | 0x0002)
#define MUIV_HKTriggerResetH    (TAGBASE_DEVELIN | 0x0003)

#define MUIA_Initializing_Gfx   (TAGBASE_DEVELIN | 0x0004)
#define MUIA_Cleanup_Gfx        (TAGBASE_DEVELIN | 0x0005)
#define MUIA_Pointer_State      (TAGBASE_DEVELIN | 0x0006)
#define MUIA_Render_State       (TAGBASE_DEVELIN | 0x0007)

// Initial setup...
#define MUIV_TestTrigger        (TAGBASE_DEVELIN | 0x0008)
#define MUIV_InitGraphics       (TAGBASE_DEVELIN | 0x0009)
#define MUIV_InitColours        (TAGBASE_DEVELIN | 0x000a)
#define MUIV_CleanupGraphics    (TAGBASE_DEVELIN | 0x000b)

// Mouse pointers...
#define MUIV_ShowPointer        (TAGBASE_DEVELIN | 0x000c)
#define MUIV_HidePointer        (TAGBASE_DEVELIN | 0x000d)

// Render flushes...
#define MUIV_FlushLineCGX       (TAGBASE_DEVELIN | 0x000e)
#define MUIV_FlushBlockCGX      (TAGBASE_DEVELIN | 0x000f)
#define MUIV_ScreenShoot        (TAGBASE_DEVELIN | 0x0010)
//#define MUIV_FlushBlockOverlay  (TAGBASE_DEVELIN | 0x0011)
#define MUIV_FlushClearScreen   (TAGBASE_DEVELIN | 0x0012)

#define MUIA_Floppy_Hotkey     (TAGBASE_DEVELIN | 0x0013)

#define MUIV_HKTriggerFloppy0  0 //(TAGBASE_DEVELIN | 0x0014) //202
#define MUIV_HKTriggerFloppy1  1 //(TAGBASE_DEVELIN | 0x0015) // 203
#define MUIV_HKTriggerFloppy2  2 //(TAGBASE_DEVELIN | 0x0016) // 204
#define MUIV_HKTriggerFloppy3  3 //(TAGBASE_DEVELIN | 0x0017) // 205
#define MUIV_HKTriggerEjectAll 4

#define MUIA_Reset_Type        (TAGBASE_DEVELIN | 0x0019)
#define MUIV_Reset_Soft        0
#define MUIV_Reset_Hard        1
#define MUIV_Reset_Custom      2

#define MUIA_Display_Type      (TAGBASE_DEVELIN | 0x001c)
#define MUIV_Display_Window    0
#define MUIV_Display_Screen    1
#define MUIV_Display_Toggle    2

#define MUIA_Toolbar_Active    (TAGBASE_DEVELIN | 0x0020)
#define MUIV_Toolbar_Off       0
#define MUIV_Toolbar_On        1
#define MUIV_Toolbar_Toggle    2

#define MUIA_Control_UAE        (TAGBASE_DEVELIN | 0x0025)
#define MUIV_Control_UAE_Pause  0
#define MUIV_Control_UAE_Resume 1
#define MUIV_Control_UAE_Toggle 2

#define MUIA_Settings_Adjust   (TAGBASE_DEVELIN | 0x0030)
#define MUIV_Reset_All         0
#define MUIV_Reset_General     1
#define MUIV_Reset_OCS         2
#define MUIV_Reset_ECS         3
#define MUIV_Reset_AGA         4
#define MUIV_Reset_Custom      5
#define MUIV_Settings_Use      6
#define MUIV_Settings_SaveUse  7
#define MUIV_Settings_Save     8
#define MUIV_Settings_Cancel   9

// Global variable...
static struct MUI_CustomClass *render_mcc = NULL; // Our Render MCC

#define swapw(x) ( (((x)&0x00FF)<<8)+(((x)&0xFF00)>>8) )

MUI_HOOK(AppMsg,APTR obj, struct AppMessage **x)
{
   struct WBArg *ap;
   struct AppMessage *amsg = *x;
   int i;
   static char buf[256];
   char *b=buf;

   for (ap=amsg->am_ArgList,i=0;i<amsg->am_NumArgs;i++,ap++)
   {
      NameFromLock(ap->wa_Lock,buf,sizeof(buf));
      AddPart(buf,ap->wa_Name,sizeof(buf));
      strcpy (changed_prefs.df[0], b); // Attach image to DF0: as default...
   }

   return(0);
}

MUI_HOOK(StrObjFunc, Object *pop, Object *str)
{
   char *x,*s;
   int i;

   get(str,MUIA_String_Contents,&s);

   for (i=0;;i++)
   {
      DoMethod(pop,MUIM_List_GetEntry,i,&x);
      if (!x)
      {
         set(pop,MUIA_List_Active,MUIV_List_Active_Off);
      break;
      }
      else if (!stricmp(x,s))
      {
         set(pop,MUIA_List_Active,i);
         break;
      }
   }
   return(TRUE);
}

MUI_HOOK(ObjStrFunc, Object *pop, Object *str)
{
   char *x;
   DoMethod(pop,MUIM_List_GetEntry,MUIV_List_GetEntry_Active,&x);
   set(str,MUIA_String_Contents,x);
   return 0;
}

MUI_HOOK(WindowFunc, Object *pop, Object *win)
{
   set(win,MUIA_Window_DefaultObject,pop);
   set(win,MUIA_Window_ID,27);
   return 0;
}

void reset_tab(unsigned int tab)
{
   switch (tab)
   {
      case ID_BUT_GEN_RESET :
      {
         set(but_gen_machine, MUIA_Cycle_Active, 2);   // AGA
         set(but_gen_sound, MUIA_Cycle_Active, 2);     // Normal
         set(but_gen_channels, MUIA_Cycle_Active, 1);  // Stereo
         set(but_gen_frequency, MUIA_Cycle_Active, 2); // 44100Hz
         set(but_gen_joy0, MUIA_Cycle_Active, 0);      // Mouse
         set(but_gen_joy1, MUIA_Cycle_Active, 2);      // Joy1
         set(but_gen_floppy, MUIA_Cycle_Active, 0);    // Normal
         set(but_gen_blitter, MUIA_Cycle_Active, 0);   // Off - Check this!
         set(but_gen_sprite, MUIA_Cycle_Active, 3);    // Full - Check this!
         set(but_gen_resetmode, MUIA_Cycle_Active, 0); // Soft!
      }  break;

      case ID_BUT_OCS_RESET :
      {
         set(ocs_kickstart_str, MUIA_String_Contents, "Kickstarts/Kick1.rom");
         set(ocs_kickstartkey_str, MUIA_String_Contents, "Kickstarts/");
         set(but_ocs_chipmem, MUIA_Cycle_Active, 0);   // 0,5Mb
         set(but_ocs_fastmem, MUIA_Cycle_Active, 0);   // 0 Mb
      }  break;

      case ID_BUT_ECS_RESET :
      {
         set(ecs_kickstart_str, MUIA_String_Contents, "Kickstarts/Kick2.rom");
         set(ecs_kickstartkey_str, MUIA_String_Contents, "Kickstarts/");
         set(but_ecs_mode, MUIA_Cycle_Active, 0);      // ECS Agnus
         set(but_ecs_chipmem, MUIA_Cycle_Active, 1);   // 2 Mb
         set(but_ecs_fastmem, MUIA_Cycle_Active, 0);   // 0 Mb
      }  break;

      case ID_BUT_AGA_RESET :
      {
         set(aga_kickstart_str, MUIA_String_Contents, "Kickstarts/Kick3.rom");
         set(aga_kickstartkey_str, MUIA_String_Contents, "Kickstarts/");
         set(but_aga_fastmem, MUIA_Cycle_Active, 4);   // 8 Mb
      }  break;

      case ID_BUT_CUS_RESET :
      {
         set(cus_kickstart_str, MUIA_String_Contents, "Kickstarts/Kick3.rom");
         set(cus_kickstartkey_str, MUIA_String_Contents, "Kickstarts/");
         set(but_cus_cpu, MUIA_Cycle_Active, 1);      // 68040
         set(but_cus_speed, MUIA_Cycle_Active, 1);    // Max
         set(but_cus_jit, MUIA_Cycle_Active, 0);      // Off
         set(but_cus_chipset, MUIA_Cycle_Active, 2);  // AGA
         set(but_cus_chipmem, MUIA_Cycle_Active, 2);  // 2 Mb
         set(but_cus_fastmem, MUIA_Cycle_Active, 4);  // 8 Mb
         set(but_cus_zorromem, MUIA_Cycle_Active, 5); // 16 Mb
      }  break;
   } 
}


void reset_all(void)
{
   reset_tab(ID_BUT_GEN_RESET);
   reset_tab(ID_BUT_OCS_RESET);
   reset_tab(ID_BUT_ECS_RESET);
   reset_tab(ID_BUT_AGA_RESET);
   reset_tab(ID_BUT_CUS_RESET);
}

void setup_specific(int conf)
{
   STRPTR ks, ksk;
   LONG val;

   if (conf == 0) // OCS
   {
      GetAttr(MUIA_String_Contents, ocs_kickstart_str, (ULONG *)&ks);
      GetAttr(MUIA_String_Contents, ocs_kickstartkey_str, (ULONG *)&ksk);
      changed_prefs.chipset_mask = 0;

      get(but_ocs_chipmem, MUIA_Cycle_Active, &val);
      changed_prefs.chipmem_size = (val == 0 ? 0x80000 : 0x100000);

      get(but_ocs_fastmem, MUIA_Cycle_Active, &val);
      changed_prefs.fastmem_size = (val == 0 ? 0 :
                                    val == 1 ? 0x100000 :
                                    val == 2 ? 0x200000 :
                                    val == 3 ? 0x400000 : 0x800000);

      changed_prefs.cpu_level = 0; // 68000
      changed_prefs.m68k_speed = 0; // Real
   }
   else if (conf == 1) // ECS
   {
      GetAttr(MUIA_String_Contents, ecs_kickstart_str, (ULONG *)&ks);
      GetAttr(MUIA_String_Contents, ecs_kickstartkey_str, (ULONG *)&ksk);
      get(but_ecs_mode, MUIA_Cycle_Active, &val);
      changed_prefs.chipset_mask = (val == 0 ? 1 : val == 1 ? 2 : 3);

      get(but_ecs_chipmem, MUIA_Cycle_Active, &val);
      changed_prefs.chipmem_size = (val == 0 ? 0x100000 : 0x200000);

      get(but_ecs_fastmem, MUIA_Cycle_Active, &val);
      changed_prefs.fastmem_size = (val == 0 ? 0 :
                                    val == 1 ? 0x100000 :
                                    val == 2 ? 0x200000 :
                                    val == 3 ? 0x400000 : 0x800000);

      changed_prefs.cpu_level = 0; // 68000
      changed_prefs.m68k_speed = 0; // Real
   }
   else if (conf == 2) // AGA
   {
      GetAttr(MUIA_String_Contents, aga_kickstart_str, (ULONG *)&ks);
      GetAttr(MUIA_String_Contents, aga_kickstartkey_str, (ULONG *)&ksk);
      changed_prefs.chipset_mask = 7;

      changed_prefs.chipmem_size = 0x200000;

      get(but_aga_fastmem, MUIA_Cycle_Active, &val);
      changed_prefs.fastmem_size = (val == 0 ? 0 :
                                    val == 1 ? 0x100000 :
                                    val == 2 ? 0x200000 :
                                    val == 3 ? 0x400000 : 0x800000);

      changed_prefs.cpu_level = 2; // 68020
      changed_prefs.m68k_speed = 0; // Real
   }
   else // Custom
   {
      GetAttr(MUIA_String_Contents, cus_kickstart_str, (ULONG *)&ks);
      GetAttr(MUIA_String_Contents, cus_kickstartkey_str, (ULONG *)&ksk);
      get(but_cus_chipset, MUIA_Cycle_Active, &val);
      changed_prefs.chipset_mask = (val == 0 ? 0 : val == 1 ? 3 : 7);

      get(but_cus_cpu, MUIA_Cycle_Active, &val);
      changed_prefs.cpu_level = (val == 0 ? 2 :
                                 val == 1 ? 4 : 6);

      get(but_cus_speed, MUIA_Cycle_Active, &val);
      changed_prefs.m68k_speed = (val == 0 ? 0 : -1);

      get(but_cus_jit, MUIA_Cycle_Active, &val);
      if (val > 0)
      {
         changed_prefs.m68k_speed = -1;        // Set the speed to Max, overrides the other settings...
         changed_prefs.cpu_compatible = 0;
         changed_prefs.cpu_cycle_exact = 0;
         changed_prefs.blitter_cycle_exact = 0;
         changed_prefs.cachesize = (val == 1 ? 8192 : 16384);
         changed_prefs.comp_hardflush = 1;
         changed_prefs.comp_constjump = 1;
         changed_prefs.comptrustbyte = 1;
         changed_prefs.comptrustword = 1;
         changed_prefs.comptrustlong = 1;
         changed_prefs.compoptim = 1;
      }
   }

   get(but_cus_chipmem, MUIA_Cycle_Active, &val);
   changed_prefs.chipmem_size = (val == 0 ? 0x80000 :
                                 val == 1 ? 0x100000 : 0x200000);

   get(but_cus_fastmem, MUIA_Cycle_Active, &val);
   changed_prefs.fastmem_size = (val == 0 ? 0 :
                                 val == 1 ? 0x100000 :
                                 val == 2 ? 0x200000 :
                                 val == 3 ? 0x400000 : 0x800000);

   get(but_cus_zorromem, MUIA_Cycle_Active, &val);
   changed_prefs.z3fastmem_size = (val == 0 ? 0 :
                                   val == 1 ? 0x100000 :
                                   val == 2 ? 0x200000 :
                                   val == 3 ? 0x400000 :
                                   val == 4 ? 0x800000 :
                                   val == 5 ? 0x1000000 :
                                   val == 6 ? 0x2000000 :
                                   val == 7 ? 0x4000000 :
                                   val == 8 ? 0x8000000 : 0x10000000);

   strcpy(changed_prefs.romfile, ks);
   strcpy(changed_prefs.keyfile, ksk);
}

void setup_generic(void)
{
   LONG mpos, spos, jpos, val;
   get(but_gen_machine, MUIA_Cycle_Active, &mpos);

   setup_specific(mpos);

   //Sound
   get(but_gen_sound, MUIA_Cycle_Active, &spos);
   changed_prefs.produce_sound = spos;
   get(but_gen_channels, MUIA_Cycle_Active, &spos);
   changed_prefs.sound_stereo = spos;
   get(but_gen_frequency, MUIA_Cycle_Active, &spos);
   changed_prefs.sound_freq = (spos == 0 ? 11025 : spos == 1 ? 22055 : spos == 2 ? 44100 : 48000);

   // IO Devices...
   get(but_gen_joy0, MUIA_Cycle_Active, &jpos);
   changed_prefs.jport0 = (jpos == 0 ? 200 : jpos == 1 ? 100 : jpos == 2 ? 101 : jpos == 3 ? 0 : jpos == 4 ? 1 : 2);
   get(but_gen_joy1, MUIA_Cycle_Active, &jpos);
   changed_prefs.jport1 = (jpos == 0 ? 200 : jpos == 1 ? 100 : jpos == 2 ? 101 : jpos == 3 ? 0 : jpos == 4 ? 1 : 2);
   get(but_gen_floppy, MUIA_Cycle_Active, &jpos);
   changed_prefs.floppy_speed = (jpos == 0 ? 100 : jpos == 1 ? 500 : 1000);

   // Chipset...
   get(but_gen_blitter, MUIA_Cycle_Active, &val);
   changed_prefs.immediate_blits = val;
   get(but_gen_sprite, MUIA_Cycle_Active, &val);
   changed_prefs.collision_level = val;
}


/*=----------------------------- openfile() ----------------------------------*
 *                                                                            *
 *----------------------------------------------------------------------------*/
void insertimagefile(UBYTE unit)
{
   struct Library *AslBase = NULL;
   struct FileRequester *freq;
   char tmpstr[100];
   static char buf[256];
   char *b=buf;
   
   debug_print("%s (%d)\n", __func__, __LINE__);

   if ((AslBase = OpenLibrary("asl.library", 37L)))
   {
         sprintf(tmpstr, "Insert image on DF%d", unit);
         ULONG filetags[] = {ASLFR_TitleText, tmpstr, ASLFR_DoPatterns, TRUE, ASLFR_InitialPattern, "#?(.adf|.dms)",/* ASLFR_InitialDrawer, get_last_floppy_dir(),*/ TAG_DONE}; // "Insert image on DFx"

         if ((freq = (struct FileRequester *) AllocAslRequest(ASL_FileRequest, (struct TagItem*)&filetags)))
         {
            if (AslRequest(freq, NULL))
            {
               if (strcmp(freq->fr_File, "") != 0)
               {
                  strcpy(buf, freq->fr_Drawer);
                  AddPart(buf, freq->fr_File, sizeof(buf));
                  strcpy (changed_prefs.df[unit], b); 
               }
            }
            
            FreeAslRequest(freq);
         }

      CloseLibrary(AslBase);
      AslBase = NULL;
   }
   else
      debug_print("%s (%d)\n", __func__, __LINE__);
}
/*=*/

/*=----------------------------- savepng() -----------------------------------*
 *                                                                            *
 *----------------------------------------------------------------------------*/
BOOL savepng(char *fname, UBYTE *imgdata, WORD width, WORD height, WORD depth)
{
   Object *saver, *output, *memory_stream, *video_filter, *pngencoder;
   QUAD slen = width * height * depth;
   BOOL retval = FALSE;

   if (memory_stream = NewObject(NULL, "memory.stream", MMA_StreamHandle, (IPTR)imgdata, MMA_StreamLength, &slen, TAG_END))
   {
      if (video_filter = NewObject(NULL, "rawvideo.filter", MMA_Video_Width, width, MMA_Video_Height, height, TAG_END))
      {
         MediaSetPort(video_filter, 1, MMA_Port_Format, MMFC_VIDEO_ARGB32);

         if (MediaConnectTagList(memory_stream, 0, video_filter, 0, NULL))
         {
            if (saver = MediaBuildFromArgsTagList("PNG", video_filter, 1, TAG_END))
            {
               if (output = NewObject(NULL, "file.output", MMA_StreamName, fname, TAG_END))
               {
                  if (MediaConnectTagList(saver, 1, output, 0, NULL))
                  {
                     DoMethod(output, MMM_SignalAtEnd, (IPTR)FindTask(NULL), SIGBREAKB_CTRL_C);
                     DoMethod(output, MMM_Play);
                     Wait(SIGBREAKF_CTRL_C);
                     retval = TRUE;
                  }
                  DisposeObject(output);
               }
            }
            DisposeObject(saver);
         }
         DisposeObject(video_filter);
      }
      DisposeObject(memory_stream);
   }

   return retval;
}

/*=*/

// Render MCC

/*=----------------------------- Render_New() --------------------------------*
 *                                                                            *
 *----------------------------------------------------------------------------*/
static ULONG Render_New(struct IClass *cl, Object *obj, struct opSet *msg)
{
   struct RenderData *data;
   //debug_print("%s (%d)\n", __func__, __LINE__);

   obj = DoSuperNew(cl, obj,
                    InnerSpacing(0, 0),
                    MUIA_Frame,        MUIV_Frame_None,
                    MUIA_Background,   MUII_WindowBack,
                    MUIA_FillArea,     FALSE,
                    MUIA_DoubleBuffer, FALSE,
                    TAG_MORE,          msg->ops_AttrList);

   if (!obj)
      return(0);

   data = (struct RenderData *)INST_DATA(cl, obj);

   data->screen = NULL;
   data->ogscreen = NULL;
   data->modeid = 0;
   data->window = NULL;
   data->Active = FALSE;
   data->InitOK = FALSE;
   data->showpointer = TRUE;
   data->FullScreen = FALSE;
   data->ToolBar = TRUE;

   data->BitMap = NULL;
   data->Buffer = NULL;
   data->XOffset = 0;
   data->YOffset = 0;
   data->render_state = MUIV_FlushClearScreen;

   data->WinWidth = 640;
   data->WinHeight = 512;
   data->ScrWidth = 640;
   data->ScrHeight = 512;
   data->Depth = 24;
   data->MouseX = 0;
   data->MouseY = 0;
   data->OldX = 0;
   data->OldY = 0;
   
   return((ULONG)obj);
}
/*=*/

/*=----------------------------- Render_Dispose() ----------------------------*
 *                                                                            *
 *----------------------------------------------------------------------------*/
static ULONG Render_Dispose(struct IClass *cl, Object *obj, Msg msg)
{
   struct RenderData *data = (struct RenderData *)INST_DATA(cl, obj);
   //debug_print("%s (%d)\n", __func__, __LINE__);

   data->Active = FALSE;

   // The below action might need to be moved to the set-dispatcher and called with MUIV_CleanupGraphics
   if (data->Buffer)
   {
      FreeVec(data->Buffer);
      data->Buffer = NULL;
   }

   if (data->BitMap)
   {
      WaitBlit();
      FreeBitMap(data->BitMap);
      data->BitMap = NULL;
   }

   return(DoSuperMethodA(cl, obj, msg));
}
/*=*/

/*=----------------------------- Render_Set() --------------------------------*
 *                                                                            *
 *----------------------------------------------------------------------------*/
static ULONG Render_Set(struct IClass *cl, Object *obj, struct opSet *msg)
{
   struct RenderData *data = (struct RenderData *)INST_DATA(cl, obj);
   struct TagItem *TagList = NULL;
   struct TagItem *tag = NULL;
   struct RastPort *rp = _rp(obj);
   LONG val;
   int redbits,  greenbits,  bluebits;
   int redshift, greenshift, blueshift;
   int byte_swap = FALSE;
   int pixfmt;
   int found = TRUE;
   int dcnt;

   //debug_print("%s (%d)\n", __func__, __LINE__);

   if(msg -> ops_AttrList)
   {
      for(TagList = msg -> ops_AttrList; tag = NextTagItem(&TagList);)
      {
         switch (tag->ti_Tag)
         {
            case MUIA_Render_State :
               data->render_state = tag->ti_Data;
               MUI_Redraw(obj_rendermcc, MADF_DRAWOBJECT);
               break;

            case MUIA_Pointer_State :
               //debug_print("%s (%d) - X1=%d, Y1=%d, X2=%d, Y2=%d\n", __func__, __LINE__, _mleft(obj), _mtop(obj), _mwidth(obj), _mheight(obj));

               if (tag->ti_Data == MUIV_ShowPointer)
               {
                  if (!data->showpointer)
                  {
                     //debug_print("%s (%d)\n", __func__, __LINE__);
                     SetWindowPointer(data->window, WA_PointerType, POINTERTYPE_NORMAL, WM_ObtainEvents, TRUE, TAG_DONE);
                     data->showpointer = TRUE;
                  }
               }
               else if (tag->ti_Data == MUIV_HidePointer)
               {
                  if (data->showpointer)
                  {
                     //debug_print("%s (%d)\n", __func__, __LINE__);
                     SetWindowPointer(data->window, WA_PointerType, POINTERTYPE_DOT, WM_ObtainEvents, TRUE, TAG_DONE);
                     data->showpointer = FALSE;
                  }
               }  break;

            case MUIA_Floppy_Hotkey :
               if (tag->ti_Data == MUIV_HKTriggerEjectAll)
                  for (dcnt=0; dcnt < MUIV_HKTriggerEjectAll; dcnt++)
                     strcpy(changed_prefs.df[dcnt], "");
               else 
                  insertimagefile(tag->ti_Data); 
               break;

            case MUIA_Reset_Type :
               //debug_print("%s (%d)\n", __func__, __LINE__);
               if (tag->ti_Data == MUIV_Reset_Custom)
               {
                  get(but_gen_sprite, MUIA_Cycle_Active, &val);
                  uae_reset(val);
               }
               else
                  uae_reset (tag->ti_Data);
               break;

            case MUIA_Display_Type :
               set(win_main, MUIA_Window_Open, FALSE);
               if (data->FullScreen)
               {
                  CloseScreen(data->screen);
                  data->screen = data->ogscreen;

                  data->FullScreen = FALSE;
                  //set(obj_rendermcc, MUIA_Width, data->WinWidth);
                  //set(obj_rendermcc, MUIA_Height, data-> WinHeight);

                  set(obj_rendermcc, MUIA_Toolbar_Active, MUIV_Toolbar_On);
                  SetAttrs(win_main,
                           MUIA_Window_Screen,      data->ogscreen,
                           MUIA_Window_Borderless,  FALSE,
                           MUIA_Window_DragBar,     TRUE,
                           MUIA_Window_CloseGadget, TRUE,
                           MUIA_Window_DepthGadget, TRUE,
                           MUIA_Window_SizeGadget,  FALSE,
                           MUIA_Window_Frontdrop,   FALSE,
                           MUIA_Window_Title,       "MorphUAE",
                           //MUIA_Window_Width,       data->WinWidth,
                           //MUIA_Window_Height,      data-> WinHeight,
                           TAG_DONE);
               }
               else
               {
                  data->modeid = BestCModeIDTags(CYBRBIDTG_NominalWidth,  data->WinWidth, CYBRBIDTG_NominalHeight, data->WinHeight, CYBRBIDTG_Depth, data->Depth, TAG_DONE);
                  //debug_print("%s (%d) - ModeID : %d\n", __func__, __LINE__, data->modeid);
                  if (data->modeid != INVALID_ID)
                  {
                     struct Screen *tmpscreen;

                     data->ogscreen = data->screen; // Store the orginal ID

                     tmpscreen = OpenScreenTags(NULL,
                                                SA_Title,     "MorphUAE Screen",
                     //                           SA_ShowTitle, FALSE,
                     //                           SA_Type,      CUSTOMSCREEN,
                                                SA_LikeWorkbench, TRUE,
                     //                           SA_DisplayID, data->modeid,
                     //                           SA_Width,     data->WinWidth,
                     //                           SA_Height,    data->WinHeight,
                     //                           SA_Depth,     data->Depth,
                                                SA_Quiet,     TRUE,
                                                TAG_DONE);
                     if (tmpscreen)
                     {
                        data->screen = tmpscreen;
                        data->FullScreen = TRUE;
                        set(obj_rendermcc, MUIA_Toolbar_Active, MUIV_Toolbar_Off);
                        SetAttrs(win_main,
                                 MUIA_Window_Screen,      data->screen,
                                 MUIA_Window_Borderless,  TRUE,
                                 MUIA_Window_DragBar,     FALSE,
                                 MUIA_Window_CloseGadget, FALSE,
                                 MUIA_Window_DepthGadget, FALSE,
                                 MUIA_Window_SizeGadget,  FALSE,
                                 MUIA_Window_Frontdrop,   TRUE,
                                 MUIA_Window_Title,       NULL,
                                 TAG_DONE);
                     }
                  }
                  //set(obj_rendermcc, MUIA_Render_State, MUIV_FlushClearScreen);
               }
               set(win_main, MUIA_Window_Open, TRUE);
               break;

            case MUIA_Toolbar_Active :
               if (tag->ti_Data == MUIV_Toolbar_On)
               {
                  data->ToolBar = TRUE;
                  set(grp_toolbar, MUIA_ShowMe, TRUE);
               }
               else if (tag->ti_Data == MUIV_Toolbar_Off)
               {
                  data->ToolBar = FALSE;
                  set(grp_toolbar, MUIA_ShowMe, FALSE);
               }
               else // MUIV_Toolbar_Toggle
               {
                  if (!data->FullScreen)
                     data->ToolBar = !data->ToolBar;
                  else
                     data->ToolBar = FALSE;

                  set(grp_toolbar, MUIA_ShowMe, data->ToolBar);
               }

            case MUIA_Initializing_Gfx :
               if (tag->ti_Data == MUIV_InitGraphics)
               {
                  int bytes_per_row;
                  int bytes_per_pixel;
                  APTR buffer;

                  data->InitOK = TRUE;

                  gfxvidinfo.width  = data->WinWidth;  //currprefs.gfx_width_win;
                  gfxvidinfo.height = data->WinHeight; //currprefs.gfx_height_win;

                  if (gfxvidinfo.width < 320)
                     gfxvidinfo.width = 320;

                  if (!currprefs.gfx_correct_aspect && (gfxvidinfo.width < 64))
                     gfxvidinfo.width = 200;

                  gfxvidinfo.width += 7;
                  gfxvidinfo.width &= ~7;

                  data->BitMap = AllocBitMap (gfxvidinfo.width, 1, 8, BMF_CLEAR | BMF_MINPLANES, rp->BitMap);

                  if (!data->BitMap)
                  {
                     write_log ("Unable to allocate BitMap.\n");
                     data->InitOK = FALSE;
                  }

                  bytes_per_row   = GetCyberMapAttr (rp->BitMap, CYBRMATTR_XMOD);
                  bytes_per_pixel = GetCyberMapAttr (rp->BitMap, CYBRMATTR_BPPIX);

                  buffer = AllocVec (bytes_per_row * tmp_gfxinfo->height, MEMF_ANY);

                  if (buffer)
                  {
                     tmp_gfxinfo->bufmem      = buffer;
                     tmp_gfxinfo->pixbytes    = bytes_per_pixel;
                     tmp_gfxinfo->rowbytes    = bytes_per_row;
                     tmp_gfxinfo->flush_line  = flush_line_cgx;
                     tmp_gfxinfo->flush_block = flush_block_cgx;
                  }

                  data->Buffer = buffer;

                  if (!data->Buffer)
                  {
                     write_log ("Unable to allocate off-screen buffer.\n");
                     data->InitOK = FALSE;
                  }

                  gfxvidinfo.flush_clear_screen = flush_clear_screen_gfxlib;
                  gfxvidinfo.flush_screen       = dummy_flush_screen;
                  gfxvidinfo.lockscr            = dummy_lock;
                  gfxvidinfo.unlockscr          = dummy_unlock;


                  if (!gfxvidinfo.bufmem)
                  {
                     write_log ("MUIGFX: Not enough memory for video bufmem.\n");
                     data->InitOK = FALSE;
                  }

                  gfxvidinfo.maxblocklines = MAXBLOCKLINES_MAX;

                  if (!init_colors ())
                  {
                     write_log ("MUIGFX: Failed to init colors.\n");
                     data->InitOK = FALSE;
                  }

                  if (data->InitOK)
                  {
                     reset_drawing ();
                     //set_default_hotkeys (ami_hotkeys);
                  }

               }
               else if (tag->ti_Data == MUIV_InitColours)
               {

                  pixfmt = GetCyberMapAttr (_rp(obj)->BitMap, (LONG)CYBRMATTR_PIXFMT);
                  data->Depth = GetCyberMapAttr (_rp(obj)->BitMap, (LONG)CYBRMATTR_DEPTH);

                  switch (pixfmt)
                  {
                     case PIXFMT_RGB15PC:
                        //debug_print("%s (%d)\n", __func__, __LINE__);
                        byte_swap = TRUE;
                     case PIXFMT_RGB15:
                        //debug_print("%s (%d)\n", __func__, __LINE__);
                        redbits  = 5;  greenbits  = 5; bluebits  = 5;
                        redshift = 10; greenshift = 5; blueshift = 0;
                        break;
                     case PIXFMT_RGB16PC:
                        //debug_print("%s (%d)\n", __func__, __LINE__);
                        byte_swap = TRUE;
                     case PIXFMT_RGB16:
                        //debug_print("%s (%d)\n", __func__, __LINE__);
                        redbits  = 5;  greenbits  = 6;  bluebits  = 5;
                        redshift = 11; greenshift = 5;  blueshift = 0;
                        break;
                     case PIXFMT_RGBA32:
                        //debug_print("%s (%d)\n", __func__, __LINE__);
                        redbits  = 8;  greenbits  = 8;  bluebits  = 8;
                        redshift = 24; greenshift = 16; blueshift = 8;
                        break;
                     case PIXFMT_BGRA32: // //RGBA
                        //debug_print("%s (%d) - %d bpp\n", __func__, __LINE__, data->Depth);
                        redbits  = 8;  greenbits  = 8;  bluebits  = 8;
                        //redshift = 8;  greenshift = 16; blueshift = 24;
                        redshift = 16;  greenshift = 8; blueshift = 0;
                        break;
                     case PIXFMT_ARGB32:
                        //debug_print("%s (%d)\n", __func__, __LINE__);
                        redbits  = 8;  greenbits  = 8;  bluebits  = 8;
                        redshift = 16; greenshift = 8;  blueshift = 0;
                        break;
                     default:
                        //debug_print("%s (%d)\n", __func__, __LINE__);
                        redbits  = 0;  greenbits  = 0;  bluebits  = 0;
                        redshift = 0;  greenshift = 0;  blueshift = 0;
                        found = FALSE;
                        break;
                  }

                  if (found)
                  {
                     alloc_colors64k (redbits, greenbits, bluebits, redshift, greenshift, blueshift, 8, 24, 0xff, 0);
                        write_log ("MUIGFX: Using a %d-bit true-colour display.\n", redbits + greenbits + bluebits);
                  }
                  else
                     write_log ("MUIGFX: Unsupported pixel format.\n");
               }  break;

            case MUIA_Cleanup_Gfx :
               if (tag->ti_Data == MUIV_CleanupGraphics)
               {
                  closepseudodevices ();
               }  break;
            case MUIA_Settings_Adjust :
               if (tag->ti_Data == MUIV_Reset_General)
                  reset_tab(ID_BUT_GEN_RESET);
               else if (tag->ti_Data == MUIV_Reset_OCS)
                  reset_tab(ID_BUT_OCS_RESET);
               else if (tag->ti_Data == MUIV_Reset_ECS)
                  reset_tab(ID_BUT_ECS_RESET);
               else if (tag->ti_Data == MUIV_Reset_AGA)
                  reset_tab(ID_BUT_AGA_RESET);
               else if (tag->ti_Data == MUIV_Reset_Custom)
                  reset_tab(ID_BUT_CUS_RESET);
               else if (tag->ti_Data == MUIV_Reset_All)
                  reset_all();
               else if (tag->ti_Data == MUIV_Settings_Use)
               {
                  DoMethod(app, MUIM_Application_Save, MUIV_Application_Save_ENV);
                  setup_generic();
                  uae_restarted = TRUE;
                  set(win_settings, MUIA_Window_Open, FALSE);
                  uae_restart (-1, NULL);
               }
               else if (tag->ti_Data == MUIV_Settings_SaveUse)
               {
                  DoMethod(app, MUIM_Application_Save, MUIV_Application_Save_ENV);
                  DoMethod(app, MUIM_Application_Save, MUIV_Application_Save_ENVARC);
                  setup_generic();
                  uae_restarted = TRUE;
                  set(win_settings, MUIA_Window_Open, FALSE);
                  uae_restart (-1, NULL);
               }
               else if (tag->ti_Data == MUIV_Settings_Save)
               {
                  DoMethod(app, MUIM_Application_Save, MUIV_Application_Save_ENV);
                  DoMethod(app, MUIM_Application_Save, MUIV_Application_Save_ENVARC);
                  set(win_settings, MUIA_Window_Open, FALSE);
               }
               else // MUIV_Settings_Cancel
                  set(win_settings, MUIA_Window_Open, FALSE);
               break;

            case MUIA_Control_UAE :
               if (tag->ti_Data == MUIV_Control_UAE_Toggle)
               {
                  if (uae_get_state() == UAE_STATE_PAUSED)
                  {
                     uae_resume();
                     set(win_main, MUIA_Window_Title, "MorphUAE");
                  }
                  else
                  {
                     uae_pause();
                     set(win_main, MUIA_Window_Title, "MorphUAE - Paused");
                  }
               }
               else if (tag->ti_Data == MUIV_Control_UAE_Pause)
               {
                  if (data->Active)
                     uae_pause();
                  data->Active = FALSE;
                  MUI_Request(NULL, NULL, 0L, "Error Message", "Ok", "MorphUAE needs a valid ROM file to be able to work as intended\nIf you have a ROM from Cloantos AmigaForever you might need to add the rom.key!\n\nPlease adjust the settings accordingly!");
               }
               else // MUIV_Control_UAE_Resume
               {
                  if (data->Active == FALSE)
                     uae_resume();
                  data->Active = TRUE;
               }
               break;
         }
      }
   }
   return(DoSuperMethodA(cl, obj, (Msg) msg));
}
/*=*/

/*=----------------------------- Render_Setup() ------------------------------*
 *                                                                            *
 *----------------------------------------------------------------------------*/
static ULONG Render_Setup(struct IClass *cl, Object *obj, Msg msg)
{
   struct RenderData *data = (struct RenderData *)INST_DATA(cl, obj);
   //debug_print("%s (%d)\n", __func__, __LINE__);
   if (!DoSuperMethodA(cl, obj, msg))
      return(FALSE);

   data->screen = _screen(obj);
   data->window = _window(obj);

   // IDCMP_DELTAMOVE

   data->eh.ehn_Object = obj;
   data->eh.ehn_Class  = cl;
   //data->eh.ehn_Events = IDCMP_MOUSEBUTTONS|IDCMP_RAWKEY|IDCMP_ACTIVEWINDOW|IDCMP_INACTIVEWINDOW|IDCMP_MOUSEMOVE|IDCMP_DELTAMOVE|IDCMP_CLOSEWINDOW|IDCMP_REFRESHWINDOW|IDCMP_NEWSIZE;//|IDCMP_INTUITICKS;
   data->eh.ehn_Events = IDCMP_MOUSEBUTTONS|IDCMP_RAWKEY|IDCMP_ACTIVEWINDOW|IDCMP_INACTIVEWINDOW|IDCMP_MOUSEMOVE|IDCMP_CLOSEWINDOW|IDCMP_REFRESHWINDOW;//|IDCMP_INTUITICKS;
   data->eh.ehn_Flags  = MUI_EHF_GUIMODE; // Check this... React if the object is active or not...

   DoMethod(_win(obj), MUIM_Window_AddEventHandler, &data->eh);
   return(TRUE);
}
/*=*/

/*=----------------------------- Render_Cleanup() ----------------------------*
 *                                                                            *
 *----------------------------------------------------------------------------*/
static ULONG Render_Cleanup(struct IClass *cl, Object *obj, Msg msg)
{
   struct RenderData *data = (struct RenderData *)INST_DATA(cl, obj);
   debug_print("%s (%d)\n", __func__, __LINE__);

   DoMethod(_win(obj), MUIM_Window_RemEventHandler, &data->eh);

   //if (data->FullScreen)
   //   CloseScreen(data->screen);

   return(DoSuperMethodA(cl, obj, (Msg)msg));
}
/*=*/

static ULONG Render_Askminmax(struct IClass *cl, Object *obj, struct MUIP_AskMinMax *msg)
{
   struct RenderData *data = (struct RenderData *)INST_DATA(cl, obj);
   
   DoSuperMethodA(cl, obj, (Msg)msg);

   if ((data->screen = _screen(obj)) != NULL)
   {
      //debug_print("%s (%d) : FS = %d - %d x %d : %d x %d\n", __func__, __LINE__, data->FullScreen, data->ScrWidth, data->ScrHeight, currprefs.gfx_width_win, currprefs.gfx_height_win);
      msg->MinMaxInfo->MinWidth  += DEFAULT_GFX_WIDTH;
      msg->MinMaxInfo->DefWidth  += DEFAULT_GFX_WIDTH;
      msg->MinMaxInfo->MinHeight += DEFAULT_GFX_HEIGHT;
      msg->MinMaxInfo->DefHeight += DEFAULT_GFX_HEIGHT;
      msg->MinMaxInfo->MaxWidth  += (data->FullScreen) ? data->ScrWidth : DEFAULT_GFX_WIDTH;
      msg->MinMaxInfo->MaxHeight += (data->FullScreen) ? data->ScrHeight : DEFAULT_GFX_HEIGHT;
   }

   debug_print("%s (%d) : %d x %d\n", __func__, __LINE__, currprefs.gfx_width_win, currprefs.gfx_height_win);
   debug_print("%s (%d) : %d x %d\n", __func__, __LINE__, data->ScrWidth, data->ScrHeight);
   
   return(0);
}


/*=----------------------------- Render_Draw() -------------------------------*
 *                                                                            *
 *----------------------------------------------------------------------------*/
static ULONG Render_Draw(struct IClass *cl, Object *obj, struct MUIP_Draw *msg)
{
   struct RenderData *data = (struct RenderData *)INST_DATA(cl, obj);
   struct RastPort *rp = _rp(obj);
   //debug_print("%s (%d)\n", __func__, __LINE__);

   DoSuperMethodA(cl, obj, (Msg)msg);

   if (data->Active)
   {
      if ((msg->flags & MADF_DRAWOBJECT)) // Triggered draw update from function
      {
         if (data->render_state == MUIV_FlushClearScreen)
         {
            if (_rp(obj))
               WritePixelArray(gfx_logo, 0, 0, _width(obj)*4, _rp(obj), _left(obj), _top(obj), _width(obj), _mbottom(obj)-_mtop(obj)+1, RECTFMT_ARGB);
               //FillPixelArray (_rp(obj), _left(obj), _top(obj), _width(obj), _mbottom(obj)-_mtop(obj), 0x00ffff00); //render_bottom-render_top, 0x00000000); //0);
         }
         else if (data->render_state == MUIV_FlushLineCGX)
         {
            //if (!data->FullScreen)
               WritePixelArray(data->Buffer, 0, tmp_line_no, tmp_gfxinfo->rowbytes, _rp(obj), data->XOffset, data->YOffset + tmp_line_no, tmp_gfxinfo->width, 1, RECTFMT_RAW);
//            else
//               ScalePixelArray(data->Buffer, data->WinWidth, data->WinHeight, tmp_gfxinfo->rowbytes, _rp(obj), data->XOffset, data->YOffset + tmp_line_no, data->WinWidth, data->WinHeight, RECTFMT_RGBA);
         }
         else if (data->render_state == MUIV_FlushBlockCGX)
         {
            //debug_print("%s (%d) - BLOCK - FL = %d ; RB = %d\n", __func__, __LINE__, tmp_first_line, tmp_gfxinfo->rowbytes);
            if (!data->FullScreen)
               WritePixelArray(data->Buffer, 0, tmp_first_line, tmp_gfxinfo->rowbytes, _rp(obj), data->XOffset, data->YOffset + tmp_first_line, tmp_gfxinfo->width, tmp_last_line - tmp_first_line + 1, RECTFMT_ARGB); //RECTFMT_RAW);

               //ScalePixelArrayAlpha(data->Buffer, data->WinWidth, data->WinHeight, tmp_gfxinfo->rowbytes, _rp(obj), 1, data->YOffset + tmp_first_line, tmp_gfxinfo->width, tmp_last_line - tmp_first_line + 1, 0xffffffff); //RECTFMT_ARGB); //0xFFFFFFFF);
            else
               ScalePixelArrayAlpha(data->Buffer, data->WinWidth, data->WinHeight, tmp_gfxinfo->rowbytes, _rp(obj), 0, data->YOffset + tmp_first_line, tmp_gfxinfo->width, tmp_last_line - tmp_first_line + 1, 0xffffffff);//RECTFMT_ARGB); //0xFFFFFFFF);
         }
         else if (data->render_state == MUIV_ScreenShoot)
         {
            ULONG lock;
            UBYTE *tmpdata = NULL;

            tmpdata = AllocVec(_width(obj)*_height(obj)*4, MEMF_ANY);
            if (tmpdata)
            {
               lock = LockIBase(0);
               ReadPixelArray(tmpdata, 0, 0, _width(obj)*4, _rp(obj), _left(obj), _top(obj), _width(obj), _height(obj), RECTFMT_ARGB);
               savepng("RAM:MorphUAE-Screenshoot.png", tmpdata, _width(obj), _height(obj), 4);
               UnlockIBase (lock);
               FreeVec(tmpdata);
            }
         }
         else
            //FillPixelArray(rp, render_left, render_top, render_right, render_bottom-render_top, 0x00000000);
            return(0);
      }
   }
   return(0);
}
/*=*/

/*=----------------------------- Render_Hide ---------------------------------*
 *                                                                            *
 *----------------------------------------------------------------------------*/
static ULONG Render_Hide(struct IClass *cl, Object *obj, Msg msg)
{
   struct RenderData *data = (struct RenderData *)INST_DATA(cl, obj);
   //debug_print("%s (%d)\n", __func__, __LINE__);
   data->Active = FALSE;

   return(DoSuperMethodA(cl, obj, (Msg)msg));
}
/*=*/

/*=----------------------------- Render_Show ---------------------------------*
 *                                                                            *
 *----------------------------------------------------------------------------*/
static ULONG Render_Show(struct IClass *cl, Object *obj, Msg msg)
{
   struct RenderData *data = (struct RenderData *)INST_DATA(cl, obj);
   //debug_print("%s (%d)\n", __func__, __LINE__);

   data->screen = _screen(obj);
   data->window = (struct Window *)_window(obj);
   data->Active = TRUE;
   data->XOffset = _mleft(obj);
   data->YOffset = _mtop(obj);

   reset_drawing (); // Test

   return(DoSuperMethodA(cl, obj, (Msg)msg));
}
/*=*/

/*=----------------------------- Render_EventHandler() -----------------------*
 *                                                                            *
 *----------------------------------------------------------------------------*/
static ULONG Render_EventHandler(struct IClass *cl, Object *obj, struct MUIP_HandleEvent *msg)
{
   int dmx, dmy, mx, my, classi, code, qualifier;
   struct RenderData *data = (struct RenderData *)INST_DATA(cl, obj);
   #define _between(a,x,b) ((x)>=(a) && (x)<=(b))
   #define _isinobject(x,y) (_between(_mleft(obj),(x),_mright(obj)) && _between(_mtop(obj),(y),_mbottom(obj)))

   //debug_print("%s (%d)\n", __func__, __LINE__); // IntuiMessage

   if (msg->imsg)
   {
      code = msg->imsg->Code;
      qualifier = msg->imsg->Qualifier;
      dmx       = msg->imsg->MouseX;
      dmy       = msg->imsg->MouseY;

      switch(msg->imsg->Class)
      {
//         case IDCMP_NEWSIZE:
//            do_inhibit_frame ((_window(obj)->Flags & WFLG_ZOOMED) ? 1 : 0);
//            break;

//         case IDCMP_REFRESHWINDOW:
//            BeginRefresh(_win(obj));      // Do we really need this one ?
//            flush_block (0, currprefs.gfx_height_win - 1);
//            EndRefresh (_win(obj), TRUE); // Do we really need this one ?
//            break;

         case IDCMP_RAWKEY:
         {
            int keycode = code & 127;
            int state   = code & 128 ? 0 : 1;

            if ((qualifier & IEQUALIFIER_REPEAT) == 0)
               inputdevice_do_keyboard (keycode, state);

         }  break;

         case IDCMP_MOUSEMOVE:
            data->MouseX = msg->imsg->IDCMPWindow->MouseX; //msg->Window->IDCMPWindow->MouseX;
            data->MouseY = msg->imsg->IDCMPWindow->MouseY; //IntuiMessage
         
            //debug_print("%s (%d) - NEW XPOS : %d  YPOS : YPOS : %d\n", __func__, __LINE__, data->MouseX, data->MouseY);
            //debug_print("%s (%d) - OLD XPOS : %d  YPOS : YPOS : %d\n", __func__, __LINE__, data->OldX, data->OldY);
            if (_isinobject(data->MouseX, data->MouseY))
            {
               if(data->showpointer)
               {
                  set(obj_rendermcc, MUIA_Pointer_State, MUIV_HidePointer);
                  //data->MouseX = data->OldX;
                  //data->MouseY = data->OldY;
                  setmousestate (0, 0, data->MouseX, 1); //dmx
                  setmousestate (0, 1, data->MouseY, 1); //dmy
               }
               setmousestate (0, 0, data->MouseX, 1);
               setmousestate (0, 1, data->MouseY, 1);
            }
            else
            {
               if(!data->showpointer)
               {
                  set(obj_rendermcc, MUIA_Pointer_State, MUIV_ShowPointer);
                  data->OldX = data->MouseX;
                  data->OldY = data->MouseY;
                  setmousestate (0, 0, data->MouseX, 1);
                  setmousestate (0, 1, data->MouseY, 1);
                  //debug_print("%s (%d) -OLD -  XPOS : %d  YPOS : YPOS : %d\n", __func__, __LINE__, data->OldX, data->OldY);
               }
            }
            break;

         case IDCMP_MOUSEBUTTONS:
         {
            data->MouseX = msg->imsg->IDCMPWindow->MouseX;
            data->MouseY = msg->imsg->IDCMPWindow->MouseY;

            if (_isinobject(data->MouseX, data->MouseY))
            {
               switch (code) //(msg->imsg->Code)
               {
                  case SELECTDOWN :
                     setmousebuttonstate (0, 0, 1); break;
                  case SELECTUP :
                     setmousebuttonstate (0, 0, 0); break;
                  case MIDDLEDOWN :
                     setmousebuttonstate (0, 2, 1); break;
                  case MIDDLEUP :
                     setmousebuttonstate (0, 2, 0); break;
                  case MENUDOWN :
                     setmousebuttonstate (0, 1, 1); break;
                  case MENUUP :
                     setmousebuttonstate (0, 1, 0); break;
               }
            }  
         }  break;

         case IDCMP_ACTIVEWINDOW:
            inputdevice_acquire ();
            inputdevice_release_all_keys ();
            break;

         case IDCMP_INACTIVEWINDOW:
            inputdevice_unacquire ();
            break;

         default :
            //debug_print("%s (%d) - Unknown event class: %x\n", __func__, __LINE__, msg->imsg->Class);
            break;
      }
   }
   return 0;
}
/*=*/

DISPATCHER(Render)
{
   switch (msg->MethodID)
   {
      case OM_NEW           : return Render_New          (cl, obj, (APTR)msg); break;
      case OM_DISPOSE       : return Render_Dispose      (cl, obj, (APTR)msg); break;
      case OM_SET           : return Render_Set          (cl, obj, (APTR)msg); break;
      case MUIM_Setup       : return Render_Setup        (cl, obj, (APTR)msg); break;
      case MUIM_Cleanup     : return Render_Cleanup      (cl, obj, (APTR)msg); break;
      case MUIM_AskMinMax   : return Render_Askminmax    (cl, obj, (APTR)msg); break;
      case MUIM_Draw        : return Render_Draw         (cl, obj, (APTR)msg); break;
      case MUIM_Show        : return Render_Show         (cl, obj, (APTR)msg); break;
      case MUIM_Hide        : return Render_Hide         (cl, obj, (APTR)msg); break;
      case MUIM_HandleEvent : return Render_EventHandler (cl, obj, (APTR)msg); break;
   }

   return DoSuperMethodA(cl, obj, msg);
}
DISPATCHER_END

void Cleanup_Render(struct MUI_CustomClass *mcc)
{
   debug_print("%s (%d)\n", __func__, __LINE__);
   if (mcc) MUI_DeleteCustomClass(mcc);
   mcc = NULL;
}

struct MUI_CustomClass *Init_Render(void)
{
   struct MUI_CustomClass *mcc = NULL;
   debug_print("%s (%d)\n", __func__, __LINE__);
   mcc = MUI_CreateCustomClass(NULL, MUIC_Area, NULL, sizeof(struct RenderData), DISPATCHER_REF(Render));
   return(mcc);
}

void update_led_status(int led, int on)
{
   set(obj_LEDmcc[led-1], MUIA_LED_Colour, (on) ? MUIV_LED_Colour_Green :  MUIV_LED_Colour_Off);
}

//END MUI-Test

/****************************************************************************/

extern void initpseudodevices(void);
extern void closepseudodevices(void);
extern int ievent_alive;


/*
 * Dummy buffer locking methods
 */
static int dummy_lock (struct vidbuf_description *gfxinfo)
{
   return 1;
}

static void dummy_unlock (struct vidbuf_description *gfxinfo)
{
}

static void dummy_flush_screen (struct vidbuf_description *gfxinfo, int first_line, int last_line)
{
}

static void flush_line_cgx (struct vidbuf_description *gfxinfo, int line_no)
{
   tmp_gfxinfo = gfxinfo;
   tmp_line_no = line_no;

   set(obj_rendermcc, MUIA_Render_State, MUIV_FlushLineCGX);
}

static void flush_block_cgx (struct vidbuf_description *gfxinfo, int first_line, int last_line)
{
   tmp_gfxinfo = gfxinfo;
   tmp_first_line = first_line;
   tmp_last_line = last_line;

   set(obj_rendermcc, MUIA_Render_State, MUIV_FlushBlockCGX);
}

static void flush_clear_screen_gfxlib (struct vidbuf_description *gfxinfo)
{
   tmp_gfxinfo = gfxinfo;

   set(obj_rendermcc, MUIA_Render_State, MUIV_FlushClearScreen);
}


/****************************************************************************/

static int init_colors (void)
{
   int success = TRUE;  // This really doesn't do anything... TODO...
   //debug_print("%s (%d)\n", __func__, __LINE__);
   set(obj_rendermcc, MUIA_Initializing_Gfx, MUIV_InitColours);
   return success;
}

/****************************************************************************/

// BEGIN - MUI-Test

static int mui_setup_window(void)
{
   debug_print("%s (%d)\n", __func__, __LINE__);

   app = ApplicationObject,MUIA_Application_Title          , "MorphUAE",
                           MUIA_Application_Version        , "1.0",
                           MUIA_Application_Copyright      , "OnyxSoft",
                           MUIA_Application_Author         , "Stefan Blixth",
                           MUIA_Application_Description    , "MorphUAE",
                           MUIA_Application_Base           , "MorphUAE",
                           MUIA_Application_DiskObject     , morphuae_icon,
                           MUIA_Application_UseCommodities , FALSE,

                           SubWindow, win_main = WindowObject,
                              MUIA_Frame,                 MUIV_Frame_None,
                              MUIA_Window_Borderless,     FALSE,
                              MUIA_Window_CloseGadget,    TRUE,
                              MUIA_Window_DepthGadget,    TRUE,
                              MUIA_Window_SizeGadget,     FALSE,
                              MUIA_Window_DragBar,        TRUE,
                              MUIA_Window_Title,          "MorphUAE",
                              MUIA_Window_ID,             MAKE_ID('M','U','A','E'),
                              MUIA_Window_AppWindow,      TRUE,
                              MUIA_Window_DisableKeys,    MUIKEYF_WINDOW_CLOSE,
                              //MUIA_Application_DiskObject,  dobj = GetDiskObject("uae"), 

                              WindowContents, VGroup,

                                 Child, obj_rendermcc = NewObject(render_mcc->mcc_Class, NULL, NoFrame, MUIA_InnerLeft, 0, MUIA_InnerRight, 0, MUIA_InnerTop, 0, MUIA_InnerBottom, 0, MUIA_Background, MUII_WindowBack, TAG_DONE),
                                 Child, grp_toolbar = HGroup, NoFrame,
                                    MUIA_InnerTop, 1,
                                    MUIA_InnerLeft, 2,
                                    MUIA_InnerRight, 2,
                                    MUIA_InnerBottom, 2,
                                    Child, btn_settings = RawimageObject,
                                       MUIA_DoubleBuffer, 0,
                                       MUIA_InnerLeft, 0, MUIA_InnerRight, 0, MUIA_InnerTop, 0, MUIA_InnerBottom, 0,
                                       MUIA_Frame, MUIV_Frame_None, //MUIV_Frame_Button,
                                       MUIA_InputMode, MUIV_InputMode_RelVerify,
                                       MUIA_Rawimage_Data, gfx_tools,
                                    End,

                                    Child, btn_reset = RawimageObject,
                                       MUIA_DoubleBuffer, 0,
                                       MUIA_InnerLeft, 0, MUIA_InnerRight, 0, MUIA_InnerTop, 0, MUIA_InnerBottom, 0,
                                       MUIA_Frame, MUIV_Frame_None, //MUIV_Frame_Button,
                                       MUIA_InputMode, MUIV_InputMode_RelVerify,
                                       MUIA_Rawimage_Data, gfx_clockwise,
                                    End,

                                    Child, btn_fullscreen = RawimageObject,
                                       MUIA_DoubleBuffer, 0,
                                       MUIA_InnerLeft, 0, MUIA_InnerRight, 0, MUIA_InnerTop, 0, MUIA_InnerBottom, 0,
                                       MUIA_Frame, MUIV_Frame_None, //MUIV_Frame_Button,
                                       MUIA_InputMode, MUIV_InputMode_RelVerify,
                                       MUIA_Rawimage_Data, gfx_fullscreen,
                                    End,

                                    Child, btn_camera = RawimageObject,
                                       MUIA_DoubleBuffer, 0,
                                       MUIA_InnerLeft, 0, MUIA_InnerRight, 0, MUIA_InnerTop, 0, MUIA_InnerBottom, 0,
                                       MUIA_Frame, MUIV_Frame_None, //MUIV_Frame_Button,
                                       MUIA_InputMode, MUIV_InputMode_RelVerify,
                                       MUIA_Rawimage_Data, gfx_camera,
                                    End,

                                    Child, btn_pauseresume = RawimageObject,
                                       MUIA_DoubleBuffer, 0,
                                       MUIA_InnerLeft, 0, MUIA_InnerRight, 0, MUIA_InnerTop, 0, MUIA_InnerBottom, 0,
                                       MUIA_Frame, MUIV_Frame_None, //MUIV_Frame_Button,
                                       MUIA_InputMode, MUIV_InputMode_RelVerify,
                                       MUIA_Rawimage_Data, gfx_resume,
                                    End,

                                    Child, HVSpace,
                                    Child, btn_eject = RawimageObject,
                                       MUIA_DoubleBuffer, 0,
                                       MUIA_InnerLeft, 0, MUIA_InnerRight, 0, MUIA_InnerTop, 0, MUIA_InnerBottom, 0,
                                       MUIA_Frame, MUIV_Frame_None, //MUIV_Frame_Button,
                                       MUIA_InputMode, MUIV_InputMode_RelVerify,
                                       MUIA_Rawimage_Data, gfx_eject,
                                    End,
                                    Child, obj_LEDmcc[0] = NewObject(LED_mcc[0]->mcc_Class, NULL, NoFrame, MUIA_InnerLeft, 0, MUIA_InnerRight, 0, MUIA_InnerTop, 0, MUIA_InnerBottom, 0, TAG_DONE),
                                    Child, obj_LEDmcc[1] = NewObject(LED_mcc[1]->mcc_Class, NULL, NoFrame, MUIA_InnerLeft, 0, MUIA_InnerRight, 0, MUIA_InnerTop, 0, MUIA_InnerBottom, 0, TAG_DONE),
                                    Child, obj_LEDmcc[2] = NewObject(LED_mcc[2]->mcc_Class, NULL, NoFrame, MUIA_InnerLeft, 0, MUIA_InnerRight, 0, MUIA_InnerTop, 0, MUIA_InnerBottom, 0, TAG_DONE),
                                    Child, obj_LEDmcc[3] = NewObject(LED_mcc[3]->mcc_Class, NULL, NoFrame, MUIA_InnerLeft, 0, MUIA_InnerRight, 0, MUIA_InnerTop, 0, MUIA_InnerBottom, 0, TAG_DONE),
                                 End,
                              End,
                           End,

                           SubWindow, win_settings = WindowObject,
                              MUIA_Frame,                 MUIV_Frame_Window,
                              MUIA_Window_Borderless,     FALSE,
                              MUIA_Window_CloseGadget,    TRUE,
                              MUIA_Window_DepthGadget,    TRUE,
                              MUIA_Window_SizeGadget,     TRUE,
                              MUIA_Window_DragBar,        TRUE,
                              MUIA_Window_Title,          "MorphUAE - Settings",
                              MUIA_Window_ID,             MAKE_ID('S','U','A','E'),
                              MUIA_Window_AppWindow,      FALSE,

                              WindowContents, HGroup, NoFrame,

                               Child, VGroup, NoFrame,
                                  InnerSpacing(0, 0),
                                  Child, RegisterGroup(Pages),
                                     MUIA_Register_Frame, TRUE,

                                     Child, HGroup,  // General Tab
                                        Child, ColGroup(2),
                                           Child, but_gen_reset = TextObject, ButtonFrame,
                                              MUIA_Background, MUII_ButtonBack,
                                              MUIA_Weight, 0,
                                              MUIA_Text_PreParse, "\33c",
                                              MUIA_Text_Contents, "Reset to Default",
                                              MUIA_InputMode, MUIV_InputMode_RelVerify,
                                           End,
                                           Child, HSpace(0),
                                           Child, VSpace(0), Child, VSpace(0),
                                           Child, Label1("Machine Type :" ), Child, but_gen_machine = CycleObject, MUIA_Cycle_Entries, cyc_gen_machine, MUIA_ObjectID, ID_PRFS_GEN_MACHINE, MUIA_UserData, ID_PRFS_GEN_MACHINE, End,
                                           Child, VSpace(0), Child, VSpace(0),
                                           Child, RectangleObject, MUIA_Rectangle_HBar, TRUE, MUIA_FixHeight, 8, End,
                                           Child, RectangleObject, MUIA_Rectangle_HBar, TRUE, MUIA_Rectangle_BarTitle, "Sound", MUIA_FixHeight, 8, End,
                                           Child, Label1("Output :"), Child, but_gen_sound = CycleObject, MUIA_Cycle_Entries, cyc_gen_sound, MUIA_ObjectID, ID_PRFS_GEN_SOUND, MUIA_UserData, ID_PRFS_GEN_SOUND, End,
                                           Child, Label1("Channels:"), Child, but_gen_channels = CycleObject, MUIA_Cycle_Entries, cyc_gen_channels, MUIA_ObjectID, ID_PRFS_GEN_CHANNELS, MUIA_UserData, ID_PRFS_GEN_CHANNELS, End,
                                           Child, Label1("Frequency :"), Child, but_gen_frequency = CycleObject, MUIA_Cycle_Entries, cyc_gen_frequency, MUIA_ObjectID, ID_PRFS_GEN_FREQUENCY, MUIA_UserData, ID_PRFS_GEN_FREQUENCY, End,
                                           Child, RectangleObject, MUIA_Rectangle_HBar, TRUE, MUIA_FixHeight, 8, End,
                                           Child, RectangleObject, MUIA_Rectangle_HBar, TRUE, MUIA_Rectangle_BarTitle, "I/O Devices", MUIA_FixHeight, 8, End,
                                           Child, Label1("Joystick Port 0 :"), Child, but_gen_joy0 = CycleObject, MUIA_Cycle_Entries, cyc_gen_joy0, MUIA_ObjectID, ID_PRFS_GEN_JOY0, MUIA_UserData, ID_PRFS_GEN_JOY0, End,
                                           Child, Label1("Joystick Port 1 :"), Child, but_gen_joy1 = CycleObject, MUIA_Cycle_Entries, cyc_gen_joy1, MUIA_ObjectID, ID_PRFS_GEN_JOY1, MUIA_UserData, ID_PRFS_GEN_JOY1, End,
                                           Child, Label1("Floppy Speed :"), Child, but_gen_floppy = CycleObject, MUIA_Cycle_Entries, cyc_gen_floppy, MUIA_ObjectID, ID_PRFS_GEN_FLOPPY, MUIA_UserData, ID_PRFS_GEN_FLOPPY, End,
                                           Child, RectangleObject, MUIA_Rectangle_HBar, TRUE, MUIA_FixHeight, 8, End,
                                           Child, RectangleObject, MUIA_Rectangle_HBar, TRUE, MUIA_Rectangle_BarTitle, "Chipset", MUIA_FixHeight, 8, End,
                                           Child, Label1("Immediate Blits :"), Child, but_gen_blitter = CycleObject, MUIA_Cycle_Entries, cyc_gen_blitter, MUIA_ObjectID, ID_PRFS_GEN_BLITTER, MUIA_UserData, ID_PRFS_GEN_BLITTER, End,
                                           Child, Label1("Sprite Collisions :"), Child, but_gen_sprite = CycleObject, MUIA_Cycle_Entries, cyc_gen_sprite, MUIA_ObjectID, ID_PRFS_GEN_SPRITE, MUIA_UserData, ID_PRFS_GEN_SPRITE, End,
                                           Child, RectangleObject, MUIA_Rectangle_HBar, TRUE, MUIA_FixHeight, 8, End,
                                           Child, RectangleObject, MUIA_Rectangle_HBar, TRUE, MUIA_Rectangle_BarTitle, "Misc", MUIA_FixHeight, 8, End,
                                           Child, Label1("Reset Type :"), Child, but_gen_resetmode = CycleObject, MUIA_Cycle_Entries, cyc_gen_resetmode, MUIA_ObjectID, ID_PRFS_GEN_RESETMODE, MUIA_UserData, ID_PRFS_GEN_RESETMODE, End,
                                           Child, VSpace(0), Child, VSpace(0),
                                        End,
                                     End,
                                     Child, HGroup,  // OCS Tab
                                        Child, ColGroup(2),
                                           Child, but_ocs_reset = TextObject, ButtonFrame,
                                              MUIA_Background, MUII_ButtonBack,
                                              MUIA_Weight, 0,
                                              MUIA_Text_PreParse, "\33c",
                                              MUIA_Text_Contents, "Reset to Default",
                                              MUIA_InputMode, MUIV_InputMode_RelVerify,
                                           End,
                                           Child, HSpace(0),
                                           Child, VSpace(0), Child, VSpace(0),
                                           Child, KeyLabel2("Kickstart File :",'f'),
                                           Child, cyc_ocs_kickstart = PopaslObject,
                                              MUIA_Popstring_String, ocs_kickstart_str = MyKeyString("Kickstarts/Kick1.rom", 1023, NULL, ID_PRFS_OCS_KICKSTART),
                                              MUIA_Popstring_Button, PopButton(MUII_PopFile),
                                              ASLFR_TitleText, "Please select a kickstart file...",
                                           End,
                                           Child, KeyLabel2("Kickstart Key File :",'f'),
                                           Child, cyc_ocs_kickstartkey = PopaslObject,
                                              MUIA_Popstring_String, ocs_kickstartkey_str = MyKeyString("Kickstarts/", 1023, NULL, ID_PRFS_OCS_KICKSTARTKEY),
                                              MUIA_Popstring_Button, PopButton(MUII_PopFile),
                                              ASLFR_TitleText, "Please select a kickstart key file...",
                                           End,
                                           Child, Label1("Chip Memory :" ), Child, but_ocs_chipmem = CycleObject, MUIA_Cycle_Entries, cyc_ocs_chipmem, MUIA_ObjectID, ID_PRFS_OCS_CHIPMEM, MUIA_UserData, ID_PRFS_OCS_CHIPMEM, End,
                                           Child, Label1("Fast Memory :"), Child, but_ocs_fastmem = CycleObject, MUIA_Cycle_Entries, cyc_ocs_fastmem, MUIA_ObjectID, ID_PRFS_OCS_FASTMEM, MUIA_UserData, ID_PRFS_OCS_FASTMEM,  End,
                                           Child, VSpace(0), Child, VSpace(0),
                                        End,
                                     End,
                                     Child, HGroup,  // ECS Tab
                                        Child, ColGroup(2),
                                           Child, but_ecs_reset = TextObject, ButtonFrame,
                                              MUIA_Background, MUII_ButtonBack,
                                              MUIA_Weight, 0,
                                              MUIA_Text_PreParse, "\33c",
                                              MUIA_Text_Contents, "Reset to Default",
                                              MUIA_InputMode, MUIV_InputMode_RelVerify,
                                           End,
                                           Child, HSpace(0),
                                           Child, VSpace(0), Child, VSpace(0),
                                           Child, KeyLabel2("Kickstart File :",'f'),
                                           Child, cyc_ecs_kickstart = PopaslObject,
                                              MUIA_Popstring_String, ecs_kickstart_str = MyKeyString("Kickstarts/Kick2.rom", 1023, NULL, ID_PRFS_ECS_KICKSTART),
                                              MUIA_Popstring_Button, PopButton(MUII_PopFile),
                                              ASLFR_TitleText, "Please select a kickstart file...",
                                           End,
                                           Child, KeyLabel2("Kickstart Key File :",'f'),
                                           Child, cyc_ecs_kickstartkey = PopaslObject,
                                              MUIA_Popstring_String, ecs_kickstartkey_str = MyKeyString("Kickstarts/", 1023, NULL, ID_PRFS_ECS_KICKSTARTKEY),
                                              MUIA_Popstring_Button, PopButton(MUII_PopFile),
                                              ASLFR_TitleText, "Please select a kickstart key file...",
                                           End,
                                           Child, Label1("ECS Mode :"), Child, but_ecs_mode = CycleObject, MUIA_Cycle_Entries, cyc_ecs_mode, MUIA_ObjectID, ID_PRFS_ECS_MODE, MUIA_UserData, ID_PRFS_ECS_MODE, End,
                                           Child, Label1("Chip Memory :" ), Child, but_ecs_chipmem = CycleObject, MUIA_Cycle_Entries, cyc_ecs_chipmem, MUIA_ObjectID, ID_PRFS_ECS_CHIPMEM, MUIA_UserData, ID_PRFS_ECS_CHIPMEM, End,
                                           Child, Label1("Fast Memory :"), Child, but_ecs_fastmem = CycleObject, MUIA_Cycle_Entries, cyc_ecs_fastmem, MUIA_ObjectID, ID_PRFS_ECS_FASTMEM, MUIA_UserData, ID_PRFS_ECS_FASTMEM, End,
                                          // Child, Label1("Zorro3 Memory :"), Child, but_ecs_zorromem = CycleObject, MUIA_Cycle_Entries, cyc_ecs_zorromem, MUIA_ObjectID, ID_PRFS_ECS_ZORROMEM, MUIA_UserData, ID_PRFS_ECS_ZORROMEM, End,
                                           Child, VSpace(0), Child, VSpace(0),
                                        End,
                                     End,
                                     Child, HGroup,  // AGA Tab
                                        Child, ColGroup(2),
                                           Child, but_aga_reset = TextObject, ButtonFrame,
                                              MUIA_Background, MUII_ButtonBack,
                                              MUIA_Weight, 0,
                                              MUIA_Text_PreParse, "\33c",
                                              MUIA_Text_Contents, "Reset to Default",
                                              MUIA_InputMode, MUIV_InputMode_RelVerify,
                                           End,
                                           Child, HSpace(0),
                                           Child, VSpace(0), Child, VSpace(0),
                                           Child, KeyLabel2("Kickstart File :",'f'),
                                           Child, cyc_aga_kickstart = PopaslObject,
                                              MUIA_Popstring_String, aga_kickstart_str = MyKeyString("Kickstarts/Kick3.rom", 1023, NULL, ID_PRFS_AGA_KICKSTART),
                                              MUIA_Popstring_Button, PopButton(MUII_PopFile),
                                              ASLFR_TitleText, "Please select a kickstart file...",
                                           End,
                                           Child, KeyLabel2("Kickstart Key File :",'f'),
                                           Child, cyc_aga_kickstartkey = PopaslObject,
                                              MUIA_Popstring_String, aga_kickstartkey_str = MyKeyString("Kickstarts/", 1023, NULL, ID_PRFS_AGA_KICKSTARTKEY),
                                              MUIA_Popstring_Button, PopButton(MUII_PopFile),
                                              ASLFR_TitleText, "Please select a kickstart key file...",
                                           End,
                                           Child, Label1("Fast Memory :"), Child, but_aga_fastmem = CycleObject, MUIA_Cycle_Entries, cyc_aga_fastmem, MUIA_ObjectID, ID_PRFS_AGA_FASTMEM, MUIA_UserData, ID_PRFS_AGA_FASTMEM, End,
                                           //Child, Label1("Zorro3 Memory :"), Child, but_aga_zorromem = CycleObject, MUIA_Cycle_Entries, cyc_aga_zorromem, MUIA_ObjectID, ID_PRFS_AGA_ZORROMEM, MUIA_UserData, ID_PRFS_AGA_ZORROMEM, End,
                                           Child, VSpace(0), Child, VSpace(0),
                                        End,
                                     End,
                                     Child, HGroup,  // Custom Tab
                                        Child, ColGroup(2),
                                           Child, but_cus_reset = TextObject, ButtonFrame,
                                              MUIA_Background, MUII_ButtonBack,
                                              MUIA_Weight, 0,
                                              MUIA_Text_PreParse, "\33c",
                                              MUIA_Text_Contents, "Reset to Default",
                                              MUIA_InputMode, MUIV_InputMode_RelVerify,
                                           End,
                                           Child, HSpace(0),
                                           Child, VSpace(0), Child, VSpace(0),
                                           Child, KeyLabel2("Kickstart File :",'f'),
                                           Child, cyc_cus_kickstart = PopaslObject,
                                              MUIA_Popstring_String, cus_kickstart_str = MyKeyString("Kickstarts/Kick3.rom", 1023, NULL, ID_PRFS_CUS_KICKSTART),
                                              MUIA_Popstring_Button, PopButton(MUII_PopFile),
                                              ASLFR_TitleText, "Please select a kickstart file...",
                                           End,
                                           Child, KeyLabel2("Kickstart Key File :",'f'),
                                           Child, cyc_cus_kickstartkey = PopaslObject,
                                              MUIA_Popstring_String, cus_kickstartkey_str = MyKeyString("Kickstarts/", 1023, NULL, ID_PRFS_CUS_KICKSTARTKEY),
                                              MUIA_Popstring_Button, PopButton(MUII_PopFile),
                                              ASLFR_TitleText, "Please select a kickstart key file...",
                                           End,
                                           Child, Label1("CPU Model :" ), Child, but_cus_cpu = CycleObject, MUIA_Cycle_Entries, cyc_cus_cpu, MUIA_ObjectID, ID_PRFS_CUS_CPU, MUIA_UserData, ID_PRFS_CUS_CPU, End,
                                           Child, Label1("CPU Speed :" ), Child, but_cus_speed = CycleObject, MUIA_Cycle_Entries, cyc_cus_speed, MUIA_ObjectID, ID_PRFS_CUS_SPEED, MUIA_UserData, ID_PRFS_CUS_SPEED, End,
                                           Child, Label1("JIT Compiler :" ), Child, but_cus_jit = CycleObject, MUIA_Cycle_Entries, cyc_cus_jit, MUIA_ObjectID, ID_PRFS_CUS_JIT, MUIA_UserData, ID_PRFS_CUS_JIT, End,
                                           Child, Label1("Chipset :" ), Child, but_cus_chipset = CycleObject, MUIA_Cycle_Entries, cyc_cus_chipset, MUIA_ObjectID, ID_PRFS_CUS_CHIPSET, MUIA_UserData, ID_PRFS_CUS_CHIPSET, End,
                                           Child, Label1("Chip Memory :"), Child, but_cus_chipmem = CycleObject, MUIA_Cycle_Entries, cyc_cus_chipmem, MUIA_ObjectID, ID_PRFS_CUS_CHIPMEM, MUIA_UserData, ID_PRFS_CUS_CHIPMEM, End,
                                           Child, Label1("Fast Memory :"), Child, but_cus_fastmem = CycleObject, MUIA_Cycle_Entries, cyc_cus_fastmem, MUIA_ObjectID, ID_PRFS_CUS_FASTMEM, MUIA_UserData, ID_PRFS_CUS_FASTMEM, End,
                                           Child, Label1("Zorro3 Memory :"), Child, but_cus_zorromem = CycleObject, MUIA_Cycle_Entries, cyc_cus_zorromem, MUIA_ObjectID, ID_PRFS_CUS_ZORROMEM, MUIA_UserData, ID_PRFS_CUS_ZORROMEM, End,
                                           Child, VSpace(0), Child, VSpace(0),
                                        End,
                                     End,
                                     Child, VGroup,  // About Tab
                                        Child, RawimageObject,
                                           MUIA_DoubleBuffer, 0,
                                           MUIA_InnerLeft, 0, MUIA_InnerRight, 0, MUIA_InnerTop, 0, MUIA_InnerBottom, 0,
                                           MUIA_Frame, MUIV_Frame_None,
                                           MUIA_Rawimage_Data, small_logo,
                                        End,
                                        Child, TextObject, NoFrame,
                                           MUIA_Weight, 0,
                                           MUIA_Text_PreParse, "\33c",
                                           MUIA_Text_Contents, ABOUTSTR, //about_text,
                                        End,
                                        Child, VSpace(0),
                                        Child, TextObject, NoFrame,
                                           MUIA_Weight, 0,
                                           MUIA_Text_PreParse, "\33c",
                                           MUIA_Text_Contents, "Amiga are trademark of Amiga Corporation",
                                        End,
                                     End,
                                  End,
                                  Child, HGroup,
                                     Child, but_use = TextObject, ButtonFrame,
                                        MUIA_Background, MUII_ButtonBack,
                                        MUIA_Text_PreParse, "\33c",
                                        MUIA_Text_Contents, "Use",
                                        MUIA_InputMode, MUIV_InputMode_RelVerify,
                                     End,
                                     Child, but_saveuse = TextObject, ButtonFrame,
                                        MUIA_Background, MUII_ButtonBack,
                                        MUIA_Text_PreParse, "\33c",
                                        MUIA_Text_Contents, "Save & Use",
                                        MUIA_InputMode, MUIV_InputMode_RelVerify,
                                     End,
                                     Child, but_save = TextObject, ButtonFrame,
                                        MUIA_Background, MUII_ButtonBack,
                                        MUIA_Text_PreParse, "\33c",
                                        MUIA_Text_Contents, "Save",
                                        MUIA_InputMode, MUIV_InputMode_RelVerify,
                                     End,
                                     Child, but_cancel = TextObject, ButtonFrame,
                                        MUIA_Background, MUII_ButtonBack,
                                        MUIA_Text_PreParse, "\33c",
                                        MUIA_Text_Contents, "Cancel",
                                        MUIA_InputMode, MUIV_InputMode_RelVerify,
                                     End,
                                  End,
                               End,
                            End,
                         End,
                      End;

   if (!app)
   {
      write_log ("Could not create MUI application!\n");
      return 0;
   }

   DoMethod(win_main, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, app, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
   DoMethod(win_main, MUIM_Notify, MUIA_AppMessage, MUIV_EveryTime, win_main, 3, MUIM_CallHook, &AppMsg_hook, MUIV_TriggerValue);
   DoMethod(win_main, MUIM_Notify, MUIA_Window_InputEvent, "ctrl lalt r", obj_rendermcc, 3, MUIM_Set, MUIA_Reset_Type, MUIV_Reset_Soft);
   DoMethod(win_main, MUIM_Notify, MUIA_Window_InputEvent, "ctrl lalt h", obj_rendermcc, 3, MUIM_Set, MUIA_Reset_Type, MUIV_Reset_Hard);
   DoMethod(win_main, MUIM_Notify, MUIA_Window_InputEvent, "ctrl lalt q", app, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);

   DoMethod(win_main, MUIM_Notify, MUIA_Window_InputEvent, "alt h", obj_rendermcc, 3, MUIM_Set, MUIA_Pointer_State, MUIV_HidePointer);
   DoMethod(win_main, MUIM_Notify, MUIA_Window_InputEvent, "alt s", obj_rendermcc, 3, MUIM_Set, MUIA_Pointer_State, MUIV_ShowPointer);
   
   DoMethod(win_main, MUIM_Notify, MUIA_Window_InputEvent, "ctrl alt 1", obj_rendermcc, 3, MUIM_Set, MUIA_Floppy_Hotkey, MUIV_HKTriggerFloppy0);
   DoMethod(win_main, MUIM_Notify, MUIA_Window_InputEvent, "ctrl alt 2", obj_rendermcc, 3, MUIM_Set, MUIA_Floppy_Hotkey, MUIV_HKTriggerFloppy1);
   DoMethod(win_main, MUIM_Notify, MUIA_Window_InputEvent, "ctrl alt 3", obj_rendermcc, 3, MUIM_Set, MUIA_Floppy_Hotkey, MUIV_HKTriggerFloppy2);
   DoMethod(win_main, MUIM_Notify, MUIA_Window_InputEvent, "ctrl alt 4", obj_rendermcc, 3, MUIM_Set, MUIA_Floppy_Hotkey, MUIV_HKTriggerFloppy3);
   DoMethod(win_main, MUIM_Notify, MUIA_Window_InputEvent, "ctrl alt e", obj_rendermcc, 3, MUIM_Set, MUIA_Floppy_Hotkey, MUIV_HKTriggerEjectAll);

   DoMethod(obj_LEDmcc[0], MUIM_Notify, MUIA_Pressed, FALSE, obj_rendermcc, 3, MUIM_Set, MUIA_Floppy_Hotkey, MUIV_HKTriggerFloppy0);
   DoMethod(obj_LEDmcc[1], MUIM_Notify, MUIA_Pressed, FALSE, obj_rendermcc, 3, MUIM_Set, MUIA_Floppy_Hotkey, MUIV_HKTriggerFloppy1);
   DoMethod(obj_LEDmcc[2], MUIM_Notify, MUIA_Pressed, FALSE, obj_rendermcc, 3, MUIM_Set, MUIA_Floppy_Hotkey, MUIV_HKTriggerFloppy2);
   DoMethod(obj_LEDmcc[3], MUIM_Notify, MUIA_Pressed, FALSE, obj_rendermcc, 3, MUIM_Set, MUIA_Floppy_Hotkey, MUIV_HKTriggerFloppy3);

   DoMethod(btn_reset, MUIM_Notify, MUIA_Pressed, FALSE, obj_rendermcc, 3, MUIM_Set, MUIA_Reset_Type, MUIV_Reset_Custom);
   DoMethod(btn_eject, MUIM_Notify, MUIA_Pressed, FALSE, obj_rendermcc, 3, MUIM_Set, MUIA_Floppy_Hotkey, MUIV_HKTriggerEjectAll);
   DoMethod(btn_camera, MUIM_Notify, MUIA_Pressed, FALSE, obj_rendermcc, 3, MUIM_Set, MUIA_Render_State, MUIV_ScreenShoot);

   DoMethod(btn_fullscreen, MUIM_Notify, MUIA_Pressed, FALSE, obj_rendermcc, 3, MUIM_Set, MUIA_Display_Type, MUIV_Display_Toggle);
   DoMethod(btn_pauseresume, MUIM_Notify, MUIA_Pressed, FALSE, obj_rendermcc, 3, MUIM_Set, MUIA_Control_UAE, MUIV_Control_UAE_Toggle);

   DoMethod(win_main, MUIM_Notify, MUIA_Window_InputEvent, "alt return", obj_rendermcc, 3, MUIM_Set, MUIA_Display_Type, MUIV_Display_Toggle);

   DoMethod(win_main, MUIM_Notify, MUIA_Window_InputEvent, "ctrl alt t", obj_rendermcc, 3, MUIM_Set, MUIA_Toolbar_Active, MUIV_Toolbar_Toggle);

   DoMethod(win_main, MUIM_Notify, MUIA_Window_InputEvent, "ctrl alt p", obj_rendermcc, 3, MUIM_Set, MUIA_Control_UAE, MUIV_Control_UAE_Toggle);

   DoMethod(win_main, MUIM_Notify, MUIA_Window_InputEvent, "ctrl alt s", obj_rendermcc, 3, MUIM_Set, MUIA_Render_State, MUIV_ScreenShoot);

   DoMethod(btn_settings, MUIM_Notify, MUIA_Pressed, FALSE, win_settings, 3, MUIM_Set, MUIA_Window_Open, TRUE);
   DoMethod(win_settings, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, win_settings, 3, MUIM_Set, MUIA_Window_Open, FALSE);
   DoMethod(but_gen_reset, MUIM_Notify, MUIA_Pressed, FALSE, obj_rendermcc, 3, MUIM_Set, MUIA_Settings_Adjust, MUIV_Reset_General);
   DoMethod(but_ocs_reset, MUIM_Notify, MUIA_Pressed, FALSE, obj_rendermcc, 3, MUIM_Set, MUIA_Settings_Adjust, MUIV_Reset_OCS);
   DoMethod(but_ecs_reset, MUIM_Notify, MUIA_Pressed, FALSE, obj_rendermcc, 3, MUIM_Set, MUIA_Settings_Adjust, MUIV_Reset_ECS);
   DoMethod(but_aga_reset, MUIM_Notify, MUIA_Pressed, FALSE, obj_rendermcc, 3, MUIM_Set, MUIA_Settings_Adjust, MUIV_Reset_AGA);
   DoMethod(but_cus_reset, MUIM_Notify, MUIA_Pressed, FALSE, obj_rendermcc, 3, MUIM_Set, MUIA_Settings_Adjust, MUIV_Reset_Custom);
   
   DoMethod(but_use, MUIM_Notify, MUIA_Pressed, FALSE, obj_rendermcc, 3, MUIM_Set, MUIA_Settings_Adjust, MUIV_Settings_Use);
   DoMethod(but_saveuse, MUIM_Notify, MUIA_Pressed, FALSE, obj_rendermcc, 3, MUIM_Set, MUIA_Settings_Adjust, MUIV_Settings_SaveUse);
   DoMethod(but_save, MUIM_Notify, MUIA_Pressed, FALSE, obj_rendermcc, 3, MUIM_Set, MUIA_Settings_Adjust, MUIV_Settings_Save);
   DoMethod(but_cancel, MUIM_Notify, MUIA_Pressed, FALSE, obj_rendermcc, 3, MUIM_Set, MUIA_Settings_Adjust, MUIV_Settings_Cancel);

   set(obj_rendermcc, MUIA_Settings_Adjust, MUIV_Reset_All); //reset_all();
   DoMethod(app, MUIM_Application_Load, MUIV_Application_Load_ENV);
   setup_generic();
   uae_restarted = TRUE;
   uae_restart (-1, NULL);
   set(win_main, MUIA_Window_Open, TRUE);

   //valid_kick = uae_get_kick_status();
   //if (!valid_kick)
   //      set(obj_rendermcc, MUIA_Control_UAE, MUIV_Control_UAE_Pause);

   gfxvidinfo.width  = currprefs.gfx_width_win;
   gfxvidinfo.height = currprefs.gfx_height_win;

   return 1;
}


/****************************************************************************/

int graphics_setup (void)
{
   debug_print("%s (%d)\n", __func__, __LINE__);

   if (!(IntuitionBase = (void*) OpenLibrary ("intuition.library", 0L))) return 0;
   if (!(IconBase = OpenLibrary ("icon.library", 37L))) return 0;
   if (!(GfxBase = (void*) OpenLibrary ("graphics.library", 0L))) return 0;
   if (!(LayersBase = OpenLibrary ("layers.library", 0L))) return 0;
   if (!(MUIMasterBase = OpenLibrary(MUIMASTER_NAME, MUIMASTER_VMIN))) return 0;
   if (!(CyberGfxBase = OpenLibrary ("cybergraphics.library", 40))) return 0;
   if (!(UtilityBase = OpenLibrary ("utility.library", 39))) return 0;
   if (!(render_mcc = Init_Render())) return 0;
   if (!(RawFilterBase = OpenLibrary("multimedia/rawvideo.filter", 51))) return 0;
   if (!(MemoryStreamBase = OpenLibrary("multimedia/memory.stream", 51))) return 0;
   if (!(FileOutputBase = OpenLibrary("multimedia/file.output", 51))) return 0;

   for (int cntr=0; cntr <NUM_OF_LEDS; cntr++)
   {
      if (!(LED_mcc[cntr] = Init_LED()))
         return 0;
   }

   morphuae_icon = GetDiskObject("PROGDIR:MorphUAE");

   initpseudodevices ();
   return 1;
}


/* Allocate and set-up off-screen buffer for rendering Amiga display to
 * when using CGX V41 or better
 *
 * gfxinfo - the buffer description (which gets filled in by this routine)
 *
 */
static APTR setup_cgx_buffer (struct vidbuf_description *gfxinfo)
{
   tmp_gfxinfo = gfxinfo;
   //debug_print("%s (%d)\n", __func__, __LINE__);
   set(obj_rendermcc, MUIA_Initializing_Gfx, MUIV_InitGraphics);
   return tmp_gfxinfo->bufmem;
}

int graphics_init(void)  // TEST
{
   //debug_print("%s (%d) - ROMFile = %s\n", __func__, __LINE__, currprefs.romfile);

   //valid_kick = uae_get_kick_status();
   //debug_print("%s (%d) VALID KickStart = %d\n", __func__, __LINE__, valid_kick);

   if (!uae_restarted)
   {
      gfxvidinfo.width  = DEFAULT_GFX_WIDTH;
      gfxvidinfo.height = DEFAULT_GFX_HEIGHT;

      gfxvidinfo.width += 7;
      gfxvidinfo.width &= ~7;

      if (!mui_setup_window ())
            return 0;

//      valid_kick = uae_get_kick_status();
//      if (!valid_kick)
//         set(obj_rendermcc, MUIA_Control_UAE, MUIV_Control_UAE_Pause);

      gfxvidinfo.emergmem = 0;
      gfxvidinfo.linemem  = 0;

      setup_cgx_buffer (&gfxvidinfo);

      gfxvidinfo.flush_clear_screen = flush_clear_screen_gfxlib;
      gfxvidinfo.flush_screen       = dummy_flush_screen;
      gfxvidinfo.lockscr            = dummy_lock;
      gfxvidinfo.unlockscr          = dummy_unlock;


      if (!gfxvidinfo.bufmem)
      {
            write_log ("MUIGFX: Not enough memory for video bufmem.\n");
            return 0;
      }

      gfxvidinfo.maxblocklines = MAXBLOCKLINES_MAX;

      if (!init_colors ())
      {
            write_log ("MUIGFX: Failed to init colors.\n");
            return 0;
      }
   }
   else
   {
      uae_restarted = FALSE;
      valid_kick = uae_get_kick_status();
      debug_print("%s (%d) VALID KickStart = %d\n", __func__, __LINE__, valid_kick);
      if (!valid_kick)
         set(obj_rendermcc, MUIA_Control_UAE, MUIV_Control_UAE_Pause);
      else
         set(obj_rendermcc, MUIA_Control_UAE, MUIV_Control_UAE_Resume);
   }

   setup_generic();
   reset_drawing ();
   debug_print("%s (%d) - ROMFile = %s\n", __func__, __LINE__, currprefs.romfile);

   return 1; 
}


/****************************************************************************/

void graphics_leave (void)
{
   debug_print("%s (%d) - Restarted = %d\n", __func__, __LINE__, uae_restarted);
   set(obj_rendermcc, MUIA_Cleanup_Gfx, MUIV_CleanupGraphics);

   if (!uae_restarted)
   {
      if (app) MUI_DisposeObject(app);
      if (render_mcc) Cleanup_Render(render_mcc);

      for (int cntr=0; cntr <NUM_OF_LEDS; cntr++)
            if (LED_mcc[cntr]) Cleanup_LED(LED_mcc[cntr]);

      if (FileOutputBase) CloseLibrary(FileOutputBase);
      if (MemoryStreamBase) CloseLibrary(MemoryStreamBase);
      if (RawFilterBase) CloseLibrary(RawFilterBase);
      if (UtilityBase) CloseLibrary ((void*)UtilityBase);
      if (AslBase) CloseLibrary( (void*) AslBase);
      if (GfxBase) CloseLibrary ((void*)GfxBase);
      if (IconBase) CloseLibrary ((void*)IconBase);
      if (LayersBase) CloseLibrary (LayersBase);
      if (IntuitionBase) CloseLibrary ((void*)IntuitionBase);
      if (MUIMasterBase) CloseLibrary(MUIMasterBase);
      if (CyberGfxBase) CloseLibrary((void*)CyberGfxBase);
   }
   else
   {
//      valid_kick = uae_get_kick_status();
//       if (!valid_kick)
//          set(obj_rendermcc, MUIA_Control_UAE, MUIV_Control_UAE_Pause);
   }
}

/****************************************************************************/
/*
int do_inhibit_frame (int onoff)
{
   debug_print("%s (%d)\n", __func__, __LINE__);
   if (onoff != -1)
   {
      inhibit_frame = onoff ? 1 : 0;
      if (inhibit_frame)
         write_log ("display disabled\n");
      else
         write_log ("display enabled\n");
         //set_title ();
   }
   return inhibit_frame;
}
*/
/***************************************************************************/

void graphics_notify_state (int state)
{
   //debug_print("%s (%d)\n", __func__, __LINE__);
}

/***************************************************************************/

void handle_events(void)
{
   ULONG muisig = 0;

   if (DoMethod(app, MUIM_Application_NewInput, &muisig) == MUIV_Application_ReturnID_Quit)
      uae_quit();
}

/***************************************************************************/

int debuggable (void)
{
    return 1;
}

/***************************************************************************/

int mousehack_allowed (void)
{
   //debug_print("%s (%d)\n", __func__, __LINE__);
   return 0;
}

/***************************************************************************/

void LED (int on)
{
   //debug_print("%s (%d)\n", __func__, __LINE__);
}

/***************************************************************************/

/* sam: need to put all this in a separate module */

#ifdef PICASSO96

void DX_Invalidate (int first, int last)
{
   //debug_print("%s (%d)\n", __func__, __LINE__);
}

int DX_BitsPerCannon (void)
{
   //debug_print("%s (%d)\n", __func__, __LINE__);
   return 8;
}

void DX_SetPalette (int start, int count)
{
   //debug_print("%s (%d)\n", __func__, __LINE__);
}

int DX_FillResolutions (uae_u16 *ppixel_format)
{
   //debug_print("%s (%d)\n", __func__, __LINE__);
   return 0;
}

void gfx_set_picasso_modeinfo (int w, int h, int depth)
{
   //debug_print("%s (%d)\n", __func__, __LINE__);
}

void gfx_set_picasso_state (int on)
{
   //debug_print("%s (%d)\n", __func__, __LINE__);
}
#endif

/***************************************************************************/

//static int led_state[5];

//#define WINDOW_TITLE PACKAGE_NAME " " PACKAGE_VERSION

/****************************************************************************/

void main_window_led (int led, int on)                /* is used in amigui.c */
{
   //debug_print("%s (%d)\n", __func__, __LINE__);
/*
#if 0
   if (led >= 0 && led <= 4)
      led_state[led] = on;
#endif
*/
}

/****************************************************************************/

int check_prefs_changed_gfx (void)
{
   //debug_print("%s (%d)\n", __func__, __LINE__);
   return 0;
}

/****************************************************************************/

void toggle_mousegrab (void)
{
   //debug_print("%s (%d)\n", __func__, __LINE__);
   write_log ("Mouse grab not supported\n");
}

int is_fullscreen (void)
{
   //debug_print("%s (%d)\n", __func__, __LINE__);
   //return fullscreen;
}

int is_vsync (void)
{
   //debug_print("%s (%d)\n", __func__, __LINE__);
   return 0;
}

void toggle_fullscreen (void)
{
   //debug_print("%s (%d)\n", __func__, __LINE__);
}

void screenshot (int type)
{
   //debug_print("%s (%d)\n", __func__, __LINE__);
   write_log ("Screenshot not implemented yet\n");
}

/****************************************************************************
 *
 * Mouse inputdevice functions
 */

#define MAX_BUTTONS     3
#define MAX_AXES        3
#define FIRST_AXIS      0
#define FIRST_BUTTON    MAX_AXES

static int init_mouse (void)
{
   //debug_print("%s (%d)\n", __func__, __LINE__);
   return 1;
}

static void close_mouse (void)
{
   //debug_print("%s (%d)\n", __func__, __LINE__);
   return;
}

static int acquire_mouse (unsigned int num, int flags)
{
   //debug_print("%s (%d)\n", __func__, __LINE__);
   return 1;
}

static void unacquire_mouse (unsigned int num)
{
   //debug_print("%s (%d)\n", __func__, __LINE__);
   return;
}

static unsigned int get_mouse_num (void)
{
   //debug_print("%s (%d)\n", __func__, __LINE__);
   return 1;
}

static const char *get_mouse_name (unsigned int mouse)
{
   //debug_print("%s (%d)\n", __func__, __LINE__);
   return "Default mouse";
}

static unsigned int get_mouse_widget_num (unsigned int mouse)
{
   //debug_print("%s (%d)\n", __func__, __LINE__);
   return MAX_AXES + MAX_BUTTONS;
}

static int get_mouse_widget_first (unsigned int mouse, int type)
{
   //debug_print("%s (%d)\n", __func__, __LINE__);
   switch (type)
   {
      case IDEV_WIDGET_BUTTON:
         return FIRST_BUTTON;
      case IDEV_WIDGET_AXIS:
         return FIRST_AXIS;
   }
   return -1;
}

static int get_mouse_widget_type (unsigned int mouse, unsigned int num, char *name, uae_u32 *code)
{
   //debug_print("%s (%d)\n", __func__, __LINE__);
   if (num >= MAX_AXES && num < MAX_AXES + MAX_BUTTONS)
   {
      if (name)
         sprintf (name, "Button %d", num + 1 + MAX_AXES);
      return IDEV_WIDGET_BUTTON;
   }
   else if (num < MAX_AXES)
   {
      if (name)
         sprintf (name, "Axis %d", num + 1);
      return IDEV_WIDGET_AXIS;
   }
   return IDEV_WIDGET_NONE;
}

static void read_mouse (void)
{
   //debug_print("%s (%d)\n", __func__, __LINE__);
    /* We handle mouse input in handle_events() */
}

struct inputdevice_functions inputdevicefunc_mouse = {
   init_mouse,
   close_mouse,
   acquire_mouse,
   unacquire_mouse,
   read_mouse,
   get_mouse_num,
   get_mouse_name,
   get_mouse_widget_num,
   get_mouse_widget_type,
   get_mouse_widget_first
};

/*
 * Default inputdevice config for mouse
 */
void input_get_default_mouse (struct uae_input_device *uid)
{
   //debug_print("%s (%d)\n", __func__, __LINE__);
   /* Supports only one mouse for now */
   uid[0].eventid[ID_AXIS_OFFSET + 0][0]   = INPUTEVENT_MOUSE1_HORIZ;
   uid[0].eventid[ID_AXIS_OFFSET + 1][0]   = INPUTEVENT_MOUSE1_VERT;
   uid[0].eventid[ID_AXIS_OFFSET + 2][0]   = INPUTEVENT_MOUSE1_WHEEL;
   uid[0].eventid[ID_BUTTON_OFFSET + 0][0] = INPUTEVENT_JOY1_FIRE_BUTTON;
   uid[0].eventid[ID_BUTTON_OFFSET + 1][0] = INPUTEVENT_JOY1_2ND_BUTTON;
   uid[0].eventid[ID_BUTTON_OFFSET + 2][0] = INPUTEVENT_JOY1_3RD_BUTTON;
   uid[0].enabled = 1;
}

/****************************************************************************
 *
 * Keyboard inputdevice functions
 */
static unsigned int get_kb_num (void)
{
   //debug_print("%s (%d)\n", __func__, __LINE__);
   return 1;
}

static const char *get_kb_name (unsigned int kb)
{
   //debug_print("%s (%d)\n", __func__, __LINE__);
   return "Default keyboard";
}

static unsigned int get_kb_widget_num (unsigned int kb)
{
   //debug_print("%s (%d)\n", __func__, __LINE__);
   return 128;
}

static int get_kb_widget_first (unsigned int kb, int type)
{
   //debug_print("%s (%d)\n", __func__, __LINE__);
   return 0;
}

static int get_kb_widget_type (unsigned int kb, unsigned int num, char *name, uae_u32 *code)
{
   //debug_print("%s (%d)\n", __func__, __LINE__);
   // fix me
   *code = num;
   return IDEV_WIDGET_KEY;
}

static int keyhack (int scancode, int pressed, int num)
{
   //debug_print("%s (%d)\n", __func__, __LINE__);
   return scancode;
}

static void read_kb (void)
{
}

static int init_kb (void)
{
   return 1;
}

static void close_kb (void)
{
}

static int acquire_kb (unsigned int num, int flags)
{
   return 1;
}

static void unacquire_kb (unsigned int num)
{
}

struct inputdevice_functions inputdevicefunc_keyboard =
{
   init_kb,
   close_kb,
   acquire_kb,
   unacquire_kb,
   read_kb,
   get_kb_num,
   get_kb_name,
   get_kb_widget_num,
   get_kb_widget_type,
   get_kb_widget_first
};

int getcapslockstate (void)
{
   //debug_print("%s (%d)\n", __func__, __LINE__);
   return 0;
}

void setcapslockstate (int state)
{
   //debug_print("%s (%d)\n", __func__, __LINE__);
}

/****************************************************************************
 *
 * Handle gfx specific cfgfile options
 */

//static const char *screen_type[] = { "custom", "public", "ask", 0 };

void gfx_default_options (struct uae_prefs *p)
{
   //debug_print("%s (%d)\n", __func__, __LINE__);
}

void gfx_save_options (FILE *f, const struct uae_prefs *p)
{
   //debug_print("%s (%d)\n", __func__, __LINE__);
}

int gfx_parse_option (struct uae_prefs *p, const char *option, const char *value)
{
   //debug_print("%s (%d)\n", __func__, __LINE__);
}

/****************************************************************************/
