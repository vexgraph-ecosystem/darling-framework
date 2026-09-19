// Include our headers BEFORE any ObjC to avoid CarbonCore's `Collection`
// typedef colliding with our struct Collection (collection.h).
#include <stddef.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "oop/type.h"
#include "nio/mem.h"
#include "lang/vec4.h"
#include "darling/container.h"
#include "darling/panel/panel.h"
#include "darling/panel/scroll_container.h"
#include "darling/scene/scene.h"
#include "vulkan/vk_layer.h"
#include "window/window.h"
#include "annotation/overview.h"

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: Panel_bridge (window/panel_bridge.c)
 * LEVEL: L4 — Self-Management (OS window/Metal panel glue)
 * ============================================================================
 * pure-C bridge for Metal pane operations.
 *
 * STRUCT FIELDS: none — procedural (pure-C Metal panel glue, no struct).
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - PanelCocoa_newMetal(panel, w, h)
 *   - PanelCocoa_newBoard(panel, w, h)
 *
 * Core Functions:
 *   - Darling_attachPanes(window, contentPanel, width, height)
  *   - Darling_attachPanelBoards(window, scenePane, contentPane, width, height, drawW, drawH)
  *     (boards are RETAINED OFFSCREEN VkLayer targets — fixed pixel size,
  *     never a CALayer, never in the window tree; the Frame seam canvas is
  *     the window's single on-screen layer per the Window Compositing
  *     Layer Order Law; width/height are live points, drawW/drawH are the
  *     live drawable px — board px == drawable px directly when drawW/drawH
  *     are positive (the Native Pixel Law, never width * stale scale), else
  *     a width * TextCore_backingScale fallback via lround — the scale is
  *     override-pinned to the dragged window's liveScale every drag step
  *     (frameCocoaResizeHook setter, cleared on settle), so the fallback
  *     never reads mainScreen mid-drag and never breathes; board setSize
  *     idle-gated via Darling_compositorIdleForResize per the Pane-of-Glass
  *     Law on the settle path, BYPASSED while Window_isLiveResizing
  *     (boards track the drawable every drag step — the retired chains
  *     drain 3 generations later, so immediate resize is safe); first-time
  *     PanelCocoa_newBoard ungated; GRAPHICS_VK_STATS-gated vk:board-resize
  *     log on true drift only)
 *   - Darling_attachLayers(window, boardPanel, width, height)
 *     (classification-driven retained flight targets per the Immediate vs
 *     Retained Element Model — darling.md section 55: a depth-1 child owns
 *     a retained offscreen VkLayer flight target at fixed pixel size ONLY
 *     when its subtree contains retained-output (RR) content — a COMPOSITED
 *     scene rendered on its own timeline and collaged. All-immediate /
 *     retained-texture (I / R) subtrees paint INLINE into the board pass via
 *     the recursive subtree walk in compositor.c — zero targets, zero
 *     copies, zero memory for plain UI. A child reclassified to I/R
 *     unregisters its stale layer, idle-gated exactly like the resize path
 *     (its targets may still be referenced by a flying seam sampler — the
 *     unregister wait drains them). DIRECT-pane children keep their
 *     exception per the Conflict Triage Law and are skipped here.
  *     Children iterated via Panel_childCount, never hardcoded counts, per
  *     the Dynamic Scalability & Anti-Hardcoding Law; dirty=true on register
  *     per the Pane-of-Glass Law; GRAPHICS_VK_STATS-gated vk:child-resize
  *     log on true drift only, old extent via VkLayer_extent; px =
  *     lround(rect * override-pinned TextCore_backingScale) from live
  *     points — no signature change, the pin flows through the seam)
 *   - Darling_propagatePaneDirty(window, scenePane, contentPanel)
 *     (Panel_isTreeDirty -> VkPane_markDirty per DIRECT chain; retained
 *     child layers re-arm from owner-subtree dirt and arm board demand one
 *     hop on child PUBLISH (VkLayer_presentCount delta against a per-board
 *     snapshot — dirt dies on publish inside VkLayer_visit, so dirt is never
 *     the publish signal); content-child demand arms ONLY the content board
 *     and scene demand arms ONLY from the scene board's own children, never
 *     cross-talk; boards re-arm their VkLayer on tree dirt; bool stores only,
 *     safe on the worker mid-drag)
 *   - TextCore_backingScale(void)
 *   - for(i++)
 *   - PanelCocoa_fromPanel(panel)
 *   - Darling_resizePanes(window, contentPanel, width, height)
 *
 * Setters:
 *   - PanelCocoa_setSize(pc, w, h)
 *
 * Getters:
 *   - Darling_getChildLayout(child, winW, winH, outX, outY, outW, outH)
 *   - Darling_getChildCount(contentPanel)
 *   - Darling_getChildAt(contentPanel, index)
 *   - Panel_getChild(contentPanel, index)
 *   - Darling_getPanelSize(p, outW, outH)
 *   - Darling_getChildAnchor(child)
 *   - Darling_getChildPivot(child)
 * ============================================================================
 */


