#include "darling/panel/scroll_container.h"

#include "darling/field/scrollbar.h"
#include "darling/panel/panel.h"
#include "annotation/overview.h"
#include "nio/mem.h"
#include "oop/type.h"

#include <math.h>

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: ScrollContainer (embeds Panel)
 * LEVEL: L2 — Behavior (clipped viewport behavior API)
 * ============================================================================
 * Viewport over an oversized content panel with start/end offsets, inset
 * padding on both scroll ends, child clipping, an owned vertical ScrollBar,
 * touchscreen feel (fling momentum + slippery friction + overscroll
 * rubber-band), and a content-panel forwarding part. Offsets clamp to
 * [-startInset, content-view+endInset] per axis, extended by the overscroll
 * allowance when rubber-banding is enabled.
 * The offset pair is the single source of truth: the bar writes the offset
 * via syncFromBar, setOffset writes back to the bar via syncToBar, and
 * tick integrates fling velocity into the offset every frame.
 * Content attach is detach-only and never frees.
 *
 * THE SCROLLBAR (what it is, what it does):
 * ----------------------------------------------------------------------------
 * The scrollbar is the owned ScrollBar node (*bar): a vertical track+thumb
 * that MIRRORS offsetY. It does two jobs and nothing else:
 *   1. Display: syncToBar maps the current offsetY into the bar's [min,max]
 *      value so the thumb position always shows where you are.
 *   2. Input:  syncFromBar maps a dragged/clicked bar value back into
 *      offsetY so grabbing the thumb scrolls the content.
 * The bar is a child of the viewport (painted on top, clipped with it).
 * The viewport RIGHT-DOCKS it on every content/bar/sync pass: TOP_RIGHT /
 * TOP_RIGHT at (0,0), 10px wide, full viewport height. Anchors are the
 * whole trick — the layer bridge (anti_GetChildLayout + autoresizingMask)
 * resolves them live per resize, so the thumb tracks the edge with zero
 * repaint: Vulkan layers below, IOSurface in the middle, CALayer on top
 * (the vk_test stack), all moving without touching a pixel. That is why
 * the bar lives at the right: it is a layer pinned to an edge, not a
 * painted rect.
 * Hide it with ScrollContainer_scrollbar_setVisible when it gets in the way —
 * touch readers, fullscreen galleries, game logs, auto-hiding overlays —
 * and the offsets keep working exactly the same with no thumb on screen.
 * Swap it with ScrollContainer_scrollbar_setBar (detach-only, sync survives).
 * The bar never owns the offset; hiding never disables scrolling.
 *
 * FEEL (touchscreen physics):
 * ----------------------------------------------------------------------------
 *   fling(vx, vy)  — toss the content with an initial velocity (px/sec),
 *                    e.g. from a swipe-release gesture.
 *   slippery 0..1 — how long the glide lasts. 0 stops dead (legacy direct
 *                    manipulation), 1 glides far. Maps to exponential
 *                    friction inside tick; honor-system default 0 so old
 *                    call sites behave bit-identically.
 *   overscroll px — rubber-band allowance past each clamp end. 0 disables
 *                    (hard clamp, legacy). Positive lets drags/flings pop
 *                    past the edge with extra damping, then tick springs
 *                    them home. That iOS "pulled too far and it snaps back"
 *                    is overscroll > 0 plus tick running every frame.
 *   tick(dt)       — advance on Thread 0 (next to layout): integrate
 *                    velocity, decay by friction, spring back overshoot.
 *                    Call it once per frame while isScrolling, or always —
 *                    it is a cheap no-op at rest inside the bounds.
 *
 * DIRECTION (which way deltas push content):
 * ----------------------------------------------------------------------------
 *   setNatural(true)  — gesture-following (ox-dx, oy-dy): fingers-down
 *                       (dy>0) pushes the content down, like a hand on paper.
 *   setNatural(false) — legacy inverted mapping for rigs that disagree.
 *   scrollBy(dx, dy)  — the ONLY call-site entry for raw deltas; it owns
 *                       the flag. Callers never hand-negate signs.
 *
 * CONTENT-PANEL PART (modify the panel through here):
 * ----------------------------------------------------------------------------
 * A C purist would write content->field and pierce the struct. Here the
 * ScrollContainer owns the content relationship, so edits go through the
 * ScrollContainer_panel_* forwarders: panel_setSize, panel_setBackgroundColor,
 * panel_setRadius (+symmetric getters). They no-op on empty viewports and
 * re-clamp the offset after resizes so you never strand the view past the
 * new content end. Anything finer (margins, anchors) stays on Panel itself.
 *
 * STRUCT FIELDS (Mirroring darling/panel/scroll_container.h — same part banners):
 * ----------------------------------------------------------------------------
 *   --- ScrollContainer core (owner fields) ---
 *   Panel base;                  // Inherited layout, bounds, hierarchy state
 *   Panel *content;              // Viewport child (via panel_*); null = empty
 *   float offsetX;               // Horizontal scroll offset into content
 *   float offsetY;               // Vertical scroll offset into content
 *   float startInset;            // Padding before the first child
 *   float endInset;              // Overscroll past the last child
 *   --- Scrollbar part (live view) ---
 *   ScrollBar *bar;              // Owned vertical bar (replaceable, never freed)
 *   bool barVisible;             // Scrollbar-part visibility (default true)
 *   --- Feel part (owner fields, tick advances) ---
 *   float velX, velY;            // Fling velocity px/sec (tick decays)
 *   float slippery;              // 0 stops dead, 1 long glide (default 0)
 *   float overscroll;            // Rubber-band px past ends (0 = disabled)
 *   --- Direction part (owner field) ---
 *   bool natural;                // True: deltas subtracted (ox-dx, oy-dy); false: added (ox+dx, oy+dy)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - ScrollContainer_2(viewW, viewH)
 *
 * Core Functions:
 *   - ScrollContainer_setContent(sp, content)
 *   - ScrollContainer_setOffset(sp, x, y)
 *   - ScrollContainer_setViewportSize(sp, w, h)
 *   - ScrollContainer_syncFromBar(sp)
 *   - ScrollContainer_syncToBar(sp)
 *
 * Setters:
 *   - ScrollContainer_setStartInset(sp, inset)
 *   - ScrollContainer_setEndInset(sp, inset)
 *
 * Scrollbar part:
 *   - ScrollContainer_scrollbar_setVisible(sp, visible)
 *   - ScrollContainer_scrollbar_setBar(sp, bar)   // replace the view, keep sync
 *
 * Feel part:
 *   - ScrollContainer_setSlippery(sp, slippery)
 *   - ScrollContainer_setOverscroll(sp, px)
 *   - ScrollContainer_fling(sp, vx, vy)
 *   - ScrollContainer_stop(sp)
 *   - ScrollContainer_tick(sp, dt)
 *
 * Direction part:
 *   - ScrollContainer_setNatural(sp, natural)
 *   - ScrollContainer_scrollBy(sp, dx, dy)
 *
 * Content-panel part:
 *   - ScrollContainer_panel_setSize(sp, w, h)
 *   - ScrollContainer_panel_setBackgroundColor(sp, color)
 *   - ScrollContainer_panel_setRadius(sp, radius)
 *
 * Layer part:
 *   - ScrollContainer_childFrame(sp, child, winW, winH, outX, outY, outW, outH)
 *
 * Getters:
 *   - ScrollContainer_getContent(sp)
 *   - ScrollContainer_getOffset(sp, outX, outY)
 *   - ScrollContainer_getStartInset(sp)
 *   - ScrollContainer_getEndInset(sp)
 *   - ScrollContainer_getBar(sp)
 *   - ScrollContainer_scrollbar_isVisible(sp)
 *   - ScrollContainer_getSlippery(sp)
 *   - ScrollContainer_getOverscroll(sp)
 *   - ScrollContainer_getVelocity(sp, outVX, outVY)
 *   - ScrollContainer_isScrolling(sp)
 *   - ScrollContainer_isOverscrolled(sp)
 *   - ScrollContainer_isNatural(sp)
 *   - ScrollContainer_panel_getSize(sp, outW, outH)
 *   - ScrollContainer_panel_getBackgroundColor(sp)
 *   - ScrollContainer_panel_getRadius(sp)
 * ============================================================================
 */

