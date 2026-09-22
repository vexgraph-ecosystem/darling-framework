#include "darling/scene/canvas.h"

#include "darling/component.h"
#include "darling/panel/panel.h"
#include "darling/picture/picture.h"
#include "nio/mem.h"
#include "oop/type.h"
#include "annotation/definition.h"
#include "annotation/overview.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Canvas
 * ============================================================================
 * The flat 2D layout root: one canonical coordinate space the whole UI
 * resolves into, with a fixed virtual resolution independent of the
 * framebuffer and backing scale. A node placed at canvas (100, 60) stays
 * there no matter how the window changes — only the projection moves. Canvas
 * owns no tree: it is a pure projection/geometry handle the compositor owns
 * and passes, computing the visible rect, the Y-down orthographic projection,
 * root-node resolution, and window-to-canvas mapping (all dest-last) from the
 * virtual size, mapping mode (STRETCH/FIT/PIXEL), and DPI scale. Per the
 * Forward Rendering & Bounded Surface Law, the canvas bounds every surface it
 * projects into.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Canvas
 * LEVEL: L2 — Behavior (UI layout/projection behavior API)
 * ============================================================================
 * The flat 2D layout root: one canonical coordinate space the whole UI
 * resolves into, with a fixed virtual resolution, mapping mode, and DPI
 * scale driving projection and visible-rect computation.
 *
 * STRUCT FIELDS (Mirroring darling/scene/canvas.h):
 * ----------------------------------------------------------------------------
 *   float virtualWidth;    // Virtual space width (<= 0 = follow framebuffer)
 *   float virtualHeight;   // Virtual space height (<= 0 = follow framebuffer)
 *   int mode;              // CANVAS_MODE_STRETCH/FIT/PIXEL mapping mode
 *   float dpiScale;        // Backing-store DPI scale factor
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Canvas_0(void)
 *
 * Core Functions:
 *   - Canvas_visibleRect(c, fbW, fbH, outRect)
 *   - Canvas_buildProjection(c, fbW, fbH, dest)
 *   - Canvas_resolveRoot(c, node, fbW, fbH, outRect)
 *   - Canvas_windowToCanvas(c, winX, winY, fbW, fbH, outPoint)
 *
 * Setters:
 *   - Canvas_setVirtualSize(c, width, height)
 *   - Canvas_setMode(c, mode)
 *   - Canvas_setDpiScale(c, scale)
 *
 * Getters:
 *   - Canvas_getVirtualWidth(c)
 *   - Canvas_getVirtualHeight(c)
 *   - Canvas_getMode(c)
 *   - Canvas_getDpiScale(c)
 * ============================================================================
 */


// darling/canvas.c — canonical 2D space + projection (Legacy: Canvas.java).

Canvas *Canvas_0(void) {
    Canvas *c = (Canvas*) Memory_alloc(TYPE_CONTAINER_SINGLETON, sizeof(Canvas));
    if (!c)
        return nullptr;
    (*c).virtualWidth = 0.0f;  // follow framebuffer
    (*c).virtualHeight = 0.0f;
    (*c).mode = CANVAS_MODE_PIXEL;
    (*c).dpiScale = 1.0f;
    return c;
}

void Canvas_setVirtualSize(Canvas *c, float width, float height) {
    if (!c)
        return;
    (*c).virtualWidth = width;
    (*c).virtualHeight = height;
}

float Canvas_getVirtualWidth(const Canvas *c) { return c ? (*c).virtualWidth : 0.0f; }
float Canvas_getVirtualHeight(const Canvas *c) { return c ? (*c).virtualHeight : 0.0f; }

void Canvas_setMode(Canvas *c, int mode) {
    if (!c || mode < CANVAS_MODE_STRETCH || mode > CANVAS_MODE_PIXEL)
        return;
    (*c).mode = mode;
}

int Canvas_getMode(const Canvas *c) {
    return c ? (*c).mode : CANVAS_MODE_PIXEL;
}

void Canvas_setDpiScale(Canvas *c, float scale) {
    if (!c)
        return;
    (*c).dpiScale = scale > 0.0f ? scale : 1.0f;
}

float Canvas_getDpiScale(const Canvas *c) {
    return c ? (*c).dpiScale : 1.0f;
}

static float resolveSize(float virtual, float fb) {
    return virtual > 0.0f ? virtual : fb;
}

