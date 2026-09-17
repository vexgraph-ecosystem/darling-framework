#include "annotation/overview.h"
#include "darling/compositor.h"
#include "darling/frame.h"
#include "darling/container.h"
#include "darling/panel/panel.h"
#include "darling/scene/scene.h"
#include "darling/field/input.h"
#include "event/dispatch.h"
#include "graphvex/gfx_loop.h"
#include "nio/mem.h"
#include "oop/type.h"
#include "time/nanotime.h"
#include "vulkan/sdf_gpu.h"
#include "vulkan/vk.h"
#include "vulkan/vk_iosurface.h"
#include "vulkan/vk_layer.h"
#include "vulkan/vk_pane.h"
#include "vulkan/vk_scene.h"
#include "vulkan/vk_view.h"
#include "window/window.h"

#include <vulkan/vulkan_core.h>
#include <stdlib.h>

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Compositor
 * LEVEL: L2 — Behavior (retained-mode UI compositing behavior API)
 * ============================================================================
 * Retained-mode UI compositor connecting Darling UI nodes and Vulkan scene
 * viewports into the host window and presentation loop. Every panel is a
 * Vulkan rect: boards (scene/content) own full-window Metal layers + VkPane
 * chains and paint their whole subtree; scenes reach the screen through one
 * of two Present-On-Demand Law destinations — COMPOSITED (default) owns a retained
 * offscreen VkLayer flight target the canvas samples as a textured quad at
 * the anchor rect, DIRECT owns a child pane (CAMetalLayer + swapchain).
 * WindowServer composites the layer tree; no IOSurface transport remains.
 *
 * STRUCT FIELDS (local to this file): none — procedural (no owned struct).
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - Darling_initCompositor(frame)
 *   - Darling_shutdownCompositor(void)
 *   - darlingGfxFrameFn(window, dt, userdata) (private) : GfxFrameFn demand
 *     probe registered into graphvex's GfxLoop (the Conflict Triage Law
 *     downward seam — the loop lives in R3, darling registers into it).
 *     Probed every resting pass (the Present-On-Demand Law): ticks the
 *     focused Input's caret blink and re-arms present demand on
 *     tree/pane/live-resize dirt — probe free, present gated on demand.
 *   - Darling_renderFrame(cmdBuffer, drawW, drawH, userdata=Frame*)
 *   - Darling_preFrame(window, drawW, drawH, userdata=Frame*)
 *     (full layout every tick: live gate was removed; setFrameSize drives
 *     layout directly per drag step; Darling_layerRender runs inside
 *     VkPane_presentAll and VkLayer_visit always)
 *   - Darling_layerRender(cmdBuffer, w, h, owner) : pane + retained-layer
 *     pass leaf (leaf panel or board subtree via paintChildIntoPass; runs
 *     every tick — the shared painter for DIRECT pane chains and COMPOSITED
 *     VkLayer targets)
 *   - paintChildIntoPass(cmdBuffer, child, ...) (private) : one child
 *     into board or board-pane pass (the Present-On-Demand Law pane-skip + COMPOSITED layer
 *     composite inside)
 *   - Darling_compositorSettled(void)          : true when no pane submit flies
 *   - Darling_compositorIdleForResize(void)    : settled alias for pane/
 *     texture resize callers — resize-class work runs only when idle
 *   - darlingRetireGuard(void) (private)       : texture-retire drain probe
 *     registered into graphvex (the Conflict Triage Law downward seam) — destroys a retired
 *     texture only when NO bindless-sampling Submit flies: the board present
 *     fence signaled AND every pane fence signaled. Closes the page-fault
 *     window where texture.c's 2-frame CPU lag freed an old image under a
 *     still-flying pane/present CB.
 *   - Darling_resizeRenderHook(userdata) (private) : WindowResizeRenderFn
 *     registered by Darling_initCompositor; called on thread 0 by
 *     VulkanView.setFrameSize once per drag step, AFTER drawableSize and
 *     all panel layouts are already updated. Drives one synchronous
 *     VkPane_presentAll (wrapped in Window_workerPresentBegin/End) so
 *     rendered content tracks the window border in real time. Thread 0
 *     exclusively owns presents during live resize; the present worker gates
 *     itself out via Window_isLiveResizing to avoid a concurrent present race.
 * ============================================================================
 */

