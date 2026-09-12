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
 * Retained-mode UI compositor connecting Darling UI nodes, IOSurface overlays,
 * and Vulkan scene viewports into the host window and presentation loop.
 *
 * STRUCT FIELDS (local to this file):
 * ----------------------------------------------------------------------------
 *   IOSurfaceChild {       // Per-child IOSurface render target cache entry
 *     Panel *panel;        // Owning Darling panel this entry renders
 *     VkIOSurface *surf;   // IOSurface-backed Vulkan image (resized on drift)
 *     VkFramebuffer fb;    // Framebuffer targeting surf's image
 *     bool valid;          // True once the entry holds a current render
 *   }
 *
 * SLOT RECORD (batch flight row — behaviorless, owned by the batch ring):
 * ----------------------------------------------------------------------------
 *   VkCommandBuffer s_batchCb[i];    // ring-owned re-record buffer
 *   VkFence s_batchFence[i];         // per-slot submit fence (null = unarmed)
 *   bool s_batchSlotBusy[i];         // true between submit and drain
 * Ring: COMPOSITOR_BATCH_SLOTS 3; s_batchCursor rotates claims. A claim
 * polls its fence non-blocking; no free slot => skip-and-retry next tick
 * (dirty flags stay set, work is deferred never dropped). Shutdown drains
 * each live fence with a bounded 100ms wait (Rule 27) before destroy.
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - Darling_initCompositor(window)
 *   - Darling_renderFrame(cmdBuffer, drawW, drawH, userdata)
 *   - Darling_compositorSettled(void)          : true when no batch slot is flying
 *   - Darling_compositorIdleForResize(void)    : settled alias for pane/
 *     texture resize callers — resize-class work (replaceRaw-resize) runs
 *     only when idle, else defers to a same-size update or skips the tick
 *   - darlingRetireGuard(void) (private)       : texture-retire drain probe
 *     registered into graphvex (Rule 33 downward seam) — destroys a retired
 *     texture only when NO bindless-sampling Submit flies: batch settled, the
 *     board present fence signaled, AND every pane fence signaled. Closes the
 *     page-fault window where texture.c's 2-frame CPU lag freed an old image
 *     under a still-flying batch/pane/present CB.
 *   - Darling_compositorBatchDepth(void)       : live in-flight slot count
 *   - Darling_compositorBatchCapacity(void)    : COMPOSITOR_BATCH_SLOTS (3)
 *     (Rule 39: batch submit carries the VkGuard_check seam guard)
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

typedef struct IOSurfaceChild {
    Panel *panel;
    VkIOSurface *surf;
    VkFramebuffer fb;
    int lastPxW;
    int lastPxH;
    bool valid;
} IOSurfaceChild;

#define IOSURFACE_CHILD_MAX 256
#define COMPOSITOR_BATCH_SLOTS 3
#define COMPOSITOR_FENCE_WAIT_NS 100000000ULL
static IOSurfaceChild s_iosurfaceChildren[IOSURFACE_CHILD_MAX] = {0};
static int s_iosurfaceChildCount = 0;
static VkCommandPool s_compositorCmdPool = VK_NULL_HANDLE;
static VkCommandBuffer s_batchCb[COMPOSITOR_BATCH_SLOTS] = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
static VkFence s_batchFence[COMPOSITOR_BATCH_SLOTS] = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
static bool s_batchSlotBusy[COMPOSITOR_BATCH_SLOTS] = {false, false, false};
static int s_batchCursor = 0;

// Batch-flight guard: a re-record command buffer must never be reset or
// resubmitted while its own submit is still pending (reset-while-pending
// and double-pending submits are illegal and can kill the device). The ring
// claims the next non-flying slot via non-blocking poll; when every slot
// flies the tick skips re-recording and retries next — dirty flags stay set
// throughout, so deferred work is retried, never dropped. s_batchPending
// mirrors "any slot busy" for the settled query below.
static bool s_batchPending = false;

static void refenceSlotSignaled(VkDevice dev, int slot);
static int claimBatchSlot(VkDevice dev);
static void refreshBatchPending(void);

// Mirror "any slot busy" into s_batchPending for the settled query.
static void refreshBatchPending(void) {
    bool any = false;
    for (int i = 0; i < COMPOSITOR_BATCH_SLOTS; i++) {
        if (s_batchSlotBusy[i]) {
            any = true;
            break;
        }
    }
    s_batchPending = any;
}

