#include "darling/panel/panel.h"

#include "annotation/incomplete.h"
#include "nio/mem.h"
#include "oop/type.h"
#include "annotation/definition.h"
#include "annotation/overview.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Panel
 * ============================================================================
 * The UI panel: Container layout + Component element metadata +
 * background color + the parent/child tree — the base class of
 * Label/Picture/Scene and every R4 widget, since
 * every node IS-A Panel with extra payload on top. It embeds Container as
 * its first member (the Container-vs-Panel Law; the (Container*) pun in
 * canvas.c depends on the prefix) plus an embedded Component holding the
 * canonical element metadata (anchor/origin/pivot/margin/absolute cascade).
 * Geometry facades dual-write both (Container stays the reader until the
 * Component cascade wires up) and it owns a List of child
 * Panels; node connect runs both trees — Panel_addContainer /
 * Panel_removeChild mirror the edge into the embedded Component tree
 * (each geometry setter recomputes the content box eagerly), so a panel stack
 * IS a component stack; the VIEW model deep-copies structure but aliases shared payloads
 * (image/filters) BY POINTER through the source slot, with dirty flags
 * fanning out through the parent-ref set so every holder of a view
 * re-renders. Rendering is an ordered five-stage part pipeline
 * (background -> image -> text -> border -> foreground) with per-instance
 * function-pointer slots — the setter is the @Override, nullptr skips the
 * stage, and callers route through Panel_paintParts, which issues each
 * stage's draws through the active Graphics row into the panel's absolute
 * rect (the legacy Vulkan renderHandler slot died with the old renderer).
 * The background color lives on the embedded Component (Strict 0xRRGGBBAA).
 * All state is
 * arena-allocated (Memory_alloc) with symmetric getters/setters and
 * dest-last layout facades forwarding to the embedded Container.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Panel (embeds Container prefix + Component metadata)
 * LEVEL: L2 — Behavior (UI panel hierarchy behavior API)
 * ============================================================================
 * the UI panel: Container layout + Component element metadata + background
 * color + the parent/child tree. Base class of Label/Picture/Scene — every
 * node IS-A Panel with extra payload on top.
 *
 * STRUCT FIELDS (Mirroring darling/panel/panel.h):
 * ----------------------------------------------------------------------------
 *   Container base;                // Inherited layout/bounds/anchors/flags (see container.h).
 *                                  // FIRST: (Container*) punning depends on the prefix.
 *   Component component;           // Element metadata (anchor/origin/pivot/abs cascade
 *                                  // + background color, Strict 0xRRGGBBAA). Dual-written
 *                                  // by the facades; Container stays the reader until Shift 2.
 *   void *filters;                 // Render-graph slot (@Draft placeholder, not yet wired)
 *   Image *image;                  // Shared payload (aliased through views; graphvex Image)
 *   Panel_PartFn backgroundFn;     // Stage 0: fill; nullptr = skip
 *   Panel_PartFn imageFn;          // Stage 1: picture content; nullptr = skip
 *   Panel_PartFn textFn;           // Stage 2: label quad; nullptr = skip
 *   Panel_PartFn borderFn;         // Stage 3: stroke; nullptr = skip
 *   Panel_PartFn foregroundFn;     // Stage 4: caret/selection/filter; nullptr = skip
 *   struct Panel *source;          // Canonical panel this view proxies; nullptr = owns data
 *   struct Panel *parent;          // Tree parent; nullptr = root
 *   List *children;                // Owned child panels (List of Panel*)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Panel_0(void)
 *   - Panel_1(parent)
 *
 * Core Functions:
 *   - Panel_paintParts(panel, rect)
 *   - Panel_childCount(p)
 *   - Panel_containsChild(p, child)
 *   - Panel_addContainer(p, child)
 *   - Panel_removeChild(p, child)
 *   - Panel_add(parent, node)
 *   - Panel_refCount(p)
 *   - Panel_isTreeDirty(p)
 *   - Panel_clearTreeDirty(p)
 *
 * Setters:
 *   - Panel_setBackgroundColor(p, color)
 *   - Panel_setBackgroundColorRGBA(p, r, g, b, a)
 *   - Panel_setBackgroundFn(p, fn)
 *   - Panel_setImageFn(p, fn)
 *   - Panel_setTextFn(p, fn)
 *   - Panel_setBorderFn(p, fn)
 *   - Panel_setForegroundFn(p, fn)
 *   - Panel_setLocation(p, x, y)
 *   - Panel_setSize(p, w, h)
 *   - Panel_setMinSize(p, w, h)
 *   - Panel_setMaxSize(p, w, h)
 *   - Panel_setAnchor(p, anchor)
 *   - Panel_setPivot(p, pivot)
 *   - Panel_setOrigin(p, origin)             // Component-only (Container has no origin)
 *   - Panel_setVisible(p, visible)
 *   - Panel_setZ(p, z)
 *   - Panel_setImage(p, image)
 *   - Panel_setFilters(p, filters)
 *   - Panel_setBackgroundColorAndMark(p, color)
 *
 * Getters:
 *   - Panel_getBackgroundColor(p)
 *   - Panel_getBackgroundFn(p)
 *   - Panel_getImageFn(p)
 *   - Panel_getTextFn(p)
 *   - Panel_getBorderFn(p)
 *   - Panel_getForegroundFn(p)
 *   - Panel_isVisible(p)
 *   - Panel_getImage(p)
 *   - Panel_getFilters(p)
 *   - Panel_getParent(p)
 *   - Panel_hasParent(p)
 *   - Panel_getChild(p, index)
 *   - Panel_hasChildren(p)
 *   - Panel_getSource(p)
 * ============================================================================
 */


