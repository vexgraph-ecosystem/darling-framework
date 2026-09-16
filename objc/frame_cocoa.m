#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>
#import <QuartzCore/CATransaction.h>
#import <Metal/Metal.h>

#include "frame_cocoa.h"
#include "annotation/overview.h"
#include "window/window.h"
#include "window/window_event.h"

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: FrameCocoa (OS CAMetalLayer frame shim)
 * LEVEL: L4 — Self-Management (AppKit NSVisualEffectView + CAMetalLayer bridge)
 * ============================================================================
 * Native macOS AppKit bridge establishing the window rendering hierarchy:
 *   NSWindow -> NSVisualEffectView -> CAMetalLayer (mtklayer)
 * where mtklayer is configured with presentsWithTransaction = YES to align
 * drawables atomically with the macOS WindowServer during live resize,
 * minimize, zoom, and restore events.
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
 * ============================================================================
 */

static void frameCocoaResizeHook(void *userdata) {
    Frame *frame = (Frame*) userdata;
    if (frame == nullptr || (*frame).window == nullptr)
        return;

    int w = Window_width((*frame).window);
    int h = Window_height((*frame).window);
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
