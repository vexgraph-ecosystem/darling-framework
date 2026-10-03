#include "input/pointer.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "input/mouse.h"   // vexspoke R2: window-scoped events + Mouse_x/y
#include "c23/event_invoke.h"   // Element_dispatchEvent + the Event carrier
#include "properties/set_cursor.h"

#include "annotation/definition.h"
#include "annotation/overview.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Pointer (input/pointer.c)
 * ============================================================================
 * The pointer bridge between the OS and the element tree. Handlers bind to
 * Panels (the wrappers are the interactables); a Cursor holds the per-root
 * press state so a press/release pair resolves against the same tree.
 *
 * Two paths feed the SAME tree: the synthetic actions (Pointer_press/release/
 * click/hold/drag) and the optional real-mouse bridge (Pointer_track). The OS
 * bridge installs a vexspoke MouseHandler on the Frame's window; every event is
 * BOTH the legacy click binding and a per-element event dispatched through
 * c23/event_invoke (down/up/move/drag/scroll/zoom). Registrations are owned by
 * this file and dropped on Frame close via the close hook, so frame.c never
 * learns about input.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: Pointer (input/pointer.c)
 * ============================================================================
 * Pointer actions + optional OS mouse bridge. File-static registries, no owned
 * class instance.
 *
 * FILE-STATIC STATE:
 * ----------------------------------------------------------------------------
 *   Cursor  **s_cursors;   // one per root Element (press state + tracked bridge)
 *   int       s_curCount, s_curCap;
 *   Binding  *s_bindings;  // Panel click bindings keyed by the panel's Element
 *   int       s_bindCount, s_bindCap;
 *
 * PRIVATE TYPES:
 * ----------------------------------------------------------------------------
 *   Binding : { Element *graphics; Panel *panel; PointerFn fn; void *userdata; }
 *   Cursor  : { Element *root; Element *pressed; Frame *frame;
 *               MouseHandler handler; bool tracked; }
 *
 * PRIVATE HELPERS:
 * ----------------------------------------------------------------------------
 *   find_binding / cursor_for / cursor_open : registry lookups
 *   bound_at(root,x,y) : nearest bound element under the cursor
 *   track_dispatch / track_down|up|move|mod|dragf|scroll|zoom : OS bridge
 *   track_cursor : apply the hit node's inherited hover preference
 *   cleanup_on_close   : drop registrations when the owning Frame closes
 *
 * FUNCTION REGISTRY (exported by input/pointer.h):
 * ----------------------------------------------------------------------------
 * Actions:
 *   - Pointer_hover, Pointer_press, Pointer_release, Pointer_move,
 *     Pointer_click, Pointer_hold, Pointer_drag
 * Handlers:
 *   - Pointer_onClick, Pointer_addButton
 * OS bridge:
 *   - Pointer_track
 * ============================================================================
 */

typedef struct {
    Element *graphics;   // the panel's GraphicsPanel (the hit target)
    Panel *panel;        // the interactable wrapper (handed back on click)
    PointerFn fn;
    void *userdata;
} Binding;

typedef struct {
    Element *root;       // the top element (keyed here)
    Element *pressed;    // the bound element under the button
    Frame *frame;        // only when tracked (OS bridge)
    MouseHandler handler;
    bool tracked;
} Cursor;

static Cursor **s_cursors = NULL;
static int s_curCount = 0, s_curCap = 0;

static Binding *s_bindings = NULL;
static int s_bindCount = 0, s_bindCap = 0;

static Binding *find_binding(Element *graphics) {
    for (int i = 0; i < s_bindCount; i++)
        if (s_bindings[i].graphics == graphics) return &s_bindings[i];
    return NULL;
}

static Cursor *cursor_for(Element *root) {
    for (int i = 0; i < s_curCount; i++)
        if ((*s_cursors[i]).root == root) return s_cursors[i];
    return NULL;
}

static Cursor *cursor_open(Element *root) {
    Cursor *c = cursor_for(root);
    if (c) return c;
    c = calloc(1, sizeof *c);
    if (!c) return NULL;
    (*c).root = root;
    if (s_curCount == s_curCap) {
        s_curCap = s_curCap ? s_curCap * 2 : 4;
        s_cursors = realloc(s_cursors, (size_t)s_curCap * sizeof *s_cursors);
    }
    s_cursors[s_curCount++] = c;
    return c;
}