// Accessors for Vulkan device state from vexspoke
extern VkDevice Vk_getDevice(void);
extern VkQueue Vk_getQueue(void);
extern VkCommandBuffer Vk_getCmdBuffer(void);
extern VkPipeline Vk_getTriPipeline(void);
extern VkPipelineLayout Vk_getTriLayout(void);
extern uint64_t Vk_getAnimStartNanos(void);
extern PFN_vkGetDeviceProcAddr Vk_getGdpa(void);
extern VkInstance Vk_getInstance(void);
extern PFN_vkGetInstanceProcAddr Vk_getGpa(void);
extern VkPhysicalDevice Vk_getPhys(void);
extern uint32_t Vk_getQueueFamily(void);

extern bool VkView_refreshAll(VkInstance instance, PFN_vkGetInstanceProcAddr gpa, VkPhysicalDevice phys, VkDevice device);
extern bool VkSceneCanvas_initModule(VkInstance instance, PFN_vkGetInstanceProcAddr gpa, VkPhysicalDevice phys, VkDevice device);
extern bool VkIOSurface_initModule(VkInstance instance, PFN_vkGetInstanceProcAddr gpa, VkPhysicalDevice phys, VkDevice device);
extern bool Texture_initModule(void *instance, void *gpa, void *phys, void *device, void *queue, uint32_t queueFamily);
extern void Texture_shutdown(void);
extern void Texture_setRetireGuard(bool (*guard)(void));
extern void VkView_shutdown(void);
extern void VkSceneCanvas_shutdownModule(void);

#define COMPOSITOR_LOAD_DEVICE(name) \
    static PFN_vk##name name##_fn; \
    if (!name##_fn) { \
        name##_fn = (PFN_vk##name)Vk_getGdpa()(Vk_getDevice(), "vk" #name); \
    }



// Drain query for present-on-demand loops: true when no pane submit flies.
// Loops gate their tree-dirty clear on this so in-flight pane work is
// never dropped by a clear. Replaces the retired batch-ring settled query:
// every scene paints into its own pane chain now, so pane flight IS the
// only flight.
bool Darling_compositorSettled(void) {
    return VkPane_flightIdle();
}

// Retire-guard callback registered into the texture module (the Conflict Triage Law
// canonical downward seam — the graphvex leaf never reaches up for sampler
// flight state; the composer, which owns every bindless-sampling Submit,
// answers instead). A retired texture is destroyed only when NO sampler CB
// referencing it is in flight: the board present fence signaled AND every
// pane submit fence signaled. A false answer only defers destroys
// (retireDrain retries); it never blocks, allocates, or samples the driver
// beyond GetFenceStatus polls. This closes the page-fault window where
// texture.c's 2-frame CPU lag freed an old image while a flying
// pane/present CB still read it. Trade (the Cold-Strict, Hot-Minimal Validation Law): while sampler submits
// saturate, retired rows stay ringed and resize/free rollovers drop-degrade
// to keep-old-content until a quiescent tick.
static bool darlingRetireGuard(void) {
    if (!Vk_presentFlightIdle())
        return false;
    if (!VkPane_flightIdle())
        return false;
    return true;
}

// Resize gate for pane/texture callers: resize-class work (a
// Texture_replaceRaw that changes dimensions) runs only when no pane
// submit flies; otherwise the caller defers to a same-size update or
// skips the tick. Headless-safe: true with no flight.
bool Darling_compositorIdleForResize(void) {
    return VkPane_flightIdle();
}

// // CORE FUNCTIONS

