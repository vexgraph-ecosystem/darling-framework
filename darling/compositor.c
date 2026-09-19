#include "annotation/overview.h"
#include "darling/compositor.h"
#include "darling/frame.h"
#include "darling/container.h"
#include "darling/panel/panel.h"
#include "darling/scene/scene.h"
#include "darling/field/input.h"
#include "event/dispatch.h"
#include "graphvex/graphics_loop.h"
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
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// Infancy demand gate: the first DARLING_INFANCY_PRESENTS confirmed seam
// presents run the full resize sequence every tick with demand-gating off —
// infancy is not steady state, so a phantom first-present success must not
// latch the loop into on-demand rest on unconfirmed glass.

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Compositor
 * LEVEL: L2 — Behavior (retained-mode UI compositing behavior API)
 * ============================================================================
 * Retained-mode UI compositor connecting Darling UI nodes and Vulkan scene
 * viewports into the host window and presentation loop. Every panel is a
 * Vulkan rect: boards (scene/content) own retained OFFSCREEN VkLayer
 * targets and paint their whole subtree into them; scenes reach the screen
 * through one of two Present-On-Demand Law destinations — COMPOSITED
 * (default) owns a retained offscreen VkLayer flight target (same registry
 * as the boards) the canvas samples as a textured quad at the anchor rect,
 * DIRECT owns a child pane (CAMetalLayer + swapchain). The Frame seam
 * CAMetalLayer is the window's SINGLE on-screen layer: the seam pass
 * composites the published board images in z-order (scene bottom, content
 * top) at full drawable extent and presents on demand (the Window
 * Compositing Layer Order Law, managed exception per the Conflict Triage
 *   Law). No IOSurface transport remains.
 *
 * LOOP1 PSEUDOCODE (board collage, classification-driven per the Immediate
 * vs Retained Element Model; Loop1 samples, Loop2 renders):
 * ----------------------------------------------------------------------------
 *   for every active window {
 *       if minimized: continue
 *       demanded = neverPresented || liveResizing
 *       infancy (presentedFrames < DARLING_INFANCY_PRESENTS): demand-gating
 *           off — run the resize sequence every tick until 3 consecutive
 *           successful presents, then settle into on-demand rest
 *       (warm-up: Darling_initCompositor presents once synchronously
 *           pre-show, so the opened window is already painted)
 *       for each board in [scene, content] {  // scene bottom, content top
 *           if board tree dirty, target dirty, or a depth-1 child published
 *               since its last composite (present-count delta): demanded = true
 *           // demand gates re-render only — never sampling
 *       }
 *       if demanded: present once, collaging every published board
 *           (sampling is one draw call per board — cheap; skipping a board
 *           would erase it to the fresh clear); clear each composited
 *           board's tree dirt; neverPresented = false
 *       // first tick runs the resize sequence (one path for both)
 *       else: sleep until woken (dirty / publish / resize ticket)
 *   }
 *
 * Loop2 (workers, outside this file):
 *
 *   for every retained item not a board {
 *       if dirty and fence idle:
 *           render into flight target;
 *           publish
 *  }
 *
 * Retained presentables are the two boards plus the first-gen subtrees that
 * contain retained-output content (a COMPOSITED scene rendered on its own
 * timeline — classification, the Immediate vs Retained Element Model,
 * darling.md section 55); synchronous plaster needs finished images. Depth
 * below 1 is each retained child's private affair. All-immediate /
 * retained-texture (I/R) subtrees paint INLINE into the board pass via
 * paintChildIntoPass's recursive subtree walk — zero flight targets, zero
 * copies for plain UI. Immediate parts (caret blink) paint inside their
 * owner's own target or inline, so Loop1 paints nothing except the collage.
 * Pane-backed children (DIRECT scenes) present their own chain (the
 * Present-On-Demand Law).
 *
 * STRUCT FIELDS (local to this file): none — procedural (no owned struct).
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - Darling_initCompositor(frame)
 *     (Vk init, shader bake, board/pane flight setup, GraphicsLoop client
 *     registration — then a synchronous WARM-UP PRESENT: "run at least
 *     once" per the Present-On-Demand Law first-render baseline and the
 *     Window Compositing Layer Order Law registration-demand. Runs the same
 *     resize sequence the loop's first step would (boards attach + render +
 *     publish, seam composites, one clear-present) BEFORE Frame_show, so
 *     the window opens already painted — no blank first beat over the blur
 *     view while the loop's first step is still scheduling. Latches
 *     hasPresented so the probe rests; animated content re-arms via its own
 *     demand paths. Failure degrades: the loop retries at its first step.)
 *   - Darling_shutdownCompositor(void)
 *   - darlingGfxFrameFn(window, dt, userdata) (private) : GfxFrameFn demand
 *     probe registered into graphvex's GfxLoop (the Conflict Triage Law
 *     downward seam — the loop lives in R3, darling registers into it).
 *     Probed every resting pass (the Present-On-Demand Law): ticks the
  *     focused Input's caret blink and re-arms present demand on
  *     tree/pane/layer/live-resize dirt (VkLayer_hasDemand covers retained
  *     boards + COMPOSITED scenes) or a publish-generation delta
  *     (VkLayer_publishGeneration vs the frame's lastPublishGen — a fresh
  *     publish summons its composite) — probe free, present gated on demand.
  *   - Darling_renderFrame(cmdBuffer, drawW, drawH, userdata=Frame*)
  *     (Loop1 seam collage, composite != render: winW/winH resolve from the
  *     Frame's live points first (Window cached size is fallback only) so
  *     kx = drawW/winW tracks the live scale every step and never stretches
  *     fresh px over a stale point size (the Native Pixel Law); the seam pass samples EVERY
  *     registered board on EVERY present — sampling is one draw call per
  *     board (cheap) and the fresh-cleared swapchain image would ERASE a
  *     skipped board, so demand gates board RE-RENDER (visit-side dirty,
  *     already working), never sampling. Each board composites at its OWN
  *     pixel extent pinned top-left in the drawable — never stretched to
  *     fill it, so a board target that lags the window for a step pins its
  *     last publish crisply instead of gravity-resizing it (the Continuous
  *     Real-Time Live Resize Law); a converged board (extent == drawable)
  *     samples identically to a full-extent quad. Unpublished boards
  *     (published<0) no-op inside VkLayer_composite. A successful board
  *     composite clears that board's tree dirt (scene and content alike) so
  *     a clean board CLEAN-SKIPs next tick instead of re-arming forever.
  *     Window-level `demanded` (any board demand via tree/target dirt or
  *     depth-1 child present-count delta, never-presented, or live-resizing)
  *     presents exactly once per tick, else rests. Scene-bottom/content-top
  *     order per the Window Compositing Layer Order Law; minimized windows
  *     suppress. When no board exists, paints root children directly.
  *     Reports the pass through
  *     s_seamNonEmpty (false at entry; true on any board composite success,
  *     any fallback child actually painted, or deliberate !demanded rest —
  *     the empty-present guard: present callbacks gate their verdicts on it
  *     so a successful-but-empty present retries instead of settling blank)
  *   - Darling_preFrame(window, drawW, drawH, userdata=Frame*)
  *     (full layout every tick: the live flag is consumed as a demand ticket
  *     (moving edge re-arms the client dirty so the drag presents at cadence,
  *     idle ticks rest); live points resolve from the Frame first (the frame
  *     hook already ran Frame_resize with live OS points) with the cached
  *     Window size as fallback only, so Container_setSize + pane/layer
  *     attaches stay in step with the border (the Native Pixel Law + the
  *     Window Board Root Lock Law); board VkLayers attach with the live
  *     drawable px directly (board px == drawable px, never width * stale
  *     scale) and VkLayer_visit publishes dirty boards + COMPOSITED scenes
  *     BEFORE the seam pass samples them — same-queue ordering;
  *     Darling_layerRender runs inside VkLayer_visit and VkPane_presentAll
  *     always)
*   - Darling_layerRender(cmdBuffer, w, h, owner) : pane + retained-layer
  *     pass leaf (leaf panel or board subtree; runs every tick — the shared
  *     painter for DIRECT pane chains and COMPOSITED VkLayer targets, boards
  *     included. A board target collages each retained depth-1 child's
  *     last-published frame via VkLayer_composite at the child's anchor rect
  *     and skips unpublished children (published<0 keeps prior canvas
  *     content); children without a target fall back to paintChildIntoPass —
  *     now recursive: the whole I/R subtree paints inline into the same pass
  *     (the Immediate vs Retained Element Model, darling.md section 55).
  *     Depth below 1 is each retained child's private affair: a child target
  *     paints its own subtree directly)
  *   - paintChildIntoPass(cmdBuffer, child, ...) (private) : one child
  *     — and, for handler-less plain containers, its whole immediate subtree
  *     recursively — into board or board-pane pass (the Present-On-Demand
  *     Law pane-skip + COMPOSITED layer composite inside); each level resolves
  *     against its own parent's extents and offsets by the parent's absolute
  *     origin; returns true when any draw was issued (feeds the
  *     s_seamNonEmpty empty-present guard)
 *   - Darling_compositorSettled(void)          : true when no pane submit flies
 *   - Darling_compositorIdleForResize(void)    : settled alias for pane/
 *     texture resize callers — resize-class work runs only when idle
 *   - darlingRetireGuard(void) (private)       : texture-retire drain probe
 *     registered into graphvex (the Conflict Triage Law downward seam) — destroys a retired
 *     texture only when NO bindless-sampling Submit flies: the seam present
 *     fence signaled AND every pane fence signaled. Closes the page-fault
 *     window where texture.c's 2-frame CPU lag freed an old image under a
 *     still-flying pane/present CB.
  *   - darlingPresentResizeSequence(window, userdata) (private) : shared
  *     first-frame/resize present body (one path for both): guards
  *     (Vk_ready, minimized, extent>0), Darling_preFrame at current extent
  *     (VkLayer_visit included) + VkPane_presentAll + Vk_clearPresent
  *     (wrapped in Window_workerPresentBegin/End, which are inert stubs);
  *     on a NON-EMPTY success (s_seamNonEmpty) latches hasPresented via
  *     GraphicsLoop_findClient so a healed resize present stops the
  *     never-presented re-arm spin — an empty success latches nothing, so
  *     the loop retries instead of resting blank. Returns
  *     the Vk_clearPresent verdict for Loop2 bookkeeping.
 *   - Darling_resizeRenderHook(userdata) (private)
 *     : WindowResizeRenderFn registered by Darling_initCompositor into the
 *     window's resize-hook slot. NOTE: FrameCocoa_attach later OVERWRITES
 *     that slot with frameCocoaResizeHook (drawableSize re-chase in native
 *     px + Frame_resize) — the drag path is the frame hook, and the IRS is
 *     the shared present body for the warm-up and infancy presents. Either
 *     way the sequence is thread 0, per drag step, with the cached window
 *     size already the NEW extent: preFrame layout stays in step with the
 *     border and the seam presents the boards pinned top-left at their own
 *     extent (never stretched). Thread 0 exclusively owns presents during
 *     live resize; the present worker gates itself out via
 *     Window_isLiveResizing to avoid a concurrent present race.
  *   - darlingWindowPresentFn(window, userdata) (private) : GraphicsPresentFn
  *     per-window present callback; while the frame's presentedFrames stays
  *     below DARLING_INFANCY_PRESENTS it ignores demand and runs
  *     darlingPresentResizeSequence every tick (ready/minimized/extent
  *     guards stay), bumping presentedFrames only on NON-EMPTY success
  *     (s_seamNonEmpty) and retrying infancy on empty or failed presents —
  *     never settling on an unconfirmed or blank present. Past
  *     DARLING_EMPTY_PRESENT_CAP consecutive empty presents it settles on
  *     clear color (bounded retry, the Bounded Wait Law). At
  *     infancy it emits one GRAPHICS_VK_STATS-gated settle line and takes
  *     the normal on-demand path thereafter (never-presented client runs
  *     the resize sequence gated on non-empty, otherwise the bare
  *     Vk_clearPresent). The return value feeds Loop2 hasPresented latch +
  *     dirty clear, so false keeps demand armed for the retry.
 * ============================================================================
 */

// Accessors for Vulkan device state from vexspoke
extern VkDevice Vk_getDevice();
extern VkQueue Vk_getQueue();
extern VkCommandBuffer Vk_getCmdBuffer();
extern VkPipeline Vk_getTriPipeline();
extern VkPipelineLayout Vk_getTriLayout();
extern uint64_t Vk_getAnimStartNanos();
extern PFN_vkGetDeviceProcAddr Vk_getGdpa();
extern VkInstance Vk_getInstance();
extern PFN_vkGetInstanceProcAddr Vk_getGpa();
extern VkPhysicalDevice Vk_getPhys();
extern uint32_t Vk_getQueueFamily();

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

#define DARLING_INFANCY_PRESENTS (3u)

// Empty-present guard: the seam present succeeds (a clear-color frame hits
// the screen) even when Darling_renderFrame composited zero boards —
// unpublished targets no-op, unregistered boards skip, the root fallback
// paints nothing. Latching hasPresented / clearing dirty / bumping
// presentedFrames on such an empty success rests the loop on a blank frame
// with no dirt left to re-arm it (the first-frame-blank defect: only a
// resize re-arms demand). s_seamNonEmpty reports the last seam pass:
// Darling_renderFrame sets it false at entry, true when at least one board
// composite succeeds, a fallback child actually paints, or the pass rests
// on purpose (!demanded with hasPresented already latched — the screen
// already holds the last composite). Present callbacks gate their verdicts
// on it so empty presents retry instead of settling. DARLING_EMPTY_PRESENT_CAP
// bounds the retry (the Bounded Wait Law): past the cap the frame settles
// on clear color rather than hot-spinning a persistently empty composite.
#define DARLING_EMPTY_PRESENT_CAP (180u)

static bool s_seamNonEmpty = false;



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

// Paint one child panel — and, for handler-less plain containers, its whole
// immediate subtree recursively — into an already-begun render pass, shared
// by the legacy board (Darling_renderFrame), board panes, and retained
// layers (Darling_layerRender subtree walk). The Immediate vs Retained
// Element Model (darling.md section 55) makes this the INLINE path for
// all-immediate / retained-texture subtrees: a depth-1 child with no
// retained-output content owns no flight target, so the pass paints the
// child, then its children, then theirs — tree order is z-order; each level
// resolves against its own parent's extents and offsets by the parent's
// absolute origin (mirrors the depth-1 layer pass whose canvas was the
// child's own rect). Scenes always paint (handler or tri fallback); plain
// UI paints only when paintUI is set (content-board subtree — the legacy
// board stamps scenes alone). Pane-backed children never paint here: they
// present their OWN chain (the Present-On-Demand Law, recursive Vulkan-rect
// tree). Returns true when any draw was issued (feeds the s_seamNonEmpty
// empty-present guard). Recursion depth equals panel tree depth — shallow
// by design, cold path only, zero allocation (the Cold-Strict, Hot-Minimal
// Validation Law).
static bool paintChildIntoPass(void *cmdBuffer, Panel *child,
                               float absX, float absY,
                               float parentW, float parentH,
                               float kx, float ky,
                               float drawW, float drawH, bool paintUI) {
    if (!cmdBuffer || !child)
        return false;
    Vec4 rect;
    Container_resolve(&(*child).base, 0.0f, 0.0f, parentW, parentH, &rect);
    if (rect.z <= 0.0f || rect.w <= 0.0f)
        return false;

    uint64_t cType = Memory_type(child);
    bool childIsScene = (cType == TYPE_SCENE3D_SINGLETON || cType == TYPE_SCENE2D_SINGLETON
                         || cType == TYPE_SCENE_SINGLETON);
    if (!childIsScene && !paintUI)
        return false;

    // Pane-backed child (CAMetalLayer + own swapchain): renders into its
    // OWN chain — the pass must never stamp it (the Present-On-Demand Law).
    extern void *PanelCocoa_fromPanel(void *panel);
    extern bool PanelCocoa_isMetal(const void *pc);
    void *panePc = PanelCocoa_fromPanel(child);
    if (panePc && PanelCocoa_isMetal(panePc))
        return false;

    float px = (absX + rect.x) * kx;
    float py = (absY + rect.y) * ky;
    float pw = rect.z * kx;
    float ph = rect.w * ky;
    if (px < 0.0f) { pw += px; px = 0.0f; }
    if (py < 0.0f) { ph += py; py = 0.0f; }
    if (pw <= 0.0f || ph <= 0.0f)
        return false;
    if (px + pw > drawW) pw = drawW - px;
    if (py + ph > drawH) ph = drawH - py;
    if (pw <= 0.0f || ph <= 0.0f)
        return false;

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
            return false;
        int layer = VkLayer_find(child);
        if (layer < 0)
            return false;
        return VkLayer_composite(cmdBuffer, drawW, drawH, layer, px, py, pw, ph,
                                 1.0f, 1.0f, 1.0f, 1.0f);
    }

    if (isScene) {
        Panel_RenderFn handler = Panel_getRenderHandler(child);
        if (handler) {
            handler(child, nullptr, cmdBuffer, drawW, drawH, px, py, pw, ph);
            return true;
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
            return true;
        }
    } else {
        // Ordered part pipeline (background -> image -> text -> border ->
        // foreground). Legacy monoliths keep their exact old contract (paint
        // self, no recurse) via the back-compat path inside Panel_paintParts;
        // migrated / plain nodes paint stages then collage kids below.
        if (Panel_getRenderHandler(child)) {
            Panel_paintParts(child, nullptr, cmdBuffer, drawW, drawH,
                             px, py, pw, ph);
            return true;
        }
        bool drew = Panel_paintParts(child, nullptr, cmdBuffer, drawW, drawH,
                                     px, py, pw, ph);
        // Inline subtree (the Immediate vs Retained Element Model, darling.md
        // section 55): a handler-less plain container paints its children
        // beneath itself in tree order — each resolved against THIS node's
        // extents and offset by this node's absolute origin, mirroring the
        // depth-1 retained pass whose canvas was the child's own rect.
        // Pane-backed grandchildren skip (own chain); layer-backed
        // grandchildren cannot exist beyond depth 1 (attach registers
        // depth-1 only) and the leaf logic above leaves them unpainted,
        // exactly as before classification.
        size_t n = Panel_childCount(child);
        for (size_t i = 0; i < n; i++) {
            Panel *g = Panel_getChild(child, i);
            if (!g)
                continue;
            void *gpc = PanelCocoa_fromPanel(g);
            if (gpc && PanelCocoa_isMetal(gpc))
                continue;
            if (paintChildIntoPass(cmdBuffer, g, rect.x, rect.y, rect.z, rect.w,
                                   kx, ky, drawW, drawH, paintUI))
                drew = true;
        }
        return drew;
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
        // Leaf pane: ordered parts first (legacy monolith included); scene
        // tri-fallback only when nothing drew.
        if (Panel_paintParts(panel, nullptr, cmdBuffer, (float) w, (float) h,
                             0.0f, 0.0f, (float) w, (float) h))
            return;

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
        // Retained depth-1 child (classification — the Immediate vs Retained
        // Element Model: only COMPOSITED-scene subtrees own targets): reaches
        // the board ONLY as a collaged published frame — the board pass never
        // re-invokes the child's painter (composite != render). Unpublished
        // children (published<0) skip and keep prior canvas content.
        // DIRECT-pane children keep their exception (the Conflict Triage
        // Law): own chain, own present. Children with NO target (I/R
        // subtrees) fall through to paintChildIntoPass, which paints the
        // whole subtree inline recursively.
        extern void *PanelCocoa_fromPanel(void *panel);
        extern bool PanelCocoa_isMetal(const void *pc);
        void *childPc = PanelCocoa_fromPanel(child);
        if (childPc && PanelCocoa_isMetal(childPc))
            continue;
        int childLayer = VkLayer_find(child);
        if (childLayer >= 0) {
            Vec4 crect;
            Container *childBase = &(*child).base;
            Container_resolve(childBase, 0.0f, 0.0f, (float) panelW, (float) panelH, &crect);
            float cx = crect.x * kx;
            float cy = crect.y * ky;
            float cw = crect.z * kx;
            float ch = crect.w * ky;
            if (cx < 0.0f) { cw += cx; cx = 0.0f; }
            if (cy < 0.0f) { ch += cy; cy = 0.0f; }
            if (cw <= 0.0f || ch <= 0.0f)
                continue;
            if (cx + cw > (float) w) cw = (float) w - cx;
            if (cy + ch > (float) h) ch = (float) h - cy;
            if (cw <= 0.0f || ch <= 0.0f)
                continue;
            VkLayer_composite(cmdBuffer, (float) w, (float) h, childLayer,
                              cx, cy, cw, ch, 1.0f, 1.0f, 1.0f, 1.0f);
            continue;
        }
        paintChildIntoPass(cmdBuffer, child, 0.0f, 0.0f, (float) panelW, (float) panelH, kx, ky, (float) w, (float) h, true);
    }
}