// src/window/panel_bridge.c — pure-C bridge for Metal pane operations.
//
// LAYER MODEL (the stack, front to back):
//   seam canvas ........ the Frame's CAMetalLayer — the window's SINGLE
//                        on-screen layer; the seam pass composites the two
//                        retained board images in z-order (content top,
//                        scene bottom) and presents on demand
//   content board ...... retained offscreen VkLayer target (UI subtree;
//                        samples every COMPOSITED scene layer)
//   scene board ........ retained offscreen VkLayer target (scene subtree)
//   child panes ........ one CAMetalLayer + VkPane chain EACH (DIRECT scenes)
//   COMPOSITED layers .. retained offscreen VkLayer targets — NO
//                        CAMetalLayer; the canvas samples them as quads
//   window ............ blur view only (clear-transparent glass bottom)
//
// Darling numbering:
//   layer 1 = seam canvas (CAMetalLayer, the glass bottom)
//   layer 2 = contentPanel board + scenePanel board (retained offscreen
//             VkLayer targets — RENDERED, never presented)
//   layer 3 = DIRECT scene panes (own chain each; deeper nesting —
//             a1/a2/a3 inside a — paints inside the parent pass via
//             Vulkan render handlers, NOT as nested layers).
//   COMPOSITED scenes live inside the content board pass — one canvas
//   total, no per-scene surfaces.
//   Scroll offsets reach layers through ScrollContainer_childFrame (content
//   shifts by -offset, chrome stays) — C-side resolve is the source of
//   truth, exactly as vk_test's hand-placed anchors proved.
//
// TRAFFIC LAW: DIRECT scenes render into their own VkPane chains and
// COMPOSITED scenes into retained VkLayer flight targets (boards paint whole
// subtrees); the seam pass composites the published board images and
// WindowServer composites the single canvas layer onto the glass. Resize
// = the seam canvas frame tracks the window natively (autoresizingMask);
// boards keep their fixed pixel extent (the Pane-of-Glass Law).
//
// Each DIRECT scene child gets its own Metal pane; each COMPOSITED scene
// child gets a retained offscreen target. Plain UI paints into the board
// pass. This file iterates children and calls into ObjC PanelCocoa for the
// Metal/CALayer plumbing.

