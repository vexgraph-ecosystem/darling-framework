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
#include "window/window.h"
#include "annotation/overview.h"

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: Panel_bridge (window/panel_bridge.c)
 * LEVEL: L4 — Self-Management (OS window/IOSurface panel glue)
 * ============================================================================
 * pure-C bridge for IOSurface panel operations.
 *
 * STRUCT FIELDS: none — procedural (pure-C IOSurface panel glue, no struct).
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - PanelCocoa_new(panel, w, h)
 *
 * Core Functions:
 *   - Darling_attachPanelIOSurfaceChildren(window, contentPanel, width, height)
 *   - Darling_attachPanelBoards(window, width, height)
 *   - TextCore_backingScale(void)
 *   - for(i++)
 *   - PanelCocoa_fromPanel(panel)
 *   - Darling_resizePanelIOSurfaceChildren(window, contentPanel, width, height)
 *   - Darling_compositeIOSurfaceChildren(window, contentPanel)
 *
 * Setters:
 *   - PanelCocoa_setSize(pc, w, h)
 *
 * Getters:
 *   - Darling_getPanelMaxSize(p, outMaxW, outMaxH)
 *   - Darling_getChildLayout(child, winW, winH, outX, outY, outW, outH)
 *   - Darling_getChildCount(contentPanel)
 *   - Darling_getChildAt(contentPanel, index)
 *   - Panel_getChild(contentPanel, index)
 *   - Darling_getPanelSize(p, outW, outH)
 *   - Darling_getChildAnchor(child)
 *   - Darling_getChildPivot(child)
 * ============================================================================
 */


// src/window/panel_bridge.c — pure-C bridge for IOSurface panel operations.
//
// LAYER MODEL (the stack, front to back):
//   scrollbar ......... own IOSurface (ScrollContainer right-dock, topmost child)
//   1st-gen children .. one IOSurface EACH (attached here, per direct child)
//   window ............ CAMetalLayer (AppKit-owned, Vulkan never touches it)
//
// Darling numbering:
//   layer 1 = window (CAMetalLayer, the composite target)
//   layer 2 = contentPanel — most honestly a SCROLLPANEL: the window IS y,
//             not a child filling it. Children a/b/c/d sit inside y under
//             ABSOLUTE layout (explicit frames, no layout manager).
//   layer 3 = first-gen panels (own IOSurface each; deeper nesting —
//             a1/a2/a3 inside a — paints inside the parent surface via
//             Vulkan render handlers (Panel_setRenderHandler, cf. vk_test's
//             hud_pulse/pic_render), NOT as nested layers/JPanels).
//   scrollbar = own IOSurface at the layer-3 edge (topmost child order).
//   Scroll offsets reach layers through ScrollContainer_childFrame (content
//   shifts by -offset, chrome stays) — C-side resolve is the source of
//   truth, exactly as vk_test's hand-placed anchors proved.
//
// TRAFFIC LAW: Vulkan renders INSIDE the IOSurfaces (producer only) and
// never presents the window; WindowServer composites the stack onto the
// CAMetalLayer. Resize = layer moves via anchors, zero repaint.
//
// Each content panel child gets an IOSurface. Vulkan renders into each
// IOSurface independently (async). AppKit composites the CALayers.
// This file iterates children and calls into ObjC PanelCocoa for the
// IOSurface/CALayer plumbing.