// CONSTRUCTORS

static void layoutBar(ScrollContainer *sp);
static void raiseBar(ScrollContainer *sp);

ScrollContainer *ScrollContainer_2(float viewW, float viewH) {
    ScrollContainer *sp = (ScrollContainer*) Memory_alloc(TYPE_SCROLL_PANEL_SINGLETON, sizeof(ScrollContainer));
    if (!sp)
        return nullptr;
    Panel *b = Panel_0();
    if (!b) {
        Memory_free(sp);
        return nullptr;
    }
    (*sp).base = (*b);
    Memory_free(b);
    (*sp).content = nullptr;
    (*sp).bar = nullptr;
    (*sp).offsetX = 0.0f;
    (*sp).offsetY = 0.0f;
    (*sp).startInset = 0.0f;
    (*sp).endInset = 0.0f;
    (*sp).barVisible = true;
    (*sp).velX = 0.0f;
    (*sp).velY = 0.0f;
    (*sp).slippery = 0.0f;
    (*sp).overscroll = 0.0f;
    (*sp).natural = true;
    Panel *self = &(*sp).base;
    Container *c = &(*self).base;
    Container_setSize(c, viewW, viewH);
    Container_setClipChildren(c, true);
    ScrollBar *bar = ScrollBar_0();
    if (!bar) {
        Memory_free(sp);
        return nullptr;
    }
    Panel *thumb = &(*bar).base;
    Panel_addContainer(self, thumb);
    (*sp).bar = bar;
    layoutBar(sp);
    return sp;
}

