#include "event/gesture.h"

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
 * DEFINITION: GestureEvent
 * ============================================================================
 * Transient gesture message for tap, double-tap, long-press, pinch, and
 * swipe: the target panel, gesture centroid in target-local points, pinch
 * scale, rotation in radians, and active touch count. Not a node and never
 * attachable — central wiring delivers it through the Panel tree, where
 * consume() short-circuits the bubble walk.
 *
 * Arena-allocated through Memory_alloc with the TYPE_GESTURE_EVENT_SINGLETON
 * type id; the target is a borrowed Panel reference, never owned. nanos is a
 * plain settable timestamp (clock wiring is behavior phase).
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: GestureEvent (plain struct, no Panel base)
 * LEVEL: L2 — Behavior (transient gesture message)
 * ============================================================================
 * Transient gesture message for tap, double-tap, long-press, pinch, and
 * swipe: target, centroid, pinch scale, rotation, and touch count. Not a
 * node, never attachable; central wiring delivers it through the Panel tree.
 *
 * STRUCT FIELDS (Mirroring event/gesture.h):
 * ----------------------------------------------------------------------------
 *   Panel *target;      // Panel under the gesture; nullptr = none yet
 *   int32_t kind;       // GESTURE_TAP/DOUBLE/LONGPRESS/PINCH/SWIPE
 *   float x;            // Gesture centroid x in target-local points
 *   float y;            // Gesture centroid y in target-local points
 *   float scale;        // Pinch scale factor; 1.0 = identity
 *   float rotation;     // Rotation in radians; 0.0 = none
 *   int32_t touches;    // Active touch count
 *   bool consumed;      // True = stop the bubble walk
 *   uint64_t nanos;     // Plain settable timestamp (clock wiring is behavior phase)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - GestureEvent_0(void)
 *   - GestureEvent_2(kind, touches)
 *
 * Core Functions:
 *   - GestureEvent_consume(ev)
 *
 * Setters:
 *   - GestureEvent_setTarget(ev, target)
 *   - GestureEvent_setKind(ev, kind)
 *   - GestureEvent_setX(ev, x)
 *   - GestureEvent_setY(ev, y)
 *   - GestureEvent_setScale(ev, scale)
 *   - GestureEvent_setRotation(ev, rotation)
 *   - GestureEvent_setTouches(ev, touches)
 *   - GestureEvent_setNanos(ev, nanos)
 *
 * Getters:
 *   - GestureEvent_getTarget(ev)
 *   - GestureEvent_getKind(ev)
 *   - GestureEvent_getX(ev)
 *   - GestureEvent_getY(ev)
 *   - GestureEvent_getScale(ev)
 *   - GestureEvent_getRotation(ev)
 *   - GestureEvent_getTouches(ev)
 *   - GestureEvent_getNanos(ev)
 *   - GestureEvent_isConsumed(ev)
 * ============================================================================
 */

// CONSTRUCTORS
// ============================================================================

GestureEvent *GestureEvent_0(void) {
    GestureEvent *ev = (GestureEvent*) Memory_alloc(TYPE_GESTURE_EVENT_SINGLETON, sizeof(GestureEvent));
    if (!ev)
        return nullptr;
    (*ev).target = nullptr;
    (*ev).kind = GESTURE_TAP;
    (*ev).x = 0.0f;
    (*ev).y = 0.0f;
    (*ev).scale = 1.0f;
    (*ev).rotation = 0.0f;
    (*ev).touches = 0;
    (*ev).consumed = false;
    (*ev).nanos = 0u;
    return ev;
}

GestureEvent *GestureEvent_2(int32_t kind, int32_t touches) {
    GestureEvent *ev = GestureEvent_0();
    if (ev) {
        (*ev).kind = kind;
        (*ev).touches = touches;
    }
    return ev;
}

// CORE FUNCTIONS
// ============================================================================

void GestureEvent_consume(GestureEvent *ev) {
    if (!ev)
        return;
    (*ev).consumed = true;
}

// SETTERS
// ============================================================================

void GestureEvent_setTarget(GestureEvent *ev, Panel *target) {
    if (!ev)
        return;
    (*ev).target = target;
}

void GestureEvent_setKind(GestureEvent *ev, int32_t kind) {
    if (!ev)
        return;
    (*ev).kind = kind;
}

void GestureEvent_setX(GestureEvent *ev, float x) {
    if (!ev)
        return;
    (*ev).x = x;
}

void GestureEvent_setY(GestureEvent *ev, float y) {
    if (!ev)
        return;
    (*ev).y = y;
}

void GestureEvent_setScale(GestureEvent *ev, float scale) {
    if (!ev)
        return;
    (*ev).scale = scale;
}

void GestureEvent_setRotation(GestureEvent *ev, float rotation) {
    if (!ev)
        return;
    (*ev).rotation = rotation;
}

void GestureEvent_setTouches(GestureEvent *ev, int32_t touches) {
    if (!ev)
        return;
    (*ev).touches = touches;
}

void GestureEvent_setNanos(GestureEvent *ev, uint64_t nanos) {
    if (!ev)
        return;
    (*ev).nanos = nanos;
}

// GETTERS
// ============================================================================

Panel *GestureEvent_getTarget(const GestureEvent *ev) {
    return ev ? (*ev).target : nullptr;
}

int32_t GestureEvent_getKind(const GestureEvent *ev) {
    return ev ? (*ev).kind : GESTURE_TAP;
}

float GestureEvent_getX(const GestureEvent *ev) {
    return ev ? (*ev).x : 0.0f;
}

float GestureEvent_getY(const GestureEvent *ev) {
    return ev ? (*ev).y : 0.0f;
}

float GestureEvent_getScale(const GestureEvent *ev) {
    return ev ? (*ev).scale : 1.0f;
}

float GestureEvent_getRotation(const GestureEvent *ev) {
    return ev ? (*ev).rotation : 0.0f;
}

int32_t GestureEvent_getTouches(const GestureEvent *ev) {
    return ev ? (*ev).touches : 0;
}

uint64_t GestureEvent_getNanos(const GestureEvent *ev) {
    return ev ? (*ev).nanos : 0u;
}

bool GestureEvent_isConsumed(const GestureEvent *ev) {
    return ev ? (*ev).consumed : false;
}
