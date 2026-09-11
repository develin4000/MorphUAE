#ifndef MORPHOS_OVERLAY_H
#define MORPHOS_OVERLAY_H

#include <exec/types.h>
#include <libraries/mui.h>

struct Screen;
struct Window;

/* CGXVideo overlay backend. There is one Render MCC instance in MorphUAE,
 * therefore the backend keeps one private VLayer state.
 */
void MorphOverlay_Init(void);
void MorphOverlay_DetectCPU(void);
void MorphOverlay_Destroy(void);

BOOL MorphOverlay_HasLayer(void);
BOOL MorphOverlay_IsActive(void);
BOOL MorphOverlay_IsFailed(void);
BOOL MorphOverlay_IsSuspended(void);
void MorphOverlay_SetFailed(BOOL failed);
void MorphOverlay_ResetRuntimeState(void);
void MorphOverlay_Suspend(void);
void MorphOverlay_Resume(void);

BOOL MorphOverlay_FullscreenScreenIsActive(BOOL fullscreen,
                                            struct Screen *screen,
                                            struct Window *window);
void MorphOverlay_MarkBackingDirty(void);
void MorphOverlay_UpdateGeometry(Object *obj, struct Window *window,
                                 BOOL fullscreen,
                                 ULONG src_width, ULONG src_height);
void MorphOverlay_ClearBacking(Object *obj, struct Window *window,
                               BOOL fullscreen);

BOOL MorphOverlay_Attach(Object *obj, struct Window *window,
                         struct Screen *screen, BOOL fullscreen,
                         ULONG src_width, ULONG src_height);
BOOL MorphOverlay_Present(Object *obj, struct Window *window,
                          struct Screen *screen, BOOL fullscreen,
                          BOOL live_resize,
                          const UBYTE *buffer, int pixel_format,
                          int width, int height, int pitch, int pixbytes,
                          BOOL vsync);

#endif /* MORPHOS_OVERLAY_H */
