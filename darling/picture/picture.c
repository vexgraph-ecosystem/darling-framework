#include "annotation/definition.h"
#include "annotation/overview.h"
#include "annotation/getter.h"
#include "annotation/setter.h"
#include "darling/picture/picture.h"
#include "nio/mem.h"
#include "oop/type.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Picture
 * ============================================================================
 * The UI picture: a Panel (Container layout + Component presentation + the
 * background color) whose stage-1 image hook draws a graphvex Image INSIDE it.
 * The node itself is tiny — the asset lives in the Panel's shared payload slot
 * (Panel_setImage, aliased through views like every payload), so the picture's
 * own state is just the fit MODE and the source-pixel WINDOW that crops it.
 *
 * Painting is two stages the Panel pipeline already orders: stage 0 fills the
 * background color from the embedded Component, stage 1 (this class's
 * @Override, installed in the constructor) draws the image through the R3 fit
 * contract — Graphics_drawImageFit, which resolves Image_fitRect, scissors the
 * overflow, draws, and resets. The picture owns no renderer, no Vulkan handle,
 * and no texture registry: the row is the graphics seam (the Vertical
 * Integration Law), and an unbound picture simply shows its background.
 *
 * The window is the crop system: fillWidth / fillHeight name ONE source-pixel
 * window from whichever axis the caller thinks in (the other derives from the
 * widget's aspect), the window scales to fill the widget, unset means widget
 * pixels (true 1:1), and it always clamps to the image — you cannot show
 * pixels that do not exist. The base Panel is the FIRST member, so a Panel*
 * handed to the hook puns back to the Picture* with no offset.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Picture (inherits Panel -> Container)
 * LEVEL: L2 — Behavior (UI picture node behavior API)
 * ============================================================================
 * A Panel that draws a fitted image inside itself: background color from the
 * embedded Component (stage 0), the image from the R3 fit contract (stage 1).
 *
 * STRUCT FIELDS (Mirroring darling/picture/picture.h):
 * ----------------------------------------------------------------------------
 *   Panel base;         // Inherited panel state. FIRST: the Panel/Picture pun
 *                       // depends on the prefix. Carries the shared image slot.
 *   float fillWidth;    // Source-pixel window width (0 = unset -> widget px 1:1)
 *   float fillHeight;   // Source-pixel window height (0 = unset; other axis derives)
 *   PictureMode mode;   // Fit family: stretch / cover / contain / anchored window
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Picture()          : Picture_0()
 *   - Picture(image)     : Picture_1(image)
 *
 * Core Functions:
 *   - picturePaintImage(panel, rect) : Stage 1 hook — fitted image via the row
 *   - Picture_cycleMode(p)           : Advance to the next PictureMode
 *   - Picture_getModeName(mode)      : String label for a PictureMode
 *   - Picture_free(p)                : Free the node (the image is borrowed)
 *
 * Private Core Functions: (.c static)
 *   - pictureFit(mode, outFit, outAnchor) : PictureMode -> (ImageFitMode, ImageAnchor)
 *
 * Setters:
 *   - Picture_setImage(p, image)     : Panel payload slot (view-aliased)
 *   - Picture_setMode(p, mode)
 *   - Picture_setFillWidth(p, px)    : Source window width (clears the height door)
 *   - Picture_setFillHeight(p, px)   : Source window height (clears the width door)
 *   - Picture_clearFillWindow(p)     : Back to widget pixels (1:1)
 *
 * Getters:
 *   - Picture_getImage(p)
 *   - Picture_getMode(p)
 *   - Picture_getFillWidth(p) / Picture_getFillHeight(p)
 * ============================================================================
 */

// ============================================================================
// CORE FUNCTIONS (PUBLIC & PRIVATE)
// ============================================================================

// PictureMode -> the R3 fit contract: the three scaling families plus the
// anchored source-window family.
static void pictureFit(PictureMode mode, ImageFitMode *outFit, ImageAnchor *outAnchor) {
    ImageFitMode fit = IMAGE_FIT_STRETCH;
    ImageAnchor anchor = IMAGE_ANCHOR_CENTER;
    switch (mode) {
        case PICTURE_MODE_ZOOM_FILL:
            fit = IMAGE_FIT_COVER;
            break;
        case PICTURE_MODE_ZOOM_FIT:
            fit = IMAGE_FIT_CONTAIN;
            break;
        case PICTURE_MODE_FILL_CENTER:
            fit = IMAGE_FIT_WINDOW;
            anchor = IMAGE_ANCHOR_CENTER;
            break;
        case PICTURE_MODE_FILL_TOP_LEFT:
            fit = IMAGE_FIT_WINDOW;
            anchor = IMAGE_ANCHOR_TOP_LEFT;
            break;
        case PICTURE_MODE_FILL_TOP_RIGHT:
            fit = IMAGE_FIT_WINDOW;
            anchor = IMAGE_ANCHOR_TOP_RIGHT;
            break;
        case PICTURE_MODE_FILL_BOTTOM_LEFT:
            fit = IMAGE_FIT_WINDOW;
            anchor = IMAGE_ANCHOR_BOTTOM_LEFT;
            break;
        case PICTURE_MODE_FILL_BOTTOM_RIGHT:
            fit = IMAGE_FIT_WINDOW;
            anchor = IMAGE_ANCHOR_BOTTOM_RIGHT;
            break;
        case PICTURE_MODE_FIT:
        default:
            fit = IMAGE_FIT_STRETCH;
            break;
    }
    *outFit = fit;
    *outAnchor = anchor;
}

// Stage 1: the fitted image. Background (stage 0) has already run beneath.
// The panel pointer IS the Picture's base (first member), so the pun is stable.
static bool picturePaintImage(Panel *panel, const Rectangle *rect) {
    if (!panel || !rect)
        return false;
    Picture *p = (Picture*) panel;
    Image *image = Panel_getImage(panel);
    if (image == nullptr)
        return false;
    if (GraphicsComponent_getOpacity(&(*panel).component) <= 0.0f)
        return false;
    ImageFitMode fit;
    ImageAnchor anchor;
    pictureFit((*p).mode, &fit, &anchor);
    return Graphics_drawImageFit(image, rect, fit, anchor,
                                 (*p).fillWidth, (*p).fillHeight, nullptr);
}

// ============================================================================
// CONSTRUCTORS (PUBLIC & PRIVATE)
// ============================================================================

Picture *Picture_0(void) {
    Picture *p = (Picture*) Memory_alloc(TYPE_PICTURE_SINGLETON, sizeof(Picture));
    if (!p)
        return nullptr;

    Panel *basePanel = Panel_0();
    if (!basePanel) {
        Memory_free(p);
        return nullptr;
    }

    // adopt the panel block's contents into our prefix, then free the shell
    (*p).base = (*basePanel);
    Memory_free(basePanel);

    (*p).fillWidth = 0.0f;
    (*p).fillHeight = 0.0f;
    (*p).mode = PICTURE_MODE_FIT;

    // The @Override: stage 1 draws the image (stage 0 keeps the Panel default).
    Panel_setImageFn(&(*p).base, picturePaintImage);
    return p;
}

Picture *Picture_1(Image *image) {
    Picture *p = Picture_0();
    if (p && image)
        Panel_setImage(&(*p).base, image);
    return p;
}

void Picture_free(Picture *p) {
    if (!p)
        return;
    // The image is a borrowed asset (the caller owns it) — never freed here.
    Memory_free(p);
}

// ============================================================================
// SETTERS (PUBLIC & PRIVATE)
// ============================================================================

;;SETTER
void Picture_setImage(Picture *p, Image *image) {
    if (!p)
        return;
    Panel_setImage(&(*p).base, image);
}

;;SETTER
void Picture_setMode(Picture *p, PictureMode mode) {
    if (!p)
        return;
    (*p).mode = mode;
}

;;SETTER
void Picture_setFillWidth(Picture *p, float sourceWidthPx) {
    if (!p)
        return;
    (*p).fillWidth = sourceWidthPx > 0.0f ? sourceWidthPx : 0.0f;
    (*p).fillHeight = 0.0f;   // one window, two doors: the last axis wins
}

;;SETTER
void Picture_setFillHeight(Picture *p, float sourceHeightPx) {
    if (!p)
        return;
    (*p).fillHeight = sourceHeightPx > 0.0f ? sourceHeightPx : 0.0f;
    (*p).fillWidth = 0.0f;    // one window, two doors: the last axis wins
}

;;SETTER
void Picture_clearFillWindow(Picture *p) {
    if (!p)
        return;
    (*p).fillWidth = 0.0f;
    (*p).fillHeight = 0.0f;
}

void Picture_cycleMode(Picture *p) {
    if (!p)
        return;
    (*p).mode = (PictureMode) (((int) (*p).mode + 1) % PICTURE_MODE_COUNT);
}

const char *Picture_getModeName(PictureMode mode) {
    switch (mode) {
        case PICTURE_MODE_FIT:               return "FIT";
        case PICTURE_MODE_ZOOM_FILL:         return "ZOOM_FILL";
        case PICTURE_MODE_ZOOM_FIT:          return "ZOOM_FIT";
        case PICTURE_MODE_FILL_CENTER:       return "FILL_CENTER";
        case PICTURE_MODE_FILL_TOP_LEFT:     return "FILL_TOP_LEFT";
        case PICTURE_MODE_FILL_TOP_RIGHT:    return "FILL_TOP_RIGHT";
        case PICTURE_MODE_FILL_BOTTOM_LEFT:  return "FILL_BOTTOM_LEFT";
        case PICTURE_MODE_FILL_BOTTOM_RIGHT: return "FILL_BOTTOM_RIGHT";
        default:                             return "UNKNOWN";
    }
}

// ============================================================================
// GETTERS (PUBLIC & PRIVATE)
// ============================================================================

;;GETTER
Image *Picture_getImage(const Picture *p) {
    return p ? Panel_getImage(&(*p).base) : nullptr;
}

;;GETTER
PictureMode Picture_getMode(const Picture *p) {
    return p ? (*p).mode : PICTURE_MODE_FIT;
}

;;GETTER
float Picture_getFillWidth(const Picture *p) {
    return p ? (*p).fillWidth : 0.0f;
}

;;GETTER
float Picture_getFillHeight(const Picture *p) {
    return p ? (*p).fillHeight : 0.0f;
}