// The per-mode scale/offset pair, shared by visibleRect/projection/inv mapping.
static void mapping(const Canvas *c, float fbW, float fbH,
                    float *scaleX, float *scaleY, float *ox, float *oy) {
    float vw = resolveSize((*c).virtualWidth, fbW);
    float vh = resolveSize((*c).virtualHeight, fbH);

    switch ((*c).mode) {
        case CANVAS_MODE_STRETCH:
            *scaleX = fbW / vw;
            *scaleY = fbH / vh;
            *ox = 0.0f;
            *oy = 0.0f;
            break;
        case CANVAS_MODE_FIT: {
            float s = fbW / vw < fbH / vh ? fbW / vw : fbH / vh;
            *scaleX = s;
            *scaleY = s;
            *ox = (fbW - vw * s) * 0.5f;
            *oy = (fbH - vh * s) * 0.5f;
            break;
        }
        default: // PIXEL
            *scaleX = (*c).dpiScale;
            *scaleY = (*c).dpiScale;
            *ox = 0.0f;
            *oy = 0.0f;
            break;
    }
}

void Canvas_visibleRect(const Canvas *c, float fbW, float fbH, Vec4 *outRect) {
    if (!c || !outRect)
        return;
    if ((*c).mode == CANVAS_MODE_PIXEL) {
        Vec4_set(outRect, 0.0f, 0.0f, fbW / (*c).dpiScale, fbH / (*c).dpiScale);
        return;
    }
    float sx = 1.0f, sy = 1.0f, ox = 0.0f, oy = 0.0f;
    mapping(c, fbW, fbH, &sx, &sy, &ox, &oy);
    float vw = resolveSize((*c).virtualWidth, fbW);
    float vh = resolveSize((*c).virtualHeight, fbH);
    Vec4_set(outRect, ox, oy, vw * sx, vh * sy);
}

bool Canvas_buildProjection(const Canvas *c, float fbW, float fbH, Mat4 *dest) {
    if (!c || !dest || fbW <= 0.0f || fbH <= 0.0f)
        return false;

    float sx = 1.0f, sy = 1.0f, ox = 0.0f, oy = 0.0f;
    mapping(c, fbW, fbH, &sx, &sy, &ox, &oy);

    Mat4_zero(dest);
    Mat4_set(dest, 0, 0, 2.0f * sx / fbW);
    Mat4_set(dest, 1, 1, 2.0f * sy / fbH);
    Mat4_set(dest, 0, 3, 2.0f * ox / fbW - 1.0f);
    Mat4_set(dest, 1, 3, 2.0f * oy / fbH - 1.0f);
    Mat4_set(dest, 2, 2, 1.0f);
    Mat4_set(dest, 3, 3, 1.0f);
    return true;
}

void Canvas_resolveRoot(const Canvas *c, void *node, float fbW, float fbH, Vec4 *outRect) {
    if (!c || !node || !outRect)
        return;

    float cw;
    float ch;
    if ((*c).mode == CANVAS_MODE_PIXEL) {
        cw = fbW / (*c).dpiScale;
        ch = fbH / (*c).dpiScale;
    } else {
        cw = resolveSize((*c).virtualWidth, fbW);
        ch = resolveSize((*c).virtualHeight, fbH);
    }

    uint32_t classId = Type_class(Memory_type(node));
    if (classId == ID_PICTURE) {
        Picture *pic = (Picture*) node;
        Panel *panel = &(*pic).base;
        Component *meta = &(*panel).component;
        Component_setParentAbs(meta, 0.0f, 0.0f, cw, ch);
        Component_getAbsRect(meta, outRect);
    } else {
        Panel *panel = (Panel*) node;
        Component *meta = &(*panel).component;
        Component_setParentAbs(meta, 0.0f, 0.0f, cw, ch);
        Component_getAbsRect(meta, outRect);
    }
}

bool Canvas_windowToCanvas(const Canvas *c, float winX, float winY,
                           float fbW, float fbH, Vec2 *outPoint) {
    if (!c || !outPoint)
        return false;

    float vw = resolveSize((*c).virtualWidth, fbW);
    float vh = resolveSize((*c).virtualHeight, fbH);
    float scaleX = 1.0f, scaleY = 1.0f, ox = 0.0f, oy = 0.0f;

    if ((*c).mode != CANVAS_MODE_PIXEL)
        mapping(c, fbW, fbH, &scaleX, &scaleY, &ox, &oy);

    float canvasX = (winX - ox) / scaleX;
    float canvasY = (winY - oy) / scaleY;
    Vec2_set(outPoint, canvasX, canvasY);

    return canvasX >= 0.0f && canvasX < vw && canvasY >= 0.0f && canvasY < vh;
}
