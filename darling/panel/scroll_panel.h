#ifndef DARLING_SCROLL_PANEL_H
#define DARLING_SCROLL_PANEL_H

#include <stdbool.h>
#include <stdint.h>

#include "c23/constructor.h"
#include "darling/field/scrollbar.h"
#include "darling/panel/panel.h"
#include "oop/type.h"

// darling/panel/scroll_panel.h — viewport over an oversized content panel.
//
// The panel itself, a horizontal scrollbar, a vertical scrollbar, and a
// content panel. Three part families, one owner (never touch members
// directly):
//   ScrollPanel_*                  — viewport core (content, offsets, AUTO)
//   ScrollPanel_verticalScroll_*   — the owned vertical bar part (geometry)
//   ScrollPanel_horizontalScroll_* — the owned horizontal bar part (geometry)
//   ScrollPanel_contentPanel_*     — the content Panel part (size, visibility)
//
// Same properties as a Panel, as a whole (the viewport IS a Panel). The
// content panel is NOT stuck to the viewport: it keeps its own size unless it
// carries SIZE_AUTO, in which case it hugs the viewport (the Absolute Size
// and Location Law). Offsets are the single source of truth, clamped to the
// content/viewport bounds; bars derive from the absolute every layout pass,
// so a live resize never drifts them (BUG-008).

// Extra grab area around a bar's track (px), so a 12px bar is easy to hold.
#define SCROLLPANEL_DRAG_HIT_PAD 4.0f

typedef struct ScrollPanel {
    Panel base;                 // the viewport itself (same properties as a whole)
    Panel *contentPanel;        // borrowed content (detach-only, never freed)
    ScrollBar *hBar;            // owned horizontal bar (arena lifetime, never freed)
    ScrollBar *vBar;            // owned vertical bar (arena lifetime, never freed)
    float offsetX;              // scroll offset into content (single source of truth)
    float offsetY;
    bool contentAutoW;          // AUTO content hugs the viewport width
    bool contentAutoH;          // AUTO content hugs the viewport height
    bool hVisible;              // horizontal bar master visibility
    bool vVisible;              // vertical bar master visibility
    uint64_t lastTickMs;        // caller clock for overlay auto-hide
    int32_t dragAxis;           // -1 none, 0 vertical bar, 1 horizontal bar
} ScrollPanel;

// Constructors:
//   ScrollPanel(viewW, viewH) — viewport with owned h/v bars, no content
ScrollPanel *ScrollPanel_2(float viewW, float viewH);

#define ScrollPanel(...) CONSTRUCTOR_DISPATCH(ScrollPanel, __VA_ARGS__)

// Core (content attach is detach-only, never frees; bars re-raise on top;
// offsets clamp to content/viewport bounds; AUTO content hugs the viewport).
// The *At forms stamp the caller clock so overlay bars can show on scroll;
// tick hides them again after the idle timeout. No threads, no waits.
void ScrollPanel_setContent(ScrollPanel *sp, Panel *content);
void ScrollPanel_setOffset(ScrollPanel *sp, float x, float y);
void ScrollPanel_setOffsetAt(ScrollPanel *sp, float x, float y, uint64_t nowMs);
void ScrollPanel_scrollBy(ScrollPanel *sp, float dx, float dy);
void ScrollPanel_scrollByAt(ScrollPanel *sp, float dx, float dy, uint64_t nowMs);
// Chained scroll for nesting: consume what fits inside the bounds, report
// the leftover in outDx/outDy (dest-last) so the caller can bubble it to
// the parent — when the inner panel is at its end, the outer continues.
void ScrollPanel_scrollByChained(ScrollPanel *sp, float dx, float dy, uint64_t nowMs,
                                 float *outDx, float *outDy);
// Wheel/trackpad input entry: each axis scales by its bar's sensitivity and
// arms that bar's momentum; the offset then moves and glides on tick.
void ScrollPanel_scrollInputAt(ScrollPanel *sp, float dx, float dy, uint64_t nowMs);
// Input + chaining in one: sensitivity + momentum arm, then the leftover
// bubbles to the caller (dest-last) so a parent continues at the inner end.
void ScrollPanel_scrollInputChainedAt(ScrollPanel *sp, float dx, float dy, uint64_t nowMs,
                                      float *outDx, float *outDy);
