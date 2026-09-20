#ifndef __APPLE__

#include "darling/frame.h"
#include "annotation/definition.h"
#include "annotation/overview.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: FrameStub
 * ============================================================================
 * The non-Apple stub implementation of the Frame platform hooks
 * (Frame_platformAttach / Frame_platformDetach /
 * Frame_platformSyncTransaction / Frame_platformReassertResizeHook /
 * Frame_platformSyncLayer): every hook is a no-op because there is no AppKit
 * window, no NSVisualEffectView, and no CAMetalLayer seam off Apple. The live
 * counterpart is objc/frame_cocoa.m, which establishes the NSWindow ->
 * NSVisualEffectView -> CAMetalLayer hierarchy, the sticky manual seam
 * geometry, and the single-transaction live-resize hook. The whole file is
 * guarded by #ifndef __APPLE__ so the stub and the Cocoa shim never compile
 * together; it owns no struct — the Frame it operates on is owned by
 * darling/frame.h.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: FrameStub
 * LEVEL: L4 — Self-Management (Non-Apple platform Frame fallback)
 * ============================================================================
 * Stub implementation of platform hooks for non-Apple environments.
 *
 * STRUCT FIELDS: none — procedural stub (owns no struct; operates on the
 * Frame owned by darling/frame.h).
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - Frame_platformAttach(frame)
 *   - Frame_platformDetach(frame)
 *   - Frame_platformSyncTransaction(frame)
 * ============================================================================
 */

void Frame_platformAttach(Frame *frame) {
    (void) frame;
}

void Frame_platformDetach(Frame *frame) {
    (void) frame;
}

void Frame_platformSyncTransaction(Frame *frame) {
    (void) frame;
}

void Frame_platformReassertResizeHook(Frame *frame) {
    (void) frame;
}

void Frame_platformSyncLayer(Frame *frame, int width, int height) {
    (void) frame;
    (void) width;
    (void) height;
}

#endif // !__APPLE__