// darling/panel.c — panel + tree + view model (Legacy: darling/Panel.java).

#define PANEL_CHILDREN_INITIAL 4

// Default stage 0: solid background fill through the active Graphics row.
// Transparent color skips; opacity folds over the color. The color lives on
// the embedded Component (Strict 0xRRGGBBAA — the Brush contract), so the
// stack Brush is a plain literal: zero heap on the paint path.
static bool paintBackground(Panel *panel, const Rectangle *rect) {
    if (!panel || !rect)
        return false;
    if ((*rect).width <= 0.0f || (*rect).height <= 0.0f)
        return false;
    uint32_t color = GraphicsComponent_getBackgroundColor(&(*panel).component);
    if ((color & 0xFFu) == 0)
        return false;
    float op = GraphicsComponent_getOpacity(&(*panel).component);
    if (op <= 0.0f)
        return false;
    Brush brush = { color, op };
    return Graphics_fillRect(rect, &brush);
}

Panel *Panel_0(void) {
    Panel *p = (Panel*) Memory_alloc(TYPE_PANEL_SINGLETON, sizeof(Panel));
    if (!p)
        return nullptr;
    Container *b = Container_0();
    if (!b) {
        Memory_free(p);
        return nullptr;
    }
    // adopt the container block's contents into our prefix, then free the shell
    *(&(*p).base) = (*b);
    Memory_free(b);
    GraphicsComponent_init(&(*p).component);

    (*p).filters = nullptr;
    (*p).image = nullptr;
    (*p).backgroundFn = paintBackground;
    (*p).imageFn = nullptr;
    (*p).textFn = nullptr;
    (*p).borderFn = nullptr;
    (*p).foregroundFn = nullptr;
    (*p).source = nullptr;
    (*p).parent = nullptr;
    (*p).children = nullptr;
    return p;
}

Panel *Panel_1(Panel *parent) {
    Panel *p = Panel_0();
    if (p && parent)
        Panel_addContainer(parent, p);
    return p;
}

uint32_t Panel_getBackgroundColor(const Panel *p) {
    return p ? GraphicsComponent_getBackgroundColor(&(*p).component) : PANEL_COLOR_CLEAR;
}

