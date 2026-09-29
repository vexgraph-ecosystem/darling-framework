#include "darling/panel/scroll_panel.h"

#include "annotation/definition.h"
#include "annotation/overview.h"
#include "annotation/getter.h"
#include "annotation/setter.h"
#include "lang/graphics_component.h"
#include "lang/scroll_panel_graphics.h"
#include "lang/size.h"
#include "lang/str.h"
#include "nio/mem.h"
#include "oop/type.h"

#include <math.h>
#include <float.h>

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: ScrollPanel
 * ============================================================================
 * Viewport over an oversized content panel: the panel itself (same properties
 * as a Panel, as a whole), a horizontal scrollbar, a vertical scrollbar, and
 * a content panel. Successor to ScrollContainer (single vertical bar only):
 * this panel owns the h/v pair from construction, speaks geometry part verbs
 * per bar (thickness/inset/visibility/range/value/thumb), and resolves AUTO
 * content to the viewport. ScrollPanel is the sole offset physics writer:
 * direct, native, fallback deceleration, and exact critical spring states are
 * mutually coordinated per axis. UIScrollView-style nonlinear resistance
 * preserves raw pull for reversal, while bars remain configuration and chrome.
 * Every layoutBars pass re-derives edges without clamping valid active
 * overscroll, but discards pull when current geometry no longer overflows.
 * Non-finite cold tuning is rejected and non-finite hot packets are dropped.
 * A successful bar grab cancels both axes before bar values become offsets.
 *
 * The content panel is borrowed (detach-only, never freed); the bars are
 * arena-lifetime (never freed, like the donor). Bar geometry resolves through
 * each bar's graphvex GraphicsComponent track (the Absolute Size and Location
 * Law): right-docked / bottom-docked via anchor, eager abs, thumb derived on
 * demand. The Panel bases carry R4 widget identity underneath.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: ScrollPanel (embeds Panel; owns ScrollBars; borrows content Panel)
 * LEVEL: L2 — Behavior (viewport + h/v scrollbars + content panel)
 * ============================================================================
 * SUMMARY:
 *   Viewport Panel + owned h/v ScrollBars + borrowed content Panel. It owns
 *   captured offset physics; AUTO content hugs the viewport; bars dock and
 *   derive from the absolute without integrating motion.
 *
 * STRUCT FIELDS (Mirroring darling/panel/scroll_panel.h):
 * ----------------------------------------------------------------------------
 *   Panel base;                 // the viewport itself
 *   Panel *contentPanel;        // borrowed content (detach-only, never freed)
 *   ScrollBar *hBar;            // owned horizontal bar (arena lifetime)
 *   ScrollBar *vBar;            // owned vertical bar (arena lifetime)
 *   float offsetX, offsetY;     // displayed scroll offsets (single source of truth)
 *   bool contentAutoW;          // AUTO content hugs the viewport width
 *   bool contentAutoH;          // AUTO content hugs the viewport height
 *   bool hVisible;              // horizontal bar master visibility
 *   bool vVisible;              // vertical bar master visibility
 *   uint64_t lastTickMs;        // caller clock for overlay auto-hide
 *   int32_t dragAxis;           // -1 none, 0 vertical bar, 1 horizontal bar
 *   bool gestureHeld;           // Live CONTACT holds the spring; momentum never does
 *   bool nativeMomentum;        // Native momentum is authoritative
 *   float rawPullX, rawPullY;    // Signed uncompressed elastic pulls
 *   float velocityX, velocityY; // Time-derived fallback velocity (px/sec)
 *   float springVelocityX, springVelocityY; // Critical spring raw velocity
 *   uint64_t lastInputXMs, lastInputYMs;     // Velocity sample clocks
 *   int motionWriterX, motionWriterY;        // Single writer per axis
 *   float rubberCoefficientX, rubberCoefficientY; // Resistance coefficients
 *   float overscrollExtentX, overscrollExtentY;   // Resistance asymptotes
 *   float velocitySampleTauMsX, velocitySampleTauMsY; // EWMA constants
 *   float decelerationTauMsX, decelerationTauMsY; // Exponential decay constants
 *   float stopVelocityX, stopVelocityY;       // Deceleration stop thresholds
 *   float springOmegaX, springOmegaY;         // Critical spring rates
 *   float springSnapDistanceX, springSnapDistanceY; // Snap distances
 *   float springSnapVelocityX, springSnapVelocityY; // Snap velocities
 *
 * PRIVATE HELPERS:
 * ----------------------------------------------------------------------------
 *   pinOffset(value, lo, hi)
 *   offsetBounds(sp, outLoX, outHiX, outLoY, outHiY)
 *   raiseBars(sp)                        : bars last (front) after structure
 *   resolveContentAuto(sp)               : AUTO content hugs the viewport
 *   dockBar(sp, bar, horizontal)         : anchor/dock one bar + its track
 *   applyBarVisible(sp)                  : master && !autoHidden into bar bases
 *   noteBarsScrolled(sp, nowMs)          : show overlay bars on user scroll
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Public Constructors: (.h)
 *   - ScrollPanel_2(viewW, viewH)
 *
 * Private Constructors: (.c static)
 *   - (none)
 *
 * Public Core Functions: (.h)
 *   - ScrollPanel_setContent(sp, content)
 *   - ScrollPanel_setOffset(sp, x, y)
 *   - ScrollPanel_setOffsetAt(sp, x, y, nowMs)
 *   - ScrollPanel_scrollBy(sp, dx, dy)
 *   - ScrollPanel_scrollByAt(sp, dx, dy, nowMs)
 *   - ScrollPanel_scrollInputAt(sp, dx, dy, nowMs)
 *   - ScrollPanel_directBegin/directChange/directEnd
 *   - ScrollPanel_nativeMomentumBegin/nativeMomentumChange/nativeMomentumEnd
 *   - ScrollPanel_cancelMotion
 *   - ScrollPanel_canScrollHorizontal/canScrollVertical/canAcquire
 *   - ScrollPanel_barDragBegin(sp, localX, localY)
 *   - ScrollPanel_barDragTo(sp, localX, localY)
 *   - ScrollPanel_barDragEnd(sp)
 *   - ScrollPanel_isBarDragging(sp)
 *   - ScrollPanel_tick(sp, nowMs)
 *   - ScrollPanel_paint(sp, rect, skip) / ScrollPanel_paintSkips(sp, rect, skips, n)
 *   - ScrollPanel_paintBars(sp, rect)
 *   - ScrollPanel_setViewportSize(sp, w, h)
 *   - ScrollPanel_setContentSize(sp, w, h)
 *   - ScrollPanel_setOverscrollLimit(sp, px) / ScrollPanel_getOverscrollLimit(sp)
 *   - ScrollPanel_setGestureHeld(sp, held) / ScrollPanel_isGestureHeld(sp)
 *   - ScrollPanel_stopGlide(sp)
 *   - ScrollPanel_layoutBars(sp)
 *   - ScrollPanel_syncToBars(sp)
 *   - ScrollPanel_syncFromBars(sp)
 *
 * Private Core Functions: (.c static)
 *   - pinOffset / offsetBounds / axisElastic / axisResistanceEnabled /
 *     finiteFloat / rubberDistance / commitAxisOffset / setAxisMotionWriter /
 *     getAxisMotionWriter / transitionReleasedAxis / cancelMotionForBarDrag /
 *     raiseBars /
 *     resolveContentAuto / dockBar / applyBarVisible / noteBarsScrolled /
 *     placeContent / paintSubtree / fillPanelGraphics / barGeometry / barHit /
 *     barBeginDrag / barDragTo
 *
 * Public verticalScroll Part Verbs: (.h)
 *   - ScrollPanel_verticalScroll_setThickness/setInset/setVisible/setRange/setValue
 *   - ScrollPanel_verticalScroll_setShortLengthLimit/setHideWhenUnused/setOpacity/setIdleTimeoutMs
 *   - ScrollPanel_verticalScroll_getValue/getThickness/getInset/getShortLengthLimit
 *   - ScrollPanel_verticalScroll_isHideWhenUnused/getOpacity/getIdleTimeoutMs/isEffectiveVisible
 *   - ScrollPanel_verticalScroll_setScrollMode/setScrollFriction/setScrollSensitivity/setScrollDelay
 *   - ScrollPanel_verticalScroll_getScrollMode/getScrollFriction/getScrollSensitivity/getScrollDelay
 *   - ScrollPanel_verticalScroll_isNeeded
 *   - ScrollPanel_verticalScroll_set/getRubberCoefficient/set/getOverscrollExtent
 *   - ScrollPanel_verticalScroll_set/getVelocitySampleTauMs/set/getDecelerationTauMs
 *   - ScrollPanel_verticalScroll_set/getStopVelocity/set/getSpringOmega
 *   - ScrollPanel_verticalScroll_set/getSpringSnapDistance/set/getSpringSnapVelocity
 *   - ScrollPanel_verticalScroll_getMotionWriter
 *   - ScrollPanel_verticalScroll_getRange/getThumbRect
 *
 * Public horizontalScroll Part Verbs: (.h)
 *   - ScrollPanel_horizontalScroll_setThickness/setInset/setVisible/setRange/setValue
 *   - ScrollPanel_horizontalScroll_setShortLengthLimit/setHideWhenUnused/setOpacity/setIdleTimeoutMs
 *   - ScrollPanel_horizontalScroll_getValue/getThickness/getInset/getShortLengthLimit
 *   - ScrollPanel_horizontalScroll_isHideWhenUnused/getOpacity/getIdleTimeoutMs/isEffectiveVisible
 *   - ScrollPanel_horizontalScroll_setScrollMode/setScrollFriction/setScrollSensitivity/setScrollDelay
 *   - ScrollPanel_horizontalScroll_getScrollMode/getScrollFriction/getScrollSensitivity/getScrollDelay
 *   - ScrollPanel_horizontalScroll_isNeeded
 *   - ScrollPanel_horizontalScroll_set/getRubberCoefficient/set/getOverscrollExtent
 *   - ScrollPanel_horizontalScroll_set/getVelocitySampleTauMs/set/getDecelerationTauMs
 *   - ScrollPanel_horizontalScroll_set/getStopVelocity/set/getSpringOmega
 *   - ScrollPanel_horizontalScroll_set/getSpringSnapDistance/set/getSpringSnapVelocity
 *   - ScrollPanel_horizontalScroll_getMotionWriter
 *   - ScrollPanel_horizontalScroll_getRange/getThumbRect
 *
 * Public contentPanel Part Verbs: (.h)
 *   - ScrollPanel_contentPanel_setSize/getSize/setVisible
 *
 * Private Setters: (.c static)
 *   - (none)
 *
 * Public Getters: (.h)
 *   - ScrollPanel_getOffset(sp, outX, outY)
 *   - ScrollPanel_getContentPanel(sp)
 *   - ScrollPanel_isContentAutoWidth/Height(sp)
 *
 * Private Getters: (.c static)
 *   - (none)
 *
 * Public toString: (.h)
 *   - ScrollPanel_toString / ScrollPanel_toStringStruct
 * ============================================================================
 */

// CONSTRUCTORS (PUBLIC & PRIVATE)

// Forward: defined with the other private helpers below; called by the ctor.
static void applyBarVisible(ScrollPanel *sp);

