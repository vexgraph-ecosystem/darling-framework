#ifndef DARLING_PANEL_H
#define DARLING_PANEL_H

#include <stdbool.h>
#include <stdint.h>

#include "c23/constructor.h"
#include "../../c23/darling-type.h"
#include "darling/container.h"
#include "darling/component.h"
#include "lang/graphics.h"
#include "lang/image.h"
#include "struct/list.h"
#include "struct/set.h"

// darling/panel/panel.h — the UI panel: Container layout + background color +
// the parent/child tree (Legacy: darling/Panel.java, contract-first).
//
// The VIEW model: Panel_add deep-copies STRUCTURE but aliases shared payloads
// (image/filters) BY POINTER via the source slot; dirty flags fan _out through
// the parent-ref set so every holder of a view re-renders.
//
// METHOD SLOTS ("@Override" in C): Java override swaps a vtable entry at
// class load; here it is an explicit function-pointer slot per INSTANCE.
// The setter is the @Override annotation, nullptr restores the built-in
// default, and callers never read slots directly — they call the dispatcher
// (Panel_paintParts), which runs the stages in order through the active
// Graphics row. Subclasses inherit the slots by embedding (Scene3D -> Scene
// -> Panel), no vtable needed.

struct Panel;

// Per-part paint slot: one stage of the ordered pipeline
// (background -> image -> text -> border -> foreground). A stage ISSUES its
// draws through the active Graphics row (bind the target + row first), painting
// into the panel's absolute rect; returns true when a draw was issued (feeds
// the empty-present guard). nullptr = skip stage.
typedef bool (*Panel_PartFn)(struct Panel *panel, const Rectangle *rect);

typedef struct Panel {
    Container base;         // embedded prefix — pass &(*panel).base upward.
                            // Stays FIRST: (Container*) panel punning (canvas.c) depends on it.
    Component component;    // element metadata (anchor/origin/pivot/abs cascade).
                            // Dual-written by the facades below; Container stays the
                            // reader until the Component cascade wires up (Shift 2).
                            // The background color lives here too (Strict 0xRRGGBBAA).
    void *filters;          // render-graph slot (@Draft placeholder)
    Image *image;           // payload slot: backing Image (Image class in graphvex)
    Panel_PartFn backgroundFn; // stage 0: fill / material; nullptr = skip
    Panel_PartFn imageFn;   // stage 1: picture / video / scene content
    Panel_PartFn textFn;    // stage 2: raster / SDF label quad
    Panel_PartFn borderFn;  // stage 3: stroke over content; nullptr = skip
    Panel_PartFn foregroundFn; // stage 4: caret / selection / filter / glow
    struct Panel *source;   // canonical panel this view proxies; nullptr = owns
    struct Panel *parent;   // nullptr = root
    List *children;
} Panel;

#define PANEL_COLOR_WHITE 0xFFFFFFFFu
#define PANEL_COLOR_BLACK 0x000000FFu
#define PANEL_COLOR_CLEAR 0x00000000u

// Constructors:
//   Panel()          — detached bare panel
//   Panel(parent)    — created and attached
Panel *Panel_0(void);
Panel *Panel_1(Panel *parent);

#define Panel(...) CONSTRUCTOR_DISPATCH(Panel, __VA_ARGS__)

