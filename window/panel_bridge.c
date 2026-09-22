// Include our headers BEFORE any ObjC to avoid CarbonCore's `Collection`
// typedef colliding with our struct Collection (collection.h).
#include <stddef.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Edge-snapping pixel math (the Single Rounding Currency Law). Rounding
// SIZES (lround(w*scale)) lets two containers sharing an edge round it to
// different sides — a toggling 1px crack/overlap mid-drag — and an alloc
// derived as lround(lround(points) × scale) double-rounds, toggling ±1px as
// a drag crosses a .5 boundary. Snapping EDGES and subtracting keeps
// adjacent quads / layer allocations on one shared device-pixel grid
// aligned with the paint pass's compositorSnapEdge (compositor.c).
static int edgeSnapPx(float deviceEdge) {
    return (int) floorf(deviceEdge + 0.5f);
}
#include "oop/type.h"
#include "nio/mem.h"
#include "lang/vec4.h"
#include "darling/container.h"
#include "darling/component.h"
#include "darling/panel/panel.h"
#include "darling/scene/scene.h"
#include "vulkan/vk_layer.h"
#include "window/window.h"
#include "annotation/definition.h"
#include "annotation/overview.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Panel_bridge
 * ============================================================================
 * Pure-C bridge between the darling panel tree and the OS window/Metal stack.
 * The layer model (front to back) is: the Frame's seam canvas — the window's
 * SINGLE on-screen CAMetalLayer — composites the two retained offscreen
 * board images in z-order (content top, scene bottom) per the Window
 * Compositing Layer Order Law; COMPOSITED scenes live inside their board's
 * retained pass. Classification-driven retained flight targets follow the
 * Immediate vs Retained Element Model: a depth-1 child owns a retained
 * offscreen VkLayer flight target ONLY when its subtree contains retained
 * output (a COMPOSITED scene); all-immediate / retained-texture subtrees
 * paint INLINE into the board pass — zero targets, zero copies. Boards
 * track the drawable every drag step (the Native Pixel Law, px =
 * lround(rect * override-pinned TextCore_backingScale) from live points)
 * with idle-gated resize per the Single-Seam Canvas Law, and children are
 * iterated via Panel_childCount per the Dynamic Scalability &
 * Anti-Hardcoding Law. The bridge owns no struct — it is procedural glue
 * over Panel, Frame, and VkLayer.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: Panel_bridge (window/panel_bridge.c)
 * LEVEL: L4 — Self-Management (OS window/Metal board glue)
 * ============================================================================
 * pure-C bridge between the darling panel tree and the Metal layer stack.
 *
 * STRUCT FIELDS: none — procedural (pure-C board glue, no struct).
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - Darling_attachPanelBoards(window, scenePane, contentPane, width, height, drawW, drawH)
 *     (boards are full-window containers resized to the live window bounds)
 *   - Darling_attachLayers(window, boardPanel, width, height)
 *     (classification-driven retained flight targets per the Immediate vs
 *     Retained Element Model — darling.md section 55: a depth-1 child owns
 *     a retained offscreen VkLayer flight target at fixed pixel size ONLY
 *     when its subtree contains retained output — a COMPOSITED scene
 *     rendered on its own timeline and collaged. All-immediate /
 *     retained-texture (I / R) subtrees paint INLINE into the board pass
 *     via the recursive subtree walk in compositor.c — zero targets, zero
 *     copies, zero memory for plain UI. A child reclassified to I/R
 *     unregisters its stale layer, idle-gated exactly like the resize path
 *     (its targets may still be referenced by a flying seam sampler — the
 *     unregister wait drains them). Children iterated via
 *     Panel_childCount, never hardcoded counts, per the Dynamic
 *     Scalability & Anti-Hardcoding Law; dirty=true on register per the
 *     Single-Seam Canvas Law; GRAPHICS_VK_STATS-gated vk:child-resize
 *     log on true drift only, old extent via VkLayer_extent; px =
 *     lround(rect * override-pinned TextCore_backingScale) from live
 *     points — no signature change, the pin flows through the seam)
 *   - Darling_propagatePaneDirty(window, scenePane, contentPanel)
 *     (retained child layers re-arm from owner-subtree dirt and arm board
 *     demand one hop on child PUBLISH (VkLayer_presentCount delta against
 *     a per-board snapshot — dirt dies on publish inside VkLayer_visit,
 *     so dirt is never the publish signal); content-child demand arms
 *     ONLY the content board and scene demand arms ONLY from the scene
 *     board's own children, never cross-talk; boards re-arm their VkLayer
 *     on tree dirt; bool stores only, safe on the worker mid-drag)
 *   - TextCore_backingScale(void)                 (extern, objc/text_core.m)
 *   - Darling_setPanelSize(p, w, h)
 *
 * Getters:
 *   - Darling_getPanelSize(p, outW, outH)
 * ============================================================================
 */

