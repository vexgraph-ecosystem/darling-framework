#ifndef DARLING_CONTAINER_H
#define DARLING_CONTAINER_H

#include <stdbool.h>
#include <stdint.h>

#include "c23/constructor.h"
#include "lang/vec4.h"

// darling/container.h — layout base of every darling node
// (Legacy: darling/Container.java, translated under contract-first).
//
// Owns position, size, scale, the anchor+pivot system, percentage placement,
// z-order and the visible/enabled/dirty/clipping flags. Subclasses EMBED this
// struct as their first member, so a subclass pointer's prefix lines up:
// pass &(*panel).base to any Container accessor.
//
// The anchor+pivot system:
//   ANCHOR (9-grid): where on the parent the element tracks during resize.
//                    Determines which edges/corners follow the parent's delta.
//   PIVOT (5 points): where on the element is the reference point for placement.
//                     Determines which corner/center lands at the resolved target.
//
// The subtle law: invalidateBase fires ONLY on layout edits. State edits
// (color, hover) must never recapture the base or anchored panels jump
// mid-resize.

typedef struct Container {
    float x, y, w, h;
    float scaleX, scaleY;
    uint8_t anchor;         // CONTAINER_ANCHOR_* 0..8: where on parent this tracks during resize
    int32_t pivot;          // CONTAINER_PIVOT_* 0..4: element's reference point for placement
    float percentX, percentY;
    int32_t z;
    uint8_t visible;
    uint8_t enabled;
    uint8_t dirty;
    uint8_t clipping;
    uint8_t lockedRoot;     // 1 = board-root pane (Window Board Root Lock Law): setSize/setLocation are no-ops
    float opacity;        // 0..1 alpha multiplier over every paint of this node (default 1 = opaque)
    float baseW, baseH;     // parent size at last layout (resize-delta reference)
    float minW, minH;       // size constraints (default 0,0)
    float maxW, maxH;       // size constraints (default 0 = unset)
    float marginL, marginT; // additive margin: final = location + margin (default 0)
    float marginR, marginB; // right/bottom edges stored for sibling layout (default 0)
    float radius;           // corner radius in parent units (default 0 = square)
    int radiusMode;         // CORNER_ARC (0) or CORNER_SUPERELLIPSE (1)
} Container;

// Anchor: where on the parent the element tracks during resize (9-grid).
#define CONTAINER_ANCHOR_TOP_LEFT      0
#define CONTAINER_ANCHOR_TOP_CENTER    1
#define CONTAINER_ANCHOR_TOP_RIGHT     2
#define CONTAINER_ANCHOR_MIDDLE_LEFT   3
#define CONTAINER_ANCHOR_MIDDLE_CENTER 4
#define CONTAINER_ANCHOR_MIDDLE_RIGHT  5
#define CONTAINER_ANCHOR_BOTTOM_LEFT   6
#define CONTAINER_ANCHOR_BOTTOM_CENTER 7
#define CONTAINER_ANCHOR_BOTTOM_RIGHT  8

// Pivot: the element's reference point for placement (5 points).
#define CONTAINER_PIVOT_TOP_LEFT      0
#define CONTAINER_PIVOT_TOP_RIGHT     1
#define CONTAINER_PIVOT_BOTTOM_LEFT   2
#define CONTAINER_PIVOT_BOTTOM_RIGHT  3
#define CONTAINER_PIVOT_CENTER        4

#define CONTAINER_PERCENT_UNSET (-1.0f)

// Corner radius modes (Phase 1 substrate: margin + radius laws).
#define CORNER_ARC 0
#define CORNER_SUPERELLIPSE 1

// Constructor: Container() — defaults at origin, TOP_LEFT everything.
Container *Container_0(void);

// Position / size / scale
float Container_getX(const Container *c);
float Container_getY(const Container *c);
float Container_getWidth(const Container *c);
float Container_getHeight(const Container *c);
void Container_setX(Container *c, float x);
void Container_setY(Container *c, float y);
void Container_setWidth(Container *c, float w);
void Container_setHeight(Container *c, float h);
void Container_setLocation(Container *c, float x, float y);
void Container_setSize(Container *c, float w, float h);
void Container_setMinSize(Container *c, float w, float h);
void Container_setMaxSize(Container *c, float w, float h);
float Container_getScaleWidth(const Container *c);
float Container_getScaleHeight(const Container *c);
void Container_setScale(Container *c, float sx, float sy);

// Anchor / pivot / percent
int Container_getAnchor(const Container *c);
void Container_setAnchor(Container *c, int anchor);
int Container_getPivot(const Container *c);
void Container_setPivot(Container *c, int pivot);
void Container_setCenter(Container *c);
float Container_getPercentX(const Container *c);
float Container_getPercentY(const Container *c);
void Container_setPercentX(Container *c, float pct);
void Container_setPercentY(Container *c, float pct);
bool Container_hasPercentX(const Container *c);
bool Container_hasPercentY(const Container *c);

// Z / state flags
int Container_getZ(const Container *c);
void Container_setZ(Container *c, int z);
bool Container_isVisible(const Container *c);
void Container_setVisible(Container *c, bool visible);
bool Container_isEnabled(const Container *c);
void Container_setEnabled(Container *c, bool enabled);
bool Container_isClipChildren(const Container *c);
void Container_setClipChildren(Container *c, bool clip);
// Opacity: per-node alpha multiplier folded over bg, text, and quads (0 = fully
// transparent but still laid out and hit-tested; 1 = opaque). State edit:
// dirties, never recaptures base.
void Container_setOpacity(Container *c, float opacity);
float Container_getOpacity(const Container *c);
bool Container_isDirty(const Container *c);
void Container_markDirty(Container *c);
void Container_clearDirty(Container *c);

// Root lock (the Window Board Root Lock Law): setSize/setLocation are silent
// no-ops when lockedRoot is set. Frame_resize uses Container_forceSize /
// Container_forceLocation (internal bypass — NOT public API) to update
// locked root geometry without triggering the guard.
bool Container_isLockedRoot(const Container *c);
void Container_setLockedRoot(Container *c, bool locked);
// Internal force-setters: bypass lockedRoot guard. Used ONLY by Frame_resize.
// Not public API — do not call from outside darling-framework.
void Container_forceSize(Container *c, float w, float h);
void Container_forceLocation(Container *c, float x, float y);

// Margin (additive: final = location + margin, dest-last outs) + corner radius.
void Container_setMargin(Container *c, float l, float t, float r, float b);
void Container_getMargin(const Container *c, float *l, float *t, float *r, float *b);
void Container_setRadius(Container *c, float r);
float Container_getRadius(const Container *c);
void Container_setRadiusMode(Container *c, int mode);
int Container_getRadiusMode(const Container *c);

// Resolve layout into a screen rect [x, y, w, h] — dest LAST.
void Container_resolve(Container *c, float parentX, float parentY,
                       float parentW, float parentH, Vec4 *outRect);

// Point-in-resolved-rect _test (visible nodes only).
bool Container_hitTest(Container *c, float parentX, float parentY,
                       float parentW, float parentH, float pointX, float pointY);

#endif
