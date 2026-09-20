#include "event/value.h"

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
 * DEFINITION: ValueEvent
 * ============================================================================
 * Transient value-change message for slider drags, checkbox toggles, and
 * text edits: the source panel, a value slot tag, and old/new payloads in
 * both integer and double form. Not a node and never attachable — central
 * wiring delivers it through the Panel tree, where consume() short-circuits
 * the bubble walk.
 *
 * Arena-allocated through Memory_alloc with the TYPE_VALUE_EVENT_SINGLETON
 * type id; the source is a borrowed Panel reference, never owned. nanos is a
 * plain settable timestamp (clock wiring is behavior phase).
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: ValueEvent (plain struct, no Panel base)
 * LEVEL: L2 — Behavior (transient value-change message)
 * ============================================================================
 * Transient value-change message for slider drags, toggles, and text edits:
 * source panel, slot tag, and old/new integer and float payloads. Not a
 * node, never attachable; central wiring delivers it through the Panel tree.
 *
 * STRUCT FIELDS (Mirroring event/value.h):
 * ----------------------------------------------------------------------------
 *   Panel *source;      // Panel whose value changed; nullptr = none yet
 *   int32_t tag;        // Value slot tag
 *   int64_t oldV;       // Previous integer value
 *   int64_t newV;       // Current integer value
 *   double oldD;        // Previous float value
 *   double newD;        // Current float value
 *   bool consumed;      // True = stop the bubble walk
 *   uint64_t nanos;     // Plain settable timestamp (clock wiring is behavior phase)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - ValueEvent_0(void)
 *   - ValueEvent_2(source, tag)
 *
 * Core Functions:
 *   - ValueEvent_consume(ev)
 *
 * Setters:
 *   - ValueEvent_setSource(ev, source)
 *   - ValueEvent_setTag(ev, tag)
 *   - ValueEvent_setOldV(ev, oldV)
 *   - ValueEvent_setNewV(ev, newV)
 *   - ValueEvent_setOldD(ev, oldD)
 *   - ValueEvent_setNewD(ev, newD)
 *   - ValueEvent_setNanos(ev, nanos)
 *
 * Getters:
 *   - ValueEvent_getSource(ev)
 *   - ValueEvent_getTag(ev)
 *   - ValueEvent_getOldV(ev)
 *   - ValueEvent_getNewV(ev)
 *   - ValueEvent_getOldD(ev)
 *   - ValueEvent_getNewD(ev)
 *   - ValueEvent_getNanos(ev)
 *   - ValueEvent_isConsumed(ev)
 * ============================================================================
 */

// CONSTRUCTORS
// ============================================================================

ValueEvent *ValueEvent_0(void) {
    ValueEvent *ev = (ValueEvent*) Memory_alloc(TYPE_VALUE_EVENT_SINGLETON, sizeof(ValueEvent));
    if (!ev)
        return nullptr;
    (*ev).source = nullptr;
    (*ev).tag = 0;
    (*ev).oldV = 0;
    (*ev).newV = 0;
    (*ev).oldD = 0.0;
    (*ev).newD = 0.0;
    (*ev).consumed = false;
    (*ev).nanos = 0u;
    return ev;
}

ValueEvent *ValueEvent_2(Panel *source, int32_t tag) {
    ValueEvent *ev = ValueEvent_0();
    if (ev) {
        (*ev).source = source;
        (*ev).tag = tag;
    }
    return ev;
}

// CORE FUNCTIONS
// ============================================================================

void ValueEvent_consume(ValueEvent *ev) {
    if (!ev)
        return;
    (*ev).consumed = true;
}

// SETTERS
// ============================================================================

void ValueEvent_setSource(ValueEvent *ev, Panel *source) {
    if (!ev)
        return;
    (*ev).source = source;
}

void ValueEvent_setTag(ValueEvent *ev, int32_t tag) {
    if (!ev)
        return;
    (*ev).tag = tag;
}

void ValueEvent_setOldV(ValueEvent *ev, int64_t oldV) {
    if (!ev)
        return;
    (*ev).oldV = oldV;
}

void ValueEvent_setNewV(ValueEvent *ev, int64_t newV) {
    if (!ev)
        return;
    (*ev).newV = newV;
}

void ValueEvent_setOldD(ValueEvent *ev, double oldD) {
    if (!ev)
        return;
    (*ev).oldD = oldD;
}

void ValueEvent_setNewD(ValueEvent *ev, double newD) {
    if (!ev)
        return;
    (*ev).newD = newD;
}

void ValueEvent_setNanos(ValueEvent *ev, uint64_t nanos) {
    if (!ev)
        return;
    (*ev).nanos = nanos;
}

// GETTERS
// ============================================================================

Panel *ValueEvent_getSource(const ValueEvent *ev) {
    return ev ? (*ev).source : nullptr;
}

int32_t ValueEvent_getTag(const ValueEvent *ev) {
    return ev ? (*ev).tag : 0;
}

int64_t ValueEvent_getOldV(const ValueEvent *ev) {
    return ev ? (*ev).oldV : 0;
}

int64_t ValueEvent_getNewV(const ValueEvent *ev) {
    return ev ? (*ev).newV : 0;
}

double ValueEvent_getOldD(const ValueEvent *ev) {
    return ev ? (*ev).oldD : 0.0;
}

double ValueEvent_getNewD(const ValueEvent *ev) {
    return ev ? (*ev).newD : 0.0;
}

uint64_t ValueEvent_getNanos(const ValueEvent *ev) {
    return ev ? (*ev).nanos : 0u;
}

bool ValueEvent_isConsumed(const ValueEvent *ev) {
    return ev ? (*ev).consumed : false;
}
