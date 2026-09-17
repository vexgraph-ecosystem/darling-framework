#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>
#import <QuartzCore/CATransaction.h>
#import <Metal/Metal.h>

#include "frame_cocoa.h"
#include "annotation/overview.h"
#include "window/window.h"
#include "window/window_event.h"

void Dialog_focus(Dialog *dialog);
void Dialog_bringToFront(Dialog *dialog);
bool Dialog_requestClose(Dialog *dialog);

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
 * layer frame tracks the window natively via autoresizingMask, so resize
 * never waits on layout.
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
 *     re-chases drawableSize in native px (Native Pixel Law) and forwards
 *     to Frame_resize; runs per drag step on thread 0.
 *   - frameCocoaOnResized/OnMinimized/OnRestored/OnZoom/... (static) :
 *     WindowEvent bridge callbacks.
 *   - seamLayerOf(frame) (static) : resolves the seam CAMetalLayer from
 *     frame.nativeView (NSView layer or bare CAMetalLayer).
 * ============================================================================
 */

// The seam CAMetalLayer: the window's single on-screen layer (the Frame
// seam canvas — boards are retained offscreen VkLayer targets). Resolves
// from frame.nativeView, which is the NSVisualEffectView when blur is on,
// else the bare CAMetalLayer.
static CAMetalLayer *seamLayerOf(Frame *frame) {
    if (frame == nullptr || (*frame).nativeView == nullptr)
        return nullptr;
    id obj = (__bridge id) (*frame).nativeView;
    if ([obj isKindOfClass:[NSView class]])
        return (CAMetalLayer*) [(NSView*) obj layer];
    if ([obj isKindOfClass:[CAMetalLayer class]])
        return (CAMetalLayer*) obj;
    return nullptr;
}

static void frameCocoaResizeHook(void *userdata) {
    Frame *frame = (Frame*) userdata;
    if (frame == nullptr || (*frame).window == nullptr)
        return;

    int w = Window_width((*frame).window);
    int h = Window_height((*frame).window);
    if (w <= 0 || h <= 0)
        return;

    // Native Pixel Law: chase drawableSize in native hardware pixels from
    // the live bounds + backing scale factor, BEFORE layout runs — the
    // swapchain extent must match the screen every drag step (the
    // Continuous Real-Time Live Resize Law); the layer frame itself tracks
    // the window natively (autoresizingMask set at attach).
    CAMetalLayer *seam = seamLayerOf(frame);
    if (seam != nil) {
        NSWindow *nsWindow = (*frame).window ? (__bridge NSWindow*) Window_nativeHandle((*frame).window) : nil;
        CGFloat scale = nsWindow != nil ? [nsWindow backingScaleFactor] : 1.0;
        if (scale <= 0.0)
            scale = 1.0;
        [seam setContentsScale:scale];
        [seam setDrawableSize:CGSizeMake((CGFloat) w * scale, (CGFloat) h * scale)];
    }

    Frame_resize(frame, w, h);
}

static void frameCocoaOnResized(void *self, Window *window, int width, int height) {
    (void) window;
    Frame *frame = (Frame*) self;
    if (frame == nullptr)
        return;

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
        NSView *contentView = [nsWindow contentView];
        if (contentView == nil)
            return;

        NSRect bounds = [contentView bounds];
        CAMetalLayer *metalLayer = [CAMetalLayer layer];
        metalLayer.presentsWithTransaction = YES;
        metalLayer.contentsGravity = kCAGravityTopLeft;
        metalLayer.bounds = bounds;
        metalLayer.frame = bounds;
        metalLayer.autoresizingMask = kCALayerWidthSizable | kCALayerHeightSizable;
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

            [vfx setWantsLayer:YES];
            [vfx setLayer:metalLayer];
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
