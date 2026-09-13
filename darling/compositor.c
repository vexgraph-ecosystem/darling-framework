#include "annotation/overview.h"
#include "darling/compositor.h"
#include "darling/container.h"
#include "darling/panel/panel.h"
#include "darling/scene/scene.h"
#include "nio/mem.h"
#include "oop/type.h"
#include "time/nanotime.h"
#include "vulkan/sdf_gpu.h"
#include "vulkan/vk.h"
#include "vulkan/vk_iosurface.h"
#include "vulkan/vk_pane.h"
#include "vulkan/vk_guard.h"
#include "vulkan/vk_scene.h"
#include "vulkan/vk_view.h"
#include "window/window.h"

#include <vulkan/vulkan_core.h>
#include <IOSurface/IOSurface.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Compositor
 * LEVEL: L2 — Behavior (retained-mode UI compositing behavior API)
 * ============================================================================
 * Retained-mode UI compositor connecting Darling UI nodes and Vulkan scene
 * viewports into the host window and presentation loop. Every panel is a
 * Vulkan rect: boards (scene/content) own full-window Metal layers + VkPane
 * chains and paint their whole subtree; nested scenes own child panes.
 * WindowServer composites the layer tree; no IOSurface transport remains.
 *
 * STRUCT FIELDS (local to this file): none — procedural (no owned struct).
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - Darling_initCompositor(window)
 *   - Darling_shutdownCompositor(void)
 *   - Darling_renderFrame(cmdBuffer, drawW, drawH, userdata)
 *   - Darling_preFrame(window, drawW, drawH, userdata)
 *   - Darling_layerRender(cmdBuffer, w, h, owner) : pane pass (leaf panel
 *     or board subtree via paintChildIntoPass)
 *   - paintChildIntoPass(cmdBuffer, child, ...) (private) : one child
 *     into board or board-pane pass (Rule 14 pane-skip inside)
 *   - Darling_compositorSettled(void)          : true when no pane submit flies
 *   - Darling_compositorIdleForResize(void)    : settled alias for pane/
 *     texture resize callers — resize-class work runs only when idle
 *   - darlingRetireGuard(void) (private)       : texture-retire drain probe
 *     registered into graphvex (Rule 33 downward seam) — destroys a retired
 *     texture only when NO bindless-sampling Submit flies: the board present
 *     fence signaled AND every pane fence signaled. Closes the page-fault
 *     window where texture.c's 2-frame CPU lag freed an old image under a
 *     still-flying pane/present CB.
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
extern bool VkMac_ensureIOSurfacePass(void);
extern VkRenderPass VkMac_getIOSurfacePass(void);

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