// Paint one child panel into an already-begun render pass — shared by the
// legacy board (Darling_renderFrame) and board panes (Darling_layerRender
// subtree walk). Scenes always paint (handler or tri fallback); plain UI
// paints only when paintUI is set (content-board subtree — the legacy
// board stamps scenes alone). Pane-backed children never paint here: they
// present their OWN chain (the Present-On-Demand Law, recursive Vulkan-rect tree).
static void paintChildIntoPass(void *cmdBuffer, Panel *child, float winW, float winH, float kx, float ky, float drawW, float drawH, bool paintUI) {
    if (!cmdBuffer || !child)
        return;
    Vec4 rect;
    Container_resolve(&(*child).base, 0.0f, 0.0f, winW, winH, &rect);
    if (rect.z <= 0.0f || rect.w <= 0.0f)
        return;

    uint64_t cType = Memory_type(child);
    bool childIsScene = (cType == TYPE_SCENE3D_SINGLETON || cType == TYPE_SCENE2D_SINGLETON
                         || cType == TYPE_SCENE_SINGLETON);
    if (!childIsScene && !paintUI)
        return;

    // Pane-backed child (CAMetalLayer + own swapchain): renders into its
    // OWN chain — the pass must never stamp it (the Present-On-Demand Law).
    extern void *PanelCocoa_fromPanel(void *panel);
    extern bool PanelCocoa_isMetal(const void *pc);
    void *panePc = PanelCocoa_fromPanel(child);
    if (panePc && PanelCocoa_isMetal(panePc))
        return;

    float px = rect.x * kx;
    float py = rect.y * ky;
    float pw = rect.z * kx;
    float ph = rect.w * ky;
    if (px < 0.0f) { pw += px; px = 0.0f; }
    if (py < 0.0f) { ph += py; py = 0.0f; }
    if (pw <= 0.0f || ph <= 0.0f) return;
    if (px + pw > drawW) pw = drawW - px;
    if (py + ph > drawH) ph = drawH - py;
    if (pw <= 0.0f || ph <= 0.0f) return;

    uint64_t childType = Memory_type(child);
    bool isScene = (childType == TYPE_SCENE3D_SINGLETON || childType == TYPE_SCENE2D_SINGLETON
                    || childType == TYPE_SCENE_SINGLETON);

    // COMPOSITED scene (the Present-On-Demand Law): its pixels live in a retained offscreen
    // VkLayer flight target rendered by the present worker. The CANVAS (the
    // board pass — paintUI=true) SAMPLES the last-published frame as a
    // textured quad at the anchor rect — the scene's render handler is NEVER
    // invoked here (composite != render). The legacy glass pass
    // (paintUI=false) is the transparent backdrop, NOT a canvas: it must skip
    // layer-backed scenes exactly like pane-backed ones, or every scene is
    // stamped TWICE (once into the window swapchain, once into the board).
    if (isScene && Scene_getPresentMode((Scene*) child) == SCENE_PRESENT_COMPOSITED) {
        if (!paintUI)
            return;
        int layer = VkLayer_find(child);
        if (layer >= 0)
            VkLayer_composite(cmdBuffer, drawW, drawH, layer, px, py, pw, ph,
                              1.0f, 1.0f, 1.0f, 1.0f);
        return;
    }

    if (isScene) {
        Panel_RenderFn handler = Panel_getRenderHandler(child);
        if (handler) {
            handler(child, nullptr, cmdBuffer, drawW, drawH, px, py, pw, ph);
        } else {
            COMPOSITOR_LOAD_DEVICE(CmdSetViewport);
            COMPOSITOR_LOAD_DEVICE(CmdSetScissor);
            COMPOSITOR_LOAD_DEVICE(CmdBindPipeline);
            COMPOSITOR_LOAD_DEVICE(CmdPushConstants);
            COMPOSITOR_LOAD_DEVICE(CmdDraw);

            float uTime = (float)((double)(NanoTime_now() - Vk_getAnimStartNanos()) / 1e9);
            CmdBindPipeline_fn((VkCommandBuffer) cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Vk_getTriPipeline());
            CmdPushConstants_fn((VkCommandBuffer) cmdBuffer, Vk_getTriLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, 4, &uTime);
            VkViewport vp = { .x = px, .y = py, .width = pw, .height = ph, .minDepth = 0.0f, .maxDepth = 1.0f };
            VkRect2D sc = { .offset = { (int32_t)px, (int32_t)py }, .extent = { (uint32_t)pw, (uint32_t)ph } };
            CmdSetViewport_fn((VkCommandBuffer) cmdBuffer, 0, 1, &vp);
            CmdSetScissor_fn((VkCommandBuffer) cmdBuffer, 0, 1, &sc);
            CmdDraw_fn((VkCommandBuffer) cmdBuffer, 3, 1, 0, 0);

            VkViewport defaultVp = { .x = 0.0f, .y = 0.0f, .width = drawW, .height = drawH, .minDepth = 0.0f, .maxDepth = 1.0f };
            VkRect2D defaultSc = { .offset = { 0, 0 }, .extent = { (uint32_t)drawW, (uint32_t)drawH } };
            CmdSetViewport_fn((VkCommandBuffer) cmdBuffer, 0, 1, &defaultVp);
            CmdSetScissor_fn((VkCommandBuffer) cmdBuffer, 0, 1, &defaultSc);
        }
    } else {
        Panel_RenderFn handler = Panel_getRenderHandler(child);
        if (handler) {
            handler(child, nullptr, cmdBuffer, drawW, drawH, px, py, pw, ph);
        } else {
            uint32_t color = Panel_getBackgroundColor(child);
            if (color != 0) {
                float r = (float)((color >> 16) & 0xFF) / 255.0f;
                float g = (float)((color >> 8)  & 0xFF) / 255.0f;
                float b = (float)( color        & 0xFF) / 255.0f;
                float a = (float)((color >> 24) & 0xFF) / 255.0f
                    * Container_getOpacity(&(*child).base);
                if (a > 0.0f)
                    Vk_fillRect(cmdBuffer, drawW, drawH, px, py, pw, ph, r, g, b, a);
            }
        }
    }
}

