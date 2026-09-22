#include "darling/component.h"

#include <math.h>

#include "../c23/darling-type.h"
#include "lang/rect/rectangle.h"
#include "nio/mem.h"
#include "oop/type.h"
#include "annotation/definition.h"
#include "annotation/overview.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Component
 * ============================================================================
 * The element metadata of the darling architecture: everything one element
 * needs — placement, size, scale, origin/anchor/pivot, constraints,
 * margin/padding, presentation state (border, background, radius, opacity,
 * z, visible) — plus an ABSOLUTE rect recomputed eagerly on every geometry
 * setter, so a reader consumes ((*self).absX..absH) directly with no resolve
 * phase and no stale reads. A Container is just a node of Component[]: it
 * owns the list, Components own no tree, no hooks, no retained targets, no
 * generation counter. Abs resolves against the parent abs box stored via
 * Component_setParentAbs (fed by the owning Container/Panel once per
 * layout); a parentless component resolves against (0,0,0,0). Setters never
 * layout beyond this component's own abs — cascading is the owner's job.
 * ComponentView is pure math (scale + origin): points map to device pixels
 * as (abs - origin) * scale with gapless floor/ceil rounding.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Component
 * LEVEL: L2 — Behavior (element metadata)
 * ============================================================================
 * SUMMARY:
 *   Pure metadata for one element plus its eager absolute rect. No tree, no
 *   hooks, no retained targets. Owned in arrays by Container nodes.
 *
 * STRUCT FIELDS (Mirroring darling/component.h):
 * ----------------------------------------------------------------------------
 *   float x, y, w, h;        // Placement + size in parent units
 *   float scaleX, scaleY;    // Axis scale multipliers (1 = unscaled)
 *   uint8_t origin;          // COMPONENT_ORIGIN_* 0..3
 *   uint8_t anchor;          // COMPONENT_ANCHOR_* 0..8
 *   int32_t pivot;           // COMPONENT_PIVOT_* 0..4
 *   float minW, minH;        // Size constraints (0 = unset)
 *   float maxW, maxH;        // Size constraints (0 = unset)
 *   float minX, minY;        // Location constraints (0 = unset both ends)
 *   float maxX, maxY;
 *   float marginL, marginT;  // Additive placement offsets
 *   float marginR, marginB;  // Right/bottom edges stored for sibling layout
 *   float paddingL, paddingT; // Inward content insets (content box = abs + padding)
 *   float paddingR, paddingB;
 *   float borderWidth;       // 0 = no border
 *   uint32_t borderColor;    // 0xRRGGBBAA
 *   uint32_t backgroundColor; // 0xRRGGBBAA
 *   float radius;            // Corner radius (0 = square)
 *   int radiusMode;          // COMPONENT_CORNER_ARC (0) / COMPONENT_CORNER_SUPERELLIPSE (1)
 *   float opacity;           // 0..1 alpha multiplier (1 = opaque)
 *   int32_t z;               // Z-order
 *   uint8_t visible;         // Visibility gate for paint/hitTest
 *   float absX, absY;        // Absolute left/top (eager)
 *   float absW, absH;        // Absolute extent (eager)
 *   float parentAbsX, parentAbsY; // Parent abs box at last recompute
 *   float parentAbsW, parentAbsH;
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Public Constructors: (.h)
 *   - Component_0(void)
 *   - Component_init(self)
 *
 * Private Constructors: (.c static)
 *   - (none)
 *
 * Public Core Functions: (.h)
 *   - Component_recompute(self)
 *   - Component_setParentAbs(self, px, py, pw, ph)
 *   - Component_hitTest(self, pointX, pointY)
 *   - Component_getContentRect(self, outX, outY, outW, outH)
 *   - Component_viewMap(view, ax, ay, aw, ah, outX, outY, outW, outH)
 *
 * Private Core Functions: (.c static)
 *   - contentBox(self, outX, outY, outW, outH)
 *
 * Public Setters: (.h)
 *   - Component_setX/Y/Width/Height/Scale/Location/Size/MinSize/MaxSize/MinLocation/MaxLocation(self, ...)
 *   - Component_setOrigin/Anchor/Pivot/Center/Margin/Padding(self, ...)
 *   - Component_setBorderWidth/BorderColor/BackgroundColor/Radius/RadiusMode/Opacity/Z/Visible(self, ...)
 *
 * Private Setters: (.c static)
 *   - (none)
 *
 * Public Getters: (.h)
 *   - Component_getX/Y/Width/Height/ScaleX/ScaleY/AbsX/AbsY/AbsW/AbsH(self)
 *   - Component_getAbsRect/ParentAbsRect(self, outRect)
 *   - Component_getOrigin/Anchor/Pivot/MinWidth/MinHeight/MaxWidth/MaxHeight/MinX/MinY/MaxX/MaxY(self)
 *   - Component_getMargin/Padding/BorderWidth/BorderColor/BackgroundColor/Radius/RadiusMode/Opacity/Z(self)
 *   - Component_isVisible(self)
 *
 * Private Getters: (.c static)
 *   - (none)
 * ============================================================================
 */