// window/panel_bridge.c — pure-C bridge between the panel tree and the
// Metal layer stack.
//
// LAYER MODEL (the stack, front to back):
//   seam canvas ........ the Frame's CAMetalLayer — the window's SINGLE
//                        on-screen layer; the seam pass composites the two
//                        retained board images in z-order (content top,
//                        scene bottom) and presents on demand
//   content board ...... retained offscreen VkLayer target (UI subtree;
//                        samples every COMPOSITED scene layer)
//   scene board ........ retained offscreen VkLayer target (scene subtree)
//   COMPOSITED layers .. retained offscreen VkLayer targets — NO
//                        CAMetalLayer; the canvas samples them as quads
//   window ............ blur view only (clear-transparent glass bottom)
//
// Darling numbering:
//   layer 1 = seam canvas (CAMetalLayer, the glass bottom)
//   layer 2 = contentPanel board + scenePanel board (retained offscreen
//             VkLayer targets — RENDERED, never presented)
//   layer 3 = COMPOSITED scene targets (retained offscreen VkLayer flight
//             images, sampled by the board pass as collaged quads)
//   Scroll offsets reach layers through ScrollContainer_childFrame (content
//   shifts by -offset, chrome stays) — C-side resolve is the source of
//   truth, exactly as vk_test's hand-placed anchors proved.
//
// TRAFFIC LAW: COMPOSITED scenes render into retained VkLayer flight
// targets (boards paint whole subtrees); the seam pass composites the
// published board images and WindowServer composites the single canvas
// layer onto the glass. Resize = the seam canvas frame tracks the window
// natively (autoresizingMask); boards keep their fixed pixel extent (the
// Single-Seam Canvas Law).