// Pointer drag on the bars: begin grabs the bar under the viewport-local
// point (thumb or track), dragTo tracks the held pointer, end releases.
// Viewport-local coordinates (0,0 = panel top-left).
bool ScrollPanel_barDragBegin(ScrollPanel *sp, float localX, float localY);
void ScrollPanel_barDragTo(ScrollPanel *sp, float localX, float localY);
void ScrollPanel_barDragEnd(ScrollPanel *sp);
bool ScrollPanel_isBarDragging(const ScrollPanel *sp);
void ScrollPanel_tick(ScrollPanel *sp, uint64_t nowMs);
// Immediate-mode paint: viewport stages, scissored content subtree shifted
// by the offsets, bars last. skip (nullable) excludes one subtree — the
// demo passes a nested ScrollPanel's base and paints it separately.
bool ScrollPanel_paint(ScrollPanel *sp, const Rectangle *rect, const Panel *skip);
// Same, but excludes a set of subtrees (nested panels a page paints itself).
bool ScrollPanel_paintSkips(ScrollPanel *sp, const Rectangle *rect,
                            const Panel *const *skips, size_t skipCount);
void ScrollPanel_setViewportSize(ScrollPanel *sp, float w, float h);
void ScrollPanel_setContentSize(ScrollPanel *sp, float w, float h);
void ScrollPanel_layoutBars(ScrollPanel *sp);
void ScrollPanel_syncToBars(ScrollPanel *sp);
void ScrollPanel_syncFromBars(ScrollPanel *sp);

// verticalScroll part (the owned vertical bar; right-docked geometry).
// Each bar keeps independent state: short limit, auto-hide, opacity.
void ScrollPanel_verticalScroll_setThickness(ScrollPanel *sp, float px);
void ScrollPanel_verticalScroll_setInset(ScrollPanel *sp, float px);
void ScrollPanel_verticalScroll_setVisible(ScrollPanel *sp, bool visible);
void ScrollPanel_verticalScroll_setRange(ScrollPanel *sp, float min, float max);
void ScrollPanel_verticalScroll_setValue(ScrollPanel *sp, float value);
void ScrollPanel_verticalScroll_setShortLengthLimit(ScrollPanel *sp, float percent);
void ScrollPanel_verticalScroll_setHideWhenUnused(ScrollPanel *sp, bool hide);
void ScrollPanel_verticalScroll_setOpacity(ScrollPanel *sp, float opacity);
void ScrollPanel_verticalScroll_setIdleTimeoutMs(ScrollPanel *sp, uint64_t timeoutMs);
void ScrollPanel_verticalScroll_setScrollMode(ScrollPanel *sp, int mode);
void ScrollPanel_verticalScroll_setScrollFriction(ScrollPanel *sp, float friction);
void ScrollPanel_verticalScroll_setScrollSensitivity(ScrollPanel *sp, float sensitivity);
void ScrollPanel_verticalScroll_setScrollDelay(ScrollPanel *sp, uint64_t delayMs);
float ScrollPanel_verticalScroll_getValue(const ScrollPanel *sp);
float ScrollPanel_verticalScroll_getThickness(const ScrollPanel *sp);
float ScrollPanel_verticalScroll_getInset(const ScrollPanel *sp);
float ScrollPanel_verticalScroll_getShortLengthLimit(const ScrollPanel *sp);
bool ScrollPanel_verticalScroll_isHideWhenUnused(const ScrollPanel *sp);
float ScrollPanel_verticalScroll_getOpacity(const ScrollPanel *sp);
uint64_t ScrollPanel_verticalScroll_getIdleTimeoutMs(const ScrollPanel *sp);
int ScrollPanel_verticalScroll_getScrollMode(const ScrollPanel *sp);
float ScrollPanel_verticalScroll_getScrollFriction(const ScrollPanel *sp);
float ScrollPanel_verticalScroll_getScrollSensitivity(const ScrollPanel *sp);
uint64_t ScrollPanel_verticalScroll_getScrollDelay(const ScrollPanel *sp);
bool ScrollPanel_verticalScroll_isEffectiveVisible(const ScrollPanel *sp);
void ScrollPanel_verticalScroll_getRange(const ScrollPanel *sp, float *outMin, float *outMax);
void ScrollPanel_verticalScroll_getThumbRect(const ScrollPanel *sp,
                                             float *outX, float *outY, float *outW, float *outH);

