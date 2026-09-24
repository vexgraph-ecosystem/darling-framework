#ifndef DARLING_SCROLLBAR_H
#define DARLING_SCROLLBAR_H

#include <stdbool.h>
#include <stdint.h>

#include "c23/constructor.h"
#include "darling/panel/panel.h"
#include "event/pointer.h"
#include "lang/graphics_component.h"
#include "oop/type.h"



#define SCROLL_BAR_GESTURE  0
#define SCROLL_BAR_POINT    1

#define SCROLL_BAR_VERTICAL    0
#define SCROLL_BAR_HORIZONTAL  1

// Labeled defaults (the No Hardcoding Law): every constructor magnitude and
// paint color is named once here — no bare literals in the .c.
#define SCROLL_BAR_VALUE_MIN_DEFAULT    0.0f
#define SCROLL_BAR_VALUE_MAX_DEFAULT    1.0f
#define SCROLL_BAR_THUMB_MIN_DEFAULT    24.0f
#define SCROLL_BAR_THICKNESS_DEFAULT    12.0f
#define SCROLL_BAR_INSET_DEFAULT        2.0f
#define SCROLL_BAR_SHORT_LIMIT_DEFAULT  0.0f
#define SCROLL_BAR_OPACITY_DEFAULT      1.0f
#define SCROLL_BAR_IDLE_MS_DEFAULT      1200u
#define SCROLL_BAR_TRACK_COLOR          0xFFFFFF2Eu
#define SCROLL_BAR_THUMB_COLOR          0xFFFFFFB3u

// Scroll behavior (per bar = per axis, per panel): how input becomes motion.
#define SCROLL_BAR_STEP     0   // discrete: each input lands immediately
#define SCROLL_BAR_SMOOTH   1   // glides: momentum decays by friction

// Behavior defaults + glide tuning (the No Hardcoding Law).
#define SCROLL_BAR_MODE_DEFAULT         SCROLL_BAR_STEP
#define SCROLL_BAR_FRICTION_DEFAULT     1.0f    // 0 = no glide, 1 = normal, >1 = longer
#define SCROLL_BAR_SENSITIVITY_DEFAULT  1.0f    // input multiplier (1.0 = unchanged)
#define SCROLL_BAR_DELAY_MS_DEFAULT     0u      // settle hold before glide begins
#define SCROLL_BAR_GLIDE_TAU_MS         300.0f  // friction-1 glide time constant
#define SCROLL_BAR_GLIDE_MIN_PX         0.05f   // below this, momentum stops

// darling/field/scrollbar.h — track+thumb scroller with two thumb laws.
//
// Value math (gesture deltas, point jumps, range clamps) is preserved from the
// donor; geometry is rewritten onto the graphvex language: the bar owns an
// AUTHORITATIVE GraphicsComponent track (docked via anchor, eager abs) while
// the Panel base carries R4 widget identity (pointer, color, hierarchy).
// Orientation selects the axis (vertical bars map localY, horizontal localX).

typedef struct ScrollBar {
    Panel base;                // R4 identity (pointer, color, hierarchy slot)
    GraphicsComponent track;   // AUTHORITATIVE track geometry (graphvex language)
    int mode;                  // SCROLL_BAR_GESTURE or SCROLL_BAR_POINT
    int orientation;           // SCROLL_BAR_VERTICAL or SCROLL_BAR_HORIZONTAL
    float min;                 // Lower track bound
    float max;                 // Upper track bound
    float value;               // Thumb value, clamped to min/max
    float thumbMin;            // Minimum thumb extent in px
    float thickness;           // Cross-axis extent (bar width / bar height)
    float inset;               // Edge inset from the docked corner
    float shortLimit;          // Minimum thumb share of the track (0..1)
    bool hideWhenUnused;       // True = overlay bar: hidden until scrolled
    float opacity;             // Bar opacity (0..1, pushed to the base)
    uint64_t idleTimeoutMs;    // Hide delay after the last scroll
    uint64_t lastScrollMs;     // Clock of the last noted scroll
    bool hasScrolled;          // True once any scroll was noted
    bool autoHidden;           // True = auto-hide currently hiding the bar
    float viewLen;             // Viewport length along the bar (paint lens)
    float contentLen;          // Content length along the bar (paint lens)
    int scrollMode;            // SCROLL_BAR_STEP or SCROLL_BAR_SMOOTH
    float friction;            // 0 = no glide, 1 = default, >1 = longer glide
    float sensitivity;         // Input multiplier (1.0 = unchanged)
    uint64_t delayMs;          // Settle hold before glide begins
    float velocity;            // Remaining px carried by momentum (current var)
    uint64_t lastInputMs;      // Clock of the last input (momentum arming)
} ScrollBar;

// Constructors:
//   ScrollBar()                  — detached gesture-mode vertical bar on [0, 1]
//   ScrollBar(mode)              — detached vertical bar in the given mode
//   ScrollBar(mode, orientation) — detached bar in the given mode + orientation
ScrollBar *ScrollBar_0(void);
ScrollBar *ScrollBar_1(int mode);
ScrollBar *ScrollBar_2(int mode, int orientation);