// CORE FUNCTIONS

static float pinOffset(float value, float lo, float hi) {
    if (value < lo)
        return lo;
    if (value > hi)
        return hi;
    return value;
}

static void markDirty(ScrollContainer *sp) {
    if (!sp)
        return;
    Panel *b = &(*sp).base;
    Container_markDirty(&(*b).base);
}

// Right-dock: the bar hugs the viewport's right edge (TOP_RIGHT/TOP_RIGHT
// at (0,0), 10px wide, full viewport height). Anchors are the whole trick:
// the layer bridge resolves them live per resize (anti_GetChildLayout +
// autoresizingMask), so the thumb tracks the edge with zero repaint —
// Vulkan layers below, IOSurface in the middle, CALayer on top, all moving
// without touching a pixel. Max tracks the viewport so later growth is
// never clamped by the first layout's ceiling. Idempotent: safe to run on
// every content/bar/sync pass.
#define SCROLLBAR_THICKNESS 10.0f

static void layoutBar(ScrollContainer *sp) {
    if (!sp || !(*sp).bar)
        return;
    Panel *self = &(*sp).base;
    float vh = Container_getHeight(&(*self).base);
    Panel *thumb = &(*(*sp).bar).base;
    Container *bc = &(*thumb).base;
    Container_setAnchor(bc, CONTAINER_ANCHOR_TOP_RIGHT);
    Container_setPivot(bc, CONTAINER_PIVOT_TOP_RIGHT);
    Container_setLocation(bc, 0.0f, 0.0f);
    Container_setMaxSize(bc, SCROLLBAR_THICKNESS, vh);
    Container_setSize(bc, SCROLLBAR_THICKNESS, vh);
    raiseBar(sp);
}