ScrollPanel *ScrollPanel_2(float viewW, float viewH) {
    ScrollPanel *sp = (ScrollPanel*) Memory_alloc(TYPE_SCROLLPANEL_SINGLETON, sizeof(ScrollPanel));
    if (!sp)
        return nullptr;
    Panel *b = Panel_0();
    if (!b) {
        Memory_free(sp);
        return nullptr;
    }
    (*sp).base = (*b);
    Memory_free(b);
    (*sp).contentPanel = nullptr;
    ScrollBar *h = ScrollBar_2(SCROLL_BAR_GESTURE, SCROLL_BAR_HORIZONTAL);
    ScrollBar *v = ScrollBar_2(SCROLL_BAR_GESTURE, SCROLL_BAR_VERTICAL);
    if (!h || !v) {
        Memory_free(sp);          // bars are arena-lifetime; the struct frees here
        return nullptr;
    }
    (*sp).hBar = h;
    (*sp).vBar = v;
    (*sp).offsetX = 0.0f;
    (*sp).offsetY = 0.0f;
    (*sp).contentAutoW = false;
    (*sp).contentAutoH = false;
    (*sp).hVisible = true;
    (*sp).vVisible = true;
    (*sp).lastTickMs = 0u;
    (*sp).dragAxis = -1;
    (*sp).gestureHeld = false;
    (*sp).nativeMomentum = false;
    (*sp).rawPullX = 0.0f;
    (*sp).rawPullY = 0.0f;
    (*sp).velocityX = 0.0f;
    (*sp).velocityY = 0.0f;
    (*sp).springVelocityX = 0.0f;
    (*sp).springVelocityY = 0.0f;
    (*sp).lastInputXMs = 0u;
    (*sp).lastInputYMs = 0u;
    (*sp).motionWriterX = SCROLLPANEL_MOTION_IDLE;
    (*sp).motionWriterY = SCROLLPANEL_MOTION_IDLE;
    (*sp).rubberCoefficientX = SCROLLPANEL_RUBBER_COEFFICIENT_DEFAULT;
    (*sp).rubberCoefficientY = SCROLLPANEL_RUBBER_COEFFICIENT_DEFAULT;
    (*sp).overscrollExtentX = SCROLLPANEL_OVERSCROLL_LIMIT_DEFAULT;
    (*sp).overscrollExtentY = SCROLLPANEL_OVERSCROLL_LIMIT_DEFAULT;
    (*sp).velocitySampleTauMsX = SCROLLPANEL_VELOCITY_SAMPLE_TAU_MS_DEFAULT;
    (*sp).velocitySampleTauMsY = SCROLLPANEL_VELOCITY_SAMPLE_TAU_MS_DEFAULT;
    (*sp).decelerationTauMsX = SCROLLPANEL_DECELERATION_TAU_MS_DEFAULT;
    (*sp).decelerationTauMsY = SCROLLPANEL_DECELERATION_TAU_MS_DEFAULT;
    (*sp).stopVelocityX = SCROLLPANEL_STOP_VELOCITY_DEFAULT;
    (*sp).stopVelocityY = SCROLLPANEL_STOP_VELOCITY_DEFAULT;
    (*sp).springOmegaX = SCROLLPANEL_SPRING_OMEGA_DEFAULT;
    (*sp).springOmegaY = SCROLLPANEL_SPRING_OMEGA_DEFAULT;
    (*sp).springSnapDistanceX = SCROLLPANEL_SPRING_SNAP_PX_DEFAULT;
    (*sp).springSnapDistanceY = SCROLLPANEL_SPRING_SNAP_PX_DEFAULT;
    (*sp).springSnapVelocityX = SCROLLPANEL_SPRING_SNAP_VELOCITY_DEFAULT;
    (*sp).springSnapVelocityY = SCROLLPANEL_SPRING_SNAP_VELOCITY_DEFAULT;
    Panel *self = &(*sp).base;
    Component *c = &(*self).component;
    GraphicsComponent_setSize(c, viewW, viewH);
    Panel_addContainer(self, &(*h).base);
    Panel_addContainer(self, &(*v).base);
    applyBarVisible(sp);
    ScrollPanel_layoutBars(sp);
    return sp;
}

// CORE FUNCTIONS (PUBLIC & PRIVATE)

static float pinOffset(float value, float lo, float hi) {
    if (!isfinite(value))
        return lo;
    if (value < lo)
        return lo;
    if (value > hi)
        return hi;
    return value;
}

static float finiteFloat(double value) {
    if (value > (double) FLT_MAX)
        return FLT_MAX;
    if (value < (double) -FLT_MAX)
        return -FLT_MAX;
    if (!isfinite(value))
        return 0.0f;
    return (float) value;
}

static void offsetBounds(const ScrollPanel *sp, float *outLoX, float *outHiX,
                         float *outLoY, float *outHiY);

// True once the axis's last input is old enough that the hand has released
// (the elastic stretch then springs home; before that it is held).
// True when the axis's bar is in elastic mode (stretches past the ends).
static bool axisElastic(const ScrollPanel *sp, bool horizontal) {
    const ScrollBar *bar = horizontal ? (*sp).hBar : (*sp).vBar;
    return bar && ScrollBar_getScrollMode(bar) == SCROLL_BAR_ELASTIC;
}

static bool axisResistanceEnabled(const ScrollPanel *sp, bool horizontal) {
    float extent = horizontal ? (*sp).overscrollExtentX : (*sp).overscrollExtentY;
    float coefficient = horizontal ? (*sp).rubberCoefficientX : (*sp).rubberCoefficientY;
    return axisElastic(sp, horizontal) && extent > 0.0f && coefficient > 0.0f;
}

// The sole runtime offset writer. Constructor zero-initialization is the only
// direct offset assignment outside this helper.
static void commitAxisOffset(ScrollPanel *sp, bool horizontal, float offset, float rawPull) {
    if (horizontal) {
        (*sp).offsetX = offset;
        (*sp).rawPullX = rawPull;
    } else {
        (*sp).offsetY = offset;
        (*sp).rawPullY = rawPull;
    }
}

// Scrollable bounds per axis: [0, max(0, content - viewport)]. The *Elastic
// variant widens a scrollable elastic axis by the overscroll limit, so the
// offset may stretch past an end (and springs home on tick).
static float rubberDistance(float raw, float viewport, float extent, float coefficient) {
    if (!isfinite(raw) || !isfinite(viewport) || !isfinite(extent) || !isfinite(coefficient))
        return 0.0f;
    float magnitude = raw < 0.0f ? -raw : raw;
    float dimension = extent;
    if (viewport > 0.0f && dimension > viewport)
        dimension = viewport;
    if (dimension <= 0.0f || coefficient <= 0.0f)
        return 0.0f;
    double scaled = (double) coefficient * (double) magnitude;
    double resistedWide = (double) dimension * scaled / ((double) dimension + scaled);
    float resisted = resistedWide > (double) FLT_MAX ? FLT_MAX : (float) resistedWide;
    return raw < 0.0f ? -resisted : resisted;
}

// Scrollable bounds per axis: [0, max(0, content - viewport)].
static void offsetBounds(const ScrollPanel *sp, float *outLoX, float *outHiX,
                         float *outLoY, float *outHiY) {
    float viewW = 0.0f, viewH = 0.0f, contentW = 0.0f, contentH = 0.0f;
    if (sp) {
        const Panel *b = &(*sp).base;
        viewW = Component_getWidth(&(*b).component);
        viewH = Component_getHeight(&(*b).component);
        if (!isfinite(viewW) || viewW < 0.0f)
            viewW = 0.0f;
        if (!isfinite(viewH) || viewH < 0.0f)
            viewH = 0.0f;
        Panel *content = (*sp).contentPanel;
        if (content) {
            contentW = Component_getWidth(&(*content).component);
            contentH = Component_getHeight(&(*content).component);
            if (!isfinite(contentW) || contentW < 0.0f)
                contentW = 0.0f;
            if (!isfinite(contentH) || contentH < 0.0f)
                contentH = 0.0f;
        }
    }
    double hxWide = (double) contentW - (double) viewW;
    double hyWide = (double) contentH - (double) viewH;
    float hx = hxWide > (double) FLT_MAX ? FLT_MAX : (float) hxWide;
    float hy = hyWide > (double) FLT_MAX ? FLT_MAX : (float) hyWide;
    if (hx < 0.0f)
        hx = 0.0f;
    if (hy < 0.0f)
        hy = 0.0f;
    if (outLoX) *outLoX = 0.0f;
    if (outHiX) *outHiX = hx;
    if (outLoY) *outLoY = 0.0f;
    if (outHiY) *outHiY = hy;
}

// Front rule: bars paint above content, so after any structural pass both bar
// bases must be the last children. The guard makes repeat runs free.
static void raiseBars(ScrollPanel *sp) {
    if (!sp)
        return;
    Panel *self = &(*sp).base;
    ScrollBar *bars[2] = { (*sp).hBar, (*sp).vBar };
    for (int i = 0; i < 2; i++) {
        ScrollBar *bar = bars[i];
        if (bar == nullptr)
            continue;
        Panel *thumb = &(*bar).base;
        size_t n = Panel_childCount(self);
        if (n == 0 || Panel_getChild(self, n - 1) == thumb)
            continue;
        if (Panel_removeChild(self, thumb))
            Panel_addContainer(self, thumb);
    }
}

// AUTO content hugs the viewport: armed dims resolve to the viewport extent.
// Concrete dims are written through untouched.
static void resolveContentAuto(ScrollPanel *sp) {
    if (!sp || !(*sp).contentPanel)
        return;
    if (!(*sp).contentAutoW && !(*sp).contentAutoH)
        return;
    Panel *b = &(*sp).base;
    float viewW = Component_getWidth(&(*b).component);
    float viewH = Component_getHeight(&(*b).component);
    Panel *content = (*sp).contentPanel;
    Component *cc = &(*content).component;
    float w = Component_getWidth(cc);
    float h = Component_getHeight(cc);
    if ((*sp).contentAutoW)
        w = viewW;
    if ((*sp).contentAutoH)
        h = viewH;
    GraphicsComponent_setSize(cc, w, h);
}

// Dock one bar: the Panel base (R4 identity) and the GraphicsComponent track
// (authoritative geometry) resolve identically — right edge for vertical,
// bottom edge for horizontal, thickness across, length along minus insets.
// Both derive from the viewport abs every call, so a live resize re-docks
// from the same truth the compositor paints (no drift).
static void dockBar(ScrollPanel *sp, ScrollBar *bar, bool horizontal) {
    if (!sp || !bar)
        return;
    Panel *b = &(*sp).base;
    float vax = GraphicsComponent_getAbsX(&(*b).component);
    float vay = GraphicsComponent_getAbsY(&(*b).component);
    float vaw = GraphicsComponent_getAbsW(&(*b).component);
    float vah = GraphicsComponent_getAbsH(&(*b).component);
    float t = ScrollBar_getThickness(bar);
    float inset = ScrollBar_getInset(bar);
    Panel *thumb = &(*bar).base;
    Component *bc = &(*thumb).component;
    GraphicsComponent *track = &(*bar).track;
    if (horizontal) {
        float len = vaw - 2.0f * inset;
        if (len < 0.0f)
            len = 0.0f;
        GraphicsComponent_setAnchor(bc, COMPONENT_ANCHOR_BOTTOM_LEFT);
        GraphicsComponent_setPivot(bc, COMPONENT_PIVOT_BOTTOM_LEFT);
        GraphicsComponent_setLocation(bc, inset, -inset);
        GraphicsComponent_setSize(bc, len, t);
        GraphicsComponent_setParentAbs(track, vax, vay, vaw, vah);
        GraphicsComponent_setAnchor(track, GRAPHICS_COMPONENT_ANCHOR_BOTTOM_LEFT);
        GraphicsComponent_setPivot(track, GRAPHICS_COMPONENT_PIVOT_BOTTOM_LEFT);
        GraphicsComponent_setLocation(track, inset, -inset);
        GraphicsComponent_setSize(track, len, t);
    } else {
        float len = vah - 2.0f * inset;
        if (len < 0.0f)
            len = 0.0f;
        GraphicsComponent_setAnchor(bc, COMPONENT_ANCHOR_TOP_RIGHT);
        GraphicsComponent_setPivot(bc, COMPONENT_PIVOT_TOP_RIGHT);
        GraphicsComponent_setLocation(bc, -inset, inset);
        GraphicsComponent_setSize(bc, t, len);
        GraphicsComponent_setParentAbs(track, vax, vay, vaw, vah);
        GraphicsComponent_setAnchor(track, GRAPHICS_COMPONENT_ANCHOR_TOP_RIGHT);
        GraphicsComponent_setPivot(track, GRAPHICS_COMPONENT_PIVOT_TOP_RIGHT);
        GraphicsComponent_setLocation(track, -inset, inset);
        GraphicsComponent_setSize(track, t, len);
    }
}

