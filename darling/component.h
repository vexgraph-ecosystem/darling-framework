#ifndef DARLING_COMPONENT_H
#define DARLING_COMPONENT_H

#include <stdbool.h>
#include <stdint.h>

#include "lang/vec4.h"

// darling/component.h — the renderable leaf of the new darling architecture.
//
// Component is the immediate-mode-ready unit of the retained-darling model:
// geometry + presentation state + an ABSOLUTE rect recomputed eagerly on EVERY
// geometry setter, so any renderer hands ((*self).absX, (*self).absY,
// (*self).absW, (*self).absH) straight to the Graphics seam — no resolve
// phase, no parent-size threading through paint calls, no stale reads.
//
// A Component is a leaf on its own: it has NO children, NO tree, and renders
// while detached from any container (absolute placement resolves against the
// (0,0,0,0) root box). Containment is optional BY CONSTRUCTION — an element
// that does not want to be added is precisely a Component without a
// Container. Container (the future tree holder) will own Components as
// children; this class never grows a children array.
//
// THE EAGER ABS CASCADE (documented invariant):
//   1. Every geometry setter (setX/setY/setWidth/setHeight/setLocation/
//      setSize/setMinSize/setMaxSize/setAnchor/setPivot/setCenter/
//      setMargin) recomputes ((*self).absX..absH) IMMEDIATELY. Abs is always
//      current after any setter — never re-read stale.
//   2. A component's abs resolves against the PARENT'S abs box, stored via
//      Component_setParentAbs (called by the parent container whenever the
//      parent's abs changes — the only way a deeper tree cascades). A
//      parentless component's parent box is (0,0,0,0).
//   3. Setters never layout: recompute touches only THIS component's abs.
//      Cascading to children is the parent container's job, once per frame.
//
// The anchor+pivot system mirrors Container's exactly (same values), so a
// future Container rebuild over Component is a value-identical migration.
// Coordinate convention: x/y on right/bottom-anchored nodes is the EDGE
// INSET (positive = inward from the anchored edge); margin applies additively
// to the resolved placement, identical to Container_resolve.

struct Component;

// Render hook: requested to draw this component about its absolute rect into
// the opaque graphics context. backgroundRender runs before
// foregroundRender. Hooks read ((*self).absX..absH) — always current.
typedef void (*Component_RenderFn)(struct Component *self, void *graphics,
                                   void *userdata);

typedef struct Component {
    // --- Geometry (parent units; placement varies with anchor) ---
    float x, y, w, h;           // placement + size; x/y direction governed by origin
    uint8_t origin;             // COMPONENT_ORIGIN_* 0..3
    uint8_t anchor;             // COMPONENT_ANCHOR_* 0..8
    int32_t pivot;              // COMPONENT_PIVOT_* 0..4
    float minW, minH;           // size constraints (0 = unset)
    float maxW, maxH;           // size constraints (0 = unset)
    // --- Spacing ---
    float marginL, marginT;     // additive placement offsets (final = resolved + margin)
    float marginR, marginB;     // right/bottom edges stored for sibling layout
    float paddingL, paddingT;   // inward content insets (content box = abs + padding)
    float paddingR, paddingB;
    // --- Presentation state ---
    float borderWidth;          // 0 = no border
    uint32_t borderColor;       // 0xAARRGGBB
    uint32_t backgroundColor;   // 0xAARRGGBB (consumed by render hooks)
    float radius;               // corner radius (0 = square)
    int radiusMode;             // COMPONENT_CORNER_ARC (0) / COMPONENT_CORNER_SUPERELLIPSE (1)
    float opacity;              // 0..1 alpha multiplier (1 = opaque)
    int32_t z;                  // z-order
    uint8_t visible;            // visibility gate for render/hitTest
    struct Component *parent;   // borrowed view, nullptr = root; NEVER owned
    struct Component **children; // child components owned or attached
    uint32_t childCount;
    uint32_t childCapacity;
    // --- Eager absolute rect (recomputed on every geometry setter) ---
    float absX, absY;           // absolute left/top in the rendering space
    float absW, absH;           // absolute extent
    float parentAbsX, parentAbsY; // parent abs box at last recompute
    float parentAbsW, parentAbsH;
    // --- Render hooks (immediate on-demand rendering) ---
    Component_RenderFn backgroundRender; // stage 0
    Component_RenderFn foregroundRender; // stage 1
    void *renderUserdata;       // opaque arg handed to both hooks
} Component;

// Origin: the parent container's coordinate zero-point and axis direction (4 corners).
#define COMPONENT_ORIGIN_TOP_LEFT      0
#define COMPONENT_ORIGIN_TOP_RIGHT     1
#define COMPONENT_ORIGIN_BOTTOM_LEFT   2
#define COMPONENT_ORIGIN_BOTTOM_RIGHT  3

