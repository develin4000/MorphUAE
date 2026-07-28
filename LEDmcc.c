/*
->==================================================<-
->= MUI-Framework - © Copyright 2025-2026 OnyxSoft =<-
->==================================================<-
->= Version  : 1.0                                 =<-
->= File     : LEDmcc.c                            =<-
->= Author   : Stefan Blixth                       =<-
->= Compiled : 2026-02-01                          =<-
->==================================================<-
*/

//#define USEDEBUG

#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/cybergraphics.h>
#include <cybergraphx/cybergraphics.h>

#include "LEDmcc.h"

/*=----------------------------- GenerateGfxData() ---------------------------*
 *                                                                            *
 *----------------------------------------------------------------------------*/
void GenerateGfxData(unsigned long *gfxdata, int tsize, unsigned long tpen, BOOL useflare)
{
   int tmpw, tmph;
   //debug_print("%s (%d)\n", __func__, __LINE__);

   //Generate the frame
   for (tmpw = 0; tmpw < tsize-1; tmpw++)
      gfxdata[tmpw] = Frame_Colour_Dark;
   for (tmpw = 1; tmpw < tsize; tmpw++)
      gfxdata[((tsize-1)*tsize)+tmpw] = Frame_Colour_Light;
   for (tmph = 1; tmph < tsize; tmph++)
      gfxdata[tsize*tmph] = Frame_Colour_Dark;
   for (tmph = 0; tmph < tsize; tmph++)
      gfxdata[(tsize*tmph)-1] = Frame_Colour_Light;
   
   // Generate the main colour...
   for (tmph = 1; tmph < tsize-1; tmph++)
      for (tmpw = 1; tmpw < tsize-1; tmpw++)
         gfxdata[(tsize*tmph)+tmpw] = tpen;

   // Generate the "lens flares"
   if (useflare)
   {
      gfxdata[(tsize*2)+2] = Frame_Colour_Light;
      gfxdata[(tsize*2)+3] = Frame_Colour_Light;
      gfxdata[(tsize*3)+2] = Frame_Colour_Light;
      gfxdata[(tsize*(tsize-3)-3)] = Frame_Colour_Light;
      gfxdata[(tsize*(tsize-2)-3)] = Frame_Colour_Light;
      gfxdata[(tsize*(tsize-2)-4)] = Frame_Colour_Light;
   }
}
/*=*/

/*=----------------------------- LED_New() -----------------------------------*
 *                                                                            *
 *----------------------------------------------------------------------------*/
static ULONG LED_New(struct IClass *cl, Object *obj, struct opSet *msg)
{
   struct LEDData *data;
   int LED_Size;
   BOOL LED_Flare;
   //debug_print("%s (%d)\n", __func__, __LINE__);

   obj = DoSuperNew(cl, obj,
         InnerSpacing(0, 0),
         //MUIA_Frame,        MUIV_Frame_None,
         //MUIA_Background,   MUII_WindowBack,
         MUIA_InputMode,    MUIV_InputMode_RelVerify,
         MUIA_FillArea,     TRUE,
         MUIA_DoubleBuffer, TRUE,
         TAG_MORE,          msg->ops_AttrList);

   if (!obj)
   {
      return(0);
   }

   data = (struct LEDData *)INST_DATA(cl, obj);

   LED_Size = LED_SIZE_EXTREME; //LED_SIZE_BIG; // Here is the size we want to use "globally"
   LED_Flare = TRUE;           // Here you can set if you would like to have a little "lens flare" on the image...
   
   GenerateGfxData(&LED_gfx_off, LED_Size, LED_Colour_Idle, LED_Flare);
   GenerateGfxData(&LED_gfx_red, LED_Size, LED_Colour_Red, LED_Flare);
   GenerateGfxData(&LED_gfx_green, LED_Size, LED_Colour_Green, LED_Flare);
   GenerateGfxData(&LED_gfx_blue, LED_Size, LED_Colour_Blue, LED_Flare);
   GenerateGfxData(&LED_gfx_yellow, LED_Size, LED_Colour_Yellow, LED_Flare);

   GenerateGfxData(&LED_gfx_blue1x, LED_Size, LED_Colour_Blue1x, LED_Flare);
   GenerateGfxData(&LED_gfx_blue2x, LED_Size, LED_Colour_Blue2x, LED_Flare);

   data->size   = LED_Size; //LED_SIZE_MEDIUM;
   data->style  = LED_Style_Square;
   data->active = FALSE;
   data->colour = MUIV_LED_Colour_Off;

   return((ULONG)obj);
}
/*=*/

