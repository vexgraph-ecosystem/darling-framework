#include "event/focus.h"

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
 * DEFINITION: FocusEvent
 * ============================================================================
 * Transient focus message for keyboard focus gain/loss: the target panel,
 * the panel on the opposite side of the move, and the gained/lost direction.
 * Not a node and never attachable — central wiring delivers it through the
 * Panel tree, where consume() short-circuits the bubble walk.
 *
 * Arena-allocated through Memory_alloc with the TYPE_FOCUS_EVENT_SINGLETON
 * type id; target and opposite are borrowed Panel references, never owned.
 * nanos is a plain settable timestamp (clock wiring is behavior phase).
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: FocusEvent (plain struct, no Panel base)
 * LEVEL: L2 — Behavior (transient focus message)
 * ============================================================================
 * Transient focus message for keyboard focus gain/loss: target, the panel
 * on the opposite side of the move, and direction. Not a node, never
 * attachable; central wiring delivers it through the Panel tree.
 *
 * STRUCT FIELDS (Mirroring event/focus.h):
 * ----------------------------------------------------------------------------
 *   Panel *target;      // Panel gaining or losing focus; nullptr = none yet
 *   Panel *opposite;    // Panel on the other side of the move; nullptr = none
 *   bool gained;        // True = gained focus, false = lost focus
 *   bool consumed;      // True = stop the bubble walk
 *   uint64_t nanos;     // Plain settable timestamp (clock wiring is behavior phase)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - FocusEvent_0(void)
 *   - FocusEvent_2(target, gained)
 *
 * Core Functions:
 *   - FocusEvent_consume(ev)
 *
 * Setters:
 *   - FocusEvent_setTarget(ev, target)
 *   - FocusEvent_setOpposite(ev, opposite)
 *   - FocusEvent_setGained(ev, gained)
 *   - FocusEvent_setNanos(ev, nanos)
 *
 * Getters:
 *   - FocusEvent_getTarget(ev)
 *   - FocusEvent_getOpposite(ev)
 *   - FocusEvent_isGained(ev)
 *   - FocusEvent_getNanos(ev)
 *   - FocusEvent_isConsumed(ev)
 * ============================================================================
 */

// CONSTRUCTORS
// ============================================================================

FocusEvent *FocusEvent_0(void) {
    FocusEvent *ev = (FocusEvent*) Memory_alloc(TYPE_FOCUS_EVENT_SINGLETON, sizeof(FocusEvent));
    if (!ev)
        return nullptr;
    (*ev).target = nullptr;
    (*ev).opposite = nullptr;
    (*ev).gained = false;
    (*ev).consumed = false;
    (*ev).nanos = 0u;
    return ev;
}

FocusEvent *FocusEvent_2(Panel *target, bool gained) {
    FocusEvent *ev = FocusEvent_0();
    if (ev) {
        (*ev).target = target;
        (*ev).gained = gained;
    }
    return ev;
}

// CORE FUNCTIONS
// ============================================================================

void FocusEvent_consume(FocusEvent *ev) {
    if (!ev)
        return;
    (*ev).consumed = true;
}

// SETTERS
// ============================================================================

void FocusEvent_setTarget(FocusEvent *ev, Panel *target) {
    if (!ev)
        return;
    (*ev).target = target;
}

void FocusEvent_setOpposite(FocusEvent *ev, Panel *opposite) {
    if (!ev)
        return;
    (*ev).opposite = opposite;
}

void FocusEvent_setGained(FocusEvent *ev, bool gained) {
    if (!ev)
        return;
    (*ev).gained = gained;
}

void FocusEvent_setNanos(FocusEvent *ev, uint64_t nanos) {
    if (!ev)
        return;
    (*ev).nanos = nanos;
}

// GETTERS
// ============================================================================

Panel *FocusEvent_getTarget(const FocusEvent *ev) {
    return ev ? (*ev).target : nullptr;
}

Panel *FocusEvent_getOpposite(const FocusEvent *ev) {
    return ev ? (*ev).opposite : nullptr;
}

bool FocusEvent_isGained(const FocusEvent *ev) {
    return ev ? (*ev).gained : false;
}

uint64_t FocusEvent_getNanos(const FocusEvent *ev) {
    return ev ? (*ev).nanos : 0u;
}

bool FocusEvent_isConsumed(const FocusEvent *ev) {
    return ev ? (*ev).consumed : false;
}