static void applyBarVisible(ScrollPanel *sp) {
    if (!sp)
        return;
    if ((*sp).hBar) {
        ScrollBar *bar = (*sp).hBar;
        Panel *thumb = &(*bar).base;
        bool shown = (*sp).hVisible && !(*bar).autoHidden;
        GraphicsComponent_setVisible(&(*thumb).component, shown);
    }
    if ((*sp).vBar) {
        ScrollBar *bar = (*sp).vBar;
        Panel *thumb = &(*bar).base;
        bool shown = (*sp).vVisible && !(*bar).autoHidden;
        GraphicsComponent_setVisible(&(*thumb).component, shown);
    }
}

static void noteBarsScrolled(ScrollPanel *sp, uint64_t nowMs) {
    if (!sp)
        return;
    if ((*sp).hBar)
        ScrollBar_noteScroll((*sp).hBar, nowMs);
    if ((*sp).vBar)
        ScrollBar_noteScroll((*sp).vBar, nowMs);
    applyBarVisible(sp);
}

// The content rides at -offset (top-left anchor default): scrolling shifts
// the subtree under the viewport instead of caching a paint transform —
// the same model ListContainer uses when it stacks children at layout.
static void placeContent(ScrollPanel *sp) {
    if (!sp || !(*sp).contentPanel)
        return;
    Panel *content = (*sp).contentPanel;
    GraphicsComponent_setLocation(&(*content).component, -(*sp).offsetX, -(*sp).offsetY);
}

void ScrollPanel_setContent(ScrollPanel *sp, Panel *content) {
    if (!sp)
        return;
    Panel *old = (*sp).contentPanel;
    if (old == content)
        return;
    Panel *self = &(*sp).base;
    if (old)
        Panel_removeChild(self, old);
    (*sp).contentPanel = nullptr;
    if (content && content != self) {
        Panel_addContainer(self, content);
        (*sp).contentPanel = content;
    }
    raiseBars(sp);
    resolveContentAuto(sp);
    ScrollPanel_setOffset(sp, (*sp).offsetX, (*sp).offsetY);
    placeContent(sp);
    ScrollPanel_layoutBars(sp);
}

void ScrollPanel_setOffset(ScrollPanel *sp, float x, float y) {
    if (!sp)
        return;
    ScrollPanel_setOffsetAt(sp, x, y, (*sp).lastTickMs);
}

void ScrollPanel_setOffsetAt(ScrollPanel *sp, float x, float y, uint64_t nowMs) {
    if (!sp || !isfinite(x) || !isfinite(y))
        return;
    float loX = 0.0f, hiX = 0.0f, loY = 0.0f, hiY = 0.0f;
    offsetBounds(sp, &loX, &hiX, &loY, &hiY);
    Panel *base = &(*sp).base;
    float viewW = Component_getWidth(&(*base).component);
    float viewH = Component_getHeight(&(*base).component);
    if (x < loX && hiX > loX && axisResistanceEnabled(sp, true)) {
        float raw = x - loX;
        float offset = loX + rubberDistance(raw, viewW, (*sp).overscrollExtentX,
                                             (*sp).rubberCoefficientX);
        commitAxisOffset(sp, true, offset, raw);
    } else if (x > hiX && hiX > loX && axisResistanceEnabled(sp, true)) {
        float raw = x - hiX;
        float offset = hiX + rubberDistance(raw, viewW, (*sp).overscrollExtentX,
                                             (*sp).rubberCoefficientX);
        commitAxisOffset(sp, true, offset, raw);
    } else {
        commitAxisOffset(sp, true, pinOffset(x, loX, hiX), 0.0f);
    }
    if (y < loY && hiY > loY && axisResistanceEnabled(sp, false)) {
        float raw = y - loY;
        float offset = loY + rubberDistance(raw, viewH, (*sp).overscrollExtentY,
                                             (*sp).rubberCoefficientY);
        commitAxisOffset(sp, false, offset, raw);
    } else if (y > hiY && hiY > loY && axisResistanceEnabled(sp, false)) {
        float raw = y - hiY;
        float offset = hiY + rubberDistance(raw, viewH, (*sp).overscrollExtentY,
                                             (*sp).rubberCoefficientY);
        commitAxisOffset(sp, false, offset, raw);
    } else {
        commitAxisOffset(sp, false, pinOffset(y, loY, hiY), 0.0f);
    }
    placeContent(sp);
    ScrollPanel_syncToBars(sp);
    noteBarsScrolled(sp, nowMs);
}

void ScrollPanel_scrollInputAt(ScrollPanel *sp, float dx, float dy, uint64_t nowMs) {
    if (!sp)
        return;
    ScrollPanel_directChange(sp, dx, dy, nowMs);
    ScrollPanel_directEnd(sp, nowMs);
}

static void applyAxisDelta(ScrollPanel *sp, bool horizontal, float delta) {
    float loX = 0.0f, hiX = 0.0f, loY = 0.0f, hiY = 0.0f;
    offsetBounds(sp, &loX, &hiX, &loY, &hiY);
    float lo = horizontal ? loX : loY;
    float hi = horizontal ? hiX : hiY;
    float offset = horizontal ? (*sp).offsetX : (*sp).offsetY;
    float raw = horizontal ? (*sp).rawPullX : (*sp).rawPullY;
    Panel *base = &(*sp).base;
    float viewport = horizontal ? Component_getWidth(&(*base).component)
                                : Component_getHeight(&(*base).component);
    float extent = horizontal ? (*sp).overscrollExtentX : (*sp).overscrollExtentY;
    float coefficient = horizontal ? (*sp).rubberCoefficientX : (*sp).rubberCoefficientY;
    if (hi <= lo) {
        commitAxisOffset(sp, horizontal, lo, 0.0f);
        return;
    }
    if (raw != 0.0f && !axisResistanceEnabled(sp, horizontal)) {
        offset = raw < 0.0f ? lo : hi;
        raw = 0.0f;
        commitAxisOffset(sp, horizontal, offset, raw);
    }
    if (raw != 0.0f) {
        bool reversing = (raw > 0.0f && delta < 0.0f) || (raw < 0.0f && delta > 0.0f);
        if (reversing) {
            float next = finiteFloat((double) raw + (double) delta);
            bool crossed = (raw > 0.0f && next < 0.0f) || (raw < 0.0f && next > 0.0f);
            if (!crossed) {
                raw = next;
                float edge = raw < 0.0f ? lo : hi;
                commitAxisOffset(sp, horizontal,
                                 edge + rubberDistance(raw, viewport, extent, coefficient), raw);
                return;
            }
            delta = next;
            raw = 0.0f;
            offset = delta < 0.0f ? hi : lo;
        } else {
            raw = finiteFloat((double) raw + (double) delta);
            float edge = raw < 0.0f ? lo : hi;
            commitAxisOffset(sp, horizontal,
                             edge + rubberDistance(raw, viewport, extent, coefficient), raw);
            return;
        }
    }
    float next = finiteFloat((double) offset + (double) delta);
    if (next >= lo && next <= hi) {
        commitAxisOffset(sp, horizontal, next, 0.0f);
        return;
    }
    float edge = next < lo ? lo : hi;
    float excess = next - edge;
    if (axisResistanceEnabled(sp, horizontal)) {
        raw = excess;
        commitAxisOffset(sp, horizontal,
                         edge + rubberDistance(raw, viewport, extent, coefficient), raw);
    } else {
        commitAxisOffset(sp, horizontal, edge, 0.0f);
    }
}

static float sampledVelocity(float oldVelocity, float delta, uint64_t elapsedMs, float tauMs) {
    if (elapsedMs == 0u)
        return oldVelocity;
    double instantWide = (double) delta * 1000.0 / (double) elapsedMs;
    float instant = finiteFloat(instantWide);
    if (tauMs <= 0.0f)
        return instant;
    double alpha = 1.0 - exp(-(double) elapsedMs / (double) tauMs);
    return finiteFloat((double) oldVelocity + ((double) instant - (double) oldVelocity) * alpha);
}

static void setAxisMotionWriter(ScrollPanel *sp, bool horizontal, int writer) {
    if (horizontal)
        (*sp).motionWriterX = writer;
    else
        (*sp).motionWriterY = writer;
}

static int getAxisMotionWriter(const ScrollPanel *sp, bool horizontal) {
    return horizontal ? (*sp).motionWriterX : (*sp).motionWriterY;
}

static void transitionReleasedAxis(ScrollPanel *sp, bool horizontal, bool allowSynthetic) {
    float raw = horizontal ? (*sp).rawPullX : (*sp).rawPullY;
    float velocity = horizontal ? (*sp).velocityX : (*sp).velocityY;
    float omega = horizontal ? (*sp).springOmegaX : (*sp).springOmegaY;
    if (raw != 0.0f && axisResistanceEnabled(sp, horizontal)) {
        // Outward release velocity would first deepen the pull. The spring
        // owns return, so retain only inward velocity and clamp it to the
        // fastest no-cross critical trajectory.
        if ((raw > 0.0f && velocity > 0.0f) || (raw < 0.0f && velocity < 0.0f))
            velocity = 0.0f;
        float crossingLimit = finiteFloat(-(double) omega * (double) raw);
        if (raw > 0.0f && velocity < crossingLimit)
            velocity = crossingLimit;
        if (raw < 0.0f && velocity > crossingLimit)
            velocity = crossingLimit;
        if (horizontal) {
            (*sp).springVelocityX = velocity;
            (*sp).velocityX = 0.0f;
        } else {
            (*sp).springVelocityY = velocity;
            (*sp).velocityY = 0.0f;
        }
        setAxisMotionWriter(sp, horizontal, SCROLLPANEL_MOTION_SPRING);
        if (horizontal)
            (*sp).springConsumedMomentumX = true;
        else
            (*sp).springConsumedMomentumY = true;
        return;
    }
    ScrollBar *bar = horizontal ? (*sp).hBar : (*sp).vBar;
    int mode = bar ? ScrollBar_getScrollMode(bar) : SCROLL_BAR_STEP;
    float friction = bar ? ScrollBar_getScrollFriction(bar) : 0.0f;
    if (allowSynthetic && mode != SCROLL_BAR_STEP && friction > 0.0f && velocity != 0.0f) {
        setAxisMotionWriter(sp, horizontal, SCROLLPANEL_MOTION_SYNTHETIC);
        return;
    }
    if (horizontal) {
        (*sp).velocityX = 0.0f;
        (*sp).springVelocityX = 0.0f;
    } else {
        (*sp).velocityY = 0.0f;
        (*sp).springVelocityY = 0.0f;
    }
    setAxisMotionWriter(sp, horizontal, SCROLLPANEL_MOTION_IDLE);
}

void ScrollPanel_directBegin(ScrollPanel *sp, uint64_t nowMs) {
    if (!sp)
        return;
    (*sp).gestureHeld = true;
    (*sp).nativeMomentum = false;
    (*sp).springConsumedMomentumX = false;
    (*sp).springConsumedMomentumY = false;
    (*sp).velocityX = 0.0f;
    (*sp).velocityY = 0.0f;
    (*sp).lastInputXMs = nowMs;
    (*sp).lastInputYMs = nowMs;
    (*sp).motionWriterX = SCROLLPANEL_MOTION_DIRECT;
    (*sp).motionWriterY = SCROLLPANEL_MOTION_DIRECT;
}

