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

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: Frame_cocoa (objc/frame_cocoa.m)
 * ============================================================================
 * The macOS window for a Frame. Builds the Single-Seam Canvas hierarchy:
 *
 *   NSWindow -> contentView -> NSVisualEffectView -> CAMetalLayer (the one seam)
 *
 * The NSVisualEffectView (VisualEffect) is ALWAYS the parent of the seam: it is
 * the blur chrome, and the seam is a sticky, TopLeft-pinned, non-sizable sublayer
 * of the material view's layer (never the contentView's own layer — AppKit owns a
 * backing layer and resets drawableSize with resize gravity on every live-resize
 * beat). The window's bounds clip it (masksToBounds).
 *
 * Creating the window also assembles the object model: Context -> Surface (the
 * seam) -> Adapter -> Device. A window-state change (resize, zoom/fill, enter/
 * exit fullscreen, a move to another display, a backing-scale change) re-derives
 * the seam's NATIVE-pixel extent and arms the demand loop (the Native Pixel Law
 * + the Continuous Real-Time Live Resize Law).
 *
 * STRUCT FIELDS: none — the view carries the frame pointer, the seam layer and
 * the Context it must destroy.
 * FUNCTION REGISTRY:
 *   Core Functions:
 *     - Frame_platformShow(frame, width, height, title)
 *     - Frame_platformFree(frame)
 * ============================================================================
 */

@interface DarlingFrameView : NSView <NSWindowDelegate>
@property (assign, nonatomic) Frame *framePtr;
@property (assign, nonatomic) CAMetalLayer *seamLayer;
@property (assign, nonatomic) Context *context;   // owned: destroyed on free
@end

@implementation DarlingFrameView

// Re-derive the seam's native-pixel extent from the live window, resize the
// device + surface to match, and arm the demand loop — on every window-state
// change. One disabled-actions transaction so CoreAnimation never animates the
// geometry (the slinky).
- (void)syncSeam {
    Frame *frame = self.framePtr;
    if (frame == nullptr)
        return;
    NSRect bounds = [self bounds];
    NSRect backing = [self convertRectToBacking:bounds];
    CGFloat scale = bounds.size.width > 0.0 ? backing.size.width / bounds.size.width : 1.0;
    if (scale <= 0.0)
        scale = 1.0;
    CAMetalLayer *seam = self.seamLayer;
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    if (seam != nil) {
        if (!CGRectEqualToRect(seam.frame, bounds))
            seam.frame = bounds;
        if (!CGRectEqualToRect(seam.bounds, bounds))
            seam.bounds = bounds;
        if (seam.contentsScale != scale)
            seam.contentsScale = scale;
        CGSize px = CGSizeMake(backing.size.width, backing.size.height);
        if (!CGSizeEqualToSize(seam.drawableSize, px))
            seam.drawableSize = px;
    }
    [CATransaction commit];
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
}

- (void)setFrameSize:(NSSize)newSize {
    [super setFrameSize:newSize];
    [self syncSeam];
}
- (void)windowDidResize:(NSNotification *)note { (void) note; [self syncSeam]; }
- (void)windowDidEndLiveResize:(NSNotification *)note { (void) note; [self syncSeam]; }
- (void)windowDidEnterFullScreen:(NSNotification *)note { (void) note; [self syncSeam]; }
- (void)windowDidExitFullScreen:(NSNotification *)note { (void) note; [self syncSeam]; }
- (void)windowWillEnterFullScreen:(NSNotification *)note { (void) note; [self syncSeam]; }
- (void)windowWillExitFullScreen:(NSNotification *)note { (void) note; [self syncSeam]; }
- (void)windowDidChangeScreen:(NSNotification *)note { (void) note; [self syncSeam]; }
- (void)windowDidChangeBackingProperties:(NSNotification *)note { (void) note; [self syncSeam]; }
@end

bool Frame_platformShow(Frame *frame, int width, int height, const char *title) {
    if (frame == nullptr || width <= 0 || height <= 0 || title == nullptr)
        return false;
    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        NSRect rect = NSMakeRect(0.0, 0.0, (CGFloat) width, (CGFloat) height);
        NSWindow *window = [[NSWindow alloc]
            initWithContentRect:rect
                      styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                               | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable)
                        backing:NSBackingStoreBuffered
                          defer:NO];
        [window setTitle:[NSString stringWithUTF8String:title]];
        [window setReleasedWhenClosed:NO];
        [window setPreservesContentDuringLiveResize:NO];

        DarlingFrameView *view = [[DarlingFrameView alloc] initWithFrame:rect];
        view.framePtr = frame;
        [window setContentView:view];
        [window setDelegate:view];   // window-state events -> syncSeam

        // The blur chrome behind the seam (Single-Seam Canvas Law).
        VisualEffect *vfx = Frame_getVisualEffect(frame);
        if (vfx != nullptr)
            (void) VisualEffect_attach(vfx, (__bridge void*) view);
        NSVisualEffectView *vfxView = vfx != nullptr
            ? (__bridge NSVisualEffectView*) VisualEffect_nativeHandle(vfx) : nil;
        NSView *host = vfxView != nil ? vfxView : view;
        [host setWantsLayer:YES];
        CALayer *hostLayer = [host layer];
        hostLayer.masksToBounds = YES;   // window bounds clip the seam

        NSRect bounds = [host bounds];
        CGFloat scale = [[window screen] backingScaleFactor];
        if (scale <= 0.0)
            scale = 1.0;
        CAMetalLayer *seam = [CAMetalLayer layer];
        seam.presentsWithTransaction = YES;       // joined to the WindowServer transaction
        seam.contentsGravity = kCAGravityTopLeft; // 1:1 crop, never scaled
        seam.anchorPoint = CGPointMake(0.0, 0.0);
        seam.geometryFlipped = YES;               // Vulkan top-down in a non-flipped host
        seam.autoresizingMask = kCALayerNotSizable;
        seam.contentsScale = scale;
        seam.frame = bounds;
        seam.bounds = bounds;
        seam.drawableSize = CGSizeMake(bounds.size.width * scale, bounds.size.height * scale);
        [hostLayer addSublayer:seam];
        view.seamLayer = seam;

        // The object model: Context -> Surface (the seam) -> Adapter -> Device.
        Context *context = Context_1(LANG_BACKEND_VULKAN);
        if (context == nullptr) {
            [window close];
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
            if (surface != nullptr)
                Surface_destroy(surface);
            Context_destroy(context);
            [window close];
            return false;
        }
        view.context = context;                 // the view owns it; destroyed on free
        Frame_setSurface(frame, surface);
        Frame_setDevice(frame, device);
        Frame_setPlatformWindow(frame, (__bridge_retained void*) window);

        [window center];
        [window makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];
        [view syncSeam];
        return true;
    }
}

void Frame_platformFree(Frame *frame) {
    void *windowPtr = Frame_getPlatformWindow(frame);
    if (windowPtr == nullptr)
        return;
    @autoreleasepool {
        NSWindow *window = (__bridge_transfer NSWindow*) windowPtr;
        DarlingFrameView *view = (DarlingFrameView*) [window contentView];
        [window setDelegate:nil];
        if (view != nil && view.context != nullptr) {
            Context_destroy(view.context);   // after the device + surface are gone
            view.context = nullptr;
        }
        VisualEffect *vfx = Frame_getVisualEffect(frame);
        if (vfx != nullptr)
            VisualEffect_detach(vfx);
        [window close];
    }
}
