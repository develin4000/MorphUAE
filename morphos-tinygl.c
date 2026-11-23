/*
 * MorphUAE - MorphOS TinyGL presentation backend
 *
 * Context/bitmap/texture lifecycle and the framebuffer -> RGBA conversion live
 * here so morphos-win.c only coordinates the MUI renderer and the fallback
 * chain (TinyGL -> CGXVideo overlay -> surface).
 *
 * Rendering follows the usual TinyGL-on-MorphOS pattern: a 3D-capable bitmap
 * (BMF_3DTARGET, friend of the window bitmap) is the GL render target, the
 * emulated frame is a texture drawn on one quad, and the finished bitmap is
 * copied into the window RastPort with BltBitMapRastPort().
 */

#include "sysconfig.h"
#include "sysdeps.h"

#include <exec/libraries.h>
#include <exec/system.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <intuition/intuition.h>
#include <libraries/mui.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <proto/tinygl.h>

#include <cybergraphx/cybergraphics.h>
#include <tgl/gl.h>
#include <tgl/gla.h>

#include <stdlib.h>
#include <string.h>

#include "uae.h"
#include "morphos-tinygl.h"

#ifndef BMF_3DTARGET
#define BMF_3DTARGET (1UL << 12)
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

struct Library *TinyGLBase = NULL;

struct MorphTinyGLState
{
   GLContext *context;
   struct BitMap *bitmap;
   GLuint texture;
   BOOL texture_ready;
   LONG tex_width;            /* size of the uploaded source texture */
   LONG tex_height;
   LONG bm_width;             /* size of the render bitmap           */
   LONG bm_height;
   struct Window *owner_window;
   struct Screen *owner_screen;
   BOOL failed;
   UBYTE *rgba;               /* converted source frame (RGBA8), 16-byte aligned */
   UBYTE *rgba_raw;           /* pointer returned by malloc()        */
   ULONG rgba_capacity;
   BOOL cpu_has_altivec;
};

static struct MorphTinyGLState gl;

/* RGB565 -> RGBA8888 (R in the high byte, as stored on big-endian PPC). */
static ULONG rgb565_lut[65536];
static BOOL rgb565_lut_ready = FALSE;

static void MorphTinyGL_InitLUT(void)
{
   ULONG v;

   if (rgb565_lut_ready)
      return;

   for (v = 0; v < 65536; v++)
   {
      ULONG r5 = (v >> 11) & 31;
      ULONG g6 = (v >> 5) & 63;
      ULONG b5 = v & 31;
      ULONG r8 = (r5 << 3) | (r5 >> 2);
      ULONG g8 = (g6 << 2) | (g6 >> 4);
      ULONG b8 = (b5 << 3) | (b5 >> 2);

      rgb565_lut[v] = (r8 << 24) | (g8 << 16) | (b8 << 8) | 0xffUL;
   }

   rgb565_lut_ready = TRUE;
}

static void MorphTinyGL_DetectCPU(void)
{
   ULONG altivec = 0;

   gl.cpu_has_altivec = FALSE;
   if (NewGetSystemAttrs(&altivec, sizeof(altivec),
                         SYSTEMINFOTYPE_PPC_ALTIVEC, TAG_DONE))
      gl.cpu_has_altivec = altivec ? TRUE : FALSE;
}

BOOL MorphTinyGL_Init(void)
{
   memset(&gl, 0, sizeof(gl));
   MorphTinyGL_InitLUT();

   MorphTinyGL_DetectCPU();

   if (!TinyGLBase)
      TinyGLBase = OpenLibrary("tinygl.library", 0);

   if (!TinyGLBase)
      write_log("MUIGFX: tinygl.library not available.\n");

   return TinyGLBase != NULL;
}

void MorphTinyGL_Shutdown(void)
{
   MorphTinyGL_Destroy();

   if (gl.rgba_raw)
   {
      free(gl.rgba_raw);
      gl.rgba_raw = NULL;
      gl.rgba = NULL;
      gl.rgba_capacity = 0;
   }

   if (TinyGLBase)
   {
      CloseLibrary(TinyGLBase);
      TinyGLBase = NULL;
   }
}

