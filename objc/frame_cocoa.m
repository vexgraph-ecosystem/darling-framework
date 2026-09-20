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
  *     bounds read (never the truncated cache), drawableSize re-chase in
  *     rounded native px (Native Pixel Law) + EXPLICIT seam.frame =
  *     contentView.bounds and seam.bounds = same rect (points, sticky manual
  *     geometry — the ONLY mover, AppKit autoresizing is off), live scale
  *     pinned via TextCore_setBackingScaleOverride(liveScale) so button /
  *     label / input raster and attachLayers / attachPanes px math track
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
        // Sublayer seam: named at attach so we never mistake a DIRECT pane
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

static void frameCocoaResizeHook(void *userdata) {
    Frame *frame = (Frame*) userdata;
    if (frame == nullptr || (*frame).window == nullptr)
        return;

    // Live bounds straight from the OS (points, never the truncated cache):
    // a Retina sub-point step is a whole native pixel and must fire the seam.
    NSWindow *nsWindow = (__bridge NSWindow*) Window_nativeHandle((*frame).window);
    if (nsWindow == nil)
        return;
    NSRect liveContent = [nsWindow contentRectForFrameRect:[nsWindow frame]];
    if (liveContent.size.width <= 0.0 || liveContent.size.height <= 0.0)
        return;
    CGFloat liveScale = [nsWindow backingScaleFactor];
    if (liveScale <= 0.0)
        liveScale = 1.0;
    int w = (int) lround(liveContent.size.width);
    int h = (int) lround(liveContent.size.height);
    if (w <= 0 || h <= 0)
        return;

    // GEOMETRY PROBE (VEX_GEOMETRY_LOG=1): one line per drag step showing the
    // window frame (points, screen coords), content view bounds, seam layer
    // frame/anchorPoint/position/geometryFlipped BEFORE the native-pixel
    // chase — plus the drawable extent after it. Directional drag artifacts
    // (top/right vs bottom/left) localize to whichever line lags or drifts.
    static int geomLog = -1;
    if (geomLog < 0)
        geomLog = getenv("VEX_GEOMETRY_LOG") != nullptr;
    if (geomLog) {
        NSWindow *gw = (__bridge NSWindow*) Window_nativeHandle((*frame).window);
        NSRect wf = gw ? [gw frame] : NSZeroRect;
        NSRect bf = gw ? [[gw contentView] bounds] : NSZeroRect;
        CAMetalLayer *sl = seamLayerOf(frame);
        if (sl != nil) {
            NSRect lf = [sl frame];
            CGPoint ap = [sl anchorPoint];
            CGPoint pos = [sl position];
            CGSize dw = [sl drawableSize];
            fprintf(stderr, "geom pre: win=(%.0f,%.0f %.0fx%.0f) view=(%.0fx%.0f) layer=(%.0f,%.0f %.0fx%.0f) ap=(%.2f,%.2f) pos=(%.0f,%.0f) flip=%d scale=%.2f drawable=(%.0fx%.0f)\n",
                   wf.origin.x, wf.origin.y, wf.size.width, wf.size.height,
                   bf.size.width, bf.size.height,
                   lf.origin.x, lf.origin.y, lf.size.width, lf.size.height,
                   ap.x, ap.y, pos.x, pos.y, [sl isGeometryFlipped], [sl contentsScale],
                   dw.width, dw.height);
        }
    }

    // Single-transaction live step (the Single-Transaction Live
    // Coordination Law): the WHOLE step — drawableSize chase + Frame_resize
    // layout + forced present — lands in ONE explicit CATransaction with
    // actions disabled, so the chase and the present commit atomically with
    // the moving edge instead of in two transactions (chase outside,
    // present-only inside) that let WindowServer stretch the old drawable
    // to the new bounds until the next vsync. Never spans sendEvent:
    // begin/commit both live inside this hook (the
    // No-Transaction-Across-Event-Dispatch Law). The nested begin/commit
    // pairs inside the present path coalesce into this outer commit.
    // No wait added.
    [CATransaction begin];
    [CATransaction setDisableActions:YES];

    // Native Pixel Law: chase drawableSize in rounded native hardware pixels
    // from the live bounds + backing scale factor, BEFORE layout runs — the
    // swapchain extent must match the screen every drag step (the
    // Continuous Real-Time Live Resize Law). Sticky seam: the layer frame
    // NEVER tracks the window natively (autoresizingMask kCALayerNotSizable
    // at attach) — the hook sets frame + bounds explicitly from the live
    // content bounds (points) inside this same transaction, so ONLY this
    // transaction moves the seam. Points paint the CALayer frame, rounded
    // px paint the drawable — never truncated. The live scale is pinned
    // for raster + pane px math (button/label/input breathe fix).
    //
    // (externs are function-local per the panel_bridge.c seam pattern: no
    // text_core.h pull into ObjC, no cross-module include.)
    CAMetalLayer *seam = seamLayerOf(frame);
    if (seam != nil) {
        extern void TextCore_setBackingScaleOverride(float scale);
        TextCore_setBackingScaleOverride((float) liveScale);
        [seam setContentsScale:liveScale];
        [seam setDrawableSize:CGSizeMake((CGFloat) lround(liveContent.size.width * liveScale),
                                         (CGFloat) lround(liveContent.size.height * liveScale))];
        NSView *contentView = [nsWindow contentView];
        if (contentView != nil) {
            NSRect liveBounds = NSMakeRect(0.0, 0.0, liveContent.size.width, liveContent.size.height);
            // Per-step pin (not just at attach): AppKit/VFX can reset these
            // behind our back, and a gravityResize seam stretches the old
            // drawable to the new bounds for a frame — worst on top/right
            // drags where the origin also moves. TopLeft never scales.
            [seam setContentsGravity:kCAGravityTopLeft];
            [seam setAnchorPoint:CGPointMake(0.0, 0.0)];
            [seam setGeometryFlipped:YES];
            [seam setFrame:liveBounds];
            [seam setBounds:liveBounds];
            [seam setPosition:CGPointMake(0.0, 0.0)];
        }
        if (geomLog)
            fprintf(stderr, "geom post: win=(%dx%d) drawable=(%.0fx%.0f) scale=%.2f gravity=%s\n",
                   w, h, seam.drawableSize.width, seam.drawableSize.height, seam.contentsScale,
                   [[seam contentsGravity] isEqualToString:kCAGravityTopLeft] ? "TopLeft" : [[seam contentsGravity] UTF8String]);
    }

    // ONE attempt per step: Frame_resize already runs Frame_render +
    // Frame_present (darling/frame.c), so this hook owns the whole live step
    // and frameCocoaOnResized stands down while live (no double layout).
    Frame_resize(frame, w, h);

    // Aggressive live demand: re-arm the client dirty and run the FORCED
    // modal tick NOW so this window renders at the NEW size and presents
    // synchronously before the next drag step fires (the Continuous
    // Real-Time Live Resize Law — reference the new size, wait for the
    // render, then present). The forced path CANNOT skip on not-dirty or
    // not-ready; minimized + Vk_ready + bounded GPU waits stay honored
    // inside the present path, and the try-lock retries in ~1ms slices up
    // to ~8ms before dropping (the Bounded Wait Law). A drop keeps dirty
    // armed inside the loop (the Present-On-Demand Law), so the next step
    // retries with fresher state; idle windows stay dirty-clean and rest.
    // The GfxLoop's own steps are starved while AppKit's modal tracking
    // loop owns thread 0, so the resize hook is the only live seam. No new
    // thread, no unbounded wait — R3 owns the GPU bounds.
    GraphicsLoop *loop = GraphicsLoop_default();
    Window *win = (*frame).window;
    GraphicsLoop_markDirty(loop, win);
    GraphicsLoop_modalTickForced(win);

    [CATransaction commit];
}

