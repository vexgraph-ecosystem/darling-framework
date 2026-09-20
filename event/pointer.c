#include "event/pointer.h"

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
 * DEFINITION: PointerEvent
 * ============================================================================
 * Transient pointer message for mouse/touch/stylus input: the hit target
 * panel, position in target-local points, button, normalized pressure, and
 * kind (DOWN/MOVE/UP/DRAG/HOVER/ENTER/LEAVE/CANCEL). Not a node and never
 * attachable — central wiring delivers it through the Panel tree, where
 * consume() short-circuits the bubble walk.
 *
 * The phase field (0 capture, 1 target, 2 bubble) records where in the
 * delivery walk the event currently sits. Arena-allocated through
 * Memory_alloc with the TYPE_POINTER_EVENT_SINGLETON type id; target and
 * related are borrowed Panel references. nanos is a plain settable timestamp
 * (clock wiring is behavior phase).
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: PointerEvent (plain struct, no Panel base)
 * LEVEL: L2 — Behavior (transient pointer message)
 * ============================================================================
 * Transient pointer message for mouse/touch/stylus input: hit target,
 * position, button, pressure, and kind. Not a node, never attachable;
 * central wiring delivers it through the Panel tree.
 *
 * STRUCT FIELDS (Mirroring event/pointer.h):
 * ----------------------------------------------------------------------------
 *   Panel *target;      // Hit panel under the pointer; nullptr = none yet
 *   Panel *related;     // Secondary panel (enter/leave pair); nullptr = none
 *   float x;            // Pointer x in target-local points
 *   float y;            // Pointer y in target-local points
 *   int32_t button;     // 0 none, 1 primary, 2 secondary, 3 middle
 *   float pressure;     // Normalized 0.0 to 1.0; 0.0 = unknown
 *   int32_t kind;       // PTR_DOWN/MOVE/UP/DRAG/HOVER/ENTER/LEAVE/CANCEL
 *   uint64_t nanos;     // Plain settable timestamp (clock wiring is behavior phase)
 *   int32_t phase;      // 0 capture, 1 target, 2 bubble
 *   bool consumed;      // True = stop the bubble walk
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - PointerEvent_0(void)
 *   - PointerEvent_4(kind, x, y, button)
 *
 * Core Functions:
 *   - PointerEvent_consume(ev)
 *
 * Setters:
 *   - PointerEvent_setTarget(ev, target)
 *   - PointerEvent_setRelated(ev, related)
 *   - PointerEvent_setX(ev, x)
 *   - PointerEvent_setY(ev, y)
 *   - PointerEvent_setButton(ev, button)
 *   - PointerEvent_setPressure(ev, pressure)
 *   - PointerEvent_setKind(ev, kind)
 *   - PointerEvent_setNanos(ev, nanos)
 *   - PointerEvent_setPhase(ev, phase)
 *
 * Getters:
 *   - PointerEvent_getTarget(ev)
 *   - PointerEvent_getRelated(ev)
 *   - PointerEvent_getX(ev)
 *   - PointerEvent_getY(ev)
 *   - PointerEvent_getButton(ev)
 *   - PointerEvent_getPressure(ev)
 *   - PointerEvent_getKind(ev)
 *   - PointerEvent_getNanos(ev)
 *   - PointerEvent_getPhase(ev)
 *   - PointerEvent_isConsumed(ev)
 * ============================================================================
 */

// CONSTRUCTORS
// ============================================================================

PointerEvent *PointerEvent_0(void) {
    PointerEvent *ev = (PointerEvent*) Memory_alloc(TYPE_POINTER_EVENT_SINGLETON, sizeof(PointerEvent));
    if (!ev)
        return nullptr;
    (*ev).target = nullptr;
    (*ev).related = nullptr;
    (*ev).x = 0.0f;
    (*ev).y = 0.0f;
    (*ev).button = 0;
    (*ev).pressure = 0.0f;
    (*ev).kind = PTR_DOWN;
    (*ev).nanos = 0u;
    (*ev).phase = 1;
    (*ev).consumed = false;
    return ev;
}

PointerEvent *PointerEvent_4(int32_t kind, float x, float y, int32_t button) {
    PointerEvent *ev = PointerEvent_0();
    if (ev) {
        (*ev).kind = kind;
        (*ev).x = x;
        (*ev).y = y;
        (*ev).button = button;
    }
    return ev;
}

// CORE FUNCTIONS
// ============================================================================

void PointerEvent_consume(PointerEvent *ev) {
    if (!ev)
        return;
    (*ev).consumed = true;
}

// SETTERS
// ============================================================================

void PointerEvent_setTarget(PointerEvent *ev, Panel *target) {
    if (!ev)
        return;
    (*ev).target = target;
}

void PointerEvent_setRelated(PointerEvent *ev, Panel *related) {
    if (!ev)
        return;
    (*ev).related = related;
}

void PointerEvent_setX(PointerEvent *ev, float x) {
    if (!ev)
        return;
    (*ev).x = x;
}

void PointerEvent_setY(PointerEvent *ev, float y) {
    if (!ev)
        return;
    (*ev).y = y;
}

void PointerEvent_setButton(PointerEvent *ev, int32_t button) {
    if (!ev)
        return;
    (*ev).button = button;
}

void PointerEvent_setPressure(PointerEvent *ev, float pressure) {
    if (!ev)
        return;
    (*ev).pressure = pressure;
}

void PointerEvent_setKind(PointerEvent *ev, int32_t kind) {
    if (!ev)
        return;
    (*ev).kind = kind;
}

void PointerEvent_setNanos(PointerEvent *ev, uint64_t nanos) {
    if (!ev)
        return;
    (*ev).nanos = nanos;
}

void PointerEvent_setPhase(PointerEvent *ev, int32_t phase) {
    if (!ev)
        return;
    (*ev).phase = phase;
}

// GETTERS
// ============================================================================

Panel *PointerEvent_getTarget(const PointerEvent *ev) {
    return ev ? (*ev).target : nullptr;
}

Panel *PointerEvent_getRelated(const PointerEvent *ev) {
    return ev ? (*ev).related : nullptr;
}

float PointerEvent_getX(const PointerEvent *ev) {
    return ev ? (*ev).x : 0.0f;
}

float PointerEvent_getY(const PointerEvent *ev) {
    return ev ? (*ev).y : 0.0f;
}

int32_t PointerEvent_getButton(const PointerEvent *ev) {
    return ev ? (*ev).button : 0;
}

float PointerEvent_getPressure(const PointerEvent *ev) {
    return ev ? (*ev).pressure : 0.0f;
}

int32_t PointerEvent_getKind(const PointerEvent *ev) {
    return ev ? (*ev).kind : PTR_DOWN;
}

uint64_t PointerEvent_getNanos(const PointerEvent *ev) {
    return ev ? (*ev).nanos : 0u;
}

int32_t PointerEvent_getPhase(const PointerEvent *ev) {
    return ev ? (*ev).phase : 1;
}

bool PointerEvent_isConsumed(const PointerEvent *ev) {
    return ev ? (*ev).consumed : false;
}