// Update board dimensions to track the live window width and height —
// and, on the first settle, REGISTER each board as a retained VkLayer
// target at the seam's FIXED monitor-native extent (Vk_seamMaxExtent), never
// the window's drawable px: the board image and the seam buffer/IOSurface are
// the same thing at the same size (the Single-Seam Canvas Law), so the seam
// pass composites them at the full chain rect and the window crops its
// top-left region 1:1. Registration is idempotent (VkLayer_find gate) and the
// targets are NEVER resized — even mid-drag, live points re-render into the
// fixed targets (no per-step rebuild, no fence churn). VkLayer_resize stays a
// no-op on unchanged size, so settled re-attaches cost nothing.
// Returns the number of boards sized.
int Darling_attachPanelBoards(Window *window, Panel *scenePane, Panel *contentPane, int width, int height, int drawW, int drawH) {
    (void) window;
    if (width <= 0 || height <= 0)
        return 0;
    int done = 0;
    if (scenePane) {
        Component *sceneMeta = &(*scenePane).component;
        Component_setSize(sceneMeta, (float) width, (float) height);
        Component_setParentAbs(sceneMeta, 0.0f, 0.0f, (float) width, (float) height);
        done++;
    }
    if (contentPane) {
        Component *contentMeta = &(*contentPane).component;
        Component_setSize(contentMeta, (float) width, (float) height);
        Component_setParentAbs(contentMeta, 0.0f, 0.0f, (float) width, (float) height);
        done++;
    }

    extern int VkLayer_register(int width, int height, void *owner);
    extern int VkLayer_find(void *owner);
    extern void Vk_seamMaxExtent(int32_t *outW, int32_t *outH);
    int32_t maxW = 0;
    int32_t maxH = 0;
    Vk_seamMaxExtent(&maxW, &maxH);
    if (maxW <= 0 || maxH <= 0) {
        // Degenerate seam: no display-backed extent published yet (headless
        // probe, chain not built). Fall back to the drawable px so the board
        // targets still exist; the seam composite falls back to inline paint
        // until the chain carries a real monitor extent.
        maxW = (int32_t) drawW;
        maxH = (int32_t) drawH;
    }
    if (maxW <= 0 || maxH <= 0)
        return done;
    if (scenePane && VkLayer_find(scenePane) < 0)
        VkLayer_register(maxW, maxH, scenePane);
    if (contentPane && VkLayer_find(contentPane) < 0)
        VkLayer_register(maxW, maxH, contentPane);
    return done;
}

// Classification probe (the Immediate vs Retained Element Model — darling.md
// section 55): true when the subtree contains retained output — a
// COMPOSITED scene whose pixels are produced on its own timeline and
// sampled as a collaged quad. All-immediate / retained-texture-only (I / R)
// subtrees answer false: every widget already paints itself through its own
// render handler, so such subtrees paint inline into the board pass
// (paintChildIntoPass's recursive subtree walk) with zero flight targets,
// zero copies, zero memory. Walk is bounded by panel tree depth and runs on
// the attach path only (cold, the Cold-Strict, Hot-Minimal Validation Law).
static bool panelSubtreeNeedsRetained(Panel *p) {
    if (!p)
        return false;
    uint64_t t = Memory_type(p);
    bool isScene = (t == TYPE_SCENE3D_SINGLETON || t == TYPE_SCENE2D_SINGLETON
                    || t == TYPE_SCENE_SINGLETON);
    if (isScene && Scene_getPresentMode((Scene*) p) == SCENE_PRESENT_COMPOSITED)
        return true;
    size_t n = Panel_childCount(p);
    for (size_t i = 0; i < n; i++) {
        if (panelSubtreeNeedsRetained(Panel_getChild(p, i)))
            return true;
    }
    return false;
}