void Panel_setBackgroundColor(Panel *p, uint32_t color) {
    if (!p)
        return;
    GraphicsComponent_setBackgroundColor(&(*p).component, color);
}

void Panel_setBackgroundColorRGBA(Panel *p, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    Panel_setBackgroundColor(p, ((uint32_t)r << 24) | ((uint32_t)g << 16)
        | ((uint32_t)b << 8) | (uint32_t)a);
}

static void markPartDirty(Panel *p) {
    (void) p;
}

Panel_PartFn Panel_getBackgroundFn(const Panel *p) {
    return p ? (*p).backgroundFn : nullptr;
}

void Panel_setBackgroundFn(Panel *p, Panel_PartFn fn) {
    if (!p)
        return;
    (*p).backgroundFn = fn;
    markPartDirty(p);
}

Panel_PartFn Panel_getImageFn(const Panel *p) {
    return p ? (*p).imageFn : nullptr;
}

void Panel_setImageFn(Panel *p, Panel_PartFn fn) {
    if (!p)
        return;
    (*p).imageFn = fn;
    markPartDirty(p);
}

Panel_PartFn Panel_getTextFn(const Panel *p) {
    return p ? (*p).textFn : nullptr;
}

void Panel_setTextFn(Panel *p, Panel_PartFn fn) {
    if (!p)
        return;
    (*p).textFn = fn;
    markPartDirty(p);
}

Panel_PartFn Panel_getBorderFn(const Panel *p) {
    return p ? (*p).borderFn : nullptr;
}

void Panel_setBorderFn(Panel *p, Panel_PartFn fn) {
    if (!p)
        return;
    (*p).borderFn = fn;
    markPartDirty(p);
}

Panel_PartFn Panel_getForegroundFn(const Panel *p) {
    return p ? (*p).foregroundFn : nullptr;
}

void Panel_setForegroundFn(Panel *p, Panel_PartFn fn) {
    if (!p)
        return;
    (*p).foregroundFn = fn;
    markPartDirty(p);
}

bool Panel_paintParts(Panel *panel, const Rectangle *rect) {
    if (!panel || !rect)
        return false;
    if ((*rect).width <= 0.0f || (*rect).height <= 0.0f)
        return false;
    bool drew = false;
    Panel_PartFn bg = (*panel).backgroundFn;
    if (bg)
        drew = bg(panel, rect) || drew;
    Panel_PartFn img = (*panel).imageFn;
    if (img)
        drew = img(panel, rect) || drew;
    Panel_PartFn txt = (*panel).textFn;
    if (txt)
        drew = txt(panel, rect) || drew;
    Panel_PartFn bd = (*panel).borderFn;
    if (bd)
        drew = bd(panel, rect) || drew;
    Panel_PartFn fg = (*panel).foregroundFn;
    if (fg)
        drew = fg(panel, rect) || drew;
    return drew;
}

void Panel_setBackgroundColorAndMark(Panel *p, uint32_t color) {
    Panel_setBackgroundColor(p, color);
}

Image *Panel_getImage(const Panel *p) {
    if (!p)
        return nullptr;
    const Panel *src = (*p).source;
    return src ? Panel_getImage(src) : (*p).image;
}

void Panel_setImage(Panel *p, Image *image) {
    if (!p)
        return;
    Panel *src = (*p).source;
    if (src) {
        (*src).image = image;
        return;
    }
    (*p).image = image;
}

void *Panel_getFilters(const Panel *p) {
    if (!p)
        return nullptr;
    const Panel *src = (*p).source;
    return src ? Panel_getFilters(src) : (*p).filters;
}

void Panel_setFilters(Panel *p, void *filters) {
    if (!p)
        return;
    Panel *src = (*p).source;
    if (src)
        Panel_setFilters(src, filters);
    else
        (*p).filters = filters;
}

const Panel *Panel_getSource(const Panel *p) {
    return p ? (*p).source : nullptr;
}

