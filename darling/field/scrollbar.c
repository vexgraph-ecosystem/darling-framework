#include "darling/field/scrollbar.h"

#include "annotation/definition.h"
#include "annotation/overview.h"
#include "event/pointer.h"
#include "nio/mem.h"
#include "oop/type.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: ScrollBar
 * ============================================================================
 * Track+thumb scroller with two switchable thumb laws: gesture mode maps drag
 * deltas to value deltas scaled by track length (position-independent,
 * eyes-free); point mode jumps the value to the exact clicked fraction.
 * Value-mapping math lives in pure static helpers over scalars so it stays
 * testable without a window; no input-ring wiring yet. The struct is
 * arena-allocated with zero owned heap and no callbacks. A leaf R4 field
 * widget with symmetric getters/setters.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: ScrollBar (embeds Panel)
 * LEVEL: L2 — Behavior (track+thumb scroller behavior API)
 * ============================================================================
 * Track+thumb scroller with two switchable thumb laws: gesture mode maps
 * drag deltas to value deltas scaled by track length (position-independent,
 * eyes-free); point mode jumps the value to the exact clicked fraction.
 * Value-mapping math lives in pure static helpers over scalars so it stays
 * testable without a window; no input-ring wiring yet.
 *
 * STRUCT FIELDS (Mirroring darling/field/scrollbar.h):
 * ----------------------------------------------------------------------------
 *   Panel base;                  // Inherited layout, bounds, hierarchy state
 *   int mode;                    // SCROLL_BAR_GESTURE or SCROLL_BAR_POINT
 *   float min;                   // Lower track bound
 *   float max;                   // Upper track bound
 *   float value;                 // Thumb value, clamped to min/max
 *   float thumbMin;              // Minimum thumb extent in px
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - ScrollBar_0(void)
 *   - ScrollBar_1(mode)
 *
 * Core Functions:
 *   - ScrollBar_dragBy(s, deltaPx, trackLen)
 *   - ScrollBar_clickAt(s, fraction)
 *   - ScrollBar_setRange(s, min, max)
 *   - ScrollBar_handlePointer(s, kind, localX, localY)
 *
 * Setters:
 *   - ScrollBar_setMode(s, mode)
 *   - ScrollBar_setValue(s, value)
 *   - ScrollBar_setThumbMin(s, px)
 *
 * Getters:
 *   - ScrollBar_getMode(s)
 *   - ScrollBar_getValue(s)
 *   - ScrollBar_getThumbMin(s)
 *   - ScrollBar_getRange(s, outMin, outMax)
 * ============================================================================
 */

// CONSTRUCTORS

ScrollBar *ScrollBar_0(void) {
    ScrollBar *s = (ScrollBar*) Memory_alloc(TYPE_SCROLLBAR_SINGLETON, sizeof(ScrollBar));
    if (!s)
        return nullptr;
    Panel *base = Panel_0();
    if (!base) {
        Memory_free(s);
        return nullptr;
    }
    (*s).base = (*base);
    Memory_free(base);
    (*s).mode = SCROLL_BAR_GESTURE;
    (*s).min = 0.0f;
    (*s).max = 1.0f;
    (*s).value = 0.0f;
    (*s).thumbMin = 24.0f;
    return s;
}

ScrollBar *ScrollBar_1(int mode) {
    ScrollBar *s = ScrollBar_0();
    if (s)
        ScrollBar_setMode(s, mode);
    return s;
}

// CORE FUNCTIONS

static float pinValue(float value, float lo, float hi) {
    if (value < lo)
        return lo;
    if (value > hi)
        return hi;
    return value;
}

static float pin01(float t) {
    if (t < 0.0f)
        return 0.0f;
    if (t > 1.0f)
        return 1.0f;
    return t;
}

static float gestureDelta(float deltaPx, float trackLen, float span) {
    if (trackLen <= 0.0f)
        return 0.0f;
    return deltaPx / trackLen * span;
}

static float pointLerp(float lo, float hi, float fraction) {
    float t = pin01(fraction);
    return lo + t * (hi - lo);
}

static void markDirty(ScrollBar *s) {
    if (!s)
        return;
    Panel *b = &(*s).base;
    Container_markDirty(&(*b).base);
}

void ScrollBar_dragBy(ScrollBar *s, float deltaPx, float trackLen) {
    if (!s)
        return;
    float lo = (*s).min;
    float hi = (*s).max;
    if (lo > hi) {
        float tmp = lo;
        lo = hi;
        hi = tmp;
    }
    float span = hi - lo;
    float next = (*s).value + gestureDelta(deltaPx, trackLen, span);
    (*s).value = pinValue(next, lo, hi);
    markDirty(s);
}

void ScrollBar_clickAt(ScrollBar *s, float fraction) {
    if (!s)
        return;
    float lo = (*s).min;
    float hi = (*s).max;
    if (lo > hi) {
        float tmp = lo;
        lo = hi;
        hi = tmp;
    }
    (*s).value = pointLerp(lo, hi, fraction);
    markDirty(s);
}

void ScrollBar_setRange(ScrollBar *s, float min, float max) {
    if (!s)
        return;
    if (min > max) {
        float tmp = min;
        min = max;
        max = tmp;
    }
    (*s).min = min;
    (*s).max = max;
    (*s).value = pinValue((*s).value, min, max);
    markDirty(s);
}

void ScrollBar_handlePointer(ScrollBar *s, int kind, float localX, float localY) {
    (void) localX;
    if (!s) return;
    if (kind != PTR_DOWN && kind != PTR_DRAG) return;
    Panel *p = &(*s).base;
    Container *cnt = &(*p).base;
    float h = (*cnt).h > 0.0f ? (*cnt).h : 100.0f;
    float fraction = localY / h;
    ScrollBar_clickAt(s, fraction);
}

// SETTERS

void ScrollBar_setMode(ScrollBar *s, int mode) {
    if (!s)
        return;
    if (mode != SCROLL_BAR_GESTURE && mode != SCROLL_BAR_POINT)
        return;
    (*s).mode = mode;
    markDirty(s);
}

void ScrollBar_setValue(ScrollBar *s, float value) {
    if (!s)
        return;
    float lo = (*s).min;
    float hi = (*s).max;
    if (lo > hi) {
        float tmp = lo;
        lo = hi;
        hi = tmp;
    }
    (*s).value = pinValue(value, lo, hi);
    markDirty(s);
}

void ScrollBar_setThumbMin(ScrollBar *s, float px) {
    if (!s)
        return;
    if (px < 0.0f)
        px = 0.0f;
    (*s).thumbMin = px;
    markDirty(s);
}

// GETTERS

int ScrollBar_getMode(const ScrollBar *s) {
    return s ? (*s).mode : SCROLL_BAR_GESTURE;
}

float ScrollBar_getValue(const ScrollBar *s) {
    return s ? (*s).value : 0.0f;
}

float ScrollBar_getThumbMin(const ScrollBar *s) {
    return s ? (*s).thumbMin : 0.0f;
}

void ScrollBar_getRange(const ScrollBar *s, float *outMin, float *outMax) {
    float lo = 0.0f;
    float hi = 1.0f;
    if (s) {
        lo = (*s).min;
        hi = (*s).max;
    }
    if (outMin)
        (*outMin) = lo;
    if (outMax)
        (*outMax) = hi;
}
