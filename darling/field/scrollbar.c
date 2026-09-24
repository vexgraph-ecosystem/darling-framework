#include "darling/field/scrollbar.h"

#include "annotation/definition.h"
#include "annotation/overview.h"
#include "annotation/getter.h"
#include "annotation/setter.h"
#include "event/pointer.h"
#include "lang/str.h"
#include "nio/mem.h"
#include "oop/type.h"

#include <math.h>

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
 *   float shortLimit;          // Minimum thumb share of the track (0..1)
 *   bool hideWhenUnused;       // Overlay bar: hidden until scrolled
 *   float opacity;             // Bar opacity (0..1, pushed to the base)
 *   uint64_t idleTimeoutMs;    // Hide delay after the last scroll
 *   uint64_t lastScrollMs;     // Clock of the last noted scroll
 *   bool hasScrolled;          // True once any scroll was noted
 *   bool autoHidden;           // Auto-hide currently hiding the bar
 *   float viewLen;             // Viewport length along the bar (paint lens)
 *   float contentLen;          // Content length along the bar (paint lens)
 *   int scrollMode;            // SCROLL_BAR_STEP or SCROLL_BAR_SMOOTH
 *   float friction;            // 0 = no glide, 1 = default, >1 = longer
 *   float sensitivity;         // Input multiplier (1.0 = unchanged)
 *   uint64_t delayMs;          // Settle hold before glide begins
 *   float velocity;            // Remaining px carried by momentum
 *   uint64_t lastInputMs;      // Clock of the last input (momentum arming)
 *   bool dragging;             // Pointer grab is active
 *   float dragGrab;            // Px from the thumb start to the grab point
 *   float fadeAlpha;           // Overlay fade (1 = solid, 0 = faded out)
 *   uint64_t fadeOutMs;        // Fade-out duration after the idle hold
 *   bool grappable;            // False = the bar refuses pointer grabs
 *
 * PRIVATE HELPERS:
 * ----------------------------------------------------------------------------
 *   pinValue(value, lo, hi) / pin01(t) / gestureDelta(dPx, trackLen, span)
 *   pointLerp(lo, hi, fraction) / trackLen(s, horizontal)
 *   thumbDims(trackLen, viewportLen, contentLen, lo, hi, value,
 *     thumbMin, shortLimit, outPos, outLen)
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
 *   - ScrollBar_noteScroll(s, nowMs)
 *   - ScrollBar_tick(s, nowMs, scrollable)
 *   - ScrollBar_setLengths(s, viewportLen, contentLen)
 *   - ScrollBar_paint(s, trackRect)
 *   - ScrollBar_applyInput(s, deltaPx, nowMs)
 *   - ScrollBar_glideStep(s, nowMs, dtMs)
 *   - ScrollBar_beginDrag(s, trackLen, trackPos, thumbLen)
 *   - ScrollBar_dragTo(s, trackLen, trackPos, thumbLen)
 *   - ScrollBar_endDrag(s) / ScrollBar_isDragging(s)
 *
 * Private Core Functions: (.c static)
 *   - pinValue / pin01 / gestureDelta / pointLerp / trackLen / thumbDims /
 *     valueSpan
 *
 * Public Setters: (.h)
 *   - ScrollBar_setMode(s, mode)
 *   - ScrollBar_setOrientation(s, orientation)
 *   - ScrollBar_setValue(s, value)
 *   - ScrollBar_setThumbMin(s, px)
 *   - ScrollBar_setShortLengthLimit(s, percent)
 *   - ScrollBar_setHideWhenUnused(s, hide)
 *   - ScrollBar_setOpacity(s, opacity)
 *   - ScrollBar_setIdleTimeoutMs(s, timeoutMs)
 *   - ScrollBar_setFadeOutMs(s, fadeOutMs)
 *   - ScrollBar_setGrappable(s, grappable)
 *   - ScrollBar_setScrollMode(s, mode)
 *   - ScrollBar_setScrollFriction(s, friction)
 *   - ScrollBar_setScrollSensitivity(s, sensitivity)
 *   - ScrollBar_setScrollDelay(s, delayMs)
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
 *   - ScrollBar_getShortLengthLimit(s)
 *   - ScrollBar_isHideWhenUnused(s)
 *   - ScrollBar_getOpacity(s)
 *   - ScrollBar_getIdleTimeoutMs(s)
 *   - ScrollBar_getFadeOutMs(s) / ScrollBar_isGrappable(s) / ScrollBar_getFadeAlpha(s)
 *   - ScrollBar_getScrollMode(s)
 *   - ScrollBar_getScrollFriction(s)
 *   - ScrollBar_getScrollSensitivity(s)
 *   - ScrollBar_getScrollDelay(s)
 *   - ScrollBar_isAutoHidden(s)
 *   - ScrollBar_isEffectiveVisible(s)
 *   - ScrollBar_isNeeded(s)
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
    (*s).min = SCROLL_BAR_VALUE_MIN_DEFAULT;
    (*s).max = SCROLL_BAR_VALUE_MAX_DEFAULT;
    (*s).value = SCROLL_BAR_VALUE_MIN_DEFAULT;
    (*s).thumbMin = SCROLL_BAR_THUMB_MIN_DEFAULT;
    (*s).thickness = SCROLL_BAR_THICKNESS_DEFAULT;
    (*s).inset = SCROLL_BAR_INSET_DEFAULT;
    (*s).shortLimit = SCROLL_BAR_SHORT_LIMIT_DEFAULT;
    (*s).hideWhenUnused = false;
    (*s).opacity = SCROLL_BAR_OPACITY_DEFAULT;
    (*s).idleTimeoutMs = SCROLL_BAR_IDLE_MS_DEFAULT;
    (*s).lastScrollMs = 0u;
    (*s).hasScrolled = false;
    (*s).autoHidden = false;
    (*s).viewLen = 0.0f;
    (*s).contentLen = 0.0f;
    (*s).scrollMode = SCROLL_BAR_MODE_DEFAULT;
    (*s).friction = SCROLL_BAR_FRICTION_DEFAULT;
    (*s).sensitivity = SCROLL_BAR_SENSITIVITY_DEFAULT;
    (*s).delayMs = SCROLL_BAR_DELAY_MS_DEFAULT;
    (*s).velocity = 0.0f;
    (*s).lastInputMs = 0u;
    (*s).dragging = false;
    (*s).dragGrab = 0.0f;
    (*s).fadeAlpha = 1.0f;
    (*s).fadeOutMs = SCROLL_BAR_FADE_MS_DEFAULT;
    (*s).grappable = SCROLL_BAR_GRAPPABLE_DEFAULT;
    GraphicsComponent_setOpacity(&(*base).component, SCROLL_BAR_OPACITY_DEFAULT);
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