// Topmost rule: the compositor stacks child layers in child order (later =
// front), so the scrollbar must be the LAST child — above the content,
// below nothing. Re-assert after every structural pass; the guard makes
// repeat runs free.
static void raiseBar(ScrollContainer *sp) {
    if (!sp || !(*sp).bar || !(*sp).content)
        return;
    Panel *self = &(*sp).base;
    Panel *thumb = &(*(*sp).bar).base;
    size_t n = Panel_childCount(self);
    if (n == 0 || Panel_getChild(self, n - 1) == thumb)
        return;
    if (Panel_removeChild(self, thumb))
        Panel_addContainer(self, thumb);
}

static void offsetBounds(const ScrollContainer *sp, float *loX, float *hiX, float *loY, float *hiY) {
    float start = (*sp).startInset;
    float end = (*sp).endInset;
    const Panel *b = &(*sp).base;
    const Container *vc = &(*b).base;
    float viewW = Container_getWidth(vc);
    float viewH = Container_getHeight(vc);
    float contentW = 0.0f;
    float contentH = 0.0f;
    Panel *content = (*sp).content;
    if (content) {
        Container *cc = &(*content).base;
        contentW = Container_getWidth(cc);
        contentH = Container_getHeight(cc);
    }
    float lx = -start;
    float hx = contentW - viewW + end;
    float ly = -start;
    float hy = contentH - viewH + end;
    if (hx < lx)
        hx = lx;
    if (hy < ly)
        hy = ly;
    if (loX)
        (*loX) = lx;
    if (hiX)
        (*hiX) = hx;
    if (loY)
        (*loY) = ly;
    if (hiY)
        (*hiY) = hy;
}

void ScrollContainer_setContent(ScrollContainer *sp, Panel *content) {
    if (!sp)
        return;
    Panel *old = (*sp).content;
    if (old == content)
        return;
    Panel *self = &(*sp).base;
    if (old)
        Panel_removeChild(self, old);
    (*sp).content = nullptr;
    if (content && content != self) {
        Panel_addContainer(self, content);
        (*sp).content = content;
    }
    layoutBar(sp);
    ScrollContainer_setOffset(sp, (*sp).offsetX, (*sp).offsetY);
}

void ScrollContainer_setViewportSize(ScrollContainer *sp, float w, float h) {
    if (!sp)
        return;
    Panel *self = &(*sp).base;
    Container *vc = &(*self).base;
    // Lift the first-size ceiling: the viewport follows the window, so its
    // max IS the window. Content is untouched — its size is its own business.
    Container_setMaxSize(vc, w, h);
    Container_setSize(vc, w, h);
    layoutBar(sp); // re-dock: same 10px, new right edge, full new height
    ScrollContainer_setOffset(sp, (*sp).offsetX, (*sp).offsetY); // re-clamp
}

void ScrollContainer_setOffset(ScrollContainer *sp, float x, float y) {
    if (!sp)
        return;
    float loX = 0.0f;
    float hiX = 0.0f;
    float loY = 0.0f;
    float hiY = 0.0f;
    offsetBounds(sp, &loX, &hiX, &loY, &hiY);
    float over = (*sp).overscroll > 0.0f ? (*sp).overscroll : 0.0f;
    (*sp).offsetX = pinOffset(x, loX - over, hiX + over);
    (*sp).offsetY = pinOffset(y, loY - over, hiY + over);
    markDirty(sp);
    ScrollContainer_syncToBar(sp);
}

void ScrollContainer_syncFromBar(ScrollContainer *sp) {
    if (!sp)
        return;
    ScrollBar *bar = (*sp).bar;
    if (!bar)
        return;
    float lo = 0.0f;
    float hi = 0.0f;
    offsetBounds(sp, nullptr, nullptr, &lo, &hi);
    float bmin = 0.0f;
    float bmax = 1.0f;
    ScrollBar_getRange(bar, &bmin, &bmax);
    float span = bmax - bmin;
    float t = 0.0f;
    if (span != 0.0f)
        t = pinOffset((ScrollBar_getValue(bar) - bmin) / span, 0.0f, 1.0f);
    float extent = hi - lo;
    (*sp).offsetY = lo + t * extent;
    markDirty(sp);
}

