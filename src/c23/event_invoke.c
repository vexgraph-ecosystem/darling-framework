#include "c23/event_invoke.h"

#include <stdlib.h>
#include <string.h>

#include "annotation/definition.h"
#include "annotation/overview.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: EventInvoke (c23/event_invoke.c)
 * ============================================================================
 * The registry behind the per-element event invoker (see c23/event_invoke.h).
 * Every Element is an event target; a class opts in with IMPLEMENT_<KIND>_EVENT,
 * which generates <Class>_add<Kind>Event from the class name. This file owns the
 * storage all those adders write into and the one dispatcher that reads it.
 *
 * Lifetime: a binding is BORROWED. The tree owns no handler and no userdata.
 * One binding per (element, kind-group): registering again REPLACES the
 * handlers, so a class can re-arm without leaking. Because pointers are keyed,
 * an element must be Element_clearEvents()'d (or its root cleared) before its
 * subtree is freed, or the table keeps dangling keys.
 *
 * Dispatch: hit-test the tree to the deepest element under the point, then walk
 * ancestors — the child handler runs before its parent's (bubbling). Pointer
 * kinds (mouse/scroll/zoom) use the hit element; key/touch/document route to
 * the given root, since they carry no point to test.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: EventInvoke (c23/event_invoke.c)
 * ============================================================================
 * Flat registry + dispatcher for per-element handlers. File-static state, no
 * owned class instance.
 *
 * FILE-STATIC STATE:
 * ----------------------------------------------------------------------------
 *   Binding *s_bindings;  // growable table, one row per (element, group)
 *   int      s_count;     // live rows
 *   int      s_capacity;  // allocated rows
 *
 * PRIVATE TYPES:
 * ----------------------------------------------------------------------------
 *   Handlers : union of MouseEvent/ScrollEvent/ZoomEvent/KeyEvent/TouchEvent/
 *              DocumentEvent — one row stores whichever group it is tagged with
 *   Binding  : { Element *element; int group; Handlers h; }
 *   GRP_*    : mouse, scroll, zoom, key, touch, document group tags
 *
 * PRIVATE HELPERS:
 * ----------------------------------------------------------------------------
 *   bind(element, group, handlers, size) : replace-or-insert one row
 *   find(element, group)                 : row lookup (const)
 *   group_of(kind)                       : EV_* to GRP_* (-1 if unknown)
 *   invoke(binding, event)               : run the one handler for the kind
 *   under(element, root)                 : is element inside root's subtree?
 *
 * FUNCTION REGISTRY (exported by c23/event_invoke.h):
 * ----------------------------------------------------------------------------
 * Base adders:
 *   - Element_addMouseEvent / addScrollEvent / addZoomEvent / addKeyEvent /
 *     addTouchEvent / addDocumentEvent
 * Dispatch / lifecycle:
 *   - Element_dispatchEvent(root, event)
 *   - Element_clearEvents(root)
 *   - Element_eventBindingCount(void)
 * ============================================================================
 */

typedef union Handlers {
    MouseEvent    mouse;
    ScrollEvent   scroll;
    ZoomEvent     zoom;
    KeyEvent      key;
    TouchEvent    touch;
    DocumentEvent document;
} Handlers;

typedef struct Binding {
    Element *element;
    int      group;
    Handlers h;
} Binding;

enum {
    GRP_MOUSE = 0,
    GRP_SCROLL,
    GRP_ZOOM,
    GRP_KEY,
    GRP_TOUCH,
    GRP_DOCUMENT,
};

static Binding *s_bindings = NULL;
static int s_count = 0;
static int s_capacity = 0;

static Element *bind(Element *element, int group, const void *handlers, size_t size) {
    if (!element) return NULL;
    for (int i = 0; i < s_count; i++) {
        if (s_bindings[i].element == element && s_bindings[i].group == group) {
            memcpy(&s_bindings[i].h, handlers, size);
            return element;
        }
    }
    if (s_count == s_capacity) {
        int next = s_capacity ? s_capacity * 2 : 16;
        Binding *grown = realloc(s_bindings, (size_t)next * sizeof *grown);
        if (!grown) return element;   // registration is best-effort
        s_bindings = grown;
        s_capacity = next;
    }
    s_bindings[s_count].element = element;
    s_bindings[s_count].group   = group;
    memcpy(&s_bindings[s_count].h, handlers, size);
    s_count++;
    return element;
}

Element *Element_addMouseEvent(Element *element, MouseEvent handlers)       { return bind(element, GRP_MOUSE,    &handlers, sizeof handlers); }
Element *Element_addScrollEvent(Element *element, ScrollEvent handlers)     { return bind(element, GRP_SCROLL,   &handlers, sizeof handlers); }
Element *Element_addZoomEvent(Element *element, ZoomEvent handlers)         { return bind(element, GRP_ZOOM,     &handlers, sizeof handlers); }
Element *Element_addKeyEvent(Element *element, KeyEvent handlers)           { return bind(element, GRP_KEY,      &handlers, sizeof handlers); }
Element *Element_addTouchEvent(Element *element, TouchEvent handlers)       { return bind(element, GRP_TOUCH,    &handlers, sizeof handlers); }
Element *Element_addDocumentEvent(Element *element, DocumentEvent handlers) { return bind(element, GRP_DOCUMENT, &handlers, sizeof handlers); }

