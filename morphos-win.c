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
  *
  * About SVG Vector
  * (c) Siemens
  * https://www.svgrepo.com/svg/486929/about
  *
  */

#define CATCOMP_ARRAY
#define CATCOMP_NUMBERS

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
#include <libraries/locale.h>

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
# include <proto/locale.h>

#include <proto/cybergraphics.h>
#include <cybergraphx/cybergraphics.h>

#include <proto/multimedia.h>
#include <classes/multimedia/video.h>
#include <classes/multimedia/metadata.h>

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
#include <morphuae_locale.h>

// MCC Classes
#include <mui/Rawimage_mcc.h>
#include <LEDmcc.h>

#define STRNAME     "MorphUAE"
#define STRVERSION  "1"
#define STRREVISION "0"

#define DATE        __AMIGADATE__
#define VSTRING     STRVERSION"."STRREVISION " ("DATE") "
#define MVERSTAG    "$VER: " STRNAME " " VSTRING
#define ABOUTSTR    "\n" STRNAME " " VSTRING "\n"

/****************************************************************************/

extern xcolnr xcolors[4096];
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
struct Library       *LocaleBase = NULL;


struct vidbuf_description *tmp_gfxinfo;
int tmp_line_no;
int tmp_first_line;
int tmp_last_line;
BOOL uae_restarted = FALSE;
int valid_kick = 0;
ULONG lock;
UBYTE *tmpdata = NULL;

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

#define MyKeyString(contents, maxlen, controlchar, id, weight)\
   StringObject,\
      StringFrame,\
      MUIA_ControlChar    , controlchar,\
      MUIA_String_MaxLen  , maxlen,\
      MUIA_String_Contents, contents,\
      MUIA_Weight, weight,\
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

// Function borrowed from Ambient sources... and adjusted slightly
APTR MUICreateCheckbox(ULONG defstate, ULONG id)
{
   APTR checkbox;

   checkbox = MUI_NewObject(MUIC_Image, MUIA_Frame, MUIV_Frame_ImageButton,
                            MUIA_Background , MUII_ButtonBack,
                            MUIA_Image_FreeVert, TRUE,
                            MUIA_InputMode, MUIV_InputMode_Toggle,
                            MUIA_Image_Spec, MUII_CheckMark,
                            MUIA_Selected, defstate,
                            MUIA_ShowSelState, FALSE,
                            MUIA_ObjectID, id,
                            MUIA_UserData, id,
                            TAG_DONE);
   return(checkbox);
}


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
static Object *win_about       = NULL;  // Window object
static Object *obj_rendermcc   = NULL;  // Render object
struct Object *btn_settings    = NULL;
struct Object *btn_camera      = NULL;
struct Object *btn_reset       = NULL;
struct Object *btn_eject       = NULL;
struct Object *btn_fullscreen  = NULL;
struct Object *btn_pauseresume = NULL;
struct Object *btn_about       = NULL;
struct Object *grp_toolbar     = NULL;
struct Object *ctm_reset       = NULL;
struct Object *ctm_eject       = NULL;

struct DiskObject *morphuae_icon = NULL;
struct Locale *MorphUAE_Locale;
static struct Catalog *MorphUAE_Catalog;


static STRPTR cyc_list_jport[4];
static STRPTR cyc_list_sndout[5];
static STRPTR cyc_list_sndchan[4];
static STRPTR cyc_list_floppy[4];
static STRPTR cyc_list_blits[3];
static STRPTR cyc_list_sprites[5];
static STRPTR cyc_list_frames[4];
static STRPTR cyc_list_reset[3];
static STRPTR cyc_list_speed[3];
static STRPTR cyc_list_jit[5];
static STRPTR cyc_list_keys[8];


#define DEFAULT_GFX_WIDTH 640
#define DEFAULT_GFX_HEIGHT 512


struct RenderData
{
   BOOL Active;
   BOOL InitOK;
   BOOL showpointer;
   BOOL FullScreen;
   BOOL ToolBar;
   BOOL Iconified;
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
#define MUIV_Iconified          (TAGBASE_DEVELIN | 0x0008)
#define MUIV_UnIconified        (TAGBASE_DEVELIN | 0x0009)
#define MUIV_InitGraphics       (TAGBASE_DEVELIN | 0x000a)
#define MUIV_InitColours        (TAGBASE_DEVELIN | 0x000b)
#define MUIV_CleanupGraphics    (TAGBASE_DEVELIN | 0x000c)

// Mouse pointers...
#define MUIV_ShowPointer        (TAGBASE_DEVELIN | 0x000d)
#define MUIV_HidePointer        (TAGBASE_DEVELIN | 0x000e)

// Render flushes...
#define MUIV_FlushLineCGX       (TAGBASE_DEVELIN | 0x000f)
#define MUIV_FlushBlockCGX      (TAGBASE_DEVELIN | 0x0010)
#define MUIV_ScreenShoot        (TAGBASE_DEVELIN | 0x0011)
#define MUIV_FlushClearScreen   (TAGBASE_DEVELIN | 0x0012)

#define MUIA_Floppy_Hotkey      (TAGBASE_DEVELIN | 0x0013)

#define MUIV_HKTriggerFloppy0   0
#define MUIV_HKTriggerFloppy1   1
#define MUIV_HKTriggerFloppy2   2
#define MUIV_HKTriggerFloppy3   3
#define MUIV_HKTriggerEjectAll  4

#define MUIA_Reset_Type         (TAGBASE_DEVELIN | 0x0019)
#define MUIV_Reset_Soft         0
#define MUIV_Reset_Hard         1
#define MUIV_Reset_UserSelect   2

#define MUIA_Display_Type       (TAGBASE_DEVELIN | 0x001c)
#define MUIV_Display_Window     0
#define MUIV_Display_Screen     1
#define MUIV_Display_Toggle     2

#define MUIA_Toolbar_Active     (TAGBASE_DEVELIN | 0x0020)
#define MUIV_Toolbar_Off        0
#define MUIV_Toolbar_On         1
#define MUIV_Toolbar_Toggle     2

#define MUIA_Control_UAE        (TAGBASE_DEVELIN | 0x0025)
#define MUIV_Control_UAE_Pause  0
#define MUIV_Control_UAE_Resume 1
#define MUIV_Control_UAE_Toggle 2

#define MUIA_Settings_Adjust    (TAGBASE_DEVELIN | 0x0030)
#define MUIV_Reset_All          0
#define MUIV_Reset_General      1
#define MUIV_Reset_OCS          2
#define MUIV_Reset_ECS          3
#define MUIV_Reset_AGA          4
#define MUIV_Reset_Custom       5
#define MUIV_Settings_Use       6
#define MUIV_Settings_SaveUse   7
#define MUIV_Settings_Save      8
#define MUIV_Settings_Cancel    9

#define MUIA_Runtime_Port0     (TAGBASE_DEVELIN | 0x0040)
#define MUIA_Runtime_Port1     (TAGBASE_DEVELIN | 0x0041)


// Global variable...
static struct MUI_CustomClass *render_mcc = NULL; // Our Render MCC

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


/*=----------------------------- Locale_Open()--------------------------------*
 *                                                                            *
 *----------------------------------------------------------------------------*/
BOOL Locale_Open( char *catname, ULONG version, ULONG revision)
{
   if ( (LocaleBase = (struct Library *)OpenLibrary("locale.library", 0)) )
   {
      if ( (MorphUAE_Locale = OpenLocale(NULL)) )
      {
         if ( (MorphUAE_Catalog = OpenCatalogA(MorphUAE_Locale, catname, TAG_DONE)) )
         {
            if (MorphUAE_Catalog->cat_Version == version && MorphUAE_Catalog->cat_Revision == revision)
            {
               return(TRUE);
            }

            CloseCatalog(MorphUAE_Catalog);
            MorphUAE_Catalog = NULL;
         }

         CloseLocale(MorphUAE_Locale);
         MorphUAE_Locale = NULL;
      }
      CloseLibrary(LocaleBase);
   }

   return(FALSE);
}
/*=*/

/*=----------------------------- Locale_Close()-------------------------------*
 *                                                                            *
 *----------------------------------------------------------------------------*/
void Locale_Close(void)
{
   if (LocaleBase)
   {
      if (MorphUAE_Catalog)
      {
         CloseCatalog(MorphUAE_Catalog);
         MorphUAE_Catalog = NULL;
      }

      if (MorphUAE_Locale)
      {
         CloseLocale(MorphUAE_Locale);
         MorphUAE_Locale = NULL;
      }


      CloseLibrary(LocaleBase);
      LocaleBase = NULL;
   }
}
/*=*/

/*=----------------------------- Locale_GetString()---------------------------*
 *                                                                            *
 *----------------------------------------------------------------------------*/
STRPTR Locale_GetString( long id )
{
   STRPTR defstr;
   LONG strnum;

   strnum = CatCompArray[id].cca_ID;
   defstr = CatCompArray[id].cca_Str;

   if (MorphUAE_Catalog && LocaleBase)
      return GetCatalogStr(MorphUAE_Catalog, strnum, defstr);
   else
      return defstr;
}
/*=*/


/*=----------------------------- Populate_CycleStrings()----------------------*
 *                                                                            *
 *----------------------------------------------------------------------------*/
void Populate_CycleStrings(void)
{
      cyc_list_jport[0] = Locale_GetString(MSG_CYC_JPORT_0);
      cyc_list_jport[1] = Locale_GetString(MSG_CYC_JPORT_1);
      cyc_list_jport[2] = Locale_GetString(MSG_CYC_JPORT_2);

      cyc_list_sndout[0] = Locale_GetString(MSG_CYC_SOUNDOUT_0);
      cyc_list_sndout[1] = Locale_GetString(MSG_CYC_SOUNDOUT_1);
      cyc_list_sndout[2] = Locale_GetString(MSG_CYC_SOUNDOUT_2);
      cyc_list_sndout[3] = Locale_GetString(MSG_CYC_SOUNDOUT_3);

      cyc_list_sndchan[0] = Locale_GetString(MSG_CYC_SOUNDCHAN_0);
      cyc_list_sndchan[1] = Locale_GetString(MSG_CYC_SOUNDCHAN_1);
      cyc_list_sndchan[2] = Locale_GetString(MSG_CYC_SOUNDCHAN_2);

      cyc_list_floppy[0] = Locale_GetString(MSG_CYC_FLOPPY_0);
      cyc_list_floppy[1] = Locale_GetString(MSG_CYC_FLOPPY_1);
      cyc_list_floppy[2] = Locale_GetString(MSG_CYC_FLOPPY_2);

      cyc_list_blits[0] = Locale_GetString(MSG_CYC_BLITS_0);
      cyc_list_blits[1] = Locale_GetString(MSG_CYC_BLITS_1);

      cyc_list_sprites[0] = Locale_GetString(MSG_CYC_SPRITES_0);
      cyc_list_sprites[1] = Locale_GetString(MSG_CYC_SPRITES_1);
      cyc_list_sprites[2] = Locale_GetString(MSG_CYC_SPRITES_2);
      cyc_list_sprites[3] = Locale_GetString(MSG_CYC_SPRITES_3);

      cyc_list_frames[0] = Locale_GetString(MSG_CYC_FRAMER_0);
      cyc_list_frames[1] = Locale_GetString(MSG_CYC_FRAMER_1);
      cyc_list_frames[2] = Locale_GetString(MSG_CYC_FRAMER_2);

      cyc_list_reset[0] = Locale_GetString(MSG_CYC_RESET_0);
      cyc_list_reset[1] = Locale_GetString(MSG_CYC_RESET_1);

      cyc_list_speed[0] = Locale_GetString(MSG_CYC_SPEED_0);
      cyc_list_speed[1] = Locale_GetString(MSG_CYC_SPEED_1);

      cyc_list_jit[0] = Locale_GetString(MSG_CYC_JIT_0);
      cyc_list_jit[1] = Locale_GetString(MSG_CYC_JIT_1);
      cyc_list_jit[2] = Locale_GetString(MSG_CYC_JIT_2);
      cyc_list_jit[3] = Locale_GetString(MSG_CYC_JIT_3);

      cyc_list_keys[0] = Locale_GetString(MSG_CYC_KEYS_0);
      cyc_list_keys[1] = Locale_GetString(MSG_CYC_KEYS_1);
      cyc_list_keys[2] = Locale_GetString(MSG_CYC_KEYS_2);
      cyc_list_keys[3] = Locale_GetString(MSG_CYC_KEYS_3);
      cyc_list_keys[4] = Locale_GetString(MSG_CYC_KEYS_4);
      cyc_list_keys[5] = Locale_GetString(MSG_CYC_KEYS_5);
      cyc_list_keys[6] = Locale_GetString(MSG_CYC_KEYS_6);
}
/*=*/

/*=----------------------------- reset_tab() ---------------------------------*
 *                                                                            *
 *----------------------------------------------------------------------------*/
void reset_tab(unsigned int tab)
{
   debug_print("%s (%d)\n", __func__, __LINE__);

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
         set(but_gen_language, MUIA_Cycle_Active, 0);  // US / UK (Default)
         set(but_gen_blitter, MUIA_Cycle_Active, 0);   // Off - Check this!
         set(but_gen_sprite, MUIA_Cycle_Active, 3);    // Full - Check this!
         set(but_gen_framerate, MUIA_Cycle_Active, 0); // Every one
         set(but_gen_resetmode, MUIA_Cycle_Active, 0); // Soft!
      }  break;