// darling/component.c — pure element metadata with an eager absolute rect.

// FILE-LOCAL HELPERS

static void contentBox(const Component *self, float *outX, float *outY,
                       float *outW, float *outH) {
    float cx = (*self).absX + (*self).paddingL;
    float cy = (*self).absY + (*self).paddingT;
    float cw = (*self).absW - (*self).paddingL - (*self).paddingR;
    float ch = (*self).absH - (*self).paddingT - (*self).paddingB;
    if (cw < 0.0f)
        cw = 0.0f;
    if (ch < 0.0f)
        ch = 0.0f;
    if (outX)
        *outX = cx;
    if (outY)
        *outY = cy;
    if (outW)
        *outW = cw;
    if (outH)
        *outH = ch;
}

// CONSTRUCTORS (PUBLIC & PRIVATE)

void Component_init(Component *self) {
    if (!self)
        return;
    (*self).x = 0.0f;
    (*self).y = 0.0f;
    (*self).w = 0.0f;
    (*self).h = 0.0f;
    (*self).scaleX = 1.0f;
    (*self).scaleY = 1.0f;
    (*self).origin = COMPONENT_ORIGIN_TOP_LEFT;
    (*self).anchor = COMPONENT_ANCHOR_TOP_LEFT;
    (*self).pivot = COMPONENT_PIVOT_TOP_LEFT;
    (*self).minW = 0.0f;
    (*self).minH = 0.0f;
    (*self).maxW = 0.0f;
    (*self).maxH = 0.0f;
    (*self).minX = 0.0f;
    (*self).minY = 0.0f;
    (*self).maxX = 0.0f;
    (*self).maxY = 0.0f;
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
    (*self).absX = 0.0f;
    (*self).absY = 0.0f;
    (*self).absW = 0.0f;
    (*self).absH = 0.0f;
    (*self).parentAbsX = 0.0f;
    (*self).parentAbsY = 0.0f;
    (*self).parentAbsW = 0.0f;
    (*self).parentAbsH = 0.0f;
}

Component *Component_0(void) {
    Component *self = (Component*) Memory_alloc(TYPE_COMPONENT_SINGLETON, sizeof(Component));
    if (!self)
        return nullptr;
    Component_init(self);
    return self;
}

// CORE FUNCTIONS (PUBLIC & PRIVATE)

