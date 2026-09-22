#include "darling/field/scrollbar.h"

#include "annotation/definition.h"
#include "annotation/overview.h"
#include "annotation/getter.h"
#include "annotation/setter.h"
#include "event/pointer.h"
#include "lang/str.h"
#include "nio/mem.h"
#include "oop/type.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: ScrollBar
 * ============================================================================
 * Track+thumb scroller with two switchable thumb laws: gesture mode maps drag
 * deltas to value deltas scaled by track length (position-independent,
 * eyes-free); point mode jumps the value to the exact clicked fraction. An
 * orientation selects the axis (vertical maps localY, horizontal maps localX),
 * so one class serves both ScrollPanel bars. Value-mapping math lives in pure
 * static helpers over scalars so it stays testable without a window; no
 * input-ring wiring yet. The struct is arena-allocated with zero owned heap
 * and no callbacks. A leaf R4 field widget with symmetric getters/setters.
 *
 * Geometry is rewritten onto the graphvex language (the Absolute Size and
 * Location Law): the bar owns an AUTHORITATIVE GraphicsComponent track —
 * docked via anchor (right edge for vertical, bottom edge for horizontal),
 * eager absolute on every setter — while the Panel base carries R4 widget
 * identity (pointer dispatch, color, hierarchy slot). The thumb rect derives
 * on demand from the track abs + value + viewport/content lengths, so there is
 * no cached thumb state to drift during a live resize (BUG-008): the bar
 * re-derives its edge every step from the same abs the compositor paints.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: ScrollBar (embeds Panel + GraphicsComponent)
 * LEVEL: L2 — Behavior (track+thumb scroller behavior API)
 * ============================================================================
 * SUMMARY:
 *   R4 widget identity (Panel base) + authoritative track geometry
 *   (GraphicsComponent) + value state. Gesture/point thumb laws over pure
 *   scalar helpers; thumb rect derived on demand from track abs.
 *
 * STRUCT FIELDS (Mirroring darling/field/scrollbar.h):
 * ----------------------------------------------------------------------------
 *   Panel base;                // R4 identity (pointer, color, hierarchy slot)
 *   GraphicsComponent track;   // AUTHORITATIVE track geometry (graphvex)
 *   int mode;                  // SCROLL_BAR_GESTURE or SCROLL_BAR_POINT
 *   int orientation;           // SCROLL_BAR_VERTICAL or SCROLL_BAR_HORIZONTAL
 *   float min;                 // Lower track bound
 *   float max;                 // Upper track bound
 *   float value;               // Thumb value, clamped to min/max
 *   float thumbMin;            // Minimum thumb extent in px
 *   float thickness;           // Cross-axis extent (bar width / bar height)
 *   float inset;               // Edge inset from the docked corner
 *
 * PRIVATE HELPERS:
 * ----------------------------------------------------------------------------
 *   pinValue(value, lo, hi) / pin01(t) / gestureDelta(dPx, trackLen, span)
 *   pointLerp(lo, hi, fraction) / trackLen(s, horizontal)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Public Constructors: (.h)
 *   - ScrollBar_0(void)
 *   - ScrollBar_1(mode)
 *   - ScrollBar_2(mode, orientation)
 *
 * Private Constructors: (.c static)
 *   - (none)
 *
 * Public Core Functions: (.h)
 *   - ScrollBar_dragBy(s, deltaPx, trackLen)
 *   - ScrollBar_clickAt(s, fraction)
 *   - ScrollBar_setRange(s, min, max)
 *   - ScrollBar_handlePointer(s, kind, localX, localY)
 *   - ScrollBar_thumbRect(s, viewportLen, contentLen, outX, outY, outW, outH)
 *
 * Private Core Functions: (.c static)
 *   - pinValue / pin01 / gestureDelta / pointLerp / trackLen
 *
 * Public Setters: (.h)
 *   - ScrollBar_setMode(s, mode)
 *   - ScrollBar_setOrientation(s, orientation)
 *   - ScrollBar_setValue(s, value)
 *   - ScrollBar_setThumbMin(s, px)
 *   - ScrollBar_setThickness(s, px)
 *   - ScrollBar_setInset(s, px)
 *
 * Private Setters: (.c static)
 *   - (none)
 *
 * Public Getters: (.h)
 *   - ScrollBar_getMode(s)
 *   - ScrollBar_getOrientation(s)
 *   - ScrollBar_getValue(s)
 *   - ScrollBar_getThumbMin(s)
 *   - ScrollBar_getThickness(s)
 *   - ScrollBar_getInset(s)
 *   - ScrollBar_getRange(s, outMin, outMax)
 *
 * Private Getters: (.c static)
 *   - (none)
 *
 * Public toString: (.h)
 *   - ScrollBar_toString / ScrollBar_toStringStruct
 * ============================================================================
 */

