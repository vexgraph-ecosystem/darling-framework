#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>

#include <stdbool.h>
#include <stdint.h>

#include "annotation/overview.h"
#include "darling/frame.h"
#include "effect/visual_effect.h"
#include "lang/adapter.h"
#include "lang/context.h"
#include "lang/device.h"
#include "lang/surface.h"
#include "window/window.h"
#include "window/window_event.h"

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: Frame_cocoa (objc/frame_cocoa.m)
 * ============================================================================
 * Builds the Single-Seam Canvas hierarchy INSIDE a hotcwap R1 window (R1 owns
 * the window; the Frame borrows its content view and never creates or closes it):
 *
 *   Window (R1) contentView -> NSVisualEffectView (VisualEffect) -> CAMetalLayer
 *
 * The VisualEffect (blur chrome) is ALWAYS the parent of the seam; the
 * CAMetalLayer is a sticky, TopLeft-pinned, non-sizable sublayer of the material
 * view's layer, and the window bounds clip it (masksToBounds). Creating the seam
 * also assembles the object model on it: Context -> Surface -> Adapter -> Device.
 *
 * Window-state changes arrive through hotcwap's WindowEvent (onResized,
 * onZoomFilled/ZoomBack, onFullscreen, onRestored) and re-derive the seam's
 * NATIVE-pixel extent, resize the Device + Surface, and arm the demand loop
 * (the Native Pixel Law + the Continuous Real-Time Live Resize Law).
 *
 * STRUCT FIELDS: none (helper DarlingSeam holds the frame, seam and Context).
 * FUNCTION REGISTRY:
 *   Core Functions:
 *     - Frame_platformShow(frame, width, height, title)
 *     - Frame_platformFree(frame)
 * ============================================================================
 */

@interface DarlingSeam : NSObject
@property (assign, nonatomic) Frame *frame;          // borrowed (C struct)
@property (strong, nonatomic) CAMetalLayer *layer;   // the seam
@property (assign, nonatomic) Context *context;      // owned here (C struct)
@property (strong, nonatomic) NSView *host;          // the material view
- (void)resync;
@end

// One frame per process for now (the probe scaffold); the R1/Kernel path drives
// a single window, and multi-frame arrives with the Kernel registry.
static DarlingSeam *s_seam = nil;

@implementation DarlingSeam
- (void)resync {
    Frame *frame = self.frame;
    if (frame == nullptr)
        return;
    NSRect bounds = [self.host bounds];
    NSRect backing = [self.host convertRectToBacking:bounds];
    CGFloat scale = bounds.size.width > 0.0 ? backing.size.width / bounds.size.width : 1.0;
    if (scale <= 0.0)
        scale = 1.0;
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    if (self.layer != nil) {
        if (!CGRectEqualToRect(self.layer.frame, bounds))
            self.layer.frame = bounds;
        if (!CGRectEqualToRect(self.layer.bounds, bounds))
            self.layer.bounds = bounds;
        if (self.layer.contentsScale != scale)
            self.layer.contentsScale = scale;
        CGSize px = CGSizeMake(backing.size.width, backing.size.height);
        if (!CGSizeEqualToSize(self.layer.drawableSize, px))
            self.layer.drawableSize = px;
    }
    [CATransaction commit];
    Frame_setScale(frame, (float) scale);
    if (getenv("VEX_GEOMETRY_LOG") != nullptr) {
        static CGSize lastLogged = { 0, 0 };
        if (!CGSizeEqualToSize(lastLogged, backing.size)) {
            lastLogged = backing.size;
            fprintf(stderr,
                    "seam: hostView bounds=%.0fx%.0f pt | backing=%.0fx%.0f px | scale=%.2f | "
                    "layer bounds=%.0fx%.0f drawableSize=%.0fx%.0f contentsScale=%.2f\n",
                    bounds.size.width, bounds.size.height, backing.size.width, backing.size.height,
                    (double) scale, self.layer.bounds.size.width, self.layer.bounds.size.height,
                    self.layer.drawableSize.width, self.layer.drawableSize.height,
                    (double) self.layer.contentsScale);
        }
    }
    int w = (int) (backing.size.width + 0.5);
    int h = (int) (backing.size.height + 0.5);
    if (w > 0 && h > 0) {
        Device *device = Frame_getDevice(frame);
        if (device != nullptr)
            (void) Device_resize(device, (uint32_t) w, (uint32_t) h);
        Surface *surface = Frame_getSurface(frame);
        if (surface != nullptr)
            (void) Surface_resize(surface, (uint32_t) w, (uint32_t) h);
    }
    Frame_markDirty(frame);
    // Immediate on-demand: a window event (resize/zoom/fullscreen/backing) is
    // itself a demand ticket. AppKit's live-resize nested tracking loop blocks
    // the display-link tick, so the demand loop cannot step — without painting
    // NOW the seam changes extent while showing the PREVIOUS drawable, scaled
    // (the stretch). Paint synchronously inside the event.
    (void) Frame_present(frame);
}

