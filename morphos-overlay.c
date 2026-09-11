/*
 * MorphUAE - MorphOS CGXVideo overlay backend
 *
 * Hardware VLayer lifecycle, aspect-preserving destination geometry and
 * framebuffer conversion live here so morphos-win.c only coordinates the
 * MUI renderer and the software fallback path.
 */

#include "sysconfig.h"
#include "sysdeps.h"

#include <exec/execbase.h>
#include <exec/system.h>
#include <graphics/gfxbase.h>
#include <intuition/intuitionbase.h>
#include <libraries/mui.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <proto/cgxvideo.h>

#include <cybergraphx/cybergraphics.h>
#include <cybergraphx/cgxvideo.h>

#include <string.h>

#include "uae.h"
#include "morphos-overlay.h"

extern struct IntuitionBase *IntuitionBase;
extern struct Library *CGXVideoBase;

struct MorphOverlayState
{
   struct VLayerHandle *vlayer;
   LONG left;
   LONG top;
   LONG width;
   LONG height;
   BOOL backing_dirty;
   BOOL active;
   BOOL failed;
   BOOL suspended;
   BOOL cpu_has_altivec;
};

static struct MorphOverlayState overlay;

static BOOL MorphOverlay_CalcGeometry(Object *obj, struct Window *window,
                                      BOOL fullscreen,
                                      ULONG src_width, ULONG src_height,
                                      LONG *dst_left, LONG *dst_top,
                                      LONG *dst_width, LONG *dst_height,
                                      LONG *right, LONG *bottom)
{
   LONG inner_width, inner_height;
   LONG area_left, area_top, area_width, area_height;

   if (!window || !obj)
      return FALSE;

   inner_width  = (LONG)window->Width  - (LONG)window->BorderLeft - (LONG)window->BorderRight;
   inner_height = (LONG)window->Height - (LONG)window->BorderTop  - (LONG)window->BorderBottom;

   if (inner_width < 1 || inner_height < 1)
      return FALSE;

   if (fullscreen)
   {
      area_left = 0;
      area_top = 0;
      area_width = inner_width;
      area_height = inner_height;
   }
   else
   {
      area_left   = (LONG)_mleft(obj) - (LONG)window->BorderLeft;
      area_top    = (LONG)_mtop(obj)  - (LONG)window->BorderTop;
      area_width  = (LONG)_mwidth(obj);
      area_height = (LONG)_mheight(obj);
   }

   if (area_left < 0) area_left = 0;
   if (area_top < 0) area_top = 0;
   if (area_width < 1 || area_height < 1)
      return FALSE;

   if (!src_width) src_width = 1;
   if (!src_height) src_height = 1;

   *dst_width = area_width;
   *dst_height = area_height;

   if (((unsigned long long)area_width * (unsigned long long)src_height) >
       ((unsigned long long)area_height * (unsigned long long)src_width))
   {
      *dst_width = (LONG)(((unsigned long long)area_height * src_width) / src_height);
      if (*dst_width < 1) *dst_width = 1;
   }
   else
   {
      *dst_height = (LONG)(((unsigned long long)area_width * src_height) / src_width);
      if (*dst_height < 1) *dst_height = 1;
   }

   *dst_left = area_left + (area_width - *dst_width) / 2;
   *dst_top  = area_top  + (area_height - *dst_height) / 2;

   *right  = inner_width  - (*dst_left + *dst_width);
   *bottom = inner_height - (*dst_top + *dst_height);
   if (*right < 0) *right = 0;
   if (*bottom < 0) *bottom = 0;

   return TRUE;
}

void MorphOverlay_Init(void)
{
   memset(&overlay, 0, sizeof(overlay));
   overlay.left = -1;
   overlay.top = -1;
   overlay.width = -1;
   overlay.height = -1;
   overlay.backing_dirty = TRUE;
}

void MorphOverlay_DetectCPU(void)
{
   ULONG altivec = 0;

   overlay.cpu_has_altivec = FALSE;
   if (NewGetSystemAttrs(&altivec, sizeof(altivec),
                         SYSTEMINFOTYPE_PPC_ALTIVEC, TAG_DONE))
      overlay.cpu_has_altivec = altivec ? TRUE : FALSE;
}

void MorphOverlay_Destroy(void)
{
   if (overlay.vlayer)
   {
      DetachVLayer(overlay.vlayer);
      DeleteVLayerHandle(overlay.vlayer);
      overlay.vlayer = NULL;
   }

   overlay.active = FALSE;
   overlay.left = -1;
   overlay.top = -1;
   overlay.width = -1;
   overlay.height = -1;
   overlay.backing_dirty = TRUE;
}

BOOL MorphOverlay_HasLayer(void)
{
   return overlay.vlayer != NULL;
}

BOOL MorphOverlay_IsActive(void)
{
   return overlay.active;
}