// Claim the next non-flying batch slot (non-blocking poll, Rule 27).
// Returns the slot index, or -1 when every slot still flies — the tick
// skips re-recording and retries next (dirty stays set, never dropped).
static int claimBatchSlot(VkDevice dev) {
    COMPOSITOR_LOAD_DEVICE(GetFenceStatus)
    for (int n = 0; n < COMPOSITOR_BATCH_SLOTS; n++) {
        int slot = (s_batchCursor + n) % COMPOSITOR_BATCH_SLOTS;
        if (s_batchCb[slot] == VK_NULL_HANDLE)
            continue;
        if (s_batchSlotBusy[slot]) {
            if (s_batchFence[slot] == VK_NULL_HANDLE) {
                s_batchSlotBusy[slot] = false;
            } else {
                if (!GetFenceStatus_fn)
                    continue;
                if (GetFenceStatus_fn(dev, s_batchFence[slot]) != VK_SUCCESS)
                    continue;
                s_batchSlotBusy[slot] = false;
            }
        }
        s_batchCursor = (slot + 1) % COMPOSITOR_BATCH_SLOTS;
        return slot;
    }
    return -1;
}

static IOSurfaceChild *recordChildToIOSurface(VkCommandBuffer cb, Panel *child, void *surface, int w, int h) {
    if (!child || !surface || w <= 0 || h <= 0) return nullptr;

    if (!VkMac_ensureIOSurfacePass()) return nullptr;
    VkRenderPass pass = VkMac_getIOSurfacePass();

    VkDevice dev = Vk_getDevice();
    COMPOSITOR_LOAD_DEVICE(CmdBeginRenderPass);
    COMPOSITOR_LOAD_DEVICE(CmdEndRenderPass);
    COMPOSITOR_LOAD_DEVICE(CmdSetViewport);
    COMPOSITOR_LOAD_DEVICE(CmdSetScissor);
    COMPOSITOR_LOAD_DEVICE(CmdBindPipeline);
    COMPOSITOR_LOAD_DEVICE(CmdPushConstants);
    COMPOSITOR_LOAD_DEVICE(CmdDraw);
    COMPOSITOR_LOAD_DEVICE(DestroyFramebuffer);

    int canvasW = (int) IOSurfaceGetWidth((IOSurfaceRef) surface);
    int canvasH = (int) IOSurfaceGetHeight((IOSurfaceRef) surface);
    if (canvasW <= 0 || canvasH <= 0 || canvasW > 16384 || canvasH > 16384)
        return nullptr;

    IOSurfaceChild *ioChild = nullptr;
    for (int i = 0; i < s_iosurfaceChildCount; i++) {
        if (s_iosurfaceChildren[i].panel == child) {
            ioChild = &s_iosurfaceChildren[i];
            break;
        }
    }
    if (!ioChild) {
        if (s_iosurfaceChildCount >= IOSURFACE_CHILD_MAX) {
            fprintf(stderr, "[compositor] WARNING: IOSURFACE_CHILD_MAX (%d) exceeded, dropping panel %p\n",
                    IOSURFACE_CHILD_MAX, (void*) child);
            return nullptr;
        }
        ioChild = &s_iosurfaceChildren[s_iosurfaceChildCount++];
        (*ioChild).panel = child;
        (*ioChild).surf = nullptr;
        (*ioChild).fb = VK_NULL_HANDLE;
        (*ioChild).valid = false;
    }

    if ((*ioChild).surf && (VkIOSurface_width((*ioChild).surf) != (uint32_t)canvasW ||
                            VkIOSurface_height((*ioChild).surf) != (uint32_t)canvasH)) {
        // Resize-class rewrap: a flying batch slot may still reference this
        // fb. When the ring is unsettled the rewrap DEFERS — record at the
        // old size this tick and retry next (dirty stays set, never dropped).
        if (s_batchPending) {
            canvasW = (int) VkIOSurface_width((*ioChild).surf);
            canvasH = (int) VkIOSurface_height((*ioChild).surf);
        } else {
            if ((*ioChild).fb) DestroyFramebuffer_fn(dev, (*ioChild).fb, nullptr);
            VkIOSurface_free((*ioChild).surf);
            (*ioChild).surf = nullptr;
            (*ioChild).fb = VK_NULL_HANDLE;
            (*ioChild).valid = false;
        }
    }

    if (!(*ioChild).surf) {
        (*ioChild).surf = VkIOSurface_wrap(surface, (uint32_t)canvasW, (uint32_t)canvasH);
        if (!(*ioChild).surf) return nullptr;
    }

    if ((*ioChild).fb == VK_NULL_HANDLE) {
        (*ioChild).fb = VkIOSurface_createFramebuffer((*ioChild).surf, pass);
        if ((*ioChild).fb == VK_NULL_HANDLE) {
            VkIOSurface_free((*ioChild).surf);
            (*ioChild).surf = nullptr;
            return nullptr;
        }
    }

    uint64_t childType = Memory_type(child);
    bool isScene = (childType == TYPE_SCENE3D_SINGLETON || childType == TYPE_SCENE2D_SINGLETON
                    || childType == TYPE_SCENE_SINGLETON);

    VkClearValue clear = {0};
    VkRenderPassBeginInfo rpbi = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = pass,
        .framebuffer = (*ioChild).fb,
        .renderArea.extent = (VkExtent2D){ .width = (uint32_t)canvasW, .height = (uint32_t)canvasH },
        .clearValueCount = 1,
        .pClearValues = &clear,
    };
    CmdBeginRenderPass_fn(cb, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

    int renderW = w > canvasW ? canvasW : w;
    int renderH = h > canvasH ? canvasH : h;
    if (renderW <= 0) renderW = 1;
    if (renderH <= 0) renderH = 1;

    VkViewport vp = { .width = (float)renderW, .height = (float)renderH, .maxDepth = 1.0f };
    VkRect2D sc = { .extent = (VkExtent2D){ .width = (uint32_t)renderW, .height = (uint32_t)renderH } };
    CmdSetViewport_fn(cb, 0, 1, &vp);
    CmdSetScissor_fn(cb, 0, 1, &sc);

    if (isScene) {
        float uTime = (float)((double)(NanoTime_now() - Vk_getAnimStartNanos()) / 1e9);
        CmdBindPipeline_fn(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, Vk_getTriPipeline());
        CmdPushConstants_fn(cb, Vk_getTriLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, 4, &uTime);
        CmdDraw_fn(cb, 3, 1, 0, 0);
    } else {
        Panel_RenderFn handler = Panel_getRenderHandler(child);
        if (handler) {
            handler(child, nullptr, cb, (float) renderW, (float) renderH, 0.0f, 0.0f, (float) renderW, (float) renderH);
        } else {
            uint32_t color = Panel_getBackgroundColor(child);
            if (color != 0) {
                float r = (float)((color >> 16) & 0xFF) / 255.0f;
                float g = (float)((color >> 8)  & 0xFF) / 255.0f;
                float b = (float)( color        & 0xFF) / 255.0f;
                float a = (float)((color >> 24) & 0xFF) / 255.0f
                    * Container_getOpacity(&(*child).base);
                if (a > 0.0f)
                    Vk_fillRect(cb, (float)renderW, (float)renderH, 0.0f, 0.0f, (float)renderW, (float)renderH, r, g, b, a);
            }
        }
    }

    CmdEndRenderPass_fn(cb);
    (*ioChild).lastPxW = w;
    (*ioChild).lastPxH = h;
    return ioChild;
}