static void frameCocoaOnResized(void *self, Window *window, int width, int height) {
    (void) window;
    Frame *frame = (Frame*) self;
    if (frame == nullptr)
        return;

    // Live steps belong to frameCocoaResizeHook (drawableSize chase first,
    // then ONE Frame_resize + best-effort modalTick): laying out here too
    // would run every drag step twice. Settle steps (not live) land here —
    // clearing the live scale pin so raster resyncs to mainScreen.
    if (Window_isLiveResizing((*frame).window))
        return;
    extern void TextCore_clearBackingScaleOverride(void);
    TextCore_clearBackingScaleOverride();
    Frame_resize(frame, width, height);
}

static void frameCocoaOnMinimized(void *self, Window *window) {
    (void) window;
    Frame *frame = (Frame*) self;
    if (frame == nullptr)
        return;

    (*frame).isMinimized = true;
    Frame_render(frame);
    Frame_present(frame);
}

static void frameCocoaOnRestored(void *self, Window *window) {
    (void) window;
    Frame *frame = (Frame*) self;
    if (frame == nullptr)
        return;

    (*frame).isMinimized = false;
    Frame_render(frame);
    Frame_present(frame);
}

static void frameCocoaOnZoom(void *self, Window *window) {
    (void) window;
    Frame *frame = (Frame*) self;
    if (frame == nullptr)
        return;

    (*frame).isZoomed = !(*frame).isZoomed;
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
        metalLayer.presentsWithTransaction = YES;
        metalLayer.contentsGravity = kCAGravityTopLeft;
        metalLayer.anchorPoint = CGPointMake(0.0, 0.0);
        metalLayer.geometryFlipped = YES;
        metalLayer.bounds = bounds;
        metalLayer.frame = bounds;
        // Sticky seam: AppKit MUST NOT move this layer in its own
        // transaction (two movers = slinky stretch). kCALayerNotSizable (0)
        // disables native tracking; frameCocoaResizeHook sets frame + bounds
        // explicitly every drag step inside the single live transaction.
        metalLayer.autoresizingMask = kCALayerNotSizable;
        // Native Pixel Law: contentsScale mirrors the backing scale factor
        // and drawableSize is set in native hardware pixels at attach —
        // bounds (logical points) x scale. The resize hook re-chases both
        // per drag step; the swapchain always matches the screen 1:1.
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
            [blurLayer addSublayer:metalLayer];
            [contentView addSubview:vfx positioned:NSWindowBelow relativeTo:nil];
            (*frame).nativeView = (__bridge_retained void*) vfx;
        } else {
            [contentView setWantsLayer:YES];
            if (contentView.layer != nil) {
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