// CONSTRUCTORS (PUBLIC & PRIVATE)

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
    GraphicsComponent_init(&(*s).track);
    (*s).mode = SCROLL_BAR_GESTURE;
    (*s).orientation = SCROLL_BAR_VERTICAL;
    (*s).min = 0.0f;
    (*s).max = 1.0f;
    (*s).value = 0.0f;
    (*s).thumbMin = 24.0f;
    (*s).thickness = 12.0f;
    (*s).inset = 2.0f;
    return s;
}

ScrollBar *ScrollBar_1(int mode) {
    ScrollBar *s = ScrollBar_0();
    if (s)
        ScrollBar_setMode(s, mode);
    return s;
}

ScrollBar *ScrollBar_2(int mode, int orientation) {
    ScrollBar *s = ScrollBar_1(mode);
    if (s)
        ScrollBar_setOrientation(s, orientation);
    return s;
}

// CORE FUNCTIONS (PUBLIC & PRIVATE)

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

// The oriented track length in absolute px: the track abs when resolved,
// else the Panel base component dim (donor fallback), else 100.
static float trackLen(const ScrollBar *s, bool horizontal) {
    float tx, ty, tw, th;
    GraphicsComponent_getAbsRect(&(*s).track, &tx, &ty, &tw, &th);
    (void) tx;
    (void) ty;
    float len = horizontal ? tw : th;
    if (len > 0.0f)
        return len;
    Panel *p = (Panel*) &(*s).base;
    Component *cnt = &(*p).component;
    float fallback = horizontal ? (*cnt).w : (*cnt).h;
    return fallback > 0.0f ? fallback : 100.0f;
}

void ScrollBar_dragBy(ScrollBar *s, float deltaPx, float trackLenPx) {
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
    float next = (*s).value + gestureDelta(deltaPx, trackLenPx, span);
    (*s).value = pinValue(next, lo, hi);
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
}

void ScrollBar_handlePointer(ScrollBar *s, int kind, float localX, float localY) {
    if (!s)
        return;
    if (kind != PTR_DOWN && kind != PTR_DRAG)
        return;
    bool horizontal = (*s).orientation == SCROLL_BAR_HORIZONTAL;
    float len = trackLen(s, horizontal);
    float pos = horizontal ? localX : localY;
    ScrollBar_clickAt(s, len > 0.0f ? pos / len : 0.0f);
}

void ScrollBar_thumbRect(const ScrollBar *s, float viewportLen, float contentLen,
                         float *outX, float *outY, float *outW, float *outH) {
    float tx = 0.0f, ty = 0.0f, tw = 0.0f, th = 0.0f;
    float lo = 0.0f, hi = 1.0f, value = 0.0f, thumbMin = 0.0f;
    bool horizontal = false;
    if (s) {
        GraphicsComponent_getAbsRect(&(*s).track, &tx, &ty, &tw, &th);
        lo = (*s).min;
        hi = (*s).max;
        value = (*s).value;
        thumbMin = (*s).thumbMin;
        horizontal = (*s).orientation == SCROLL_BAR_HORIZONTAL;
    }
    if (lo > hi) {
        float tmp = lo;
        lo = hi;
        hi = tmp;
    }
    float span = hi - lo;
    float frac = span > 0.0f ? (value - lo) / span : 0.0f;
    frac = pin01(frac);
    float ratio = contentLen > 0.0f ? viewportLen / contentLen : 1.0f;
    ratio = pin01(ratio);
    if (horizontal) {
        float thumbW = ratio * tw;
        if (thumbW < thumbMin)
            thumbW = thumbMin;
        if (thumbW > tw)
            thumbW = tw;
        float thumbX = tx + frac * (tw - thumbW);
        if (outX) *outX = thumbX;
        if (outY) *outY = ty;
        if (outW) *outW = thumbW;
        if (outH) *outH = th;
    } else {
        float thumbH = ratio * th;
        if (thumbH < thumbMin)
            thumbH = thumbMin;
        if (thumbH > th)
            thumbH = th;
        float thumbY = ty + frac * (th - thumbH);
        if (outX) *outX = tx;
        if (outY) *outY = thumbY;
        if (outW) *outW = tw;
        if (outH) *outH = thumbH;
    }
}

