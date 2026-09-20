#include "darling/component.h"

#include <math.h>
#include <string.h>

#include "../c23/darling-type.h"
#include "graphics/graphics.h"
#include "buffer/buffer.h"
#include "image/image.h"
#include "raster/raster_graphics.h"
#include "lang/rect/rectangle.h"
#include "paint/brush.h"
#include "paint/stroke.h"
#include "nio/mem.h"
#include "oop/type.h"
#include "annotation/definition.h"
#include "annotation/overview.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Component
 * ============================================================================
 * The renderable leaf of the new darling architecture: geometry +
 * presentation state + an ABSOLUTE rect recomputed eagerly on every geometry
 * setter, so a renderer consumes ((*self).absX..absH) directly — no resolve
 * phase, no parent-size threading, no stale reads. A Component is a leaf on
 * its own: containment is optional by construction — an element that does
 * not want to be added is exactly a Component without a Container. It DOES
 * own an optional children list (the tree form): the parent cascades its
 * CONTENT box (abs inset by padding — padding insets children) into each
 * child eagerly, and Component_render walks the tree in order.
 * The eager abs cascade is O(1) per leaf: setters recompute this component's
 * abs immediately against the parent's abs box (stored via
 * Component_setParentAbs), and setters never layout, so the whole tree cost
 * equals one interleaved resolve pass. The anchor+pivot system mirrors
 * Container's exactly for a value-identical migration.
 * Component_render takes a ComponentView — the active unified Graphics row
 * plus the point-to-native-pixel scale of the current present — instead of a
 * bare pointer, so the seam renders the tree directly in device pixels and
 * the same view flows to every node and hook. The view also carries an
 * abs-space origin offset: samples map as (abs - origin) * scale, so the
 * identical currency serves shifted (scroll/pan) views and the retained
 * sub-pass view of a deferred component's bake. A process-wide generation
 * counter (Component_gen) bumps on every recompute and every visual/content
 * mutation; the compositor latches it per frame to re-arm present demand
 * (the Present-On-Demand Law) when a component tree changes without a
 * board/panel paint.
 * OPT-IN DEFERRED SUBTREES: a node flagged with Component_setDeferred stops
 * painting its subtree per frame — the render pass bakes it once into a
 * retained alpha-first ARGB8 tile (ceil(absW*scale) x ceil(absH*scale)
 * native px) whenever its mapped size, the view scale, or the generation
 * counter drifts, then blits the tile with Graphics_drawImage. The bake
 * swaps the Raster row's framebuffer (RasterGraphics_setFramebuffer),
 * paints the subtree through a bake view shifted to the node's abs origin,
 * reads the target back into the Image shadow, and restores the outer
 * target — so nested deferred children bake into the parent's tile cleanly.
 * Rows without a live drawImage (Vk/Metal draft today) degrade the subtree
 * back to inline rendering.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Component
 * LEVEL: L2 — Behavior (immediate on-demand rendering leaf)
 * ============================================================================
 * The renderable leaf of the new darling architecture: geometry +
 * presentation state + an ABSOLUTE rect computed eagerly on every geometry
 * setter, so a renderer consumes ((*self).absX..absH) directly — no resolve
 * phase, no parent-size threading, no stale reads. A Component is a leaf on
 * its own: containment is optional by construction — an element that does
 * not want to be added is exactly a Component without a Container — but it
 * DOES own an optional children list (the tree form), cascades its content
 * box (abs inset by padding) into each child eagerly, and renders the whole
 * tree in order under one ComponentView (unified Graphics row + device
 * scale). The generation counter (Component_gen) re-arms the compositor's
 * present demand on any component mutation.
 *
 * ;;INTENTION("eager abs cascade per the Present-On-Demand Law: geometry
 * setters recompute abs immediately; parent containers cascade via
 * Component_setParentAbs; setters never layout, so recompute is O(1) per
 * leaf and the whole tree cost equals one resolve pass, interleaved")
 *
 * STRUCT FIELDS (Mirroring darling/component.h):
 * ----------------------------------------------------------------------------
 *   float x, y, w, h;      // Placement + size in parent units; x/y direction
 *                          // governed by origin (4 corners)
 *   uint8_t origin;        // COMPONENT_ORIGIN_* 0..3 (4 corners: coordinate frame)
 *   uint8_t anchor;        // COMPONENT_ANCHOR_* 0..8 (9-grid docking point on parent)
 *   int32_t pivot;         // COMPONENT_PIVOT_* 0..4 (5 child points: 4 corners + center)
 *   float minW, minH;      // Size constraints (0 = unset)
 *   float maxW, maxH;      // Size constraints (0 = unset)
 *   float marginL, marginT; // Additive placement: final = resolved + margin
 *   float marginR, marginB; // Right/bottom edges stored for sibling layout
 *   float paddingL, paddingT; // Inward content insets (content box = abs + padding)
 *   float paddingR, paddingB;
 *   float borderWidth;     // 0 = no border
 *   uint32_t borderColor;  // 0xAARRGGBB
 *   uint32_t backgroundColor; // 0xAARRGGBB (consumed by render hooks)
 *   float radius;          // Corner radius (0 = square)
 *   int radiusMode;        // COMPONENT_CORNER_ARC (0) / COMPONENT_CORNER_SUPERELLIPSE (1)
 *   float opacity;         // 0..1 alpha multiplier (1 = opaque)
 *   int32_t z;             // Z-order
 *   uint8_t visible;       // Visibility gate for render/hitTest
 *   Component *parent;     // Borrowed view, nullptr = root; NEVER owned
 *   Component **children;  // Child components owned or attached
 *   uint32_t childCount;
 *   uint32_t childCapacity;
 *   float absX, absY;      // Absolute left/top in the rendering space (eager)
 *   float absW, absH;      // Absolute extent (eager)
 *   float parentAbsX, parentAbsY; // Parent abs box at last recompute
 *   float parentAbsW, parentAbsH;
 *   Component_RenderFn backgroundRender; // Stage 0 render hook
 *   Component_RenderFn foregroundRender; // Stage 1 render hook
 *   void *renderUserdata;  // Opaque arg handed to both hooks
 *   uint8_t deferred;      // Opt-in retained subtree: bake on drift, then blit
 *   struct Image *retainImage;    // Owned alpha-first ARGB8 artifact blitted by the render pass
 *   struct Buffer *retainBuffer;  // Owned raster target painted during bake (framebuffer sub-pass)
 *   uint32_t retainW;      // ceil(absW * viewScaleX), native px, at last bake
 *   uint32_t retainH;      // ceil(absH * viewScaleY), native px, at last bake
 *   float retainScaleX;    // Point->px scale at last bake (drift check)
 *   float retainScaleY;    // Point->px scale at last bake (drift check)
 *   uint64_t retainGen;    // Component_gen() latched at last bake (content drift check)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Component_0(void)
 *
 * Core Functions:
 *   - Component_recompute(self)
 *   - Component_setParentAbs(self, px, py, pw, ph)
 *   - Component_render(self, view)
 *   - Component_bake(self, view)              : rebuild retain target iff drift (dims/scale/content); true when current
 *   - Component_viewMap(view, ax, ay, aw, ah, outX, outY, outW, outH) : points -> native px (origin-translated, provably gapless)
 *   - Component_hitTest(self, pointX, pointY)
 *   - Component_getContentRect(self, outX, outY, outW, outH)
 *   - Component_addChild(self, child)
 *   - Component_removeChild(self, child)
 *   - Component_gen(void)
 *
 * Private Core Functions: (.c static)
 *   - touchGen(void)                           : bump the process-wide generation counter
 *   - contentBox(self, outX, outY, outW, outH) : abs rect inset by padding (children cascade box)
 *   - renderInline(self, view)                 : the immediate subtree paint (bg, hooks, children, border)
 *   - renderDeferred(self, view)               : bake-if-dirty then blit the retained tile
 *
 * Setters:
 *   - Component_setX/Y(self, v)
 *   - Component_setWidth/Height(self, v)
 *   - Component_setLocation(self, x, y)
 *   - Component_setSize(self, w, h)          // clamps [min, max]
 *   - Component_setMinSize/MaxSize(self, w, h) // re-clamps current size
 *   - Component_setOrigin(self, origin)
 *   - Component_setAnchor(self, anchor)
 *   - Component_setPivot(self, pivot)
 *   - Component_setCenter(self)
 *   - Component_setMargin(self, l, t, r, b)
 *   - Component_setPadding(self, l, t, r, b)
 *   - Component_setBorderWidth(self, w)
 *   - Component_setBorderColor(self, color)
 *   - Component_setBackgroundColor(self, color)
 *   - Component_setRadius(self, r)
 *   - Component_setRadiusMode(self, mode)
 *   - Component_setOpacity(self, opacity)
 *   - Component_setZ(self, z)
 *   - Component_setVisible(self, visible)
 *   - Component_setParent(self, parent)
 *   - Component_setBackgroundRender(self, fn)
 *   - Component_setForegroundRender(self, fn)
 *   - Component_setRenderUserdata(self, userdata)
 *   - Component_setDeferred(self, deferred)    // opt-in retained subtree (drift-rebaked)
 *
 * Getters:
 *   - Component_getX/Y/Width/Height(self)
 *   - Component_getAbsX/Y/W/H(self)
 *   - Component_getAbsRect(self, outRect)
 *   - Component_getParentAbsRect(self, outRect)
 *   - Component_getOrigin(self)
 *   - Component_getAnchor(self)
 *   - Component_getPivot(self)
 *   - Component_getMinWidth/MinHeight/MaxWidth/MaxHeight(self)
 *   - Component_getMargin(self, outL, outT, outR, outB)
 *   - Component_getPadding(self, outL, outT, outR, outB)
 *   - Component_getBorderWidth/BorderColor/BackgroundColor(self)
 *   - Component_getRadius/RadiusMode(self)
 *   - Component_getOpacity/Z(self)
 *   - Component_isVisible(self)
 *   - Component_isDeferred(self)
 *   - Component_getRetainWidth(self)
 *   - Component_getRetainHeight(self)
 *   - Component_getParent(self)
 *   - Component_getChildCount(self)
 *   - Component_getChild(self, index)
 *   - Component_getBackgroundRender/ForegroundRender(self)
 *   - Component_getRenderUserdata(self)
 * ============================================================================
 */

// darling/component.c — immediate on-demand rendering leaf (new architecture).

// FILE-LOCAL STATE & HELPERS

// s_componentGen — the process-wide Component generation counter. Every
// geometry recompute and every visual/content mutation (padding, hooks,
// visibility, membership) bumps it once. The compositor's probe compares
// Component_gen() against the frame's latched lastComponentGen to re-arm
// present demand (the Present-On-Demand Law) when a component tree changed
// without a board/panel paint.
static uint64_t s_componentGen = 0u;

static void touchGen(void) {
    s_componentGen++;
}

// The eager content box: abs rect inset by padding (clamped >= 0). Children
// cascade against THIS box, never the raw abs box, so padding insets child
// placement (the padding-insets-children decision).
static void contentBox(const Component *self, float *outX, float *outY,
                       float *outW, float *outH) {
    float cx = (*self).absX + (*self).paddingL;
    float cy = (*self).absY + (*self).paddingT;
    float cw = (*self).absW - (*self).paddingL - (*self).paddingR;
    float ch = (*self).absH - (*self).paddingT - (*self).paddingB;
    if (cw < 0.0f)
        cw = 0.0f;
    if (ch < 0.0f)
        ch = 0.0f;
    if (outX)
        *outX = cx;
    if (outY)
        *outY = cy;
    if (outW)
        *outW = cw;
    if (outH)
        *outH = ch;
}

// CONSTRUCTORS

Component *Component_0(void) {
    Component *self = (Component*) Memory_alloc(TYPE_COMPONENT_SINGLETON, sizeof(Component));
    if (!self)
        return nullptr;
    (*self).x = 0.0f;
    (*self).y = 0.0f;
    (*self).w = 0.0f;
    (*self).h = 0.0f;
    (*self).origin = COMPONENT_ORIGIN_TOP_LEFT;
    (*self).anchor = COMPONENT_ANCHOR_TOP_LEFT;
    (*self).pivot = COMPONENT_PIVOT_TOP_LEFT;
    (*self).minW = 0.0f;
    (*self).minH = 0.0f;
    (*self).maxW = 0.0f;
    (*self).maxH = 0.0f;
    (*self).marginL = 0.0f;
    (*self).marginT = 0.0f;
    (*self).marginR = 0.0f;
    (*self).marginB = 0.0f;
    (*self).paddingL = 0.0f;
    (*self).paddingT = 0.0f;
    (*self).paddingR = 0.0f;
    (*self).paddingB = 0.0f;
    (*self).borderWidth = 0.0f;
    (*self).borderColor = COMPONENT_COLOR_CLEAR;
    (*self).backgroundColor = COMPONENT_COLOR_CLEAR;
    (*self).radius = 0.0f;
    (*self).radiusMode = COMPONENT_CORNER_ARC;
    (*self).opacity = 1.0f;
    (*self).z = 0;
    (*self).visible = 1;
    (*self).parent = nullptr;
    (*self).children = nullptr;
    (*self).childCount = 0;
    (*self).childCapacity = 0;
    (*self).absX = 0.0f;
    (*self).absY = 0.0f;
    (*self).absW = 0.0f;
    (*self).absH = 0.0f;
    (*self).parentAbsX = 0.0f;
    (*self).parentAbsY = 0.0f;
    (*self).parentAbsW = 0.0f;
    (*self).parentAbsH = 0.0f;
    (*self).backgroundRender = nullptr;
    (*self).foregroundRender = nullptr;
    (*self).renderUserdata = nullptr;
    (*self).deferred = 0;
    (*self).retainImage = nullptr;
    (*self).retainBuffer = nullptr;
    (*self).retainW = 0;
    (*self).retainH = 0;
    (*self).retainScaleX = 0.0f;
    (*self).retainScaleY = 0.0f;
    (*self).retainGen = 0;
    return self;
}

// CORE FUNCTIONS

void Component_recompute(Component *self) {
    if (!self)
        return;
    touchGen();

    float sw = (*self).w;
    float sh = (*self).h;
    float locX = (*self).x;
    float locY = (*self).y;
    float parentW = (*self).parentAbsW;
    float parentH = (*self).parentAbsH;

    // 1. Anchor: 9-grid tether point on parent bounds (normalized Ua, Va)
    float Ua = 0.0f, Va = 0.0f;
    switch (Component_getAnchor(self)) {
        case COMPONENT_ANCHOR_TOP_LEFT:       Ua = 0.0f; Va = 0.0f; break;
        case COMPONENT_ANCHOR_TOP_CENTER:     Ua = 0.5f; Va = 0.0f; break;
        case COMPONENT_ANCHOR_TOP_RIGHT:      Ua = 1.0f; Va = 0.0f; break;
        case COMPONENT_ANCHOR_MIDDLE_LEFT:    Ua = 0.0f; Va = 0.5f; break;
        case COMPONENT_ANCHOR_MIDDLE_CENTER:  Ua = 0.5f; Va = 0.5f; break;
        case COMPONENT_ANCHOR_MIDDLE_RIGHT:   Ua = 1.0f; Va = 0.5f; break;
        case COMPONENT_ANCHOR_BOTTOM_LEFT:    Ua = 0.0f; Va = 1.0f; break;
        case COMPONENT_ANCHOR_BOTTOM_CENTER:  Ua = 0.5f; Va = 1.0f; break;
        case COMPONENT_ANCHOR_BOTTOM_RIGHT:   Ua = 1.0f; Va = 1.0f; break;
        default: break;
    }
    float anchorX = (*self).parentAbsX + Ua * parentW;
    float anchorY = (*self).parentAbsY + Va * parentH;

    // 2. Pivot: 5 alignment points on child component (normalized Up, Vp)
    float Up = 0.0f, Vp = 0.0f;
    switch (Component_getPivot(self)) {
        case COMPONENT_PIVOT_TOP_LEFT:      Up = 0.0f; Vp = 0.0f; break;
        case COMPONENT_PIVOT_TOP_RIGHT:     Up = 1.0f; Vp = 0.0f; break;
        case COMPONENT_PIVOT_BOTTOM_LEFT:   Up = 0.0f; Vp = 1.0f; break;
        case COMPONENT_PIVOT_BOTTOM_RIGHT:  Up = 1.0f; Vp = 1.0f; break;
        case COMPONENT_PIVOT_CENTER:        Up = 0.5f; Vp = 0.5f; break;
        default: break;
    }
    float pivotX = Up * sw;
    float pivotY = Vp * sh;

    // 3. Origin: 4-corner coordinate system orientation
    // Determines axis direction (+ or -) for location offsets (x, y)
    float dirX = 1.0f;
    float dirY = 1.0f;
    switch (Component_getOrigin(self)) {
        case COMPONENT_ORIGIN_TOP_RIGHT:
            dirX = -1.0f;
            dirY =  1.0f;
            break;
        case COMPONENT_ORIGIN_BOTTOM_LEFT:
            dirX =  1.0f;
            dirY = -1.0f;
            break;
        case COMPONENT_ORIGIN_BOTTOM_RIGHT:
            dirX = -1.0f;
            dirY = -1.0f;
            break;
        case COMPONENT_ORIGIN_TOP_LEFT:
        default:
            dirX =  1.0f;
            dirY =  1.0f;
            break;
    }

    // Universal resolution: Anchor - Pivot + (dir * offset) + margin
    float screenX = anchorX - pivotX + (dirX * locX) + (*self).marginL;
    float screenY = anchorY - pivotY + (dirY * locY) + (*self).marginT;

    (*self).absX = screenX;
    (*self).absY = screenY;
    (*self).absW = sw;
    (*self).absH = sh;

    // Eager Cascade Law: cascade the CONTENT box (abs inset by padding —
    // padding insets children) to all children immediately
    float contentX = 0.0f, contentY = 0.0f, contentW = 0.0f, contentH = 0.0f;
    contentBox(self, &contentX, &contentY, &contentW, &contentH);
    for (uint32_t i = 0; i < (*self).childCount; ++i) {
        Component *child = (*self).children[i];
        if (child) {
            Component_setParentAbs(child, contentX, contentY, contentW, contentH);
        }
    }
}

void Component_setParentAbs(Component *self, float px, float py, float pw, float ph) {
    if (!self)
        return;
    (*self).parentAbsX = px;
    (*self).parentAbsY = py;
    (*self).parentAbsW = pw;
    (*self).parentAbsH = ph;
    Component_recompute(self);
}

// renderInline — the immediate subtree paint: native background fill,
// background hook, children (each through Component_render, so a deferred
// child bakes INTO this pass's target — nested bakes swap targets and
// restore), border stroke, foreground hook. Shared by the non-deferred
// render path and the deferred bake's sub-pass.
static bool renderInline(Component *self, const ComponentView *view) {
    if (!self || !(*self).visible)
        return false;
    bool ran = false;

    // 1. Native background fill through unified Graphics seam (device-mapped)
    if ((*self).backgroundColor != COMPONENT_COLOR_CLEAR && (*self).opacity > 0.0f
        && (*self).absW > 0.0f && (*self).absH > 0.0f) {
        Rectangle rect;
        Component_viewMap(view, (*self).absX, (*self).absY, (*self).absW, (*self).absH,
                          &rect.x, &rect.y, &rect.width, &rect.height);
        Brush brush = {(*self).backgroundColor, (*self).opacity, 0u};
        Graphics_fillRect(&rect, &brush);
        ran = true;
    }

    // 2. Background custom hook (receives the view; maps its own rects)
    if ((*self).backgroundRender) {
        (*self).backgroundRender(self, view, (*self).renderUserdata);
        ran = true;
    }

    // 3. Render children in order (same view; abs live in the same point space)
    for (uint32_t i = 0; i < (*self).childCount; ++i) {
        Component *child = (*self).children[i];
        if (child && (*child).visible) {
            if (Component_render(child, view))
                ran = true;
        }
    }

    // 4. Native border stroke through unified Graphics seam (device-mapped)
    if ((*self).borderWidth > 0.0f && (*self).borderColor != COMPONENT_COLOR_CLEAR
        && (*self).absW > 0.0f && (*self).absH > 0.0f) {
        Rectangle rect;
        Component_viewMap(view, (*self).absX, (*self).absY, (*self).absW, (*self).absH,
                          &rect.x, &rect.y, &rect.width, &rect.height);
        Stroke stroke = {(*self).borderWidth, 0.0f, STROKE_CAP_BUTT, STROKE_JOIN_MITER, (*self).borderColor, 0u};
        Graphics_drawRect(&rect, &stroke);
        ran = true;
    }

    // 5. Foreground custom hook (receives the view)
    if ((*self).foregroundRender) {
        (*self).foregroundRender(self, view, (*self).renderUserdata);
        ran = true;
    }
    return ran;
}

// renderDeferred — the retained path: bake the subtree when its mapped size,
// view scale, or process-wide generation drifted, then blit the retained
// tile at the node's mapped rect (1:1 nearest: the target is ceil-mapped at
// the very same scale). A failed bake (cold row, degenerate extent, OOM)
// degrades to the inline paint so pixels stay correct.
static bool renderDeferred(Component *self, const ComponentView *view) {
    if (!Component_bake(self, view))
        return renderInline(self, view);
    if (!(*self).retainImage || !(*self).retainBuffer)
        return false; // cold guard: bake reports current only with a target
    Rectangle rect;
    Component_viewMap(view, (*self).absX, (*self).absY, (*self).absW, (*self).absH,
                      &rect.x, &rect.y, &rect.width, &rect.height);
    if (rect.width <= 0.0f || rect.height <= 0.0f)
        return false;
    return Graphics_drawImage((*self).retainImage, &rect);
}

bool Component_render(Component *self, const ComponentView *view) {
    if (!self || !(*self).visible)
        return false;
    // ;;INTENTION("deferred retain needs a live drawImage row: RASTER is live today; Vk/Metal rows draft-false, so the subtree degrades to inline rendering until their samplers land")
    if ((*self).deferred && view && Graphics_getGraphicsId() == GRAPHICS_BACKEND_RASTER)
        return renderDeferred(self, view);
    return renderInline(self, view);
}

bool Component_bake(Component *self, const ComponentView *view) {
    if (!self || !view || !(*self).visible)
        return false;
    if (Graphics_getGraphicsId() != GRAPHICS_BACKEND_RASTER)
        return false; // the sub-pass swaps the Raster row's target — cold elsewhere
    if (!RasterGraphics_getFramebuffer())
        return false; // no outer target to restore — the swap would orphan the singleton

    // Mapped retain extent: ceil(abs * scale), the gapless tile size. NaN
    // and infinity fail the comparisons below (cold-strict, no crash paths).
    float fw = ceilf((*self).absW * (*view).scaleX);
    float fh = ceilf((*self).absH * (*view).scaleY);
    if (fw >= (float) UINT32_MAX || fh >= (float) UINT32_MAX)
        return false;
    if (!(fw >= 1.0f) || !(fh >= 1.0f))
        return false; // degenerate extent — nothing to retain
    uint32_t rw = (uint32_t) fw;
    uint32_t rh = (uint32_t) fh;

    // Current? Retain is valid when the baked dims, view scale, and content
    // generation all match — the O(1) demand check (per the Present-On-Demand
    // Law: the seam repaints on Component_gen drift, so any subtree mutation
    // lands here as a rebuild, and clean frames skip render entirely).
    bool current = (*self).retainImage != nullptr && (*self).retainBuffer != nullptr
        && (*self).retainW == rw && (*self).retainH == rh
        && (*self).retainScaleX == (*view).scaleX
        && (*self).retainScaleY == (*view).scaleY
        && (*self).retainGen == Component_gen();
    if (current)
        return true;

    // Rebuild the target pair (cold path, rare): free the old artifacts and
    // allocate the ceil-mapped raster target + its ARGB8 image shadow.
    if ((*self).retainBuffer)
        Buffer_free((*self).retainBuffer);
    if ((*self).retainImage)
        Image_free((*self).retainImage);
    (*self).retainBuffer = nullptr;
    (*self).retainImage = nullptr;
    Buffer *buf = Buffer_4(ID_COMPONENT, rw, rh, 4u);
    Image *img = Image_2(rw, rh);
    if (buf == nullptr || img == nullptr) {
        if (buf)
            Buffer_free(buf);
        if (img)
            Image_free(img);
        return false; // cold-strict: OOM leaves the retain cleared
    }
    (*self).retainBuffer = buf;
    (*self).retainImage = img;
    (*self).retainW = rw;
    (*self).retainH = rh;
    (*self).retainScaleX = (*view).scaleX;
    (*self).retainScaleY = (*view).scaleY;

    // Sub-pass: swap the Raster row's target to the retain buffer, clear to
    // transparent, paint the subtree through a bake view that shifts the
    // origin to this node's abs top-left (children map (abs - origin)*scale
    // into target device px), then restore the outer target.
    Buffer *savedFb = RasterGraphics_getFramebuffer();
    uint32_t savedW = RasterGraphics_getWidth();
    uint32_t savedH = RasterGraphics_getHeight();
    if (!RasterGraphics_setFramebuffer(buf, rw, rh))
        return false; // unexpected swap rejection — cold state untouched below
    Graphics_clip(nullptr); // the bake sub-pass owns its scissor space
    Graphics_clear(COMPONENT_COLOR_CLEAR); // transparent base for the tile
    ComponentView bakeView = {
        .graphics = (*view).graphics,
        .scaleX = (*view).scaleX,
        .scaleY = (*view).scaleY,
        .originX = (*self).absX,
        .originY = (*self).absY,
    };
    renderInline(self, &bakeView);
    RasterGraphics_setFramebuffer(savedFb, savedW, savedH);
    Graphics_clip(nullptr); // ;;INTENTION("bake leaves clip disabled: nothing on the seam uses clip today")

    // Read back the painted target into the alpha-first ARGB8 shadow:
    // Buffer channels 0=A 1=R 2=G 3=B, image bytes [A,R,G,B].
    uint8_t *shadow = (*img).rgba;
    if (shadow) {
        for (uint32_t y = 0; y < rh; ++y) {
            for (uint32_t x = 0; x < rw; ++x) {
                size_t p = ((size_t) y * (size_t) rw + (size_t) x) * 4u;
                shadow[p + 0u] = (uint8_t) Buffer_getPixel(buf, x, y, 0u);
                shadow[p + 1u] = (uint8_t) Buffer_getPixel(buf, x, y, 1u);
                shadow[p + 2u] = (uint8_t) Buffer_getPixel(buf, x, y, 2u);
                shadow[p + 3u] = (uint8_t) Buffer_getPixel(buf, x, y, 3u);
            }
        }
    }
    (*self).retainGen = Component_gen();
    return true;
}

bool Component_hitTest(const Component *self, float pointX, float pointY) {
    if (!self || !(*self).visible)
        return false;
    return pointX >= (*self).absX && pointX < (*self).absX + (*self).absW
        && pointY >= (*self).absY && pointY < (*self).absY + (*self).absH;
}

void Component_getContentRect(const Component *self, float *outX, float *outY,
                              float *outW, float *outH) {
    if (!self) {
        if (outX)
            *outX = 0.0f;
        if (outY)
            *outY = 0.0f;
        if (outW)
            *outW = 0.0f;
        if (outH)
            *outH = 0.0f;
        return;
    }
    contentBox(self, outX, outY, outW, outH);
}

void Component_viewMap(const ComponentView *view, float ax, float ay, float aw, float ah,
                       float *outX, float *outY, float *outW, float *outH) {
    float x0 = ax;
    float y0 = ay;
    float w = aw;
    float h = ah;
    if (view) {
        // Provably gapless device mapping: translate (abs - origin), then
        // floor leading edges, ceil trailing edges, so adjacent cells never
        // leave a pixel gap and never overlap across a boundary — the seam's
        // single rounding currency (no drift vs Board surfaces). A null view
        // maps identity (point space, origin 0).
        x0 = floorf((ax - (*view).originX) * (*view).scaleX);
        y0 = floorf((ay - (*view).originY) * (*view).scaleY);
        float x1 = ceilf((ax + aw - (*view).originX) * (*view).scaleX);
        float y1 = ceilf((ay + ah - (*view).originY) * (*view).scaleY);
        w = x1 - x0;
        h = y1 - y0;
        if (w < 0.0f)
            w = 0.0f;
        if (h < 0.0f)
            h = 0.0f;
    }
    if (outX)
        *outX = x0;
    if (outY)
        *outY = y0;
    if (outW)
        *outW = w;
    if (outH)
        *outH = h;
}

uint64_t Component_gen(void) {
    return s_componentGen;
}

// SETTERS

void Component_setX(Component *self, float x) {
    if (!self)
        return;
    (*self).x = x;
    Component_recompute(self);
}

void Component_setY(Component *self, float y) {
    if (!self)
        return;
    (*self).y = y;
    Component_recompute(self);
}

void Component_setWidth(Component *self, float w) {
    if (!self)
        return;
    (*self).w = w;
    Component_recompute(self);
}

void Component_setHeight(Component *self, float h) {
    if (!self)
        return;
    (*self).h = h;
    Component_recompute(self);
}

void Component_setLocation(Component *self, float x, float y) {
    if (!self)
        return;
    Component_setX(self, x);
    Component_setY(self, y);
}

void Component_setSize(Component *self, float w, float h) {
    if (!self)
        return;
    if (w < (*self).minW)
        w = (*self).minW;
    if (h < (*self).minH)
        h = (*self).minH;
    if ((*self).maxW > 0.0f && w > (*self).maxW)
        w = (*self).maxW;
    if ((*self).maxH > 0.0f && h > (*self).maxH)
        h = (*self).maxH;
    Component_setWidth(self, w);
    Component_setHeight(self, h);
}

void Component_setMinSize(Component *self, float w, float h) {
    if (!self)
        return;
    (*self).minW = w;
    (*self).minH = h;
    float cw = (*self).w;
    float ch = (*self).h;
    if (cw < w)
        cw = w;
    if (ch < h)
        ch = h;
    if ((*self).maxW > 0.0f && cw > (*self).maxW)
        cw = (*self).maxW;
    if ((*self).maxH > 0.0f && ch > (*self).maxH)
        ch = (*self).maxH;
    Component_setWidth(self, cw);
    Component_setHeight(self, ch);
}

void Component_setMaxSize(Component *self, float w, float h) {
    if (!self)
        return;
    (*self).maxW = w;
    (*self).maxH = h;
    float cw = (*self).w;
    float ch = (*self).h;
    if (w > 0.0f && cw > w)
        cw = w;
    if (h > 0.0f && ch > h)
        ch = h;
    if (cw < (*self).minW)
        cw = (*self).minW;
    if (ch < (*self).minH)
        ch = (*self).minH;
    Component_setWidth(self, cw);
    Component_setHeight(self, ch);
}

void Component_setOrigin(Component *self, int origin) {
    if (!self || origin < COMPONENT_ORIGIN_TOP_LEFT || origin > COMPONENT_ORIGIN_BOTTOM_RIGHT)
        return;
    (*self).origin = (uint8_t) origin;
    Component_recompute(self);
}

void Component_setAnchor(Component *self, int anchor) {
    if (!self || anchor < COMPONENT_ANCHOR_TOP_LEFT || anchor > COMPONENT_ANCHOR_BOTTOM_RIGHT)
        return;
    (*self).anchor = (uint8_t) anchor;
    Component_recompute(self);
}

void Component_setPivot(Component *self, int pivot) {
    if (!self || pivot < COMPONENT_PIVOT_TOP_LEFT || pivot > COMPONENT_PIVOT_CENTER)
        return;
    (*self).pivot = pivot;
    Component_recompute(self);
}

void Component_setCenter(Component *self) {
    if (!self)
        return;
    Component_setPivot(self, COMPONENT_PIVOT_CENTER);
}

void Component_setMargin(Component *self, float l, float t, float r, float b) {
    if (!self)
        return;
    (*self).marginL = l;
    (*self).marginT = t;
    (*self).marginR = r;
    (*self).marginB = b;
    Component_recompute(self);
}

void Component_setPadding(Component *self, float l, float t, float r, float b) {
    if (!self)
        return;
    float pl = l < 0.0f ? 0.0f : l;
    float pt = t < 0.0f ? 0.0f : t;
    float pr = r < 0.0f ? 0.0f : r;
    float pb = b < 0.0f ? 0.0f : b;
    (*self).paddingL = pl;
    (*self).paddingT = pt;
    (*self).paddingR = pr;
    (*self).paddingB = pb;
    // Padding insets children: re-cascade the content box to every child
    // immediately (recompute bumps the generation counter).
    Component_recompute(self);
}

void Component_setBorderWidth(Component *self, float w) {
    if (!self)
        return;
    (*self).borderWidth = w < 0.0f ? 0.0f : w;
    touchGen();
}

void Component_setBorderColor(Component *self, uint32_t color) {
    if (!self)
        return;
    (*self).borderColor = color;
    touchGen();
}

void Component_setBackgroundColor(Component *self, uint32_t color) {
    if (!self)
        return;
    (*self).backgroundColor = color;
    touchGen();
}

void Component_setRadius(Component *self, float r) {
    if (!self)
        return;
    (*self).radius = r < 0.0f ? 0.0f : r;
    touchGen();
}

void Component_setRadiusMode(Component *self, int mode) {
    if (!self)
        return;
    if (mode != COMPONENT_CORNER_ARC && mode != COMPONENT_CORNER_SUPERELLIPSE)
        return;
    (*self).radiusMode = mode;
    touchGen();
}

void Component_setOpacity(Component *self, float opacity) {
    if (!self)
        return;
    (*self).opacity = opacity < 0.0f ? 0.0f : (opacity > 1.0f ? 1.0f : opacity);
    touchGen();
}

void Component_setZ(Component *self, int z) {
    if (!self)
        return;
    (*self).z = z;
    touchGen();
}

void Component_setVisible(Component *self, bool visible) {
    if (!self)
        return;
    (*self).visible = visible ? 1 : 0;
    touchGen();
}

void Component_setParent(Component *self, Component *parent) {
    if (!self)
        return;
    (*self).parent = parent;
    touchGen();
}

bool Component_addChild(Component *self, Component *child) {
    if (!self || !child || child == self)
        return false;

    // Check if already present
    for (uint32_t i = 0; i < (*self).childCount; ++i) {
        if ((*self).children[i] == child)
            return true;
    }

    if ((*self).childCount >= (*self).childCapacity) {
        uint32_t newCap = (*self).childCapacity == 0 ? 8 : (*self).childCapacity * 2;
        Component **newArr = (Component**) Memory_alloc(TYPE_COMPONENT_SINGLETON, sizeof(Component*) * newCap);
        if (!newArr)
            return false;
        if ((*self).children && (*self).childCount > 0) {
            memcpy(newArr, (*self).children, sizeof(Component*) * (*self).childCount);
        }
        (*self).children = newArr;
        (*self).childCapacity = newCap;
    }

    (*self).children[(*self).childCount++] = child;
    (*child).parent = self;
    touchGen();

    // Eager Cascade: update child abs immediately with the parent's CONTENT
    // box (abs inset by padding — padding insets children)
    float contentX = 0.0f, contentY = 0.0f, contentW = 0.0f, contentH = 0.0f;
    contentBox(self, &contentX, &contentY, &contentW, &contentH);
    Component_setParentAbs(child, contentX, contentY, contentW, contentH);
    return true;
}

bool Component_removeChild(Component *self, Component *child) {
    if (!self || !child || (*self).childCount == 0 || !(*self).children)
        return false;

    int foundIdx = -1;
    for (uint32_t i = 0; i < (*self).childCount; ++i) {
        if ((*self).children[i] == child) {
            foundIdx = (int) i;
            break;
        }
    }
    if (foundIdx < 0)
        return false;

    for (uint32_t i = (uint32_t) foundIdx; i + 1 < (*self).childCount; ++i) {
        (*self).children[i] = (*self).children[i + 1];
    }
    (*self).childCount--;
    if ((*child).parent == self) {
        (*child).parent = nullptr;
    }
    touchGen();
    return true;
}

void Component_setBackgroundRender(Component *self, Component_RenderFn fn) {
    if (!self)
        return;
    (*self).backgroundRender = fn;
    touchGen();
}

void Component_setForegroundRender(Component *self, Component_RenderFn fn) {
    if (!self)
        return;
    (*self).foregroundRender = fn;
    touchGen();
}

void Component_setRenderUserdata(Component *self, void *userdata) {
    if (!self)
        return;
    (*self).renderUserdata = userdata;
    touchGen();
}

void Component_setDeferred(Component *self, bool deferred) {
    if (!self)
        return;
    (*self).deferred = deferred ? 1 : 0;
    touchGen(); // mode flip drifts the retain: the next bake re-renders the subtree
}

// GETTERS

float Component_getX(const Component *self) { return self ? (*self).x : 0.0f; }
float Component_getY(const Component *self) { return self ? (*self).y : 0.0f; }
float Component_getWidth(const Component *self) { return self ? (*self).w : 0.0f; }
float Component_getHeight(const Component *self) { return self ? (*self).h : 0.0f; }

float Component_getAbsX(const Component *self) { return self ? (*self).absX : 0.0f; }
float Component_getAbsY(const Component *self) { return self ? (*self).absY : 0.0f; }
float Component_getAbsW(const Component *self) { return self ? (*self).absW : 0.0f; }
float Component_getAbsH(const Component *self) { return self ? (*self).absH : 0.0f; }

void Component_getAbsRect(const Component *self, Vec4 *outRect) {
    if (!outRect)
        return;
    float ax = self ? (*self).absX : 0.0f;
    float ay = self ? (*self).absY : 0.0f;
    float aw = self ? (*self).absW : 0.0f;
    float ah = self ? (*self).absH : 0.0f;
    Vec4_set(outRect, ax, ay, aw, ah);
}

void Component_getParentAbsRect(const Component *self, Vec4 *outRect) {
    if (!outRect)
        return;
    float px = self ? (*self).parentAbsX : 0.0f;
    float py = self ? (*self).parentAbsY : 0.0f;
    float pw = self ? (*self).parentAbsW : 0.0f;
    float ph = self ? (*self).parentAbsH : 0.0f;
    Vec4_set(outRect, px, py, pw, ph);
}

int Component_getOrigin(const Component *self) {
    return self ? (*self).origin : COMPONENT_ORIGIN_TOP_LEFT;
}

int Component_getAnchor(const Component *self) {
    return self ? (*self).anchor : COMPONENT_ANCHOR_TOP_LEFT;
}

int Component_getPivot(const Component *self) {
    return self ? (*self).pivot : COMPONENT_PIVOT_TOP_LEFT;
}

float Component_getMinWidth(const Component *self) { return self ? (*self).minW : 0.0f; }
float Component_getMinHeight(const Component *self) { return self ? (*self).minH : 0.0f; }
float Component_getMaxWidth(const Component *self) { return self ? (*self).maxW : 0.0f; }
float Component_getMaxHeight(const Component *self) { return self ? (*self).maxH : 0.0f; }

void Component_getMargin(const Component *self, float *outL, float *outT, float *outR, float *outB) {
    float ml = self ? (*self).marginL : 0.0f;
    float mt = self ? (*self).marginT : 0.0f;
    float mr = self ? (*self).marginR : 0.0f;
    float mb = self ? (*self).marginB : 0.0f;
    if (outL)
        *outL = ml;
    if (outT)
        *outT = mt;
    if (outR)
        *outR = mr;
    if (outB)
        *outB = mb;
}

void Component_getPadding(const Component *self, float *outL, float *outT, float *outR, float *outB) {
    float pl = self ? (*self).paddingL : 0.0f;
    float pt = self ? (*self).paddingT : 0.0f;
    float pr = self ? (*self).paddingR : 0.0f;
    float pb = self ? (*self).paddingB : 0.0f;
    if (outL)
        *outL = pl;
    if (outT)
        *outT = pt;
    if (outR)
        *outR = pr;
    if (outB)
        *outB = pb;
}

float Component_getBorderWidth(const Component *self) { return self ? (*self).borderWidth : 0.0f; }
uint32_t Component_getBorderColor(const Component *self) { return self ? (*self).borderColor : COMPONENT_COLOR_CLEAR; }
uint32_t Component_getBackgroundColor(const Component *self) { return self ? (*self).backgroundColor : COMPONENT_COLOR_CLEAR; }
float Component_getRadius(const Component *self) { return self ? (*self).radius : 0.0f; }
int Component_getRadiusMode(const Component *self) { return self ? (*self).radiusMode : COMPONENT_CORNER_ARC; }
float Component_getOpacity(const Component *self) { return self ? (*self).opacity : 1.0f; }
int Component_getZ(const Component *self) { return self ? (*self).z : 0; }
bool Component_isVisible(const Component *self) { return self && (*self).visible != 0; }
bool Component_isDeferred(const Component *self) { return self && (*self).deferred != 0; }
uint32_t Component_getRetainWidth(const Component *self) { return self ? (*self).retainW : 0u; }
uint32_t Component_getRetainHeight(const Component *self) { return self ? (*self).retainH : 0u; }
Component *Component_getParent(const Component *self) { return self ? (*self).parent : nullptr; }
uint32_t Component_getChildCount(const Component *self) { return self ? (*self).childCount : 0; }
Component *Component_getChild(const Component *self, uint32_t index) {
    if (!self || index >= (*self).childCount || !(*self).children)
        return nullptr;
    return (*self).children[index];
}
Component_RenderFn Component_getBackgroundRender(const Component *self) { return self ? (*self).backgroundRender : nullptr; }
Component_RenderFn Component_getForegroundRender(const Component *self) { return self ? (*self).foregroundRender : nullptr; }
void *Component_getRenderUserdata(const Component *self) { return self ? (*self).renderUserdata : nullptr; }