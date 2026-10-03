#include "properties/set_size.h"

#include "panel/panel.h"
#include "frame/frame_internal.h"
#include "ui/element.h"

#include "annotation/definition.h"
#include "annotation/overview.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: SetSize (properties/set_size.c)
 * ============================================================================
 * The SIZE property for the darling tree. A size write goes through the shared
 * bound (Property), so an element that aliases a Property resizes together, and
 * the rippling layout is picked up by the next revalidation rather than here.
 * Frame sizing first updates its private render targets through the widget's
 * resize seam, then invokes the revalidation cascade. An unchanged or invalid
 * extent is a no-op: it neither rebuilds targets nor publishes another frame.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: SetSize (properties/set_size.c)
 * ============================================================================
 * Size entry points. No owned state.
 *
 * STRUCT FIELDS: none — no owned state.
 *
 * FUNCTION REGISTRY (exported by properties/set_size.h):
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - Panel_setSize_3(panel, width, height)
 *   - Frame_setSize_3(frame, widthPx, heightPx)
 * Public macros:
 *   - Panel_setSize(...), Frame_setSize(...) select arity 3
 * ============================================================================
 */

Panel *Panel_setSize_3(Panel *panel, float width, float height) {
    if (panel) Element_setSize(Panel_graphics(panel), width, height);
    return panel;
}

void Frame_setSize_3(Frame *frame, int widthPx, int heightPx) {
    if (Frame_resizeTargets(frame, widthPx, heightPx)) Frame_revalidate(frame);
}