void ScrollPanel_directChange(ScrollPanel *sp, float dx, float dy, uint64_t nowMs) {
    if (!sp || !isfinite(dx) || !isfinite(dy))
        return;
    float sx = finiteFloat((double) dx
        * (double) ScrollPanel_horizontalScroll_getScrollSensitivity(sp));
    float sy = finiteFloat((double) dy
        * (double) ScrollPanel_verticalScroll_getScrollSensitivity(sp));
    uint64_t dtX = nowMs >= (*sp).lastInputXMs ? nowMs - (*sp).lastInputXMs : 0u;
    uint64_t dtY = nowMs >= (*sp).lastInputYMs ? nowMs - (*sp).lastInputYMs : 0u;
    (*sp).velocityX = sampledVelocity((*sp).velocityX, sx, dtX, (*sp).velocitySampleTauMsX);
    (*sp).velocityY = sampledVelocity((*sp).velocityY, sy, dtY, (*sp).velocitySampleTauMsY);
    (*sp).lastInputXMs = nowMs;
    (*sp).lastInputYMs = nowMs;
    (*sp).motionWriterX = SCROLLPANEL_MOTION_DIRECT;
    (*sp).motionWriterY = SCROLLPANEL_MOTION_DIRECT;
    applyAxisDelta(sp, true, sx);
    applyAxisDelta(sp, false, sy);
    placeContent(sp);
    ScrollPanel_syncToBars(sp);
    noteBarsScrolled(sp, nowMs);
}

void ScrollPanel_directEnd(ScrollPanel *sp, uint64_t nowMs) {
    if (!sp)
        return;
    (*sp).gestureHeld = false;
    (*sp).lastTickMs = nowMs;
    transitionReleasedAxis(sp, true, true);
    transitionReleasedAxis(sp, false, true);
}

void ScrollPanel_nativeMomentumBegin(ScrollPanel *sp, uint64_t nowMs) {
    if (!sp)
        return;
    // The fingers are already up, so the contact hold is NOT reasserted. An
    // axis the spring already owns keeps rebounding; momentum owns only the
    // axes still inside their bounds.
    (*sp).nativeMomentum = true;
    (*sp).lastTickMs = nowMs;
    if ((*sp).motionWriterX != SCROLLPANEL_MOTION_SPRING) {
        (*sp).velocityX = 0.0f;
        (*sp).motionWriterX = SCROLLPANEL_MOTION_NATIVE;
    }
    if ((*sp).motionWriterY != SCROLLPANEL_MOTION_SPRING) {
        (*sp).velocityY = 0.0f;
        (*sp).motionWriterY = SCROLLPANEL_MOTION_NATIVE;
    }
}

void ScrollPanel_nativeMomentumChange(ScrollPanel *sp, float dx, float dy, uint64_t nowMs) {
    if (!sp || !(*sp).nativeMomentum || !isfinite(dx) || !isfinite(dy))
        return;
    float sx = finiteFloat((double) dx
        * (double) ScrollPanel_horizontalScroll_getScrollSensitivity(sp));
    float sy = finiteFloat((double) dy
        * (double) ScrollPanel_verticalScroll_getScrollSensitivity(sp));
    // A spring-owned axis ignores momentum (the rubber band is already pulling
    // it home); the other axis consumes its share as usual.
    if (!(*sp).springConsumedMomentumX && (*sp).motionWriterX != SCROLLPANEL_MOTION_SPRING)
        applyAxisDelta(sp, true, sx);
    if (!(*sp).springConsumedMomentumY && (*sp).motionWriterY != SCROLLPANEL_MOTION_SPRING)
        applyAxisDelta(sp, false, sy);
    // Momentum that carries an axis past the edge hands that axis to the
    // spring now, so it rebounds instead of parking in mid-air until the tail
    // finally ends.
    if ((*sp).rawPullX != 0.0f && (*sp).motionWriterX != SCROLLPANEL_MOTION_SPRING)
        transitionReleasedAxis(sp, true, false);
    if ((*sp).rawPullY != 0.0f && (*sp).motionWriterY != SCROLLPANEL_MOTION_SPRING)
        transitionReleasedAxis(sp, false, false);
    placeContent(sp);
    ScrollPanel_syncToBars(sp);
    noteBarsScrolled(sp, nowMs);
}

void ScrollPanel_nativeMomentumEnd(ScrollPanel *sp, uint64_t nowMs) {
    if (!sp)
        return;
    (*sp).nativeMomentum = false;
    (*sp).lastTickMs = nowMs;
    // No synthetic continuation after native momentum: an overscrolled axis
    // keeps its existing spring trajectory; an in-bounds axis stops cleanly.
    if ((*sp).motionWriterX != SCROLLPANEL_MOTION_SPRING)
        transitionReleasedAxis(sp, true, false);
    if ((*sp).motionWriterY != SCROLLPANEL_MOTION_SPRING)
        transitionReleasedAxis(sp, false, false);
    (*sp).springConsumedMomentumX = false;
    (*sp).springConsumedMomentumY = false;
}

void ScrollPanel_cancelMotion(ScrollPanel *sp) {
    if (!sp)
        return;
    (*sp).nativeMomentum = false;
    (*sp).gestureHeld = false;
    (*sp).velocityX = 0.0f;
    (*sp).velocityY = 0.0f;
    transitionReleasedAxis(sp, true, false);
    transitionReleasedAxis(sp, false, false);
    (*sp).springConsumedMomentumX = false;
    (*sp).springConsumedMomentumY = false;
}

bool ScrollPanel_canScrollHorizontal(const ScrollPanel *sp, float dx) {
    if (!sp || dx == 0.0f)
        return false;
    float loX = 0.0f, hiX = 0.0f;
    offsetBounds(sp, &loX, &hiX, nullptr, nullptr);
    if (hiX <= loX)
        return false;
    if (axisResistanceEnabled(sp, true))
        return true;
    float current = pinOffset((*sp).offsetX, loX, hiX);
    return dx < 0.0f ? current > loX : current < hiX;
}

bool ScrollPanel_canScrollVertical(const ScrollPanel *sp, float dy) {
    if (!sp || dy == 0.0f)
        return false;
    float loY = 0.0f, hiY = 0.0f;
    offsetBounds(sp, nullptr, nullptr, &loY, &hiY);
    if (hiY <= loY)
        return false;
    if (axisResistanceEnabled(sp, false))
        return true;
    float current = pinOffset((*sp).offsetY, loY, hiY);
    return dy < 0.0f ? current > loY : current < hiY;
}

bool ScrollPanel_canAcquire(const ScrollPanel *sp, float dx, float dy) {
    return ScrollPanel_canScrollHorizontal(sp, dx) || ScrollPanel_canScrollVertical(sp, dy);
}

// The bar's docked track + thumb rects, in viewport-local coordinates (the
// panel's own rect), derived through the R3 holder so docking has one home.
static void barGeometry(const ScrollPanel *sp, const ScrollBar *bar, bool horizontal,
                        Rectangle *trackOut, Rectangle *thumbOut) {
    Rectangle view;
    const Panel *b = &(*sp).base;
    view.x = 0.0f;
    view.y = 0.0f;
    view.width = Component_getWidth(&(*b).component);
    view.height = Component_getHeight(&(*b).component);
    ScrollBarGraphics g;
    ScrollBar_fillGraphics(bar, &g);
    if (horizontal)
        g.orientation = SCROLL_GRAPHICS_HORIZONTAL;
    else
        g.orientation = SCROLL_GRAPHICS_VERTICAL;
    ScrollBarGraphics_trackRect(&g, &view, trackOut);
    ScrollBarGraphics_thumbRect(&g, &view, thumbOut);
}

static bool barHit(const ScrollPanel *sp, const ScrollBar *bar, bool horizontal,
                   float localX, float localY) {
    if (!bar)
        return false;
    Rectangle track, thumb;
    barGeometry(sp, bar, horizontal, &track, &thumb);
    (void) thumb;
    float pad = SCROLLPANEL_DRAG_HIT_PAD;
    if (localX < track.x - pad || localX > track.x + track.width + pad)
        return false;
    if (localY < track.y - pad || localY > track.y + track.height + pad)
        return false;
    return true;
}

static bool barBeginDrag(ScrollPanel *sp, ScrollBar *bar, bool horizontal, float localX, float localY) {
    Rectangle track, thumb;
    barGeometry(sp, bar, horizontal, &track, &thumb);
    float trackLen = horizontal ? track.width : track.height;
    float thumbLen = horizontal ? thumb.width : thumb.height;
    float trackPos = horizontal ? (localX - track.x) : (localY - track.y);
    return ScrollBar_beginDrag(bar, trackLen, trackPos, thumbLen);
}

static void barDragTo(ScrollPanel *sp, ScrollBar *bar, bool horizontal, float localX, float localY) {
    Rectangle track, thumb;
    barGeometry(sp, bar, horizontal, &track, &thumb);
    float trackLen = horizontal ? track.width : track.height;
    float thumbLen = horizontal ? thumb.width : thumb.height;
    float trackPos = horizontal ? (localX - track.x) : (localY - track.y);
    ScrollBar_dragTo(bar, trackLen, trackPos, thumbLen);
}

static void cancelMotionForBarDrag(ScrollPanel *sp) {
    (*sp).nativeMomentum = false;
    (*sp).springConsumedMomentumX = false;
    (*sp).springConsumedMomentumY = false;
    (*sp).velocityX = 0.0f;
    (*sp).velocityY = 0.0f;
    (*sp).springVelocityX = 0.0f;
    (*sp).springVelocityY = 0.0f;
    (*sp).motionWriterX = SCROLLPANEL_MOTION_IDLE;
    (*sp).motionWriterY = SCROLLPANEL_MOTION_IDLE;
}

bool ScrollPanel_barDragBegin(ScrollPanel *sp, float localX, float localY) {
    if (!sp)
        return false;
    // A non-grappable bar ignores the grab entirely (read-only chrome).
    if ((*sp).vBar && ScrollBar_isGrappable((*sp).vBar)
        && barHit(sp, (*sp).vBar, false, localX, localY)) {
        (*sp).dragAxis = 0;
        if (barBeginDrag(sp, (*sp).vBar, false, localX, localY)) {
            cancelMotionForBarDrag(sp);
            (*sp).gestureHeld = true;
            ScrollPanel_syncFromBars(sp);
            return true;
        }
    }
    if ((*sp).hBar && ScrollBar_isGrappable((*sp).hBar)
        && barHit(sp, (*sp).hBar, true, localX, localY)) {
        (*sp).dragAxis = 1;
        if (barBeginDrag(sp, (*sp).hBar, true, localX, localY)) {
            cancelMotionForBarDrag(sp);
            (*sp).gestureHeld = true;
            ScrollPanel_syncFromBars(sp);
            return true;
        }
    }
    (*sp).dragAxis = -1;
    return false;
}

void ScrollPanel_barDragTo(ScrollPanel *sp, float localX, float localY) {
    if (!sp || (*sp).dragAxis < 0)
        return;
    if ((*sp).dragAxis == 0)
        barDragTo(sp, (*sp).vBar, false, localX, localY);
    else
        barDragTo(sp, (*sp).hBar, true, localX, localY);
    ScrollPanel_syncFromBars(sp);
}

void ScrollPanel_barDragEnd(ScrollPanel *sp) {
    if (!sp)
        return;
    if ((*sp).dragAxis == 0 && (*sp).vBar)
        ScrollBar_endDrag((*sp).vBar);
    else if ((*sp).dragAxis == 1 && (*sp).hBar)
        ScrollBar_endDrag((*sp).hBar);
    (*sp).dragAxis = -1;
    (*sp).gestureHeld = false;   // release lets gravity pull home
}

bool ScrollPanel_isBarDragging(const ScrollPanel *sp) {
    return sp ? ((*sp).dragAxis >= 0) : false;
}