// Pane render hook: called by VkPane_presentAll per CAMetalLayer pane, inside
// that pane's OWN render pass (already begun, cleared, viewport at 0,0 = pane
// size). A leaf pane renders its Panel handler (or the fallback spinning tri
// for a scene with no handler). A board pane (scene/content panel with
// children) paints its whole subtree — scenes AND ui — through
// paintChildIntoPass, so nested panes keep their own chains while everything
// else lands in the board pass. Mirror of the board's Darling_renderFrame
// scene path, but with NO window-absolute transform — a pane owns its whole
// extent.
static void Darling_layerRender(void *cmdBuffer, int w, int h, void *owner) {
    Panel *panel = (Panel*) owner;
    if (!panel || !cmdBuffer || w <= 0 || h <= 0)
        return;

    // Resize contract (the Ecosystem Vulkan Safety Nets Law): this pane paints into its OWN chain — never
    // the shared batch CB — so steady rendering proceeds regardless of batch
    // flight. Resize-class work the handler triggers (a Texture_replaceRaw
    // that changes dimensions) must consult Darling_compositorIdleForResize
    // first: when the ring flies it defers to a same-size update or skips
    // the tick, exactly like the pane drift-defer in the resize gate.

    size_t childCount = Panel_childCount(panel);
    if (childCount == 0) {
        Panel_RenderFn handler = Panel_getRenderHandler(panel);
        if (handler) {
            handler(panel, nullptr, cmdBuffer, (float) w, (float) h, 0.0f, 0.0f, (float) w, (float) h);
            return;
        }

        uint64_t panelType = Memory_type(panel);
        bool isScene = (panelType == TYPE_SCENE3D_SINGLETON || panelType == TYPE_SCENE2D_SINGLETON
                        || panelType == TYPE_SCENE_SINGLETON);
        if (!isScene)
            return;

        // Fallback scene content: the animated triangle, full pane.
        COMPOSITOR_LOAD_DEVICE(CmdSetViewport);
        COMPOSITOR_LOAD_DEVICE(CmdSetScissor);
        COMPOSITOR_LOAD_DEVICE(CmdBindPipeline);
        COMPOSITOR_LOAD_DEVICE(CmdPushConstants);
        COMPOSITOR_LOAD_DEVICE(CmdDraw);

        float uTime = (float)((double)(NanoTime_now() - Vk_getAnimStartNanos()) / 1e9);
        CmdBindPipeline_fn((VkCommandBuffer) cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Vk_getTriPipeline());
        CmdPushConstants_fn((VkCommandBuffer) cmdBuffer, Vk_getTriLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, 4, &uTime);
        VkViewport vp = { .x = 0.0f, .y = 0.0f, .width = (float) w, .height = (float) h, .minDepth = 0.0f, .maxDepth = 1.0f };
        VkRect2D sc = { .offset = { 0, 0 }, .extent = { (uint32_t) w, (uint32_t) h } };
        CmdSetViewport_fn((VkCommandBuffer) cmdBuffer, 0, 1, &vp);
        CmdSetScissor_fn((VkCommandBuffer) cmdBuffer, 0, 1, &sc);
        CmdDraw_fn((VkCommandBuffer) cmdBuffer, 3, 1, 0, 0);
        return;
    }

    // Board subtree: children resolve against the board's own point size,
    // scaled to pane pixels — the same contract the legacy board honors
    // against window points.
    extern void Darling_getPanelSize(Panel *p, int *outW, int *outH);
    int panelW = 0, panelH = 0;
    Darling_getPanelSize(panel, &panelW, &panelH);
    if (panelW <= 0 || panelH <= 0)
        return;
    float kx = (float) w / (float) panelW;
    float ky = (float) h / (float) panelH;

    // Board's own backdrop: the board panel IS the window surface (the
    // Window Board Root Lock Law) — paint its background as the full-pane
    // first op so a styled root (dark app backdrop) shows through between
    // children, instead of the pane chain's transparent-black clear.
    uint32_t bgColor = Panel_getBackgroundColor(panel);
    if (bgColor != 0) {
        float r = ((bgColor >> 16) & 0xFF) / 255.0f;
        float g = ((bgColor >> 8) & 0xFF) / 255.0f;
        float b = (bgColor & 0xFF) / 255.0f;
        float a = ((bgColor >> 24) & 0xFF) / 255.0f;
        if (a > 0.0f)
            Vk_fillRect(cmdBuffer, (float) w, (float) h, 0.0f, 0.0f, (float) w, (float) h, r, g, b, a);
    }

    for (size_t i = 0; i < childCount; i++) {
        Panel *child = Panel_getChild(panel, i);
        if (!child)
            continue;
        paintChildIntoPass(cmdBuffer, child, (float) panelW, (float) panelH, kx, ky, (float) w, (float) h, true);
    }
}