static void renderNativeContent(Window *window, Panel *contentPanel, int winW, int winH, float kx, float ky) {
    if (!window || !contentPanel) return;

    Panel *scenePanel = Window_getScenePanel(window);
    Window_attachPanelIOSurface(window, contentPanel, winW, winH);

    size_t childCount = Panel_childCount(contentPanel);
    if (childCount == 0) return;

    // LIVE RESIZE GATE: thread 0 is mid-drag. Layer frames keep moving
    // (WindowServer composites pane/IOSurface layers at full rate) but the
    // IOSurface children are NOT re-recorded here — re-recording at the live
    // pixel size every drag frame costs work proportional to window size and
    // glitches content (the "large window lags more" defect). The last
    // rendered surface stretches with its CALayer; on settle the flag clears
    // and the next tick re-records exactly once at the final size.
    if (Window_isLiveResizing(window))
        return;

    VkDevice dev = Vk_getDevice();
    VkQueue queue = Vk_getQueue();

    // Ring claim: the next non-flying slot, or -1 when every slot flies —
    // skip-and-retry next tick (dirty stays set). Never reset a flying CB.
    int slot = claimBatchSlot(dev);
    if (slot < 0) {
        refreshBatchPending();
        return;
    }
    refreshBatchPending();
    VkCommandBuffer cb = s_batchCb[slot];

    COMPOSITOR_LOAD_DEVICE(ResetCommandBuffer);
    COMPOSITOR_LOAD_DEVICE(BeginCommandBuffer);
    COMPOSITOR_LOAD_DEVICE(EndCommandBuffer);
    COMPOSITOR_LOAD_DEVICE(QueueSubmit);
    COMPOSITOR_LOAD_DEVICE(WaitForFences);
    COMPOSITOR_LOAD_DEVICE(ResetFences);
    COMPOSITOR_LOAD_DEVICE(CreateFence);

    ResetCommandBuffer_fn(cb, 0);
    VkCommandBufferBeginInfo bi = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    BeginCommandBuffer_fn(cb, &bi);

    IOSurfaceChild *recorded[IOSURFACE_CHILD_MAX];
    int recordedCount = 0;

    // TEMP DIAGNOSIS: VEX_EMPTY_BATCH=1 / VEX_HALF_BATCH=1 slice the recorded
    // children to bisect the faulting draw content (to be removed).
    static int s_emptyBatch = -1;
    static int s_halfBatch = -1;
    if (s_emptyBatch < 0)
        s_emptyBatch = getenv("VEX_EMPTY_BATCH") != nullptr;
    if (s_halfBatch < 0)
        s_halfBatch = getenv("VEX_HALF_BATCH") != nullptr;

    for (size_t i = 0; i < childCount; i++) {
        if (s_emptyBatch)
            break;
        if (s_halfBatch && i >= childCount / 2)
            break;
        Panel *child = Panel_getChild(contentPanel, i);
        if (!child || child == scenePanel) continue;

        uint32_t bg = Panel_getBackgroundColor(child);
        Panel_RenderFn rfn = Panel_getRenderHandler(child);
        uint64_t childType = Memory_type(child);
        bool isScene = (childType == TYPE_SCENE3D_SINGLETON || childType == TYPE_SCENE2D_SINGLETON
                        || childType == TYPE_SCENE_SINGLETON);
        if (bg == PANEL_COLOR_CLEAR && !rfn && !isScene)
            continue;

        extern void *PanelCocoa_fromPanel(void *panel);
        void *pc = PanelCocoa_fromPanel(child);
        if (!pc) continue;
        extern void *PanelCocoa_surface(void *pc);
        void *surface = PanelCocoa_surface(pc);
        if (!surface) continue;

        Vec4 rect;
        Container_resolve(&(*child).base, 0.0f, 0.0f, (float)winW, (float)winH, &rect);
        const int pxW = (int)(rect.z * kx + 0.5f);
        const int pxH = (int)(rect.w * ky + 0.5f);
        if (pxW <= 0 || pxH <= 0 || pxW > 16384 || pxH > 16384) continue;

        // Dirty-gate: a recorded surface that is already valid at the SAME pixel
        // size needs no re-render this tick — the IOSurface is pixel-identical,
        // so re-recording only burns GPU time and stalls on fences every tick.
        // Dynamic children (or dirtied trees) re-record; static children keep cache.
        if (!Panel_isTreeDirty(child)) {
            IOSurfaceChild *cached = nullptr;
            for (int ci = 0; ci < s_iosurfaceChildCount; ci++) {
                if (s_iosurfaceChildren[ci].panel == child) {
                    cached = &s_iosurfaceChildren[ci];
                    break;
                }
            }
            if (cached && (*cached).valid
                && (*cached).lastPxW == pxW
                && (*cached).lastPxH == pxH) {
                continue;
            }
        }

        IOSurfaceChild *ioChild = recordChildToIOSurface(cb, child, surface, pxW, pxH);
        if (ioChild) {
            if (recordedCount < IOSURFACE_CHILD_MAX) {
                recorded[recordedCount++] = ioChild;
            } else {
                fprintf(stderr, "[compositor] WARNING: recordedCount exceeded IOSURFACE_CHILD_MAX (%d)\n", IOSURFACE_CHILD_MAX);
            }
        }
    }

    EndCommandBuffer_fn(cb);

    // TEMP DIAGNOSIS: VEX_NO_BATCH=1 skips the batch submit (env knob for
    // bisecting the page-fault submit; to be removed).
    {
        static int s_noBatch = -1;
        if (s_noBatch < 0)
            s_noBatch = getenv("VEX_NO_BATCH") != nullptr;
        if (s_noBatch)
            return;
    }

    if (recordedCount > 0) {
        // Rule 39 seam guard: the batch submits to the shared queue; a dead
        // or nulled device must never receive it. Debug net (NDEBUG-stripped).
        if (!VkGuard_check("compositor batch", Vk_getDevice(), Vk_getQueue(), Vk_isDeviceLost()))
            return;
        if (s_batchFence[slot] == VK_NULL_HANDLE) {
            VkFenceCreateInfo fi = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
            if (CreateFence_fn(dev, &fi, nullptr, &s_batchFence[slot]) != VK_SUCCESS)
                return;
        }

        VkSubmitInfo si = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &cb,
        };

        ResetFences_fn(dev, 1, &s_batchFence[slot]);
        VkResult sr = QueueSubmit_fn(queue, 1, &si, s_batchFence[slot]);
        if (sr != VK_SUCCESS) {
            // Failed submits queue nothing: the just-reset fence would never
            // signal again, wedging every future batch wait. Recreate it
            // signaled and retry next tick (dirty flags stay set). Throttled:
            // one line per 2s, not one per tick.
            static uint64_t batchErrNs = 0;
            uint64_t errNow = NanoTime_now();
            if (batchErrNs == 0 || errNow - batchErrNs >= 2000000000ULL) {
                batchErrNs = errNow;
                fprintf(stderr, "[compositor] batch submit failed (%d)\n", (int) sr);
                fflush(stderr);
            }
            refenceSlotSignaled(dev, slot);
            s_batchSlotBusy[slot] = false;
            refreshBatchPending();
            return;
        }
        s_batchSlotBusy[slot] = true;
        refreshBatchPending();
        // Bounded wait: a dead drawable (fullscreen close) may never signal.
        // Hanging here parks the worker and freezes teardown with a ghost
        // window — drop the batch and keep old content instead. The slot
        // stays busy: the next tick polls and retries (never resets a
        // flying command buffer), so the timeout defers work instead of
        // dropping it.
        if (WaitForFences_fn(dev, 1, &s_batchFence[slot], VK_TRUE, COMPOSITOR_FENCE_WAIT_NS) != VK_SUCCESS) {
            fprintf(stderr, "[compositor] batch fence wait TIMEOUT at %llu ms\n",
                    (unsigned long long) (NanoTime_now() / 1000000ULL));
            return;
        }
        s_batchSlotBusy[slot] = false;
        refreshBatchPending();

        for (int i = 0; i < recordedCount; i++) {
            VkIOSurface_export((*recorded[i]).surf);
            (*recorded[i]).valid = true;
            Panel_clearTreeDirty((*recorded[i]).panel);
        }
    }
}