int Panel_refCount(const Panel *p) {
    // v1: ref-set tracked implicitly via source back-refs is not yet built;
    // count holders by walking? Contract keeps legacy Set — deferred until a
    // workload needs it. Reported as 0 for now.
    ;;INCOMPLETE // parent-ref set lands with the damage-rect walker
    (void)p;
    return 0;
}

Panel *Panel_getParent(const Panel *p) {
    return p ? (*p).parent : nullptr;
}

bool Panel_hasParent(const Panel *p) {
    return p && (*p).parent != nullptr;
}

size_t Panel_childCount(const Panel *p) {
    return (p && (*p).children) ? List_size((*p).children) : 0;
}

bool Panel_hasChildren(const Panel *p) {
    return Panel_childCount(p) > 0;
}

Panel *Panel_getChild(const Panel *p, size_t index) {
    if (!p || !(*p).children || index >= List_size((*p).children))
        return nullptr;
    return (Panel*) List_get((*p).children, index);
}

bool Panel_containsChild(const Panel *p, const Panel *child) {
    if (!p || !(*p).children || !child)
        return false;
    size_t n = List_size((*p).children);
    for (size_t i = 0; i < n; i++) {
        if ((Panel*) List_get((*p).children, i) == child)
            return true;
    }
    return false;
}

// Detach from current parent so the tree stays consistent.
static void detachFromParent(Panel *child) {
    Panel *old = (*child).parent;
    if (old)
        Panel_removeChild(old, child);
}

void Panel_addContainer(Panel *p, Panel *child) {
    if (!p || !child || p == child)
        return;
    detachFromParent(child);
    (*child).parent = p;

    if (!(*p).children)
        (*p).children = List(ID_LONG, PANEL_CHILDREN_INITIAL);
    if (!(*p).children) {
        (*child).parent = nullptr;
        return;
    }
    List_add((*p).children, (uint64_t)(uintptr_t)child);
    float contentX = 0.0f, contentY = 0.0f, contentW = 0.0f, contentH = 0.0f;
    GraphicsComponent_getContentRect(&(*p).component, &contentX, &contentY, &contentW, &contentH);
    GraphicsComponent_setParentAbs(&(*child).component, contentX, contentY, contentW, contentH);
}

bool Panel_removeChild(Panel *p, Panel *child) {
    if (!p || !child || !(*p).children)
        return false;
    size_t n = List_size((*p).children);
    for (size_t i = 0; i < n; i++) {
        if ((Panel*) List_get((*p).children, i) == (const Panel*) child) {
            List_remove((*p).children, i);
            if ((*child).parent == p)
                (*child).parent = nullptr;
            return true;
        }
    }
    return false;
}

Panel *Panel_add(Panel *parent, const Panel *node) {
    if (!parent || !node || parent == node)
        return nullptr;

    Panel *copy = Panel_0();
    if (!copy)
        return nullptr;

    // structural deep copy: the Component metadata is its own layout
    // (and carries the background color — Strict 0xRRGGBBAA)
    (*copy).component = (*node).component;
    // behavior travels with structure: a view renders exactly like its source
    (*copy).backgroundFn = (*node).backgroundFn;
    (*copy).imageFn = (*node).imageFn;
    (*copy).textFn = (*node).textFn;
    (*copy).borderFn = (*node).borderFn;
    (*copy).foregroundFn = (*node).foregroundFn;

    // payloads alias through the source slot (read/write-through above)
    (*copy).source = (Panel*) node;

    // deep-copy children
    size_t n = Panel_childCount(node);
    for (size_t i = 0; i < n; i++)
        Panel_add(copy, Panel_getChild(node, i));

    Panel_addContainer(parent, copy);
    return copy;
}

// Note: children lists are owned by each parent; Panel_free would need the
// pool-wide walker. Deferred to the scene teardown pass.

bool Panel_isTreeDirty(const Panel *p) {
    (void) p;
    return false;
}

void Panel_clearTreeDirty(Panel *p) {
    (void) p;
}