void Darling_preFrame(Window *window, int drawW, int drawH, void *userdata) {
    (void)userdata;
    if (!window) return;

    // LIVE RESIZE GATE, LAYOUT-ONLY (the Continuous Real-Time Live Resize Law): during a drag thread 0 owns
    // ALL layer motion — setFrameSize's synchronous composite pins every pane
    // to its selfAnchor per drag step. The present worker must NOT mutate
    // container layout or composite layers concurrently: that race tears the
    // anchor math and pane layers drift away from their pinned corners. Only
    // the layout block below is gated; repaint-demand propagation and the
    // clear-color refresh run every tick (zero layer mutation), and pane
    // painting runs inside VkPane_presentAll through Darling_layerRender —
    // never through preFrame — so the four scenes keep animating the whole
    // drag. On settle the flag clears and preFrame resumes: one layout, one
    // final re-record, one board rebuild. Layer mutation stays thread-0-only
    // (composite calls self-gate + dispatch to main).
    bool live = Window_isLiveResizing(window);

    int winW = Window_width(window);
    int winH = Window_height(window);
    if (winW <= 0 || winH <= 0) return;

    (void)drawW;
    (void)drawH;

    // Panes resolve from the borrowing Frame (userdata) — never the Window.
    Frame *frame = (Frame*) userdata;
    if (frame == nullptr)
        return;
    Panel *root = Frame_getRootPanel(frame);
    Panel *contentPanel = Frame_getContentPane(frame);
    Panel *scenePanel = Frame_getScenePane(frame);
    (void)live;
    // Boards first: scene + content panels attach their full-window Metal
    // boards here so the pane attach below sees board backing (its
    // metal-parent gate) and the subtree painters see board sizes.
    extern int Darling_attachPanelBoards(Window *window, Panel *scenePane, Panel *contentPane, int width, int height);
    Darling_attachPanelBoards(window, scenePanel, contentPanel, winW, winH);

    if (contentPanel) {
        static int s_lastCompW = 0, s_lastCompH = 0;
        static int s_lastCompChildren = -1;
        int curChildCount = (int)Panel_childCount(contentPanel);
        bool needsComposite = (winW != s_lastCompW || winH != s_lastCompH
                               || curChildCount != s_lastCompChildren
                               || Panel_isTreeDirty(contentPanel));
        if (!needsComposite) {
            extern void *PanelCocoa_fromPanel(void *panel);
            for (int ci = 0; ci < curChildCount; ci++) {
                Panel *child = Panel_getChild(contentPanel, ci);
                if (!child)
                    continue;
                uint64_t t = Memory_type(child);
                bool scene = (t == TYPE_SCENE3D_SINGLETON || t == TYPE_SCENE2D_SINGLETON
                              || t == TYPE_SCENE_SINGLETON);
                if (scene || PanelCocoa_fromPanel(child)) {
                    needsComposite = true;
                    break;
                }
            }
        }
        Container_setSize(&(*contentPanel).base, (float)winW, (float)winH);
        // Children are Vulkan rects:
        Window_attachPanes(window, contentPanel, winW, winH);
        extern int Darling_attachLayers(Window *window, Panel *contentPanel, int width, int height);
        Darling_attachLayers(window, contentPanel, winW, winH);

        // Propagate repaint demand into pane chains before clearing tree dirty
        extern void Darling_propagatePaneDirty(Window *window, Panel *scenePane, Panel *contentPane);
        Darling_propagatePaneDirty(window, scenePanel, contentPanel);

        if (needsComposite) {
            Window_compositePanes(window, contentPanel);
            Panel_clearTreeDirty(contentPanel);
            s_lastCompW = winW;
            s_lastCompH = winH;
            s_lastCompChildren = curChildCount;
        }
    }

    // Board layers parent under the root layer every tick (scene below
    // content); the call self-gates live resize and off-thread delivery.
    Window_compositeBoards(window);

    if (scenePanel)
        Container_setSize(&(*scenePanel).base, (float)winW, (float)winH);

    // Every tick, live or settled: clear-color refresh (uniform update only,
    // zero layer mutation — safe on the worker mid-drag).
    if (scenePanel) {
        uint32_t bgColor = Panel_getBackgroundColor(scenePanel);
        if (bgColor != 0) {
            float r = ((bgColor >> 16) & 0xFF) / 255.0f;
            float g = ((bgColor >> 8) & 0xFF) / 255.0f;
            float b = (bgColor & 0xFF) / 255.0f;
            float a = ((bgColor >> 24) & 0xFF) / 255.0f;
            Vk_setClearColor(r, g, b, a);
        }
    } else if (root && root != contentPanel) {
        uint32_t bgColor = Panel_getBackgroundColor(root);
        if (bgColor != 0) {
            float r = ((bgColor >> 16) & 0xFF) / 255.0f;
            float g = ((bgColor >> 8) & 0xFF) / 255.0f;
            float b = (bgColor & 0xFF) / 255.0f;
            float a = ((bgColor >> 24) & 0xFF) / 255.0f;
            Vk_setClearColor(r, g, b, a);
        }
    }
}