void ScrollPanel_scrollBy(ScrollPanel *sp, float dx, float dy) {
    if (!sp)
        return;
    ScrollPanel_scrollByAt(sp, dx, dy, (*sp).lastTickMs);
}

void ScrollPanel_scrollByAt(ScrollPanel *sp, float dx, float dy, uint64_t nowMs) {
    if (!sp)
        return;
    ScrollPanel_setOffsetAt(sp, (*sp).offsetX + dx, (*sp).offsetY + dy, nowMs);
}

void ScrollPanel_tick(ScrollPanel *sp, uint64_t nowMs) {
    if (!sp)
        return;
    uint64_t dt = nowMs >= (*sp).lastTickMs ? nowMs - (*sp).lastTickMs : 0u;
    (*sp).lastTickMs = nowMs;
    if (dt > 0u && !(*sp).gestureHeld) {
        float seconds = (float) dt / 1000.0f;
        bool changed = false;
        for (int i = 0; i < 2; i++) {
            bool horizontal = i == 0;
            int writer = getAxisMotionWriter(sp, horizontal);
            if (writer == SCROLLPANEL_MOTION_SPRING) {
                float raw = horizontal ? (*sp).rawPullX : (*sp).rawPullY;
                float springVelocity = horizontal ? (*sp).springVelocityX
                                                   : (*sp).springVelocityY;
                float omega = horizontal ? (*sp).springOmegaX : (*sp).springOmegaY;
                float snapDistance = horizontal ? (*sp).springSnapDistanceX
                                                : (*sp).springSnapDistanceY;
                float snapVelocity = horizontal ? (*sp).springSnapVelocityX
                                                : (*sp).springSnapVelocityY;
                float loX = 0.0f, hiX = 0.0f, loY = 0.0f, hiY = 0.0f;
                offsetBounds(sp, &loX, &hiX, &loY, &hiY);
                float lo = horizontal ? loX : loY;
                float hi = horizontal ? hiX : hiY;
                double exponent = (double) omega * (double) seconds;
                double decay = exp(-exponent);
                if (!isfinite(exponent) || decay == 0.0) {
                    float edge = raw < 0.0f ? lo : hi;
                    commitAxisOffset(sp, horizontal, edge, 0.0f);
                    if (horizontal)
                        (*sp).springVelocityX = 0.0f;
                    else
                        (*sp).springVelocityY = 0.0f;
                    setAxisMotionWriter(sp, horizontal, SCROLLPANEL_MOTION_IDLE);
                    changed = true;
                    continue;
                }
                double c2 = (double) springVelocity + (double) omega * (double) raw;
                float nextRaw = finiteFloat(((double) raw + c2 * (double) seconds) * decay);
                float nextVelocity = finiteFloat(((double) springVelocity
                    - (double) omega * c2 * (double) seconds) * decay);
                Panel *base = &(*sp).base;
                float viewport = horizontal ? Component_getWidth(&(*base).component)
                                            : Component_getHeight(&(*base).component);
                float extent = horizontal ? (*sp).overscrollExtentX : (*sp).overscrollExtentY;
                float coefficient = horizontal ? (*sp).rubberCoefficientX
                                                : (*sp).rubberCoefficientY;
                float distance = rubberDistance(nextRaw, viewport, extent, coefficient);
                float distanceMagnitude = distance < 0.0f ? -distance : distance;
                float velocityMagnitude = nextVelocity < 0.0f ? -nextVelocity : nextVelocity;
                if (distanceMagnitude <= snapDistance && velocityMagnitude <= snapVelocity) {
                    float edge = raw < 0.0f ? lo : hi;
                    commitAxisOffset(sp, horizontal, edge, 0.0f);
                    nextVelocity = 0.0f;
                    setAxisMotionWriter(sp, horizontal, SCROLLPANEL_MOTION_IDLE);
                } else {
                    float edge = nextRaw < 0.0f ? lo : hi;
                    commitAxisOffset(sp, horizontal, edge + distance, nextRaw);
                }
                if (horizontal)
                    (*sp).springVelocityX = nextVelocity;
                else
                    (*sp).springVelocityY = nextVelocity;
                changed = true;
                continue;
            }
            if (writer == SCROLLPANEL_MOTION_SYNTHETIC) {
                // Fallback glide is only for phase-less wheels; native momentum
                // is authoritative and never also runs synthetic continuation.
                if ((*sp).nativeMomentum) {
                    setAxisMotionWriter(sp, horizontal, SCROLLPANEL_MOTION_IDLE);
                    continue;
                }
                float velocity = horizontal ? (*sp).velocityX : (*sp).velocityY;
                float tauMs = horizontal ? (*sp).decelerationTauMsX : (*sp).decelerationTauMsY;
                float stop = horizontal ? (*sp).stopVelocityX : (*sp).stopVelocityY;
                uint64_t inputMs = horizontal ? (*sp).lastInputXMs : (*sp).lastInputYMs;
                ScrollBar *bar = horizontal ? (*sp).hBar : (*sp).vBar;
                float friction = bar ? ScrollBar_getScrollFriction(bar) : 0.0f;
                uint64_t delayMs = bar ? ScrollBar_getScrollDelay(bar) : 0u;
                bool waiting = nowMs >= inputMs && nowMs - inputMs < delayMs;
                if (waiting)
                    continue;
                double effectiveTauMs = (double) tauMs * (double) friction;
                double factor = effectiveTauMs > 0.0
                    ? exp(-(double) dt / effectiveTauMs) : 0.0;
                double deltaWide = effectiveTauMs > 0.0
                    ? (double) velocity * (effectiveTauMs / 1000.0) * (1.0 - factor)
                    : (double) velocity * (double) seconds;
                float delta = finiteFloat(deltaWide);
                applyAxisDelta(sp, horizontal, delta);
                velocity = finiteFloat((double) velocity * factor);
                if (horizontal)
                    (*sp).velocityX = velocity;
                else
                    (*sp).velocityY = velocity;
                float raw = horizontal ? (*sp).rawPullX : (*sp).rawPullY;
                if (raw != 0.0f) {
                    transitionReleasedAxis(sp, horizontal, false);
                } else {
                    float magnitude = velocity < 0.0f ? -velocity : velocity;
                    if (magnitude < stop) {
                        if (horizontal)
                            (*sp).velocityX = 0.0f;
                        else
                            (*sp).velocityY = 0.0f;
                        setAxisMotionWriter(sp, horizontal, SCROLLPANEL_MOTION_IDLE);
                    }
                }
                changed = true;
            }
        }
        if (changed) {
            placeContent(sp);
            ScrollPanel_syncToBars(sp);
            // Demand-on-motion: a moved offset requests ONE coalesced repaint of
            // the window, so a scroll/spring animates with no caller polling.
            GraphicsComponent_requestRepaint(&(*sp).base.component);
        }
    }
    float loX = 0.0f, hiX = 0.0f, loY = 0.0f, hiY = 0.0f;
    offsetBounds(sp, &loX, &hiX, &loY, &hiY);
    bool scrollH = hiX > 0.0f;
    bool scrollV = hiY > 0.0f;
    if ((*sp).hBar)
        ScrollBar_tick((*sp).hBar, nowMs, scrollH);
    if ((*sp).vBar)
        ScrollBar_tick((*sp).vBar, nowMs, scrollV);
    applyBarVisible(sp);
}

void ScrollPanel_setViewportSize(ScrollPanel *sp, float w, float h) {
    if (!sp || !isfinite(w) || !isfinite(h))
        return;
    Panel *self = &(*sp).base;
    GraphicsComponent_setSize(&(*self).component, w, h);
    ScrollPanel_layoutBars(sp);
}

void ScrollPanel_setOverscrollLimit(ScrollPanel *sp, float px) {
    if (!sp || !isfinite(px))
        return;
    if (px < 0.0f)
        px = 0.0f;
    (*sp).overscrollExtentX = px;
    (*sp).overscrollExtentY = px;
    ScrollPanel_layoutBars(sp);
}

float ScrollPanel_getOverscrollLimit(const ScrollPanel *sp) {
    return sp ? (*sp).overscrollExtentY : 0.0f;
}

void ScrollPanel_setGestureHeld(ScrollPanel *sp, bool held) {
    if (!sp)
        return;
    (*sp).gestureHeld = held;
}

bool ScrollPanel_isGestureHeld(const ScrollPanel *sp) {
    return sp ? (*sp).gestureHeld : false;
}

void ScrollPanel_stopGlide(ScrollPanel *sp) {
    if (!sp)
        return;
    if ((*sp).hBar)
        ScrollBar_stopMomentum((*sp).hBar);
    if ((*sp).vBar)
        ScrollBar_stopMomentum((*sp).vBar);
    (*sp).velocityX = 0.0f;
    (*sp).velocityY = 0.0f;
    if ((*sp).motionWriterX == SCROLLPANEL_MOTION_SYNTHETIC)
        (*sp).motionWriterX = SCROLLPANEL_MOTION_IDLE;
    if ((*sp).motionWriterY == SCROLLPANEL_MOTION_SYNTHETIC)
        (*sp).motionWriterY = SCROLLPANEL_MOTION_IDLE;
}

void ScrollPanel_setContentSize(ScrollPanel *sp, float w, float h) {
    if (!sp || !isfinite(w) || !isfinite(h))
        return;
    // AUTO arms the hug for that dim (the flag persists); concrete clears it.
    (*sp).contentAutoW = Size_isAutoF(w);
    (*sp).contentAutoH = Size_isAutoF(h);
    if ((*sp).contentPanel)
        GraphicsComponent_setSize(&(*(*sp).contentPanel).component, w, h);
    ScrollPanel_layoutBars(sp);
}

// The per-step entry: resolve AUTO content, re-dock both bars from the live
// viewport abs, re-clamp the offsets, re-sync the bar values. Idempotent and
// safe to run on every live-resize step.
void ScrollPanel_layoutBars(ScrollPanel *sp) {
    if (!sp)
        return;
    resolveContentAuto(sp);
    dockBar(sp, (*sp).hBar, true);
    dockBar(sp, (*sp).vBar, false);
    float loX = 0.0f, hiX = 0.0f, loY = 0.0f, hiY = 0.0f;
    offsetBounds(sp, &loX, &hiX, &loY, &hiY);
    bool hadRawX = (*sp).rawPullX != 0.0f;
    bool hadRawY = (*sp).rawPullY != 0.0f;
    Panel *base = &(*sp).base;
    float viewW = Component_getWidth(&(*base).component);
    float viewH = Component_getHeight(&(*base).component);
    if ((*sp).rawPullX != 0.0f && hiX > loX && axisResistanceEnabled(sp, true)) {
        float edge = (*sp).rawPullX < 0.0f ? loX : hiX;
        float offset = edge + rubberDistance((*sp).rawPullX, viewW,
            (*sp).overscrollExtentX, (*sp).rubberCoefficientX);
        commitAxisOffset(sp, true, offset, (*sp).rawPullX);
    } else {
        commitAxisOffset(sp, true, pinOffset((*sp).offsetX, loX, hiX), 0.0f);
        if (hadRawX) {
            (*sp).springVelocityX = 0.0f;
            setAxisMotionWriter(sp, true, SCROLLPANEL_MOTION_IDLE);
        }
    }
    if ((*sp).rawPullY != 0.0f && hiY > loY && axisResistanceEnabled(sp, false)) {
        float edge = (*sp).rawPullY < 0.0f ? loY : hiY;
        float offset = edge + rubberDistance((*sp).rawPullY, viewH,
            (*sp).overscrollExtentY, (*sp).rubberCoefficientY);
        commitAxisOffset(sp, false, offset, (*sp).rawPullY);
    } else {
        commitAxisOffset(sp, false, pinOffset((*sp).offsetY, loY, hiY), 0.0f);
        if (hadRawY) {
            (*sp).springVelocityY = 0.0f;
            setAxisMotionWriter(sp, false, SCROLLPANEL_MOTION_IDLE);
        }
    }
    placeContent(sp);
    float contentW = viewW;
    float contentH = viewH;
    if ((*sp).contentPanel) {
        Panel *content = (*sp).contentPanel;
        contentW = Component_getWidth(&(*content).component);
        contentH = Component_getHeight(&(*content).component);
    }
    if ((*sp).hBar)
        ScrollBar_setLengths((*sp).hBar, viewW, contentW);
    if ((*sp).vBar)
        ScrollBar_setLengths((*sp).vBar, viewH, contentH);
    ScrollPanel_syncToBars(sp);
}