BOOL MorphOverlay_IsFailed(void)
{
   return overlay.failed;
}

BOOL MorphOverlay_IsSuspended(void)
{
   return overlay.suspended;
}

void MorphOverlay_SetFailed(BOOL failed)
{
   overlay.failed = failed;
}

void MorphOverlay_ResetRuntimeState(void)
{
   overlay.failed = FALSE;
   overlay.suspended = FALSE;
}

void MorphOverlay_Suspend(void)
{
   overlay.suspended = TRUE;
}

void MorphOverlay_Resume(void)
{
   overlay.suspended = FALSE;
   overlay.failed = FALSE;
}

BOOL MorphOverlay_FullscreenScreenIsActive(BOOL fullscreen,
                                            struct Screen *screen,
                                            struct Window *window)
{
   struct Screen *frontscreen;
   ULONG ibaselock;

   if (!fullscreen)
      return TRUE;

   if (!IntuitionBase || !screen || !window || window->WScreen != screen)
      return FALSE;

   ibaselock = LockIBase(0);
   frontscreen = IntuitionBase->FirstScreen;
   UnlockIBase(ibaselock);

   return frontscreen == screen;
}

void MorphOverlay_MarkBackingDirty(void)
{
   overlay.backing_dirty = TRUE;
}

void MorphOverlay_UpdateGeometry(Object *obj, struct Window *window,
                                 BOOL fullscreen,
                                 ULONG src_width, ULONG src_height)
{
   LONG dst_left, dst_top, dst_width, dst_height;
   LONG right, bottom;

   if (!overlay.vlayer)
      return;

   if (!MorphOverlay_CalcGeometry(obj, window, fullscreen,
                                  src_width, src_height,
                                  &dst_left, &dst_top,
                                  &dst_width, &dst_height,
                                  &right, &bottom))
      return;

   if (dst_left == overlay.left && dst_top == overlay.top &&
       dst_width == overlay.width && dst_height == overlay.height)
      return;

   overlay.left = dst_left;
   overlay.top = dst_top;
   overlay.width = dst_width;
   overlay.height = dst_height;
   overlay.backing_dirty = TRUE;

   SetVLayerAttrTags(overlay.vlayer,
                     VOA_LeftIndent,   (ULONG)dst_left,
                     VOA_RightIndent,  (ULONG)right,
                     VOA_TopIndent,    (ULONG)dst_top,
                     VOA_BottomIndent, (ULONG)bottom,
                     TAG_DONE);
}

void MorphOverlay_ClearBacking(Object *obj, struct Window *window,
                               BOOL fullscreen)
{
   LONG area_left, area_top, area_width, area_height;
   struct RastPort *rp;

   if (!overlay.backing_dirty || !window || !obj)
      return;

   rp = _rp(obj);
   if (!rp)
      return;

   if (fullscreen)
   {
      area_left = (LONG)window->BorderLeft;
      area_top = (LONG)window->BorderTop;
      area_width = (LONG)window->Width - (LONG)window->BorderLeft - (LONG)window->BorderRight;
      area_height = (LONG)window->Height - (LONG)window->BorderTop - (LONG)window->BorderBottom;
   }
   else
   {
      area_left = (LONG)_mleft(obj);
      area_top = (LONG)_mtop(obj);
      area_width = (LONG)_mwidth(obj);
      area_height = (LONG)_mheight(obj);
   }

   if (area_width <= 0 || area_height <= 0)
      return;

   FillPixelArray(rp,
                  (UWORD)area_left, (UWORD)area_top,
                  (UWORD)area_width, (UWORD)area_height,
                  0x00000000);

   overlay.backing_dirty = FALSE;
}