void ScrollContainer_syncToBar(ScrollContainer *sp) {
    if (!sp)
        return;
    ScrollBar *bar = (*sp).bar;
    if (!bar)
        return;
    float lo = 0.0f;
    float hi = 0.0f;
    offsetBounds(sp, nullptr, nullptr, &lo, &hi);
    float extent = hi - lo;
    float t = 0.0f;
    if (extent > 0.0f)
        t = ((*sp).offsetY - lo) / extent;
    float bmin = 0.0f;
    float bmax = 1.0f;
    ScrollBar_getRange(bar, &bmin, &bmax);
    ScrollBar_setValue(bar, bmin + t * (bmax - bmin));
    layoutBar(sp);
}

// SCROLLBAR PART

static void applyBarVisible(ScrollContainer *sp) {
    if (!sp || !(*sp).bar)
        return;
    Panel *thumb = &(*(*sp).bar).base;
    Container *c = &(*thumb).base;
    Container_setVisible(c, (*sp).barVisible);
}

void ScrollContainer_scrollbar_setVisible(ScrollContainer *sp, bool visible) {
    if (!sp)
        return;
    (*sp).barVisible = visible;
    applyBarVisible(sp);
    markDirty(sp);
}

void ScrollContainer_scrollbar_setBar(ScrollContainer *sp, ScrollBar *bar) {
    if (!sp || !bar || (*sp).bar == bar)
        return;
    // Borrowed view, detach-only (Rule 29): old bar is detached, never
    // freed; the new one is attached, never re-owned. Games swap in a
    // skinned track+thumb and the offset sync keeps working untouched.
    Panel *self = &(*sp).base;
    if ((*sp).bar)
        Panel_removeChild(self, &(*(*sp).bar).base);
    Panel_addContainer(self, &(*bar).base);
    (*sp).bar = bar;
    applyBarVisible(sp);
    layoutBar(sp);
    ScrollContainer_syncToBar(sp);
    markDirty(sp);
}

// FEEL PART

// Slippery 0..1 maps to exponential friction: 0 stops dead (legacy),
// 1 glides far. friction = 12 at 0, 0.8 at 1.
static float feelFriction(float slippery) {
    return 12.0f + (0.8f - 12.0f) * slippery;
}

void ScrollContainer_setSlippery(ScrollContainer *sp, float slippery) {
    if (!sp)
        return;
    if (slippery < 0.0f)
        slippery = 0.0f;
    if (slippery > 1.0f)
        slippery = 1.0f;
    (*sp).slippery = slippery;
}

void ScrollContainer_setOverscroll(ScrollContainer *sp, float px) {
    if (!sp)
        return;
    (*sp).overscroll = px > 0.0f ? px : 0.0f;
    ScrollContainer_setOffset(sp, (*sp).offsetX, (*sp).offsetY);
}

void ScrollContainer_fling(ScrollContainer *sp, float vx, float vy) {
    if (!sp)
        return;
    (*sp).velX = vx;
    (*sp).velY = vy;
    markDirty(sp);
}

void ScrollContainer_stop(ScrollContainer *sp) {
    if (!sp)
        return;
    (*sp).velX = 0.0f;
    (*sp).velY = 0.0f;
}

// DIRECTION PART

// Deltas arrive raw from the OS. natural=true applies the gesture-following
// mapping (ox-dx, oy-dy): fingers-down (dy>0) pushes the content down, the
// way a hand on paper behaves. false restores the legacy inverted mapping
// for rigs that disagree. If fingers and content ever disagree, this one
// flag is the entire argument — never hand-negate at the call site.
void ScrollContainer_setNatural(ScrollContainer *sp, bool natural) {
    if (!sp)
        return;
    (*sp).natural = natural;
}

void ScrollContainer_scrollBy(ScrollContainer *sp, float dx, float dy) {
    if (!sp)
        return;
    float ox = 0.0f, oy = 0.0f;
    ScrollContainer_getOffset(sp, &ox, &oy);
    if ((*sp).natural)
        ScrollContainer_setOffset(sp, ox - dx, oy - dy);
    else
        ScrollContainer_setOffset(sp, ox + dx, oy + dy);
}