void ScrollPanel_syncToBars(ScrollPanel *sp) {
    if (!sp)
        return;
    float loX = 0.0f, hiX = 0.0f, loY = 0.0f, hiY = 0.0f;
    offsetBounds(sp, &loX, &hiX, &loY, &hiY);
    ScrollBar *bars[2] = { (*sp).hBar, (*sp).vBar };
    float offsets[2] = { (*sp).offsetX, (*sp).offsetY };
    float los[2] = { loX, loY };
    float his[2] = { hiX, hiY };
    for (int i = 0; i < 2; i++) {
        ScrollBar *bar = bars[i];
        if (bar == nullptr)
            continue;
        float extent = his[i] - los[i];
        float t = extent > 0.0f ? (offsets[i] - los[i]) / extent : 0.0f;
        if (!isfinite(t))
            t = 0.0f;
        if (t < 0.0f)
            t = 0.0f;
        if (t > 1.0f)
            t = 1.0f;
        float bmin = 0.0f, bmax = 1.0f;
        ScrollBar_getRange(bar, &bmin, &bmax);
        if (!isfinite(bmin) || !isfinite(bmax)) {
            bmin = 0.0f;
            bmax = 1.0f;
        }
        ScrollBar_setValue(bar, finiteFloat((double) bmin
            + (double) t * ((double) bmax - (double) bmin)));
    }
}

void ScrollPanel_syncFromBars(ScrollPanel *sp) {
    if (!sp)
        return;
    float loX = 0.0f, hiX = 0.0f, loY = 0.0f, hiY = 0.0f;
    offsetBounds(sp, &loX, &hiX, &loY, &hiY);
    ScrollBar *bars[2] = { (*sp).hBar, (*sp).vBar };
    float los[2] = { loX, loY };
    float his[2] = { hiX, hiY };
    for (int i = 0; i < 2; i++) {
        ScrollBar *bar = bars[i];
        if (bar == nullptr)
            continue;
        float bmin = 0.0f, bmax = 1.0f;
        ScrollBar_getRange(bar, &bmin, &bmax);
        float barValue = ScrollBar_getValue(bar);
        if (!isfinite(bmin) || !isfinite(bmax) || !isfinite(barValue))
            continue;
        double span = (double) bmax - (double) bmin;
        float t = span != 0.0
            ? finiteFloat(((double) barValue - (double) bmin) / span) : 0.0f;
        if (t < 0.0f)
            t = 0.0f;
        if (t > 1.0f)
            t = 1.0f;
        double extent = (double) his[i] - (double) los[i];
        float offset = finiteFloat((double) los[i] + (double) t * extent);
        commitAxisOffset(sp, i == 0, offset, 0.0f);
    }
    placeContent(sp);
    noteBarsScrolled(sp, (*sp).lastTickMs);
}

// VERTICALSCROLL PART VERBS (PUBLIC)

;;SETTER
void ScrollPanel_verticalScroll_setThickness(ScrollPanel *sp, float px) {
    if (!sp || !(*sp).vBar || !isfinite(px))
        return;
    ScrollBar_setThickness((*sp).vBar, px);
    ScrollPanel_layoutBars(sp);
}

;;SETTER
void ScrollPanel_verticalScroll_setInset(ScrollPanel *sp, float px) {
    if (!sp || !(*sp).vBar || !isfinite(px))
        return;
    ScrollBar_setInset((*sp).vBar, px);
    ScrollPanel_layoutBars(sp);
}

;;SETTER
void ScrollPanel_verticalScroll_setVisible(ScrollPanel *sp, bool visible) {
    if (!sp)
        return;
    (*sp).vVisible = visible;
    applyBarVisible(sp);
}

;;SETTER
void ScrollPanel_verticalScroll_setRange(ScrollPanel *sp, float min, float max) {
    if (!sp || !(*sp).vBar || !isfinite(min) || !isfinite(max))
        return;
    ScrollBar_setRange((*sp).vBar, min, max);
    ScrollPanel_syncToBars(sp);
}

;;SETTER
void ScrollPanel_verticalScroll_setValue(ScrollPanel *sp, float value) {
    if (!sp || !(*sp).vBar || !isfinite(value))
        return;
    ScrollBar_setValue((*sp).vBar, value);
    ScrollPanel_syncFromBars(sp);
}

;;GETTER
float ScrollPanel_verticalScroll_getValue(const ScrollPanel *sp) {
    return (sp && (*sp).vBar) ? ScrollBar_getValue((*sp).vBar) : 0.0f;
}

;;GETTER
float ScrollPanel_verticalScroll_getThickness(const ScrollPanel *sp) {
    return (sp && (*sp).vBar) ? ScrollBar_getThickness((*sp).vBar) : 0.0f;
}

;;GETTER
float ScrollPanel_verticalScroll_getInset(const ScrollPanel *sp) {
    return (sp && (*sp).vBar) ? ScrollBar_getInset((*sp).vBar) : 0.0f;
}

;;SETTER
void ScrollPanel_verticalScroll_setShortLengthLimit(ScrollPanel *sp, float percent) {
    if (!sp || !(*sp).vBar || !isfinite(percent))
        return;
    ScrollBar_setShortLengthLimit((*sp).vBar, percent);
}

;;SETTER
void ScrollPanel_verticalScroll_setHideWhenUnused(ScrollPanel *sp, bool hide) {
    if (!sp || !(*sp).vBar)
        return;
    ScrollBar_setHideWhenUnused((*sp).vBar, hide);
    applyBarVisible(sp);
}

;;SETTER
void ScrollPanel_verticalScroll_setOpacity(ScrollPanel *sp, float opacity) {
    if (!sp || !(*sp).vBar || !isfinite(opacity))
        return;
    ScrollBar_setOpacity((*sp).vBar, opacity);
}

;;SETTER
void ScrollPanel_verticalScroll_setIdleTimeoutMs(ScrollPanel *sp, uint64_t timeoutMs) {
    if (!sp || !(*sp).vBar)
        return;
    ScrollBar_setIdleTimeoutMs((*sp).vBar, timeoutMs);
}

;;GETTER
float ScrollPanel_verticalScroll_getShortLengthLimit(const ScrollPanel *sp) {
    return (sp && (*sp).vBar) ? ScrollBar_getShortLengthLimit((*sp).vBar) : 0.0f;
}

;;GETTER
bool ScrollPanel_verticalScroll_isHideWhenUnused(const ScrollPanel *sp) {
    return (sp && (*sp).vBar) ? ScrollBar_isHideWhenUnused((*sp).vBar) : false;
}

;;GETTER
float ScrollPanel_verticalScroll_getOpacity(const ScrollPanel *sp) {
    return (sp && (*sp).vBar) ? ScrollBar_getOpacity((*sp).vBar) : 0.0f;
}

;;GETTER
uint64_t ScrollPanel_verticalScroll_getIdleTimeoutMs(const ScrollPanel *sp) {
    return (sp && (*sp).vBar) ? ScrollBar_getIdleTimeoutMs((*sp).vBar) : 0u;
}

;;GETTER
bool ScrollPanel_verticalScroll_isEffectiveVisible(const ScrollPanel *sp) {
    if (!sp || !(*sp).vBar)
        return false;
    if (!(*sp).vVisible)
        return false;
    return ScrollBar_isEffectiveVisible((*sp).vBar);
}

;;SETTER
void ScrollPanel_verticalScroll_setScrollMode(ScrollPanel *sp, int mode) {
    if (!sp || !(*sp).vBar)
        return;
    ScrollBar_setScrollMode((*sp).vBar, mode);
    ScrollPanel_layoutBars(sp);
}

;;SETTER
void ScrollPanel_verticalScroll_setScrollFriction(ScrollPanel *sp, float friction) {
    if (!sp || !(*sp).vBar || !isfinite(friction))
        return;
    ScrollBar_setScrollFriction((*sp).vBar, friction);
}

;;SETTER
void ScrollPanel_verticalScroll_setScrollSensitivity(ScrollPanel *sp, float sensitivity) {
    if (!sp || !(*sp).vBar || !isfinite(sensitivity))
        return;
    ScrollBar_setScrollSensitivity((*sp).vBar, sensitivity);
}

;;SETTER
void ScrollPanel_verticalScroll_setScrollDelay(ScrollPanel *sp, uint64_t delayMs) {
    if (!sp || !(*sp).vBar)
        return;
    ScrollBar_setScrollDelay((*sp).vBar, delayMs);
}

;;GETTER
int ScrollPanel_verticalScroll_getScrollMode(const ScrollPanel *sp) {
    return (sp && (*sp).vBar) ? ScrollBar_getScrollMode((*sp).vBar) : SCROLL_BAR_MODE_DEFAULT;
}

;;GETTER
float ScrollPanel_verticalScroll_getScrollFriction(const ScrollPanel *sp) {
    return (sp && (*sp).vBar) ? ScrollBar_getScrollFriction((*sp).vBar) : SCROLL_BAR_FRICTION_DEFAULT;
}

;;GETTER
float ScrollPanel_verticalScroll_getScrollSensitivity(const ScrollPanel *sp) {
    return (sp && (*sp).vBar) ? ScrollBar_getScrollSensitivity((*sp).vBar) : SCROLL_BAR_SENSITIVITY_DEFAULT;
}

;;GETTER
uint64_t ScrollPanel_verticalScroll_getScrollDelay(const ScrollPanel *sp) {
    return (sp && (*sp).vBar) ? ScrollBar_getScrollDelay((*sp).vBar) : SCROLL_BAR_DELAY_MS_DEFAULT;
}

;;SETTER
void ScrollPanel_verticalScroll_setFadeOutMs(ScrollPanel *sp, uint64_t fadeOutMs) {
    if (!sp || !(*sp).vBar)
        return;
    ScrollBar_setFadeOutMs((*sp).vBar, fadeOutMs);
}

;;SETTER
void ScrollPanel_verticalScroll_setGrappable(ScrollPanel *sp, bool grappable) {
    if (!sp || !(*sp).vBar)
        return;
    ScrollBar_setGrappable((*sp).vBar, grappable);
    if (!grappable && (*sp).dragAxis == 0) {
        ScrollBar_endDrag((*sp).vBar);
        (*sp).dragAxis = -1;
        (*sp).gestureHeld = false;
    }
}

;;GETTER
uint64_t ScrollPanel_verticalScroll_getFadeOutMs(const ScrollPanel *sp) {
    return (sp && (*sp).vBar) ? ScrollBar_getFadeOutMs((*sp).vBar) : SCROLL_BAR_FADE_MS_DEFAULT;
}

;;GETTER
bool ScrollPanel_verticalScroll_isGrappable(const ScrollPanel *sp) {
    return (sp && (*sp).vBar) ? ScrollBar_isGrappable((*sp).vBar) : false;
}

;;GETTER
bool ScrollPanel_verticalScroll_isNeeded(const ScrollPanel *sp) {
    return (sp && (*sp).vBar) ? ScrollBar_isNeeded((*sp).vBar) : false;
}

;;GETTER
void ScrollPanel_verticalScroll_getRange(const ScrollPanel *sp, float *outMin, float *outMax) {
    if (sp && (*sp).vBar)
        ScrollBar_getRange((*sp).vBar, outMin, outMax);
    else {
        if (outMin) *outMin = 0.0f;
        if (outMax) *outMax = 1.0f;
    }
}

