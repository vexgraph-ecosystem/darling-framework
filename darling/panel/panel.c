#include "darling/panel/panel.h"

#include "annotation/incomplete.h"
#include "nio/mem.h"
#include "oop/type.h"
#include "annotation/overview.h"
#include "vulkan/vk.h"

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Panel (embeds Container)
 * LEVEL: L2 — Behavior (UI panel hierarchy behavior API)
 * ============================================================================
 * the UI panel: Container layout + background color +
 * the parent/child tree. Base class of Label/Picture/Scene — every node
 * IS-A Panel with extra payload on top.
 *
 * STRUCT FIELDS (Mirroring darling/panel/panel.h):
 * ----------------------------------------------------------------------------
 *   Container base;                // Inherited layout/bounds/anchors/flags (see container.h)
 *   uint32_t color;                // Background fill, packed 0xAARRGGBB
 *   void *filters;                 // Render-graph slot (@Draft placeholder, not yet wired)
 *   void *image;                   // Shared payload pointer (aliased through views)
 *   Panel_RenderFn renderHandler;  // Legacy monolith; non-null = back-compat path
 *   void *renderUserdata;          // Opaque handler state; never interpreted
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
 *   - Panel_paintParts(panel, renderer, cmdBuffer, surfaceW, surfaceH, x, y, w, h)
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
 *   - Panel_setRenderHandler(p, fn)
 *   - Panel_setRenderUserdata(p, userdata)
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
 *   - Panel_setVisible(p, visible)
 *   - Panel_setZ(p, z)
 *   - Panel_setImage(p, image)
 *   - Panel_setFilters(p, filters)
 *   - Panel_setBackgroundColorAndMark(p, color)
 *
 * Getters:
 *   - Panel_getBackgroundColor(p)
 *   - Panel_getRenderHandler(p)
 *   - Panel_getRenderUserdata(p)
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

// Default stage 0: solid background fill via solid_quad.
// Transparent color skips; opacity folds per Container.
static bool paintBackground(Panel *panel, void *renderer, void *cmdBuffer,
                            float surfaceW, float surfaceH,
                            float x, float y, float w, float h) {
    (void) renderer;
    if (!panel || !cmdBuffer)
        return false;
    if (w <= 0.0f || h <= 0.0f)
        return false;
    uint32_t color = (*panel).color;
    if ((color >> 24) == 0)
        return false;
    Container *base = &(*panel).base;
    float op = Container_getOpacity(base);
    if (op <= 0.0f)
        return false;
    float r = (float) ((color >> 16) & 0xFF) / 255.0f;
    float g = (float) ((color >> 8) & 0xFF) / 255.0f;
    float b = (float) (color & 0xFF) / 255.0f;
    float a = (float) ((color >> 24) & 0xFF) / 255.0f * op;
    if (a <= 0.0f)
        return false;
    Vk_fillRect(cmdBuffer, surfaceW, surfaceH, x, y, w, h, r, g, b, a);
    return true;
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

    (*p).color = PANEL_COLOR_CLEAR;
    (*p).filters = nullptr;
    (*p).image = nullptr;
    (*p).renderHandler = nullptr;
    (*p).renderUserdata = nullptr;
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
    return p ? (*p).color : PANEL_COLOR_CLEAR;
}

void Panel_setBackgroundColor(Panel *p, uint32_t color) {
    if (!p)
        return;
    (*p).color = color;
    Container_markDirty(&(*p).base);
}

void Panel_setBackgroundColorRGBA(Panel *p, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    Panel_setBackgroundColor(p, ((uint32_t)a << 24) | ((uint32_t)r << 16)
        | ((uint32_t)g << 8) | (uint32_t)b);
}

// Method-slot accessors: setting a handler is the @Override; nullptr restores
// the renderer default. Marked dirty so every holder re-renders this tick.
Panel_RenderFn Panel_getRenderHandler(const Panel *p) {
    return p ? (*p).renderHandler : nullptr;
}

void Panel_setRenderHandler(Panel *p, Panel_RenderFn fn) {
    if (!p)
        return;
    (*p).renderHandler = fn;
    Container_markDirty(&(*p).base);
}

// Opaque handler state (e.g. per-pane animation structs). Pure slot — never
// interpreted, never copied by views (payload aliasing stops at handler data).
void *Panel_getRenderUserdata(const Panel *p) {
    return p ? (*p).renderUserdata : nullptr;
}

void Panel_setRenderUserdata(Panel *p, void *userdata) {
    if (!p)
        return;
    (*p).renderUserdata = userdata;
}