BOOL MorphOverlay_Attach(Object *obj, struct Window *window,
                         struct Screen *screen, BOOL fullscreen,
                         ULONG src_width, ULONG src_height)
{
   ULONG error = 0;
   ULONG srcfmt;
   LONG dst_left, dst_top, dst_width, dst_height;
   LONG right, bottom;

   if (overlay.suspended || !CGXVideoBase || !window || !window->WScreen)
      return FALSE;

   if (!MorphOverlay_FullscreenScreenIsActive(fullscreen, screen, window))
      return FALSE;

   if (overlay.vlayer)
   {
      MorphOverlay_UpdateGeometry(obj, window, fullscreen, src_width, src_height);
      overlay.active = TRUE;
      return TRUE;
   }

#if defined(SRCFMT_RGB16)
   srcfmt = SRCFMT_RGB16;
#elif defined(SRCFMT_R5G6B5PC)
   srcfmt = SRCFMT_R5G6B5PC;
#else
   return FALSE;
#endif

   overlay.vlayer = CreateVLayerHandleTags(window->WScreen,
                                           VOA_SrcType,      srcfmt,
                                           VOA_SrcWidth,     src_width,
                                           VOA_SrcHeight,    src_height,
                                           VOA_DoubleBuffer, TRUE,
                                           VOA_UseFilter,    TRUE,
                                           VOA_Error,        (ULONG)&error,
                                           TAG_DONE);
   if (!overlay.vlayer)
   {
      write_log("MUIGFX: Unable to create CGXVideo overlay (error %lu).\n", error);
      return FALSE;
   }

   if (!MorphOverlay_CalcGeometry(obj, window, fullscreen,
                                  src_width, src_height,
                                  &dst_left, &dst_top,
                                  &dst_width, &dst_height,
                                  &right, &bottom))
   {
      MorphOverlay_Destroy();
      return FALSE;
   }

   if ((LONG)AttachVLayerTags(overlay.vlayer, window,
                              VOA_LeftIndent,   (ULONG)dst_left,
                              VOA_RightIndent,  (ULONG)right,
                              VOA_TopIndent,    (ULONG)dst_top,
                              VOA_BottomIndent, (ULONG)bottom,
                              TAG_DONE) != 0)
   {
      write_log("MUIGFX: Unable to attach CGXVideo overlay.\n");
      MorphOverlay_Destroy();
      return FALSE;
   }

   overlay.left = dst_left;
   overlay.top = dst_top;
   overlay.width = dst_width;
   overlay.height = dst_height;
   overlay.backing_dirty = TRUE;
   overlay.active = TRUE;
   write_log("MUIGFX: CGXVideo overlay enabled (%lux%lu).\n", src_width, src_height);
   return TRUE;
}