void Darling_renderFrame(void *cmdBuffer, int drawW, int drawH, void *userdata) {
    Frame *rframe = (Frame*) userdata;
    Window *window = rframe ? Frame_getWindow(rframe) : nullptr;
    if (!window || !cmdBuffer) return;

    int winW = Window_width(window);
    int winH = Window_height(window);
    if (winW <= 0 || winH <= 0) return;

    float kx = (float)drawW / (float)winW;
    float ky = (float)drawH / (float)winH;

    Panel *root = Frame_getRootPanel(rframe);

    // Board-owned scenes (the Present-On-Demand Law): a metal-backed scenePanel paints its
    // whole subtree into its own board chain — the legacy board stamps
    // nothing and degrades to its clear pass.
    Panel *boardScene = Frame_getScenePane(rframe);
    if (boardScene) {
        extern void *PanelCocoa_fromPanel(void *panel);
        extern bool PanelCocoa_isBoard(const void *pc);
        void *boardPc = PanelCocoa_fromPanel(boardScene);
        if (boardPc && PanelCocoa_isBoard(boardPc))
            return;
    }

    if (root) {
        size_t childCount = Panel_childCount(root);
        for (size_t i = 0; i < childCount; i++) {
            Panel *child = Panel_getChild(root, i);
            if (!child) continue;
            paintChildIntoPass(cmdBuffer, child, (float)winW, (float)winH, kx, ky, (float)drawW, (float)drawH, false);
        }
    }
}