/*=----------------------------- LED_Dispose() -------------------------------*
 *                                                                            *
 *----------------------------------------------------------------------------*/
static ULONG LED_Dispose(struct IClass *cl, Object *obj, Msg msg)
{
   struct LEDData *data = (struct LEDData *)INST_DATA(cl, obj);
   //debug_print("%s (%d)\n", __func__, __LINE__);

   return(DoSuperMethodA(cl, obj, msg));
}
/*=*/

/*=----------------------------- LED_Set() -----------------------------------*
 *                                                                            *
 *----------------------------------------------------------------------------*/
static ULONG LED_Set(struct IClass *cl, Object *obj, struct opSet *msg)
{
   struct LEDData *data = (struct LEDData *)INST_DATA(cl, obj);
   struct TagItem *TagList = NULL;
   struct TagItem *tag = NULL;

   //debug_print("%s (%d)\n", __func__, __LINE__);

   if(msg -> ops_AttrList)
   {
      for(TagList = msg -> ops_AttrList; tag = NextTagItem(&TagList);)
      {
         switch(tag->ti_Tag)
         {
            case MUIA_LED_Colour :
               data->colour = tag->ti_Data;
               break;
        
            case MUIA_LED_Style :
               data->style = tag->ti_Data;
               break;
           
            case MUIA_LED_Size :
               data->size = tag->ti_Data;
               break;
         }
      }
   }

   MUI_Redraw(obj, MADF_DRAWOBJECT);
   return(DoSuperMethodA(cl, obj, (Msg) msg));
}
/*=*/


/*=----------------------------- LED_Setup() ---------------------------------*
 *                                                                            *
 *----------------------------------------------------------------------------*/
static ULONG LED_Setup(struct IClass *cl, Object *obj, Msg msg)
{
   struct LEDData *data = (struct LEDData *)INST_DATA(cl, obj);
   //debug_print("%s (%d)\n", __func__, __LINE__);

   if (!DoSuperMethodA(cl, obj, msg))
      return(FALSE);

   //MUI_RequestIDCMP(obj, IDCMP_MOUSEBUTTONS);

   return(TRUE);
}
/*=*/


/*=----------------------------- LED_Cleanup() -------------------------------*
 *                                                                            *
 *----------------------------------------------------------------------------*/
static ULONG LED_Cleanup(struct IClass *cl, Object *obj, Msg msg)
{
   //struct LEDData *data = (struct LEDData *)INST_DATA(cl, obj);
   //debug_print("%s (%d)\n", __func__, __LINE__);

   //MUI_RejectIDCMP(obj, IDCMP_MOUSEBUTTONS);
   return(DoSuperMethodA(cl, obj, (Msg)msg));
}
/*=*/

/*=----------------------------- LED_Askminmax() -----------------------------*
 *                                                                            *
 *----------------------------------------------------------------------------*/
static ULONG LED_Askminmax(struct IClass *cl, Object *obj, struct MUIP_AskMinMax *msg)
{
   struct LEDData *data = (struct LEDData *)INST_DATA(cl, obj);
   LONG newsize;

   DoSuperMethodA(cl, obj, (Msg)msg);

   switch (data->size)
   {
      case LED_SIZE_SMALL :
         newsize = LED_SIZE_SMALL; break;
      case LED_SIZE_BIG :
         newsize = LED_SIZE_BIG; break;
      case LED_SIZE_EXTREME :
         newsize = LED_SIZE_EXTREME; break;
      default : // LED_SIZE_MEDIUM
         newsize = LED_SIZE_MEDIUM; break;
         break;
   }

   msg->MinMaxInfo->MinWidth  += newsize;
   msg->MinMaxInfo->DefWidth  += newsize;
   msg->MinMaxInfo->MaxWidth  += newsize;
   msg->MinMaxInfo->MinHeight += newsize;
   msg->MinMaxInfo->DefHeight += newsize;
   msg->MinMaxInfo->MaxHeight += newsize;

   return(0);
}
/*=*/