// Background color (Strict 0xRRGGBBAA — lives on the embedded Component).
uint32_t Panel_getBackgroundColor(const Panel *p);
void Panel_setBackgroundColor(Panel *p, uint32_t color);
void Panel_setBackgroundColorRGBA(Panel *p, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

// Per-part paint slots (ordered pipeline: background -> image -> text
// -> border -> foreground). Setter is the @Override; nullptr skips the
// stage. Each setter marks the tree dirty so holders re-render this tick.
Panel_PartFn Panel_getBackgroundFn(const Panel *p);
void Panel_setBackgroundFn(Panel *p, Panel_PartFn fn);
Panel_PartFn Panel_getImageFn(const Panel *p);
void Panel_setImageFn(Panel *p, Panel_PartFn fn);
Panel_PartFn Panel_getTextFn(const Panel *p);
void Panel_setTextFn(Panel *p, Panel_PartFn fn);
Panel_PartFn Panel_getBorderFn(const Panel *p);
void Panel_setBorderFn(Panel *p, Panel_PartFn fn);
Panel_PartFn Panel_getForegroundFn(const Panel *p);
void Panel_setForegroundFn(Panel *p, Panel_PartFn fn);

// Ordered dispatcher: runs background -> image -> text -> border ->
// foreground through the active Graphics row, skipping nulls. Returns true
// when any stage issued a draw.
bool Panel_paintParts(Panel *panel, const Rectangle *rect);

// Layout facade — the delegation chain ends here. Every accessor below is a
// one-hop static inline to the embedded Component metadata, so call sites
// never pierce (*panel).component directly. The embedded Container base is
// just a Component[] node (child metadata list) and carries no layout.
// Subclass levels re-export the same names over their embedded prefix
// (Scene_setLocation -> Panel_setLocation -> GraphicsComponent_setLocation), which
// is Java's inherited methods without a vtable: static binding, zero
// runtime cost, type-checked at each level.
static inline void Panel_setLocation(Panel *p, float x, float y)
    { if (p) GraphicsComponent_setLocation(&(*p).component, x, y); }
static inline void Panel_setSize(Panel *p, float w, float h)
    { if (p) GraphicsComponent_setSize(&(*p).component, w, h); }
static inline void Panel_setMinSize(Panel *p, float w, float h)
    { if (p) GraphicsComponent_setMinSize(&(*p).component, w, h); }
static inline void Panel_setMaxSize(Panel *p, float w, float h)
    { if (p) GraphicsComponent_setMaxSize(&(*p).component, w, h); }
static inline void Panel_setAnchor(Panel *p, int anchor)
    { if (p) GraphicsComponent_setAnchor(&(*p).component, anchor); }
static inline void Panel_setPivot(Panel *p, int pivot)
    { if (p) GraphicsComponent_setPivot(&(*p).component, pivot); }
static inline void Panel_setOrigin(Panel *p, int origin)
    { if (p) GraphicsComponent_setOrigin(&(*p).component, origin); }
static inline void Panel_setVisible(Panel *p, bool visible)
    { if (p) GraphicsComponent_setVisible(&(*p).component, visible); }
static inline void Panel_setOpacity(Panel *p, float opacity)
    { if (p) GraphicsComponent_setOpacity(&(*p).component, opacity); }
static inline float Panel_getOpacity(const Panel *p)
    { return p ? GraphicsComponent_getOpacity(&(*p).component) : 1.0f; }
static inline bool Panel_isVisible(const Panel *p)
    { return p && GraphicsComponent_isVisible(&(*p).component); }
static inline void Panel_setZ(Panel *p, int z)
    { if (p) GraphicsComponent_setZ(&(*p).component, z); }
static inline void Panel_setMargin(Panel *p, float l, float t, float r, float b)
    { if (p) GraphicsComponent_setMargin(&(*p).component, l, t, r, b); }
static inline void Panel_getMargin(const Panel *p, float *l, float *t, float *r, float *b)
    { if (p) GraphicsComponent_getMargin(&(*p).component, l, t, r, b); }
static inline void Panel_setRadius(Panel *p, float r)
    { if (p) GraphicsComponent_setCornerRadius(&(*p).component, r); }
static inline float Panel_getRadius(const Panel *p)
    { return p ? GraphicsComponent_getCornerRadius(&(*p).component) : 0.0f; }
static inline void Panel_setRadiusMode(Panel *p, int mode)
    { if (p) GraphicsComponent_setRadiusMode(&(*p).component, mode); }
static inline int Panel_getRadiusMode(const Panel *p)
    { return p ? GraphicsComponent_getRadiusMode(&(*p).component) : COMPONENT_CORNER_ARC; }

// Shared payload slots (read/write-through to the canonical source on views).
Image *Panel_getImage(const Panel *p);
void   Panel_setImage(Panel *p, Image *image);
void *Panel_getFilters(const Panel *p);
void Panel_setFilters(Panel *p, void *filters);

// Tree.
Panel *Panel_getParent(const Panel *p);
bool Panel_hasParent(const Panel *p);
size_t Panel_childCount(const Panel *p);
Panel *Panel_getChild(const Panel *p, size_t index);
bool Panel_hasChildren(const Panel *p);
bool Panel_containsChild(const Panel *p, const Panel *child);
void Panel_addContainer(Panel *p, Panel *child);
bool Panel_removeChild(Panel *p, Panel *child);

// Structural deep copy with aliased payloads; the copy is attached to parent.
Panel *Panel_add(Panel *parent, const Panel *node);

// Tree dirtiness (present-on-demand support).
bool Panel_isTreeDirty(const Panel *p);
void Panel_clearTreeDirty(Panel *p);

// View bookkeeping.
const Panel *Panel_getSource(const Panel *p);
int Panel_refCount(const Panel *p);

// Layout passthroughs live on Container: use &(*panel).base.
// Convenience here for the two most common:
void Panel_setBackgroundColorAndMark(Panel *p, uint32_t color); // legacy parity alias

#endif
