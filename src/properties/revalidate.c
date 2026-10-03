#include "properties/revalidate.h"

#include "frame/frame_internal.h"
#include "vulkan/surface.h"

#include "annotation/definition.h"
#include "annotation/overview.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Revalidate (properties/revalidate.c)
 * ============================================================================
 * The REVALIDATE operation — the top-down render cascade expressed as one call.
 * A Frame does not paint itself directly; it asks its Surface to revalidate,
 * and the Surface walks its Boards. The content Board's step revalidates the
 * Element tree and paints it, then publishes a generation, and finally the
 * Surface presents the finished image to the host. This is why revalidate lives
 * with the other properties rather than in frame.c: it is the generic entry all
 * frames share, not window-specific behavior.
 *
 * The preamble (Frame_prepareRevalidate) sizes the frame from its window once;
 * that first sizing repaints, so revalidate must stop rather than recurse.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: Revalidate (properties/revalidate.c)
 * ============================================================================
 * Cascade entry points. No owned state.
 *
 * STRUCT FIELDS: none — no owned state.
 *
 * FUNCTION REGISTRY (exported by properties/revalidate.h):
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - Frame_revalidate_1(frame)
 * Public macro:
 *   - Frame_revalidate(...)  -> OVERLOAD_DISPATCH (arity 1 today)
 * ============================================================================
 */

void Frame_revalidate_1(Frame *frame) {
    if (!Frame_prepareRevalidate(frame)) return;   // unsized: setSize repaints
    Surface_revalidate(Frame_surface(frame));      // boards, then present
}