static UWORD MorphOverlay_RGB565Pixel(int pixel_format, const UBYTE *src, int pixbytes)
{
   ULONG r, g, b;
   UWORD value;

   if (pixbytes == 4)
   {
      r = src[1];
      g = src[2];
      b = src[3];
      return (UWORD)(((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3));
   }

   if (pixel_format == PIXFMT_RGB15PC || pixel_format == PIXFMT_RGB16PC)
      value = (UWORD)(((UWORD)src[1] << 8) | src[0]);
   else
      value = (UWORD)(((UWORD)src[0] << 8) | src[1]);

   if (pixel_format == PIXFMT_RGB15PC || pixel_format == PIXFMT_RGB15)
   {
      r = (value >> 10) & 31;
      g = (value >> 5) & 31;
      b = value & 31;
      g = (g << 1) | (g >> 4);
      return (UWORD)((r << 11) | (g << 5) | b);
   }

   return value;
}

static void MorphOverlay_CopyRGB16ToRGB16PC_ScalarRow(const UBYTE *s, UBYTE *d,
                                                       int width)
{
   int x = 0;

   if ((((ULONG)(APTR)s | (ULONG)(APTR)d) & 3UL) == 0)
   {
      const ULONG *s32 = (const ULONG *)(APTR)s;
      ULONG *d32 = (ULONG *)(APTR)d;
      int pairs = width >> 1;
      int i;

      for (i = 0; i < pairs; i++)
      {
         ULONG v = s32[i];
         d32[i] = ((v & 0x00ff00ffUL) << 8) | ((v & 0xff00ff00UL) >> 8);
      }
      x = pairs << 1;
   }

   for (; x < width; x++)
   {
      d[x * 2 + 0] = s[x * 2 + 1];
      d[x * 2 + 1] = s[x * 2 + 0];
   }
}

static void MorphOverlay_CopyRGB16ToRGB16PC_Scalar(const UBYTE *src, ULONG src_pitch,
                                                    UBYTE *dst, ULONG dst_pitch,
                                                    int width, int height)
{
   int y;

   for (y = 0; y < height; y++)
      MorphOverlay_CopyRGB16ToRGB16PC_ScalarRow(src + ((ULONG)y * src_pitch),
                                                dst + ((ULONG)y * dst_pitch), width);
}

static const UBYTE MorphOverlay_RGB16PCSwapMask[16] __attribute__((aligned(16))) =
{
   1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14
};

static void MorphOverlay_CopyRGB16ToRGB16PC_AltiVec(const UBYTE *src, ULONG src_pitch,
                                                    UBYTE *dst, ULONG dst_pitch,
                                                    int width, int height)
{
   ULONG old_vrsave;
   ULONG new_vrsave;
   int y;

   __asm__ volatile("mfspr %0,256" : "=r"(old_vrsave));
   new_vrsave = old_vrsave | 0xc0000000UL;
   __asm__ volatile(
      "mtspr 256,%0\n\t"
      ".machine altivec\n\t"
      "lvx 0,0,%1"
      :
      : "r"(new_vrsave), "r"(MorphOverlay_RGB16PCSwapMask)
      : "memory");

   for (y = 0; y < height; y++)
   {
      const UBYTE *s = src + ((ULONG)y * src_pitch);
      UBYTE *d = dst + ((ULONG)y * dst_pitch);
      int x = 0;

      if ((((ULONG)(APTR)s ^ (ULONG)(APTR)d) & 15UL) != 0 ||
          (((ULONG)(APTR)s | (ULONG)(APTR)d) & 1UL) != 0)
      {
         MorphOverlay_CopyRGB16ToRGB16PC_ScalarRow(s, d, width);
         continue;
      }

      while (x < width && (((ULONG)(APTR)s) & 15UL) != 0)
      {
         d[0] = s[1];
         d[1] = s[0];
         s += 2;
         d += 2;
         x++;
      }

      while (x + 8 <= width)
      {
         __asm__ volatile(
            ".machine altivec\n\t"
            "lvx 1,0,%0\n\t"
            "vperm 1,1,1,0\n\t"
            "stvx 1,0,%1"
            :
            : "r"(s), "r"(d)
            : "memory");

         s += 16;
         d += 16;
         x += 8;
      }

      while (x < width)
      {
         d[0] = s[1];
         d[1] = s[0];
         s += 2;
         d += 2;
         x++;
      }
   }

   __asm__ volatile("mtspr 256,%0" : : "r"(old_vrsave) : "memory");
}

static void MorphOverlay_CopyRGB16ToRGB16PC(const UBYTE *src, ULONG src_pitch,
                                             UBYTE *dst, ULONG dst_pitch,
                                             int width, int height)
{
   if (overlay.cpu_has_altivec)
      MorphOverlay_CopyRGB16ToRGB16PC_AltiVec(src, src_pitch, dst, dst_pitch, width, height);
   else
      MorphOverlay_CopyRGB16ToRGB16PC_Scalar(src, src_pitch, dst, dst_pitch, width, height);
}

static void MorphOverlay_CopyRGB16PC(const UBYTE *src, ULONG src_pitch,
                                     UBYTE *dst, ULONG dst_pitch,
                                     int width, int height)
{
   int y;
   ULONG bytes = (ULONG)width * 2UL;

   for (y = 0; y < height; y++)
      memcpy(dst + ((ULONG)y * dst_pitch), src + ((ULONG)y * src_pitch), bytes);
}

BOOL MorphOverlay_Present(Object *obj, struct Window *window,
                          struct Screen *screen, BOOL fullscreen,
                          BOOL live_resize,
                          const UBYTE *buffer, int pixel_format,
                          int width, int height, int pitch, int pixbytes,
                          BOOL vsync)
{
   struct VLayerHandle *vlayer;
   UBYTE *dst;
   ULONG modulo;
   int x, y;

   if (!overlay.vlayer || !buffer)
      return FALSE;

   if (!MorphOverlay_FullscreenScreenIsActive(fullscreen, screen, window))
   {
      MorphOverlay_Destroy();
      return TRUE;
   }

   if (width <= 0 || height <= 0 || pitch <= 0 || (pixbytes != 2 && pixbytes != 4))
      return FALSE;

   MorphOverlay_UpdateGeometry(obj, window, fullscreen, (ULONG)width, (ULONG)height);
   MorphOverlay_ClearBacking(obj, window, fullscreen);

   if (live_resize)
      return TRUE;

   vlayer = overlay.vlayer;
   if (!LockVLayer(vlayer))
      return TRUE;

   dst = (UBYTE *)(APTR)GetVLayerAttr(vlayer, VOA_BaseAddress);
   modulo = GetVLayerAttr(vlayer, VOA_Modulo);
   if (!dst || modulo < (ULONG)(width * 2))
   {
      UnlockVLayer(vlayer);
      return FALSE;
   }

   if (pixbytes == 2 && pixel_format == PIXFMT_RGB16)
   {
      MorphOverlay_CopyRGB16ToRGB16PC(buffer, (ULONG)pitch, dst, modulo, width, height);
   }
   else if (pixbytes == 2 && pixel_format == PIXFMT_RGB16PC)
   {
      MorphOverlay_CopyRGB16PC(buffer, (ULONG)pitch, dst, modulo, width, height);
   }
   else
   {
      for (y = 0; y < height; y++)
      {
         const UBYTE *src = buffer + (y * pitch);
         UBYTE *out = dst + (y * modulo);

         for (x = 0; x < width; x++)
         {
            UWORD rgb565 = MorphOverlay_RGB565Pixel(pixel_format,
                                                    src + (x * pixbytes), pixbytes);
            out[x * 2 + 0] = (UBYTE)(rgb565 & 0xff);
            out[x * 2 + 1] = (UBYTE)(rgb565 >> 8);
         }
      }
   }

   UnlockVLayer(vlayer);

   if (vsync)
      WaitTOF();

   SwapVLayerBuffer(vlayer);
   return TRUE;
}