// Retire-guard callback registered into the texture module (Rule 33
// canonical downward seam — the graphvex leaf never reaches up for sampler
// flight state; the composer, which owns every bindless-sampling Submit,
// answers instead). A retired texture is destroyed only when NO sampler CB
// referencing it is in flight: the board present fence signaled AND every
// pane submit fence signaled. A false answer only defers destroys
// (retireDrain retries); it never blocks, allocates, or samples the driver
// beyond GetFenceStatus polls. This closes the page-fault window where
// texture.c's 2-frame CPU lag freed an old image while a flying
// pane/present CB still read it. Trade (Rule 35): while sampler submits
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
// present their OWN chain (Rule 14, recursive Vulkan-rect tree).
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
    // OWN chain — the pass must never stamp it (Rule 14).
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

    // Resize contract (Rule 39): this pane paints into its OWN chain — never
    // the shared batch CB — so steady rendering proceeds regardless of batch
    // flight. Resize-class work the handler triggers (a Texture_replaceRaw
    // that changes dimensions) must consult Darling_compositorIdleForResize
    // first: when the ring flies it defers to a same-size update or skips
    // the tick, exactly like the IOSurface drift-defer above.

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

    // LIVE RESIZE GATE (Rule 11.6): during a drag thread 0 owns ALL layer
    // motion — setFrameSize's synchronous composite pins every pane to its
    // selfAnchor per drag step. The present worker must NOT mutate container
    // layout or composite layers concurrently: that race tears the anchor
    // math and pane layers drift away from their pinned corners. The worker
    // still presents the board + all pane chains (panes render through
    // Darling_layerRender, never through preFrame), so the four scenes keep
    // animating the whole drag. On settle the flag clears and preFrame
    // resumes: one layout, one final re-record, one board rebuild.
    if (Window_isLiveResizing(window))
        return;

    int winW = Window_width(window);
    int winH = Window_height(window);
    if (winW <= 0 || winH <= 0) return;

    (void)drawW;
    (void)drawH;

    // Boards first: scene + content panels attach their full-window Metal
    // boards here so the pane attach below sees board backing (its
    // metal-parent gate) and the subtree painters see board sizes.
    extern int Darling_attachPanelBoards(Window *window, int width, int height);
    Darling_attachPanelBoards(window, winW, winH);

    Panel *root = Window_getContainer(window);
    Panel *contentPanel = Window_getContentPanel(window);
    Panel *scenePanel = Window_getScenePanel(window);

    if (contentPanel) {
        static int s_lastCompW = 0, s_lastCompH = 0;
        static int s_lastCompChildren = -1;
        int curChildCount = (int)Panel_childCount(contentPanel);
        bool needsComposite = (winW != s_lastCompW || winH != s_lastCompH
                               || curChildCount != s_lastCompChildren
                               || Panel_isTreeDirty(contentPanel));
        Container_setSize(&(*contentPanel).base, (float)winW, (float)winH);
        // No IOSurface transport anymore: children are Vulkan rects — nested
        // scenes own pane chains (attached here), everything else paints
        // into the board pass. Only layer compositing remains.
        Window_attachPanes(window, contentPanel, winW, winH);
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

    if (scenePanel) {
        Container_setSize(&(*scenePanel).base, (float)winW, (float)winH);
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
    Window *window = (Window*) userdata;
    if (!window || !cmdBuffer) return;

    int winW = Window_width(window);
    int winH = Window_height(window);
    if (winW <= 0 || winH <= 0) return;

    float kx = (float)drawW / (float)winW;
    float ky = (float)drawH / (float)winH;

    Panel *root = Window_getContainer(window);

    // Board-owned scenes (Rule 14): a metal-backed scenePanel paints its
    // whole subtree into its own board chain — the legacy board stamps
    // nothing and degrades to its clear pass.
    Panel *boardScene = Window_getScenePanel(window);
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

void Darling_initCompositor(Window *window) {
    if (!window) return;

    if (!Vk_ready()) {
        Vk_setWindowSeam(window,
                         (void *(*)(void *))Window_metalLayer,
                         (bool (*)(void *))Window_isTransparent,
                         (VkWindowPresentMode (*)(void *))Window_getPresentMode,
                         (uint64_t (*)(void *))Window_renderGeneration,
                         (bool (*)(void *))Window_isLiveResizing,
                         (void (*)(void *, void *, void *))Window_setResizeRenderHook,
                         (void (*)(void *))Window_setGravityTopLeft);
        Vk_init();
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
    // page-fault the GPU (Rule 39 net).
    Texture_setRetireGuard(darlingRetireGuard);

    Vk_setPreFrameRenderer((VkPreFrameFn)Darling_preFrame, window);
    Vk_setFrameRenderer(Darling_renderFrame, window);

    // Pane hook: per-CAMetalLayer swapchain children render through here.
    VkPane_setRenderer(Darling_layerRender);
}

void Darling_shutdownCompositor(void) {
    Vk_setPreFrameRenderer(nullptr, nullptr);
    Vk_setFrameRenderer(nullptr, nullptr);

    // No GPU teardown here: scenes paint into their own pane chains
    // (VkPane_shutdown owns them) and the board holds no resources.
    Texture_shutdown();
    SdfGpu_shutdown();
    VkView_shutdown();
    VkSceneCanvas_shutdownModule();
}
