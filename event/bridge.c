#include "event/bridge.h"

#include "event/dispatch.h"
#include "event/keyevent.h"
#include "event/pointer.h"
#include "darling/panel/panel.h"
#include "event/keyhandler.h"
#include "event/mousehandler.h"
#include "input/key.h"
#include "input/mouse.h"
#include "nio/mem.h"
#include "time/nanotime.h"
#include "annotation/definition.h"
#include "annotation/overview.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: EventBridge
 * ============================================================================
 * Procedural handoff module (no struct, no type id) forming the third part
 * of the input path: vexspoke freezes capture-time timestamps at push, and
 * EventBridge translates each raw Key/Mouse listener callback into a darling
 * UI event, firing it synchronously into the Panel tree. Down/up carry
 * exactNanos end to end so the press moment registers; move/drag are stamped
 * at delivery since motion is never judgment-critical.
 *
 * Focus is explicit only — the app sets the key target via
 * Darling_bridgeSetFocused(Window) — with auto-focus on click deferred. File
 * statics hold the bindings: a fixed per-window BridgeSlot table (windowId
 * 1..7) plus legacy global root/focus singletons. Char composition,
 * scroll/zoom/delta, and touch gestures are deferred.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: EventBridge (procedural handoff, no struct, no type id)
 * LEVEL: L2 — Behavior (raw input translation into UI events)
 * ============================================================================
 * The third part of the input path: vexspoke freezes capture-time
 * timestamps at push (slot.pressTime + packed epoch micros); this module
 * translates each raw listener callback into a darling UI event and
 * fires it synchronously. Down/up carry exactNanos end to end, so the
 * press moment is what registers. Move/drag are stamped at delivery
 * (documented: motion is never judgment-critical).
 *
 * FOCUS MODEL: explicit only. The app sets the key target with
 * Darling_bridgeSetFocused; auto-focus on click lands later.
 *
 * DEFERRED: char composition, scroll/zoom/delta, touch gestures.
 *
 * STRUCT FIELDS: none — procedural handoff (operates on Panel tree +
 * event structs). File statics below hold the bindings: s_slots (per-window
 * bridge mapping for windowId 1..7), s_root / s_focused (legacy global).
 *
 * PRIVATE HELPERS:
 * ----------------------------------------------------------------------------
 *   BridgeSlot:
 *     uint32_t windowId;           // bound window id (1..7)
 *     Panel *root;                 // tree root for pointer delivery
 *     Panel *focused;              // key target for UIKeyEvent
 *     KeyHandler keyListener;      // window-scoped key vtable
 *     MouseHandler mouseListener;  // window-scoped mouse vtable
 *     bool active;                 // slot in-use flag
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - Darling_bridgeAttachWindow(windowId, root)
 *   - Darling_bridgeDetachWindow(windowId)
 *   - Darling_bridgeAttach(root)
 *   - Darling_bridgeDetach()
 *
 * Setters:
 *   - Darling_bridgeSetFocusedWindow(windowId, p)
 *   - Darling_bridgeSetFocused(p)
 *   - Darling_bridgeSetWindow(window)   // OS window for cursor seams
 *
 * Getters:
 *   - Darling_bridgeGetFocusedWindow(windowId)
 *   - Darling_bridgeGetRootWindow(windowId)
 *   - Darling_bridgeGetFocused()
 *   - Darling_bridgeGetRoot()
 *   - Darling_bridgeGetWindow()
 * ============================================================================
 */

#define DARLING_MAX_WINDOW_BRIDGES 8

typedef struct BridgeSlot {
    uint32_t windowId;
    Panel *root;
    Panel *focused;
    KeyHandler keyListener;
    MouseHandler mouseListener;
    bool active;
} BridgeSlot;

static BridgeSlot s_slots[DARLING_MAX_WINDOW_BRIDGES];

// Legacy global singletons
static Panel *s_root = nullptr;
static Panel *s_focused = nullptr;
static bool s_attached = false;
static void *s_window = nullptr;
static KeyHandler s_keyListener;
static MouseHandler s_mouseListener;

static inline Panel *resolveFocused(void *self) {
    if (self) {
        BridgeSlot *slot = (BridgeSlot*) self;
        return (*slot).focused;
    }
    return s_focused;
}

static inline Panel *resolveRoot(void *self) {
    if (self) {
        BridgeSlot *slot = (BridgeSlot*) self;
        return (*slot).root;
    }
    return s_root;
}

// --- raw key callbacks (exactNanos = capture-frozen, passthrough) -----------