// Rebuild one slot's fence in the SIGNALED state after a failed submit left
// it reset with no pending work (an unsignaled fence with nothing queued
// never signals again). Create-first: on creation failure the old fence is
// kept (wedged, but no new crash).
static void refenceSlotSignaled(VkDevice dev, int slot) {
    COMPOSITOR_LOAD_DEVICE(DestroyFence)
    COMPOSITOR_LOAD_DEVICE(CreateFence)
    if (!DestroyFence_fn || !CreateFence_fn)
        return;
    if (slot < 0 || slot >= COMPOSITOR_BATCH_SLOTS)
        return;
    VkFenceCreateInfo fci = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    VkFence fresh = VK_NULL_HANDLE;
    if (CreateFence_fn(dev, &fci, nullptr, &fresh) != VK_SUCCESS)
        return;
    VkFence old = s_batchFence[slot];
    s_batchFence[slot] = fresh;
    if (old != VK_NULL_HANDLE)
        DestroyFence_fn(dev, old, nullptr);
}

// Drain query for present-on-demand loops: true when no batch slot flies.
// Loops gate their tree-dirty clear on this so a timed-out batch's
// unexported work is retried next tick, never dropped by a clear.
bool Darling_compositorSettled(void) {
    return !s_batchPending;
}

// Retire-guard callback registered into the texture module (Rule 33
// canonical downward seam — the graphvex leaf never reaches up for sampler
// flight state; the composer, which owns every bindless-sampling Submit,
// answers instead). A retired texture is destroyed only when NO sampler CB
// referencing it is in flight: every compositor batch slot settled, the
// board present fence signaled, AND every pane submit fence signaled. A
// false answer only defers destroys (retireDrain retries); it never blocks,
// allocates, or samples the driver beyond GetFenceStatus polls. This closes
// the page-fault window where texture.c's 2-frame CPU lag freed an old image
// while a flying batch/pane/present CB still read it. Trade (Rule 35): while
// sampler submits saturate, retired rows stay ringed and resize/free
// rollovers drop-degrade to keep-old-content until a quiescent tick.
static bool darlingRetireGuard(void) {
    if (s_batchPending)
        return false;
    if (!Vk_presentFlightIdle())
        return false;
    if (!VkPane_flightIdle())
        return false;
    return true;
}