// Anchor: where on the parent the element tracks during resize (9-grid).
// Values mirror CONTAINER_ANCHOR_* exactly for a value-identical migration.
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
// Decoupled: specifies which point on the child docks to the parent's anchor point.
#define COMPONENT_PIVOT_TOP_LEFT      0
#define COMPONENT_PIVOT_TOP_RIGHT     1
#define COMPONENT_PIVOT_BOTTOM_LEFT   2
#define COMPONENT_PIVOT_BOTTOM_RIGHT  3
#define COMPONENT_PIVOT_CENTER        4

// Corner radius modes.
#define COMPONENT_CORNER_ARC 0
#define COMPONENT_CORNER_SUPERELLIPSE 1

#define COMPONENT_COLOR_CLEAR 0x00000000u

// Constructor: Component() — detached leaf at origin, TOP_LEFT everything,
// visible, opaque, zero padding/border, hooks null.
Component *Component_0(void);

// Core Functions:
//   - Component_recompute(self)                : recompute abs from stored parent abs
//   - Component_setParentAbs(self, px, py, pw, ph) : parent reports its abs box (cascade entry)
//   - Component_render(self, graphics)         : on-demand: bg then fg hooks (visible only)
//   - Component_hitTest(self, px, py)          : point-in-abs-rect (visible only)
//   - Component_getContentRect(self, *oX, *oY, *oW, *oH) : abs content box (abs + padding)
void Component_recompute(Component *self);
void Component_setParentAbs(Component *self, float px, float py, float pw, float ph);
bool Component_render(Component *self, void *graphics);
bool Component_hitTest(const Component *self, float pointX, float pointY);
void Component_getContentRect(const Component *self, float *outX, float *outY,
                              float *outW, float *outH);

// Setters (geometry setters recompute abs eagerly; visual setters do not):
void Component_setX(Component *self, float x);
void Component_setY(Component *self, float y);
void Component_setWidth(Component *self, float w);
void Component_setHeight(Component *self, float h);
void Component_setLocation(Component *self, float x, float y);
void Component_setSize(Component *self, float w, float h);          // clamps [min, max]
void Component_setMinSize(Component *self, float w, float h);       // re-clamps current size
void Component_setMaxSize(Component *self, float w, float h);       // re-clamps current size
void Component_setOrigin(Component *self, int origin);
void Component_setAnchor(Component *self, int anchor);
void Component_setPivot(Component *self, int pivot);
void Component_setCenter(Component *self);                          // pivot CENTER (no percent)
void Component_setMargin(Component *self, float l, float t, float r, float b);
void Component_setPadding(Component *self, float l, float t, float r, float b);
void Component_setBorderWidth(Component *self, float w);
void Component_setBorderColor(Component *self, uint32_t color);
void Component_setBackgroundColor(Component *self, uint32_t color);
void Component_setRadius(Component *self, float r);
void Component_setRadiusMode(Component *self, int mode);
void Component_setOpacity(Component *self, float opacity);          // clamped 0..1
void Component_setZ(Component *self, int z);
void Component_setVisible(Component *self, bool visible);
void Component_setParent(Component *self, Component *parent);       // borrowed view
bool Component_addChild(Component *self, Component *child);
bool Component_removeChild(Component *self, Component *child);
uint32_t Component_getChildCount(const Component *self);
Component *Component_getChild(const Component *self, uint32_t index);
void Component_setBackgroundRender(Component *self, Component_RenderFn fn);
void Component_setForegroundRender(Component *self, Component_RenderFn fn);
void Component_setRenderUserdata(Component *self, void *userdata);

// Getters (nullptr-safe defaults):
float Component_getX(const Component *self);
float Component_getY(const Component *self);
float Component_getWidth(const Component *self);
float Component_getHeight(const Component *self);
float Component_getAbsX(const Component *self);
float Component_getAbsY(const Component *self);
float Component_getAbsW(const Component *self);
float Component_getAbsH(const Component *self);
void Component_getAbsRect(const Component *self, Vec4 *outRect);    // dest-last
void Component_getParentAbsRect(const Component *self, Vec4 *outRect);
int Component_getOrigin(const Component *self);
int Component_getAnchor(const Component *self);
int Component_getPivot(const Component *self);
float Component_getMinWidth(const Component *self);
float Component_getMinHeight(const Component *self);
float Component_getMaxWidth(const Component *self);
float Component_getMaxHeight(const Component *self);
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
Component *Component_getParent(const Component *self);
Component_RenderFn Component_getBackgroundRender(const Component *self);
Component_RenderFn Component_getForegroundRender(const Component *self);
void *Component_getRenderUserdata(const Component *self);

#endif