static const Binding *find(const Element *element, int group) {
    for (int i = 0; i < s_count; i++) {
        if (s_bindings[i].element == element && s_bindings[i].group == group) {
            return &s_bindings[i];
        }
    }
    return NULL;
}

static int group_of(int kind) {
    switch (kind) {
        case EV_MOUSE_DOWN: case EV_MOUSE_UP: case EV_MOUSE_MOVE:
        case EV_MOUSE_DRAG: case EV_MOUSE_ENTER: case EV_MOUSE_LEAVE:
            return GRP_MOUSE;
        case EV_SCROLL:            return GRP_SCROLL;
        case EV_ZOOM:              return GRP_ZOOM;
        case EV_KEY_DOWN: case EV_KEY_UP:
            return GRP_KEY;
        case EV_TOUCH_DOWN: case EV_TOUCH_MOVE: case EV_TOUCH_UP:
            return GRP_TOUCH;
        case EV_DOCUMENT_CHANGED: case EV_DOCUMENT_SELECTED:
            return GRP_DOCUMENT;
        default: return -1;
    }
}

// Run the one handler a binding has for this kind; true if it ran.
static bool invoke(const Binding *b, const Event *event) {
    const Binding binding = *b;
    const Event ev = *event;
    Element *element = binding.element;
    switch (binding.group) {
        case GRP_MOUSE: {
            const Mouse mouse = { ev.x, ev.y, ev.key };
            const MouseEvent *h = &binding.h.mouse;
            void (*fn)(Element *, const Mouse *, void *) = NULL;
            switch (ev.kind) {
                case EV_MOUSE_DOWN:  fn = (*h).onDown;  break;
                case EV_MOUSE_UP:    fn = (*h).onUp;    break;
                case EV_MOUSE_MOVE:  fn = (*h).onMove;  break;
                case EV_MOUSE_DRAG:  fn = (*h).onDrag;  break;
                case EV_MOUSE_ENTER: fn = (*h).onEnter; break;
                case EV_MOUSE_LEAVE: fn = (*h).onLeave; break;
            }
            if (fn) { fn(element, &mouse, (*h).userdata); return true; }
        } break;
        case GRP_SCROLL: {
            const Scroll scroll = { ev.x, ev.y, ev.dx, ev.dy };
            const ScrollEvent *h = &binding.h.scroll;
            if ((*h).onScroll) { (*h).onScroll(element, &scroll, (*h).userdata); return true; }
        } break;
        case GRP_ZOOM: {
            const Zoom zoom = { ev.x, ev.y, ev.magnitude };
            const ZoomEvent *h = &binding.h.zoom;
            if ((*h).onZoom) { (*h).onZoom(element, &zoom, (*h).userdata); return true; }
        } break;
        case GRP_KEY: {
            const Key key = { ev.key };
            const KeyEvent *h = &binding.h.key;
            void (*fn)(Element *, const Key *, void *) = NULL;
            if (ev.kind == EV_KEY_DOWN) fn = (*h).onDown;
            else if (ev.kind == EV_KEY_UP) fn = (*h).onUp;
            if (fn) { fn(element, &key, (*h).userdata); return true; }
        } break;
        case GRP_TOUCH: {
            const Touch touch = { ev.x, ev.y, ev.key };
            const TouchEvent *h = &binding.h.touch;
            void (*fn)(Element *, const Touch *, void *) = NULL;
            if (ev.kind == EV_TOUCH_DOWN) fn = (*h).onDown;
            else if (ev.kind == EV_TOUCH_MOVE) fn = (*h).onMove;
            else if (ev.kind == EV_TOUCH_UP) fn = (*h).onUp;
            if (fn) { fn(element, &touch, (*h).userdata); return true; }
        } break;
        case GRP_DOCUMENT: {
            const Document document = { NULL, 0u, 0u };
            const DocumentEvent *h = &binding.h.document;
            void (*fn)(Element *, const Document *, void *) = NULL;
            if (ev.kind == EV_DOCUMENT_CHANGED) fn = (*h).onChanged;
            else if (ev.kind == EV_DOCUMENT_SELECTED) fn = (*h).onSelected;
            if (fn) { fn(element, &document, (*h).userdata); return true; }
        } break;
    }
    return false;
}

bool Element_dispatchEvent(Element *root, const Event *event) {
    if (!root || !event) return false;
    const Event ev = *event;
    int group = group_of(ev.kind);
    if (group < 0) return false;

    // KEY/TOUCH/DOCUMENT are routed to the root (no pointer to hit-test); the
    // pointer kinds walk the deep element and bubble its ancestors.
    bool pointer = group == GRP_MOUSE || group == GRP_SCROLL || group == GRP_ZOOM;
    Element *start = pointer ? Element_hit(root, ev.x, ev.y) : root;
    if (!start) return false;

    bool ran = false;
    for (Element *element = start; element; element = Element_parent(element)) {
        const Binding *b = find(element, group);
        if (b && invoke(b, event)) ran = true;
    }
    return ran;
}

static bool under(Element *element, Element *root) {
    for (; element; element = Element_parent(element)) {
        if (element == root) return true;
    }
    return false;
}

void Element_clearEvents(Element *root) {
    if (!root) return;
    for (int i = 0; i < s_count;) {
        if (under(s_bindings[i].element, root)) {
            s_bindings[i] = s_bindings[--s_count];
        } else {
            i++;
        }
    }
}

int Element_eventBindingCount(void) {
    return s_count;
}