// Resize gate for pane/texture callers: resize-class work (a
// Texture_replaceRaw that changes dimensions, an IOSurface rewrap) runs
// only when the batch ring is idle; otherwise the caller defers to a
// same-size update or skips the tick. Headless-safe: true with no flight.
bool Darling_compositorIdleForResize(void) {
    return !s_batchPending;
}

// GETTERS (Rule 24: symmetric, null-safe counters over the batch ring)

int32_t Darling_compositorBatchDepth(void) {
    int32_t depth = 0;
    for (int i = 0; i < COMPOSITOR_BATCH_SLOTS; i++) {
        if (s_batchSlotBusy[i])
            depth++;
    }
    return depth;
}

int32_t Darling_compositorBatchCapacity(void) {
    return COMPOSITOR_BATCH_SLOTS;
}

// // CORE FUNCTIONS

// Pane render hook: called by VkPane_presentAll per CAMetalLayer pane, inside
// that pane's OWN render pass (already begun, cleared, viewport at 0,0 = pane
// size). Renders the pane's Panel handler, or the fallback spinning tri for a
// scene with no handler. Mirror of the board's Darling_renderFrame scene path,
// but with NO window-absolute transform — a pane owns its whole extent.
static void Darling_layerRender(void *cmdBuffer, int w, int h, void *owner) {
    Panel *child = (Panel*) owner;
    if (!child || !cmdBuffer || w <= 0 || h <= 0)
        return;

    // Resize contract (Rule 39): this pane paints into its OWN chain — never
    // the shared batch CB — so steady rendering proceeds regardless of batch
    // flight. Resize-class work the handler triggers (a Texture_replaceRaw
    // that changes dimensions) must consult Darling_compositorIdleForResize
    // first: when the ring flies it defers to a same-size update or skips
    // the tick, exactly like the IOSurface drift-defer above.

    Panel_RenderFn handler = Panel_getRenderHandler(child);
    if (handler) {
        handler(child, nullptr, cmdBuffer, (float) w, (float) h, 0.0f, 0.0f, (float) w, (float) h);
        return;
    }

    uint64_t childType = Memory_type(child);
    bool isScene = (childType == TYPE_SCENE3D_SINGLETON || childType == TYPE_SCENE2D_SINGLETON
                    || childType == TYPE_SCENE_SINGLETON);
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

    float kx = (float)drawW / (float)winW;
    float ky = (float)drawH / (float)winH;

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
        renderNativeContent(window, contentPanel, winW, winH, kx, ky);
        if (needsComposite) {
            Window_compositeIOSurfaceChildren(window, contentPanel);
            s_lastCompW = winW;
            s_lastCompH = winH;
            s_lastCompChildren = curChildCount;
        }
    }

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

    if (root) {
        size_t childCount = Panel_childCount(root);
        for (size_t i = 0; i < childCount; i++) {
            Panel *child = Panel_getChild(root, i);
            if (!child) continue;

            Vec4 rect;
            Container_resolve(&(*child).base, 0.0f, 0.0f, (float)winW, (float)winH, &rect);
            if (rect.z <= 0.0f || rect.w <= 0.0f) continue;

            uint64_t cType = Memory_type(child);
            bool childIsScene = (cType == TYPE_SCENE3D_SINGLETON || cType == TYPE_SCENE2D_SINGLETON
                                 || cType == TYPE_SCENE_SINGLETON);
            if (!childIsScene) {
                continue;
            }

            // Pane-backed scene (CAMetalLayer + own swapchain): renders into
            // its OWN chain — the board must never stamp it (Rule 14).
            extern void *PanelCocoa_fromPanel(void *panel);
            extern bool PanelCocoa_isMetal(const void *pc);
            void *panePc = PanelCocoa_fromPanel(child);
            if (panePc && PanelCocoa_isMetal(panePc))
                continue;

            float px = rect.x * kx;
            float py = rect.y * ky;
            float pw = rect.z * kx;
            float ph = rect.w * ky;
            if (px < 0.0f) { pw += px; px = 0.0f; }
            if (py < 0.0f) { ph += py; py = 0.0f; }
            if (pw <= 0.0f || ph <= 0.0f) continue;
            if (px + pw > (float)drawW) pw = (float)drawW - px;
            if (py + ph > (float)drawH) ph = (float)drawH - py;
            if (pw <= 0.0f || ph <= 0.0f) continue;

            uint64_t childType = Memory_type(child);
            bool isScene = (childType == TYPE_SCENE3D_SINGLETON || childType == TYPE_SCENE2D_SINGLETON
                            || childType == TYPE_SCENE_SINGLETON);
            if (isScene) {
                Panel_RenderFn handler = Panel_getRenderHandler(child);
                if (handler) {
                    handler(child, nullptr, cmdBuffer, (float) drawW, (float) drawH, px, py, pw, ph);
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

                    VkViewport defaultVp = { .x = 0.0f, .y = 0.0f, .width = (float)drawW, .height = (float)drawH, .minDepth = 0.0f, .maxDepth = 1.0f };
                    VkRect2D defaultSc = { .offset = { 0, 0 }, .extent = { (uint32_t)drawW, (uint32_t)drawH } };
                    CmdSetViewport_fn((VkCommandBuffer) cmdBuffer, 0, 1, &defaultVp);
                    CmdSetScissor_fn((VkCommandBuffer) cmdBuffer, 0, 1, &defaultSc);
                }
            } else {
                Panel_RenderFn handler = Panel_getRenderHandler(child);
                if (handler) {
                    handler(child, nullptr, cmdBuffer, (float) drawW, (float) drawH, px, py, pw, ph);
                } else {
                    uint32_t color = Panel_getBackgroundColor(child);
                    if (color != 0) {
                        float r = ((color >> 16) & 0xFF) / 255.0f;
                        float g = ((color >> 8) & 0xFF) / 255.0f;
                        float b = (color & 0xFF) / 255.0f;
                        float a = ((color >> 24) & 0xFF) / 255.0f
                            * Container_getOpacity(&(*child).base);
                        if (a > 0.0f)
                            Vk_fillRect(cmdBuffer, (float)drawW, (float)drawH, px, py, pw, ph, r, g, b, a);
                    }
                }
            }
        }
    }
}