// horizontalScroll part (the owned horizontal bar; bottom-docked geometry).
void ScrollPanel_horizontalScroll_setThickness(ScrollPanel *sp, float px);
void ScrollPanel_horizontalScroll_setInset(ScrollPanel *sp, float px);
void ScrollPanel_horizontalScroll_setVisible(ScrollPanel *sp, bool visible);
void ScrollPanel_horizontalScroll_setRange(ScrollPanel *sp, float min, float max);
void ScrollPanel_horizontalScroll_setValue(ScrollPanel *sp, float value);
void ScrollPanel_horizontalScroll_setShortLengthLimit(ScrollPanel *sp, float percent);
void ScrollPanel_horizontalScroll_setHideWhenUnused(ScrollPanel *sp, bool hide);
void ScrollPanel_horizontalScroll_setOpacity(ScrollPanel *sp, float opacity);
void ScrollPanel_horizontalScroll_setIdleTimeoutMs(ScrollPanel *sp, uint64_t timeoutMs);
void ScrollPanel_horizontalScroll_setScrollMode(ScrollPanel *sp, int mode);
void ScrollPanel_horizontalScroll_setScrollFriction(ScrollPanel *sp, float friction);
void ScrollPanel_horizontalScroll_setScrollSensitivity(ScrollPanel *sp, float sensitivity);
void ScrollPanel_horizontalScroll_setScrollDelay(ScrollPanel *sp, uint64_t delayMs);
float ScrollPanel_horizontalScroll_getValue(const ScrollPanel *sp);
float ScrollPanel_horizontalScroll_getThickness(const ScrollPanel *sp);
float ScrollPanel_horizontalScroll_getInset(const ScrollPanel *sp);
float ScrollPanel_horizontalScroll_getShortLengthLimit(const ScrollPanel *sp);
bool ScrollPanel_horizontalScroll_isHideWhenUnused(const ScrollPanel *sp);
float ScrollPanel_horizontalScroll_getOpacity(const ScrollPanel *sp);
uint64_t ScrollPanel_horizontalScroll_getIdleTimeoutMs(const ScrollPanel *sp);
int ScrollPanel_horizontalScroll_getScrollMode(const ScrollPanel *sp);
float ScrollPanel_horizontalScroll_getScrollFriction(const ScrollPanel *sp);
float ScrollPanel_horizontalScroll_getScrollSensitivity(const ScrollPanel *sp);
uint64_t ScrollPanel_horizontalScroll_getScrollDelay(const ScrollPanel *sp);
bool ScrollPanel_horizontalScroll_isEffectiveVisible(const ScrollPanel *sp);
void ScrollPanel_horizontalScroll_getRange(const ScrollPanel *sp, float *outMin, float *outMax);
void ScrollPanel_horizontalScroll_getThumbRect(const ScrollPanel *sp,
                                               float *outX, float *outY, float *outW, float *outH);

// contentPanel part (the borrowed content; SIZE_AUTO hugs the viewport).
void ScrollPanel_contentPanel_setSize(ScrollPanel *sp, float w, float h);
void ScrollPanel_contentPanel_getSize(const ScrollPanel *sp, float *outW, float *outH);
void ScrollPanel_contentPanel_setVisible(ScrollPanel *sp, bool visible);

// Getters.
void ScrollPanel_getOffset(const ScrollPanel *sp, float *outX, float *outY);
Panel *ScrollPanel_getContentPanel(const ScrollPanel *sp);
bool ScrollPanel_isContentAutoWidth(const ScrollPanel *sp);
bool ScrollPanel_isContentAutoHeight(const ScrollPanel *sp);

// --- toString Law (bounded, cold-path) ---
void ScrollPanel_toString(const ScrollPanel *self, char *dest, size_t cap, bool *outTruncated);
void ScrollPanel_toStringStruct(const ScrollPanel *self, char *dest, size_t cap, bool *outTruncated);

#endif
