#include "darling/container.h"

#include "../c23/darling-type.h"
#include "nio/mem.h"
#include "oop/type.h"
#include "annotation/overview.h"

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Container
 * LEVEL: L2 — Behavior (UI layout base behavior API)
 * ============================================================================
 * Layout base of every darling node: position, size, scale, the anchor+pivot
 * system, percentage placement, z-order and the visible/enabled/dirty flags.
 *
 * STRUCT FIELDS (Mirroring darling/container.h):
 * ----------------------------------------------------------------------------
 *   float x, y, w, h;      // Position + size in parent units
 *   float scaleX, scaleY;  // Axis scale multipliers
 *   uint8_t anchor;        // CONTAINER_ANCHOR_* 0..8: where on parent during resize
 *   int32_t pivot;         // CONTAINER_PIVOT_* 0..4: element's reference point
 *   float percentX, percentY; // Percentage placement (-1 = unset)
 *   int32_t z;             // Z-order within parent
 *   uint8_t visible;       // Visibility flag
 *   uint8_t enabled;       // Enabled flag
 *   uint8_t dirty;         // Layout-dirty flag
  *   uint8_t clipping;      // Clip-children flag
  *   uint8_t lockedRoot;    // 1 = board-root pane: setSize/setLocation are no-ops (Window Board Root Lock Law)
  *   float opacity;         // 0..1 alpha multiplier over every paint (default 1)
 *   float baseW, baseH;    // Parent size at last layout (resize-delta reference)
 *   float minW, minH;      // Size constraints (default 0,0)
 *   float maxW, maxH;      // Size constraints (default 0 = unset)
 *   float marginL, marginT; // Additive margin: final = location + margin
 *   float marginR, marginB; // Right/bottom edges stored for sibling layout
 *   float radius;          // Corner radius in parent units (0 = square)
 *   int radiusMode;        // CORNER_ARC (0) or CORNER_SUPERELLIPSE (1)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Container_0(void)
 *
 * Core Functions:
 *   - Container_markDirty(c)
 *   - Container_clearDirty(c)
 *   - Container_resolve(c, parentX, parentY, parentW, parentH, outRect)
 *   - Container_hitTest(c, parentX, parentY, parentW, parentH, pointX, pointY)
 *
 * Setters:
 *   - Container_setX(c, x)
 *   - Container_setY(c, y)
 *   - Container_setWidth(c, w)
 *   - Container_setHeight(c, h)
 *   - Container_setLocation(c, x, y)
 *   - Container_setSize(c, w, h)
 *   - Container_setMinSize(c, w, h)
 *   - Container_setMaxSize(c, w, h)
 *   - Container_setScale(c, sx, sy)
 *   - Container_setAnchor(c, anchor)
 *   - Container_setPivot(c, pivot)
 *   - Container_setCenter(c)
 *   - Container_setPercentX(c, pct)
 *   - Container_setPercentY(c, pct)
 *   - Container_setZ(c, z)
 *   - Container_setVisible(c, visible)
 *   - Container_setEnabled(c, enabled)
  *   - Container_setClipChildren(c, clip)
  *   - Container_setOpacity(c, opacity)
 *   - Container_setMargin(c, l, t, r, b)
 *   - Container_setRadius(c, r)
 *   - Container_setRadiusMode(c, mode)
 *
 * Getters:
 *   - Container_getX(c)
 *   - Container_getY(c)
 *   - Container_getWidth(c)
 *   - Container_getHeight(c)
 *   - Container_getScaleWidth(c)
 *   - Container_getScaleHeight(c)
 *   - Container_getAnchor(c)
 *   - Container_getPivot(c)
 *   - Container_getPercentX(c)
 *   - Container_getPercentY(c)
 *   - Container_hasPercentX(c)
 *   - Container_hasPercentY(c)
 *   - Container_getZ(c)
 *   - Container_isVisible(c)
 *   - Container_isEnabled(c)
  *   - Container_isClipChildren(c)
  *   - Container_getOpacity(c)
 *   - Container_isDirty(c)
 *   - Container_getMargin(c, l, t, r, b)
 *   - Container_getRadius(c)
 *   - Container_getRadiusMode(c)
 *
 * Root lock (Window Board Root Lock Law):
 *   - Container_isLockedRoot(c)
 *   - Container_setLockedRoot(c, locked)
 *   - Container_forceSize(c, w, h)      // internal: Frame_resize bypass only
 *   - Container_forceLocation(c, x, y)  // internal: Frame_resize bypass only
 * ============================================================================
 */


// darling/container.c — layout core (Legacy: darling/Container.java).

