/*
->==================================================<-
->= MUI-Framework - © Copyright 2025-2026 OnyxSoft =<-
->==================================================<-
->= Version  : 1.0                                 =<-
->= File     : LEDmcc.h                            =<-
->= Author   : Stefan Blixth                       =<-
->= Compiled : 2026-02-01                          =<-
->==================================================<-
*/

#ifndef __LEDMCC_H__
#define __LEDMCC_H__

// Useful patches and functions...
#ifndef DISPATCHER
#define DISPATCHER(Name) \
static ULONG Name##_Dispatcher(void); \
struct EmulLibEntry GATE ##Name##_Dispatcher = { TRAP_LIB, 0, (void (*)(void)) Name##_Dispatcher }; \
static ULONG Name##_Dispatcher(void) { struct IClass *cl=(struct IClass*)REG_A0; Msg msg=(Msg)REG_A1; Object *obj=(Object*)REG_A2;
   #define DISPATCHER_REF(Name) &GATE##Name##_Dispatcher
   #define DISPATCHER_END }
   #endif

#ifdef USEDEBUG
  #include <clib/debug_protos.h>
  #define debug_print(args...) { KPrintF((CONST_STRPTR)args); }
#else
 #define debug_print(...)
#endif


#define SERIALNUMBER           (1)
#define TAGBASE_DEVELIN        (TAG_USER | (SERIALNUMBER<<16))

#define MUIA_LED_Colour        (TAGBASE_DEVELIN | 0x0001)
#define MUIA_LED_Size          (TAGBASE_DEVELIN | 0x0002)
#define MUIA_LED_Style         (TAGBASE_DEVELIN | 0x0003)

#define MUIV_LED_Size_Small    (TAGBASE_DEVELIN | 0x0004)
#define MUIV_LED_Size_Medium   (TAGBASE_DEVELIN | 0x0005)
#define MUIV_LED_Size_Big      (TAGBASE_DEVELIN | 0x0006)

#define MUIV_LED_Colour_Off    (TAGBASE_DEVELIN | 0x0007)
#define MUIV_LED_Colour_Green  (TAGBASE_DEVELIN | 0x0008)
#define MUIV_LED_Colour_Red    (TAGBASE_DEVELIN | 0x0009)
#define MUIV_LED_Colour_Blue   (TAGBASE_DEVELIN | 0x000a)
#define MUIV_LED_Colour_Yellow (TAGBASE_DEVELIN | 0x000b)

#define MUIV_LED_Colour_Blue1x (TAGBASE_DEVELIN | 0x000c)
#define MUIV_LED_Colour_Blue2x (TAGBASE_DEVELIN | 0x000d)


// Size and Style defines...
#define NUM_OF_LEDS 4
#define LED_SIZE_SMALL   7
#define LED_SIZE_MEDIUM  13
#define LED_SIZE_BIG     17
#define LED_SIZE_EXTREME 24

#define LED_SIZE_WIDTH  LED_SIZE_EXTREME //LED_SIZE_BIG
#define LED_SIZE_HEIGHT LED_SIZE_EXTREME //LED_SIZE_BIG

#define LED_Style_Round     1   // NA at the moment...
#define LED_Style_Square    2

#define LED_Colour_Red     0x00ff0000
#define LED_Colour_Green   0x0000ff00
#define LED_Colour_Blue    0x000000ff
#define LED_Colour_Yellow  0x00ffff00
#define LED_Colour_Idle    0x00000000

#define LED_Colour_Blue1x  0x007777cc   // For jPV =)
#define LED_Colour_Blue2x  0x00223388   // For jPV =)

#define Frame_Colour_Dark  0x00000000
#define Frame_Colour_Light 0x00ebebeb

struct LEDData
{
   BOOL   active;
   LONG   colour;
   LONG   size;
   LONG   style;

   /* Events */
   //struct MUI_EventHandlerNode ehnode;
};

unsigned long LED_gfx_red[LED_SIZE_WIDTH*LED_SIZE_HEIGHT];    // 0xAARRGGBB
unsigned long LED_gfx_green[LED_SIZE_WIDTH*LED_SIZE_HEIGHT];  // 0xAARRGGBB
unsigned long LED_gfx_blue[LED_SIZE_WIDTH*LED_SIZE_HEIGHT];   // 0xAARRGGBB
unsigned long LED_gfx_yellow[LED_SIZE_WIDTH*LED_SIZE_HEIGHT]; // 0xAARRGGBB
unsigned long LED_gfx_off[LED_SIZE_WIDTH*LED_SIZE_HEIGHT];    // 0xAARRGGBB

unsigned long LED_gfx_blue1x[LED_SIZE_WIDTH*LED_SIZE_HEIGHT];   // 0xAARRGGBB
unsigned long LED_gfx_blue2x[LED_SIZE_WIDTH*LED_SIZE_HEIGHT];   // 0xAARRGGBB


// Prototypes...
void Cleanup_LED(struct MUI_CustomClass *);
struct MUI_CustomClass *Init_LED(void);


// Global variable...
static struct MUI_CustomClass *LED_mcc[NUM_OF_LEDS];
ULONG render_left, render_top, render_right, render_bottom;

static Object *obj_LEDmcc[NUM_OF_LEDS];  // DFx LED objects

#endif // __LEDMCC_H__