void Component_recompute(Component *self) {
    if (!self)
        return;
    float sw = (*self).w * (*self).scaleX;
    float sh = (*self).h * (*self).scaleY;
    float locX = (*self).x;
    float locY = (*self).y;
    float parentW = (*self).parentAbsW;
    float parentH = (*self).parentAbsH;
    float Ua = 0.0f, Va = 0.0f;
    switch (Component_getAnchor(self)) {
        case COMPONENT_ANCHOR_TOP_LEFT: Ua = 0.0f; Va = 0.0f; break;
        case COMPONENT_ANCHOR_TOP_CENTER: Ua = 0.5f; Va = 0.0f; break;
        case COMPONENT_ANCHOR_TOP_RIGHT: Ua = 1.0f; Va = 0.0f; break;
        case COMPONENT_ANCHOR_MIDDLE_LEFT: Ua = 0.0f; Va = 0.5f; break;
        case COMPONENT_ANCHOR_MIDDLE_CENTER: Ua = 0.5f; Va = 0.5f; break;
        case COMPONENT_ANCHOR_MIDDLE_RIGHT: Ua = 1.0f; Va = 0.5f; break;
        case COMPONENT_ANCHOR_BOTTOM_LEFT: Ua = 0.0f; Va = 1.0f; break;
        case COMPONENT_ANCHOR_BOTTOM_CENTER: Ua = 0.5f; Va = 1.0f; break;
        case COMPONENT_ANCHOR_BOTTOM_RIGHT: Ua = 1.0f; Va = 1.0f; break;
        default: break;
    }
    float anchorX = (*self).parentAbsX + Ua * parentW;
    float anchorY = (*self).parentAbsY + Va * parentH;
    float Up = 0.0f, Vp = 0.0f;
    switch (Component_getPivot(self)) {
        case COMPONENT_PIVOT_TOP_LEFT: Up = 0.0f; Vp = 0.0f; break;
        case COMPONENT_PIVOT_TOP_RIGHT: Up = 1.0f; Vp = 0.0f; break;
        case COMPONENT_PIVOT_BOTTOM_LEFT: Up = 0.0f; Vp = 1.0f; break;
        case COMPONENT_PIVOT_BOTTOM_RIGHT: Up = 1.0f; Vp = 1.0f; break;
        case COMPONENT_PIVOT_CENTER: Up = 0.5f; Vp = 0.5f; break;
        default: break;
    }
    float pivotX = Up * sw;
    float pivotY = Vp * sh;
    float dirX = 1.0f;
    float dirY = 1.0f;
    switch (Component_getOrigin(self)) {
        case COMPONENT_ORIGIN_TOP_RIGHT:
            dirX = -1.0f;
            dirY = 1.0f;
            break;
        case COMPONENT_ORIGIN_BOTTOM_LEFT:
            dirX = 1.0f;
            dirY = -1.0f;
            break;
        case COMPONENT_ORIGIN_BOTTOM_RIGHT:
            dirX = -1.0f;
            dirY = -1.0f;
            break;
        case COMPONENT_ORIGIN_TOP_LEFT:
        default:
            dirX = 1.0f;
            dirY = 1.0f;
            break;
    }
    float screenX = anchorX - pivotX + (dirX * locX) + (*self).marginL;
    float screenY = anchorY - pivotY + (dirY * locY) + (*self).marginT;
    (*self).absX = screenX;
    (*self).absY = screenY;
    (*self).absW = sw;
    (*self).absH = sh;
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

bool Component_hitTest(const Component *self, float pointX, float pointY) {
    if (!self || !(*self).visible)
        return false;
    return pointX >= (*self).absX && pointX < (*self).absX + (*self).absW
        && pointY >= (*self).absY && pointY < (*self).absY + (*self).absH;
}

void Component_getContentRect(const Component *self, float *outX, float *outY,
                              float *outW, float *outH) {
    if (!self) {
        if (outX)
            *outX = 0.0f;
        if (outY)
            *outY = 0.0f;
        if (outW)
            *outW = 0.0f;
        if (outH)
            *outH = 0.0f;
        return;
    }
    contentBox(self, outX, outY, outW, outH);
}

void Component_viewMap(const ComponentView *view, float ax, float ay, float aw, float ah,
                       float *outX, float *outY, float *outW, float *outH) {
    float x0 = ax;
    float y0 = ay;
    float w = aw;
    float h = ah;
    if (view) {
        x0 = floorf((ax - (*view).originX) * (*view).scaleX);
        y0 = floorf((ay - (*view).originY) * (*view).scaleY);
        float x1 = ceilf((ax + aw - (*view).originX) * (*view).scaleX);
        float y1 = ceilf((ay + ah - (*view).originY) * (*view).scaleY);
        w = x1 - x0;
        h = y1 - y0;
        if (w < 0.0f)
            w = 0.0f;
        if (h < 0.0f)
            h = 0.0f;
    }
    if (outX)
        *outX = x0;
    if (outY)
        *outY = y0;
    if (outW)
        *outW = w;
    if (outH)
        *outH = h;
}

// SETTERS (PUBLIC & PRIVATE)

void Component_setX(Component *self, float x) {
    if (!self)
        return;
    if ((*self).minX != 0.0f && x < (*self).minX)
        x = (*self).minX;
    if ((*self).maxX != 0.0f && x > (*self).maxX)
        x = (*self).maxX;
    (*self).x = x;
    Component_recompute(self);
}

void Component_setY(Component *self, float y) {
    if (!self)
        return;
    if ((*self).minY != 0.0f && y < (*self).minY)
        y = (*self).minY;
    if ((*self).maxY != 0.0f && y > (*self).maxY)
        y = (*self).maxY;
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

void Component_setScale(Component *self, float sx, float sy) {
    if (!self)
        return;
    (*self).scaleX = sx;
    (*self).scaleY = sy;
    Component_recompute(self);
}

void Component_setMinLocation(Component *self, float x, float y) {
    if (!self)
        return;
    (*self).minX = x;
    (*self).minY = y;
    float cx = (*self).x;
    float cy = (*self).y;
    if ((*self).minX != 0.0f && cx < x)
        cx = x;
    if ((*self).minY != 0.0f && cy < y)
        cy = y;
    if ((*self).maxX != 0.0f && cx > (*self).maxX)
        cx = (*self).maxX;
    if ((*self).maxY != 0.0f && cy > (*self).maxY)
        cy = (*self).maxY;
    Component_setX(self, cx);
    Component_setY(self, cy);
}

void Component_setMaxLocation(Component *self, float x, float y) {
    if (!self)
        return;
    (*self).maxX = x;
    (*self).maxY = y;
    float cx = (*self).x;
    float cy = (*self).y;
    if (x != 0.0f && cx > x)
        cx = x;
    if (y != 0.0f && cy > y)
        cy = y;
    if ((*self).minX != 0.0f && cx < (*self).minX)
        cx = (*self).minX;
    if ((*self).minY != 0.0f && cy < (*self).minY)
        cy = (*self).minY;
    Component_setX(self, cx);
    Component_setY(self, cy);
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
    Component_recompute(self);
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

// GETTERS (PUBLIC & PRIVATE)

float Component_getX(const Component *self) { return self ? (*self).x : 0.0f; }
float Component_getY(const Component *self) { return self ? (*self).y : 0.0f; }
float Component_getWidth(const Component *self) { return self ? (*self).w : 0.0f; }
float Component_getHeight(const Component *self) { return self ? (*self).h : 0.0f; }
float Component_getScaleX(const Component *self) { return self ? (*self).scaleX : 1.0f; }
float Component_getScaleY(const Component *self) { return self ? (*self).scaleY : 1.0f; }

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
float Component_getMinX(const Component *self) { return self ? (*self).minX : 0.0f; }
float Component_getMinY(const Component *self) { return self ? (*self).minY : 0.0f; }
float Component_getMaxX(const Component *self) { return self ? (*self).maxX : 0.0f; }
float Component_getMaxY(const Component *self) { return self ? (*self).maxY : 0.0f; }

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