static void bridgeKeyDown(void *self, int keyEvent, uint64_t exactNanos) {
    Panel *f = resolveFocused(self);
    if (!f)
        return;
    UIKeyEvent *ev = UIKeyEvent_0();
    if (!ev)
        return;
    uint32_t mods = 0u;
    if (keyEvent & KEY_MOD_SHIFT)
        mods |= 1u;
    if (keyEvent & KEY_MOD_CONTROL)
        mods |= 2u;
    if (keyEvent & KEY_MOD_OPTION)
        mods |= 4u;
    if (keyEvent & KEY_MOD_COMMAND)
        mods |= 8u;
    UIKeyEvent_setTarget(ev, f);
    UIKeyEvent_setKeyCode(ev, keyEvent & KEY_MASK_CODE);
    UIKeyEvent_setCh(ev, -1);
    UIKeyEvent_setMods(ev, mods);
    UIKeyEvent_setPressed(ev, true);
    UIKeyEvent_setRepeat(ev, false);
    UIKeyEvent_setNanos(ev, exactNanos);
    Darling_fireKey(f, ev);
    Memory_free(ev);
}

static void bridgeKeyUp(void *self, int keyEvent, uint64_t exactNanos) {
    Panel *f = resolveFocused(self);
    if (!f)
        return;
    UIKeyEvent *ev = UIKeyEvent_0();
    if (!ev)
        return;
    uint32_t mods = 0u;
    if (keyEvent & KEY_MOD_SHIFT)
        mods |= 1u;
    if (keyEvent & KEY_MOD_CONTROL)
        mods |= 2u;
    if (keyEvent & KEY_MOD_OPTION)
        mods |= 4u;
    if (keyEvent & KEY_MOD_COMMAND)
        mods |= 8u;
    UIKeyEvent_setTarget(ev, f);
    UIKeyEvent_setKeyCode(ev, keyEvent & KEY_MASK_CODE);
    UIKeyEvent_setCh(ev, -1);
    UIKeyEvent_setMods(ev, mods);
    UIKeyEvent_setPressed(ev, false);
    UIKeyEvent_setRepeat(ev, false);
    UIKeyEvent_setNanos(ev, exactNanos);
    Darling_fireKey(f, ev);
    Memory_free(ev);
}

static void bridgeKeyRepeat(void *self, int keyEvent, uint64_t exactNanos) {
    Panel *f = resolveFocused(self);
    if (!f)
        return;
    UIKeyEvent *ev = UIKeyEvent_0();
    if (!ev)
        return;
    uint32_t mods = 0u;
    if (keyEvent & KEY_MOD_SHIFT)
        mods |= 1u;
    if (keyEvent & KEY_MOD_CONTROL)
        mods |= 2u;
    if (keyEvent & KEY_MOD_OPTION)
        mods |= 4u;
    if (keyEvent & KEY_MOD_COMMAND)
        mods |= 8u;
    UIKeyEvent_setTarget(ev, f);
    UIKeyEvent_setKeyCode(ev, keyEvent & KEY_MASK_CODE);
    UIKeyEvent_setCh(ev, -1);
    UIKeyEvent_setMods(ev, mods);
    UIKeyEvent_setPressed(ev, true);
    UIKeyEvent_setRepeat(ev, true);
    UIKeyEvent_setNanos(ev, exactNanos);
    Darling_fireKey(f, ev);
    Memory_free(ev);
}

// --- raw mouse callbacks -----------------------------------------------------

static void bridgeMouseDown(void *self, int mouseEvent, uint64_t exactNanos) {
    Panel *root = resolveRoot(self);
    if (!root)
        return;
    int button = Mouse_button(mouseEvent);
    double x = Mouse_x();
    double y = Mouse_y();
    PointerEvent *ev = PointerEvent_4(PTR_DOWN, (float)x, (float)y, button);
    if (!ev)
        return;
    PointerEvent_setNanos(ev, exactNanos);
    Darling_firePointer(root, ev);
    Memory_free(ev);
}

static void bridgeMouseUp(void *self, int mouseEvent, uint64_t exactNanos) {
    Panel *root = resolveRoot(self);
    if (!root)
        return;
    int button = Mouse_button(mouseEvent);
    double x = Mouse_x();
    double y = Mouse_y();
    PointerEvent *ev = PointerEvent_4(PTR_UP, (float)x, (float)y, button);
    if (!ev)
        return;
    PointerEvent_setNanos(ev, exactNanos);
    Darling_firePointer(root, ev);
    Memory_free(ev);
}

static void bridgeMouseMove(void *self, double x, double y) {
    Panel *root = resolveRoot(self);
    if (!root)
        return;
    PointerEvent *ev = PointerEvent_4(PTR_MOVE, (float)x, (float)y, 0);
    if (!ev)
        return;
    PointerEvent_setNanos(ev, NanoTime_now());
    Darling_firePointer(root, ev);
    Memory_free(ev);
}

static void bridgeMouseDrag(void *self, int button, double x, double y) {
    Panel *root = resolveRoot(self);
    if (!root)
        return;
    PointerEvent *ev = PointerEvent_4(PTR_DRAG, (float)x, (float)y, button);
    if (!ev)
        return;
    PointerEvent_setNanos(ev, NanoTime_now());
    Darling_firePointer(root, ev);
    Memory_free(ev);
}