// the nearest BOUND element under the cursor (so a child of a button hits it)
static Binding *bound_at(Element *root, float x, float y) {
    Element *e = Element_hit(root, x, y);
    while (e && !find_binding(e)) e = Element_parent(e);
    return e ? find_binding(e) : NULL;
}

static void sleep_seconds(double s) {
    if (s <= 0.0) return;
    struct timespec ts;
    ts.tv_sec = (time_t)s;
    ts.tv_nsec = (long)((s - (double)ts.tv_sec) * 1e9);
    while (nanosleep(&ts, &ts) == -1) { /* resume on signal */ }
}

// ── actions ─────────────────────────────────────────────────────────────────
Panel *Pointer_hover(Element *root, float x, float y) {
    if (!root) return NULL;
    Binding *b = bound_at(Element_root(root), x, y);
    return b ? (*b).panel : NULL;
}

void Pointer_press(Element *root, float x, float y) {
    if (!root) return;
    root = Element_root(root);
    Cursor *c = cursor_open(root);
    if (!c) return;
    Binding *b = bound_at(root, x, y);
    (*c).pressed = b ? (*b).graphics : NULL;
    if ((*c).pressed) Element_setPressed((*c).pressed, true);
    Frame_invalidateElement(root);
}

void Pointer_release(Element *root, float x, float y) {
    if (!root) return;
    root = Element_root(root);
    Cursor *c = cursor_open(root);
    if (!c) return;
    Binding *b = bound_at(root, x, y);
    if (b && (*b).graphics == (*c).pressed && (*b).fn) (*b).fn((*b).panel, (*b).userdata);
    if ((*c).pressed) Element_setPressed((*c).pressed, false);
    (*c).pressed = NULL;
    Frame_invalidateElement(root);
}

void Pointer_move(Element *root, float x, float y) {
    (void)x; (void)y;
    if (root) Frame_invalidateElement(Element_root(root));
}

void Pointer_click(Element *root, float x, float y) {
    Pointer_press(root, x, y);
    Pointer_release(root, x, y);
}

void Pointer_hold(Element *root, float x, float y, double seconds) {
    Pointer_press(root, x, y);
    sleep_seconds(seconds);
    Pointer_release(root, x, y);
}

void Pointer_drag(Element *root, float x, float y, float toX, float toY, double seconds) {
    if (!root) return;
    const int steps = 30;
    Pointer_press(root, x, y);
    for (int i = 1; i <= steps; i++) {
        float t = (float)i / (float)steps;
        sleep_seconds(seconds / steps);
        Pointer_move(root, x + (toX - x) * t, y + (toY - y) * t);
    }
    Pointer_release(root, toX, toY);
}

// ── handlers ────────────────────────────────────────────────────────────────
void Pointer_onClick(Panel *panel, PointerFn fn, void *userdata) {
    Element *graphics = Panel_graphics(panel);
    if (!panel || !graphics) return;
    Binding *b = find_binding(graphics);
    if (b) { (*b).panel = panel; (*b).fn = fn; (*b).userdata = userdata; return; }
    if (s_bindCount == s_bindCap) {
        s_bindCap = s_bindCap ? s_bindCap * 2 : 16;
        s_bindings = realloc(s_bindings, (size_t)s_bindCap * sizeof *s_bindings);
    }
    s_bindings[s_bindCount++] = (Binding){graphics, panel, fn, userdata};
}

Panel *Pointer_addButton(Frame *frame, const ElementDesc *desc, PointerFn fn, void *userdata) {
    Panel *p = Frame_addPanel(frame, desc);
    if (p) Pointer_onClick(p, fn, userdata);
    return p;
}

// ── optional OS bridge (feeds the actions above AND the event dispatcher) ───
// Every OS event is BOTH the legacy pointer action (click bindings) and a
// per-element event (Element_add<Kind>Event handlers), dispatched against the
// cursor's root and bubbled.
static void track_dispatch(Cursor *c, int kind, float x, float y,
                           float dx, float dy, float magnitude, int key) {
    Event event = { .kind = kind, .x = x, .y = y, .dx = dx, .dy = dy,
                    .magnitude = magnitude, .key = key };
    Element_dispatchEvent((*c).root, &event);
}