;;GETTER
void ScrollPanel_verticalScroll_getThumbRect(const ScrollPanel *sp,
                                             float *outX, float *outY, float *outW, float *outH) {
    Rectangle view;
    view.x = 0.0f;
    view.y = 0.0f;
    view.width = 0.0f;
    view.height = 0.0f;
    if (sp) {
        const Panel *b = &(*sp).base;
        view.width = Component_getWidth(&(*b).component);
        view.height = Component_getHeight(&(*b).component);
    }
    Rectangle dest;
    dest.x = 0.0f;
    dest.y = 0.0f;
    dest.width = 0.0f;
    dest.height = 0.0f;
    if (sp && (*sp).vBar)
        ScrollBar_thumbRect((*sp).vBar, &view, &dest);
    if (outX) *outX = dest.x;
    if (outY) *outY = dest.y;
    if (outW) *outW = dest.width;
    if (outH) *outH = dest.height;
}

// HORIZONTALSCROLL PART VERBS (PUBLIC)

;;SETTER
void ScrollPanel_horizontalScroll_setThickness(ScrollPanel *sp, float px) {
    if (!sp || !(*sp).hBar || !isfinite(px))
        return;
    ScrollBar_setThickness((*sp).hBar, px);
    ScrollPanel_layoutBars(sp);
}

;;SETTER
void ScrollPanel_horizontalScroll_setInset(ScrollPanel *sp, float px) {
    if (!sp || !(*sp).hBar || !isfinite(px))
        return;
    ScrollBar_setInset((*sp).hBar, px);
    ScrollPanel_layoutBars(sp);
}

;;SETTER
void ScrollPanel_horizontalScroll_setVisible(ScrollPanel *sp, bool visible) {
    if (!sp)
        return;
    (*sp).hVisible = visible;
    applyBarVisible(sp);
}

;;SETTER
void ScrollPanel_horizontalScroll_setRange(ScrollPanel *sp, float min, float max) {
    if (!sp || !(*sp).hBar || !isfinite(min) || !isfinite(max))
        return;
    ScrollBar_setRange((*sp).hBar, min, max);
    ScrollPanel_syncToBars(sp);
}

;;SETTER
void ScrollPanel_horizontalScroll_setValue(ScrollPanel *sp, float value) {
    if (!sp || !(*sp).hBar || !isfinite(value))
        return;
    ScrollBar_setValue((*sp).hBar, value);
    ScrollPanel_syncFromBars(sp);
}

;;GETTER
float ScrollPanel_horizontalScroll_getValue(const ScrollPanel *sp) {
    return (sp && (*sp).hBar) ? ScrollBar_getValue((*sp).hBar) : 0.0f;
}

;;GETTER
float ScrollPanel_horizontalScroll_getThickness(const ScrollPanel *sp) {
    return (sp && (*sp).hBar) ? ScrollBar_getThickness((*sp).hBar) : 0.0f;
}

;;GETTER
float ScrollPanel_horizontalScroll_getInset(const ScrollPanel *sp) {
    return (sp && (*sp).hBar) ? ScrollBar_getInset((*sp).hBar) : 0.0f;
}

;;SETTER
void ScrollPanel_horizontalScroll_setShortLengthLimit(ScrollPanel *sp, float percent) {
    if (!sp || !(*sp).hBar || !isfinite(percent))
        return;
    ScrollBar_setShortLengthLimit((*sp).hBar, percent);
}

;;SETTER
void ScrollPanel_horizontalScroll_setHideWhenUnused(ScrollPanel *sp, bool hide) {
    if (!sp || !(*sp).hBar)
        return;
    ScrollBar_setHideWhenUnused((*sp).hBar, hide);
    applyBarVisible(sp);
}

;;SETTER
void ScrollPanel_horizontalScroll_setOpacity(ScrollPanel *sp, float opacity) {
    if (!sp || !(*sp).hBar || !isfinite(opacity))
        return;
    ScrollBar_setOpacity((*sp).hBar, opacity);
}

;;SETTER
void ScrollPanel_horizontalScroll_setIdleTimeoutMs(ScrollPanel *sp, uint64_t timeoutMs) {
    if (!sp || !(*sp).hBar)
        return;
    ScrollBar_setIdleTimeoutMs((*sp).hBar, timeoutMs);
}

;;GETTER
float ScrollPanel_horizontalScroll_getShortLengthLimit(const ScrollPanel *sp) {
    return (sp && (*sp).hBar) ? ScrollBar_getShortLengthLimit((*sp).hBar) : 0.0f;
}

;;GETTER
bool ScrollPanel_horizontalScroll_isHideWhenUnused(const ScrollPanel *sp) {
    return (sp && (*sp).hBar) ? ScrollBar_isHideWhenUnused((*sp).hBar) : false;
}

;;GETTER
float ScrollPanel_horizontalScroll_getOpacity(const ScrollPanel *sp) {
    return (sp && (*sp).hBar) ? ScrollBar_getOpacity((*sp).hBar) : 0.0f;
}

;;GETTER
uint64_t ScrollPanel_horizontalScroll_getIdleTimeoutMs(const ScrollPanel *sp) {
    return (sp && (*sp).hBar) ? ScrollBar_getIdleTimeoutMs((*sp).hBar) : 0u;
}

;;GETTER
bool ScrollPanel_horizontalScroll_isEffectiveVisible(const ScrollPanel *sp) {
    if (!sp || !(*sp).hBar)
        return false;
    if (!(*sp).hVisible)
        return false;
    return ScrollBar_isEffectiveVisible((*sp).hBar);
}

;;SETTER
void ScrollPanel_horizontalScroll_setScrollMode(ScrollPanel *sp, int mode) {
    if (!sp || !(*sp).hBar)
        return;
    ScrollBar_setScrollMode((*sp).hBar, mode);
    ScrollPanel_layoutBars(sp);
}

;;SETTER
void ScrollPanel_horizontalScroll_setScrollFriction(ScrollPanel *sp, float friction) {
    if (!sp || !(*sp).hBar || !isfinite(friction))
        return;
    ScrollBar_setScrollFriction((*sp).hBar, friction);
}

;;SETTER
void ScrollPanel_horizontalScroll_setScrollSensitivity(ScrollPanel *sp, float sensitivity) {
    if (!sp || !(*sp).hBar || !isfinite(sensitivity))
        return;
    ScrollBar_setScrollSensitivity((*sp).hBar, sensitivity);
}

;;SETTER
void ScrollPanel_horizontalScroll_setScrollDelay(ScrollPanel *sp, uint64_t delayMs) {
    if (!sp || !(*sp).hBar)
        return;
    ScrollBar_setScrollDelay((*sp).hBar, delayMs);
}

;;GETTER
int ScrollPanel_horizontalScroll_getScrollMode(const ScrollPanel *sp) {
    return (sp && (*sp).hBar) ? ScrollBar_getScrollMode((*sp).hBar) : SCROLL_BAR_MODE_DEFAULT;
}

;;GETTER
float ScrollPanel_horizontalScroll_getScrollFriction(const ScrollPanel *sp) {
    return (sp && (*sp).hBar) ? ScrollBar_getScrollFriction((*sp).hBar) : SCROLL_BAR_FRICTION_DEFAULT;
}

;;GETTER
float ScrollPanel_horizontalScroll_getScrollSensitivity(const ScrollPanel *sp) {
    return (sp && (*sp).hBar) ? ScrollBar_getScrollSensitivity((*sp).hBar) : SCROLL_BAR_SENSITIVITY_DEFAULT;
}

;;GETTER
uint64_t ScrollPanel_horizontalScroll_getScrollDelay(const ScrollPanel *sp) {
    return (sp && (*sp).hBar) ? ScrollBar_getScrollDelay((*sp).hBar) : SCROLL_BAR_DELAY_MS_DEFAULT;
}

;;SETTER
void ScrollPanel_horizontalScroll_setFadeOutMs(ScrollPanel *sp, uint64_t fadeOutMs) {
    if (!sp || !(*sp).hBar)
        return;
    ScrollBar_setFadeOutMs((*sp).hBar, fadeOutMs);
}

;;SETTER
void ScrollPanel_horizontalScroll_setGrappable(ScrollPanel *sp, bool grappable) {
    if (!sp || !(*sp).hBar)
        return;
    ScrollBar_setGrappable((*sp).hBar, grappable);
    if (!grappable && (*sp).dragAxis == 1) {
        ScrollBar_endDrag((*sp).hBar);
        (*sp).dragAxis = -1;
        (*sp).gestureHeld = false;
    }
}

;;GETTER
uint64_t ScrollPanel_horizontalScroll_getFadeOutMs(const ScrollPanel *sp) {
    return (sp && (*sp).hBar) ? ScrollBar_getFadeOutMs((*sp).hBar) : SCROLL_BAR_FADE_MS_DEFAULT;
}

;;GETTER
bool ScrollPanel_horizontalScroll_isGrappable(const ScrollPanel *sp) {
    return (sp && (*sp).hBar) ? ScrollBar_isGrappable((*sp).hBar) : false;
}

;;GETTER
bool ScrollPanel_horizontalScroll_isNeeded(const ScrollPanel *sp) {
    return (sp && (*sp).hBar) ? ScrollBar_isNeeded((*sp).hBar) : false;
}

;;GETTER
void ScrollPanel_horizontalScroll_getRange(const ScrollPanel *sp, float *outMin, float *outMax) {
    if (sp && (*sp).hBar)
        ScrollBar_getRange((*sp).hBar, outMin, outMax);
    else {
        if (outMin) *outMin = 0.0f;
        if (outMax) *outMax = 1.0f;
    }
}

;;GETTER
void ScrollPanel_horizontalScroll_getThumbRect(const ScrollPanel *sp,
                                               float *outX, float *outY, float *outW, float *outH) {
    Rectangle view;
    view.x = 0.0f;
    view.y = 0.0f;
    view.width = 0.0f;
    view.height = 0.0f;
    if (sp) {
        const Panel *b = &(*sp).base;
        view.width = Component_getWidth(&(*b).component);
        view.height = Component_getHeight(&(*b).component);
    }
    Rectangle dest;
    dest.x = 0.0f;
    dest.y = 0.0f;
    dest.width = 0.0f;
    dest.height = 0.0f;
    if (sp && (*sp).hBar)
        ScrollBar_thumbRect((*sp).hBar, &view, &dest);
    if (outX) *outX = dest.x;
    if (outY) *outY = dest.y;
    if (outW) *outW = dest.width;
    if (outH) *outH = dest.height;
}

// CONTENTPANEL PART VERBS (PUBLIC)

;;SETTER
void ScrollPanel_contentPanel_setSize(ScrollPanel *sp, float w, float h) {
    if (!sp || !isfinite(w) || !isfinite(h))
        return;
    // AUTO arms the hug for that dim (the flag persists); concrete clears it.
    (*sp).contentAutoW = Size_isAutoF(w);
    (*sp).contentAutoH = Size_isAutoF(h);
    if ((*sp).contentPanel)
        GraphicsComponent_setSize(&(*(*sp).contentPanel).component, w, h);
    ScrollPanel_layoutBars(sp);
}

;;GETTER
void ScrollPanel_contentPanel_getSize(const ScrollPanel *sp, float *outW, float *outH) {
    float w = 0.0f, h = 0.0f;
    if (sp && (*sp).contentPanel) {
        w = Component_getWidth(&(*(*sp).contentPanel).component);
        h = Component_getHeight(&(*(*sp).contentPanel).component);
    }
    if (outW) *outW = w;
    if (outH) *outH = h;
}

;;SETTER
void ScrollPanel_contentPanel_setVisible(ScrollPanel *sp, bool visible) {
    if (!sp || !(*sp).contentPanel)
        return;
    GraphicsComponent_setVisible(&(*(*sp).contentPanel).component, visible);
}

