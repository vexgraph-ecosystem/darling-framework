// Include our headers BEFORE any ObjC to avoid CarbonCore's `Collection`
// typedef colliding with our struct Collection (collection.h).
#include <stddef.h>
#include <stdatomic.h>
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
 *   - Darling_attachPanelBoards(window, scenePane, contentPane, width, height)
 *     (boards are RETAINED OFFSCREEN VkLayer targets — fixed pixel size,
 *     never a CALayer, never in the window tree; the Frame seam canvas is
 *     the window's single on-screen layer per the Window Compositing
 *     Layer Order Law)
 *   - Darling_attachLayers(window, contentPanel, width, height)
 *   - Darling_propagatePaneDirty(window, scenePane, contentPanel)
 *     (Panel_isTreeDirty -> VkPane_markDirty per DIRECT chain; COMPOSITED
 *     scenes re-arm their VkLayer from owner-subtree dirt (demand signal;
 *     wall-clock handlers advance on re-render, registration demands the
 *     first render); boards re-arm their VkLayer on tree dirt; bool
 *     stores only, safe on the worker mid-drag)
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
int Darling_attachPanelBoards(Window *window, Panel *scenePane, Panel *contentPane, int width, int height) {
    if (!window || width <= 0 || height <= 0)
        return 0;
    extern float TextCore_backingScale(void);
    float scale = TextCore_backingScale();
    if (scale <= 0.0f)
        scale = 1.0f;
    int pxW = (int) (width * scale + 0.5f);
    int pxH = (int) (height * scale + 0.5f);
    if (pxW <= 0 || pxH <= 0 || pxW > 16384 || pxH > 16384)
        return 0;
    Panel *boards[2] = { scenePane, contentPane };
    extern void *PanelCocoa_fromPanel(void *panel);
    extern void *PanelCocoa_newBoard(void *panel, int w, int h);
    extern bool PanelCocoa_setSize(void *pc, int w, int h);
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
        int allocW = (int) (rect.z * scale + 0.5f);
        int allocH = (int) (rect.w * scale + 0.5f);
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

// Attach retained offscreen VkLayer targets to the COMPOSITED scene children
// of a content panel (Rule 14 default: a scene keeps a fixed pixel-size
// flight target rendered by the present worker; the canvas samples it as a
// textured quad). DIRECT scenes own Metal panes and are skipped. A layer's
// pixel size is FIXED at register time — VkLayer_resize is a no-op when the
// size is unchanged, so fixed scenes never rebuild on window resize (live
// anchoring is exactly as panes: the composite rect tracks the anchor). The
// PANEL itself is the layer's owner handle, so VkLayer_find(child) resolves
// the composite pass's child -> layer index. Returns the number of layers
// registered or resized.
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

    for (size_t i = 0; i < childCount; i++) {
        Panel *child = Panel_getChild(contentPanel, i);
        if (!child)
            continue;

        uint64_t childType = Memory_type(child);
        bool isScene = (childType == TYPE_SCENE3D_SINGLETON || childType == TYPE_SCENE2D_SINGLETON
                        || childType == TYPE_SCENE_SINGLETON);
        if (!isScene)
            continue;
        if (Scene_getPresentMode((Scene*) child) != SCENE_PRESENT_COMPOSITED)
            continue;

        Vec4 rect;
        Container_resolve(&(*child).base, 0.0f, 0.0f, (float) width, (float) height, &rect);
        int allocW = (int) (rect.z * scale + 0.5f);
        int allocH = (int) (rect.w * scale + 0.5f);
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
            if (Darling_compositorIdleForResize() && VkLayer_resize(index, allocW, allocH))
                attached++;
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

    size_t childCount = Panel_childCount(contentPanel);
    bool anySceneDirty = false;
    for (size_t i = 0; i < childCount; i++) {
        Panel *child = Panel_getChild(contentPanel, i);
        if (!child)
            continue;
        uint64_t childType = Memory_type(child);
        bool isScene = (childType == TYPE_SCENE3D_SINGLETON || childType == TYPE_SCENE2D_SINGLETON
                        || childType == TYPE_SCENE_SINGLETON);
        // COMPOSITED scenes own no Metal pane: their retained offscreen
        // target re-arms only from owner-subtree dirt (the demand signal —
        // owners mark once per motion burst; the latched dirt then buys
        // full-rate re-renders until the tick clears it, so animation
        // survives a stalled tick and rests on pause).
        if (isScene && Scene_getPresentMode((Scene*) child) == SCENE_PRESENT_COMPOSITED) {
            if (isScene || Panel_isTreeDirty(child)) {
                int layer = VkLayer_find(child);
                if (layer >= 0) {
                    VkLayer_markDirty(layer, true);
                    anySceneDirty = true;
                }
            }
            continue;
        }
        void *pc = PanelCocoa_fromPanel(child);
        if (!pc || !PanelCocoa_isMetal(pc))
            continue;
        if (isScene || Panel_isTreeDirty(child)) {
            int chain = PanelCocoa_chain(pc);
            if (chain >= 0)
                VkPane_markDirty(chain, true);
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
        bool boardDirty = Panel_isTreeDirty(board) || Window_isLiveResizing(window) || anySceneDirty;
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
            int w = (int) (rect.z * scale + 0.5f);
            int h = (int) (rect.w * scale + 0.5f);
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