void Darling_initCompositor(Window *window) {
    if (!window) return;

    if (!Vk_ready()) {
        Vk_init(window);
    }

    VkInstance inst = Vk_getInstance();
    PFN_vkGetInstanceProcAddr gpa = Vk_getGpa();
    VkPhysicalDevice phys = Vk_getPhys();
    VkDevice dev = Vk_getDevice();
    VkQueue queue = Vk_getQueue();
    uint32_t qf = Vk_getQueueFamily();
    PFN_vkGetDeviceProcAddr gdpa = Vk_getGdpa();

    // Create dedicated command pool and batch ring (one re-record buffer +
    // fence per slot) for offscreen IOSurface rendering
    if (s_compositorCmdPool == VK_NULL_HANDLE && dev != VK_NULL_HANDLE && gdpa) {
        PFN_vkCreateCommandPool CreateCommandPool_fn = (PFN_vkCreateCommandPool)gdpa(dev, "vkCreateCommandPool");
        PFN_vkAllocateCommandBuffers AllocateCommandBuffers_fn = (PFN_vkAllocateCommandBuffers)gdpa(dev, "vkAllocateCommandBuffers");
        PFN_vkCreateFence CreateFence_fn = (PFN_vkCreateFence) gdpa(dev, "vkCreateFence");

        VkCommandPoolCreateInfo cpci = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .queueFamilyIndex = qf,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        };
        if (CreateCommandPool_fn && CreateCommandPool_fn(dev, &cpci, nullptr, &s_compositorCmdPool) == VK_SUCCESS) {
            VkCommandBufferAllocateInfo cbai = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = s_compositorCmdPool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = COMPOSITOR_BATCH_SLOTS,
            };
            if (AllocateCommandBuffers_fn) {
                AllocateCommandBuffers_fn(dev, &cbai, s_batchCb);
                // Rule 39 seam naming: let a device-lost log name the re-record
                // batch submit instead of the generic "vkQueueSubmit". Gated on
                // the extension actually being live (loader resolves the symbol
                // even on unsupported builds; calling it there segfaults).
                PFN_vkSetDebugUtilsObjectNameEXT setName_fn = (PFN_vkSetDebugUtilsObjectNameEXT)gdpa(dev, "vkSetDebugUtilsObjectNameEXT");
                if (setName_fn && Vk_isDebugUtilsEnabled()) {
                    char batchName[COMPOSITOR_BATCH_SLOTS][24];
                    for (int i = 0; i < COMPOSITOR_BATCH_SLOTS; i++) {
                        if (s_batchCb[i] == VK_NULL_HANDLE)
                            continue;
                        snprintf(batchName[i], sizeof(batchName[i]), "compositor batch %d", i);
                        VkDebugUtilsObjectNameInfoEXT info = { .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
                        info.objectType = VK_OBJECT_TYPE_COMMAND_BUFFER;
                        info.objectHandle = (uint64_t) s_batchCb[i];
                        info.pObjectName = batchName[i];
                        setName_fn(dev, &info);
                    }
                }
            }
            if (CreateFence_fn) {
                for (int i = 0; i < COMPOSITOR_BATCH_SLOTS; i++) {
                    if (s_batchFence[i] != VK_NULL_HANDLE)
                        continue;
                    VkFenceCreateInfo fi = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
                    CreateFence_fn(dev, &fi, nullptr, &s_batchFence[i]);
                }
            }
        }
    }

    VkView_refreshAll(inst, gpa, phys, dev);
    VkSceneCanvas_initModule(inst, gpa, phys, dev);
    VkIOSurface_initModule(inst, gpa, phys, dev);
    Texture_initModule(inst, (void*) gpa, phys, dev, queue, qf);
    SdfGpu_initModule(inst, (void*) gpa, phys, dev, queue, qf);

    // Texture-retire destroy guard: this composer owns the only bindless-
    // sampling Submits (batch ring + board present frame renderer + panes),
    // so it certifies when a retired texture's memory is provably
    // unreferenced. Without it texture.c falls back to a CPU frame lag that
    // can free an old image under a still-flying batch/pane/present CB and
    // page-fault the GPU (Rule 39 net).
    Texture_setRetireGuard(darlingRetireGuard);

    Vk_setPreFrameRenderer(Darling_preFrame, window);
    Vk_setFrameRenderer(Darling_renderFrame, window);

    // Pane hook: per-CAMetalLayer swapchain children render through here.
    VkPane_setRenderer(Darling_layerRender);
}