void Darling_preFrame(Window *window, int drawW, int drawH, void *userdata) {
    if (!window)
        return;

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

    // Live points resolve from the borrowing Frame (userdata) first — the
    // frame hook chases Frame_resize with live OS points before modalTick, so
    // Frame points lead the cached Window size by a step mid-drag (the Native
    // Pixel Law + the Window Board Root Lock Law). The Window cached size is
    // the fallback only (null frame or not-yet-synced).
    Frame *frame = (Frame*) userdata;
    int winW = 0;
    int winH = 0;
    if (frame != nullptr) {
        winW = Frame_getWidth(frame);
        winH = Frame_getHeight(frame);
    }
    if (winW <= 0 || winH <= 0) {
        winW = Window_width(window);
        winH = Window_height(window);
    }
    if (winW <= 0 || winH <= 0)
        return;

    // Panes resolve from the borrowing Frame (userdata) — never the Window.
    if (frame == nullptr)
        return;
    Panel *root = Frame_getRootPanel(frame);
    Panel *contentPanel = Frame_getContentPane(frame);
    Panel *scenePanel = Frame_getScenePane(frame);
    // Moving edge is a demand ticket (the Present-On-Demand Law): while live,
    // force the client dirty every tick so the loop keeps presenting at drag
    // cadence; idle ticks rest untouched. Atomic store only — no layout, no
    // driver call, no allocation. Visit-then-composite order below is
    // unchanged: VkLayer_visit publishes dirty boards + COMPOSITED scenes
    // BEFORE the seam pass samples them.
    if (live)
        GraphicsLoop_markDirty(GraphicsLoop_default(), window);
    // Boards first: scene + content panels attach their full-window Metal
    // boards here so the pane attach below sees board backing (its
    // metal-parent gate) and the subtree painters see board sizes.
    extern int Darling_attachPanelBoards(Window *window, Panel *scenePane, Panel *contentPane, int width, int height, int drawW, int drawH);
    Darling_attachPanelBoards(window, scenePanel, contentPanel, winW, winH, drawW, drawH);

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

    // Depth-1 flight targets for the scene board too (classification, the
    // Immediate vs Retained Element Model: only children whose subtree holds
    // retained-output content — a COMPOSITED scene — own retained targets;
    // I/R-only subtrees paint inline into the board pass; children iterated
    // via Panel_childCount inside Darling_attachLayers, never hardcoded
    // counts, per the Dynamic Scalability & Anti-Hardcoding Law).
    if (scenePanel) {
        extern int Darling_attachLayers(Window *window, Panel *contentPanel, int width, int height);
        Darling_attachLayers(window, scenePanel, winW, winH);
    }

    // Publish dirty retained targets BEFORE the seam pass samples them —
    // boards + COMPOSITED scenes render into their offscreen flight images
    // now, in queue order ahead of the composite read (visit-then-composite
    // is same-queue safe per the vk_layer thread contract). Registration
    // (attachPanelBoards above) and propagatePaneDirty re-arm demand; clean
    // targets rest on their last render (the Present-On-Demand Law).
    extern bool VkLayer_visit(void);
    VkLayer_visit();

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
    s_seamNonEmpty = false;
    Window *window = rframe ? Frame_getWindow(rframe) : nullptr;
    if (!window || !cmdBuffer) return;
    // Minimized suppression: never re-composite the layer tree or record off
    // a window the WindowServer is warping into or out of the dock.
    if (Window_isMinimized(window))
        return;

    int winW = 0;
    int winH = 0;
    if (rframe != nullptr) {
        winW = Frame_getWidth(rframe);
        winH = Frame_getHeight(rframe);
    }
    if (winW <= 0 || winH <= 0) {
        winW = Window_width(window);
        winH = Window_height(window);
    }
    if (winW <= 0 || winH <= 0)
        return;

    float kx = (float) drawW / (float) winW;
    float ky = (float) drawH / (float) winH;

    Panel *root = Frame_getRootPanel(rframe);

    // Loop1 seam collage (the Present-On-Demand Law): retained
    // presentables are EXACTLY the scene panel, the content panel, and their
    // first-generation children. The seam pass samples EVERY registered
    // board on EVERY present — sampling is one draw call per board (cheap)
    // while the fresh-cleared swapchain image would ERASE a skipped board,
    // so demand gates board RE-RENDER (visit-side dirty, already working),
    // never sampling. Scene-bottom/content-top order per the Window
    // Compositing Layer Order Law. The window-level `demanded` flag (any
    // board demand, never-presented, or live-resizing) presents exactly once
    // per tick, else rests. A successful board composite clears that board's
    // tree dirt (scene and content alike) so a clean board CLEAN-SKIPs next
    // tick instead of re-arming forever.
    extern void *PanelCocoa_fromPanel(void *panel);
    extern bool PanelCocoa_isBoard(const void *pc);
    extern int PanelCocoa_chain(const void *pc);
    extern int PanelCocoa_width(const void *pc);
    extern int PanelCocoa_height(const void *pc);
    extern bool VkLayer_composite(void *cmdBuffer, float surfaceW, float surfaceH,
                                  int index, float x, float y, float w, float h,
                                  float r, float g, float b, float a);
    Panel *boardPanels[2] = { Frame_getScenePane(rframe), Frame_getContentPane(rframe) };
    static uint64_t s_lastChildPresent[2] = { 0u, 0u };
    static bool s_loop1First = true;
    bool live = Window_isLiveResizing(window);
    GraphicsClient *client = GraphicsLoop_findClient(GraphicsLoop_default(), window);
    bool neverPresented = client && !(*client).hasPresented;
    bool want[2] = { false, false };
    uint64_t curPresent[2] = { 0u, 0u };
    bool boardsRegistered = false;
    for (int i = 0; i < 2; i++) {
        Panel *board = boardPanels[i];
        if (!board)
            continue;
        void *boardPc = PanelCocoa_fromPanel(board);
        if (!boardPc || !PanelCocoa_isBoard(boardPc))
            continue;
        int boardLayer = PanelCocoa_chain(boardPc);
        if (boardLayer < 0)
            continue;
        boardsRegistered = true;
        bool boardDirty = Panel_isTreeDirty(board) || VkLayer_isDirty(boardLayer);
        uint64_t childPresent = 0u;
        size_t n = Panel_childCount(board);
        for (size_t ci = 0; ci < n; ci++) {
            Panel *child = Panel_getChild(board, ci);
            if (!child)
                continue;
            int childLayer = VkLayer_find(child);
            if (childLayer < 0)
                continue;
            if (VkLayer_isDirty(childLayer))
                boardDirty = true;
            childPresent += VkLayer_presentCount(childLayer);
        }
        curPresent[i] = childPresent;
        bool childPublished = s_loop1First || (childPresent != s_lastChildPresent[i]);
        if (boardDirty || childPublished || live || neverPresented)
            want[i] = true;
    }
    bool demanded = live || neverPresented;
    if (want[0] || want[1])
        demanded = true;
    if (!boardsRegistered && root && Panel_isTreeDirty(root))
        demanded = true;
    if (!demanded) {
        // Deliberate rest: nothing changed since the last composite and the
        // screen already holds it (!demanded implies hasPresented — a
        // never-presented client always demands). Report non-empty so the
        // present verdict latches rest instead of retrying clean content.
        s_seamNonEmpty = true;
        return;
    }
    for (int i = 0; i < 2; i++) {
        Panel *board = boardPanels[i];
        if (!board)
            continue;
        void *boardPc = PanelCocoa_fromPanel(board);
        if (!boardPc || !PanelCocoa_isBoard(boardPc))
            continue;
        int boardLayer = PanelCocoa_chain(boardPc);
        if (boardLayer < 0)
            continue;
        // Sample always, gate render: every registered board composites on
        // every present (unpublished boards no-op inside VkLayer_composite).
        // Success clears that board's tree dirt — the scene-tree dirt leak
        // fix: Panel_clearTreeDirty ran for content only, so scene dirt
        // re-armed layer 0 every tick.
        // Pinned top-left in FRAMEBUFFER space, sized to the board's OWN
        // pixel extent — never stretched to fill the drawable (the
        // Continuous Real-Time Live Resize Law). VkLayer_composite's
        // negative viewport (y=surfaceH, height=-surfaceH) makes a quad
        // grow DOWNWARD from its y base in framebuffer space, so a y=0
        // base would pin the board at the BOTTOM-left and expose the seam
        // clear strip along the top/right during one-step convergence —
        // the original "stretch at top-right, fine at bottom-left"
        // signature. Offset y to drawH - boardH so the published image
        // pins TOP-left, matching the seam layer's kCAGravityTopLeft
        // anchor. A converged board (boardH == drawH) yields y == 0 and
        // samples identically to the old full-extent quad.
        int boardW = PanelCocoa_width(boardPc);
        int boardH = PanelCocoa_height(boardPc);
        int boardY = drawH - boardH;
        if (boardW > 0 && boardH > 0
            && VkLayer_composite(cmdBuffer, (float) drawW, (float) drawH, boardLayer,
                                 0.0f, (float) boardY, (float) boardW, (float) boardH,
                                 1.0f, 1.0f, 1.0f, 1.0f)) {
            s_lastChildPresent[i] = curPresent[i];
            Panel_clearTreeDirty(board);
            s_seamNonEmpty = true;
        }
    }
    s_loop1First = false;
    // Loop1 collages only: when boards are registered the canvas holds
    // exactly the finished composite and nothing else paints on top.
    if (boardsRegistered)
        return;

    if (root) {
        size_t childCount = Panel_childCount(root);
        for (size_t i = 0; i < childCount; i++) {
            Panel *child = Panel_getChild(root, i);
            if (!child)
                continue;
            if (paintChildIntoPass(cmdBuffer, child, 0.0f, 0.0f, (float)winW, (float)winH, kx, ky, (float)drawW, (float)drawH, false))
                s_seamNonEmpty = true;
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
// Window_workerPresentBegin/End (inert stubs) wrap the call so the seam
// present keeps the same shape as the worker path; retained boards publish
// through VkLayer_visit inside Darling_preFrame below.
// Shared first-frame/resize present body — one path for both (the first
// frame executes the SAME sequence as a resize step, so first-frame success
// equals resize success by construction). Guards (Vk_ready, minimized,
// extent>0), one preFrame pass to catch layout the setFrameSize helpers may
// have queued asynchronously (e.g. Window_compositeBoards dispatched to
// main), then synchronous presents: DIRECT pane chains first (their own
// CAMetalLayers), then the seam canvas at the fresh extent (the Continuous
// Real-Time Live Resize Law). Wrapped in Window_workerPresentBegin/End
// (inert stubs) to keep the worker path shape; retained boards publish
// through VkLayer_visit inside Darling_preFrame. Scene-bottom/content-top
// order, fence bounds, VkGuard net, teardown order, and minimized
// suppression are untouched — this helper only sequences existing calls.
static bool darlingPresentResizeSequence(void *window, void *userdata) {
    Frame *hframe = (Frame*) userdata;
    Window *w = (Window*) window;
    if (!w)
        w = hframe ? Frame_getWindow(hframe) : nullptr;
    if (!w || !Vk_ready())
        return false;
    // Genie gate (defense in depth — the pump already skips the hook while
    // miniaturized): never re-composite the layer tree or present off a
    // window the WindowServer is warping into or out of the dock.
    if (Window_isMinimized(w))
        return false;
    int winW = 0;
    int winH = 0;
    if (hframe != nullptr) {
        winW = Frame_getWidth(hframe);
        winH = Frame_getHeight(hframe);
    }
    if (winW <= 0 || winH <= 0) {
        winW = Window_width(w);
        winH = Window_height(w);
    }
    if (winW <= 0 || winH <= 0)
        return false;
    // Live drawable px (not 0,0): the board attach must see the same px the
    // seam present will use, or infancy/live steps size boards twice per
    // tick (fallback then real) and lag a step behind the drawable.
    extern float TextCore_backingScale(void);
    float liveScale = TextCore_backingScale();
    if (liveScale <= 0.0f)
        liveScale = 1.0f;
    int pxW = (int) lround((double) winW * (double) liveScale);
    int pxH = (int) lround((double) winH * (double) liveScale);
    Darling_preFrame(w, pxW, pxH, userdata);
    Window_workerPresentBegin();
    VkPane_presentAll();
    bool presented = Vk_clearPresent();
    Window_workerPresentEnd();
    if (presented && s_seamNonEmpty) {
        GraphicsClient *client = GraphicsLoop_findClient(GraphicsLoop_default(), w);
        if (client)
            (*client).hasPresented = true;
    }
    return presented;
}

static void Darling_resizeRenderHook(void *userdata) {
    (void) darlingPresentResizeSequence(nullptr, userdata);
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
//   - present demand: a live resize, any DIRTY pane chain (DIRECT scenes),
//     any dirty retained target — the boards + COMPOSITED layer re-arms
//     propagated by Darling_propagatePaneDirty (VkLayer_hasDemand) — a
//     dirty panel tree, or a publish-generation delta since the last probe,
//     re-arms the client.
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
    if (VkLayer_hasDemand())
        demand = true;
    if (Panel_isTreeDirty(Frame_getContentPane(hframe)))
        demand = true;
    if (Panel_isTreeDirty(Frame_getScenePane(hframe)))
        demand = true;
    GraphicsClient *client = GraphicsLoop_findClient(GraphicsLoop_default(), window);
    if (client && !(*client).hasPresented)
        demand = true;
    // Publish re-arm: a retained frame published since the last probe is
    // itself demand — the composite may not have sampled it yet (the visit
    // that published it consumed the dirt). Without this, a publish that
    // lands with no dirt anywhere never summons its composite and the
    // fresh pixels sit unpresented until an unrelated demand arrives.
    uint64_t gen = VkLayer_publishGeneration();
    if (gen != (*hframe).lastPublishGen) {
        (*hframe).lastPublishGen = gen;
        demand = true;
    }
    if (demand)
        GraphicsLoop_markDirty(GraphicsLoop_default(), window);
}

// Readiness probe for GraphicsLoop: verifies window's mtklayer and scenePanel / contentPanel readiness
static bool darlingWindowReadyFn(void *window, void *userdata) {
    Frame *frame = (Frame*) userdata;
    if (!frame || !window)
        return false;
    // Genie gate: do not present while minimized into dock
    if (Window_isMinimized((Window*) window))
        return false;
    // Positive window extent required
    if (Window_width((Window*) window) <= 0 || Window_height((Window*) window) <= 0)
        return false;
    // Window must have an attached MTKLayer / CAMetalLayer
    if (!Frame_getNativeView(frame))
        return false;
    // Check if scenePanel or contentPanel (or root panel) is mounted
    Panel *content = Frame_getContentPane(frame);
    Panel *scene = Frame_getScenePane(frame);
    Panel *root = Frame_getRootPanel(frame);
    if (!content && !scene && !root)
        return false;
    return true;
}

// Per-window present callback: plasters scenePanel & contentPanel into window mtklayer.
// The first frame executes the SAME sequence as a resize step (one path for
// both): a never-presented client runs the shared resize body instead of the
// bare Vk_clearPresent. The return value feeds Loop2 hasPresented latch +
// dirty clear automatically.
static bool darlingWindowPresentFn(void *window, void *userdata) {
    if (!Vk_ready())
        return false;
    Frame *hframe = (Frame*) userdata;
    if (hframe != nullptr && (*hframe).presentedFrames < DARLING_INFANCY_PRESENTS) {
        bool ok = darlingPresentResizeSequence(window, userdata);
        bool nonEmpty = ok && s_seamNonEmpty;
        bool diag = getenv("GRAPHICS_VK_STATS") != nullptr || getenv("ANTI_VK_STATS") != nullptr;
        if (nonEmpty) {
            (*hframe).presentedFrames = (*hframe).presentedFrames + 1;
            (*hframe).emptyPresents = 0;
            if (diag)
                fprintf(stderr, "darling: infancy present %u/3 ok\n", (*hframe).presentedFrames);
            if ((*hframe).presentedFrames >= DARLING_INFANCY_PRESENTS && diag)
                fprintf(stderr, "darling: settled into on-demand rest after %u infancy presents\n",
                        (*hframe).presentedFrames);
        } else {
            // Empty seam present (nothing composited) or driver failure: do
            // NOT count infancy, do NOT latch — Loop2 keeps dirty armed and
            // retries next tick. Only driver-quiet empties feed the cap;
            // driver failures already retry through the dirty flag alone.
            if (ok)
                (*hframe).emptyPresents = (*hframe).emptyPresents + 1;
            if (diag)
                fprintf(stderr, "darling: infancy present empty (no board composited), retrying\n");
            if ((*hframe).emptyPresents >= DARLING_EMPTY_PRESENT_CAP) {
                if (diag)
                    fprintf(stderr, "darling: empty-present cap reached, settling on clear color\n");
                (*hframe).presentedFrames = DARLING_INFANCY_PRESENTS;
                (*hframe).emptyPresents = 0;
                return true;
            }
        }
        return nonEmpty;
    }
    GraphicsClient *client = GraphicsLoop_findClient(GraphicsLoop_default(), window);
    bool neverPresented = client && !(*client).hasPresented;
    if (neverPresented) {
        bool ok = darlingPresentResizeSequence(window, userdata);
        return ok && s_seamNonEmpty;
    }
    return Vk_clearPresent();
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

    // Live-resize present hook FALLBACK: FrameCocoa_attach overwrites this
    // slot with frameCocoaResizeHook (the effective per-drag-step seam),
    // which chases drawableSize + Frame_resize + GfxLoop_modalTick — the
    // synchronous render-at-NEW-size-then-present per step. Darling_…
    // resizeRenderHook is only reached if a Frame never attaches its cocoa
    // shim; it runs the same present-resize sequence directly. Registered
    // AFTER Vk_init so the pane/layer hooks are installed and the first
    // present is valid.
    Window_setResizeRenderHook(window, Darling_resizeRenderHook, frame);
    // The Frame shim was already attached at Frame creation (its hook set
    // there); the fallback above just overwrote it. Reassert the cocoa hook
    // last so the effective drag seam chases drawableSize + Frame_resize +
    // forced present per step — the fallback only serves shim-less frames.
    Frame_platformReassertResizeHook(frame);

    // Register into graphvex's GraphicsLoop: the frame loop runs our probe every
    // pass (demand re-arm + caret blink) and presents through the per-window loop.
    // Registered dirty on arrival -> the first loop step presents the initial composite.
    GraphicsLoop_registerClient(GraphicsLoop_default(), window, nullptr, nullptr, nullptr,
                                darlingGfxFrameFn, frame);
    GraphicsLoop_setClientMtkLayer(GraphicsLoop_default(), window, Frame_getNativeView(frame));
    GraphicsLoop_setClientReadyFn(GraphicsLoop_default(), window, darlingWindowReadyFn);
    GraphicsLoop_setClientPresentFn(GraphicsLoop_default(), window, darlingWindowPresentFn);

    // WARM-UP PRESENT ("run at least once" — the Present-On-Demand Law's
    // first-render baseline; registration demands the first render per the
    // Window Compositing Layer Order Law). The frame loop's first step lands
    // only AFTER Frame_show, so a freshly opened window would otherwise sit
    // blank (transparent canvas over the blur view) for that first beat.
    // Present synchronously HERE — thread 0, pre-show, zero concurrency —
    // running the exact resize sequence the loop would: preFrame attaches,
    // renders, and publishes the boards (registration dirty -> first visit
    // paints them), the seam composites them, one clear-present paints the
    // frame into the layer the user's first glance sees. hasPresented is
    // latched so the loop's probe rests instead of double-forcing the same
    // frame; animated content re-arms through its own demand paths. Failure
    // is not fatal: the loop retries normally at its first step
    // (drop-degrade, the Bounded Wait Law).
    if (darlingWindowReadyFn(window, frame)) {
        bool ok = darlingPresentResizeSequence(window, frame);
        if (ok && s_seamNonEmpty) {
            GraphicsClient *client = GraphicsLoop_findClient(GraphicsLoop_default(), window);
            if (client)
                (*client).hasPresented = true;
        }
        bool diag = getenv("GRAPHICS_VK_STATS") != nullptr || getenv("ANTI_VK_STATS") != nullptr;
        if (diag)
            fprintf(stderr, "darling: warm-up present %s (non-empty %d)\n",
                    ok ? "ok" : "failed", s_seamNonEmpty ? 1 : 0);
    }
}

void Darling_shutdownCompositor(void) {
    // Deregister the GraphicsLoop client first so the frame loop stops probing
    // and presenting a window that is tearing down (the Teardown Order Law:
    // detach before free).
    Window *w = s_seamFrame ? Frame_getWindow(s_seamFrame) : nullptr;
    if (w)
        GraphicsLoop_unregisterClient(GraphicsLoop_default(), w);

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