#define ScrollBar(...) CONSTRUCTOR_DISPATCH(ScrollBar, __VA_ARGS__)

// Core (pure value mapping; the input pump calls these later).
void ScrollBar_dragBy(ScrollBar *s, float deltaPx, float trackLen);
void ScrollBar_clickAt(ScrollBar *s, float fraction);
void ScrollBar_setRange(ScrollBar *s, float min, float max);
void ScrollBar_handlePointer(ScrollBar *s, int kind, float localX, float localY);
// Geometry: the thumb rect in absolute coords from value + track abs. Pure over
// scalars (viewport/content lengths drive the thumb proportion); testable
// without a window.
void ScrollBar_thumbRect(const ScrollBar *s, float viewportLen, float contentLen,
                         float *outX, float *outY, float *outW, float *outH);
// Paint lens: the viewport/content lengths the thumb derives from when the
// bar paints itself (pushed by the owning ScrollPanel every layout pass).
void ScrollBar_setLengths(ScrollBar *s, float viewportLen, float contentLen);
// Paint the track + thumb into an explicit track rect (immediate mode, no
// cached state). False when hidden, fully transparent, nothing to scroll
// (content fits the viewport — a rangeless axis paints no bar), or hostile.
bool ScrollBar_paint(ScrollBar *s, const Rectangle *trackRect);
// Behavior: apply an input delta through the bar's sensitivity and arm
// momentum; returns the scaled delta the caller applies to the offset.
float ScrollBar_applyInput(ScrollBar *s, float deltaPx, uint64_t nowMs);
// Behavior: one momentum step. Returns the glide delta for this tick and
// decays velocity (0 when step-mode, friction 0, within the delay hold, or
// settled below SCROLL_BAR_GLIDE_MIN_PX).
float ScrollBar_glideStep(ScrollBar *s, uint64_t nowMs, uint64_t dtMs);

// Activity (overlay auto-hide driven by an explicit caller clock — no
// threads; note on scroll, tick on idle; dest-last outputs stay last).
void ScrollBar_noteScroll(ScrollBar *s, uint64_t nowMs);
bool ScrollBar_tick(ScrollBar *s, uint64_t nowMs, bool scrollable);

// Setters.
void ScrollBar_setMode(ScrollBar *s, int mode);
void ScrollBar_setOrientation(ScrollBar *s, int orientation);
void ScrollBar_setValue(ScrollBar *s, float value);
void ScrollBar_setThumbMin(ScrollBar *s, float px);
void ScrollBar_setShortLengthLimit(ScrollBar *s, float percent);
void ScrollBar_setHideWhenUnused(ScrollBar *s, bool hide);
void ScrollBar_setOpacity(ScrollBar *s, float opacity);
void ScrollBar_setIdleTimeoutMs(ScrollBar *s, uint64_t timeoutMs);
void ScrollBar_setScrollMode(ScrollBar *s, int mode);
void ScrollBar_setScrollFriction(ScrollBar *s, float friction);
void ScrollBar_setScrollSensitivity(ScrollBar *s, float sensitivity);
void ScrollBar_setScrollDelay(ScrollBar *s, uint64_t delayMs);
void ScrollBar_setThickness(ScrollBar *s, float px);
void ScrollBar_setInset(ScrollBar *s, float px);

// Getters.
int ScrollBar_getMode(const ScrollBar *s);
int ScrollBar_getOrientation(const ScrollBar *s);
float ScrollBar_getValue(const ScrollBar *s);
float ScrollBar_getThumbMin(const ScrollBar *s);
float ScrollBar_getShortLengthLimit(const ScrollBar *s);
bool ScrollBar_isHideWhenUnused(const ScrollBar *s);
float ScrollBar_getOpacity(const ScrollBar *s);
uint64_t ScrollBar_getIdleTimeoutMs(const ScrollBar *s);
int ScrollBar_getScrollMode(const ScrollBar *s);
float ScrollBar_getScrollFriction(const ScrollBar *s);
float ScrollBar_getScrollSensitivity(const ScrollBar *s);
uint64_t ScrollBar_getScrollDelay(const ScrollBar *s);
bool ScrollBar_isAutoHidden(const ScrollBar *s);
bool ScrollBar_isEffectiveVisible(const ScrollBar *s);
float ScrollBar_getThickness(const ScrollBar *s);
float ScrollBar_getInset(const ScrollBar *s);
void ScrollBar_getRange(const ScrollBar *s, float *outMin, float *outMax);

// --- toString Law (bounded, cold-path) ---
void ScrollBar_toString(const ScrollBar *self, char *dest, size_t cap, bool *outTruncated);
void ScrollBar_toStringStruct(const ScrollBar *self, char *dest, size_t cap, bool *outTruncated);

#endif
