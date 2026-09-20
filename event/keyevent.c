#include "event/keyevent.h"

#include "darling/panel/panel.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include "nio/mem.h"
#include "oop/type.h"

#include <stdbool.h>
#include <stdint.h>

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: UIKeyEvent
 * ============================================================================
 * Transient key message in the darling UI domain: platform key code, decoded
 * codepoint, modifier bitmask, and press/repeat flags. Not a node and never
 * attachable — central wiring delivers it through the Panel tree, where
 * consume() short-circuits the bubble walk.
 *
 * The split from vexspoke's raw KeyHandler vtable is deliberate: hardware
 * mechanism lives in vexspoke (polling, slots, dispatch), GUI-domain event
 * data lives here as UIKeyEvent. Arena-allocated through Memory_alloc with
 * the TYPE_KEY_EVENT_SINGLETON type id; the target is a borrowed Panel
 * reference. nanos is a plain settable timestamp (clock wiring is behavior
 * phase).
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: UIKeyEvent (plain struct, no Panel base)
 * LEVEL: L2 — Behavior (transient key message)
 * ============================================================================
 * Transient key message in the darling UI domain: key code, decoded char,
 * modifiers, and press/repeat flags. Not a node, never attachable;
 * central wiring delivers it through the Panel tree.
 *
 * STRUCT FIELDS (Mirroring event/keyevent.h):
 * ----------------------------------------------------------------------------
 *   Panel *target;      // Focused panel receiving the key; nullptr = none yet
 *   int32_t keyCode;    // Platform key code
 *   int32_t ch;         // Decoded codepoint; -1 = none
 *   uint32_t mods;      // Modifier bitmask
 *   bool pressed;       // True = press, false = release
 *   bool repeat;        // True = auto-repeat
 *   bool consumed;      // True = stop the bubble walk
 *   uint64_t nanos;     // Plain settable timestamp (clock wiring is behavior phase)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - UIKeyEvent_0(void)
 *   - UIKeyEvent_3(keyCode, pressed, repeat)
 *
 * Core Functions:
 *   - UIKeyEvent_consume(ev)
 *
 * Setters:
 *   - UIKeyEvent_setTarget(ev, target)
 *   - UIKeyEvent_setKeyCode(ev, keyCode)
 *   - UIKeyEvent_setCh(ev, ch)
 *   - UIKeyEvent_setMods(ev, mods)
 *   - UIKeyEvent_setPressed(ev, pressed)
 *   - UIKeyEvent_setRepeat(ev, repeat)
 *   - UIKeyEvent_setNanos(ev, nanos)
 *
 * Getters:
 *   - UIKeyEvent_getTarget(ev)
 *   - UIKeyEvent_getKeyCode(ev)
 *   - UIKeyEvent_getCh(ev)
 *   - UIKeyEvent_getMods(ev)
 *   - UIKeyEvent_isPressed(ev)
 *   - UIKeyEvent_isRepeat(ev)
 *   - UIKeyEvent_getNanos(ev)
 *   - UIKeyEvent_isConsumed(ev)
 * ============================================================================
 */

// CONSTRUCTORS
// ============================================================================

UIKeyEvent *UIKeyEvent_0(void) {
    UIKeyEvent *ev = (UIKeyEvent*) Memory_alloc(TYPE_KEY_EVENT_SINGLETON, sizeof(UIKeyEvent));
    if (!ev)
        return nullptr;
    (*ev).target = nullptr;
    (*ev).keyCode = 0;
    (*ev).ch = -1;
    (*ev).mods = 0u;
    (*ev).pressed = false;
    (*ev).repeat = false;
    (*ev).consumed = false;
    (*ev).nanos = 0u;
    return ev;
}

UIKeyEvent *UIKeyEvent_3(int32_t keyCode, bool pressed, bool repeat) {
    UIKeyEvent *ev = UIKeyEvent_0();
    if (ev) {
        (*ev).keyCode = keyCode;
        (*ev).pressed = pressed;
        (*ev).repeat = repeat;
    }
    return ev;
}

// CORE FUNCTIONS
// ============================================================================

void UIKeyEvent_consume(UIKeyEvent *ev) {
    if (!ev)
        return;
    (*ev).consumed = true;
}

// SETTERS
// ============================================================================

void UIKeyEvent_setTarget(UIKeyEvent *ev, Panel *target) {
    if (!ev)
        return;
    (*ev).target = target;
}

void UIKeyEvent_setKeyCode(UIKeyEvent *ev, int32_t keyCode) {
    if (!ev)
        return;
    (*ev).keyCode = keyCode;
}

void UIKeyEvent_setCh(UIKeyEvent *ev, int32_t ch) {
    if (!ev)
        return;
    (*ev).ch = ch;
}

void UIKeyEvent_setMods(UIKeyEvent *ev, uint32_t mods) {
    if (!ev)
        return;
    (*ev).mods = mods;
}

void UIKeyEvent_setPressed(UIKeyEvent *ev, bool pressed) {
    if (!ev)
        return;
    (*ev).pressed = pressed;
}

void UIKeyEvent_setRepeat(UIKeyEvent *ev, bool repeat) {
    if (!ev)
        return;
    (*ev).repeat = repeat;
}

void UIKeyEvent_setNanos(UIKeyEvent *ev, uint64_t nanos) {
    if (!ev)
        return;
    (*ev).nanos = nanos;
}

// GETTERS
// ============================================================================

Panel *UIKeyEvent_getTarget(const UIKeyEvent *ev) {
    return ev ? (*ev).target : nullptr;
}

int32_t UIKeyEvent_getKeyCode(const UIKeyEvent *ev) {
    return ev ? (*ev).keyCode : 0;
}

int32_t UIKeyEvent_getCh(const UIKeyEvent *ev) {
    return ev ? (*ev).ch : -1;
}

uint32_t UIKeyEvent_getMods(const UIKeyEvent *ev) {
    return ev ? (*ev).mods : 0u;
}

bool UIKeyEvent_isPressed(const UIKeyEvent *ev) {
    return ev ? (*ev).pressed : false;
}

bool UIKeyEvent_isRepeat(const UIKeyEvent *ev) {
    return ev ? (*ev).repeat : false;
}

uint64_t UIKeyEvent_getNanos(const UIKeyEvent *ev) {
    return ev ? (*ev).nanos : 0u;
}

bool UIKeyEvent_isConsumed(const UIKeyEvent *ev) {
    return ev ? (*ev).consumed : false;
}