static float gestureDelta(float deltaPx, float trackLen, float span) {
    if (trackLen <= 0.0f)
        return 0.0f;
    return deltaPx / trackLen * span;
}

static float pin01(float t) {
    if (t < 0.0f)
        return 0.0f;
    if (t > 1.0f)
        return 1.0f;
    return t;
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

void ScrollBar_noteScroll(ScrollBar *s, uint64_t nowMs) {
    if (!s)
        return;
    (*s).hasScrolled = true;
    (*s).lastScrollMs = nowMs;
    (*s).autoHidden = false;
    Panel *base = &(*s).base;
    Panel_setVisible(base, true);
}

// Fade-out (not a toggle): full opacity for the idle hold after the last
// scroll, then a linear fade to zero over fadeOutMs. Derived from the clock
// (not frame-stepped), so it is deterministic on any cadence.
bool ScrollBar_tick(ScrollBar *s, uint64_t nowMs, bool scrollable) {
    if (!s)
        return false;
    if (!(*s).hideWhenUnused) {
        (*s).fadeAlpha = 1.0f;
        (*s).autoHidden = false;
        return true;
    }
    float alpha = 0.0f;
    if (scrollable && (*s).hasScrolled) {
        uint64_t since = nowMs >= (*s).lastScrollMs ? nowMs - (*s).lastScrollMs : 0u;
        if (since <= (*s).idleTimeoutMs) {
            alpha = 1.0f;
        } else if ((*s).fadeOutMs == 0u) {
            alpha = 0.0f;
        } else {
            uint64_t into = since - (*s).idleTimeoutMs;
            if (into >= (*s).fadeOutMs)
                alpha = 0.0f;
            else
                alpha = 1.0f - (float) into / (float) (*s).fadeOutMs;
        }
    }
    (*s).fadeAlpha = alpha;
    bool hidden = alpha <= 0.0f;
    (*s).autoHidden = hidden;
    Panel *base = &(*s).base;
    Panel_setVisible(base, !hidden);
    return !hidden;
}

// The R4 -> R3 handoff: map this bar's behavior state into the R3 graphics
// holder (the holder owns docking + thumb geometry + the two quads).
void ScrollBar_fillGraphics(const ScrollBar *s, ScrollBarGraphics *dest) {
    if (!dest)
        return;
    ScrollBarGraphics_init(dest);
    if (!s)
        return;
    (*dest).orientation = (*s).orientation == SCROLL_BAR_HORIZONTAL
        ? SCROLL_GRAPHICS_HORIZONTAL : SCROLL_GRAPHICS_VERTICAL;
    (*dest).thickness = (*s).thickness;
    (*dest).inset = (*s).inset;
    float lo = (*s).min;
    float hi = (*s).max;
    if (lo > hi) {
        float tmp = lo;
        lo = hi;
        hi = tmp;
    }
    float span = hi - lo;
    (*dest).fraction = span > 0.0f ? ((*s).value - lo) / span : 0.0f;
    (*dest).viewLen = (*s).viewLen;
    (*dest).contentLen = (*s).contentLen;
    (*dest).thumbMin = (*s).thumbMin;
    (*dest).shortLimit = (*s).shortLimit;
    (*dest).opacity = (*s).opacity * (*s).fadeAlpha;
    const Panel *base = &(*s).base;
    (*dest).visible = Panel_isVisible(base);
}

void ScrollBar_thumbRect(const ScrollBar *s, const Rectangle *viewportRect, Rectangle *dest) {
    ScrollBarGraphics g;
    ScrollBar_fillGraphics(s, &g);
    ScrollBarGraphics_thumbRect(&g, viewportRect, dest);
}

void ScrollBar_setLengths(ScrollBar *s, float viewportLen, float contentLen) {
    if (!s)
        return;
    (*s).viewLen = viewportLen;
    (*s).contentLen = contentLen;
}

bool ScrollBar_paint(ScrollBar *s, const Rectangle *viewportRect) {
    ScrollBarGraphics g;
    ScrollBar_fillGraphics(s, &g);
    return ScrollBarGraphics_paint(&g, viewportRect);
}

// The value span (lo/hi normalized) and the current fraction along it.
static void valueSpan(const ScrollBar *s, float *outLo, float *outHi, float *outFrac) {
    float lo = (*s).min;
    float hi = (*s).max;
    if (lo > hi) {
        float tmp = lo;
        lo = hi;
        hi = tmp;
    }
    float span = hi - lo;
    float frac = span > 0.0f ? ((*s).value - lo) / span : 0.0f;
    if (outLo) *outLo = lo;
    if (outHi) *outHi = hi;
    if (outFrac) *outFrac = frac;
}

bool ScrollBar_beginDrag(ScrollBar *s, float trackLenPx, float trackPosPx, float thumbLenPx) {
    if (!s || !(*s).grappable || trackLenPx <= 0.0f)
        return false;
    float travel = trackLenPx - thumbLenPx;
    if (travel < 0.0f)
        travel = 0.0f;
    float frac = 0.0f;
    valueSpan(s, nullptr, nullptr, &frac);
    float thumbStart = frac * travel;
    if (trackPosPx >= thumbStart && trackPosPx <= thumbStart + thumbLenPx)
        (*s).dragGrab = trackPosPx - thumbStart;   // grabbed the thumb
    else
        (*s).dragGrab = thumbLenPx * 0.5f;         // track click: center it
    (*s).dragging = true;
    (*s).velocity = 0.0f;                          // a grab stops any glide
    return ScrollBar_dragTo(s, trackLenPx, trackPosPx, thumbLenPx);
}

bool ScrollBar_dragTo(ScrollBar *s, float trackLenPx, float trackPosPx, float thumbLenPx) {
    if (!s || !(*s).dragging)
        return false;
    float travel = trackLenPx - thumbLenPx;
    if (travel <= 0.0f)
        return false;
    float thumbStart = trackPosPx - (*s).dragGrab;
    float frac = thumbStart / travel;
    frac = pin01(frac);
    float lo = 0.0f, hi = 0.0f;
    valueSpan(s, &lo, &hi, nullptr);
    (*s).value = lo + frac * (hi - lo);
    return true;
}

void ScrollBar_endDrag(ScrollBar *s) {
    if (!s)
        return;
    (*s).dragging = false;
}

bool ScrollBar_isDragging(const ScrollBar *s) {
    return s ? (*s).dragging : false;
}

float ScrollBar_applyInput(ScrollBar *s, float deltaPx, uint64_t nowMs) {
    if (!s)
        return deltaPx;
    float scaled = deltaPx * (*s).sensitivity;
    (*s).lastInputMs = nowMs;
    bool glides = (*s).scrollMode == SCROLL_BAR_SMOOTH || (*s).scrollMode == SCROLL_BAR_ELASTIC;
    if (glides && (*s).friction > 0.0f)
        (*s).velocity += scaled;
    else
        (*s).velocity = 0.0f;
    return scaled;
}

float ScrollBar_glideStep(ScrollBar *s, uint64_t nowMs, uint64_t dtMs) {
    if (!s)
        return 0.0f;
    if ((*s).scrollMode == SCROLL_BAR_STEP)
        return 0.0f;
    if ((*s).friction <= 0.0f) {
        (*s).velocity = 0.0f;
        return 0.0f;
    }
    if ((*s).velocity == 0.0f || dtMs == 0u)
        return 0.0f;
    if (nowMs < (*s).lastInputMs)
        return 0.0f;
    if (nowMs - (*s).lastInputMs < (*s).delayMs)
        return 0.0f;
    float tau = SCROLL_BAR_GLIDE_TAU_MS * (*s).friction;
    float factor = expf(-(float) dtMs / tau);
    float delta = (*s).velocity * (1.0f - factor);
    (*s).velocity *= factor;
    float absVel = (*s).velocity < 0.0f ? -(*s).velocity : (*s).velocity;
    if (absVel < SCROLL_BAR_GLIDE_MIN_PX)
        (*s).velocity = 0.0f;
    return delta;
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
void ScrollBar_setShortLengthLimit(ScrollBar *s, float percent) {
    if (!s)
        return;
    if (percent < 0.0f)
        percent = 0.0f;
    if (percent > 1.0f)
        percent = 1.0f;
    (*s).shortLimit = percent;
}

;;SETTER
void ScrollBar_setHideWhenUnused(ScrollBar *s, bool hide) {
    if (!s)
        return;
    (*s).hideWhenUnused = hide;
    if (hide) {
        (*s).autoHidden = true;
        Panel *base = &(*s).base;
        Panel_setVisible(base, false);
    } else {
        (*s).autoHidden = false;
        Panel *base = &(*s).base;
        Panel_setVisible(base, true);
    }
}

;;SETTER
void ScrollBar_setOpacity(ScrollBar *s, float opacity) {
    if (!s)
        return;
    if (opacity < 0.0f)
        opacity = 0.0f;
    if (opacity > 1.0f)
        opacity = 1.0f;
    (*s).opacity = opacity;
    Panel *base = &(*s).base;
    GraphicsComponent_setOpacity(&(*base).component, opacity);
}

;;SETTER
void ScrollBar_setIdleTimeoutMs(ScrollBar *s, uint64_t timeoutMs) {
    if (!s)
        return;
    (*s).idleTimeoutMs = timeoutMs;
}

;;SETTER
void ScrollBar_setScrollMode(ScrollBar *s, int mode) {
    if (!s)
        return;
    if (mode != SCROLL_BAR_STEP && mode != SCROLL_BAR_SMOOTH && mode != SCROLL_BAR_ELASTIC)
        return;
    (*s).scrollMode = mode;
    if (mode == SCROLL_BAR_STEP)
        (*s).velocity = 0.0f;
}

;;SETTER
void ScrollBar_setFadeOutMs(ScrollBar *s, uint64_t fadeOutMs) {
    if (!s)
        return;
    (*s).fadeOutMs = fadeOutMs;
}

;;SETTER
void ScrollBar_setGrappable(ScrollBar *s, bool grappable) {
    if (!s)
        return;
    (*s).grappable = grappable;
    if (!grappable)
        (*s).dragging = false;
}

;;SETTER
void ScrollBar_setScrollFriction(ScrollBar *s, float friction) {
    if (!s)
        return;
    if (friction < 0.0f)
        friction = 0.0f;
    (*s).friction = friction;
}

;;SETTER
void ScrollBar_setScrollSensitivity(ScrollBar *s, float sensitivity) {
    if (!s)
        return;
    if (sensitivity < 0.0f)
        sensitivity = 0.0f;
    (*s).sensitivity = sensitivity;
}

;;SETTER
void ScrollBar_setScrollDelay(ScrollBar *s, uint64_t delayMs) {
    if (!s)
        return;
    (*s).delayMs = delayMs;
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
float ScrollBar_getShortLengthLimit(const ScrollBar *s) {
    return s ? (*s).shortLimit : 0.0f;
}

;;GETTER
bool ScrollBar_isHideWhenUnused(const ScrollBar *s) {
    return s ? (*s).hideWhenUnused : false;
}

;;GETTER
float ScrollBar_getOpacity(const ScrollBar *s) {
    return s ? (*s).opacity : 0.0f;
}

;;GETTER
uint64_t ScrollBar_getIdleTimeoutMs(const ScrollBar *s) {
    return s ? (*s).idleTimeoutMs : 0u;
}

;;GETTER
int ScrollBar_getScrollMode(const ScrollBar *s) {
    return s ? (*s).scrollMode : SCROLL_BAR_MODE_DEFAULT;
}

;;GETTER
float ScrollBar_getScrollFriction(const ScrollBar *s) {
    return s ? (*s).friction : SCROLL_BAR_FRICTION_DEFAULT;
}

;;GETTER
float ScrollBar_getScrollSensitivity(const ScrollBar *s) {
    return s ? (*s).sensitivity : SCROLL_BAR_SENSITIVITY_DEFAULT;
}

;;GETTER
uint64_t ScrollBar_getScrollDelay(const ScrollBar *s) {
    return s ? (*s).delayMs : SCROLL_BAR_DELAY_MS_DEFAULT;
}

;;GETTER
uint64_t ScrollBar_getFadeOutMs(const ScrollBar *s) {
    return s ? (*s).fadeOutMs : SCROLL_BAR_FADE_MS_DEFAULT;
}

;;GETTER
bool ScrollBar_isGrappable(const ScrollBar *s) {
    return s ? (*s).grappable : false;
}

;;GETTER
float ScrollBar_getFadeAlpha(const ScrollBar *s) {
    return s ? (*s).fadeAlpha : 1.0f;
}

;;GETTER
bool ScrollBar_isAutoHidden(const ScrollBar *s) {
    return s ? (*s).autoHidden : false;
}

;;GETTER
bool ScrollBar_isNeeded(const ScrollBar *s) {
    return s ? ((*s).contentLen > (*s).viewLen) : false;
}

;;GETTER
bool ScrollBar_isEffectiveVisible(const ScrollBar *s) {
    if (!s)
        return false;
    const Panel *base = &(*s).base;
    return Panel_isVisible(base) && !(*s).autoHidden;
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
    Str_printf(&s, "ScrollBar[%s %s value=%.2f [%.2f, %.2f] thickness=%.1f inset=%.1f short=%.2f %s opacity=%.2f]",
               (*self).orientation == SCROLL_BAR_HORIZONTAL ? "horizontal" : "vertical",
               (*self).mode == SCROLL_BAR_POINT ? "point" : "gesture",
               (*self).value, (*self).min, (*self).max,
               (*self).thickness, (*self).inset, (*self).shortLimit,
               (*self).hideWhenUnused ? ((*self).autoHidden ? "auto-hidden" : "auto-shown") : "always",
               (*self).opacity);
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
    Str_printf(&s, "thumbMin: %.1f, shortLimit: %.2f, thickness: %.1f, inset: %.1f, ", (*self).thumbMin, (*self).shortLimit, (*self).thickness, (*self).inset);
    Str_printf(&s, "hideWhenUnused: %s, opacity: %.2f, idleTimeoutMs: %llu, autoHidden: %s, ", (*self).hideWhenUnused ? "true" : "false", (*self).opacity, (unsigned long long) (*self).idleTimeoutMs, (*self).autoHidden ? "true" : "false");
    Str_put(&s, "track: ");
    Str_put(&s, track);
    Str_put(&s, " }");
    if (outTruncated) *outTruncated = Str_isTruncated(&s) || trackTruncated;
}