Container *Container_0(void) {
    Container *c = (Container*) Memory_alloc(TYPE_CONTAINER_SINGLETON, sizeof(Container));
    if (!c)
        return nullptr;
    (*c).x = 0.0f;
    (*c).y = 0.0f;
    (*c).w = 0.0f;
    (*c).h = 0.0f;
    (*c).scaleX = 1.0f;
    (*c).scaleY = 1.0f;
    (*c).anchor = CONTAINER_ANCHOR_TOP_LEFT;
    (*c).pivot = CONTAINER_PIVOT_TOP_LEFT;
    (*c).percentX = CONTAINER_PERCENT_UNSET;
    (*c).percentY = CONTAINER_PERCENT_UNSET;
    (*c).z = 0;
    (*c).visible = 1;
    (*c).enabled = 1;
    (*c).dirty = 0;
    (*c).clipping = 0;
    (*c).lockedRoot = 0;
    (*c).opacity = 1.0f;
    (*c).baseW = 0.0f; // unset -> first resolve captures the reference
    (*c).baseH = 0.0f;
    (*c).marginL = 0.0f;
    (*c).marginT = 0.0f;
    (*c).marginR = 0.0f;
    (*c).marginB = 0.0f;
    (*c).radius = 0.0f;
    (*c).radiusMode = CORNER_ARC;
    return c;
}

float Container_getX(const Container *c) { return c ? (*c).x : 0.0f; }
float Container_getY(const Container *c) { return c ? (*c).y : 0.0f; }
float Container_getWidth(const Container *c) { return c ? (*c).w : 0.0f; }
float Container_getHeight(const Container *c) { return c ? (*c).h : 0.0f; }

static void layoutEdited(Container *c) {
    if (!c)
        return;
    (*c).dirty = 1;
    (*c).baseW = 0.0f; // invalidateBase: recapture on next resolve
    (*c).baseH = 0.0f;
}

void Container_setX(Container *c, float x) { if (c) { (*c).x = x; layoutEdited(c); } }
void Container_setY(Container *c, float y) { if (c) { (*c).y = y; layoutEdited(c); } }
void Container_setWidth(Container *c, float w) { if (c) { (*c).w = w; layoutEdited(c); } }
void Container_setHeight(Container *c, float h) { if (c) { (*c).h = h; layoutEdited(c); } }

void Container_setLocation(Container *c, float x, float y) {
    // Window Board Root Lock Law: silently no-op when this node is a locked board root.
    if (!c || (*c).lockedRoot)
        return;
    Container_setX(c, x);
    Container_setY(c, y);
}

void Container_setSize(Container *c, float w, float h) {
    if (!c) return;
    // Window Board Root Lock Law: silently no-op when this node is a locked board root.
    if ((*c).lockedRoot) return;
    // Clamp to [min, max] if constraints are configured
    if (w < (*c).minW) w = (*c).minW;
    if (h < (*c).minH) h = (*c).minH;
    if ((*c).maxW > 0.0f && w > (*c).maxW) w = (*c).maxW;
    if ((*c).maxH > 0.0f && h > (*c).maxH) h = (*c).maxH;
    Container_setWidth(c, w);
    Container_setHeight(c, h);
}

void Container_setMinSize(Container *c, float w, float h) {
    if (!c) return;
    (*c).minW = w;
    (*c).minH = h;
    // Re-clamp current size
    float cw = (*c).w;
    float ch = (*c).h;
    if (cw < w) cw = w;
    if (ch < h) ch = h;
    if ((*c).maxW > 0.0f && cw > (*c).maxW) cw = (*c).maxW;
    if ((*c).maxH > 0.0f && ch > (*c).maxH) ch = (*c).maxH;
    Container_setWidth(c, cw);
    Container_setHeight(c, ch);
}

void Container_setMaxSize(Container *c, float w, float h) {
    if (!c) return;
    (*c).maxW = w;
    (*c).maxH = h;
    // Re-clamp current size
    float cw = (*c).w;
    float ch = (*c).h;
    if (w > 0.0f && cw > w) cw = w;
    if (h > 0.0f && ch > h) ch = h;
    if (cw < (*c).minW) cw = (*c).minW;
    if (ch < (*c).minH) ch = (*c).minH;
    Container_setWidth(c, cw);
    Container_setHeight(c, ch);
}

float Container_getScaleWidth(const Container *c) { return c ? (*c).scaleX : 1.0f; }
float Container_getScaleHeight(const Container *c) { return c ? (*c).scaleY : 1.0f; }

void Container_setScale(Container *c, float sx, float sy) {
    if (!c)
        return;
    (*c).scaleX = sx;
    (*c).scaleY = sy;
    layoutEdited(c);
}

