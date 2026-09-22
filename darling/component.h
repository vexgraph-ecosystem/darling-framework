#ifndef DARLING_COMPONENT_H
#define DARLING_COMPONENT_H

#include <stdbool.h>
#include <stdint.h>

#include "lang/vec4.h"

// darling/component.h — the element metadata of the darling architecture.
//
// Component is pure metadata: everything one element needs — placement,
// size, scale, origin/anchor/pivot, constraints, margin/padding,
// presentation state (border, background, radius, opacity, z, visible) —
// plus an EAGER absolute rect recomputed on every geometry setter, so any
// reader hands ((*self).absX, (*self).absY, (*self).absW, (*self).absH)
// straight to the paint path with no resolve phase and no stale reads.
//
// A Container is just a node of Component[]: it owns the list, Components
// own no tree. A component's abs resolves against the parent abs box stored
// via Component_setParentAbs (fed by the owning Container/Panel once per
// layout); a parentless component resolves against (0,0,0,0).
//
// Coordinate convention: x/y on right/bottom-anchored nodes is the EDGE
// INSET (positive = inward from the anchored edge); margin applies additively
// to the resolved placement. Anchor values mirror the old Container values
// exactly so existing layout numbers migrate value-identically.

// ComponentView — pure-data device mapping handed to paint paths: the
// point-to-native-pixel scale of the current present plus an abs-space
// translation (originX/originY): a sample point is
// (abs - origin) * scale, so shifted views (scroll/pan) reuse the exact
// same currency. No graphics pointer: mapping is pure math.
typedef struct ComponentView {
    float scaleX;
    float scaleY;
    float originX;
    float originY;
} ComponentView;

typedef struct Component {
    // --- Geometry (parent units; placement varies with anchor) ---
    float x, y, w, h;
    float scaleX, scaleY;
    uint8_t origin;
    uint8_t anchor;
    int32_t pivot;
    float minW, minH;
    float maxW, maxH;
    float minX, minY;
    float maxX, maxY;
    // --- Spacing ---
    float marginL, marginT;
    float marginR, marginB;
    float paddingL, paddingT;
    float paddingR, paddingB;
    // --- Presentation state ---
    float borderWidth;
    uint32_t borderColor;
    uint32_t backgroundColor;
    float radius;
    int radiusMode;
    float opacity;
    int32_t z;
    uint8_t visible;
    // --- Eager absolute rect (recomputed on every geometry setter) ---
    float absX, absY;
    float absW, absH;
    float parentAbsX, parentAbsY;
    float parentAbsW, parentAbsH;
} Component;

// Origin: the parent container's coordinate zero-point and axis direction (4 corners).
#define COMPONENT_ORIGIN_TOP_LEFT      0
#define COMPONENT_ORIGIN_TOP_RIGHT     1
#define COMPONENT_ORIGIN_BOTTOM_LEFT   2
#define COMPONENT_ORIGIN_BOTTOM_RIGHT  3

// Anchor: where on the parent the element tracks during resize (9-grid).
#define COMPONENT_ANCHOR_TOP_LEFT      0
#define COMPONENT_ANCHOR_TOP_CENTER    1
#define COMPONENT_ANCHOR_TOP_RIGHT     2
#define COMPONENT_ANCHOR_MIDDLE_LEFT   3
#define COMPONENT_ANCHOR_MIDDLE_CENTER 4
#define COMPONENT_ANCHOR_MIDDLE_RIGHT  5
#define COMPONENT_ANCHOR_BOTTOM_LEFT   6
#define COMPONENT_ANCHOR_BOTTOM_CENTER 7
#define COMPONENT_ANCHOR_BOTTOM_RIGHT  8

// Pivot: the element's reference point for placement (5 points: 4 corners + center).
#define COMPONENT_PIVOT_TOP_LEFT      0
#define COMPONENT_PIVOT_TOP_RIGHT     1
#define COMPONENT_PIVOT_BOTTOM_LEFT   2
#define COMPONENT_PIVOT_BOTTOM_RIGHT  3
#define COMPONENT_PIVOT_CENTER        4