// Deferred twin: a window transition reports its event BEFORE AppKit finishes
// re-laying the view (notably exiting fullscreen / zoom), so the bounds read now
// are stale and the seam lands at the wrong extent (the shrunk panels). Re-derive
// once now (immediate paint) and once on the next runloop turn against the
// settled bounds.
- (void)resyncSoon {
    [self resync];
    dispatch_async(dispatch_get_main_queue(), ^{
        [self resync];
    });
}
@end

static void frameCocoaResized(void *self, Window *window, int width, int height) {
    (void) self;
    (void) window;
    (void) width;
    (void) height;
    if (s_seam != nil)
        [s_seam resyncSoon];
}

static void frameCocoaNoArg(void *self, Window *window) {
    (void) self;
    (void) window;
    if (s_seam != nil)
        [s_seam resyncSoon];
}

bool Frame_platformShow(Frame *frame, int width, int height, const char *title) {
    (void) width;
    (void) height;
    (void) title;
    Window *window = (Window*) Frame_getPlatformWindow(frame);
    if (frame == nullptr || window == nullptr)
        return false;
    void *contentPtr = Window_contentView(window);
    if (contentPtr == nullptr)
        return false;
    @autoreleasepool {
        NSView *content = (__bridge NSView*) contentPtr;
        VisualEffect *vfx = Frame_getVisualEffect(frame);
        if (vfx != nullptr)
            (void) VisualEffect_attach(vfx, (__bridge void*) content);
        NSVisualEffectView *vfxView = vfx != nullptr
            ? (__bridge NSVisualEffectView*) VisualEffect_nativeHandle(vfx) : nil;
        NSView *host = vfxView != nil ? vfxView : content;
        [host setWantsLayer:YES];
        CALayer *hostLayer = [host layer];
        hostLayer.masksToBounds = YES;   // the window bounds clip the seam

        NSRect bounds = [host bounds];
        CGFloat scale = [[content window] backingScaleFactor];
        if (scale <= 0.0)
            scale = 1.0;
        CAMetalLayer *seam = [CAMetalLayer layer];
        seam.presentsWithTransaction = YES;
        seam.contentsGravity = kCAGravityTopLeft;
        seam.anchorPoint = CGPointMake(0.0, 0.0);
        seam.geometryFlipped = YES;
        seam.autoresizingMask = kCALayerNotSizable;
        seam.contentsScale = scale;
        seam.frame = bounds;
        seam.bounds = bounds;
        seam.drawableSize = CGSizeMake(bounds.size.width * scale, bounds.size.height * scale);
        [hostLayer addSublayer:seam];

        Context *context = Context_1(LANG_BACKEND_VULKAN);
        if (context == nullptr) {
            fprintf(stderr, "frame_cocoa: no Vulkan context\n");
            return false;
        }
        SurfaceDesc sd = { .backend = LANG_BACKEND_VULKAN, .native = (__bridge void*) seam,
                           .width = (uint32_t) (bounds.size.width * scale),
                           .height = (uint32_t) (bounds.size.height * scale),
                           .context = context };
        Surface *surface = Surface_new(&sd);
        Adapter *adapter = surface != nullptr ? Context_chooseAdapter(context, surface) : nullptr;
        Device *device = adapter != nullptr ? Adapter_createDevice(adapter, surface) : nullptr;
        if (device == nullptr) {
            fprintf(stderr, "frame_cocoa: no device (surface=%p adapter=%p)\n",
                    (void*) surface, (void*) adapter);
            if (surface != nullptr)
                Surface_destroy(surface);
            Context_destroy(context);
            return false;
        }
        fprintf(stderr, "frame_cocoa: seam %dx%d px scale=%.1f surface=%p device=%p\n",
                (int) (bounds.size.width * scale), (int) (bounds.size.height * scale),
                (double) scale, (void*) surface, (void*) device);
        Frame_setSurface(frame, surface);
        Frame_setDevice(frame, device);

        DarlingSeam *state = [[DarlingSeam alloc] init];
        state.frame = frame;
        state.layer = seam;
        state.context = context;
        state.host = host;
        s_seam = state;

        WindowEvent *ev = Window_getLifecycle(window);
        if (ev != nullptr) {
            WindowEvent_setSelf(ev, frame);
            WindowEvent_setOnResized(ev, frameCocoaResized);
            WindowEvent_setOnFullscreen(ev, frameCocoaNoArg);
            WindowEvent_setOnZoomFilled(ev, frameCocoaNoArg);
            WindowEvent_setOnZoomBack(ev, frameCocoaNoArg);
            WindowEvent_setOnRestored(ev, frameCocoaNoArg);
        }
        [state resync];
        return true;
    }
}

void Frame_platformFree(Frame *frame) {
    @autoreleasepool {
        VisualEffect *vfx = Frame_getVisualEffect(frame);
        if (vfx != nullptr)
            VisualEffect_detach(vfx);
        if (s_seam != nil && s_seam.frame == frame) {
            if (s_seam.context != nullptr)
                Context_destroy(s_seam.context);   // after the device + surface are gone
            s_seam = nil;
        }
        // The window belongs to R1: never closed here.
    }
}