int Container_getAnchor(const Container *c) {
    return c ? (*c).anchor : CONTAINER_ANCHOR_TOP_LEFT;
}

void Container_setAnchor(Container *c, int anchor) {
    if (!c || anchor < CONTAINER_ANCHOR_TOP_LEFT || anchor > CONTAINER_ANCHOR_BOTTOM_RIGHT)
        return;
    (*c).anchor = (uint8_t)anchor;
    layoutEdited(c);
}

int Container_getPivot(const Container *c) {
    return c ? (*c).pivot : CONTAINER_PIVOT_TOP_LEFT;
}

void Container_setPivot(Container *c, int pivot) {
    if (!c || pivot < CONTAINER_PIVOT_TOP_LEFT || pivot > CONTAINER_PIVOT_CENTER)
        return;
    (*c).pivot = pivot;
    layoutEdited(c);
}

void Container_setCenter(Container *c) {
    if (!c)
        return;
    Container_setPivot(c, CONTAINER_PIVOT_CENTER);
    (*c).percentX = 0.5f;
    (*c).percentY = 0.5f;
    (*c).dirty = 1;
}

float Container_getPercentX(const Container *c) { return c ? (*c).percentX : CONTAINER_PERCENT_UNSET; }
float Container_getPercentY(const Container *c) { return c ? (*c).percentY : CONTAINER_PERCENT_UNSET; }

void Container_setPercentX(Container *c, float pct) { if (c) { (*c).percentX = pct; (*c).dirty = 1; } }
void Container_setPercentY(Container *c, float pct) { if (c) { (*c).percentY = pct; (*c).dirty = 1; } }

bool Container_hasPercentX(const Container *c) { return Container_getPercentX(c) >= 0.0f; }
bool Container_hasPercentY(const Container *c) { return Container_getPercentY(c) >= 0.0f; }

int Container_getZ(const Container *c) { return c ? (*c).z : 0; }
void Container_setZ(Container *c, int z) { if (c) { (*c).z = z; (*c).dirty = 1; } }

bool Container_isVisible(const Container *c) { return c && (*c).visible != 0; }
bool Container_isEnabled(const Container *c) { return c && (*c).enabled != 0; }
bool Container_isClipChildren(const Container *c) { return c && (*c).clipping != 0; }
bool Container_isDirty(const Container *c) { return c && (*c).dirty != 0; }

void Container_setVisible(Container *c, bool visible) { if (c) { (*c).visible = visible ? 1 : 0; (*c).dirty = 1; } }
void Container_setEnabled(Container *c, bool enabled) { if (c) { (*c).enabled = enabled ? 1 : 0; } }
void Container_setClipChildren(Container *c, bool clip) { if (c) { (*c).clipping = clip ? 1 : 0; (*c).dirty = 1; } }

void Container_setOpacity(Container *c, float opacity) {
    if (!c)
        return;
    (*c).opacity = opacity < 0.0f ? 0.0f : (opacity > 1.0f ? 1.0f : opacity);
    (*c).dirty = 1;
}

float Container_getOpacity(const Container *c) { return c ? (*c).opacity : 1.0f; }

void Container_markDirty(Container *c) {
    if (c)
        (*c).dirty = 1;
}

void Container_clearDirty(Container *c) {
    if (c)
        (*c).dirty = 0;
}

// --- Root lock (Window Board Root Lock Law) ---

bool Container_isLockedRoot(const Container *c) {
    return c && (*c).lockedRoot != 0;
}

void Container_setLockedRoot(Container *c, bool locked) {
    if (c)
        (*c).lockedRoot = locked ? 1 : 0;
}

// Internal force-setters: bypass lockedRoot guard.
// Used ONLY by Frame_resize — NOT public API.
void Container_forceSize(Container *c, float w, float h) {
    if (!c) return;
    (*c).w = w;
    (*c).h = h;
    layoutEdited(c);
}

void Container_forceLocation(Container *c, float x, float y) {
    if (!c) return;
    (*c).x = x;
    (*c).y = y;
    layoutEdited(c);
}

void Container_setMargin(Container *c, float l, float t, float r, float b) {
    if (!c)
        return;
    (*c).marginL = l;
    (*c).marginT = t;
    (*c).marginR = r;
    (*c).marginB = b;
    (*c).dirty = 1;
}

void Container_getMargin(const Container *c, float *l, float *t, float *r, float *b) {
    float ml = c ? (*c).marginL : 0.0f;
    float mt = c ? (*c).marginT : 0.0f;
    float mr = c ? (*c).marginR : 0.0f;
    float mb = c ? (*c).marginB : 0.0f;
    if (l)
        *l = ml;
    if (t)
        *t = mt;
    if (r)
        *r = mr;
    if (b)
        *b = mb;
}