// Live-resize render hook — thread 0 only.
//
// Called by VulkanView.setFrameSize once per drag step, AFTER:
//   - cachedWidth/cachedHeight updated
//   - drawableSize updated on the CAMetalLayer
//   - Darling_setPanelSize, attachPanelBoards, compositeBoards,
//     compositePanes, markLiveDirty, propagatePaneDirty all done
//
// All that remains is one synchronous present at the new size so rendered
// content tracks the window border in real time. The present worker is gated
// out of VkPane_presentAll during live resize (Window_isLiveResizing gate in
// kernel_present_job) so this call is the sole presenter during the drag —
// no concurrent present race possible.
//
// Window_workerPresentBegin/End wrap the call so board panels, which are
// presentsWithTransaction=YES, release their drawable inside this explicit
// transaction instead of stalling for an implicit runloop commit that
// never arrives on thread 0 during the AppKit modal tracking loop.
static void Darling_resizeRenderHook(void *userdata) {
    Frame *hframe = (Frame*) userdata;
    Window *w = hframe ? Frame_getWindow(hframe) : nullptr;
    if (!w || !Vk_ready())
        return;
    // Genie gate (defense in depth — the pump already skips the hook while
    // miniaturized): never re-composite the layer tree or present off a
    // window the WindowServer is warping into or out of the dock.
    if (Window_isMinimized(w))
        return;
    // One preFrame pass to catch any layout the setFrameSize helpers may have
    // queued asynchronously (e.g. Window_compositeBoards dispatched to main).
    int winW = Window_width(w);
    int winH = Window_height(w);
    if (winW <= 0 || winH <= 0)
        return;
    Darling_preFrame(w, winW, winH, userdata);
    // Synchronous present: wraps in an explicit CATransaction so
    // presentsWithTransaction=YES board drawables are released here.
    Window_workerPresentBegin();
    if (VkPane_count() == 0) {
        Vk_clearPresent();
    } else {
        VkPane_presentAll();
    }
    Window_workerPresentEnd();
}

// The CAMetalLayer is created by the FRAME (FrameCocoa_attach) and stored in
// frame->nativeView; R1's window deliberately holds zero Metal, so its
// Window_metalLayer stub returns nullptr by design (the Window Decoupling
// Law). Advertising that stub to graphvex is why the VK_EXT_metal_surface
// path failed. The compositor therefore advertises the FRAME's layer: R3
// still receives one opaque void* and stays OS-free.
static Frame *s_seamFrame = nullptr;

static void *darlingSeamMetalLayer(void *window) {
    (void) window;
    return s_seamFrame ? Frame_getNativeView(s_seamFrame) : nullptr;
}

// GfxLoop demand probe (the Present-On-Demand Law) — the Conflict Triage Law
// downward seam (the frame loop lives in graphvex R3; darling registers into
// it, never the reverse). GfxLoop_step probes this EVERY pass, so it is the
// loop's only window onto demand while resting:
//   - caret blink: phase flips are the one demand the setters never see —
//     tick the focused Input so its dirt lands through the Input markDirty
//     path exactly on phase change;
//   - present demand: a live resize, any DIRTY pane chain (the boards
//     themselves, DIRECT scenes, or COMPOSITED layer re-arms propagated by
//     Darling_propagatePaneDirty), or a dirty panel tree re-arms the client.
// Deliberately tiny: thread 0, struct reads + one markDirty only — no
// layout, no driver calls, no allocation (the Cold-Strict, Hot-Minimal
// Validation Law hot path). The probe is free; the present gate stays strict.
static void darlingGfxFrameFn(void *window, double dt, void *userdata) {
    Frame *hframe = (Frame*) userdata;
    if (!hframe || !window)
        return;

    Panel *focus = Darling_getFocusedPanel();
    if (focus && Memory_type(focus) == TYPE_INPUT_SINGLETON)
        Input_caret_tick((Input*) focus, dt);

    bool demand = false;
    if (Window_isLiveResizing((Window*) window))
        demand = true;
    if (VkPane_hasDemand())
        demand = true;
    if (Panel_isTreeDirty(Frame_getContentPane(hframe)))
        demand = true;
    if (Panel_isTreeDirty(Frame_getScenePane(hframe)))
        demand = true;
    if (demand)
        GfxLoop_markDirty(GfxLoop_default(), window);
}

