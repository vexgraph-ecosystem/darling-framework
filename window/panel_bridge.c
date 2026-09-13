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
 *   - Darling_attachPanelBoards(window, width, height)
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
//   content board ..... full-window CAMetalLayer + VkPane chain (UI subtree)
//   scene board ....... full-window CAMetalLayer + VkPane chain (scene subtree)
//   child panes ....... one CAMetalLayer + VkPane chain EACH (nested scenes)
//   window ............ CAMetalLayer, clear-transparent glass bottom
//
// Darling numbering:
//   layer 1 = window (CAMetalLayer, the glass bottom)
//   layer 2 = contentPanel board + scenePanel board (full-window Metal)
//   layer 3 = first-gen scene panes (own chain each; deeper nesting —
//             a1/a2/a3 inside a — paints inside the parent pass via
//             Vulkan render handlers, NOT as nested layers).
//   Scroll offsets reach layers through ScrollContainer_childFrame (content
//   shifts by -offset, chrome stays) — C-side resolve is the source of
//   truth, exactly as vk_test's hand-placed anchors proved.
//
// TRAFFIC LAW: every panel renders into its own VkPane chain (boards paint
// whole subtrees, scenes paint themselves) and WindowServer composites the
// stack onto the glass. Resize = layer moves via anchors, zero rebuild.
//
// Each scene child gets its own Metal pane. Plain UI paints into the board
// pass. This file iterates children and calls into ObjC PanelCocoa for the
// Metal/CALayer plumbing.

// Attach full-window Metal board backing (PanelCocoa_newBoard, one VkPane
// chain each) to the scene + content panels — the two named boards of the
// NSWindow -> Metal -> Vulkan-rect-children stack. Boards track the window
// size (VkPane_resize at settle); their subtrees paint into the board chain.
// Live-gated: mid-drag sizes freeze and the WindowServer stretches board
// drawables; the final size lands at settle. Returns the number of boards
// attached or resized.
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

// Attach Metal pane backing to the scene children of a content panel —
// every scene is a "pane of glass" (own CAMetalLayer + Vulkan swapchain).
// Plain UI needs no backing: it paints into the board pass. Spacers
// (transparent, no handler, no scene) own nothing. Returns the number of
// panes attached or resized.
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
