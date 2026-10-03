#include "panel/scroll_panel.h"

#include <stdlib.h>

#include "annotation/definition.h"
#include "annotation/overview.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: ScrollPanel (panel/scroll_panel.c)
 * ============================================================================
 * A viewport over oversized content: one clipped Element (the viewport) holding
 * one owned child Element (the content) offset by (-offsetX, -offsetY). The
 * offset is the single source of truth and is end-clamped per axis on every
 * write, so a shrinking content or viewport can never maroon the view.
 *
 * The viewport Element clips its children (Property.clip), so the content is
 * cut at the viewport edge for free. Revalidation is explicit: every setter
 * funnels through setOffset, which re-clamps and reflects the offset into the
 * tree as the content's placement, then revalidates the owning root. Scrolling
 * from a wheel is just a scroll handler wired to ScrollPanel_scrollBy; the
 * panel itself never touches the OS.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: ScrollPanel (panel/scroll_panel.c)
 * ============================================================================
 * Clipped viewport + owned content child, with an end-clamped scroll offset.
 *
 * STRUCT FIELDS:
 * ----------------------------------------------------------------------------
 *   Element *viewport;  // owned, clipped Element (the scroll window)
 *   Element *content;   // owned; a TOP_LEFT child of the viewport
 *   float viewW, viewH; // viewport extent (native px)
 *   float contentW, contentH; // content extent (native px)
 *   float offsetX, offsetY;   // scroll offset; [0, max(0, content-viewport)]
 *
 * PRIVATE HELPERS:
 * ----------------------------------------------------------------------------
 *   (none — every path funnels through ScrollPanel_setOffset)
 *
 * FUNCTION REGISTRY (exported by panel/scroll_panel.h):
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - ScrollPanel_0 / ScrollPanel_1 / ScrollPanel_2, and ScrollPanel(...)
 * Core:
 *   - ScrollPanel_destroy, ScrollPanel_graphics, ScrollPanel_content,
 *     ScrollPanel_setContent, ScrollPanel_revalidate
 * Extents:
 *   - ScrollPanel_setViewportSize, setContentSize, viewportWidth/Height,
 *     contentWidth/Height
 * Offset:
 *   - ScrollPanel_setOffset, getOffset, scrollBy, maxX, maxY,
 *     canScrollX, canScrollY
 * Events:
 *   - ScrollPanel_add<Kind>Event (IMPLEMENT_EVENTS; see event_invoke.h)
 * ============================================================================
 */

struct ScrollPanel {
    Element *viewport;      // owned, clipped
    Element *content;       // owned; a child of the viewport
    float viewW, viewH;
    float contentW, contentH;
    float offsetX, offsetY;
};

ScrollPanel *ScrollPanel_0(void) { return ScrollPanel_2(0.0f, 0.0f); }
ScrollPanel *ScrollPanel_1(float viewSize) { return ScrollPanel_2(viewSize, viewSize); }

ScrollPanel *ScrollPanel_2(float viewW, float viewH) {
    ScrollPanel *sp = calloc(1, sizeof *sp);
    if (!sp) return NULL;
    (*sp).viewport = Element();
    if (!(*sp).viewport) { free(sp); return NULL; }
    Element_setSize((*sp).viewport, viewW, viewH);
    Element_setClip((*sp).viewport, true);
    (*sp).viewW = viewW;
    (*sp).viewH = viewH;
    return sp;
}

void ScrollPanel_destroy(ScrollPanel *sp) {
    if (!sp) return;
    Element_destroy((*sp).viewport);   // frees the viewport and its content child
    free(sp);
}

Element *ScrollPanel_graphics(const ScrollPanel *sp) { return sp ? (*sp).viewport : NULL; }
Element *ScrollPanel_content(const ScrollPanel *sp) { return sp ? (*sp).content : NULL; }