// Attach retained offscreen VkLayer targets to the depth-1 children of a
// board whose SUBTREE contains retained output (classification, the
// Immediate vs Retained Element Model: retained presentables are the scene
// panel, the content panel, and the COMPOSITED-scene-carrying subtrees
// attached here; the board pass collages each retained child's
// last-published frame — everything else, the all-immediate chrome and the
// retained-texture (text raster / image) parts every widget already paints
// through its own render handler, paints INLINE into the board pass via the
// recursive subtree painter in compositor.c (paintChildIntoPass): zero
// full-window flight targets, zero copies, zero memory for plain UI).
// A layer's pixel size is FIXED at
// register time — VkLayer_resize is a no-op when the size is unchanged, so
// fixed children never rebuild on window resize (live anchoring: the
// composite rect tracks the anchor). The PANEL itself is the layer's owner
// handle, so VkLayer_find(child) resolves the composite pass's child ->
// layer index. A child reclassified to I/R unregisters its stale layer,
// idle-gated exactly like the resize path (its targets may still be
// referenced by a flying seam sampler — the unregister wait drains them).
// Children are iterated via Panel_childCount — never hardcoded counts — per
// the Dynamic Scalability & Anti-Hardcoding Law. Returns the number of
// layers registered or resized.
int Darling_attachLayers(Window *window, Panel *contentPanel, int width, int height) {
    if (!window || !contentPanel)
        return 0;
    (void) window;
    int attached = 0;
    size_t childCount = Panel_childCount(contentPanel);
    extern float TextCore_backingScale(void);
    float scale = TextCore_backingScale();
    if (scale <= 0.0f)
        scale = 1.0f;
    static int s_childDiag = -1;
    if (s_childDiag < 0)
        s_childDiag = getenv("GRAPHICS_VK_STATS") != nullptr || getenv("ANTI_VK_STATS") != nullptr;

    for (size_t i = 0; i < childCount; i++) {
        Panel *child = Panel_getChild(contentPanel, i);
        if (!child)
            continue;

        // Classification (the Immediate vs Retained Element Model, darling.md
        // section 55): a depth-1 child owns a retained offscreen target only
        // when its SUBTREE contains retained output — a
        // COMPOSITED scene rendered on its own timeline and collaged.
        // All-immediate / retained-texture (I / R) subtrees paint INLINE
        // into the board pass via paintChildIntoPass's recursive subtree
        // walk (compositor.c) — zero targets, zero copies, zero memory. A
        // child reclassified to I/R unregisters its stale layer, idle-gated
        // exactly like the resize path (its targets may still be referenced
        // by a flying seam sampler — the unregister wait drains them);
        // VkLayer_unregister false (budget-bound wait) leaves the layer for
        // the next attach tick (drop-degrade, the Bounded Wait Law).
        if (!panelSubtreeNeedsRetained(child)) {
            int stale = VkLayer_find(child);
            if (stale >= 0) {
                extern bool Darling_compositorIdleForResize(void);
                if (Darling_compositorIdleForResize())
                    VkLayer_unregister(stale);
            }
            continue;
        }

        Vec4 rect;
        Component *childMeta = &(*child).component;
        Component_setParentAbs(childMeta, 0.0f, 0.0f, (float) width, (float) height);
        Component_getAbsRect(childMeta, &rect);
        // Edge-snapped alloc px (edgeSnapPx above): same device grid as the
        // paint pass's quads, no double-round at .5 fractional boundaries.
        int allocW = edgeSnapPx((rect.x + rect.z) * scale) - edgeSnapPx(rect.x * scale);
        int allocH = edgeSnapPx((rect.y + rect.w) * scale) - edgeSnapPx(rect.y * scale);
        if (allocW <= 0 || allocH <= 0 || allocW > 16384 || allocH > 16384)
            continue;

        int index = VkLayer_find(child);
        if (index >= 0) {
            // Rebuild gate: a layer resize tears down flight
            // images the canvas's already-submitted composite pass may still
            // reference. Defer the resize to a quiescent tick (Darling's
            // layer flight = every submit that samples layers has drained);
            // same gate targets and textures honor. Unchanged sizes would
            // no-op inside resize; being gated they just wait a tick —
            // harmless.
            extern bool Darling_compositorIdleForResize(void);
            if (Darling_compositorIdleForResize()) {
                VkExtent2D cur = VkLayer_extent(index);
                int oldW = (int) cur.width;
                int oldH = (int) cur.height;
                if (s_childDiag && (oldW != allocW || oldH != allocH))
                    fprintf(stderr, "vk:child-resize %s rect=%.0f,%.0f,%.0f,%.0f %dx%d -> %dx%d\n", "scene", rect.x, rect.y, rect.z, rect.w, oldW, oldH, allocW, allocH);
                if (VkLayer_resize(index, allocW, allocH))
                    attached++;
            }
        } else if (VkLayer_register(allocW, allocH, child) >= 0) {
            attached++;
        }
    }
    return attached;
}