// GETTERS (PUBLIC & PRIVATE)

;;GETTER
void ScrollPanel_getOffset(const ScrollPanel *sp, float *outX, float *outY) {
    float x = 0.0f, y = 0.0f;
    if (sp) {
        x = (*sp).offsetX;
        y = (*sp).offsetY;
    }
    if (outX) *outX = x;
    if (outY) *outY = y;
}

;;GETTER
Panel *ScrollPanel_getContentPanel(const ScrollPanel *sp) {
    return sp ? (*sp).contentPanel : nullptr;
}

;;GETTER
bool ScrollPanel_isContentAutoWidth(const ScrollPanel *sp) {
    return sp ? (*sp).contentAutoW : false;
}

;;GETTER
bool ScrollPanel_isContentAutoHeight(const ScrollPanel *sp) {
    return sp ? (*sp).contentAutoH : false;
}

// MOTION TUNING PART VERBS (PUBLIC)

#define DEFINE_AXIS_FLOAT_ACCESSORS(PREFIX, FIELD, STORAGE, DEFAULT_VALUE) \
    void ScrollPanel_##PREFIX##_set##FIELD(ScrollPanel *sp, float value) { \
        if (!sp || !isfinite(value)) \
            return; \
        if (value < 0.0f) \
            value = 0.0f; \
        (*sp).STORAGE = value; \
        ScrollPanel_layoutBars(sp); \
    } \
    float ScrollPanel_##PREFIX##_get##FIELD(const ScrollPanel *sp) { \
        return sp ? (*sp).STORAGE : DEFAULT_VALUE; \
    }

// The private macro stamps symmetric, null-safe part accessors. Its tokenized
// field names map directly to the mirrored struct fields above.
DEFINE_AXIS_FLOAT_ACCESSORS(verticalScroll, RubberCoefficient, rubberCoefficientY, SCROLLPANEL_RUBBER_COEFFICIENT_DEFAULT)
DEFINE_AXIS_FLOAT_ACCESSORS(verticalScroll, OverscrollExtent, overscrollExtentY, SCROLLPANEL_OVERSCROLL_LIMIT_DEFAULT)
DEFINE_AXIS_FLOAT_ACCESSORS(verticalScroll, VelocitySampleTauMs, velocitySampleTauMsY, SCROLLPANEL_VELOCITY_SAMPLE_TAU_MS_DEFAULT)
DEFINE_AXIS_FLOAT_ACCESSORS(verticalScroll, DecelerationTauMs, decelerationTauMsY, SCROLLPANEL_DECELERATION_TAU_MS_DEFAULT)
DEFINE_AXIS_FLOAT_ACCESSORS(verticalScroll, StopVelocity, stopVelocityY, SCROLLPANEL_STOP_VELOCITY_DEFAULT)
DEFINE_AXIS_FLOAT_ACCESSORS(verticalScroll, SpringOmega, springOmegaY, SCROLLPANEL_SPRING_OMEGA_DEFAULT)
DEFINE_AXIS_FLOAT_ACCESSORS(verticalScroll, SpringSnapDistance, springSnapDistanceY, SCROLLPANEL_SPRING_SNAP_PX_DEFAULT)
DEFINE_AXIS_FLOAT_ACCESSORS(verticalScroll, SpringSnapVelocity, springSnapVelocityY, SCROLLPANEL_SPRING_SNAP_VELOCITY_DEFAULT)
DEFINE_AXIS_FLOAT_ACCESSORS(horizontalScroll, RubberCoefficient, rubberCoefficientX, SCROLLPANEL_RUBBER_COEFFICIENT_DEFAULT)
DEFINE_AXIS_FLOAT_ACCESSORS(horizontalScroll, OverscrollExtent, overscrollExtentX, SCROLLPANEL_OVERSCROLL_LIMIT_DEFAULT)
DEFINE_AXIS_FLOAT_ACCESSORS(horizontalScroll, VelocitySampleTauMs, velocitySampleTauMsX, SCROLLPANEL_VELOCITY_SAMPLE_TAU_MS_DEFAULT)
DEFINE_AXIS_FLOAT_ACCESSORS(horizontalScroll, DecelerationTauMs, decelerationTauMsX, SCROLLPANEL_DECELERATION_TAU_MS_DEFAULT)
DEFINE_AXIS_FLOAT_ACCESSORS(horizontalScroll, StopVelocity, stopVelocityX, SCROLLPANEL_STOP_VELOCITY_DEFAULT)
DEFINE_AXIS_FLOAT_ACCESSORS(horizontalScroll, SpringOmega, springOmegaX, SCROLLPANEL_SPRING_OMEGA_DEFAULT)
DEFINE_AXIS_FLOAT_ACCESSORS(horizontalScroll, SpringSnapDistance, springSnapDistanceX, SCROLLPANEL_SPRING_SNAP_PX_DEFAULT)
DEFINE_AXIS_FLOAT_ACCESSORS(horizontalScroll, SpringSnapVelocity, springSnapVelocityX, SCROLLPANEL_SPRING_SNAP_VELOCITY_DEFAULT)

#undef DEFINE_AXIS_FLOAT_ACCESSORS

int ScrollPanel_verticalScroll_getMotionWriter(const ScrollPanel *sp) {
    return sp ? (*sp).motionWriterY : SCROLLPANEL_MOTION_IDLE;
}

int ScrollPanel_horizontalScroll_getMotionWriter(const ScrollPanel *sp) {
    return sp ? (*sp).motionWriterX : SCROLLPANEL_MOTION_IDLE;
}

// PAINT (PUBLIC)

// Generic subtree paint: each node paints its own stages into its
// accumulated rect, then children accumulate further. Locations chain from
// top-left anchors (the GraphicsComponent default), so accumulation is
// exact with no abs cascade needed. A node in skips is excluded (and its
// whole subtree) — the page paints nested ScrollPanels itself.
static bool nodeSkipped(const Panel *node, const Panel *const *skips, size_t skipCount) {
    for (size_t i = 0; i < skipCount; i++) {
        if (skips[i] == node)
            return true;
    }
    return false;
}

static void paintSubtree(Panel *node, float dx, float dy,
                         const Panel *const *skips, size_t skipCount) {
    if (!node || nodeSkipped(node, skips, skipCount))
        return;
    if (!Panel_isVisible(node))
        return;
    Component *c = &(*node).component;
    float x = GraphicsComponent_getX(c) + dx;
    float y = GraphicsComponent_getY(c) + dy;
    float w = Component_getWidth(c);
    float h = Component_getHeight(c);
    if (w <= 0.0f || h <= 0.0f)
        return;
    Rectangle r;
    r.x = x;
    r.y = y;
    r.width = w;
    r.height = h;
    Panel_paintParts(node, &r);
    size_t n = Panel_childCount(node);
    for (size_t i = 0; i < n; i++)
        paintSubtree(Panel_getChild(node, i), x, y, skips, skipCount);
}

// The R4-to-R3 handoff maps the panel's two bars into the R3 chrome holder
// (which docks + paints both bars). The panel background stays R4
// (Panel_paintParts); the holder's own fill is left off here.
static void fillPanelGraphics(const ScrollPanel *sp, ScrollPanelGraphics *g) {
    ScrollPanelGraphics_init(g);
    if ((*sp).hBar)
        ScrollBar_fillGraphics((*sp).hBar, &(*g).hBar);
    if ((*sp).vBar)
        ScrollBar_fillGraphics((*sp).vBar, &(*g).vBar);
}

bool ScrollPanel_paintSkips(ScrollPanel *sp, const Rectangle *rect,
                            const Panel *const *skips, size_t skipCount) {
    if (!sp || !rect)
        return false;
    if ((*rect).width <= 0.0f || (*rect).height <= 0.0f)
        return false;
    Panel *base = &(*sp).base;
    if (!Panel_isVisible(base))
        return false;
    bool drew = Panel_paintParts(base, rect);
    Rectangle saved;
    bool haveClip = Graphics_getClip(&saved);
    Rectangle clip = *rect;
    if (haveClip)
        Rectangle_intersection(&clip, &saved, &clip);
    if (!Rectangle_isEmpty(&clip)) {
        Graphics_clip(&clip);
        Panel *content = (*sp).contentPanel;
        if (content)
            paintSubtree(content, (*rect).x, (*rect).y, skips, skipCount);
        if (haveClip)
            Graphics_clip(&saved);
        else
            Graphics_clip(nullptr);
    }
    drew = ScrollPanel_paintBars(sp, rect) || drew;
    return drew;
}

bool ScrollPanel_paintBars(ScrollPanel *sp, const Rectangle *rect) {
    if (!sp || !rect)
        return false;
    ScrollPanelGraphics pg;
    fillPanelGraphics(sp, &pg);
    return ScrollPanelGraphics_paint(&pg, rect);
}

bool ScrollPanel_paint(ScrollPanel *sp, const Rectangle *rect, const Panel *skip) {
    if (skip)
        return ScrollPanel_paintSkips(sp, rect, &skip, 1u);
    return ScrollPanel_paintSkips(sp, rect, nullptr, 0u);
}

// TOSTRING (PUBLIC)

void ScrollPanel_toString(const ScrollPanel *self, char *dest, size_t cap, bool *outTruncated) {
    Str s;
    Str_init(&s, dest, cap);
    if (self == nullptr) {
        Str_put(&s, "nullptr");
        if (outTruncated) *outTruncated = false;
        return;
    }
    const Panel *b = &(*self).base;
    float viewW = Component_getWidth(&(*b).component);
    float viewH = Component_getHeight(&(*b).component);
    float contentW = 0.0f, contentH = 0.0f;
    if ((*self).contentPanel) {
        contentW = Component_getWidth(&(*(*self).contentPanel).component);
        contentH = Component_getHeight(&(*(*self).contentPanel).component);
    }
    Str_printf(&s, "ScrollPanel[view=%.0fx%.0f content=%.0fx%.0f offset=(%.1f, %.1f)%s%s]",
               viewW, viewH, contentW, contentH, (*self).offsetX, (*self).offsetY,
               (*self).contentAutoW ? " autoW" : "",
               (*self).contentAutoH ? " autoH" : "");
    if (outTruncated) *outTruncated = Str_isTruncated(&s);
}

void ScrollPanel_toStringStruct(const ScrollPanel *self, char *dest, size_t cap, bool *outTruncated) {
    Str s;
    Str_init(&s, dest, cap);
    if (self == nullptr) {
        Str_put(&s, "nullptr");
        if (outTruncated) *outTruncated = false;
        return;
    }
    // ONE layer: the nested bars render via their own toString.
    char hbar[256], vbar[256];
    const char *hText = "nullptr";
    const char *vText = "nullptr";
    bool hTrunc = false, vTrunc = false;
    if ((*self).hBar) {
        ScrollBar_toString((*self).hBar, hbar, sizeof(hbar), &hTrunc);
        hText = hbar;
    }
    if ((*self).vBar) {
        ScrollBar_toString((*self).vBar, vbar, sizeof(vbar), &vTrunc);
        vText = vbar;
    }
    Str_put(&s, "ScrollPanel { ");
    Str_printf(&s, "offset: (%.1f, %.1f), ", (*self).offsetX, (*self).offsetY);
    Str_printf(&s, "contentAuto: (%s, %s), ",
               (*self).contentAutoW ? "true" : "false",
               (*self).contentAutoH ? "true" : "false");
    Str_printf(&s, "visible: (%s, %s), ",
               (*self).hVisible ? "true" : "false",
               (*self).vVisible ? "true" : "false");
    Str_put(&s, "hBar: ");
    Str_put(&s, hText);
    Str_put(&s, ", vBar: ");
    Str_put(&s, vText);
    Str_put(&s, " }");
    if (outTruncated) *outTruncated = Str_isTruncated(&s) || hTrunc || vTrunc;
}
