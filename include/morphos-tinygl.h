#ifndef MORPHOS_TINYGL_H
#define MORPHOS_TINYGL_H

#include <exec/types.h>
#include <libraries/mui.h>

struct Screen;
struct Window;
struct Library;

/* TinyGL presentation backend.  The emulated frame is uploaded as a texture
 * and drawn as one textured quad into an offscreen 3D bitmap, which is then
 * blitted into the MUI window.  Scaling, aspect-correct letterboxing and
 * (optional) linear filtering are done by TinyGL.
 *
 * There is one Render MCC instance in MorphUAE, so the backend keeps a single
 * private context.  morphos-win.c falls back to the CGXVideo overlay and then
 * to the plain surface path if anything here fails.
 */
extern struct Library *TinyGLBase;

BOOL MorphTinyGL_Init(void);          /* opens tinygl.library; FALSE if missing */
void MorphTinyGL_Shutdown(void);      /* destroys context, closes the library   */
void MorphTinyGL_Destroy(void);       /* destroys context/bitmap/texture only   */

BOOL MorphTinyGL_IsAvailable(void);
BOOL MorphTinyGL_IsAttached(void);    /* a GL context currently exists          */
BOOL MorphTinyGL_IsFailed(void);
void MorphTinyGL_SetFailed(BOOL failed);

/* Draw one emulated frame.  Creates (or recreates after a window/size change)
 * the context on demand.  Returns FALSE on failure so the caller can fall back.
 */
BOOL MorphTinyGL_Present(Object *obj, struct Window *window,
                         struct Screen *screen, BOOL fullscreen,
                         BOOL live_resize,
                         const UBYTE *buffer, int pixel_format,
                         int width, int height, int pitch, int pixbytes,
                         BOOL vsync);

#endif /* MORPHOS_TINYGL_H */