// Attach board backing to the scene + content panels: retained OFFSCREEN
// VkLayer targets (PanelCocoa_newBoard) at fixed pixel size — never a
// CALayer, never in the window tree (the seam canvas is the window's single
// on-screen layer). The seam pass composites the published board images in
// z-order (scene bottom, content top). Returns the number of boards attached
// or resized.
int Darling_attachPanelBoards(Window *window, Panel *scenePane, Panel *contentPane, int width, int height, int drawW, int drawH) {
    if (!window || width <= 0 || height <= 0)
        return 0;
    int pxW = 0;
    int pxH = 0;
    if (drawW > 0 && drawH > 0) {
        pxW = drawW;
        pxH = drawH;
    } else {
        extern float TextCore_backingScale(void);
        float scale = TextCore_backingScale();
        if (scale <= 0.0f)
            scale = 1.0f;
        pxW = (int) lround((double) width * (double) scale);
        pxH = (int) lround((double) height * (double) scale);
    }
    if (pxW <= 0 || pxH <= 0 || pxW > 16384 || pxH > 16384)
        return 0;
    Panel *boards[2] = { scenePane, contentPane };
    extern void *PanelCocoa_fromPanel(void *panel);
    extern void *PanelCocoa_newBoard(void *panel, int w, int h);
    extern bool PanelCocoa_setSize(void *pc, int w, int h);
    extern int PanelCocoa_width(const void *pc);
    extern int PanelCocoa_height(const void *pc);
    extern bool Darling_compositorIdleForResize(void);
    static int s_boardDiag = -1;
    if (s_boardDiag < 0)
        s_boardDiag = getenv("GRAPHICS_VK_STATS") != nullptr || getenv("ANTI_VK_STATS") != nullptr;
    int done = 0;
    for (int i = 0; i < 2; i++) {
        Panel *board = boards[i];
        if (!board)
            continue;
        void *pc = PanelCocoa_fromPanel(board);
        if (pc) {
            extern bool PanelCocoa_isBoard(const void *pc);
            if (!PanelCocoa_isBoard(pc))
                continue;
            // Idle-gate parity with the child path (Darling_attachLayers):
            // a board resize rebuilds flight targets the composite pass may
            // still reference — defer to a quiescent tick per the
            // Pane-of-Glass Law and retry next tick. First-time
            // PanelCocoa_newBoard below stays ungated. Live-drag bypass:
            // while Window_isLiveResizing the boards track the drawable
            // every step (the Continuous Real-Time Live Resize Law) — the
            // graveyard retires old chains 3 generations later, so
            // immediate resize is safe; the settle path keeps the gate.
            bool live = Window_isLiveResizing(window);
            if (!live && !Darling_compositorIdleForResize())
                continue;
            int oldW = PanelCocoa_width(pc);
            int oldH = PanelCocoa_height(pc);
            if (s_boardDiag && (oldW != pxW || oldH != pxH))
                fprintf(stderr, "vk:board-resize %s %dx%d -> %dx%d\n", i == 0 ? "scene" : "content", oldW, oldH, pxW, pxH);
            if (PanelCocoa_setSize(pc, pxW, pxH))
                done++;
        } else if (PanelCocoa_newBoard(board, pxW, pxH)) {
            done++;
        }
    }
    return done;
}

// Attach Metal pane backing to the DIRECT scene children of a content panel —
// the "pane of glass" model: own CAMetalLayer + Vulkan swapchain (Rule 11.5,
// the managed exception for full-window or latency-locked scenes). COMPOSITED
// scenes (Rule 14 default) are skipped here and registered as retained
// offscreen VkLayer targets by Darling_attachLayers instead. Plain UI needs
// no backing: it paints into the board pass. Spacers (transparent, no
// handler, no scene) own nothing. Returns the number of panes attached or
// resized.
int Darling_attachPanes(Window *window, Panel *contentPanel, int width, int height) {
    if (!window || !contentPanel)
        return 0;
    (void) window;
    int attached = 0;
    size_t childCount = Panel_childCount(contentPanel);
    extern float TextCore_backingScale(void);
    float scale = TextCore_backingScale();
    if (scale <= 0.0f)
        scale = 1.0f;

    extern void *PanelCocoa_fromPanel(void *panel);
    extern bool PanelCocoa_setSize(void *pc, int w, int h);
    extern void *PanelCocoa_newMetal(void *panel, int w, int h);

    for (size_t i = 0; i < childCount; i++) {
        Panel *child = Panel_getChild(contentPanel, i);
        if (!child)
            continue;

        uint64_t childType = Memory_type(child);
        bool isScene = (childType == TYPE_SCENE3D_SINGLETON || childType == TYPE_SCENE2D_SINGLETON
                        || childType == TYPE_SCENE_SINGLETON);
        if (!isScene)
            continue;
        // COMPOSITED scenes own no Metal surface — they render into a
        // retained VkLayer and the canvas samples it (Darling_attachLayers).
        if (Scene_getPresentMode((Scene*) child) != SCENE_PRESENT_DIRECT)
            continue;

        Vec4 rect;
        Container_resolve(&(*child).base, 0.0f, 0.0f, (float) width, (float) height, &rect);
        int allocW = (int) lround((double) rect.z * (double) scale);
        int allocH = (int) lround((double) rect.w * (double) scale);
        if (allocW <= 0 || allocH <= 0 || allocW > 16384 || allocH > 16384)
            continue;

        void *pc = PanelCocoa_fromPanel(child);
        if (pc) {
            if (PanelCocoa_setSize(pc, allocW, allocH))
                attached++;
        } else if (PanelCocoa_newMetal(child, allocW, allocH)) {
            attached++;
        }
    }
    return attached;
}