void Container_setRadius(Container *c, float r) {
    if (!c)
        return;
    (*c).radius = r < 0.0f ? 0.0f : r;
    (*c).dirty = 1;
}

float Container_getRadius(const Container *c) { return c ? (*c).radius : 0.0f; }

void Container_setRadiusMode(Container *c, int mode) {
    if (!c)
        return;
    if (mode != CORNER_ARC && mode != CORNER_SUPERELLIPSE)
        return;
    (*c).radiusMode = mode;
    (*c).dirty = 1;
}

int Container_getRadiusMode(const Container *c) { return c ? (*c).radiusMode : CORNER_ARC; }

void Container_resolve(Container *c, float parentX, float parentY,
                       float parentW, float parentH, Vec4 *outRect) {
    if (!c || !outRect)
        return;

    float sw = (*c).w * (*c).scaleX;
    float sh = (*c).h * (*c).scaleY;

    // Resize-delta reference: no longer needed for new clean layout!
    (*c).dirty = 0;

    float x = (*c).x;
    float y = (*c).y;

    // Anchor: find the absolute position on the parent bounds
    float px = 0.0f;
    float py = 0.0f;
    switch (Container_getAnchor(c)) {
        case CONTAINER_ANCHOR_TOP_CENTER:    px = parentW * 0.5f; break;
        case CONTAINER_ANCHOR_TOP_RIGHT:     px = parentW;        break;
        case CONTAINER_ANCHOR_MIDDLE_LEFT:   py = parentH * 0.5f; break;
        case CONTAINER_ANCHOR_MIDDLE_CENTER: px = parentW * 0.5f; py = parentH * 0.5f; break;
        case CONTAINER_ANCHOR_MIDDLE_RIGHT:  px = parentW;        py = parentH * 0.5f; break;
        case CONTAINER_ANCHOR_BOTTOM_LEFT:   py = parentH;        break;
        case CONTAINER_ANCHOR_BOTTOM_CENTER: px = parentW * 0.5f; py = parentH;        break;
        case CONTAINER_ANCHOR_BOTTOM_RIGHT:  px = parentW;        py = parentH;        break;
        default: break; // TOP_LEFT
    }

    // Margin direction based on anchor (pushes inward from the anchor edge)
    float marginX = x;
    float marginY = y;
    int a = Container_getAnchor(c);
    if (a == CONTAINER_ANCHOR_TOP_RIGHT || a == CONTAINER_ANCHOR_MIDDLE_RIGHT || a == CONTAINER_ANCHOR_BOTTOM_RIGHT)
        marginX = -x; // right-anchored margins pull left
    if (a == CONTAINER_ANCHOR_BOTTOM_LEFT || a == CONTAINER_ANCHOR_BOTTOM_CENTER || a == CONTAINER_ANCHOR_BOTTOM_RIGHT)
        marginY = -y; // bottom-anchored margins pull up

    float screenX = parentX + px + marginX;
    float screenY = parentY + py + marginY;

    // Phase 1 margin law: final = location + margin, applied at resolve time.
    // Stored location is never rewritten; zero margins resolve bit-identically.
    screenX += (*c).marginL;
    screenY += (*c).marginT;

    // Percent overrides placement against the LIVE parent size.
    if (Container_hasPercentX(c))
        screenX = parentX + (*c).percentX * parentW;
    if (Container_hasPercentY(c))
        screenY = parentY + (*c).percentY * parentH;

    // Pivot shift: the pivot point lands at the resolved target.
    float offX = 0.0f;
    float offY = 0.0f;
    switch (Container_getPivot(c)) {
        case CONTAINER_PIVOT_TOP_RIGHT:    offX = sw; break;
        case CONTAINER_PIVOT_BOTTOM_LEFT:  offY = sh; break;
        case CONTAINER_PIVOT_BOTTOM_RIGHT: offX = sw; offY = sh; break;
        case CONTAINER_PIVOT_CENTER:       offX = sw * 0.5f; offY = sh * 0.5f; break;
        default:
            break; // TOP_LEFT
    }
    screenX -= offX;
    screenY -= offY;

    Vec4_set(outRect, screenX, screenY, sw, sh);
}

bool Container_hitTest(Container *c, float parentX, float parentY,
                       float parentW, float parentH, float pointX, float pointY) {
    if (!Container_isVisible(c))
        return false;
    Vec4 rect;
    Container_resolve(c, parentX, parentY, parentW, parentH, &rect);
    // legacy stores rects as [x, y, w, h] in Vec4 slots -> width=.z height=.w
    return pointX >= rect.x && pointX < rect.x + rect.z
        && pointY >= rect.y && pointY < rect.y + rect.w;
}
