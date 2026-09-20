#include "darling/component.h"

#include <string.h>

#include "../c23/darling-type.h"
#include "graphics/graphics.h"
#include "lang/rect/rectangle.h"
#include "paint/brush.h"
#include "paint/stroke.h"
#include "nio/mem.h"
#include "oop/type.h"
#include "annotation/definition.h"
#include "annotation/overview.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Component
 * ============================================================================
 * The renderable leaf of the new darling architecture: geometry +
 * presentation state + an ABSOLUTE rect recomputed eagerly on every geometry
 * setter, so a renderer consumes ((*self).absX..absH) directly — no resolve
 * phase, no parent-size threading, no stale reads. A Component is a leaf on
 * its own: NO children, NO tree, NO dirty flag (eager abs replaces the
 * dirty+resolve pass); containment is optional by construction — an element
 * that does not want to be added is exactly a Component without a Container.
 * The eager abs cascade is O(1) per leaf: setters recompute this component's
 * abs immediately against the parent's abs box (stored via
 * Component_setParentAbs), and setters never layout, so the whole tree cost
 * equals one interleaved resolve pass. The anchor+pivot system mirrors
 * Container's exactly for a value-identical migration.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Component
 * LEVEL: L2 — Behavior (immediate on-demand rendering leaf)
 * ============================================================================
 * The renderable leaf of the new darling architecture: geometry +
 * presentation state + an ABSOLUTE rect computed eagerly on every geometry
 * setter, so a renderer consumes ((*self).absX..absH) directly — no resolve
 * phase, no parent-size threading, no stale reads. A Component is a leaf on
 * its own: NO children, NO tree, NO dirty flag (eager abs replaces the
 * dirty+resolve pass). Containment is optional by construction: an element
 * that does not want to be added is exactly a Component without a
 * Container — the two classes are different things by design.
 *
 * ;;INTENTION("eager abs cascade per the Present-On-Demand Law: geometry
 * setters recompute abs immediately; parent containers cascade via
 * Component_setParentAbs; setters never layout, so recompute is O(1) per
 * leaf and the whole tree cost equals one resolve pass, interleaved")
 *
 * STRUCT FIELDS (Mirroring darling/component.h):
 * ----------------------------------------------------------------------------
 *   float x, y, w, h;      // Placement + size in parent units; x/y direction
 *                          // governed by origin (4 corners)
 *   uint8_t origin;        // COMPONENT_ORIGIN_* 0..3 (4 corners: coordinate frame)
 *   uint8_t anchor;        // COMPONENT_ANCHOR_* 0..8 (9-grid docking point on parent)
 *   int32_t pivot;         // COMPONENT_PIVOT_* 0..4 (5 child points: 4 corners + center)
 *   float minW, minH;      // Size constraints (0 = unset)
 *   float maxW, maxH;      // Size constraints (0 = unset)
 *   float marginL, marginT; // Additive placement: final = resolved + margin
 *   float marginR, marginB; // Right/bottom edges stored for sibling layout
 *   float paddingL, paddingT; // Inward content insets (content box = abs + padding)
 *   float paddingR, paddingB;
 *   float borderWidth;     // 0 = no border
 *   uint32_t borderColor;  // 0xAARRGGBB
 *   uint32_t backgroundColor; // 0xAARRGGBB (consumed by render hooks)
 *   float radius;          // Corner radius (0 = square)
 *   int radiusMode;        // COMPONENT_CORNER_ARC (0) / COMPONENT_CORNER_SUPERELLIPSE (1)
 *   float opacity;         // 0..1 alpha multiplier (1 = opaque)
 *   int32_t z;             // Z-order
 *   uint8_t visible;       // Visibility gate for render/hitTest
 *   Component *parent;     // Borrowed view, nullptr = root; NEVER owned
 *   Component **children;  // Child components owned or attached
 *   uint32_t childCount;
 *   uint32_t childCapacity;
 *   float absX, absY;      // Absolute left/top in the rendering space (eager)
 *   float absW, absH;      // Absolute extent (eager)
 *   float parentAbsX, parentAbsY; // Parent abs box at last recompute
 *   float parentAbsW, parentAbsH;
 *   Component_RenderFn backgroundRender; // Stage 0 render hook
 *   Component_RenderFn foregroundRender; // Stage 1 render hook
 *   void *renderUserdata;  // Opaque arg handed to both hooks
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Component_0(void)
 *
 * Core Functions:
 *   - Component_recompute(self)
 *   - Component_setParentAbs(self, px, py, pw, ph)
 *   - Component_render(self, graphics)
 *   - Component_hitTest(self, pointX, pointY)
 *   - Component_getContentRect(self, outX, outY, outW, outH)
 *   - Component_addChild(self, child)
 *   - Component_removeChild(self, child)
 *
 * Setters:
 *   - Component_setX/Y(self, v)
 *   - Component_setWidth/Height(self, v)
 *   - Component_setLocation(self, x, y)
 *   - Component_setSize(self, w, h)          // clamps [min, max]
 *   - Component_setMinSize/MaxSize(self, w, h) // re-clamps current size
 *   - Component_setOrigin(self, origin)
 *   - Component_setAnchor(self, anchor)
 *   - Component_setPivot(self, pivot)
 *   - Component_setCenter(self)
 *   - Component_setMargin(self, l, t, r, b)
 *   - Component_setPadding(self, l, t, r, b)
 *   - Component_setBorderWidth(self, w)
 *   - Component_setBorderColor(self, color)
 *   - Component_setBackgroundColor(self, color)
 *   - Component_setRadius(self, r)
 *   - Component_setRadiusMode(self, mode)
 *   - Component_setOpacity(self, opacity)
 *   - Component_setZ(self, z)
 *   - Component_setVisible(self, visible)
 *   - Component_setParent(self, parent)
 *   - Component_setBackgroundRender(self, fn)
 *   - Component_setForegroundRender(self, fn)
 *   - Component_setRenderUserdata(self, userdata)
 *
 * Getters:
 *   - Component_getX/Y/Width/Height(self)
 *   - Component_getAbsX/Y/W/H(self)
 *   - Component_getAbsRect(self, outRect)
 *   - Component_getParentAbsRect(self, outRect)
 *   - Component_getOrigin(self)
 *   - Component_getAnchor(self)
 *   - Component_getPivot(self)
 *   - Component_getMinWidth/MinHeight/MaxWidth/MaxHeight(self)
 *   - Component_getMargin(self, outL, outT, outR, outB)
 *   - Component_getPadding(self, outL, outT, outR, outB)
 *   - Component_getBorderWidth/BorderColor/BackgroundColor(self)
 *   - Component_getRadius/RadiusMode(self)
 *   - Component_getOpacity/Z(self)
 *   - Component_isVisible(self)
 *   - Component_getParent(self)
 *   - Component_getChildCount(self)
 *   - Component_getChild(self, index)
 *   - Component_getBackgroundRender/ForegroundRender(self)
 *   - Component_getRenderUserdata(self)
 * ============================================================================
 */

// darling/component.c — immediate on-demand rendering leaf (new architecture).

// CONSTRUCTORS

Component *Component_0(void) {
    Component *self = (Component*) Memory_alloc(TYPE_COMPONENT_SINGLETON, sizeof(Component));
    if (!self)
        return nullptr;
    (*self).x = 0.0f;
    (*self).y = 0.0f;
    (*self).w = 0.0f;
    (*self).h = 0.0f;
    (*self).origin = COMPONENT_ORIGIN_TOP_LEFT;
    (*self).anchor = COMPONENT_ANCHOR_TOP_LEFT;
    (*self).pivot = COMPONENT_PIVOT_TOP_LEFT;
    (*self).minW = 0.0f;
    (*self).minH = 0.0f;
    (*self).maxW = 0.0f;
    (*self).maxH = 0.0f;
    (*self).marginL = 0.0f;
    (*self).marginT = 0.0f;
    (*self).marginR = 0.0f;
    (*self).marginB = 0.0f;
    (*self).paddingL = 0.0f;
    (*self).paddingT = 0.0f;
    (*self).paddingR = 0.0f;
    (*self).paddingB = 0.0f;
    (*self).borderWidth = 0.0f;
    (*self).borderColor = COMPONENT_COLOR_CLEAR;
    (*self).backgroundColor = COMPONENT_COLOR_CLEAR;
    (*self).radius = 0.0f;
    (*self).radiusMode = COMPONENT_CORNER_ARC;
    (*self).opacity = 1.0f;
    (*self).z = 0;
    (*self).visible = 1;
    (*self).parent = nullptr;
    (*self).children = nullptr;
    (*self).childCount = 0;
    (*self).childCapacity = 0;
    (*self).absX = 0.0f;
    (*self).absY = 0.0f;
    (*self).absW = 0.0f;
    (*self).absH = 0.0f;
    (*self).parentAbsX = 0.0f;
    (*self).parentAbsY = 0.0f;
    (*self).parentAbsW = 0.0f;
    (*self).parentAbsH = 0.0f;
    (*self).backgroundRender = nullptr;
    (*self).foregroundRender = nullptr;
    (*self).renderUserdata = nullptr;
    return self;
}

// CORE FUNCTIONS

void Component_recompute(Component *self) {
    if (!self)
        return;

    float sw = (*self).w;
    float sh = (*self).h;
    float locX = (*self).x;
    float locY = (*self).y;
    float parentW = (*self).parentAbsW;
    float parentH = (*self).parentAbsH;

    // 1. Anchor: 9-grid tether point on parent bounds (normalized Ua, Va)
    float Ua = 0.0f, Va = 0.0f;
    switch (Component_getAnchor(self)) {
        case COMPONENT_ANCHOR_TOP_LEFT:       Ua = 0.0f; Va = 0.0f; break;
        case COMPONENT_ANCHOR_TOP_CENTER:     Ua = 0.5f; Va = 0.0f; break;
        case COMPONENT_ANCHOR_TOP_RIGHT:      Ua = 1.0f; Va = 0.0f; break;
        case COMPONENT_ANCHOR_MIDDLE_LEFT:    Ua = 0.0f; Va = 0.5f; break;
        case COMPONENT_ANCHOR_MIDDLE_CENTER:  Ua = 0.5f; Va = 0.5f; break;
        case COMPONENT_ANCHOR_MIDDLE_RIGHT:   Ua = 1.0f; Va = 0.5f; break;
        case COMPONENT_ANCHOR_BOTTOM_LEFT:    Ua = 0.0f; Va = 1.0f; break;
        case COMPONENT_ANCHOR_BOTTOM_CENTER:  Ua = 0.5f; Va = 1.0f; break;
        case COMPONENT_ANCHOR_BOTTOM_RIGHT:   Ua = 1.0f; Va = 1.0f; break;
        default: break;
    }
    float anchorX = (*self).parentAbsX + Ua * parentW;
    float anchorY = (*self).parentAbsY + Va * parentH;

    // 2. Pivot: 5 alignment points on child component (normalized Up, Vp)
    float Up = 0.0f, Vp = 0.0f;
    switch (Component_getPivot(self)) {
        case COMPONENT_PIVOT_TOP_LEFT:      Up = 0.0f; Vp = 0.0f; break;
        case COMPONENT_PIVOT_TOP_RIGHT:     Up = 1.0f; Vp = 0.0f; break;
        case COMPONENT_PIVOT_BOTTOM_LEFT:   Up = 0.0f; Vp = 1.0f; break;
        case COMPONENT_PIVOT_BOTTOM_RIGHT:  Up = 1.0f; Vp = 1.0f; break;
        case COMPONENT_PIVOT_CENTER:        Up = 0.5f; Vp = 0.5f; break;
        default: break;
    }
    float pivotX = Up * sw;
    float pivotY = Vp * sh;

    // 3. Origin: 4-corner coordinate system orientation
    // Determines axis direction (+ or -) for location offsets (x, y)
    float dirX = 1.0f;
    float dirY = 1.0f;
    switch (Component_getOrigin(self)) {
        case COMPONENT_ORIGIN_TOP_RIGHT:
            dirX = -1.0f;
            dirY =  1.0f;
            break;
        case COMPONENT_ORIGIN_BOTTOM_LEFT:
            dirX =  1.0f;
            dirY = -1.0f;
            break;
        case COMPONENT_ORIGIN_BOTTOM_RIGHT:
            dirX = -1.0f;
            dirY = -1.0f;
            break;
        case COMPONENT_ORIGIN_TOP_LEFT:
        default:
            dirX =  1.0f;
            dirY =  1.0f;
            break;
    }

    // Universal resolution: Anchor - Pivot + (dir * offset) + margin
    float screenX = anchorX - pivotX + (dirX * locX) + (*self).marginL;
    float screenY = anchorY - pivotY + (dirY * locY) + (*self).marginT;

    (*self).absX = screenX;
    (*self).absY = screenY;
    (*self).absW = sw;
    (*self).absH = sh;

    // Eager Cascade Law: cascade new abs dimensions to all children immediately
    for (uint32_t i = 0; i < (*self).childCount; ++i) {
        Component *child = (*self).children[i];
        if (child) {
            Component_setParentAbs(child, screenX, screenY, sw, sh);
        }
    }
}

void Component_setParentAbs(Component *self, float px, float py, float pw, float ph) {
    if (!self)
        return;
    (*self).parentAbsX = px;
    (*self).parentAbsY = py;
    (*self).parentAbsW = pw;
    (*self).parentAbsH = ph;
    Component_recompute(self);
}

bool Component_render(Component *self, void *graphics) {
    if (!self || !(*self).visible)
        return false;
    (void) graphics;
    bool ran = false;

    // 1. Native background fill through unified Graphics seam
    if ((*self).backgroundColor != COMPONENT_COLOR_CLEAR && (*self).opacity > 0.0f
        && (*self).absW > 0.0f && (*self).absH > 0.0f) {
        Rectangle rect = {(*self).absX, (*self).absY, (*self).absW, (*self).absH};
        Brush brush = {(*self).backgroundColor, (*self).opacity, 0u};
        Graphics_fillRect(&rect, &brush);
        ran = true;
    }

    // 2. Background custom hook
    if ((*self).backgroundRender) {
        (*self).backgroundRender(self, graphics, (*self).renderUserdata);
        ran = true;
    }

    // 3. Render children in order
    for (uint32_t i = 0; i < (*self).childCount; ++i) {
        Component *child = (*self).children[i];
        if (child && (*child).visible) {
            if (Component_render(child, graphics))
                ran = true;
        }
    }

    // 4. Native border stroke through unified Graphics seam
    if ((*self).borderWidth > 0.0f && (*self).borderColor != COMPONENT_COLOR_CLEAR
        && (*self).absW > 0.0f && (*self).absH > 0.0f) {
        Rectangle rect = {(*self).absX, (*self).absY, (*self).absW, (*self).absH};
        Stroke stroke = {(*self).borderWidth, 0.0f, STROKE_CAP_BUTT, STROKE_JOIN_MITER, (*self).borderColor, 0u};
        Graphics_drawRect(&rect, &stroke);
        ran = true;
    }

    // 5. Foreground custom hook
    if ((*self).foregroundRender) {
        (*self).foregroundRender(self, graphics, (*self).renderUserdata);
        ran = true;
    }
    return ran;
}

bool Component_hitTest(const Component *self, float pointX, float pointY) {
    if (!self || !(*self).visible)
        return false;
    return pointX >= (*self).absX && pointX < (*self).absX + (*self).absW
        && pointY >= (*self).absY && pointY < (*self).absY + (*self).absH;
}

void Component_getContentRect(const Component *self, float *outX, float *outY,
                              float *outW, float *outH) {
    float cx = 0.0f, cy = 0.0f, cw = 0.0f, ch = 0.0f;
    if (self) {
        cx = (*self).absX + (*self).paddingL;
        cy = (*self).absY + (*self).paddingT;
        cw = (*self).absW - (*self).paddingL - (*self).paddingR;
        ch = (*self).absH - (*self).paddingT - (*self).paddingB;
        if (cw < 0.0f)
            cw = 0.0f;
        if (ch < 0.0f)
            ch = 0.0f;
    }
    if (outX)
        *outX = cx;
    if (outY)
        *outY = cy;
    if (outW)
        *outW = cw;
    if (outH)
        *outH = ch;
}

// SETTERS

void Component_setX(Component *self, float x) {
    if (!self)
        return;
    (*self).x = x;
    Component_recompute(self);
}

void Component_setY(Component *self, float y) {
    if (!self)
        return;
    (*self).y = y;
    Component_recompute(self);
}

void Component_setWidth(Component *self, float w) {
    if (!self)
        return;
    (*self).w = w;
    Component_recompute(self);
}

void Component_setHeight(Component *self, float h) {
    if (!self)
        return;
    (*self).h = h;
    Component_recompute(self);
}

void Component_setLocation(Component *self, float x, float y) {
    if (!self)
        return;
    Component_setX(self, x);
    Component_setY(self, y);
}

void Component_setSize(Component *self, float w, float h) {
    if (!self)
        return;
    if (w < (*self).minW)
        w = (*self).minW;
    if (h < (*self).minH)
        h = (*self).minH;
    if ((*self).maxW > 0.0f && w > (*self).maxW)
        w = (*self).maxW;
    if ((*self).maxH > 0.0f && h > (*self).maxH)
        h = (*self).maxH;
    Component_setWidth(self, w);
    Component_setHeight(self, h);
}

void Component_setMinSize(Component *self, float w, float h) {
    if (!self)
        return;
    (*self).minW = w;
    (*self).minH = h;
    float cw = (*self).w;
    float ch = (*self).h;
    if (cw < w)
        cw = w;
    if (ch < h)
        ch = h;
    if ((*self).maxW > 0.0f && cw > (*self).maxW)
        cw = (*self).maxW;
    if ((*self).maxH > 0.0f && ch > (*self).maxH)
        ch = (*self).maxH;
    Component_setWidth(self, cw);
    Component_setHeight(self, ch);
}

void Component_setMaxSize(Component *self, float w, float h) {
    if (!self)
        return;
    (*self).maxW = w;
    (*self).maxH = h;
    float cw = (*self).w;
    float ch = (*self).h;
    if (w > 0.0f && cw > w)
        cw = w;
    if (h > 0.0f && ch > h)
        ch = h;
    if (cw < (*self).minW)
        cw = (*self).minW;
    if (ch < (*self).minH)
        ch = (*self).minH;
    Component_setWidth(self, cw);
    Component_setHeight(self, ch);
}

void Component_setOrigin(Component *self, int origin) {
    if (!self || origin < COMPONENT_ORIGIN_TOP_LEFT || origin > COMPONENT_ORIGIN_BOTTOM_RIGHT)
        return;
    (*self).origin = (uint8_t) origin;
    Component_recompute(self);
}

void Component_setAnchor(Component *self, int anchor) {
    if (!self || anchor < COMPONENT_ANCHOR_TOP_LEFT || anchor > COMPONENT_ANCHOR_BOTTOM_RIGHT)
        return;
    (*self).anchor = (uint8_t) anchor;
    Component_recompute(self);
}

void Component_setPivot(Component *self, int pivot) {
    if (!self || pivot < COMPONENT_PIVOT_TOP_LEFT || pivot > COMPONENT_PIVOT_CENTER)
        return;
    (*self).pivot = pivot;
    Component_recompute(self);
}

void Component_setCenter(Component *self) {
    if (!self)
        return;
    Component_setPivot(self, COMPONENT_PIVOT_CENTER);
}

void Component_setMargin(Component *self, float l, float t, float r, float b) {
    if (!self)
        return;
    (*self).marginL = l;
    (*self).marginT = t;
    (*self).marginR = r;
    (*self).marginB = b;
    Component_recompute(self);
}

void Component_setPadding(Component *self, float l, float t, float r, float b) {
    if (!self)
        return;
    float pl = l < 0.0f ? 0.0f : l;
    float pt = t < 0.0f ? 0.0f : t;
    float pr = r < 0.0f ? 0.0f : r;
    float pb = b < 0.0f ? 0.0f : b;
    (*self).paddingL = pl;
    (*self).paddingT = pt;
    (*self).paddingR = pr;
    (*self).paddingB = pb;
}

void Component_setBorderWidth(Component *self, float w) {
    if (!self)
        return;
    (*self).borderWidth = w < 0.0f ? 0.0f : w;
}

void Component_setBorderColor(Component *self, uint32_t color) {
    if (!self)
        return;
    (*self).borderColor = color;
}

void Component_setBackgroundColor(Component *self, uint32_t color) {
    if (!self)
        return;
    (*self).backgroundColor = color;
}

void Component_setRadius(Component *self, float r) {
    if (!self)
        return;
    (*self).radius = r < 0.0f ? 0.0f : r;
}

void Component_setRadiusMode(Component *self, int mode) {
    if (!self)
        return;
    if (mode != COMPONENT_CORNER_ARC && mode != COMPONENT_CORNER_SUPERELLIPSE)
        return;
    (*self).radiusMode = mode;
}

void Component_setOpacity(Component *self, float opacity) {
    if (!self)
        return;
    (*self).opacity = opacity < 0.0f ? 0.0f : (opacity > 1.0f ? 1.0f : opacity);
}

void Component_setZ(Component *self, int z) {
    if (!self)
        return;
    (*self).z = z;
}

void Component_setVisible(Component *self, bool visible) {
    if (!self)
        return;
    (*self).visible = visible ? 1 : 0;
}

void Component_setParent(Component *self, Component *parent) {
    if (!self)
        return;
    (*self).parent = parent;
}

bool Component_addChild(Component *self, Component *child) {
    if (!self || !child || child == self)
        return false;

    // Check if already present
    for (uint32_t i = 0; i < (*self).childCount; ++i) {
        if ((*self).children[i] == child)
            return true;
    }

    if ((*self).childCount >= (*self).childCapacity) {
        uint32_t newCap = (*self).childCapacity == 0 ? 8 : (*self).childCapacity * 2;
        Component **newArr = (Component**) Memory_alloc(TYPE_COMPONENT_SINGLETON, sizeof(Component*) * newCap);
        if (!newArr)
            return false;
        if ((*self).children && (*self).childCount > 0) {
            memcpy(newArr, (*self).children, sizeof(Component*) * (*self).childCount);
        }
        (*self).children = newArr;
        (*self).childCapacity = newCap;
    }

    (*self).children[(*self).childCount++] = child;
    (*child).parent = self;

    // Eager Cascade: update child abs immediately with parent's abs box
    Component_setParentAbs(child, (*self).absX, (*self).absY, (*self).absW, (*self).absH);
    return true;
}

bool Component_removeChild(Component *self, Component *child) {
    if (!self || !child || (*self).childCount == 0 || !(*self).children)
        return false;

    int foundIdx = -1;
    for (uint32_t i = 0; i < (*self).childCount; ++i) {
        if ((*self).children[i] == child) {
            foundIdx = (int) i;
            break;
        }
    }
    if (foundIdx < 0)
        return false;

    for (uint32_t i = (uint32_t) foundIdx; i + 1 < (*self).childCount; ++i) {
        (*self).children[i] = (*self).children[i + 1];
    }
    (*self).childCount--;
    if ((*child).parent == self) {
        (*child).parent = nullptr;
    }
    return true;
}

void Component_setBackgroundRender(Component *self, Component_RenderFn fn) {
    if (!self)
        return;
    (*self).backgroundRender = fn;
}

void Component_setForegroundRender(Component *self, Component_RenderFn fn) {
    if (!self)
        return;
    (*self).foregroundRender = fn;
}

void Component_setRenderUserdata(Component *self, void *userdata) {
    if (!self)
        return;
    (*self).renderUserdata = userdata;
}

// GETTERS

float Component_getX(const Component *self) { return self ? (*self).x : 0.0f; }
float Component_getY(const Component *self) { return self ? (*self).y : 0.0f; }
float Component_getWidth(const Component *self) { return self ? (*self).w : 0.0f; }
float Component_getHeight(const Component *self) { return self ? (*self).h : 0.0f; }

float Component_getAbsX(const Component *self) { return self ? (*self).absX : 0.0f; }
float Component_getAbsY(const Component *self) { return self ? (*self).absY : 0.0f; }
float Component_getAbsW(const Component *self) { return self ? (*self).absW : 0.0f; }
float Component_getAbsH(const Component *self) { return self ? (*self).absH : 0.0f; }

void Component_getAbsRect(const Component *self, Vec4 *outRect) {
    if (!outRect)
        return;
    float ax = self ? (*self).absX : 0.0f;
    float ay = self ? (*self).absY : 0.0f;
    float aw = self ? (*self).absW : 0.0f;
    float ah = self ? (*self).absH : 0.0f;
    Vec4_set(outRect, ax, ay, aw, ah);
}

void Component_getParentAbsRect(const Component *self, Vec4 *outRect) {
    if (!outRect)
        return;
    float px = self ? (*self).parentAbsX : 0.0f;
    float py = self ? (*self).parentAbsY : 0.0f;
    float pw = self ? (*self).parentAbsW : 0.0f;
    float ph = self ? (*self).parentAbsH : 0.0f;
    Vec4_set(outRect, px, py, pw, ph);
}

int Component_getOrigin(const Component *self) {
    return self ? (*self).origin : COMPONENT_ORIGIN_TOP_LEFT;
}

int Component_getAnchor(const Component *self) {
    return self ? (*self).anchor : COMPONENT_ANCHOR_TOP_LEFT;
}

int Component_getPivot(const Component *self) {
    return self ? (*self).pivot : COMPONENT_PIVOT_TOP_LEFT;
}

float Component_getMinWidth(const Component *self) { return self ? (*self).minW : 0.0f; }
float Component_getMinHeight(const Component *self) { return self ? (*self).minH : 0.0f; }
float Component_getMaxWidth(const Component *self) { return self ? (*self).maxW : 0.0f; }
float Component_getMaxHeight(const Component *self) { return self ? (*self).maxH : 0.0f; }

void Component_getMargin(const Component *self, float *outL, float *outT, float *outR, float *outB) {
    float ml = self ? (*self).marginL : 0.0f;
    float mt = self ? (*self).marginT : 0.0f;
    float mr = self ? (*self).marginR : 0.0f;
    float mb = self ? (*self).marginB : 0.0f;
    if (outL)
        *outL = ml;
    if (outT)
        *outT = mt;
    if (outR)
        *outR = mr;
    if (outB)
        *outB = mb;
}

void Component_getPadding(const Component *self, float *outL, float *outT, float *outR, float *outB) {
    float pl = self ? (*self).paddingL : 0.0f;
    float pt = self ? (*self).paddingT : 0.0f;
    float pr = self ? (*self).paddingR : 0.0f;
    float pb = self ? (*self).paddingB : 0.0f;
    if (outL)
        *outL = pl;
    if (outT)
        *outT = pt;
    if (outR)
        *outR = pr;
    if (outB)
        *outB = pb;
}

float Component_getBorderWidth(const Component *self) { return self ? (*self).borderWidth : 0.0f; }
uint32_t Component_getBorderColor(const Component *self) { return self ? (*self).borderColor : COMPONENT_COLOR_CLEAR; }
uint32_t Component_getBackgroundColor(const Component *self) { return self ? (*self).backgroundColor : COMPONENT_COLOR_CLEAR; }
float Component_getRadius(const Component *self) { return self ? (*self).radius : 0.0f; }
int Component_getRadiusMode(const Component *self) { return self ? (*self).radiusMode : COMPONENT_CORNER_ARC; }
float Component_getOpacity(const Component *self) { return self ? (*self).opacity : 1.0f; }
int Component_getZ(const Component *self) { return self ? (*self).z : 0; }
bool Component_isVisible(const Component *self) { return self && (*self).visible != 0; }
Component *Component_getParent(const Component *self) { return self ? (*self).parent : nullptr; }
uint32_t Component_getChildCount(const Component *self) { return self ? (*self).childCount : 0; }
Component *Component_getChild(const Component *self, uint32_t index) {
    if (!self || index >= (*self).childCount || !(*self).children)
        return nullptr;
    return (*self).children[index];
}
Component_RenderFn Component_getBackgroundRender(const Component *self) { return self ? (*self).backgroundRender : nullptr; }
Component_RenderFn Component_getForegroundRender(const Component *self) { return self ? (*self).foregroundRender : nullptr; }
void *Component_getRenderUserdata(const Component *self) { return self ? (*self).renderUserdata : nullptr; }