// Classification probe (the Immediate vs Retained Element Model — darling.md
// section 55): true when the subtree contains retained-output (RR) content —
// a COMPOSITED scene whose pixels are produced on its own timeline and
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
// board whose SUBTREE contains retained-output content (classification, the
// Immediate vs Retained Element Model: retained presentables are the scene
// panel, the content panel, and the COMPOSITED-scene-carrying subtrees
// attached here; the board pass collages each retained child's
// last-published frame — everything else, the all-immediate chrome and the
// retained-texture (text raster / image) parts every widget already paints
// through its own render handler, paints INLINE into the board pass via the
// recursive subtree painter in compositor.c (paintChildIntoPass): zero
// full-window flight targets, zero copies, zero memory for plain UI).
// DIRECT children (Metal pane + own swapchain) keep their exception per the
// Conflict Triage Law and are skipped here. A layer's pixel size is FIXED at
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
    extern void *PanelCocoa_fromPanel(void *panel);
    extern bool PanelCocoa_isMetal(const void *pc);
    static int s_childDiag = -1;
    if (s_childDiag < 0)
        s_childDiag = getenv("GRAPHICS_VK_STATS") != nullptr || getenv("ANTI_VK_STATS") != nullptr;

    for (size_t i = 0; i < childCount; i++) {
        Panel *child = Panel_getChild(contentPanel, i);
        if (!child)
            continue;

        // DIRECT-pane exception (the Conflict Triage Law): a child with its
        // own CAMetalLayer + swapchain renders and presents its own chain —
        // never a retained collage target. Same dirty/publish contract.
        void *childPc = PanelCocoa_fromPanel(child);
        if (childPc && PanelCocoa_isMetal(childPc))
            continue;
        uint64_t childType = Memory_type(child);
        bool isScene = (childType == TYPE_SCENE3D_SINGLETON || childType == TYPE_SCENE2D_SINGLETON
                        || childType == TYPE_SCENE_SINGLETON);
        if (isScene && Scene_getPresentMode((Scene*) child) == SCENE_PRESENT_DIRECT)
            continue;

        // Classification (the Immediate vs Retained Element Model, darling.md
        // section 55): a depth-1 child owns a retained offscreen target only
        // when its SUBTREE contains retained-output (RR) content — a
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
        Container *childBase = &(*child).base;
        Container_resolve(childBase, 0.0f, 0.0f, (float) width, (float) height, &rect);
        int allocW = (int) lround((double) rect.z * (double) scale);
        int allocH = (int) lround((double) rect.w * (double) scale);
        if (allocW <= 0 || allocH <= 0 || allocW > 16384 || allocH > 16384)
            continue;

        int index = VkLayer_find(child);
        if (index >= 0) {
            // Rebuild gate (Rule 39): a layer resize tears down flight
            // images the canvas's already-submitted composite pass may still
            // reference. Defer the resize to a quiescent tick (Darling's
            // pane flight = every submit that samples layers has drained);
            // same gate panes and textures honor. Unchanged sizes would no-op
            // inside resize; being gated they just wait a tick — harmless.
            extern bool Darling_compositorIdleForResize(void);
            if (Darling_compositorIdleForResize()) {
                VkExtent2D cur = VkLayer_extent(index);
                int oldW = (int) cur.width;
                int oldH = (int) cur.height;
                if (s_childDiag && (oldW != allocW || oldH != allocH))
                    fprintf(stderr, "vk:child-resize %s rect=%.0f,%.0f,%.0f,%.0f %dx%d -> %dx%d\n", isScene ? "scene" : "ui", rect.x, rect.y, rect.z, rect.w, oldW, oldH, allocW, allocH);
                if (VkLayer_resize(index, allocW, allocH))
                    attached++;
            }
        } else if (VkLayer_register(allocW, allocH, child) >= 0) {
            attached++;
        }
    }
    return attached;
}