static void markPartDirty(Panel *p) {
    if (!p)
        return;
    Container *c = &(*p).base;
    Container_markDirty(c);
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

bool Panel_paintParts(Panel *panel, void *renderer, void *cmdBuffer,
                      float surfaceW, float surfaceH,
                      float x, float y, float w, float h) {
    if (!panel || !cmdBuffer)
        return false;
    if (w <= 0.0f || h <= 0.0f)
        return false;
    Panel_RenderFn mono = (*panel).renderHandler;
    if (mono) {
        mono(panel, renderer, cmdBuffer, surfaceW, surfaceH, x, y, w, h);
        return true;
    }
    bool drew = false;
    Panel_PartFn bg = (*panel).backgroundFn;
    if (bg)
        drew = bg(panel, renderer, cmdBuffer, surfaceW, surfaceH, x, y, w, h) || drew;
    Panel_PartFn img = (*panel).imageFn;
    if (img)
        drew = img(panel, renderer, cmdBuffer, surfaceW, surfaceH, x, y, w, h) || drew;
    Panel_PartFn txt = (*panel).textFn;
    if (txt)
        drew = txt(panel, renderer, cmdBuffer, surfaceW, surfaceH, x, y, w, h) || drew;
    Panel_PartFn bd = (*panel).borderFn;
    if (bd)
        drew = bd(panel, renderer, cmdBuffer, surfaceW, surfaceH, x, y, w, h) || drew;
    Panel_PartFn fg = (*panel).foregroundFn;
    if (fg)
        drew = fg(panel, renderer, cmdBuffer, surfaceW, surfaceH, x, y, w, h) || drew;
    return drew;
}

void Panel_setBackgroundColorAndMark(Panel *p, uint32_t color) {
    Panel_setBackgroundColor(p, color);
}

void *Panel_getImage(const Panel *p) {
    if (!p)
        return nullptr;
    const Panel *src = (*p).source;
    return src ? Panel_getImage(src) : (*p).image;
}

void Panel_setImage(Panel *p, void *image) {
    if (!p)
        return;
    Panel *src = (*p).source;
    if (src) {
        // write-through to canonical, fan _out dirt to every holder
        (*src).image = image;
        Container_markDirty(&(*src).base);
        if ((*src).children) {
            size_t n = List_size((*src).children);
            for (size_t i = 0; i < n; i++) {
                Panel *holder = (Panel*) List_get((*src).children, i);
                Container_markDirty(&(*holder).base);
            }
        }
        return;
    }
    (*p).image = image;
    Container_markDirty(&(*p).base);
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
    Container_markDirty(&(*p).base);
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
    Container_markDirty(&(*p).base);
    Container_markDirty(&(*child).base);
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
            Container_markDirty(&(*p).base);
            Container_markDirty(&(*child).base);
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

    // structural deep copy: layout is its own
    Container *cb = &(*copy).base;
    const Container *nb = &(*((Panel*) node)).base;
    (*cb).x = (*nb).x;
    (*cb).y = (*nb).y;
    (*cb).w = (*nb).w;
    (*cb).h = (*nb).h;
    (*cb).scaleX = (*nb).scaleX;
    (*cb).scaleY = (*nb).scaleY;
    (*cb).anchor = (*nb).anchor;
    (*cb).pivot = (*nb).pivot;
    (*cb).z = (*nb).z;
    (*cb).visible = (*nb).visible;
    (*cb).enabled = (*nb).enabled;
    (*cb).clipping = (*nb).clipping;
    (*cb).percentX = (*nb).percentX;
    (*cb).percentY = (*nb).percentY;
    (*copy).color = (*node).color;
    // behavior travels with structure: a view renders exactly like its source
    (*copy).renderHandler = (*node).renderHandler;
    // handler state aliases like a payload (opaque, shared through the view)
    (*copy).renderUserdata = (*node).renderUserdata;
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
    Container_markDirty(cb);
    return copy;
}

// Note: children lists are owned by each parent; Panel_free would need the
// pool-wide walker. Deferred to the scene teardown pass.

bool Panel_isTreeDirty(const Panel *p) {
    if (!p)
        return false;
    const Container *c = &(*p).base;
    if (Container_isDirty(c))
        return true;
    size_t n = Panel_childCount(p);
    for (size_t i = 0; i < n; i++) {
        Panel *child = Panel_getChild(p, i);
        if (Panel_isTreeDirty(child))
            return true;
    }
    return false;
}

void Panel_clearTreeDirty(Panel *p) {
    if (!p)
        return;
    Container *c = &(*p).base;
    Container_clearDirty(c);
    size_t n = Panel_childCount(p);
    for (size_t i = 0; i < n; i++) {
        Panel *child = Panel_getChild(p, i);
        Panel_clearTreeDirty(child);
    }
}