      case ID_BUT_OCS_RESET :
      {
         set(ocs_kickstart_str, MUIA_String_Contents, "PROGDIR:Kickstarts/Kick1.rom");
         set(ocs_kickstartkey_str, MUIA_String_Contents, "PROGDIR:Kickstarts/rom.key");
         set(but_ocs_chipmem, MUIA_Cycle_Active, 0);   // 0,5Mb
         set(but_ocs_fastmem, MUIA_Cycle_Active, 0);   // 0 Mb
      }  break;

      case ID_BUT_ECS_RESET :
      {
         set(ecs_kickstart_str, MUIA_String_Contents, "PROGDIR:Kickstarts/Kick2.rom");
         set(ecs_kickstartkey_str, MUIA_String_Contents, "PROGDIR:Kickstarts/rom.key");
         set(but_ecs_mode, MUIA_Cycle_Active, 0);      // ECS Agnus
         set(but_ecs_chipmem, MUIA_Cycle_Active, 1);   // 2 Mb
         set(but_ecs_fastmem, MUIA_Cycle_Active, 0);   // 0 Mb
      }  break;

      case ID_BUT_AGA_RESET :
      {
         set(aga_kickstart_str, MUIA_String_Contents, "PROGDIR:Kickstarts/Kick3.rom");
         set(aga_kickstartkey_str, MUIA_String_Contents, "PROGDIR:Kickstarts/rom.key");
         set(but_aga_fastmem, MUIA_Cycle_Active, 4);   // 8 Mb
      }  break;

      case ID_BUT_CUS_RESET :
      {
         set(cus_kickstart_str, MUIA_String_Contents, "PROGDIR:Kickstarts/Kick3.rom");
         set(cus_kickstartkey_str, MUIA_String_Contents, "PROGDIR:Kickstarts/rom.key");
         set(chk_harddisk1, MUIA_Selected, FALSE);
         set(cus_harddisk1_str, MUIA_String_Contents, "PROGDIR:Harddisks/");
         set(cus_devname1_str, MUIA_String_Contents, "DH0");
         set(cus_volname1_str, MUIA_String_Contents, "System");
         set(chk_harddisk2, MUIA_Selected, FALSE);
         set(cus_harddisk2_str, MUIA_String_Contents, "PROGDIR:Harddisks/");
         set(cus_devname2_str, MUIA_String_Contents, "DH1");
         set(cus_volname2_str, MUIA_String_Contents, "Work");
         set(but_cus_cpu, MUIA_Cycle_Active, 0);      // 68020
         set(but_cus_speed, MUIA_Cycle_Active, 1);    // Max
         set(but_cus_jit, MUIA_Cycle_Active, 0);      // Off
         set(but_cus_chipset, MUIA_Cycle_Active, 2);  // AGA
         set(but_cus_chipmem, MUIA_Cycle_Active, 2);  // 2 Mb
         set(but_cus_fastmem, MUIA_Cycle_Active, 4);  // 8 Mb
         set(but_cus_zorromem, MUIA_Cycle_Active, 5); // 16 Mb
      }  break;
   } 
}
/*=*/

/*=----------------------------- reset_all() ---------------------------------*
 *                                                                            *
 *----------------------------------------------------------------------------*/
void reset_all(void)
{
   debug_print("%s (%d)\n", __func__, __LINE__);

   reset_tab(ID_BUT_GEN_RESET);
   reset_tab(ID_BUT_OCS_RESET);
   reset_tab(ID_BUT_ECS_RESET);
   reset_tab(ID_BUT_AGA_RESET);
   reset_tab(ID_BUT_CUS_RESET);
}
/*=*/

/*=----------------------------- setup_specific() ----------------------------*
 *                                                                            *
 *----------------------------------------------------------------------------*/