/*=----------------------------- LED_Draw() ----------------------------------*
 *                                                                            *
 *----------------------------------------------------------------------------*/
static ULONG LED_Draw(struct IClass *cl, Object *obj, struct MUIP_Draw *msg)
{
   struct LEDData *data = (struct LEDData *)INST_DATA(cl, obj);
   struct RastPort *rp = _rp(obj);
   unsigned long *LED_gfx;
   //debug_print("%s (%d)\n", __func__, __LINE__);

   DoSuperMethodA(cl, obj, (Msg)msg);

   switch (data->colour)
   {
      case MUIV_LED_Colour_Red :
         LED_gfx = LED_gfx_red; break;
      case MUIV_LED_Colour_Green :
         LED_gfx = LED_gfx_green; break;
      case MUIV_LED_Colour_Blue :
         LED_gfx = LED_gfx_blue; break;
      case MUIV_LED_Colour_Yellow :
         LED_gfx = LED_gfx_yellow; break;
      case MUIV_LED_Colour_Blue1x :
         LED_gfx = LED_gfx_blue1x; break;
      case MUIV_LED_Colour_Blue2x :
         LED_gfx = LED_gfx_blue2x; break;
      default : // MUIV_LED_Colour_Off
         LED_gfx = LED_gfx_off; break;
   }

   WritePixelArray(LED_gfx, 0, 0, (data->size*4), _rp(obj), _mleft(obj), _mtop(obj), _mwidth(obj), _mheight(obj), RECTFMT_ARGB);
   return(0);
}
/*=*/

/*=----------------------------- LED_Show ------------------------------------*
 *                                                                            *
 *----------------------------------------------------------------------------*/
static ULONG LED_Show(struct IClass *cl, Object *obj, Msg msg)
{
   struct LEDData *data = (struct LEDData *)INST_DATA(cl, obj);
   //debug_print("%s (%d)\n", __func__, __LINE__);

   //data->screen = _screen(obj);
   return(DoSuperMethodA(cl, obj, (Msg)msg));
}
/*=*/

/*=----------------------------- DISPATCHER ----------------------------------*
 *                                                                            *
 *----------------------------------------------------------------------------*/
DISPATCHER(LED)
{
   switch (msg->MethodID)
   {
      case OM_NEW             : return LED_New       (cl, obj, (APTR)msg); break;
      case OM_DISPOSE         : return LED_Dispose   (cl, obj, (APTR)msg); break;
      case OM_SET             : return LED_Set       (cl, obj, (APTR)msg); break;
      case MUIM_Setup         : return LED_Setup     (cl, obj, (APTR)msg); break;
      case MUIM_Cleanup       : return LED_Cleanup   (cl, obj, (APTR)msg); break;
      case MUIM_AskMinMax     : return LED_Askminmax (cl, obj, (APTR)msg); break;
      case MUIM_Draw          : return LED_Draw      (cl, obj, (APTR)msg); break;
      case MUIM_Show          : return LED_Show      (cl, obj, (APTR)msg); break;
   }

   return DoSuperMethodA(cl, obj, msg);
}
DISPATCHER_END
/*=*/


/*=----------------------------- Cleanup_LED() -------------------------------*
 *                                                                            *
 *----------------------------------------------------------------------------*/
void Cleanup_LED(struct MUI_CustomClass *mcc)
{
   //debug_print("%s (%d)\n", __func__, __LINE__);
   if (mcc) MUI_DeleteCustomClass(mcc);
}
/*=*/

/*=----------------------------- Init_LED() ----------------------------------*
 *                                                                            *
 *----------------------------------------------------------------------------*/
struct MUI_CustomClass *Init_LED(void)
{
   struct MUI_CustomClass *mcc = NULL;
   //debug_print("%s (%d)\n", __func__, __LINE__);
   mcc = MUI_CreateCustomClass(NULL, MUIC_Area, NULL, sizeof(struct LEDData), DISPATCHER_REF(LED));
   return(mcc);
}
/*=*/