// SETTERS (PUBLIC & PRIVATE)

;;SETTER
void ScrollBar_setMode(ScrollBar *s, int mode) {
    if (!s)
        return;
    if (mode != SCROLL_BAR_GESTURE && mode != SCROLL_BAR_POINT)
        return;
    (*s).mode = mode;
}

;;SETTER
void ScrollBar_setOrientation(ScrollBar *s, int orientation) {
    if (!s)
        return;
    if (orientation != SCROLL_BAR_VERTICAL && orientation != SCROLL_BAR_HORIZONTAL)
        return;
    (*s).orientation = orientation;
}

;;SETTER
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
}

;;SETTER
void ScrollBar_setThumbMin(ScrollBar *s, float px) {
    if (!s)
        return;
    if (px < 0.0f)
        px = 0.0f;
    (*s).thumbMin = px;
}

;;SETTER
void ScrollBar_setThickness(ScrollBar *s, float px) {
    if (!s)
        return;
    if (px < 0.0f)
        px = 0.0f;
    (*s).thickness = px;
}

;;SETTER
void ScrollBar_setInset(ScrollBar *s, float px) {
    if (!s)
        return;
    if (px < 0.0f)
        px = 0.0f;
    (*s).inset = px;
}

// GETTERS (PUBLIC & PRIVATE)

;;GETTER
int ScrollBar_getMode(const ScrollBar *s) {
    return s ? (*s).mode : SCROLL_BAR_GESTURE;
}

;;GETTER
int ScrollBar_getOrientation(const ScrollBar *s) {
    return s ? (*s).orientation : SCROLL_BAR_VERTICAL;
}

;;GETTER
float ScrollBar_getValue(const ScrollBar *s) {
    return s ? (*s).value : 0.0f;
}

;;GETTER
float ScrollBar_getThumbMin(const ScrollBar *s) {
    return s ? (*s).thumbMin : 0.0f;
}

;;GETTER
float ScrollBar_getThickness(const ScrollBar *s) {
    return s ? (*s).thickness : 0.0f;
}

;;GETTER
float ScrollBar_getInset(const ScrollBar *s) {
    return s ? (*s).inset : 0.0f;
}

;;GETTER
void ScrollBar_getRange(const ScrollBar *s, float *outMin, float *outMax) {
    float lo = 0.0f;
    float hi = 1.0f;
    if (s) {
        lo = (*s).min;
        hi = (*s).max;
    }
    if (outMin)
        *outMin = lo;
    if (outMax)
        *outMax = hi;
}

// TOSTRING (PUBLIC)

void ScrollBar_toString(const ScrollBar *self, char *dest, size_t cap, bool *outTruncated) {
    Str s;
    Str_init(&s, dest, cap);
    if (self == nullptr) {
        Str_put(&s, "nullptr");
        if (outTruncated) *outTruncated = false;
        return;
    }
    Str_printf(&s, "ScrollBar[%s %s value=%.2f [%.2f, %.2f] thickness=%.1f inset=%.1f]",
               (*self).orientation == SCROLL_BAR_HORIZONTAL ? "horizontal" : "vertical",
               (*self).mode == SCROLL_BAR_POINT ? "point" : "gesture",
               (*self).value, (*self).min, (*self).max,
               (*self).thickness, (*self).inset);
    if (outTruncated) *outTruncated = Str_isTruncated(&s);
}

void ScrollBar_toStringStruct(const ScrollBar *self, char *dest, size_t cap, bool *outTruncated) {
    Str s;
    Str_init(&s, dest, cap);
    if (self == nullptr) {
        Str_put(&s, "nullptr");
        if (outTruncated) *outTruncated = false;
        return;
    }
    // ONE layer: the nested track renders via its own toString.
    char track[192];
    bool trackTruncated = false;
    GraphicsComponent_toString(&(*self).track, track, sizeof(track), &trackTruncated);
    Str_put(&s, "ScrollBar { ");
    Str_printf(&s, "mode: %d, orientation: %d, ", (*self).mode, (*self).orientation);
    Str_printf(&s, "min: %.2f, max: %.2f, value: %.2f, ", (*self).min, (*self).max, (*self).value);
    Str_printf(&s, "thumbMin: %.1f, thickness: %.1f, inset: %.1f, ", (*self).thumbMin, (*self).thickness, (*self).inset);
    Str_put(&s, "track: ");
    Str_put(&s, track);
    Str_put(&s, " }");
    if (outTruncated) *outTruncated = Str_isTruncated(&s) || trackTruncated;
}