// CORE FUNCTIONS
// ============================================================================

void Darling_bridgeAttachWindow(uint32_t windowId, Panel *root) {
    if (windowId < 1 || windowId >= DARLING_MAX_WINDOW_BRIDGES)
        return;
    BridgeSlot *slot = &s_slots[windowId];
    (*slot).windowId = windowId;
    (*slot).root = root;
    if ((*slot).active)
        return;

    KeyHandler *kh = &(*slot).keyListener;
    (*kh).self = slot;
    (*kh).onKeyDown = bridgeKeyDown;
    (*kh).onKeyUp = bridgeKeyUp;
    (*kh).onKeyRepeat = bridgeKeyRepeat;
    (*kh).onCharTyped = nullptr;

    MouseHandler *mh = &(*slot).mouseListener;
    (*mh).self = slot;
    (*mh).onMouseDown = bridgeMouseDown;
    (*mh).onMouseUp = bridgeMouseUp;
    (*mh).onMouseRepeat = nullptr;
    (*mh).onMouseMove = bridgeMouseMove;
    (*mh).onMouseMoveDelta = nullptr;
    (*mh).onMouseDrag = bridgeMouseDrag;
    (*mh).onMouseScroll = nullptr;
    (*mh).onMouseZoom = nullptr;

    Key_attachWindow(windowId, kh);
    Mouse_attachWindow(windowId, mh);
    (*slot).active = true;
}

void Darling_bridgeDetachWindow(uint32_t windowId) {
    if (windowId < 1 || windowId >= DARLING_MAX_WINDOW_BRIDGES)
        return;
    BridgeSlot *slot = &s_slots[windowId];
    if (!(*slot).active)
        return;
    KeyHandler *kh = &(*slot).keyListener;
    MouseHandler *mh = &(*slot).mouseListener;
    (void) Key_detachWindow(windowId, kh);
    (void) Mouse_detachWindow(windowId, mh);
    (*slot).active = false;
    (*slot).root = nullptr;
    (*slot).focused = nullptr;
    (*slot).windowId = 0;
}

void Darling_bridgeAttach(Panel *root) {
    s_root = root;
    if (s_attached)
        return;
    s_keyListener.self = nullptr;
    s_keyListener.onKeyDown = bridgeKeyDown;
    s_keyListener.onKeyUp = bridgeKeyUp;
    s_keyListener.onKeyRepeat = bridgeKeyRepeat;
    s_keyListener.onCharTyped = nullptr;
    s_mouseListener.self = nullptr;
    s_mouseListener.onMouseDown = bridgeMouseDown;
    s_mouseListener.onMouseUp = bridgeMouseUp;
    s_mouseListener.onMouseRepeat = nullptr;
    s_mouseListener.onMouseMove = bridgeMouseMove;
    s_mouseListener.onMouseMoveDelta = nullptr;
    s_mouseListener.onMouseDrag = bridgeMouseDrag;
    s_mouseListener.onMouseScroll = nullptr;
    s_mouseListener.onMouseZoom = nullptr;
    Key_addListener(&s_keyListener);
    Mouse_addListener(&s_mouseListener);
    s_attached = true;
}

void Darling_bridgeDetach(void) {
    if (!s_attached)
        return;
    (void) Key_removeListener(&s_keyListener);
    (void) Mouse_removeListener(&s_mouseListener);
    s_attached = false;
    s_root = nullptr;
    s_focused = nullptr;
}

// SETTERS
// ============================================================================

void Darling_bridgeSetFocusedWindow(uint32_t windowId, Panel *p) {
    if (windowId < 1 || windowId >= DARLING_MAX_WINDOW_BRIDGES)
        return;
    BridgeSlot *slot = &s_slots[windowId];
    (*slot).focused = p;
}

void Darling_bridgeSetFocused(Panel *p) {
    s_focused = p;
}

// OS window registered for cursor/pointer work (I-beam caret, etc.). Every
// handlePointer seam reads it via Darling_bridgeGetWindow for its cursor
// lifecycle; the gallery registers its native window once at startup.
void Darling_bridgeSetWindow(void *window) {
    s_window = window;
}

// GETTERS
// ============================================================================

Panel *Darling_bridgeGetFocusedWindow(uint32_t windowId) {
    if (windowId < 1 || windowId >= DARLING_MAX_WINDOW_BRIDGES)
        return nullptr;
    BridgeSlot *slot = &s_slots[windowId];
    return (*slot).focused;
}

Panel *Darling_bridgeGetRootWindow(uint32_t windowId) {
    if (windowId < 1 || windowId >= DARLING_MAX_WINDOW_BRIDGES)
        return nullptr;
    BridgeSlot *slot = &s_slots[windowId];
    return (*slot).root;
}

Panel *Darling_bridgeGetFocused(void) {
    return s_focused;
}

void *Darling_bridgeGetWindow(void) {
    return s_window;
}

Panel *Darling_bridgeGetRoot(void) {
    return s_root;
}