void MorphTinyGL_Destroy(void)
{
   if (gl.context)
   {
      if (gl.texture)
         GLDeleteTextures(gl.context, 1, &gl.texture);
      GLADestroyContext(gl.context);
      GLClose(gl.context);
   }

   if (gl.bitmap)
      FreeBitMap(gl.bitmap);

   gl.context = NULL;
   gl.bitmap = NULL;
   gl.texture = 0;
   gl.texture_ready = FALSE;
   gl.tex_width = 0;
   gl.tex_height = 0;
   gl.bm_width = 0;
   gl.bm_height = 0;
   gl.owner_window = NULL;
   gl.owner_screen = NULL;
}

BOOL MorphTinyGL_IsAvailable(void)
{
   return TinyGLBase != NULL;
}

BOOL MorphTinyGL_IsAttached(void)
{
   return gl.context != NULL;
}

BOOL MorphTinyGL_IsFailed(void)
{
   return gl.failed;
}

void MorphTinyGL_SetFailed(BOOL failed)
{
   gl.failed = failed;
}

static BOOL MorphTinyGL_CreateContext(struct Window *window, LONG width, LONG height)
{
   struct BitMap *friend_bitmap;
   struct BitMap *bitmap;
   GLContext *context;
   ULONG depth;
   struct TagItem tags[3];

   if (!TinyGLBase || !window || !window->RPort || !window->RPort->BitMap)
      return FALSE;

   friend_bitmap = window->RPort->BitMap;
   depth = GetBitMapAttr(friend_bitmap, BMA_DEPTH);
   if (depth < 15)
   {
      write_log("MUIGFX: TinyGL: screen depth %lu is not supported.\n", depth);
      return FALSE;
   }

   context = GLInit();
   if (!context)
   {
      write_log("MUIGFX: TinyGL: GLInit() failed.\n");
      return FALSE;
   }

   bitmap = AllocBitMap((ULONG)width, (ULONG)height, depth,
                        BMF_CLEAR | BMF_MINPLANES | BMF_DISPLAYABLE | BMF_3DTARGET,
                        friend_bitmap);
   if (!bitmap)
   {
      write_log("MUIGFX: TinyGL: AllocBitMap(%ldx%ld, depth %lu) failed.\n",
                width, height, depth);
      GLClose(context);
      return FALSE;
   }

   tags[0].ti_Tag = TGL_CONTEXT_BITMAP;  tags[0].ti_Data = (ULONG)bitmap;
   tags[1].ti_Tag = TGL_CONTEXT_STENCIL; tags[1].ti_Data = TRUE;
   tags[2].ti_Tag = TAG_DONE;            tags[2].ti_Data = 0;

   if (GLAInitializeContext(context, tags) <= 0)
   {
      write_log("MUIGFX: TinyGL: GLAInitializeContext() failed.\n");
      GLClose(context);
      FreeBitMap(bitmap);
      return FALSE;
   }

   gl.context = context;
   gl.bitmap = bitmap;
   gl.bm_width = width;
   gl.bm_height = height;
   gl.owner_window = window;
   gl.owner_screen = window->WScreen;
   gl.texture = 0;
   gl.texture_ready = FALSE;
   gl.tex_width = 0;
   gl.tex_height = 0;

   GLGenTextures(context, 1, &gl.texture);
   GLBindTexture(context, GL_TEXTURE_2D, gl.texture);
   GLTexParameteri(context, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   GLTexParameteri(context, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   GLTexParameteri(context, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   GLTexParameteri(context, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

   write_log("MUIGFX: TinyGL context created (%ldx%ld, depth %lu).\n",
             width, height, depth);
   return TRUE;
}

static UWORD MorphTinyGL_To565(int pixel_format, const UBYTE *src)
{
   UWORD value;

   if (pixel_format == PIXFMT_RGB15PC || pixel_format == PIXFMT_RGB16PC)
      value = (UWORD)(((UWORD)src[1] << 8) | src[0]);
   else
      value = (UWORD)(((UWORD)src[0] << 8) | src[1]);

   if (pixel_format == PIXFMT_RGB15PC || pixel_format == PIXFMT_RGB15)
   {
      ULONG r = (value >> 10) & 31;
      ULONG g = (value >> 5) & 31;
      ULONG b = value & 31;

      g = (g << 1) | (g >> 4);
      return (UWORD)((r << 11) | (g << 5) | b);
   }

   return value;
}

static void MorphTinyGL_RowARGB_Scalar(const UBYTE *src, ULONG *dst, int width)
{
   const ULONG *s32 = (const ULONG *)(const void *)src;
   int x;

   if ((((ULONG)(APTR)src) & 3UL) == 0)
   {
      /* ARGB (0xAARRGGBB) -> RGBA (0xRRGGBBAA) */
      for (x = 0; x < width; x++)
         dst[x] = (s32[x] << 8) | 0xffUL;
   }
   else
   {
      for (x = 0; x < width; x++)
         dst[x] = ((ULONG)src[x * 4 + 1] << 24) | ((ULONG)src[x * 4 + 2] << 16) |
                  ((ULONG)src[x * 4 + 3] << 8) | 0xffUL;
   }
}

static void MorphTinyGL_Row565_Scalar(const UBYTE *src, ULONG *dst, int width,
                                       int pixel_format)
{
   int x;

   if (pixel_format == PIXFMT_RGB16)
   {
      for (x = 0; x < width; x++)
         dst[x] = rgb565_lut[((UWORD)src[x * 2] << 8) | src[x * 2 + 1]];
   }
   else if (pixel_format == PIXFMT_RGB16PC)
   {
      for (x = 0; x < width; x++)
         dst[x] = rgb565_lut[((UWORD)src[x * 2 + 1] << 8) | src[x * 2]];
   }
   else
   {
      for (x = 0; x < width; x++)
         dst[x] = rgb565_lut[MorphTinyGL_To565(pixel_format, src + x * 2)];
   }
}

/* vperm control: ARGB -> RGBA.  Indices 0-15 pick from the source vector and
 * index 16 picks 0xff from the second operand (all-ones) for the alpha byte. */
static const UBYTE MorphTinyGL_ARGBToRGBAMask[16] __attribute__((aligned(16))) =
{
   1, 2, 3, 16,  5, 6, 7, 16,  9, 10, 11, 16,  13, 14, 15, 16
};

/* vperm control: byte-swap each halfword (RGB16PC -> big-endian RGB565). */
static const UBYTE MorphTinyGL_SwapMask[16] __attribute__((aligned(16))) =
{
   1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14
};

/* VRSAVE tells the OS which vector registers it must preserve for this task:
 * bit 0 (MSB) is vr0.  The routines below use vr0..vr19. */
#define MORPHTINYGL_VRSAVE_MASK 0xfffff000UL

static void MorphTinyGL_ConvertARGB_AltiVec(const UBYTE *src, ULONG src_pitch,
                                             UBYTE *dst, int width, int height)
{
   ULONG old_vrsave;
   ULONG new_vrsave;
   int y;

   __asm__ volatile("mfspr %0,256" : "=r"(old_vrsave));
   new_vrsave = old_vrsave | MORPHTINYGL_VRSAVE_MASK;
   __asm__ volatile(
      "mtspr 256,%0\n\t"
      ".machine altivec\n\t"
      "lvx 0,0,%1\n\t"
      "vspltisb 1,-1"
      :
      : "r"(new_vrsave), "r"(MorphTinyGL_ARGBToRGBAMask)
      : "memory");

   for (y = 0; y < height; y++)
   {
      const UBYTE *s = src + ((ULONG)y * src_pitch);
      UBYTE *d = dst + ((ULONG)y * (ULONG)width * 4UL);
      int x = 0;

      if ((((ULONG)(APTR)s | (ULONG)(APTR)d) & 15UL) == 0)
      {
         while (x + 8 <= width)
         {
            const UBYTE *s16 = s + 16;
            UBYTE *d16 = d + 16;

            __asm__ volatile(
               ".machine altivec\n\t"
               "lvx 2,0,%0\n\t"
               "lvx 3,0,%2\n\t"
               "vperm 4,2,1,0\n\t"
               "vperm 5,3,1,0\n\t"
               "stvx 4,0,%1\n\t"
               "stvx 5,0,%3"
               :
               : "r"(s), "r"(d), "r"(s16), "r"(d16)
               : "memory");

            s += 32;
            d += 32;
            x += 8;
         }

         while (x + 4 <= width)
         {
            __asm__ volatile(
               ".machine altivec\n\t"
               "lvx 2,0,%0\n\t"
               "vperm 4,2,1,0\n\t"
               "stvx 4,0,%1"
               :
               : "r"(s), "r"(d)
               : "memory");

            s += 16;
            d += 16;
            x += 4;
         }
      }

      /* Unaligned rows and the 0-3 pixel tail. */
      if (x < width)
         MorphTinyGL_RowARGB_Scalar(src + ((ULONG)y * src_pitch) + ((ULONG)x * 4UL),
                                    (ULONG *)(APTR)(d), width - x);
   }

   __asm__ volatile("mtspr 256,%0" : : "r"(old_vrsave) : "memory");
}

/* RGB565 -> RGBA8888, 8 pixels per step.
 *
 *   r8 = (r5 << 3) | (r5 >> 2)   g8 = (g6 << 2) | (g6 >> 4)   b8 likewise
 *   rg = (r8 << 8) | g8          ba = (b8 << 8) | 0xff
 *   pixels 0-3 = vmrghh(rg, ba)  pixels 4-7 = vmrglh(rg, ba)
 *
 * vr1=11 vr2=5 vr3=3 vr4=2 vr5=4 vr6=8 (shift counts), vr7=0x001f, vr8=0x003f,
 * vr9=0x00ff (masks, built from 0xffff), vr10/vr11 scratch for the setup.
 */
#define MORPHTINYGL_565_BODY \
   "vsrh 13,12,1\n\t"   /* r5 */ \
   "vsrh 14,12,2\n\t"   \
   "vand 14,14,8\n\t"   /* g6 */ \
   "vand 15,12,7\n\t"   /* b5 */ \
   "vslh 16,13,3\n\t"   "vsrh 17,13,4\n\t"  "vor 13,16,17\n\t"  /* r8 */ \
   "vslh 16,14,4\n\t"   "vsrh 17,14,5\n\t"  "vor 14,16,17\n\t"  /* g8 */ \
   "vslh 16,15,3\n\t"   "vsrh 17,15,4\n\t"  "vor 15,16,17\n\t"  /* b8 */ \
   "vslh 16,13,6\n\t"   "vor 16,16,14\n\t"                      /* rg */ \
   "vslh 17,15,6\n\t"   "vor 17,17,9\n\t"                       /* ba */ \
   "vmrghh 18,16,17\n\t" \
   "vmrglh 19,16,17\n\t" \
   "stvx 18,0,%1\n\t" \
   "stvx 19,0,%2"

static void MorphTinyGL_Convert565_AltiVec(const UBYTE *src, ULONG src_pitch,
                                            UBYTE *dst, int width, int height,
                                            int pixel_format)
{
   ULONG old_vrsave;
   ULONG new_vrsave;
   BOOL swap = (pixel_format == PIXFMT_RGB16PC) ? TRUE : FALSE;
   int y;

   __asm__ volatile("mfspr %0,256" : "=r"(old_vrsave));
   new_vrsave = old_vrsave | MORPHTINYGL_VRSAVE_MASK;
   __asm__ volatile(
      "mtspr 256,%0\n\t"
      ".machine altivec\n\t"
      "lvx 0,0,%1\n\t"
      "vspltish 1,11\n\t"
      "vspltish 2,5\n\t"
      "vspltish 3,3\n\t"
      "vspltish 4,2\n\t"
      "vspltish 5,4\n\t"
      "vspltish 6,8\n\t"
      "vspltish 10,-1\n\t"
      "vspltish 11,10\n\t"
      "vsrh 7,10,1\n\t"
      "vsrh 8,10,11\n\t"
      "vsrh 9,10,6"
      :
      : "r"(new_vrsave), "r"(MorphTinyGL_SwapMask)
      : "memory");

   for (y = 0; y < height; y++)
   {
      const UBYTE *s = src + ((ULONG)y * src_pitch);
      UBYTE *d = dst + ((ULONG)y * (ULONG)width * 4UL);
      int x = 0;

      if ((((ULONG)(APTR)s | (ULONG)(APTR)d) & 15UL) == 0)
      {
         if (swap)
         {
            while (x + 8 <= width)
            {
               UBYTE *d16 = d + 16;

               __asm__ volatile(
                  ".machine altivec\n\t"
                  "lvx 12,0,%0\n\t"
                  "vperm 12,12,12,0\n\t"
                  MORPHTINYGL_565_BODY
                  :
                  : "r"(s), "r"(d), "r"(d16)
                  : "memory");

               s += 16;
               d += 32;
               x += 8;
            }
         }
         else
         {
            while (x + 8 <= width)
            {
               UBYTE *d16 = d + 16;

               __asm__ volatile(
                  ".machine altivec\n\t"
                  "lvx 12,0,%0\n\t"
                  MORPHTINYGL_565_BODY
                  :
                  : "r"(s), "r"(d), "r"(d16)
                  : "memory");

               s += 16;
               d += 32;
               x += 8;
            }
         }
      }

      /* Unaligned rows and the 0-7 pixel tail. */
      if (x < width)
         MorphTinyGL_Row565_Scalar(src + ((ULONG)y * src_pitch) + ((ULONG)x * 2UL),
                                   (ULONG *)(APTR)d, width - x, pixel_format);
   }

   __asm__ volatile("mtspr 256,%0" : : "r"(old_vrsave) : "memory");
}

static BOOL MorphTinyGL_ConvertFrame(const UBYTE *buffer, int pixel_format,
                                      int width, int height, int pitch, int pixbytes)
{
   ULONG bytes = (ULONG)width * (ULONG)height * 4UL;
   int y;

   if (bytes > gl.rgba_capacity)
   {
      /* Keep the RGBA frame 16-byte aligned so the AltiVec stores can be used. */
      UBYTE *raw = (UBYTE *)malloc(bytes + 16UL);

      if (!raw)
         return FALSE;

      if (gl.rgba_raw)
         free(gl.rgba_raw);
      gl.rgba_raw = raw;
      gl.rgba = (UBYTE *)(((ULONG)(APTR)raw + 15UL) & ~15UL);
      gl.rgba_capacity = bytes;
   }

   if (gl.cpu_has_altivec && width >= 8)
   {
      if (pixbytes == 4)
      {
         MorphTinyGL_ConvertARGB_AltiVec(buffer, (ULONG)pitch, gl.rgba, width, height);
         return TRUE;
      }

      if (pixbytes == 2 &&
          (pixel_format == PIXFMT_RGB16 || pixel_format == PIXFMT_RGB16PC))
      {
         MorphTinyGL_Convert565_AltiVec(buffer, (ULONG)pitch, gl.rgba, width, height,
                                        pixel_format);
         return TRUE;
      }
   }

   for (y = 0; y < height; y++)
   {
      const UBYTE *src = buffer + ((ULONG)y * (ULONG)pitch);
      ULONG *dst = (ULONG *)(APTR)(gl.rgba + ((ULONG)y * (ULONG)width * 4UL));

      if (pixbytes == 4)
         MorphTinyGL_RowARGB_Scalar(src, dst, width);
      else
         MorphTinyGL_Row565_Scalar(src, dst, width, pixel_format);
   }

   return TRUE;
}

BOOL MorphTinyGL_Present(Object *obj, struct Window *window,
                         struct Screen *screen, BOOL fullscreen,
                         BOOL live_resize,
                         const UBYTE *buffer, int pixel_format,
                         int width, int height, int pitch, int pixbytes,
                         BOOL vsync)
{
   LONG area_left, area_top, area_width, area_height;
   LONG vp_left, vp_top, vp_width, vp_height;
   struct RastPort *rp;
   GLContext *context;

   if (!TinyGLBase || !obj || !window || !window->RPort || !buffer)
      return FALSE;

   if (width <= 0 || height <= 0 || pitch <= 0 || (pixbytes != 2 && pixbytes != 4))
      return FALSE;

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

   /* Nothing visible to draw into: not an error. */
   if (area_width < 1 || area_height < 1)
      return TRUE;

   /* The render bitmap is sized to the area.  Rebuilding it for every step of
    * a live resize is wasteful, so wait until the size gadget is released. */
   if (live_resize)
      return TRUE;

   if (gl.context &&
       (gl.owner_window != window || gl.owner_screen != window->WScreen ||
        gl.bm_width != area_width || gl.bm_height != area_height))
      MorphTinyGL_Destroy();

   if (!gl.context && !MorphTinyGL_CreateContext(window, area_width, area_height))
      return FALSE;

   if (!MorphTinyGL_ConvertFrame(buffer, pixel_format, width, height, pitch, pixbytes))
      return FALSE;

   context = gl.context;
   GLASetSync(context, vsync ? 1 : 0);

   GLBindTexture(context, GL_TEXTURE_2D, gl.texture);
   if (!gl.texture_ready || gl.tex_width != width || gl.tex_height != height)
   {
      GLTexImage2D(context, GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                   GL_RGBA, GL_UNSIGNED_BYTE, gl.rgba);
      gl.texture_ready = TRUE;
      gl.tex_width = width;
      gl.tex_height = height;
   }
   else
   {
      GLTexSubImage2D(context, GL_TEXTURE_2D, 0, 0, 0, width, height,
                      GL_RGBA, GL_UNSIGNED_BYTE, gl.rgba);
   }

   /* Aspect-preserving viewport inside the area; the rest stays black. */
   vp_width = area_width;
   vp_height = area_height;
   if (((unsigned long long)area_width * (unsigned long long)height) >
       ((unsigned long long)area_height * (unsigned long long)width))
   {
      vp_width = (LONG)(((unsigned long long)area_height * (unsigned long long)width) /
                        (unsigned long long)height);
      if (vp_width < 1) vp_width = 1;
   }
   else
   {
      vp_height = (LONG)(((unsigned long long)area_width * (unsigned long long)height) /
                         (unsigned long long)width);
      if (vp_height < 1) vp_height = 1;
   }
   vp_left = (area_width - vp_width) / 2;
   vp_top = (area_height - vp_height) / 2;

   GLViewport(context, 0, 0, area_width, area_height);
   GLClearColor(context, 0, 0, 0, 1);
   GLClear(context, GL_COLOR_BUFFER_BIT);

   GLViewport(context, vp_left, vp_top, vp_width, vp_height);
   GLMatrixMode(context, GL_PROJECTION);
   GLLoadIdentity(context);
   GLOrtho(context, 0, 1, 1, 0, -1, 1);
   GLMatrixMode(context, GL_MODELVIEW);
   GLLoadIdentity(context);
   GLEnable(context, GL_TEXTURE_2D);
   GLBegin(context, GL_QUADS);
   GLTexCoord2f(context, 0, 0); GLVertex2f(context, 0, 0);
   GLTexCoord2f(context, 1, 0); GLVertex2f(context, 1, 0);
   GLTexCoord2f(context, 1, 1); GLVertex2f(context, 1, 1);
   GLTexCoord2f(context, 0, 1); GLVertex2f(context, 0, 1);
   GLEnd(context);
   GLFlush(context);
   GLASwapBuffers(context);

   rp = _rp(obj);
   if (!rp)
      rp = window->RPort;

   BltBitMapRastPort(gl.bitmap, 0, 0, rp,
                     area_left, area_top, area_width, area_height, 0xc0);
   return TRUE;
}