void setup_specific(int conf)
{
   STRPTR ks, ksk, vhd, devn, voln;
   LONG val;
   int fscnt = 0;

   debug_print("%s (%d)\n", __func__, __LINE__);

   if (conf == UAE_CFGTYPE_OCS) // OCS
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

      changed_prefs.cpu_level = 0;   // 68000
      changed_prefs.m68k_speed = 0;  // Real
   }
   else if (conf == UAE_CFGTYPE_ECS) // ECS
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

      changed_prefs.cpu_level = 0;   // 68000
      changed_prefs.m68k_speed = 0;  // Real
   }
   else if (conf == UAE_CFGTYPE_AGA) // AGA
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

      changed_prefs.cpu_level = 2;  // 68020
      changed_prefs.m68k_speed = 0; // Real
   }
   else // Custom
   {
      GetAttr(MUIA_String_Contents, cus_kickstart_str, (ULONG *)&ks);
      GetAttr(MUIA_String_Contents, cus_kickstartkey_str, (ULONG *)&ksk);

      get(chk_harddisk1, MUIA_Selected, &val);
      if (val)
      {
         debug_print("%s (%d) - CHK-Harddisk 1 - ACTIVE\n", __func__, __LINE__);
         GetAttr(MUIA_String_Contents, cus_harddisk1_str, (ULONG *)&vhd);
         GetAttr(MUIA_String_Contents, cus_devname1_str, (ULONG *)&devn);
         GetAttr(MUIA_String_Contents, cus_volname1_str, (ULONG *)&voln);
         add_filesys_unit(currprefs.mountinfo, devn, voln, vhd, 0, 0, 0, 0, 0, 0, 0, 0);
         fscnt++;
      }
      else
      {
         debug_print("%s (%d) - CHK-Harddisk 1 - INACTIVE\n", __func__, __LINE__);
         kill_filesys_unit(currprefs.mountinfo, 0);
      }

      get(chk_harddisk2, MUIA_Selected, &val);
      if (val)
      {
            debug_print("%s (%d) - CHK-Harddisk 2 - ACTIVE\n", __func__, __LINE__);
            GetAttr(MUIA_String_Contents, cus_harddisk2_str, (ULONG *)&vhd);
            GetAttr(MUIA_String_Contents, cus_devname2_str, (ULONG *)&devn);
            GetAttr(MUIA_String_Contents, cus_volname2_str, (ULONG *)&voln);
            add_filesys_unit(currprefs.mountinfo, devn, voln, vhd, 0, 0, 0, 0, 0, 0, 0, 0);
      }
      else
      {
            debug_print("%s (%d) - CHK-Harddisk 2 - INACTIVE\n", __func__, __LINE__);
            kill_filesys_unit(currprefs.mountinfo, 1);
      }

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
/*=*/

/*=----------------------------- setup_generic() -----------------------------*
 *                                                                            *
 *----------------------------------------------------------------------------*/
void setup_generic(void)
{
   LONG mpos, spos, jpos, val;
   int clicfg = UAE_CFGTYPE_DEFAULT;

   debug_print("%s (%d)\n", __func__, __LINE__);

   get(but_gen_machine, MUIA_Cycle_Active, &mpos);

   clicfg = uae_get_fgctype();    // Incase we have got a cfg parameter from commandline, we should override the GUI-settings with it...

   if (clicfg == UAE_CFGTYPE_DEFAULT)
      setup_specific(mpos);
   else
      setup_specific(clicfg);

   // Sound
   get(but_gen_sound, MUIA_Cycle_Active, &spos);
   changed_prefs.produce_sound = spos;
   get(but_gen_channels, MUIA_Cycle_Active, &spos);
   changed_prefs.sound_stereo = spos;
   get(but_gen_frequency, MUIA_Cycle_Active, &spos);
   changed_prefs.sound_freq = (spos == 0 ? 11025 : spos == 1 ? 22055 : spos == 2 ? 44100 : 48000);

   // IO Devices...
   get(but_gen_joy0, MUIA_Cycle_Active, &jpos);
   set(but_tmp_joy0, MUIA_Cycle_Active, jpos);
   changed_prefs.jport0 = (jpos == 0 ? 200 : jpos == 1 ? 100 : 101);
   get(but_gen_joy1, MUIA_Cycle_Active, &jpos);
   set(but_tmp_joy1, MUIA_Cycle_Active, jpos);
   changed_prefs.jport1 = (jpos == 0 ? 200 : jpos == 1 ? 100 : 101);
   get(but_gen_floppy, MUIA_Cycle_Active, &jpos);
   changed_prefs.floppy_speed = (jpos == 0 ? 100 : jpos == 1 ? 500 : 1000);
   get(but_gen_language, MUIA_Cycle_Active, &jpos);
   changed_prefs.keyboard_lang = jpos;

   // Chipset...
   get(but_gen_blitter, MUIA_Cycle_Active, &val);
   changed_prefs.immediate_blits = val;
   get(but_gen_sprite, MUIA_Cycle_Active, &val);
   changed_prefs.collision_level = val;
   get(but_gen_framerate, MUIA_Cycle_Active, &val);
   changed_prefs.gfx_framerate = val+1;
}
/*=*/

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
         sprintf(tmpstr, Locale_GetString(MSG_INSERT_IMAGE), unit);
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

/*=----------------------------- Save_Reggae() -------------------------------*
 *                                                                            *
 *----------------------------------------------------------------------------*/
BOOL Save_Reggae(char *fname, UBYTE *imgdata, WORD width, WORD height, WORD depth)
{
   Object *saver, *output, *memory_stream, *video_filter, *pngencoder;
   QUAD slen = width * height * depth;
   BOOL retval = FALSE;

   debug_print("%s (%d)\n", __func__, __LINE__);

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
   debug_print("%s (%d)\n", __func__, __LINE__);

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
   data->Iconified = FALSE;

   data->BitMap = NULL;
   data->Buffer = NULL;
   data->XOffset = 0;
   data->YOffset = 0;
   data->render_state = MUIV_FlushClearScreen;

   data->WinWidth = DEFAULT_GFX_WIDTH;
   data->WinHeight = DEFAULT_GFX_HEIGHT;
   data->ScrWidth = DEFAULT_GFX_WIDTH;
   data->ScrHeight = DEFAULT_GFX_HEIGHT;
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
   debug_print("%s (%d)\n", __func__, __LINE__);

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
               if (tag->ti_Data == MUIV_ShowPointer)
               {
                  if (!data->showpointer)
                  {
                     SetWindowPointer(data->window, WA_PointerType, POINTERTYPE_NORMAL, WM_ObtainEvents, TRUE, TAG_DONE);
                     data->showpointer = TRUE;
                  }
               }
               else if (tag->ti_Data == MUIV_HidePointer)
               {
                  if (data->showpointer)
                  {
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
               if (tag->ti_Data == MUIV_Reset_UserSelect)
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
                                                SA_Title,         "MorphUAE Screen",
                                                SA_LikeWorkbench, TRUE,
                                                SA_Quiet,         TRUE,
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
                                 //MUIA_Window_Width,      MUIV_Window_Width_Screen(100),   // Test
                                 //MUIA_Window_Height,     MUIV_Window_Height_Screen(100),  // Test
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

                  debug_print("%s (%d) - PixFMT = %d, DEPTH = %d\n", __func__, __LINE__, pixfmt, data->Depth);

                  switch (pixfmt)
                  {
                     case PIXFMT_RGB15PC:
                        byte_swap = TRUE;
                     case PIXFMT_RGB15:
                        redbits  = 5;  greenbits  = 5; bluebits  = 5;
                        redshift = 10; greenshift = 5; blueshift = 0;
                        break;
                     case PIXFMT_RGB16PC:
                        byte_swap = TRUE;
                     case PIXFMT_RGB16:
                        redbits  = 5;  greenbits  = 6;  bluebits  = 5;
                        redshift = 11; greenshift = 5;  blueshift = 0;
                        break;
                     case PIXFMT_RGBA32:
                        redbits  = 8;  greenbits  = 8;  bluebits  = 8;
                        redshift = 24; greenshift = 16; blueshift = 8;
                        break;
                     case PIXFMT_BGRA32: // //RGBA
                        redbits  = 8;  greenbits  = 8;  bluebits  = 8;
                        //redshift = 8;  greenshift = 16; blueshift = 24;
                        redshift = 16;  greenshift = 8; blueshift = 0;
                        break;
                     case PIXFMT_ARGB32:
                        redbits  = 8;  greenbits  = 8;  bluebits  = 8;
                        redshift = 16; greenshift = 8;  blueshift = 0;
                        break;
                     default:
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
               }
               else if (tag->ti_Data == MUIV_Iconified)
               {
                  data->Iconified = TRUE;
               }
               else if (tag->ti_Data == MUIV_UnIconified)
               {
                 if (uae_get_state() == UAE_STATE_PAUSED)
                 {
                    MUI_Redraw(obj_rendermcc, MADF_DRAWUPDATE);
                 }
                 data->Iconified = FALSE;
               } break;
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
                  int numunits = 0;
                  DoMethod(app, MUIM_Application_Save, MUIV_Application_Save_ENV);
                  setup_generic();
                  uae_restarted = TRUE;

                  set(win_settings, MUIA_Window_Open, FALSE);
                  uae_restart (-1, NULL);
               }
               else if (tag->ti_Data == MUIV_Settings_SaveUse)
               {
                  int numunits = 0;
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

            case MUIA_Runtime_Port0 :
            {
               LONG tmpp = tag->ti_Data;
               changed_prefs.jport0 = (tmpp == 0 ? 200 : tmpp == 1 ? 100 : 101);
               inputdevice_updateconfig (&changed_prefs);
            } break;

            case MUIA_Runtime_Port1 :
            {
               LONG tmpp = tag->ti_Data;
               changed_prefs.jport1 = (tmpp == 0 ? 200 : tmpp == 1 ? 100 : 101);
               inputdevice_updateconfig (&changed_prefs);
            } break;

            case MUIA_Control_UAE :
               if (tag->ti_Data == MUIV_Control_UAE_Toggle)
               {
                  if (uae_get_state() == UAE_STATE_PAUSED)
                  {
                     uae_resume();

                     if (tmpdata)
                        FreeVec(tmpdata);

                     tmpdata = NULL;

                     set(win_main, MUIA_Window_Title, "MorphUAE");
                  }
                  else
                  {
                     uae_pause();

                     tmpdata = AllocVec(_width(obj)*_height(obj)*4, MEMF_ANY);

                     if (tmpdata)
                     {
                           lock = LockIBase(0);
                           ReadPixelArray(tmpdata, 0, 0, _width(obj)*4, _rp(obj), _left(obj), _top(obj), _width(obj), _height(obj), RECTFMT_ARGB);
                           UnlockIBase (lock);
                     }

                     set(win_main, MUIA_Window_Title, Locale_GetString(MSG_EMULATION_PAUSED));
                  }
               }
               else if (tag->ti_Data == MUIV_Control_UAE_Pause)
               {
                  if (data->Active)
                     uae_pause();
                  data->Active = FALSE;
                  MUI_Request(NULL, NULL, 0L, Locale_GetString(MSG_REQUESTER_TITLE), Locale_GetString(MSG_REQUESTER_BUTTON), Locale_GetString(MSG_REQUESTER_TEXT));
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

   debug_print("%s (%d)\n", __func__, __LINE__);

   if (!DoSuperMethodA(cl, obj, msg))
      return(FALSE);

   data->screen = _screen(obj);
   data->window = _window(obj);

   data->ScrWidth  = data->screen->Width;
   data->ScrHeight = data->screen->Height;

   // IDCMP_DELTAMOVE

   data->eh.ehn_Object = obj;
   data->eh.ehn_Class  = cl;
   data->eh.ehn_Events = IDCMP_MOUSEBUTTONS|IDCMP_RAWKEY|IDCMP_MOUSEMOVE;
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
   return(DoSuperMethodA(cl, obj, (Msg)msg));
}
/*=*/

static ULONG Render_Askminmax(struct IClass *cl, Object *obj, struct MUIP_AskMinMax *msg)
{
   struct RenderData *data = (struct RenderData *)INST_DATA(cl, obj);

   debug_print("%s (%d) FS = %d\n", __func__, __LINE__, data->FullScreen);

   DoSuperMethodA(cl, obj, (Msg)msg);

   if ((data->screen = _screen(obj)) != NULL)
   {
      if (data->FullScreen)
      {
         msg->MinMaxInfo->MinWidth  += data->ScrWidth;
         msg->MinMaxInfo->DefWidth  += data->ScrWidth;
         msg->MinMaxInfo->MinHeight += data->ScrHeight;
         msg->MinMaxInfo->DefHeight += data->ScrHeight;
         msg->MinMaxInfo->MaxWidth  += data->ScrWidth;
         msg->MinMaxInfo->MaxHeight += data->ScrHeight;
      }
      else
      {
      //debug_print("%s (%d) : FS = %d - %d x %d : %d x %d\n", __func__, __LINE__, data->FullScreen, data->ScrWidth, data->ScrHeight, currprefs.gfx_width_win, currprefs.gfx_height_win);
         msg->MinMaxInfo->MinWidth  += DEFAULT_GFX_WIDTH;
         msg->MinMaxInfo->DefWidth  += DEFAULT_GFX_WIDTH;
         msg->MinMaxInfo->MinHeight += DEFAULT_GFX_HEIGHT;
         msg->MinMaxInfo->DefHeight += DEFAULT_GFX_HEIGHT;
         msg->MinMaxInfo->MaxWidth  += DEFAULT_GFX_WIDTH;
         msg->MinMaxInfo->MaxHeight += DEFAULT_GFX_HEIGHT;
      }
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
            {
               ScalePixelArrayAlpha(data->Buffer, data->WinWidth, data->WinHeight, tmp_gfxinfo->rowbytes, _rp(obj), 0, data->YOffset + tmp_first_line, tmp_gfxinfo->width, tmp_last_line - tmp_first_line + 1, 0xffffffff);//RECTFMT_ARGB); //0xFFFFFFFF);
            }
         }
         else if (data->render_state == MUIV_ScreenShoot)
         {
            ULONG slock;
            UBYTE *sdata = NULL;

            sdata = AllocVec(_width(obj)*_height(obj)*4, MEMF_ANY);

            if (sdata)
            {
               slock = LockIBase(0);
               ReadPixelArray(sdata, 0, 0, _width(obj)*4, _rp(obj), _left(obj), _top(obj), _width(obj), _height(obj), RECTFMT_ARGB);
               Save_Reggae("RAM:MorphUAE-Screenshoot.png", sdata, _width(obj), _height(obj), 4);
               UnlockIBase (slock);
               FreeVec(sdata);
               sdata = NULL;
            }
            data->render_state = 0;
         }
         else
            //FillPixelArray(rp, render_left, render_top, render_right, render_bottom-render_top, 0x00000000);
            return(0);
      }
      else if ((msg->flags & MADF_DRAWUPDATE))
         if (data->Iconified)
         {
            WritePixelArray(tmpdata, 0, 0, _width(obj)*4, _rp(obj), _left(obj), _top(obj), _width(obj), _mbottom(obj)-_mtop(obj)+1, RECTFMT_ARGB);
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
   debug_print("%s (%d)\n", __func__, __LINE__);
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
   debug_print("%s (%d)\n", __func__, __LINE__);

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

            //#define DEFAULT_GFX_WIDTH 640
            //#define DEFAULT_GFX_HEIGHT 512

            //data->MouseX = msg->imsg->MouseX;  // Test
            //data->MouseY = msg->imsg->MouseY;  // Test
/*
            // Begin Test

            //setmousestate (0, 0, data->MouseX, 1); // Test
            //setmousestate (0, 1, data->MouseY, 1); // Test

            if (_isinobject(dmx, dmy))
            {
                  setmousestate (0, 0, msg->imsg->MouseX, 1); //dmx
                  setmousestate (0, 1, msg->imsg->MouseY, 1); //dmy
            }
            else
            {
               if (msg->imsg->MouseX < 0)
                  setmousestate(0, 0, 0, 1);
               if (msg->imsg->MouseX > 639)
                  setmousestate(0, 0, 639, 1);

               if (msg->imsg->MouseY < _mtop(obj))
                  setmousestate(0, 1, _mtop(obj), 1);
               if (msg->imsg->MouseY > _mtop(obj)+511)
                  setmousestate(0, 1, _mtop(obj)+511, 1);

               //dmx = (dmx < 0 ? 1 : dmx > 639 ? 639 : dmx);
               //dmy = (dmy < 0 ? 1 : dmy > 511+_mtop(obj) ? 511+_mtop(obj) : dmy);
               //setmousestate (0, 0, dmx, 1); //dmx
               //setmousestate (0, 1, dmy, 1); //dmy
            }
            debug_print("%s (%d) - OLD XPOS : %d  YPOS : YPOS : %d\n", __func__, __LINE__, msg->imsg->MouseX, msg->imsg->MouseY);

            // End Test
*/
            //debug_print("%s (%d) - NEW XPOS : %d  YPOS : YPOS : %d\n", __func__, __LINE__, msg->imsg->IDCMPWindow->MouseX, msg->imsg->IDCMPWindow->MouseY);
            //debug_print("%s (%d) - OLD XPOS : %d  YPOS : YPOS : %d\n", __func__, __LINE__, msg->imsg->MouseX, msg->imsg->MouseY);

            if (_isinobject(data->MouseX, data->MouseY))
            {
               //if (data->MouseX < 0) data->MouseX = 0;
               //if (data->MouseY < 0) data->MouseY = 0;
               //if (data->MouseX >= 640) data->MouseX = 639;
               //if (data->MouseY >= 512) data->MouseY = 511;

               //debug_print("%s (%d) - INSIDE XPOS : %d  YPOS : %d\n", __func__, __LINE__, data->MouseX, data->MouseY);

               if(data->showpointer)
               {
                  set(obj_rendermcc, MUIA_Pointer_State, MUIV_HidePointer);
                  //data->MouseX = data->OldX;
                  //data->MouseY = data->OldY;
                  //data->OldX = data->MouseX;
                  //data->OldY = data->MouseY;

                  setmousestate (0, 0, data->MouseX, 1); //dmx
                  setmousestate (0, 1, data->MouseY, 1); //dmy
               }
               setmousestate (0, 0, data->MouseX, 1);
               setmousestate (0, 1, data->MouseY, 1);
            }
            else
            {
                  //setmousestate (0, 0, data->OldX, 1);
                  //setmousestate (0, 1, data->OldY, 1);

               //debug_print("%s (%d) - OUTSIDE XPOS : %d  YPOS : %d\n", __func__, __LINE__, data->MouseX, data->MouseY);

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

static int mui_setup_window(void)
{
   debug_print("%s (%d)\n", __func__, __LINE__);

   app = ApplicationObject,MUIA_Application_Title          , "MorphUAE",
                           MUIA_Application_Version        , MVERSTAG,
                           MUIA_Application_Copyright      , "OnyxSoft",
                           MUIA_Application_Author         , "Stefan Blixth",
                           MUIA_Application_Description    , Locale_GetString(MSG_APPLICATION_DESCRIPTION),
                           MUIA_Application_Base           , "MorphUAE",
                           MUIA_Application_DiskObject     , morphuae_icon,
                           MUIA_Application_HelpFile       , "PROGDIR:MorphUAE.guide",
                           MUIA_Application_SingleTask     , TRUE,
                           MUIA_Application_UseCommodities , TRUE,

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
                                       MUIA_Frame, MUIV_Frame_Button,
                                       MUIA_InputMode, MUIV_InputMode_RelVerify,
                                       MUIA_ShortHelp, Locale_GetString(MSG_SHORTHELP_SETTINGS),
                                       MUIA_Rawimage_Data, gfx_tools,
                                    End,

                                    Child, btn_reset = RawimageObject,
                                       MUIA_DoubleBuffer, 0,
                                       MUIA_InnerLeft, 0, MUIA_InnerRight, 0, MUIA_InnerTop, 0, MUIA_InnerBottom, 0,
                                       MUIA_Frame, MUIV_Frame_Button,
                                       MUIA_InputMode, MUIV_InputMode_RelVerify,
                                       MUIA_ShortHelp, Locale_GetString(MSG_SHORTHELP_RESET),
                                       MUIA_Rawimage_Data, gfx_clockwise,
                                    End,

                                    Child, btn_fullscreen = RawimageObject,
                                       MUIA_DoubleBuffer, 0,
                                       MUIA_InnerLeft, 0, MUIA_InnerRight, 0, MUIA_InnerTop, 0, MUIA_InnerBottom, 0,
                                       MUIA_Frame, MUIV_Frame_Button,
                                       MUIA_InputMode, MUIV_InputMode_RelVerify,
                                       MUIA_ShortHelp, Locale_GetString(MSG_SHORTHELP_FULLSCREEN),
                                       MUIA_Rawimage_Data, gfx_fullscreen,
                                    End,

                                    Child, btn_camera = RawimageObject,
                                       MUIA_DoubleBuffer, 0,
                                       MUIA_InnerLeft, 0, MUIA_InnerRight, 0, MUIA_InnerTop, 0, MUIA_InnerBottom, 0,
                                       MUIA_Frame, MUIV_Frame_Button,
                                       MUIA_InputMode, MUIV_InputMode_RelVerify,
                                       MUIA_ShortHelp, Locale_GetString(MSG_SHORTHELP_SCREENSHOT),
                                       MUIA_Rawimage_Data, gfx_camera,
                                    End,

                                    Child, btn_pauseresume = RawimageObject,
                                       MUIA_DoubleBuffer, 0,
                                       MUIA_InnerLeft, 0, MUIA_InnerRight, 0, MUIA_InnerTop, 0, MUIA_InnerBottom, 0,
                                       MUIA_Frame, MUIV_Frame_Button,
                                       MUIA_InputMode, MUIV_InputMode_RelVerify,
                                       MUIA_ShortHelp, Locale_GetString(MSG_SHORTHELP_PAUSERESUME),
                                       MUIA_Rawimage_Data, gfx_resume,
                                    End,

                                    Child, btn_about = RawimageObject,
                                       MUIA_DoubleBuffer, 0,
                                       MUIA_InnerLeft, 0, MUIA_InnerRight, 0, MUIA_InnerTop, 0, MUIA_InnerBottom, 0,
                                       MUIA_Frame, MUIV_Frame_Button,
                                       MUIA_InputMode, MUIV_InputMode_RelVerify,
                                       MUIA_ShortHelp, Locale_GetString(MSG_SHORTHELP_ABOUT),
                                       MUIA_Rawimage_Data, gfx_about,
                                    End,

                                    Child, HVSpace,
                                    Child, Label1("0 :"), Child, but_tmp_joy0 = CycleObject, MUIA_Cycle_Entries, cyc_list_jport, MUIA_ObjectID, ID_PRFS_GEN_JOY0, MUIA_UserData, ID_PRFS_GEN_JOY0, End,
                                    Child, Label1("1 :"), Child, but_tmp_joy1 = CycleObject, MUIA_Cycle_Entries, cyc_list_jport, MUIA_ObjectID, ID_PRFS_GEN_JOY1, MUIA_UserData, ID_PRFS_GEN_JOY1, End,
                                    Child, HVSpace,
                                    Child, btn_eject = RawimageObject,
                                       MUIA_DoubleBuffer, 0,
                                       MUIA_InnerLeft, 0, MUIA_InnerRight, 0, MUIA_InnerTop, 0, MUIA_InnerBottom, 0,
                                       MUIA_Frame, MUIV_Frame_Button,
                                       MUIA_InputMode, MUIV_InputMode_RelVerify,
                                       MUIA_ShortHelp, Locale_GetString(MSG_SHORTHELP_EJECT),
                                       MUIA_Rawimage_Data, gfx_eject,
                                    End,
                                    Child, obj_LEDmcc[0] = NewObject(LED_mcc[0]->mcc_Class, NULL, MUIA_Frame, MUIV_Frame_Button, MUIA_InnerLeft, 0, MUIA_InnerRight, 0, MUIA_InnerTop, 0, MUIA_InnerBottom, 0, MUIA_ShortHelp, Locale_GetString(MSG_SHORTHELP_DF0), TAG_DONE),
                                    Child, obj_LEDmcc[1] = NewObject(LED_mcc[1]->mcc_Class, NULL, MUIA_Frame, MUIV_Frame_Button, MUIA_InnerLeft, 0, MUIA_InnerRight, 0, MUIA_InnerTop, 0, MUIA_InnerBottom, 0, MUIA_ShortHelp, Locale_GetString(MSG_SHORTHELP_DF1), TAG_DONE),
                                    Child, obj_LEDmcc[2] = NewObject(LED_mcc[2]->mcc_Class, NULL, MUIA_Frame, MUIV_Frame_Button, MUIA_InnerLeft, 0, MUIA_InnerRight, 0, MUIA_InnerTop, 0, MUIA_InnerBottom, 0, MUIA_ShortHelp, Locale_GetString(MSG_SHORTHELP_DF2), TAG_DONE),
                                    Child, obj_LEDmcc[3] = NewObject(LED_mcc[3]->mcc_Class, NULL, MUIA_Frame, MUIV_Frame_Button, MUIA_InnerLeft, 0, MUIA_InnerRight, 0, MUIA_InnerTop, 0, MUIA_InnerBottom, 0, MUIA_ShortHelp, Locale_GetString(MSG_SHORTHELP_DF3), TAG_DONE),
                                 End,
                              End,
                           End,

                           SubWindow, win_about = WindowObject,
                              MUIA_Frame,                 MUIV_Frame_Window,
                              MUIA_Window_Borderless,     FALSE,
                              MUIA_Window_CloseGadget,    TRUE,
                              MUIA_Window_DepthGadget,    TRUE,
                              MUIA_Window_SizeGadget,     TRUE,
                              MUIA_Window_DragBar,        TRUE,
                              MUIA_Window_Title,          Locale_GetString(MSG_ABOUT_WINDOWTITLE),
                              MUIA_Window_ID,             MAKE_ID('A','U','A','E'),
                              MUIA_Window_AppWindow,      FALSE,

                              WindowContents, VGroup,
                                 Child, HVSpace,
                                 Child, RawimageObject,
                                    MUIA_DoubleBuffer, 0,
                                    MUIA_Frame, MUIV_Frame_None,
                                    MUIA_Rawimage_Data, small_logo,
                                 End,
                                 Child, TextObject, NoFrame,
                                    MUIA_Text_PreParse, "\33c \33b",
                                    MUIA_Text_Contents, ABOUTSTR,
                                 End,
                                 Child, TextObject, NoFrame,
                                    MUIA_Text_PreParse, "\33n \33c",
                                    MUIA_Text_Contents, Locale_GetString(MSG_ABOUTSTR),
                                 End,
                                 Child, VSpace(0),
                                 Child, TextObject, NoFrame,
                                    MUIA_Text_PreParse, "\33c",
                                    MUIA_Text_Contents, Locale_GetString(MSG_ABOUT_COPYRIGHT),
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
                              MUIA_Window_Title,          Locale_GetString(MSG_SETTINGS_WINDOWTITLE),
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
                                              MUIA_Text_Contents, Locale_GetString(MSG_SETTINGS_RESETTODEFAULT),
                                              MUIA_InputMode, MUIV_InputMode_RelVerify,
                                           End,
                                           Child, HSpace(0),
                                           Child, VSpace(0), Child, VSpace(0),
                                           Child, Label1(Locale_GetString(MSG_SETTINGS_MACHINETYPE)), Child, but_gen_machine = CycleObject, MUIA_Cycle_Entries, cyc_gen_machine, MUIA_ObjectID, ID_PRFS_GEN_MACHINE, MUIA_UserData, ID_PRFS_GEN_MACHINE, End,
                                           Child, VSpace(0), Child, VSpace(0),
                                           Child, RectangleObject, MUIA_Rectangle_HBar, TRUE, MUIA_FixHeight, 8, End,
                                           Child, RectangleObject, MUIA_Rectangle_HBar, TRUE, MUIA_Rectangle_BarTitle, Locale_GetString(MSG_SETTINGS_SOUNDTITLE), MUIA_FixHeight, 8, End,
                                           Child, Label1(Locale_GetString(MSG_SETTINGS_SOUNDOUTPUT)), Child, but_gen_sound = CycleObject, MUIA_Cycle_Entries, cyc_list_sndout, MUIA_ObjectID, ID_PRFS_GEN_SOUND, MUIA_UserData, ID_PRFS_GEN_SOUND, End,
                                           Child, Label1(Locale_GetString(MSG_SETTINGS_SOUNDCHANNELS)), Child, but_gen_channels = CycleObject, MUIA_Cycle_Entries, cyc_list_sndchan, MUIA_ObjectID, ID_PRFS_GEN_CHANNELS, MUIA_UserData, ID_PRFS_GEN_CHANNELS, End,
                                           Child, Label1(Locale_GetString(MSG_SETTINGS_SOUNDFREQ)), Child, but_gen_frequency = CycleObject, MUIA_Cycle_Entries, cyc_gen_frequency, MUIA_ObjectID, ID_PRFS_GEN_FREQUENCY, MUIA_UserData, ID_PRFS_GEN_FREQUENCY, End,
                                           Child, RectangleObject, MUIA_Rectangle_HBar, TRUE, MUIA_FixHeight, 8, End,
                                           Child, RectangleObject, MUIA_Rectangle_HBar, TRUE, MUIA_Rectangle_BarTitle, Locale_GetString(MSG_SETTINGS_IOTITLE), MUIA_FixHeight, 8, End,
                                           Child, Label1(Locale_GetString(MSG_SETTINGS_IOJOY0)), Child, but_gen_joy0 = CycleObject, MUIA_Cycle_Entries, cyc_list_jport, MUIA_ObjectID, ID_PRFS_GEN_JOY0, MUIA_UserData, ID_PRFS_GEN_JOY0, End,
                                           Child, Label1(Locale_GetString(MSG_SETTINGS_IOJOY1)), Child, but_gen_joy1 = CycleObject, MUIA_Cycle_Entries, cyc_list_jport, MUIA_ObjectID, ID_PRFS_GEN_JOY1, MUIA_UserData, ID_PRFS_GEN_JOY1, End,
                                           Child, Label1(Locale_GetString(MSG_SETTINGS_IOFLOPPY)), Child, but_gen_floppy = CycleObject, MUIA_Cycle_Entries, cyc_list_floppy, MUIA_ObjectID, ID_PRFS_GEN_FLOPPY, MUIA_UserData, ID_PRFS_GEN_FLOPPY, End,
                                           Child, Label1(Locale_GetString(MSG_SETTINGS_IOKEYBOARD)), Child, but_gen_language = CycleObject, MUIA_Cycle_Entries, cyc_list_keys, MUIA_ObjectID, ID_PRFS_GEN_LANGUAGE, MUIA_UserData, ID_PRFS_GEN_LANGUAGE, End,
                                           Child, RectangleObject, MUIA_Rectangle_HBar, TRUE, MUIA_FixHeight, 8, End,
                                           Child, RectangleObject, MUIA_Rectangle_HBar, TRUE, MUIA_Rectangle_BarTitle, Locale_GetString(MSG_SETTINGS_GFXTITLE), MUIA_FixHeight, 8, End,
                                           Child, Label1(Locale_GetString(MSG_SETTINGS_GFXBLITS)), Child, but_gen_blitter = CycleObject, MUIA_Cycle_Entries, cyc_list_blits, MUIA_ObjectID, ID_PRFS_GEN_BLITTER, MUIA_UserData, ID_PRFS_GEN_BLITTER, End,
                                           Child, Label1(Locale_GetString(MSG_SETTINGS_GFXSPRITE)), Child, but_gen_sprite = CycleObject, MUIA_Cycle_Entries, cyc_list_sprites, MUIA_ObjectID, ID_PRFS_GEN_SPRITE, MUIA_UserData, ID_PRFS_GEN_SPRITE, End,
                                           Child, Label1(Locale_GetString(MSG_SETTINGS_GFXFRAMES)), Child, but_gen_framerate = CycleObject, MUIA_Cycle_Entries, cyc_list_frames, MUIA_ObjectID, ID_PRFS_GEN_FRAMERATE, MUIA_UserData, ID_PRFS_GEN_FRAMERATE, End,
                                           Child, RectangleObject, MUIA_Rectangle_HBar, TRUE, MUIA_FixHeight, 8, End,
                                           Child, RectangleObject, MUIA_Rectangle_HBar, TRUE, MUIA_Rectangle_BarTitle, Locale_GetString(MSG_SETTINGS_MISC), MUIA_FixHeight, 8, End,
                                           Child, Label1(Locale_GetString(MSG_SETTINGS_RESETTYPE)), Child, but_gen_resetmode = CycleObject, MUIA_Cycle_Entries, cyc_list_reset, MUIA_ObjectID, ID_PRFS_GEN_RESETMODE, MUIA_UserData, ID_PRFS_GEN_RESETMODE, End,
                                           Child, VSpace(0), Child, VSpace(0),
                                        End,
                                     End,
                                     Child, HGroup,  // OCS Tab
                                        Child, ColGroup(2),
                                           Child, but_ocs_reset = TextObject, ButtonFrame,
                                              MUIA_Background, MUII_ButtonBack,
                                              MUIA_Weight, 0,
                                              MUIA_Text_PreParse, "\33c",
                                              MUIA_Text_Contents, Locale_GetString(MSG_SETTINGS_RESETTODEFAULT),
                                              MUIA_InputMode, MUIV_InputMode_RelVerify,
                                           End,
                                           Child, HSpace(0),
                                           Child, VSpace(0), Child, VSpace(0),
                                           Child, KeyLabel2(Locale_GetString(MSG_SETTINGS_KICKFILE),'f'),
                                           Child, cyc_ocs_kickstart = PopaslObject,
                                              MUIA_Popstring_String, ocs_kickstart_str = MyKeyString("PROGDIR:Kickstarts/Kick1.rom", 1023, NULL, ID_PRFS_OCS_KICKSTART, 100),
                                              MUIA_Popstring_Button, PopButton(MUII_PopFile),
                                              ASLFR_TitleText, Locale_GetString(MSG_SETTINGS_KICKFILEASL),
                                           End,
                                           Child, KeyLabel2(Locale_GetString(MSG_SETTINGS_KICKKEYFILE),'k'),
                                           Child, cyc_ocs_kickstartkey = PopaslObject,
                                              MUIA_Popstring_String, ocs_kickstartkey_str = MyKeyString("PROGDIR:Kickstarts/rom.key", 1023, NULL, ID_PRFS_OCS_KICKSTARTKEY, 100),
                                              MUIA_Popstring_Button, PopButton(MUII_PopFile),
                                              ASLFR_TitleText, Locale_GetString(MSG_SETTINGS_KICKKEYFILEASL),
                                           End,
                                           Child, Label1(Locale_GetString(MSG_SETTINGS_CHIPMEM)), Child, but_ocs_chipmem = CycleObject, MUIA_Cycle_Entries, cyc_ocs_chipmem, MUIA_ObjectID, ID_PRFS_OCS_CHIPMEM, MUIA_UserData, ID_PRFS_OCS_CHIPMEM, End,
                                           Child, Label1(Locale_GetString(MSG_SETTINGS_FASTMEM)), Child, but_ocs_fastmem = CycleObject, MUIA_Cycle_Entries, cyc_ocs_fastmem, MUIA_ObjectID, ID_PRFS_OCS_FASTMEM, MUIA_UserData, ID_PRFS_OCS_FASTMEM,  End,
                                           Child, VSpace(0), Child, VSpace(0),
                                        End,
                                     End,
                                     Child, HGroup,  // ECS Tab
                                        Child, ColGroup(2),
                                           Child, but_ecs_reset = TextObject, ButtonFrame,
                                              MUIA_Background, MUII_ButtonBack,
                                              MUIA_Weight, 0,
                                              MUIA_Text_PreParse, "\33c",
                                              MUIA_Text_Contents, Locale_GetString(MSG_SETTINGS_RESETTODEFAULT),
                                              MUIA_InputMode, MUIV_InputMode_RelVerify,
                                           End,
                                           Child, HSpace(0),
                                           Child, VSpace(0), Child, VSpace(0),
                                           Child, KeyLabel2(Locale_GetString(MSG_SETTINGS_KICKFILE),'f'),
                                           Child, cyc_ecs_kickstart = PopaslObject,
                                              MUIA_Popstring_String, ecs_kickstart_str = MyKeyString("PROGDIR:Kickstarts/Kick2.rom", 1023, NULL, ID_PRFS_ECS_KICKSTART, 100),
                                              MUIA_Popstring_Button, PopButton(MUII_PopFile),
                                              ASLFR_TitleText, Locale_GetString(MSG_SETTINGS_KICKFILEASL),
                                           End,
                                           Child, KeyLabel2(Locale_GetString(MSG_SETTINGS_KICKKEYFILE),'k'),
                                           Child, cyc_ecs_kickstartkey = PopaslObject,
                                              MUIA_Popstring_String, ecs_kickstartkey_str = MyKeyString("PROGDIR:Kickstarts/rom.key", 1023, NULL, ID_PRFS_ECS_KICKSTARTKEY, 100),
                                              MUIA_Popstring_Button, PopButton(MUII_PopFile),
                                              ASLFR_TitleText, Locale_GetString(MSG_SETTINGS_KICKKEYFILEASL),
                                           End,
                                           Child, Label1(Locale_GetString(MSG_SETTINGS_ECSMODE)), Child, but_ecs_mode = CycleObject, MUIA_Cycle_Entries, cyc_ecs_mode, MUIA_ObjectID, ID_PRFS_ECS_MODE, MUIA_UserData, ID_PRFS_ECS_MODE, End,
                                           Child, Label1(Locale_GetString(MSG_SETTINGS_CHIPMEM)), Child, but_ecs_chipmem = CycleObject, MUIA_Cycle_Entries, cyc_ecs_chipmem, MUIA_ObjectID, ID_PRFS_ECS_CHIPMEM, MUIA_UserData, ID_PRFS_ECS_CHIPMEM, End,
                                           Child, Label1(Locale_GetString(MSG_SETTINGS_FASTMEM)), Child, but_ecs_fastmem = CycleObject, MUIA_Cycle_Entries, cyc_ecs_fastmem, MUIA_ObjectID, ID_PRFS_ECS_FASTMEM, MUIA_UserData, ID_PRFS_ECS_FASTMEM, End,
                                           Child, VSpace(0), Child, VSpace(0),
                                        End,
                                     End,
                                     Child, HGroup,  // AGA Tab
                                        Child, ColGroup(2),
                                           Child, but_aga_reset = TextObject, ButtonFrame,
                                              MUIA_Background, MUII_ButtonBack,
                                              MUIA_Weight, 0,
                                              MUIA_Text_PreParse, "\33c",
                                              MUIA_Text_Contents, Locale_GetString(MSG_SETTINGS_RESETTODEFAULT),
                                              MUIA_InputMode, MUIV_InputMode_RelVerify,
                                           End,
                                           Child, HSpace(0),
                                           Child, VSpace(0), Child, VSpace(0),
                                           Child, KeyLabel2(Locale_GetString(MSG_SETTINGS_KICKFILE),'f'),
                                           Child, cyc_aga_kickstart = PopaslObject,
                                              MUIA_Popstring_String, aga_kickstart_str = MyKeyString("PROGDIR:Kickstarts/Kick3.rom", 1023, NULL, ID_PRFS_AGA_KICKSTART, 100),
                                              MUIA_Popstring_Button, PopButton(MUII_PopFile),
                                              ASLFR_TitleText, Locale_GetString(MSG_SETTINGS_KICKFILEASL),
                                           End,
                                           Child, KeyLabel2(Locale_GetString(MSG_SETTINGS_KICKKEYFILE),'k'),
                                           Child, cyc_aga_kickstartkey = PopaslObject,
                                              MUIA_Popstring_String, aga_kickstartkey_str = MyKeyString("PROGDIR:Kickstarts/rom.key", 1023, NULL, ID_PRFS_AGA_KICKSTARTKEY, 100),
                                              MUIA_Popstring_Button, PopButton(MUII_PopFile),
                                              ASLFR_TitleText, Locale_GetString(MSG_SETTINGS_KICKKEYFILEASL),
                                           End,
                                           Child, Label1(Locale_GetString(MSG_SETTINGS_FASTMEM)), Child, but_aga_fastmem = CycleObject, MUIA_Cycle_Entries, cyc_aga_fastmem, MUIA_ObjectID, ID_PRFS_AGA_FASTMEM, MUIA_UserData, ID_PRFS_AGA_FASTMEM, End,
                                           Child, VSpace(0), Child, VSpace(0),
                                        End,
                                     End,
                                     Child, HGroup,  // Custom Tab
                                        Child, ColGroup(2),
                                           Child, but_cus_reset = TextObject, ButtonFrame,
                                              MUIA_Background, MUII_ButtonBack,
                                              MUIA_Weight, 0,
                                              MUIA_Text_PreParse, "\33c",
                                              MUIA_Text_Contents, Locale_GetString(MSG_SETTINGS_RESETTODEFAULT),
                                              MUIA_InputMode, MUIV_InputMode_RelVerify,
                                           End,
                                           Child, HSpace(0),
                                           Child, VSpace(0), Child, VSpace(0),
                                           Child, KeyLabel2(Locale_GetString(MSG_SETTINGS_KICKFILE),'f'),
                                           Child, cyc_cus_kickstart = PopaslObject,
                                              MUIA_Popstring_String, cus_kickstart_str = MyKeyString("PROGDIR:Kickstarts/Kick3.rom", 1023, NULL, ID_PRFS_CUS_KICKSTART, 100),
                                              MUIA_Popstring_Button, PopButton(MUII_PopFile),
                                              ASLFR_TitleText, Locale_GetString(MSG_SETTINGS_KICKFILEASL),
                                           End,
                                           Child, KeyLabel2(Locale_GetString(MSG_SETTINGS_KICKKEYFILE),'k'),
                                           Child, cyc_cus_kickstartkey = PopaslObject,
                                              MUIA_Popstring_String, cus_kickstartkey_str = MyKeyString("PROGDIR:Kickstarts/rom.key", 1023, NULL, ID_PRFS_CUS_KICKSTARTKEY, 100),
                                              MUIA_Popstring_Button, PopButton(MUII_PopFile),
                                              ASLFR_TitleText, Locale_GetString(MSG_SETTINGS_KICKKEYFILEASL),
                                           End,
                                           Child, KeyLabel2(Locale_GetString(MSG_SETTINGS_VHD1),'v'),
                                           Child, HGroup,
                                              Child, chk_harddisk1 = MUICreateCheckbox(FALSE, ID_PRFS_CUS_USEVHD1),
                                              Child, cyc_cus_harddisk1 = PopaslObject,
                                                 MUIA_Popstring_String, cus_harddisk1_str = MyKeyString("PROGDIR:Harddisks/", 1023, NULL, ID_PRFS_CUS_HARDDISK1, 100),
                                                 MUIA_Popstring_Button, PopButton(MUII_PopDrawer),
                                                 MUIA_Disabled, TRUE,
                                                 ASLFR_TitleText, Locale_GetString(MSG_SETTINGS_VHDPATH),
                                              End,
                                           End,
                                           Child, KeyLabel2(" ",'q'), //HSpace(0),
                                           Child, grp_cus_harddisk1 = HGroup,
                                              Child, KeyLabel2(Locale_GetString(MSG_SETTINGS_DEVICENAME),'d'),
                                              Child, cus_devname1_str = MyKeyString("DH0", 32, NULL, ID_PRFS_CUS_DEVNAME1, 20),
                                              Child, KeyLabel2(Locale_GetString(MSG_SETTINGS_VOLUMENAME),'v'),
                                              Child, cus_volname1_str = MyKeyString("System", 32, NULL, ID_PRFS_CUS_VOLNAME1, 80),
                                              MUIA_Disabled, TRUE,
                                           End,
                                           Child, KeyLabel2(Locale_GetString(MSG_SETTINGS_VHD2),'v'),
                                           Child, HGroup,
                                              Child, chk_harddisk2 = MUICreateCheckbox(FALSE, ID_PRFS_CUS_USEVHD2),
                                              Child, cyc_cus_harddisk2 = PopaslObject,
                                                 MUIA_Popstring_String, cus_harddisk2_str = MyKeyString("PROGDIR:Harddisks/", 1023, NULL, ID_PRFS_CUS_HARDDISK2, 100),
                                                 MUIA_Popstring_Button, PopButton(MUII_PopDrawer),
                                                 MUIA_Disabled, TRUE,
                                                 ASLFR_TitleText, Locale_GetString(MSG_SETTINGS_VHDPATH),
                                              End,
                                           End,
                                           Child, KeyLabel2(" ",'q'), //HSpace(0),
                                           Child, grp_cus_harddisk2 = HGroup,
                                              Child, KeyLabel2(Locale_GetString(MSG_SETTINGS_DEVICENAME),'d'),
                                              Child, cus_devname2_str = MyKeyString("DH1", 32, NULL, ID_PRFS_CUS_DEVNAME2, 20),
                                              Child, KeyLabel2(Locale_GetString(MSG_SETTINGS_VOLUMENAME),'v'),
                                              Child, cus_volname2_str = MyKeyString("Work", 32, NULL, ID_PRFS_CUS_VOLNAME2, 80),
                                              MUIA_Disabled, TRUE,
                                           End,
                                           Child, Label1(Locale_GetString(MSG_SETTINGS_CPU)), Child, but_cus_cpu = CycleObject, MUIA_Cycle_Entries, cyc_cus_cpu, MUIA_ObjectID, ID_PRFS_CUS_CPU, MUIA_UserData, ID_PRFS_CUS_CPU, End,
                                           Child, Label1(Locale_GetString(MSG_SETTINGS_CPUSPEED)), Child, but_cus_speed = CycleObject, MUIA_Cycle_Entries, cyc_list_speed, MUIA_ObjectID, ID_PRFS_CUS_SPEED, MUIA_UserData, ID_PRFS_CUS_SPEED, End,
                                           Child, Label1(Locale_GetString(MSG_SETTINGS_JIT)), Child, but_cus_jit = CycleObject, MUIA_Cycle_Entries, cyc_list_jit, MUIA_ObjectID, ID_PRFS_CUS_JIT, MUIA_UserData, ID_PRFS_CUS_JIT, End,
                                           Child, Label1(Locale_GetString(MSG_SETTINGS_CHIPSET)), Child, but_cus_chipset = CycleObject, MUIA_Cycle_Entries, cyc_cus_chipset, MUIA_ObjectID, ID_PRFS_CUS_CHIPSET, MUIA_UserData, ID_PRFS_CUS_CHIPSET, End,
                                           Child, Label1(Locale_GetString(MSG_SETTINGS_CHIPMEM)), Child, but_cus_chipmem = CycleObject, MUIA_Cycle_Entries, cyc_cus_chipmem, MUIA_ObjectID, ID_PRFS_CUS_CHIPMEM, MUIA_UserData, ID_PRFS_CUS_CHIPMEM, End,
                                           Child, Label1(Locale_GetString(MSG_SETTINGS_FASTMEM)), Child, but_cus_fastmem = CycleObject, MUIA_Cycle_Entries, cyc_cus_fastmem, MUIA_ObjectID, ID_PRFS_CUS_FASTMEM, MUIA_UserData, ID_PRFS_CUS_FASTMEM, End,
                                           Child, Label1(Locale_GetString(MSG_SETTINGS_ZORROMEM)), Child, but_cus_zorromem = CycleObject, MUIA_Cycle_Entries, cyc_cus_zorromem, MUIA_ObjectID, ID_PRFS_CUS_ZORROMEM, MUIA_UserData, ID_PRFS_CUS_ZORROMEM, End,
                                           Child, VSpace(0), Child, VSpace(0),
                                        End,
                                     End,
                                  End,
                                  Child, HGroup,
                                     Child, but_use = TextObject, ButtonFrame,
                                        MUIA_Background, MUII_ButtonBack,
                                        MUIA_Text_PreParse, "\33c",
                                        MUIA_Text_Contents, Locale_GetString(MSG_SETTINGS_USE),
                                        MUIA_InputMode, MUIV_InputMode_RelVerify,
                                     End,
                                     Child, but_saveuse = TextObject, ButtonFrame,
                                        MUIA_Background, MUII_ButtonBack,
                                        MUIA_Text_PreParse, "\33c",
                                        MUIA_Text_Contents, Locale_GetString(MSG_SETTINGS_SAVEANDUSE),
                                        MUIA_InputMode, MUIV_InputMode_RelVerify,
                                     End,
                                     Child, but_save = TextObject, ButtonFrame,
                                        MUIA_Background, MUII_ButtonBack,
                                        MUIA_Text_PreParse, "\33c",
                                        MUIA_Text_Contents, Locale_GetString(MSG_SETTINGS_SAVE),
                                        MUIA_InputMode, MUIV_InputMode_RelVerify,
                                     End,
                                     Child, but_cancel = TextObject, ButtonFrame,
                                        MUIA_Background, MUII_ButtonBack,
                                        MUIA_Text_PreParse, "\33c",
                                        MUIA_Text_Contents, Locale_GetString(MSG_SETTINGS_CANCEL),
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

   DoMethod(app, MUIM_Notify, MUIA_Application_Iconified, TRUE, obj_rendermcc, 3, MUIM_Set, MUIA_Cleanup_Gfx, MUIV_Iconified);
   DoMethod(app, MUIM_Notify, MUIA_Application_Iconified, FALSE, obj_rendermcc, 3, MUIM_Set, MUIA_Cleanup_Gfx, MUIV_UnIconified);
   DoMethod(app, MUIM_Notify, MUIA_Application_DoubleStart, TRUE, app, 3, MUIM_Set, MUIA_Application_Iconified, FALSE);

   DoMethod(but_tmp_joy0, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime, obj_rendermcc, 3, MUIM_Set, MUIA_Runtime_Port0, MUIV_TriggerValue);
   DoMethod(but_tmp_joy1, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime, obj_rendermcc, 3, MUIM_Set, MUIA_Runtime_Port1, MUIV_TriggerValue);

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

   DoMethod(btn_reset, MUIM_Notify, MUIA_Pressed, FALSE, obj_rendermcc, 3, MUIM_Set, MUIA_Reset_Type, MUIV_Reset_UserSelect);
   DoMethod(btn_eject, MUIM_Notify, MUIA_Pressed, FALSE, obj_rendermcc, 3, MUIM_Set, MUIA_Floppy_Hotkey, MUIV_HKTriggerEjectAll);
   DoMethod(btn_camera, MUIM_Notify, MUIA_Pressed, FALSE, obj_rendermcc, 3, MUIM_Set, MUIA_Render_State, MUIV_ScreenShoot);

   DoMethod(btn_fullscreen, MUIM_Notify, MUIA_Pressed, FALSE, obj_rendermcc, 3, MUIM_Set, MUIA_Display_Type, MUIV_Display_Toggle);
   DoMethod(btn_pauseresume, MUIM_Notify, MUIA_Pressed, FALSE, obj_rendermcc, 3, MUIM_Set, MUIA_Control_UAE, MUIV_Control_UAE_Toggle);

   DoMethod(win_main, MUIM_Notify, MUIA_Window_InputEvent, "alt return", obj_rendermcc, 3, MUIM_Set, MUIA_Display_Type, MUIV_Display_Toggle);

   DoMethod(win_main, MUIM_Notify, MUIA_Window_InputEvent, "ctrl alt t", obj_rendermcc, 3, MUIM_Set, MUIA_Toolbar_Active, MUIV_Toolbar_Toggle);

   DoMethod(win_main, MUIM_Notify, MUIA_Window_InputEvent, "ctrl alt p", obj_rendermcc, 3, MUIM_Set, MUIA_Control_UAE, MUIV_Control_UAE_Toggle);

   DoMethod(win_main, MUIM_Notify, MUIA_Window_InputEvent, "ctrl alt s", obj_rendermcc, 3, MUIM_Set, MUIA_Render_State, MUIV_ScreenShoot);

   DoMethod(btn_about, MUIM_Notify, MUIA_Pressed, FALSE, win_about, 3, MUIM_Set, MUIA_Window_Open, TRUE);
   DoMethod(win_about, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, win_about, 3, MUIM_Set, MUIA_Window_Open, FALSE);

   DoMethod(btn_settings, MUIM_Notify, MUIA_Pressed, FALSE, win_settings, 3, MUIM_Set, MUIA_Window_Open, TRUE);
   DoMethod(win_settings, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, win_settings, 3, MUIM_Set, MUIA_Window_Open, FALSE);
   DoMethod(but_gen_reset, MUIM_Notify, MUIA_Pressed, FALSE, obj_rendermcc, 3, MUIM_Set, MUIA_Settings_Adjust, MUIV_Reset_General);
   DoMethod(but_ocs_reset, MUIM_Notify, MUIA_Pressed, FALSE, obj_rendermcc, 3, MUIM_Set, MUIA_Settings_Adjust, MUIV_Reset_OCS);
   DoMethod(but_ecs_reset, MUIM_Notify, MUIA_Pressed, FALSE, obj_rendermcc, 3, MUIM_Set, MUIA_Settings_Adjust, MUIV_Reset_ECS);
   DoMethod(but_aga_reset, MUIM_Notify, MUIA_Pressed, FALSE, obj_rendermcc, 3, MUIM_Set, MUIA_Settings_Adjust, MUIV_Reset_AGA);
   DoMethod(but_cus_reset, MUIM_Notify, MUIA_Pressed, FALSE, obj_rendermcc, 3, MUIM_Set, MUIA_Settings_Adjust, MUIV_Reset_Custom);

   DoMethod(chk_harddisk1, MUIM_Notify, MUIA_Selected, TRUE, cyc_cus_harddisk1, 3, MUIM_Set, MUIA_Disabled, FALSE);
   DoMethod(chk_harddisk1, MUIM_Notify, MUIA_Selected, TRUE, grp_cus_harddisk1, 3, MUIM_Set, MUIA_Disabled, FALSE);
   DoMethod(chk_harddisk1, MUIM_Notify, MUIA_Selected, FALSE, cyc_cus_harddisk1, 3, MUIM_Set, MUIA_Disabled, TRUE);
   DoMethod(chk_harddisk1, MUIM_Notify, MUIA_Selected, FALSE, grp_cus_harddisk1, 3, MUIM_Set, MUIA_Disabled, TRUE);

   DoMethod(chk_harddisk2, MUIM_Notify, MUIA_Selected, TRUE, cyc_cus_harddisk2, 3, MUIM_Set, MUIA_Disabled, FALSE);
   DoMethod(chk_harddisk2, MUIM_Notify, MUIA_Selected, TRUE, grp_cus_harddisk2, 3, MUIM_Set, MUIA_Disabled, FALSE);
   DoMethod(chk_harddisk2, MUIM_Notify, MUIA_Selected, FALSE, cyc_cus_harddisk2, 3, MUIM_Set, MUIA_Disabled, TRUE);
   DoMethod(chk_harddisk2, MUIM_Notify, MUIA_Selected, FALSE, grp_cus_harddisk2, 3, MUIM_Set, MUIA_Disabled, TRUE);

   DoMethod(but_use, MUIM_Notify, MUIA_Pressed, FALSE, obj_rendermcc, 3, MUIM_Set, MUIA_Settings_Adjust, MUIV_Settings_Use);
   DoMethod(but_saveuse, MUIM_Notify, MUIA_Pressed, FALSE, obj_rendermcc, 3, MUIM_Set, MUIA_Settings_Adjust, MUIV_Settings_SaveUse);
   DoMethod(but_save, MUIM_Notify, MUIA_Pressed, FALSE, obj_rendermcc, 3, MUIM_Set, MUIA_Settings_Adjust, MUIV_Settings_Save);
   DoMethod(but_cancel, MUIM_Notify, MUIA_Pressed, FALSE, obj_rendermcc, 3, MUIM_Set, MUIA_Settings_Adjust, MUIV_Settings_Cancel);

   set(obj_rendermcc, MUIA_Settings_Adjust, MUIV_Reset_All);
   DoMethod(app, MUIM_Application_Load, MUIV_Application_Load_ENV);
   setup_generic();
   uae_restarted = TRUE;
   uae_restart (-1, NULL);

   set(win_main, MUIA_Window_Open, TRUE);

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

   Locale_Open("MorphUAE.catalog", 1, 0);
   Populate_CycleStrings();
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

int graphics_init(void)
{
   if (!uae_restarted)
   {
      gfxvidinfo.width  = DEFAULT_GFX_WIDTH;
      gfxvidinfo.height = DEFAULT_GFX_HEIGHT;

      gfxvidinfo.width += 7;
      gfxvidinfo.width &= ~7;

      if (!mui_setup_window ())
            return 0;

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
      if (tmpdata) FreeVec(tmpdata);

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
      Locale_Close();
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
   //read_mouse,
   //get_mouse_num,
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