// Corner radius modes.
#define COMPONENT_CORNER_ARC 0
#define COMPONENT_CORNER_SUPERELLIPSE 1

#define COMPONENT_COLOR_CLEAR 0x00000000u

// Constructor: Component() — detached metadata at origin, TOP_LEFT everything,
// visible, opaque, zero padding/border.
Component *Component_0(void);

// Value initializer for embedded members: fills defaults in place.
void Component_init(Component *self);

// Core functions:
void Component_recompute(Component *self);
void Component_setParentAbs(Component *self, float px, float py, float pw, float ph);
bool Component_hitTest(const Component *self, float pointX, float pointY);
void Component_getContentRect(const Component *self, float *outX, float *outY,
                              float *outW, float *outH);
void Component_viewMap(const ComponentView *view, float ax, float ay, float aw, float ah,
                       float *outX, float *outY, float *outW, float *outH);

// Geometry setters (each recomputes abs eagerly):
void Component_setX(Component *self, float x);
void Component_setY(Component *self, float y);
void Component_setWidth(Component *self, float w);
void Component_setHeight(Component *self, float h);
void Component_setScale(Component *self, float sx, float sy);
void Component_setLocation(Component *self, float x, float y);
void Component_setSize(Component *self, float w, float h);
void Component_setMinSize(Component *self, float w, float h);
void Component_setMaxSize(Component *self, float w, float h);
void Component_setMinLocation(Component *self, float x, float y);
void Component_setMaxLocation(Component *self, float x, float y);
void Component_setOrigin(Component *self, int origin);
void Component_setAnchor(Component *self, int anchor);
void Component_setPivot(Component *self, int pivot);
void Component_setCenter(Component *self);
void Component_setMargin(Component *self, float l, float t, float r, float b);
void Component_setPadding(Component *self, float l, float t, float r, float b);
// Presentation setters (no recompute):
void Component_setBorderWidth(Component *self, float w);
void Component_setBorderColor(Component *self, uint32_t color);
void Component_setBackgroundColor(Component *self, uint32_t color);
void Component_setRadius(Component *self, float r);
void Component_setRadiusMode(Component *self, int mode);
void Component_setOpacity(Component *self, float opacity);
void Component_setZ(Component *self, int z);
void Component_setVisible(Component *self, bool visible);

// Getters (nullptr-safe defaults):
float Component_getX(const Component *self);
float Component_getY(const Component *self);
float Component_getWidth(const Component *self);
float Component_getHeight(const Component *self);
float Component_getScaleX(const Component *self);
float Component_getScaleY(const Component *self);
float Component_getAbsX(const Component *self);
float Component_getAbsY(const Component *self);
float Component_getAbsW(const Component *self);
float Component_getAbsH(const Component *self);
void Component_getAbsRect(const Component *self, Vec4 *outRect);
void Component_getParentAbsRect(const Component *self, Vec4 *outRect);
int Component_getOrigin(const Component *self);
int Component_getAnchor(const Component *self);
int Component_getPivot(const Component *self);
float Component_getMinWidth(const Component *self);
float Component_getMinHeight(const Component *self);
float Component_getMaxWidth(const Component *self);
float Component_getMaxHeight(const Component *self);
float Component_getMinX(const Component *self);
float Component_getMinY(const Component *self);
float Component_getMaxX(const Component *self);
float Component_getMaxY(const Component *self);
void Component_getMargin(const Component *self, float *outL, float *outT, float *outR, float *outB);
void Component_getPadding(const Component *self, float *outL, float *outT, float *outR, float *outB);
float Component_getBorderWidth(const Component *self);
uint32_t Component_getBorderColor(const Component *self);
uint32_t Component_getBackgroundColor(const Component *self);
float Component_getRadius(const Component *self);
int Component_getRadiusMode(const Component *self);
float Component_getOpacity(const Component *self);
int Component_getZ(const Component *self);
bool Component_isVisible(const Component *self);

#endif