static void track_cursor(Cursor *c, float x, float y) {
    Window *window = Frame_window((*c).frame);
    if (!window) return;
    WindowCursorType type = (WindowCursorType) Element_cursorAt((*c).root, x, y);
    if (Window_getCursorType(window) != type) Window_setCursorType(window, type);
}

static void track_down(void *self, int mouseEvent, uint64_t nanos) {
    (void)nanos;
    Cursor *c = self;
    float x = (float)Mouse_x(), y = (float)Mouse_y();
    track_dispatch(c, EV_MOUSE_DOWN, x, y, 0, 0, 1.0f, Mouse_button(mouseEvent));
    Pointer_press((*c).root, x, y);
}
static void track_up(void *self, int mouseEvent, uint64_t nanos) {
    (void)nanos;
    Cursor *c = self;
    float x = (float)Mouse_x(), y = (float)Mouse_y();
    track_dispatch(c, EV_MOUSE_UP, x, y, 0, 0, 1.0f, Mouse_button(mouseEvent));
    Pointer_release((*c).root, x, y);
}
static void track_move(void *self, double x, double y) {
    Cursor *c = self;
    track_dispatch(c, EV_MOUSE_MOVE, (float)x, (float)y, 0, 0, 1.0f, 0);
    Pointer_move((*c).root, (float)x, (float)y);
    track_cursor(c, (float)x, (float)y);
}
static void track_none0(void *self, int ev, uint64_t t) { (void)self; (void)ev; (void)t; }
static void track_mod(void *self, double dx, double dy) {
    Cursor *c = self;
    track_dispatch(c, EV_MOUSE_MOVE, (float)Mouse_x(), (float)Mouse_y(),
                   (float)dx, (float)dy, 1.0f, 0);
}
static void track_dragf(void *self, int ev, double x, double y) {
    Cursor *c = self;
    track_dispatch(c, EV_MOUSE_DRAG, (float)x, (float)y, 0, 0, 1.0f, Mouse_button(ev));
    track_cursor(c, (float)x, (float)y);
}
static void track_scroll(void *self, double dx, double dy) {
    Cursor *c = self;
    track_dispatch(c, EV_SCROLL, (float)Mouse_x(), (float)Mouse_y(),
                   (float)dx, (float)dy, 1.0f, 0);
}
static void track_zoom(void *self, double magnification) {
    Cursor *c = self;
    track_dispatch(c, EV_ZOOM, (float)Mouse_x(), (float)Mouse_y(),
                   0, 0, (float)magnification, 0);
}

static void cleanup_on_close(Frame *frame, void *userdata) {
    (void)userdata;
    Element *root = Frame_element(frame);
    for (int i = 0; i < s_curCount; i++) {
        if ((*s_cursors[i]).root != root) continue;
        free(s_cursors[i]);
        memmove(&s_cursors[i], &s_cursors[i + 1],
                (size_t)(s_curCount - i - 1) * sizeof *s_cursors);
        s_curCount--;
        break;
    }
    for (int i = 0; i < s_bindCount; ) {
        Element *g = s_bindings[i].graphics;
        Element *r = g ? Element_root(g) : NULL;
        if (r != root) { i++; continue; }
        memmove(&s_bindings[i], &s_bindings[i + 1],
                (size_t)(s_bindCount - i - 1) * sizeof *s_bindings);
        s_bindCount--;
    }
}

void Pointer_track(Frame *frame) {
    if (!frame) return;
    Element *root = Frame_element(frame);
    Cursor *c = cursor_open(root);
    if (!c || (*c).tracked) return;
    (*c).tracked = true;
    (*c).frame = frame;
    (*c).handler.self = c;
    (*c).handler.onMouseDown = track_down;
    (*c).handler.onMouseUp = track_up;
    (*c).handler.onMouseMove = track_move;
    (*c).handler.onMouseMoveDelta = track_mod;
    (*c).handler.onMouseDrag = track_dragf;
    (*c).handler.onMouseScroll = track_scroll;
    (*c).handler.onMouseZoom = track_zoom;
    (*c).handler.onMouseRepeat = track_none0;
    Window_addMouseAdapter(Frame_window(frame), &(*c).handler);
    Frame_onClose(frame, cleanup_on_close, NULL);
}