static float tickAxis(float off, float *vel, float lo, float hi, float over, float friction, double dt) {
    float v = *vel;
    if (v != 0.0f) {
        off += v * (float)dt;
        // Extra damping while riding the rubber band past the edge.
        bool past = off < lo || off > hi;
        float damp = friction * (float)dt;
        if (past)
            damp += 6.0f * (float)dt;
        v *= expf(-damp);
        if (fabsf(v) < 1.0f)
            v = 0.0f;
        // Hard stop at the rubber limit (no velocity bounce).
        if (off < lo - over) {
            off = lo - over;
            v = 0.0f;
        } else if (off > hi + over) {
            off = hi + over;
            v = 0.0f;
        }
    } else if (off < lo || off > hi) {
        // Spring home: exponential approach, snap when close.
        float bound = off < lo ? lo : hi;
        float pull = (bound - off) * (14.0f * (float)dt > 1.0f ? 1.0f : 14.0f * (float)dt);
        off += pull;
        if (fabsf(bound - off) < 0.5f)
            off = bound;
    }
    *vel = v;
    return off;
}

void ScrollContainer_tick(ScrollContainer *sp, double dt) {
    if (!sp || dt <= 0.0)
        return;
    float loX = 0.0f, hiX = 0.0f, loY = 0.0f, hiY = 0.0f;
    offsetBounds(sp, &loX, &hiX, &loY, &hiY);
    float over = (*sp).overscroll;
    float friction = feelFriction((*sp).slippery);
    bool wasMoving = (*sp).velX != 0.0f || (*sp).velY != 0.0f;
    bool wasOut = (*sp).offsetX < loX || (*sp).offsetX > hiX
        || (*sp).offsetY < loY || (*sp).offsetY > hiY;
    (*sp).offsetX = tickAxis((*sp).offsetX, &(*sp).velX, loX, hiX, over, friction, dt);
    (*sp).offsetY = tickAxis((*sp).offsetY, &(*sp).velY, loY, hiY, over, friction, dt);
    if (wasMoving || wasOut || (*sp).velX != 0.0f || (*sp).velY != 0.0f)
        markDirty(sp);
    ScrollContainer_syncToBar(sp);
}

// CONTENT-PANEL PART

void ScrollContainer_panel_setSize(ScrollContainer *sp, float w, float h) {
    if (!sp || !(*sp).content)
        return;
    Container_setSize(&(*(*sp).content).base, w, h);
    ScrollContainer_setOffset(sp, (*sp).offsetX, (*sp).offsetY);
}

void ScrollContainer_panel_setBackgroundColor(ScrollContainer *sp, uint32_t color) {
    if (!sp || !(*sp).content)
        return;
    Panel_setBackgroundColor((*sp).content, color);
}

void ScrollContainer_panel_setRadius(ScrollContainer *sp, float radius) {
    if (!sp || !(*sp).content)
        return;
    Panel_setRadius((*sp).content, radius);
}

// SETTERS

void ScrollContainer_setStartInset(ScrollContainer *sp, float inset) {
    if (!sp)
        return;
    (*sp).startInset = inset;
    ScrollContainer_setOffset(sp, (*sp).offsetX, (*sp).offsetY);
}

void ScrollContainer_setEndInset(ScrollContainer *sp, float inset) {
    if (!sp)
        return;
    (*sp).endInset = inset;
    ScrollContainer_setOffset(sp, (*sp).offsetX, (*sp).offsetY);
}

// GETTERS

Panel *ScrollContainer_getContent(const ScrollContainer *sp) {
    return sp ? (*sp).content : nullptr;
}

void ScrollContainer_getOffset(const ScrollContainer *sp, float *outX, float *outY) {
    float x = 0.0f;
    float y = 0.0f;
    if (sp) {
        x = (*sp).offsetX;
        y = (*sp).offsetY;
    }
    if (outX)
        (*outX) = x;
    if (outY)
        (*outY) = y;
}