// Propagate repaint demand into retained layer chains (Panel_isTreeDirty ->
// VkLayer_markDirty). Runs every tick from Darling_preFrame's ungated tail —
// including mid-drag — and performs zero layer mutation (bool stores only),
// so it is safe on the present worker while thread 0 owns layer motion.
// Demand-gated (immediate-on-demand): COMPOSITED layers re-arm only from
// owner-subtree dirt — time-driven handlers advance by wall clock on every
// re-render, so one mark per motion burst buys full-rate frames until the
// tick clears it; registration/resize demand the first render. Boards
// re-arm only on tree dirt and otherwise rest on their stale drawable via
// the present walk's clean-skip. Clearing happens per-chain after a
// successful present.
void Darling_propagatePaneDirty(Window *window, Panel *scenePane, Panel *contentPanel) {
    if (!window || !contentPanel)
        return;

    // Per-board snapshot of depth-1 child publish counts (one-hop on
    // publish): the summed VkLayer_presentCount over each board's retained
    // children at the last pass — index 0 scene, index 1 content.
    // VkLayer_visit clears child dirt ON PUBLISH, so the publish signal dies
    // the same tick it is born when read as dirt; the present-count delta
    // survives it (presentCount bumps on every publish).
    static uint64_t s_boardChildPublish[2] = { 0u, 0u };

    // Depth-1 retained children of the CONTENT board (classification, the
    // Immediate vs Retained Element Model): owner-subtree dirt re-arms the
    // child's own flight target; a child PUBLISH since the last pass
    // (present-count delta) arms content-board demand one hop so the board
    // re-collages the fresh frame in the same-tick visit (next tick at the
    // latest). Inline (I/R) children own no target — their dirt reaches the
    // board through the board's own Panel_isTreeDirty subtree recursion.
    size_t childCount = Panel_childCount(contentPanel);
    bool anyContentDemand = false;
    uint64_t contentPublish = 0u;
    for (size_t i = 0; i < childCount; i++) {
        Panel *child = Panel_getChild(contentPanel, i);
        if (!child)
            continue;
        int childLayer = VkLayer_find(child);
        if (childLayer < 0)
            continue;
        if (Panel_isTreeDirty(child)) {
            VkLayer_markDirty(childLayer, true);
            anyContentDemand = true;
        } else if (VkLayer_isDirty(childLayer)) {
            anyContentDemand = true;
        }
        contentPublish += VkLayer_presentCount(childLayer);
    }
    if (contentPublish != s_boardChildPublish[1]) {
        anyContentDemand = true;
        s_boardChildPublish[1] = contentPublish;
    }

    // Scene board arms ONLY from its own children — never content's. The
    // scene board owns no depth-1 layers today, so this loop is the forward
    // path: when it gains them they re-arm here under the same contract.
    bool anySceneDemand = false;
    uint64_t scenePublish = 0u;
    if (scenePane) {
        size_t sceneCount = Panel_childCount(scenePane);
        for (size_t i = 0; i < sceneCount; i++) {
            Panel *child = Panel_getChild(scenePane, i);
            if (!child)
                continue;
            int childLayer = VkLayer_find(child);
            if (childLayer < 0)
                continue;
            if (Panel_isTreeDirty(child)) {
                VkLayer_markDirty(childLayer, true);
                anySceneDemand = true;
            } else if (VkLayer_isDirty(childLayer)) {
                anySceneDemand = true;
            }
            scenePublish += VkLayer_presentCount(childLayer);
        }
        if (scenePublish != s_boardChildPublish[0]) {
            anySceneDemand = true;
            s_boardChildPublish[0] = scenePublish;
        }
    }
    (void) anySceneDemand;
    (void) anyContentDemand;
}

// Get panel's current display size.
void Darling_getPanelSize(Panel *p, int *outW, int *outH) {
    if (!p || !outW || !outH) return;
    Component *meta = &(*p).component;
    *outW = (int) lroundf(Component_getWidth(meta));
    *outH = (int) lroundf(Component_getHeight(meta));
}

void Darling_setPanelSize(Panel *p, float w, float h) {
    if (!p) return;
    Component_setSize(&(*p).component, w, h);
}