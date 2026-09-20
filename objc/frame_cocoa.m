#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>
#import <QuartzCore/CATransaction.h>
#import <Metal/Metal.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "frame_cocoa.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include "graphvex/graphics_loop.h"
#include "window/window.h"
#include "window/window_event.h"

void Dialog_focus(Dialog *dialog);
void Dialog_bringToFront(Dialog *dialog);
bool Dialog_requestClose(Dialog *dialog);

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: FrameCocoa
 * ============================================================================
 * Native macOS AppKit bridge establishing the window rendering hierarchy
 * NSWindow -> NSVisualEffectView -> CAMetalLayer, where the seam layer is the
 * window's SINGLE on-screen CAMetalLayer (the Window Compositing Layer Order
 * Law managed exception per the Conflict Triage Law): the seam pass composites
 * the retained board images and presents on demand. presentsWithTransaction =
 * YES aligns drawables atomically with the macOS WindowServer during live
 * resize, minimize, zoom, and restore, and the Native Pixel Law contract
 * (contentsScale mirrors the backing scale, drawableSize in native hardware
 * pixels) is re-chased on every resized step. The layer frame is STICKY manual
 * geometry (autoresizingMask kCALayerNotSizable): only the resize hook's
 * single explicit CATransaction moves the seam (the Single-Transaction Live
 * Coordination Law), never spanning sendEvent (the
 * No-Transaction-Across-Event-Dispatch Law), so WindowServer never stretches
 * an old drawable to an auto-moved frame; a dropped present keeps dirty armed
 * inside the loop per the Present-On-Demand Law. No new thread, no wait — the
 * hook is the only live seam while AppKit's modal tracking loop owns thread 0.
 * FrameCocoa is the platform glue for the darling Frame class (darling/frame.h)
 * and composes with PanelCocoa's retained offscreen boards and the GfxLoop.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: FrameCocoa (OS CAMetalLayer frame shim)
 * LEVEL: L4 — Self-Management (AppKit NSVisualEffectView + CAMetalLayer bridge)
 * ============================================================================
 * Native macOS AppKit bridge establishing the window rendering hierarchy:
 *   NSWindow -> NSVisualEffectView -> CAMetalLayer (mtklayer)
 * where mtklayer is the Frame SEAM canvas — the window's SINGLE on-screen
 * CAMetalLayer (the Window Compositing Layer Order Law, managed exception
 * per the Conflict Triage Law): the seam pass composites the retained board
 * images and presents on demand. It is configured with
 * presentsWithTransaction = YES to align drawables atomically with the
 * macOS WindowServer during live resize, minimize, zoom, and restore
 * events, and with the Native Pixel Law contract: contentsScale mirrors
 * the backing scale factor and drawableSize is set in native hardware
 * pixels, re-chased on every resized step (frameCocoaResizeHook). The
 * layer frame is STICKY manual geometry (autoresizingMask kCALayerNotSizable
 * = 0): AppKit never moves the seam in its own transaction — the hook sets
 * seam.frame + seam.bounds = contentView.bounds explicitly inside the single
 * live transaction, so ONLY our transaction moves the seam and WindowServer
 * never stretches an old drawable to an auto-moved frame.
 *
 * STRUCT FIELDS (Mirroring darling/frame.h — the Frame the shim operates on):
 * ----------------------------------------------------------------------------
 *   Frame {                // Window frame (see darling/frame.h)
 *     Window *window;      // R1 host window pointer
 *     Application *application; // R1 host application manifest pointer (nullable)
 *     void *graphics;      // R3 GPU graphics context (VkHotContext / Device)
 *     Panel *rootPanel;    // Root UI component tree
 *     struct Component *rootComponent; // Root Component tree (new Component + Graphics architecture)
 *     Panel *contentPane;  // Upper board root: UI canvas (borrowed, nullable)
 *     Panel *scenePane;    // Bottom board root: scene/backdrop (borrowed, nullable)
 *     char *title;         // Owned title string (strdup on set)
 *     bool visible;        // Visibility state flag
 *     int chromeMode;      // FrameChromeMode (FRAME_DECORATED / BORDERLESS / NAKED)
 *     FrameLayer layers[DARLING_FRAME_MAX_LAYERS]; // Stacked FBOs inside CAMetalLayer
 *     uint32_t layerCount; // Active layer count
 *     Dialog *childDialogs[DARLING_FRAME_MAX_DIALOGS]; // Managed child dialogs
 *     uint32_t childDialogCount; // Active child dialog count
 *     Dialog *ownerDialog; // Owning Dialog instance if embedded in a Dialog
 *     struct Frame *parentFrame; // Parent frame if this frame is a child dialog (bidirectional tracking)
 *     bool (*onQuitRequested)(struct Frame *frame, void *userData); // Quit-request callback
 *     void *quitRequestedUserData; // User data for the quit-request callback
 *     bool hasVisualEffect; // NSVisualEffectView vibrancy enabled
 *     int visualEffectMaterial; // FrameVisualEffectMaterial
 *     bool presentsWithTransaction; // CAMetalLayer presentsWithTransaction = YES
 *     uint32_t presentedFrames; // Confirmed seam presents since attach (infancy gate)
 *     uint32_t emptyPresents; // Consecutive empty seam presents (empty-cap guard)
 *     uint64_t lastPublishGen; // Last observed VkLayer publish generation (probe re-arm)
 *     int width;           // Window width in points
 *     int height;          // Window height in points
 *     bool inLiveResize;   // Live-resize drag in progress
 *     bool isMinimized;    // Window minimized state
 *     bool isZoomed;       // Window zoomed state
 *     bool inSyncResize;   // Re-entrancy guard: one render+present per geometry event
 *     int drawableWidth;   // Authoritative native-px footprint of the live content rect
 *     int drawableHeight;  // (resolved via convertRectToBacking at geometry time)
 *     FrameFunction *functions; // Master-arena grown slot table (doubling)
 *     uint32_t functionCount; // Active frame-function slot count
 *     uint32_t functionCapacity; // Frame-function slot capacity
 *     KeyMap *keyMap;      // Master-arena KeyMap; lazily created on first bind
 *     uint64_t lastRenderNanos; // Monotonic clock at last Frame_render (dt source)
 *     void *nativeView;    // Pointer to platform NSView / CAMetalLayer container
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - FrameCocoa_attach(frame)
 *   - FrameCocoa_detach(frame)
 *   - FrameCocoa_syncTransaction(frame)
 *   - Frame_platformAttach(frame)
 *   - Frame_platformDetach(frame)
 *   - Frame_platformSyncTransaction(frame)
 *
  * Private:
  *   - frameCocoaResizeHook(userdata) (static) : WindowResizeRenderFn seam —
  *     runs per drag step on thread 0 inside ONE explicit CATransaction
  *     (begin + setDisableActions:YES at top, commit after the forced
  *     present — the Single-Transaction Live Coordination Law): live OS
  *     bounds read (never the truncated cache), drawableSize re-chase from
  *     convertRectToBacking on the LIVE FRACTIONAL bounds — the WindowServer's
  *     own device-pixel rule, never lround(lround(frac) x scale), which
  *     double-rounds and toggles ±1px mid-drag (the Native Pixel Law + the
  *     Continuous Real-Time Live Resize Law) + EXPLICIT seam.frame =
  *     contentView.bounds and seam.bounds = same rect (points, sticky manual
  *     geometry — the ONLY mover, AppKit autoresizing is off), live scale
  *     pinned via TextCore_setBackingScaleOverride(liveScale) so button /
  *     label / input raster and retained-layer px math track
  *     the dragged window (never mainScreen mid-drag), then ONE
  *     Frame_resize (Frame_render + Frame_present live inside it), then
  *     re-armed dirty + GraphicsLoop_modalTickForced so the window RENDERS
  *     at the NEW size and PRESENTS synchronously within the same step —
  *     aggressive demand that CANNOT skip on not-dirty/not-ready. The chase
  *     + manual geometry + layout + present land atomically with the edge,
  *     so WindowServer never stretches the old drawable to an auto-moved
  *     frame; the stretch seen before was AppKit moving the seam in ITS
  *     transaction while drawableSize + present ran in OURS (two movers).
  *     The transaction never spans sendEvent — begin/commit both live
  *     inside this hook (the No-Transaction-Across-Event-Dispatch Law).
  *     A drop keeps dirty armed inside the loop (the Present-On-Demand
  *     Law), so the next step retries with fresher state; idle windows
  *     stay dirty-clean and rest. The GfxLoop's own steps are starved
  *     while AppKit's modal tracking loop owns thread 0, so the resize
  *     hook is the only live seam. No new thread, no wait. Seam layer
  *     contract (FrameCocoa_attach, sticky): contentsGravity TopLeft,
  *     presentsWithTransaction YES, anchorPoint (0,0), geometryFlipped YES,
  *     autoresizingMask kCALayerNotSizable (0) — the canvas NEVER tracks
  *     the window natively; the hook moves it explicitly every step and
  *     presents on the WindowServer transaction.
  *   - frameCocoaOnResized/OnMinimized/OnRestored/OnZoom/... (static) :
  *     WindowEvent bridge callbacks. OnResized stands down while live (the
  *     hook owns live steps) and handles settle steps only — clearing the
  *     live scale override (resync to mainScreen) before Frame_resize.
  *   - seamLayerOf(frame) (static) : resolves the seam CAMetalLayer from
 *     frame.nativeView (NSView layer or bare CAMetalLayer).
 * ============================================================================
 */

// The seam CAMetalLayer: the window's single on-screen layer (the Frame
// seam canvas — boards are retained offscreen VkLayer targets). Resolves
// from frame.nativeView, which is the NSVisualEffectView when blur is on,
// else the bare CAMetalLayer. The seam is ALWAYS a sublayer of the view's
// OWN backing layer (never the view's layer itself — a view-owned layer
// lets AppKit reset its drawableSize to 1x1 and force resize-gravity on
// every live-resize beat, the top/right stretch; the Window Compositing
// Layer Order Law tree: blur view -> seam child).
static CAMetalLayer *seamLayerOf(Frame *frame) {
    if (frame == nullptr || (*frame).nativeView == nullptr)
        return nullptr;
    id obj = (__bridge id) (*frame).nativeView;
    if ([obj isKindOfClass:[CAMetalLayer class]])
        return (CAMetalLayer*) obj;
    if ([obj isKindOfClass:[NSView class]]) {
        CALayer *host = [(NSView*) obj layer];
        if ([host isKindOfClass:[CAMetalLayer class]])
            return (CAMetalLayer*) host;
        // Sublayer seam: named at attach so we never mistake an unrelated
        // CAMetalLayer sublayer
        // for the canvas. Fall back to the first CAMetalLayer child.
        for (CALayer *sub in [host sublayers]) {
            if ([[sub name] isEqualToString:@"vexgraph.seam"])
                return (CAMetalLayer*) sub;
        }
        for (CALayer *sub in [host sublayers]) {
            if ([sub isKindOfClass:[CAMetalLayer class]])
                return (CAMetalLayer*) sub;
        }
    }
    return nullptr;
}

// The ONE seam-layer resolution for the whole stack (the Single Seam
// Identity Law): the Vulkan surface binding (Frame_seamMetalLayer, consumed
// by darlingSeamMetalLayer -> VkMac_createSurfaceForLayer) and the per-step
// resize target (seamLayerOf) MUST resolve the same CAMetalLayer. Handing
// MoltenVK the raw NSVisualEffectView makes it bind the blur view's own
// backing layer instead of the seam sublayer — the surface's currentExtent
// then never tracks drawableSize (frozen caps, pinned canvas, trailing
// strip on every resize).
void *Frame_seamMetalLayer(Frame *frame) {
    return (__bridge void*) seamLayerOf(frame);
}

void Frame_platformSyncLayer(Frame *frame, int width, int height) {
    if (frame == nullptr || (*frame).window == nullptr)
        return;
    static int s_syncTrace = -1;
    if (s_syncTrace < 0)
        s_syncTrace = getenv("ANTI_RESIZE_TRACE") != nullptr;
    NSWindow *nsWindow = (__bridge NSWindow*) Window_nativeHandle((*frame).window);
    if (nsWindow == nil) {
        if (s_syncTrace) fprintf(stderr, "seam:sync no-window\n");
        return;
    }
    CGFloat liveScale = [nsWindow backingScaleFactor];
    if (liveScale <= 0.0)
        liveScale = 1.0;

    extern void TextCore_setBackingScaleOverride(float scale);
    TextCore_setBackingScaleOverride((float) liveScale);

    CAMetalLayer *seam = seamLayerOf(frame);
    if (s_syncTrace)
        fprintf(stderr, "seam:sync native=%s seam=%p scale=%.2f\n",
                (*frame).nativeView ? "set" : "nil",
                (__bridge void*) seam, (double) liveScale);
    if (seam != nil) {
        [seam setContentsScale:liveScale];
        NSView *contentView = [nsWindow contentView];
        if (contentView != nil) {
            // Authoritative device footprint: the WindowServer maps the LIVE
            // fractional bounds (never the lround'ed point cache) to device
            // pixels with its own trailing-edge rule — convertRectToBacking is
            // that mapping, so the drawable matches the on-screen footprint
            // exactly. Deriving px from an already-rounded point size
            // (lround(lround(frac) x scale)) double-rounds and toggles ±1px as
            // a drag crosses a .5 boundary — the one-pixel live-resize jitter.
            NSRect liveBounds = [contentView bounds];
            NSRect backing = [contentView convertRectToBacking:liveBounds];
            if (lround(backing.size.width) <= 0 || lround(backing.size.height) <= 0)
                return;
            int drawW = (int) lround(backing.size.width);
            int drawH = (int) lround(backing.size.height);
            (*frame).drawableWidth = drawW;
            (*frame).drawableHeight = drawH;
            // Publish the LIVE fractional bounds for the layout currency: the
            // WindowServer maps THESE to the device px above, so every
            // container must resolve against them (never the rounded ints —
            // lround(800.5) = 801 puts every parent-derived edge half a point
            // off the device grid, wobbling per drag step).
            (*frame).liveWidth = (float) liveBounds.size.width;
            (*frame).liveHeight = (float) liveBounds.size.height;
            // Fixed-buffer model: the drawable is the display-sized buffer
            // already allocated at attach, so this step only publishes the
            // RENDER AREA (the live px region the pass scissors to) and moves
            // the layer frame to the live bounds — the WindowServer's own
            // fractional mapping. TopLeft gravity shows the buffer's top-left
            // 1:1, so the visible pixels are exactly the live region: no
            // scaling, no strip, no swapchain rebuild per step.
            extern void Vk_seamSetExtent(int32_t widthPx, int32_t heightPx);
            Vk_seamSetExtent(drawW, drawH);
            // Keep the drawable in lockstep with the chain extent (the fixed
            // monitor-sized buffer) — write only when it actually differs, so
            // a drag step costs one property read, never a drawable churn.
            extern void Vk_seamExtent(int32_t *outW, int32_t *outH);
            int32_t chainW = 0;
            int32_t chainH = 0;
            Vk_seamExtent(&chainW, &chainH);
            if (chainW > 0 && chainH > 0) {
                CGSize cur = [seam drawableSize];
                if (lround(cur.width) != (long) chainW || lround(cur.height) != (long) chainH)
                    [seam setDrawableSize:CGSizeMake((CGFloat) chainW, (CGFloat) chainH)];
            } else {
                [seam setDrawableSize:CGSizeMake((CGFloat) drawW, (CGFloat) drawH)];
            }
            [seam setContentsGravity:kCAGravityTopLeft];
            [seam setAnchorPoint:CGPointMake(0.0, 0.0)];
            [seam setGeometryFlipped:YES];
            if (s_syncTrace)
                fprintf(stderr, "seam:sync bounds=%.1fx%.1f draw=%dx%d chain=%dx%d area=%dx%d\n",
                        (double) liveBounds.size.width, (double) liveBounds.size.height,
                        drawW, drawH, chainW, chainH, drawW, drawH);
            // Layer frame = the BUFFER's own point size (chain px / scale),
            // never the live bounds: Core Animation resolves the drawable into
            // the layer's bounds × contentsScale, so a monitor-sized drawable
            // in a window-sized layer is MAGNIFIED (the live-resize stretch).
            // Matching the layer to the buffer keeps the mapping exactly 1:1;
            // the window then crops via clipping (masksToBounds set at attach
            // on the seam's parent), which is the fixed-buffer viewport.
            // Fallback to the live bounds before the chain exists.
            CGFloat seamW = chainW > 0 ? (CGFloat) chainW / liveScale : liveBounds.size.width;
            CGFloat seamH = chainH > 0 ? (CGFloat) chainH / liveScale : liveBounds.size.height;
            NSRect seamRect = NSMakeRect(0.0, 0.0, seamW, seamH);
            [seam setFrame:seamRect];
            [seam setBounds:seamRect];
            [seam setPosition:CGPointMake(0.0, 0.0)];
            return;
        }
        // No content view (degenerate/borderless): fall back to the layout
        // points passed in — integral points times scale is exact, no jitter.
        [seam setDrawableSize:CGSizeMake((CGFloat) lround((double) width * liveScale),
                                         (CGFloat) lround((double) height * liveScale))];
        (*frame).liveWidth = (float) width;
        (*frame).liveHeight = (float) height;
    }
}

static bool frameCocoaHookInstalled(Frame *frame) {
    if (frame == nullptr || (*frame).window == nullptr)
        return false;
    return Window_getResizeRenderHook((*frame).window) != nullptr;
}

static void frameCocoaResizeHook(void *userdata) {
    Frame *frame = (Frame*) userdata;
    if (frame == nullptr || (*frame).window == nullptr)
        return;

    NSWindow *nsWindow = (__bridge NSWindow*) Window_nativeHandle((*frame).window);
    if (nsWindow == nil)
        return;
    NSView *cv = [nsWindow contentView];
    NSRect liveContent = cv != nil ? [cv bounds] : [nsWindow contentRectForFrameRect:[nsWindow frame]];
    if (liveContent.size.width <= 0.0 || liveContent.size.height <= 0.0)
        return;
    int w = (int) lround(liveContent.size.width);
    int h = (int) lround(liveContent.size.height);
    if (w <= 0 || h <= 0)
        return;

    [CATransaction begin];
    [CATransaction setDisableActions:YES];

    // Sync the seam layer FIRST so drawableWidth/drawableHeight (resolved via
    // convertRectToBacking on the live fractional bounds) are authoritative
    // BEFORE Frame_syncResize → Darling_preFrame → Darling_attachPanelBoards
    // reads them. Without this, the board attach falls back to
    // lround(lround(frac) × scale) — the double-round ±1px jitter.
    Frame_platformSyncLayer(frame, w, h);

    bool presented = Frame_syncResize(frame, w, h);

    [CATransaction commit];
    [CATransaction flush];

    // Drop ticket (the Present-On-Demand Law): if the synchronous present
    // failed mid-drag (swapchain rebuild, minimized gate, device lost), re-arm
    // the client so the next loop step or drag step retries with fresher state
    // — a dropped step must never rest as a lagging frame.
    if (!presented && (*frame).window != nullptr)
        GraphicsLoop_markDirty(GraphicsLoop_default(), (*frame).window);
}

static void frameCocoaOnResized(void *self, Window *window, int width, int height) {
    (void) window;
    Frame *frame = (Frame*) self;
    if (frame == nullptr)
        return;

    if (Window_isLiveResizing((*frame).window))
        return;
    extern void TextCore_clearBackingScaleOverride(void);
    TextCore_clearBackingScaleOverride();

    // Uniform deferral (one render+present per geometry event): when a resize
    // render hook is installed it fires for this same geometry via
    // windowRefreshSize — the hook is the single renderer and this handler
    // only stands down. Shim-less windows (no hook) keep the direct path.
    if (frameCocoaHookInstalled(frame))
        return;

    [CATransaction begin];
    [CATransaction setDisableActions:YES];

    Frame_syncResize(frame, width, height);

    [CATransaction commit];
    [CATransaction flush];
}

static void frameCocoaOnMinimized(void *self, Window *window) {
    (void) window;
    Frame *frame = (Frame*) self;
    if (frame == nullptr)
        return;

    // Pure flag flip: the window is occluded inside the dock genie — no
    // render, no present (Frame_present never presented anyway; the frame's
    // isMinimized flag is the only state the genie path owns).
    (*frame).isMinimized = true;
}

static void frameCocoaOnRestored(void *self, Window *window) {
    (void) window;
    Frame *frame = (Frame*) self;
    if (frame == nullptr)
        return;

    (*frame).isMinimized = false;
    // Uniform deferral: the resize hook renders this geometry (windowRefreshSize
    // fires it for the deminiaturize settle); only shim-less windows render here.
    if (frameCocoaHookInstalled(frame))
        return;

    NSWindow *nsWindow = (__bridge NSWindow*) Window_nativeHandle((*frame).window);
    if (nsWindow != nil) {
        NSView *cv = [nsWindow contentView];
        NSRect liveContent = cv != nil ? [cv bounds] : [nsWindow contentRectForFrameRect:[nsWindow frame]];
        int w = (int) lround(liveContent.size.width);
        int h = (int) lround(liveContent.size.height);
        if (w > 0 && h > 0) {
            [CATransaction begin];
            [CATransaction setDisableActions:YES];
            Frame_syncResize(frame, w, h);
            [CATransaction commit];
            [CATransaction flush];
            return;
        }
    }
    Frame_render(frame);
    Frame_present(frame);
}

static void frameCocoaOnZoom(void *self, Window *window) {
    (void) window;
    Frame *frame = (Frame*) self;
    if (frame == nullptr)
        return;

    // Mirror the NATIVE zoom state (never a blind toggle — windowShouldZoom
    // fires before the frame change and delegates may fire twice per zoom).
    NSWindow *nsWindow = (__bridge NSWindow*) Window_nativeHandle((*frame).window);
    if (nsWindow != nil)
        (*frame).isZoomed = [nsWindow isZoomed];

    // Uniform deferral: this event fires BEFORE AppKit moves the frame, so
    // rendering here would present a stale-size frame. The resize hook renders
    // every geometry step of the zoom animation and the settle; only shim-less
    // windows keep the direct path.
    if (frameCocoaHookInstalled(frame))
        return;

    if (nsWindow != nil) {
        NSView *cv = [nsWindow contentView];
        NSRect liveContent = cv != nil ? [cv bounds] : [nsWindow contentRectForFrameRect:[nsWindow frame]];
        int w = (int) lround(liveContent.size.width);
        int h = (int) lround(liveContent.size.height);
        if (w > 0 && h > 0) {
            [CATransaction begin];
            [CATransaction setDisableActions:YES];
            Frame_syncResize(frame, w, h);
            [CATransaction commit];
            [CATransaction flush];
            return;
        }
    }
    Frame_render(frame);
    Frame_present(frame);
}

static void frameCocoaOnFullscreen(void *self, Window *window) {
    (void) window;
    Frame *frame = (Frame*) self;
    if (frame == nullptr)
        return;

    // Uniform deferral: the fullscreen animation drives setFrameSize per step
    // (each fires the resize hook with live bounds); this pre-transition event
    // must not render a stale-size frame. Shim-less windows keep the direct path.
    if (frameCocoaHookInstalled(frame))
        return;

    NSWindow *nsWindow = (__bridge NSWindow*) Window_nativeHandle((*frame).window);
    if (nsWindow != nil) {
        NSView *cv = [nsWindow contentView];
        NSRect liveContent = cv != nil ? [cv bounds] : [nsWindow contentRectForFrameRect:[nsWindow frame]];
        int w = (int) lround(liveContent.size.width);
        int h = (int) lround(liveContent.size.height);
        if (w > 0 && h > 0) {
            [CATransaction begin];
            [CATransaction setDisableActions:YES];
            Frame_syncResize(frame, w, h);
            [CATransaction commit];
            [CATransaction flush];
            return;
        }
    }
    Frame_render(frame);
    Frame_present(frame);
}

static bool frameCocoaOnQuitRequested(void *self, Window *window) {
    (void) window;
    Frame *frame = (Frame*) self;
    if (frame == nullptr)
        return true;
    // A dialog's own window must close through Dialog_close (full cleanup:
    // open flag, bridge, key gate, pair glue, handler refocus) — never as a
    // raw AppKit close that would leave a zombie holder behind.
    if ((*frame).ownerDialog != nullptr)
        return Dialog_requestClose((*frame).ownerDialog);
    if (!Frame_canClose(frame))
        return false;
    Frame_closeChildDialogs(frame);
    return true;
}

static void frameCocoaOnFocusGained(void *self, Window *window) {
    (void) window;
    Frame *frame = (Frame*) self;
    if (frame == nullptr)
        return;
    // A dialog's own window gaining key (or click) restacks the pair so the
    // handler sits directly below the dialog — never an app sandwiched
    // between frame and dialog.
    if ((*frame).ownerDialog != nullptr)
        Dialog_bringToFront((*frame).ownerDialog);
    Dialog *clinging = Frame_getActiveClingingDialog(frame);
    if (clinging != nullptr) {
        Dialog_focus(clinging);
        Dialog_bringToFront(clinging);
    }
}

static void frameCocoaOnPressed(void *self, Window *window) {
    (void) window;
    Frame *frame = (Frame*) self;
    if (frame == nullptr)
        return;
    if ((*frame).ownerDialog != nullptr)
        Dialog_bringToFront((*frame).ownerDialog);
    Dialog *clinging = Frame_getActiveClingingDialog(frame);
    if (clinging != nullptr) {
        Dialog_focus(clinging);
        Dialog_bringToFront(clinging);
    }
}

void FrameCocoa_attach(Frame *frame) {
    if (frame == nullptr || (*frame).window == nullptr)
        return;

    @autoreleasepool {
        void *nativeWin = Window_nativeHandle((*frame).window);
        if (nativeWin == nullptr)
            return;

        NSWindow *nsWindow = (__bridge NSWindow*) nativeWin;
        // AppKit's live-resize content preservation snapshots the window and
        // SCALES the old canvas to the new bounds every drag step until the
        // app redraws — the leading-edge stretch at top/right drags. We
        // redraw every step (the forced modal tick), so the cache only adds
        // a stretched intermediate. Stand it down: the window shows our
        // pinned per-step pixels, never a scaled cache.
        [nsWindow setPreservesContentDuringLiveResize:NO];
        NSView *contentView = [nsWindow contentView];
        if (contentView == nil)
            return;

        NSRect bounds = [contentView bounds];
        CAMetalLayer *metalLayer = [CAMetalLayer layer];
        metalLayer.name = @"vexgraph.seam";
        if (getenv("ANTI_RESIZE_TRACE") != nullptr)
            fprintf(stderr, "seam:attach layer=%p vfx=%d\n", (__bridge void*) metalLayer,
                    (*frame).hasVisualEffect ? 1 : 0);
        // Fixed-buffer model (the single-seam plaster): the seam chain is
        // allocated ONCE at the display's native pixel size, and the window is
        // a top-left CROP of it (kCAGravityTopLeft = non-resizing gravity: the
        // drawable is drawn 1:1 and clipped, never scaled). Window resizes then
        // cost only a render area + layer frame per step — no swapchain
        // rebuild, no strip, no scaling. Publish the max BEFORE Vulkan init so
        // the first chain build uses it.
        extern void Vk_seamSetMaxExtent(int32_t widthPx, int32_t heightPx);
        NSScreen *scr = [nsWindow screen] ?: [NSScreen mainScreen];
        CGFloat screenScale = scr != nil ? [scr backingScaleFactor] : 1.0;
        if (screenScale <= 0.0)
            screenScale = 1.0;
        int32_t maxPxW = 0;
        int32_t maxPxH = 0;
        if (scr != nil) {
            NSRect sframe = [scr frame];
            maxPxW = (int32_t) lround(sframe.size.width * screenScale);
            maxPxH = (int32_t) lround(sframe.size.height * screenScale);
        }
        Vk_seamSetMaxExtent(maxPxW, maxPxH);
        metalLayer.presentsWithTransaction = YES;
        metalLayer.contentsGravity = kCAGravityTopLeft;
        metalLayer.anchorPoint = CGPointMake(0.0, 0.0);
        metalLayer.geometryFlipped = YES;
        metalLayer.bounds = bounds;
        metalLayer.frame = bounds;
        // Sticky seam: AppKit MUST NOT move this layer in its own
        // transaction (two movers = slinky stretch). kCALayerNotSizable (0)
        // disables native tracking; the resize hook sets frame + bounds
        // explicitly every drag step inside the single live transaction.
        metalLayer.autoresizingMask = kCALayerNotSizable;
        // Native Pixel Law: contentsScale mirrors the backing scale factor
        // and drawableSize is set in native hardware pixels at attach —
        // bounds (logical points) x scale. The resize hook re-chases both
        // per drag step via convertRectToBacking on the live fractional
        // bounds (see Frame_platformSyncLayer); the swapchain always
        // matches the screen 1:1.
        CGFloat backingScale = [nsWindow backingScaleFactor];
        if (backingScale <= 0.0)
            backingScale = 1.0;
        metalLayer.contentsScale = backingScale;
        metalLayer.drawableSize = CGSizeMake(bounds.size.width * backingScale,
                                             bounds.size.height * backingScale);

        if ((*frame).hasVisualEffect) {
            NSVisualEffectView *vfx = [[NSVisualEffectView alloc] initWithFrame:bounds];
            [vfx setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
            [vfx setBlendingMode:NSVisualEffectBlendingModeBehindWindow];
            [vfx setMaterial:(NSVisualEffectMaterial) (*frame).visualEffectMaterial];
            [vfx setState:NSVisualEffectStateActive];

            // The law's tree: blur view owns its OWN backing layer; the seam
            // is a CHILD of it (blur -> seam). A view-owned seam
            // ([vfx setLayer:metalLayer]) hands the canvas to AppKit, which
            // resets drawableSize to 1x1 and applies resize-gravity on every
            // live-resize beat — the top/right stretch. Sublayer = AppKit
            // never touches the canvas geometry or drawable.
            [vfx setWantsLayer:YES];
            CALayer *blurLayer = [vfx layer];
            // The seam layer is the monitor-sized plaster buffer (fixed-buffer
            // model), so its PARENT must clip: the window's bounds are the
            // viewport onto that buffer. Without this the buffer draws over
            // the whole display area the view covers.
            [blurLayer setMasksToBounds:YES];
            [blurLayer addSublayer:metalLayer];
            [contentView addSubview:vfx positioned:NSWindowBelow relativeTo:nil];
            (*frame).nativeView = (__bridge_retained void*) vfx;
        } else {
            [contentView setWantsLayer:YES];
            if (contentView.layer != nil) {
                // Clip: the seam is a monitor-sized buffer; the content view's
                // bounds are the window's viewport onto it (pane of glass).
                [contentView.layer setMasksToBounds:YES];
                [contentView.layer insertSublayer:metalLayer atIndex:0];
            } else {
                [contentView setLayer:metalLayer];
            }
            (*frame).nativeView = (__bridge_retained void*) metalLayer;
        }

        // Bridge resize hook and WindowServer cadence
        Window_setResizeRenderHook((*frame).window, frameCocoaResizeHook, frame);

        WindowEvent *ev = Window_getLifecycle((*frame).window);
        if (ev != nullptr) {
            WindowEvent_setSelf(ev, frame);
            WindowEvent_setOnResized(ev, frameCocoaOnResized);
            WindowEvent_setOnMinimized(ev, frameCocoaOnMinimized);
            WindowEvent_setOnRestored(ev, frameCocoaOnRestored);
            WindowEvent_setOnZoomFilled(ev, frameCocoaOnZoom);
            WindowEvent_setOnFullscreen(ev, frameCocoaOnFullscreen);
            WindowEvent_setOnQuitRequested(ev, frameCocoaOnQuitRequested);
            WindowEvent_setOnFocusGained(ev, frameCocoaOnFocusGained);
            WindowEvent_setOnPressed(ev, frameCocoaOnPressed);
        }
    }
}

void FrameCocoa_detach(Frame *frame) {
    if (frame == nullptr)
        return;

    @autoreleasepool {
        if ((*frame).nativeView != nullptr) {
            id obj = (__bridge_transfer id) (*frame).nativeView;
            if ([obj isKindOfClass:[NSView class]]) {
                [(NSView*) obj removeFromSuperview];
            } else if ([obj isKindOfClass:[CALayer class]]) {
                [(CALayer*) obj removeFromSuperlayer];
            }
            (*frame).nativeView = nullptr;
        }

        if ((*frame).window != nullptr)
            Window_setResizeRenderHook((*frame).window, nullptr, nullptr);
    }
}

void FrameCocoa_reassertHook(Frame *frame) {
    if (frame == nullptr || (*frame).window == nullptr)
        return;
    Window_setResizeRenderHook((*frame).window, frameCocoaResizeHook, frame);
}

void Frame_platformReassertResizeHook(Frame *frame) {
    FrameCocoa_reassertHook(frame);
}

void FrameCocoa_syncTransaction(Frame *frame) {
    (void) frame;
    @autoreleasepool {
        [CATransaction begin];
        [CATransaction setDisableActions:YES];
        [CATransaction commit];
    }
}

void Frame_platformAttach(Frame *frame) {
    FrameCocoa_attach(frame);
}

void Frame_platformDetach(Frame *frame) {
    FrameCocoa_detach(frame);
}

void Frame_platformSyncTransaction(Frame *frame) {
    FrameCocoa_syncTransaction(frame);
}