// Propagate repaint demand into pane chains (Panel_isTreeDirty ->
// VkPane_markDirty). Runs every tick from Darling_preFrame's ungated tail —
// including mid-drag — and performs zero layer mutation (bool stores only),
// so it is safe on the present worker while thread 0 owns layer motion.
// Demand-gated (immediate-on-demand): COMPOSITED layers re-arm only from
// owner-subtree dirt — time-driven handlers advance by wall clock on every
// re-render, so one mark per motion burst buys full-rate frames until the
// tick clears it; registration/resize demand the first render. Static panes
// and boards re-arm only on tree dirt and otherwise rest on their stale
// drawable via the present walk's clean-skip (Rule 14: pane-backed scenes
// own their chains and never force the board). Clearing happens per-chain
// after a successful present.
void Darling_markLiveDirty(Panel *contentPanel) {
    if (!contentPanel)
        return;
    size_t childCount = Panel_childCount(contentPanel);
    for (size_t i = 0; i < childCount; i++) {
        Panel *child = Panel_getChild(contentPanel, i);
        if (child)
            Container_markDirty(&(*child).base);
    }
}

void Darling_propagatePaneDirty(Window *window, Panel *scenePane, Panel *contentPanel) {
    if (!window || !contentPanel)
        return;
    extern void *PanelCocoa_fromPanel(void *panel);
    extern bool PanelCocoa_isMetal(const void *pc);
    extern bool PanelCocoa_isBoard(const void *pc);
    extern int PanelCocoa_chain(const void *pc);
    extern void VkPane_markDirty(int index, bool dirty);

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
    // DIRECT-pane children keep their exception (the Conflict Triage Law):
    // own chain, own present, same dirty/publish contract — they never arm
    // the board.
    size_t childCount = Panel_childCount(contentPanel);
    bool anyContentDemand = false;
    uint64_t contentPublish = 0u;
    for (size_t i = 0; i < childCount; i++) {
        Panel *child = Panel_getChild(contentPanel, i);
        if (!child)
            continue;
        int childLayer = VkLayer_find(child);
        if (childLayer >= 0) {
            if (Panel_isTreeDirty(child)) {
                VkLayer_markDirty(childLayer, true);
                anyContentDemand = true;
            } else if (VkLayer_isDirty(childLayer)) {
                anyContentDemand = true;
            }
            contentPublish += VkLayer_presentCount(childLayer);
            continue;
        }
        void *pc = PanelCocoa_fromPanel(child);
        if (!pc || !PanelCocoa_isMetal(pc))
            continue;
        if (Panel_isTreeDirty(child)) {
            int chain = PanelCocoa_chain(pc);
            if (chain >= 0)
                VkPane_markDirty(chain, true);
        }
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

    Panel *boards[2] = { scenePane, contentPanel };
    for (int i = 0; i < 2; i++) {
        Panel *board = boards[i];
        if (!board)
            continue;
        void *bpc = PanelCocoa_fromPanel(board);
        if (!bpc || !PanelCocoa_isBoard(bpc))
            continue;
        int chain = PanelCocoa_chain(bpc);
        if (chain < 0)
            continue;
        bool childDemand = (i == 0) ? anySceneDemand : anyContentDemand;
        bool boardDirty = Panel_isTreeDirty(board) || Window_isLiveResizing(window) || childDemand;
        if (boardDirty)
            VkLayer_markDirty(chain, true);
    }
}

// Resize Metal pane backing for the backed children of a content panel.
// Fixed panes no-op on unchanged size; boards follow the window.
// Returns the number of panes resized.
int Darling_resizePanes(Window *window, Panel *contentPanel, int width, int height) {
    if (!window || !contentPanel)
        return 0;
    (void) window;
    int resized = 0;
    size_t childCount = Panel_childCount(contentPanel);
    extern float TextCore_backingScale(void);
    float scale = TextCore_backingScale();
    if (scale <= 0.0f)
        scale = 1.0f;

    for (size_t i = 0; i < childCount; i++) {
        Panel *child = Panel_getChild(contentPanel, i);
        if (!child)
            continue;
        extern void *PanelCocoa_fromPanel(void *panel);
        void *pc = PanelCocoa_fromPanel(child);
        if (pc) {
            Vec4 rect;
            Container_resolve(&(*child).base, 0.0f, 0.0f, (float) width, (float) height, &rect);
            int w = (int) lround((double) rect.z * (double) scale);
            int h = (int) lround((double) rect.w * (double) scale);
            if (w > 16384) w = 16384;
            if (h > 16384) h = 16384;
            if (w > 0 && h > 0) {
                extern bool PanelCocoa_setSize(void *pc, int w, int h);
                PanelCocoa_setSize(pc, w, h);
                resized++;
            }
        }
    }
    return resized;
}

// Layout helper for ObjC side (avoids pulling darling/panel.h into ObjC).
// C-side resolve is the source of truth: when the child's parent is a
// ScrollContainer (the window-IS-y model), the frame goes through
// ScrollContainer_childFrame so content shifts by -offset while chrome (the
// bar) stays — raw Container_resolve would pin scrolled content in place.
void Darling_getChildLayout(Panel *child, float winW, float winH, float *outX, float *outY, float *outW, float *outH) {
    if (!child || !outX || !outY || !outW || !outH) return;
    Panel *parent = Panel_getParent(child);
    if (parent && Memory_type(parent) == TYPE_SCROLL_PANEL_SINGLETON) {
        ScrollContainer_childFrame((ScrollContainer *)parent, child, winW, winH, outX, outY, outW, outH);
        return;
    }
    Vec4 rect;
    Container_resolve(&(*child).base, 0.0f, 0.0f, winW, winH, &rect);
    *outX = rect.x;
    *outY = rect.y;
    *outW = rect.z;
    *outH = rect.w;
}

// Child iteration helpers for ObjC side (avoids pulling panel.h into ObjC).
int Darling_getChildCount(Panel *contentPanel) {
    if (!contentPanel) return 0;
    return (int)Panel_childCount(contentPanel);
}

Panel *Darling_getChildAt(Panel *contentPanel, int index) {
    if (!contentPanel) return nullptr;
    return Panel_getChild(contentPanel, index);
}

// Get panel's current display size.
void Darling_getPanelSize(Panel *p, int *outW, int *outH) {
    if (!p || !outW || !outH) return;
    *outW = (int)((*p).base.w + 0.5f);
    *outH = (int)((*p).base.h + 0.5f);
}

int Darling_getChildAnchor(Panel *child) {
    if (!child) return CONTAINER_ANCHOR_TOP_LEFT;
    return Container_getAnchor(&(*child).base);
}

int Darling_getChildPivot(Panel *child) {
    if (!child) return CONTAINER_PIVOT_TOP_LEFT;
    return Container_getPivot(&(*child).base);
}

void Darling_setPanelSize(Panel *p, float w, float h) {
    if (!p) return;
    Container_setSize(&(*p).base, w, h);
}