void ScrollPanel_setContent(ScrollPanel *sp, Element *content) {
    if (!sp) return;
    if ((*sp).content) Element_remove((*sp).content);   // detach the old (owned) child
    (*sp).content = content;
    if (content) {
        Element_add((*sp).viewport, content);
        (*sp).contentW = Element_width(content);
        (*sp).contentH = Element_height(content);
    } else {
        (*sp).contentW = 0.0f;
        (*sp).contentH = 0.0f;
    }
    ScrollPanel_setOffset(sp, (*sp).offsetX, (*sp).offsetY);   // re-clamp
}

void ScrollPanel_setViewportSize(ScrollPanel *sp, float w, float h) {
    if (!sp) return;
    (*sp).viewW = w;
    (*sp).viewH = h;
    Element_setSize((*sp).viewport, w, h);
    ScrollPanel_setOffset(sp, (*sp).offsetX, (*sp).offsetY);   // re-clamp
}

void ScrollPanel_setContentSize(ScrollPanel *sp, float w, float h) {
    if (!sp) return;
    (*sp).contentW = w;
    (*sp).contentH = h;
    ScrollPanel_setOffset(sp, (*sp).offsetX, (*sp).offsetY);   // re-clamp
}

float ScrollPanel_viewportWidth(const ScrollPanel *sp) { return sp ? (*sp).viewW : 0.0f; }
float ScrollPanel_viewportHeight(const ScrollPanel *sp) { return sp ? (*sp).viewH : 0.0f; }
float ScrollPanel_contentWidth(const ScrollPanel *sp) { return sp ? (*sp).contentW : 0.0f; }
float ScrollPanel_contentHeight(const ScrollPanel *sp) { return sp ? (*sp).contentH : 0.0f; }

float ScrollPanel_maxX(const ScrollPanel *sp) {
    if (!sp) return 0.0f;
    float m = (*sp).contentW - (*sp).viewW;
    return m > 0.0f ? m : 0.0f;
}
float ScrollPanel_maxY(const ScrollPanel *sp) {
    if (!sp) return 0.0f;
    float m = (*sp).contentH - (*sp).viewH;
    return m > 0.0f ? m : 0.0f;
}
bool ScrollPanel_canScrollX(const ScrollPanel *sp) { return ScrollPanel_maxX(sp) > 0.0f; }
bool ScrollPanel_canScrollY(const ScrollPanel *sp) { return ScrollPanel_maxY(sp) > 0.0f; }

void ScrollPanel_setOffset(ScrollPanel *sp, float x, float y) {
    if (!sp) return;
    float mx = ScrollPanel_maxX(sp), my = ScrollPanel_maxY(sp);
    if (x < 0.0f) x = 0.0f; if (x > mx) x = mx;   // THE clamp: one funnel
    if (y < 0.0f) y = 0.0f; if (y > my) y = my;
    (*sp).offsetX = x;
    (*sp).offsetY = y;
    ScrollPanel_revalidate(sp);
}

void ScrollPanel_getOffset(const ScrollPanel *sp, float *outX, float *outY) {
    if (outX) *outX = sp ? (*sp).offsetX : 0.0f;
    if (outY) *outY = sp ? (*sp).offsetY : 0.0f;
}

void ScrollPanel_scrollBy(ScrollPanel *sp, float dx, float dy) {
    if (!sp) return;
    ScrollPanel_setOffset(sp, (*sp).offsetX + dx, (*sp).offsetY + dy);
}

void ScrollPanel_revalidate(ScrollPanel *sp) {
    if (!sp || !(*sp).viewport) return;
    if ((*sp).content) {
        // content is a TOP_LEFT child; a negative offset scrolls it up/left
        Element_setOffset((*sp).content, -(*sp).offsetX, -(*sp).offsetY);
    }
    Element_markDirty((*sp).viewport);
    Element_revalidate(Element_root((*sp).viewport));
}

// ── events ──────────────────────────────────────────────────────────────────
// The `##` IS the class: these become ScrollPanel_addMouseEvent,
// ScrollPanel_addScrollEvent, ScrollPanel_addZoomEvent, ScrollPanel_addKeyEvent,
// ScrollPanel_addTouchEvent, ScrollPanel_addDocumentEvent.
IMPLEMENT_EVENTS(ScrollPanel)
