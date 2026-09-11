#ifndef DARLING_SCROLL_PANEL_H
#define DARLING_SCROLL_PANEL_H

#include <stdbool.h>
#include <stdint.h>

#include "c23/constructor.h"
#include "darling/field/scrollbar.h"
#include "darling/panel/panel.h"
#include "oop/type.h"



// darling/panel/scroll_container.h — viewport over an oversized content panel.
//
// Three sub-APIs, one owner (never touch members directly):
//   ScrollContainer_*           — viewport core (content, offsets, insets)
//   ScrollContainer_scrollbar_* — the owned ScrollBar part (visibility)
//   ScrollContainer_panel_*     — the content Panel part (size, color, radius)
//
// Feel (touchscreen physics): fling velocity + slippery friction +
// overscroll rubber-band, advanced on Thread 0 via ScrollContainer_tick.

typedef struct ScrollContainer {
    // --- ScrollContainer core (owner fields: viewport state, not any part) ---
    Panel base;
    Panel *content;   // viewport child (edited via panel_* part); null = empty
    float offsetX;    // scroll offset into content (single source of truth)
    float offsetY;
    float startInset; // padding before the first child
    float endInset;   // overscroll past the last child
    // --- Scrollbar part (scrollbar_* verbs; bar is a live view) ---
    ScrollBar *bar;   // owned vertical bar (replaceable view, never freed)
    bool barVisible;  // scrollbar-part visibility (default true)
    // --- Feel part (owner fields: touchscreen physics, tick advances) ---
    float velX;       // fling velocity px/sec (decays in tick)
    float velY;
    float slippery;   // 0 stops dead, 1 long glide (default 0)
    float overscroll; // rubber-band px past each end (0 = disabled)
    // --- Direction part (owner field: which way deltas push content) ---
    bool natural;     // true: gesture-following (ox+dx, oy-dy); false: legacy flip
    // NOTE: the content-panel part (panel_* verbs) owns NO fields here —
    // it forwards to content above. New stored panel state is a smell.
} ScrollContainer;

// Constructors:
//   ScrollContainer(viewW, viewH)   — clipped viewport with an owned vertical bar
ScrollContainer *ScrollContainer_2(float viewW, float viewH);

#define ScrollContainer(...) CONSTRUCTOR_DISPATCH(ScrollContainer, __VA_ARGS__)

// Core (content attach is detach-only, never frees; bar<->offset stay synced).
void ScrollContainer_setContent(ScrollContainer *sp, Panel *content);
void ScrollContainer_setOffset(ScrollContainer *sp, float x, float y);
// Viewport tracks the window: resizes SELF (lifting the first-size ceiling),
// re-docks the bar, re-clamps the offset. Content keeps its own size —
// that is the point of a content panel. Call on every window resize.
void ScrollContainer_setViewportSize(ScrollContainer *sp, float w, float h);
void ScrollContainer_syncFromBar(ScrollContainer *sp);
void ScrollContainer_syncToBar(ScrollContainer *sp);

// Setters.
void ScrollContainer_setStartInset(ScrollContainer *sp, float inset);
void ScrollContainer_setEndInset(ScrollContainer *sp, float inset);

// Scrollbar part (its own thing: visibility of the owned bar node).
void ScrollContainer_scrollbar_setVisible(ScrollContainer *sp, bool visible);
void ScrollContainer_scrollbar_setBar(ScrollContainer *sp, ScrollBar *bar);

// Feel part (touchscreen physics: fling + slippery + overscroll).
void ScrollContainer_setSlippery(ScrollContainer *sp, float slippery);
void ScrollContainer_setOverscroll(ScrollContainer *sp, float px);
void ScrollContainer_fling(ScrollContainer *sp, float vx, float vy);
void ScrollContainer_stop(ScrollContainer *sp);
void ScrollContainer_tick(ScrollContainer *sp, double dt);

// Direction part (which way trackpad/mouse deltas push the content).
void ScrollContainer_setNatural(ScrollContainer *sp, bool natural);
void ScrollContainer_scrollBy(ScrollContainer *sp, float dx, float dy);

// Content-panel part (modify the panel through here, never panel->field).
void ScrollContainer_panel_setSize(ScrollContainer *sp, float w, float h);
void ScrollContainer_panel_setBackgroundColor(ScrollContainer *sp, uint32_t color);
void ScrollContainer_panel_setRadius(ScrollContainer *sp, float radius);

// Layer part (window-IS-scrollpanel model): resolve a direct child to its
// scrolled frame. Content children move by -offset (they scroll); the bar
// resolves undocked (chrome stays). The layer bridge calls this per child
// instead of raw Container_resolve — C-side resolve is the source of truth.
void ScrollContainer_childFrame(const ScrollContainer *sp, const Panel *child, float winW, float winH,
                            float *outX, float *outY, float *outW, float *outH);

// Getters.
Panel *ScrollContainer_getContent(const ScrollContainer *sp);
void ScrollContainer_getOffset(const ScrollContainer *sp, float *outX, float *outY);
float ScrollContainer_getStartInset(const ScrollContainer *sp);
float ScrollContainer_getEndInset(const ScrollContainer *sp);
ScrollBar *ScrollContainer_getBar(const ScrollContainer *sp);

// Scrollbar-part getters.
bool ScrollContainer_scrollbar_isVisible(const ScrollContainer *sp);

// Feel-part getters.
float ScrollContainer_getSlippery(const ScrollContainer *sp);
float ScrollContainer_getOverscroll(const ScrollContainer *sp);
void ScrollContainer_getVelocity(const ScrollContainer *sp, float *outVX, float *outVY);
bool ScrollContainer_isScrolling(const ScrollContainer *sp);
bool ScrollContainer_isOverscrolled(const ScrollContainer *sp);

// Direction-part getters.
bool ScrollContainer_isNatural(const ScrollContainer *sp);

// Content-panel-part getters.
void ScrollContainer_panel_getSize(const ScrollContainer *sp, float *outW, float *outH);
uint32_t ScrollContainer_panel_getBackgroundColor(const ScrollContainer *sp);
float ScrollContainer_panel_getRadius(const ScrollContainer *sp);

#endif
