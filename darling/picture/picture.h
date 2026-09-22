#ifndef DARLING_PICTURE_H
#define DARLING_PICTURE_H

#include <stdbool.h>
#include <stdint.h>

#include "c23/constructor.h"
#include "darling/panel/panel.h"
#include "lang/image.h"
#include "annotation/intention.h"

;;INTENTION("Picture is a Panel whose stage-1 image hook draws a graphvex Image through the R3 fit contract (Image_fitRect / Graphics_drawImageFit): background color from the embedded Component, image inside, and the eight picture modes spanning stretch, cover, contain, and the anchored source-pixel window (setFillWidth / setFillHeight).")

// darling/picture/picture.h — the UI picture: Panel + a fitted image.
//
// A Picture IS-A Panel (Container layout + Component presentation + the
// background color) with exactly one extra concern: the image drawn inside it.
// The asset lives in the Panel's shared payload slot (Panel_setImage, aliased
// through views like every payload) and the picture's own state is the MODE
// plus the source-pixel WINDOW that crops it.
//
// The eight modes map onto the R3 fit contract (lang/image.h):
//
//   PICTURE_MODE_FIT               -> IMAGE_FIT_STRETCH  (whole image into dst)
//   PICTURE_MODE_ZOOM_FILL         -> IMAGE_FIT_COVER    (cover, centered, crop)
//   PICTURE_MODE_ZOOM_FIT          -> IMAGE_FIT_CONTAIN  (contain, centered)
//   PICTURE_MODE_FILL_CENTER       -> IMAGE_FIT_WINDOW   + anchor CENTER
//   PICTURE_MODE_FILL_TOP_LEFT     -> IMAGE_FIT_WINDOW   + anchor TOP_LEFT
//   PICTURE_MODE_FILL_TOP_RIGHT    -> IMAGE_FIT_WINDOW   + anchor TOP_RIGHT
//   PICTURE_MODE_FILL_BOTTOM_LEFT  -> IMAGE_FIT_WINDOW   + anchor BOTTOM_LEFT
//   PICTURE_MODE_FILL_BOTTOM_RIGHT -> IMAGE_FIT_WINDOW   + anchor BOTTOM_RIGHT
//
// The WINDOW family is the crop system: fillWidth / fillHeight name the same
// window in source pixels — whichever axis you think in — the other derives
// from the widget's aspect, and the window scales to fill the widget (a
// 300-px window in a 600-px widget draws at 2x). Unset means widget pixels
// (true 1:1) and the window always clamps to the image.

typedef enum PictureMode {
    PICTURE_MODE_FIT = 0,               // stretch the whole image into the panel
    PICTURE_MODE_ZOOM_FILL,             // scale to cover, centered (crop)
    PICTURE_MODE_ZOOM_FIT,              // scale to contain, centered
    PICTURE_MODE_FILL_CENTER,           // source window, centered
    PICTURE_MODE_FILL_TOP_LEFT,         // source window, top-left anchored
    PICTURE_MODE_FILL_TOP_RIGHT,        // source window, top-right anchored
    PICTURE_MODE_FILL_BOTTOM_LEFT,      // source window, bottom-left anchored
    PICTURE_MODE_FILL_BOTTOM_RIGHT,     // source window, bottom-right anchored
} PictureMode;

#define PICTURE_MODE_COUNT 8

typedef struct Picture {
    Panel base;         // FIRST member: the (Panel*) <-> (Picture*) pun depends on it.
    float fillWidth;    // source-pixel window width (0 = unset -> widget px 1:1)
    float fillHeight;   // source-pixel window height (0 = unset; the other axis derives)
    PictureMode mode;   // which fit family the image draws with
} Picture;

Picture *Picture_0(void);
Picture *Picture_1(Image *image);

#define Picture(...) CONSTRUCTOR_DISPATCH(Picture, __VA_ARGS__)

// Free the node. The image is a borrowed asset (the caller owns it); freeing
// the picture never frees the image (the Teardown Order Law).
void Picture_free(Picture *p);

// The asset (the Panel's shared payload slot, aliased through views).
Image *Picture_getImage(const Picture *p);
void Picture_setImage(Picture *p, Image *image);

// Mode (the fit family).
void Picture_setMode(Picture *p, PictureMode mode);
PictureMode Picture_getMode(const Picture *p);
void Picture_cycleMode(Picture *p);
const char *Picture_getModeName(PictureMode mode);

// The source-pixel window (the crop): one window, two doors. Setting either
// clears the other; the other axis derives from the widget's aspect at paint.
void Picture_setFillWidth(Picture *p, float sourceWidthPx);
float Picture_getFillWidth(const Picture *p);
void Picture_setFillHeight(Picture *p, float sourceHeightPx);
float Picture_getFillHeight(const Picture *p);
void Picture_clearFillWindow(Picture *p);

// Layout facade — inherit from Panel
static inline void Picture_setLocation(Picture *p, float x, float y)
    { if (p) Panel_setLocation(&(*p).base, x, y); }
static inline void Picture_setSize(Picture *p, float w, float h)
    { if (p) Panel_setSize(&(*p).base, w, h); }
static inline void Picture_setAnchor(Picture *p, int anchor)
    { if (p) Panel_setAnchor(&(*p).base, anchor); }
static inline void Picture_setPivot(Picture *p, int pivot)
    { if (p) Panel_setPivot(&(*p).base, pivot); }
static inline void Picture_setBackgroundColor(Picture *p, uint32_t color)
    { if (p) Panel_setBackgroundColor(&(*p).base, color); }
static inline uint32_t Picture_getBackgroundColor(const Picture *p)
    { return p ? Panel_getBackgroundColor(&(*p).base) : PANEL_COLOR_CLEAR; }

#endif