float ScrollContainer_getStartInset(const ScrollContainer *sp) {
    return sp ? (*sp).startInset : 0.0f;
}

float ScrollContainer_getEndInset(const ScrollContainer *sp) {
    return sp ? (*sp).endInset : 0.0f;
}

ScrollBar *ScrollContainer_getBar(const ScrollContainer *sp) {
    return sp ? (*sp).bar : nullptr;
}

bool ScrollContainer_scrollbar_isVisible(const ScrollContainer *sp) {
    return sp && (*sp).barVisible;
}

float ScrollContainer_getSlippery(const ScrollContainer *sp) {
    return sp ? (*sp).slippery : 0.0f;
}

float ScrollContainer_getOverscroll(const ScrollContainer *sp) {
    return sp ? (*sp).overscroll : 0.0f;
}

void ScrollContainer_getVelocity(const ScrollContainer *sp, float *outVX, float *outVY) {
    float vx = 0.0f, vy = 0.0f;
    if (sp) {
        vx = (*sp).velX;
        vy = (*sp).velY;
    }
    if (outVX)
        (*outVX) = vx;
    if (outVY)
        (*outVY) = vy;
}

bool ScrollContainer_isScrolling(const ScrollContainer *sp) {
    return sp && ((*sp).velX != 0.0f || (*sp).velY != 0.0f);
}

bool ScrollContainer_isNatural(const ScrollContainer *sp) {
    return sp && (*sp).natural;
}

bool ScrollContainer_isOverscrolled(const ScrollContainer *sp) {
    if (!sp)
        return false;
    float loX = 0.0f, hiX = 0.0f, loY = 0.0f, hiY = 0.0f;
    offsetBounds(sp, &loX, &hiX, &loY, &hiY);
    return (*sp).offsetX < loX || (*sp).offsetX > hiX
        || (*sp).offsetY < loY || (*sp).offsetY > hiY;
}

void ScrollContainer_panel_getSize(const ScrollContainer *sp, float *outW, float *outH) {
    float w = 0.0f, h = 0.0f;
    if (sp && (*sp).content) {
        w = Container_getWidth(&(*(*sp).content).base);
        h = Container_getHeight(&(*(*sp).content).base);
    }
    if (outW)
        (*outW) = w;
    if (outH)
        (*outH) = h;
}

uint32_t ScrollContainer_panel_getBackgroundColor(const ScrollContainer *sp) {
    if (sp && (*sp).content)
        return Panel_getBackgroundColor((*sp).content);
    return PANEL_COLOR_CLEAR;
}

float ScrollContainer_panel_getRadius(const ScrollContainer *sp) {
    if (sp && (*sp).content)
        return Panel_getRadius((*sp).content);
    return 0.0f;
}

// LAYER PART

void ScrollContainer_childFrame(const ScrollContainer *sp, const Panel *child, float winW, float winH,
                            float *outX, float *outY, float *outW, float *outH) {
    float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
    if (sp && child) {
        Vec4 rect;
        Panel *self = (Panel *)&(*sp).base;
        float vw = Container_getWidth(&(*self).base);
        float vh = Container_getHeight(&(*self).base);
        Container_resolve(&((Panel *)child)->base, 0.0f, 0.0f, vw, vh, &rect);
        x = rect.x;
        y = rect.y;
        w = rect.z;
        h = rect.w;
        // Content scrolls under the viewport; chrome (the bar) stays put.
        Panel *bar = (*sp).bar ? &(*(*sp).bar).base : nullptr;
        if (child != bar) {
            x -= (*sp).offsetX;
            y -= (*sp).offsetY;
        }
    }
    (void)winW;
    (void)winH; // reserved: the viewport itself resolves against the window upstream
    if (outX)
        (*outX) = x;
    if (outY)
        (*outY) = y;
    if (outW)
        (*outW) = w;
    if (outH)
        (*outH) = h;
}
