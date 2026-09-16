#ifndef __APPLE__

#include "darling/frame.h"
#include "annotation/overview.h"

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: FrameStub
 * LEVEL: L4 — Self-Management (Non-Apple platform Frame fallback)
 * ============================================================================
 * Stub implementation of platform hooks for non-Apple environments.
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

#endif // !__APPLE__