void Darling_initCompositor(Frame *frame) {
    Window *window = frame ? Frame_getWindow(frame) : nullptr;
    if (!window) return;

    if (!Vk_ready()) {
        s_seamFrame = frame;
        Vk_setWindowSeam(window,
                         darlingSeamMetalLayer,
                         (bool (*)(void *))Window_isTransparent,
                         (VkWindowPresentMode (*)(void *))Window_getPresentMode,
                         (uint64_t (*)(void *))Window_renderGeneration,
                         (bool (*)(void *))Window_isLiveResizing,
                         (void (*)(void *, void *, void *))Window_setResizeRenderHook,
                         (void (*)(void *))Window_setGravityTopLeft,
                         (bool (*)(void *))Window_isMinimized);
        // Fail closed: a failed init leaves null instance/device handles, and
        // every module-init call below dereferences them (the Cold-Strict,
        // Hot-Minimal Validation Law: never crash). Report and stay dark.
        if (!Vk_init() || !Vk_ready()) {
            fprintf(stderr, "darling: Vulkan init failed (%s) — the frame stays dark\n", Vk_status());
            return;
        }
    }

    VkInstance inst = Vk_getInstance();
    PFN_vkGetInstanceProcAddr gpa = Vk_getGpa();
    VkPhysicalDevice phys = Vk_getPhys();
    VkDevice dev = Vk_getDevice();
    VkQueue queue = Vk_getQueue();
    uint32_t qf = Vk_getQueueFamily();

    VkView_refreshAll(inst, gpa, phys, dev);
    VkSceneCanvas_initModule(inst, gpa, phys, dev);
    Texture_initModule(inst, (void*) gpa, phys, dev, queue, qf);
    SdfGpu_initModule(inst, (void*) gpa, phys, dev, queue, qf);

    // Texture-retire destroy guard: this composer owns the only bindless-
    // sampling Submits (batch ring + board present frame renderer + panes),
    // so it certifies when a retired texture's memory is provably
    // unreferenced. Without it texture.c falls back to a CPU frame lag that
    // can free an old image under a still-flying batch/pane/present CB and
    // page-fault the GPU (the Ecosystem Vulkan Safety Nets Law net).
    Texture_setRetireGuard(darlingRetireGuard);

    Vk_setPreFrameRenderer((VkPreFrameFn)Darling_preFrame, frame);
    Vk_setFrameRenderer(Darling_renderFrame, frame);

    // Pane hook: per-CAMetalLayer swapchain children render through here.
    VkPane_setRenderer(Darling_layerRender);
    // Layer hook: COMPOSITED scene targets render through the same painter —
    // a retained offscreen target is just a pane whose pixels the canvas
    // samples instead of a Metal surface presenting them.
    VkLayer_setRenderer(Darling_layerRender);

    // Live-resize present hook: called by VulkanView.setFrameSize on thread 0
    // once per drag step. Drives one synchronous VkPane_presentAll so content
    // tracks the window border in real time. Must be registered AFTER Vk_init
    // so the pane/layer hooks are installed and the first present is valid.
    Window_setResizeRenderHook(window, Darling_resizeRenderHook, frame);

    // Register into graphvex's GfxLoop: the frame loop runs our probe every
    // pass (demand re-arm + caret blink) and presents through the board
    // panes. Registered dirty on arrival -> the first loop step presents the
    // initial composite. Unregistered in Darling_shutdownCompositor.
    GfxLoop_registerClient(GfxLoop_default(), window, nullptr, nullptr, nullptr,
                           darlingGfxFrameFn, frame);
}

void Darling_shutdownCompositor(void) {
    // Deregister the GfxLoop client first so the frame loop stops probing
    // and presenting a window that is tearing down (the Teardown Order Law:
    // detach before free).
    Window *w = s_seamFrame ? Frame_getWindow(s_seamFrame) : nullptr;
    if (w)
        GfxLoop_unregisterClient(GfxLoop_default(), w);

    Vk_setPreFrameRenderer(nullptr, nullptr);
    Vk_setFrameRenderer(nullptr, nullptr);

    // No GPU teardown here: scenes paint into their own pane chains
    // (VkPane_shutdown owns them) and the board holds no resources.
    Texture_shutdown();
    SdfGpu_shutdown();
    VkView_shutdown();
    VkSceneCanvas_shutdownModule();
}

bool Darling_hitTest(Panel *p, float px, float py) {
    if (!p)
        return false;
    float pw = Container_getWidth(&(*p).base);
    float ph = Container_getHeight(&(*p).base);
    size_t childCount = Panel_childCount(p);
    if (childCount == 0) {
        return Container_hitTest(&(*p).base, 0.0f, 0.0f, pw, ph, px, py);
    }
    Vec4 rect;
    for (size_t i = 0; i < childCount; i++) {
        Panel *child = Panel_getChild(p, i);
        if (!child || !Container_isVisible(&(*child).base))
            continue;
        Container_resolve(&(*child).base, 0.0f, 0.0f, pw, ph, &rect);
        if (px >= rect.x && px < rect.x + rect.z && py >= rect.y && py < rect.y + rect.w)
            return true;
    }
    return false;
}