void Darling_shutdownCompositor(void) {
    Vk_setPreFrameRenderer(nullptr, nullptr);
    Vk_setFrameRenderer(nullptr, nullptr);

    VkDevice dev = Vk_getDevice();
    PFN_vkGetDeviceProcAddr gdpa = Vk_getGdpa();

    if (dev != VK_NULL_HANDLE && gdpa) {
        PFN_vkDestroyFramebuffer DestroyFramebuffer_fn = (PFN_vkDestroyFramebuffer)gdpa(dev, "vkDestroyFramebuffer");
        for (int i = 0; i < s_iosurfaceChildCount; i++) {
            if (s_iosurfaceChildren[i].fb != VK_NULL_HANDLE && DestroyFramebuffer_fn) {
                DestroyFramebuffer_fn(dev, s_iosurfaceChildren[i].fb, nullptr);
            }
            if (s_iosurfaceChildren[i].surf) {
                VkIOSurface_free(s_iosurfaceChildren[i].surf);
            }
            s_iosurfaceChildren[i].panel = nullptr;
            s_iosurfaceChildren[i].surf = nullptr;
            s_iosurfaceChildren[i].fb = VK_NULL_HANDLE;
            s_iosurfaceChildren[i].valid = false;
        }
        s_iosurfaceChildCount = 0;

        // Rule 26/27 teardown: drain each live batch fence with a bounded
        // 100ms wait (a dead drawable never signals — drop, don't hang),
        // then destroy fences + pool. No UINT64_MAX anywhere on this path.
        PFN_vkWaitForFences WaitForFences_fn = (PFN_vkWaitForFences) gdpa(dev, "vkWaitForFences");
        PFN_vkDestroyFence DestroyFence_fn = (PFN_vkDestroyFence)gdpa(dev, "vkDestroyFence");
        for (int i = 0; i < COMPOSITOR_BATCH_SLOTS; i++) {
            if (s_batchFence[i] != VK_NULL_HANDLE) {
                if (WaitForFences_fn)
                    WaitForFences_fn(dev, 1, &s_batchFence[i], VK_TRUE, COMPOSITOR_FENCE_WAIT_NS);
                if (DestroyFence_fn) DestroyFence_fn(dev, s_batchFence[i], nullptr);
                s_batchFence[i] = VK_NULL_HANDLE;
            }
            s_batchCb[i] = VK_NULL_HANDLE;
            s_batchSlotBusy[i] = false;
        }
        s_batchCursor = 0;

        if (s_compositorCmdPool != VK_NULL_HANDLE) {
            PFN_vkDestroyCommandPool DestroyCommandPool_fn = (PFN_vkDestroyCommandPool)gdpa(dev, "vkDestroyCommandPool");
            if (DestroyCommandPool_fn) DestroyCommandPool_fn(dev, s_compositorCmdPool, nullptr);
            s_compositorCmdPool = VK_NULL_HANDLE;
        }
    }

    s_batchPending = false;

    Texture_shutdown();
    SdfGpu_shutdown();
    VkView_shutdown();
    VkSceneCanvas_shutdownModule();
}
