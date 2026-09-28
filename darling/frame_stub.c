#include "darling/frame.h"

#include "annotation/overview.h"

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: FrameStub (darling/frame_stub.c)
 * ============================================================================
 * The platform hooks for a frame with no AppKit: Frame_show builds nothing and
 * returns false, and teardown is a no-op. On Apple, objc/frame_cocoa.m provides
 * the real NSWindow -> NSVisualEffectView -> CAMetalLayer hierarchy and replaces
 * this file in the build. The stub owns no struct — the Frame it operates on is
 * owned by darling/frame.h.
 *
 * STRUCT FIELDS: none — procedural stub.
 * FUNCTION REGISTRY:
 *   Core Functions:
 *     - Frame_platformShow(frame, width, height, title)
 *     - Frame_platformFree(frame)
 * ============================================================================
 */

bool Frame_platformShow(Frame *frame, int width, int height, const char *title) {
    (void) frame;
    (void) width;
    (void) height;
    (void) title;
    return false;   // no platform window here
}

void Frame_platformFree(Frame *frame) {
    (void) frame;
}