// Attach full-window Metal board backing (PanelCocoa_newBoard, one VkPane
// chain each) to the scene + content panels — the two named boards of the
// NSWindow -> Metal -> Vulkan-rect-children stack. Boards track the window
// size (VkPane_resize at settle); their subtrees paint into the board chain
// instead of per-child IOSurfaces. Live-gated: mid-drag sizes freeze and
// the WindowServer stretches board drawables; the final size lands at
// settle. Returns the number of boards attached or resized.
int Darling_attachPanelBoards(Window *window, int width, int height) {
    if (!window || width <= 0 || height <= 0)
        return 0;
    if (Window_isLiveResizing(window))
        return 0;
    extern float TextCore_backingScale(void);
    float scale = TextCore_backingScale();
    if (scale <= 0.0f)
        scale = 1.0f;
    int pxW = (int) (width * scale + 0.5f);
    int pxH = (int) (height * scale + 0.5f);
    if (pxW <= 0 || pxH <= 0 || pxW > 16384 || pxH > 16384)
        return 0;
    Panel *boards[2] = { Window_getScenePanel(window), Window_getContentPanel(window) };
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

// Attach IOSurface backing to ALL children of a content panel.
// Returns the number of IOSurface backings attached.
int Darling_attachPanelIOSurfaceChildren(Window *window, Panel *contentPanel, int width, int height) {
    if (!window || !contentPanel)
        return 0;
    (void) window;
    int attached = 0;
    size_t childCount = Panel_childCount(contentPanel);
    extern float TextCore_backingScale(void);
    float scale = TextCore_backingScale();
    if (scale <= 0.0f)
        scale = 1.0f;

    // Board-parent gate: when the parent paints its whole subtree into its
    // own Metal board chain, plain UI children need no IOSurface — only
    // nested scenes keep their own panes (the recursive Vulkan-rect tree).
    extern void *PanelCocoa_fromPanel(void *panel);
    extern bool PanelCocoa_isBoard(const void *pc);
    void *parentPc = PanelCocoa_fromPanel(contentPanel);
    bool parentIsBoard = parentPc && PanelCocoa_isBoard(parentPc);

    for (size_t i = 0; i < childCount; i++) {
        Panel *child = Panel_getChild(contentPanel, i);
        if (!child)
            continue;

        // Skip completely transparent panels with no render handler and no scene.
        // Pure clamp/layout spacers (like ScrollContainer bounds) must never own an IOSurface.
        uint32_t bg = Panel_getBackgroundColor(child);
        Panel_RenderFn rfn = Panel_getRenderHandler(child);
        uint64_t childType = Memory_type(child);
        bool isScene = (childType == TYPE_SCENE3D_SINGLETON || childType == TYPE_SCENE2D_SINGLETON
                        || childType == TYPE_SCENE_SINGLETON);
        if (bg == PANEL_COLOR_CLEAR && !rfn && !isScene)
            continue;

        // Board-parent gate (see above): UI paints into the board chain.
        if (parentIsBoard && !isScene)
            continue;

        // Get the child's MAX size for IOSurface allocation (fixed, never reallocates)
        int maxW = 0, maxH = 0;
        extern void Darling_getPanelMaxSize(Panel *p, int *outMaxW, int *outMaxH);
        Darling_getPanelMaxSize(child, &maxW, &maxH);
        if (maxW <= 0 || maxH <= 0) {
            // Fallback: use layout rect
            Vec4 rect;
            Container_resolve(&(*child).base, 0.0f, 0.0f, (float) width, (float) height, &rect);
            maxW = (int) (rect.z + 0.5f);
            maxH = (int) (rect.w + 0.5f);
        }
        if (maxW <= 0 || maxH <= 0)
            continue;

        int allocW = (int) (maxW * scale + 0.5f);
        int allocH = (int) (maxH * scale + 0.5f);
        if (allocW <= 0 || allocH <= 0 || allocW > 16384 || allocH > 16384)
            continue;

        // Check if already attached
        void *pc = PanelCocoa_fromPanel(child);
        if (pc) {
            extern bool PanelCocoa_setSize(void *pc, int w, int h);
            PanelCocoa_setSize(pc, allocW, allocH);
        } else if (isScene) {
            // Scene = "pane of glass": its OWN CAMetalLayer + Vulkan swapchain.
            // The window board never stamps scenes (Rule 14); this pane
            // presents independent of window resize.
            extern void *PanelCocoa_newMetal(void *panel, int w, int h);
            if (PanelCocoa_newMetal(child, allocW, allocH))
                attached++;
        } else {
            extern void *PanelCocoa_new(void *panel, int w, int h);
            if (PanelCocoa_new(child, allocW, allocH))
                attached++;
        }
    }
    return attached;
}

// Resize IOSurface backing for ALL children of a content panel.
// Returns the number of IOSurface backings resized.
int Darling_resizePanelIOSurfaceChildren(Window *window, Panel *contentPanel, int width, int height) {
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

// Composite IOSurface-backed children into the window's layer tree.
void Darling_compositeIOSurfaceChildren(Window *window, Panel *contentPanel) {
    (void)window;
    (void)contentPanel;
    // Real implementation is in window_cocoa.m (ObjC)
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

// Get panel's max size (first setSize = max, subsequent = clamped current).
void Darling_getPanelMaxSize(Panel *p, int *outMaxW, int *outMaxH) {
    if (!p || !outMaxW || !outMaxH) return;
    *outMaxW = (int)((*p).base.maxW + 0.5f);
    *outMaxH = (int)((*p).base.maxH + 0.5f);
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
