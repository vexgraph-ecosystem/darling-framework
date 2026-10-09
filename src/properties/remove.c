#include "properties/remove.h"

#include "frame/frame_internal.h"   // Frame_clearPanels
#include "panel/panel_internal.h"
#include "ui/element.h"

#include "annotation/definition.h"
#include "annotation/overview.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Remove (properties/remove.c)
 * ============================================================================
 * The DETACH operation for the darling tree. It reverses add: the child's
 * Element leaves the parent's Element tree, and the wrapper leaves the parent's
 * owned set. Ownership is HANDED BACK, never destroyed — the caller decides
 * whether to re-home the child or Panel_destroy it. This is what lets a widget
 * move between parents without a free/realloc round trip.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: Remove (properties/remove.c)
 * ============================================================================
 * Detach entry points. No owned state.
 *
 * STRUCT FIELDS: none — no owned state.
 *
 * FUNCTION REGISTRY (exported by properties/remove.h):
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - Panel_remove_1(child)          // unlink, return the still-alive child
 *   - Frame_removePanels_1(frame)    // unlink + destroy every owned panel
 * Public macros:
 *   - Panel_remove(...), Frame_removePanels(...)  -> OVERLOAD_DISPATCH
 * ============================================================================
 */

Panel *Panel_remove_1(Panel *child) {
    if (!child) return nullptr;
    Element *cg = Panel_graphics(child);
    if (cg) Element_remove(cg);
    Panel *parent = Panel_parent(child);
    if (parent) Panel_disownChild(parent, child);
    return child;
}

// Detaches and destroys every panel currently owned by the frame.
void Frame_removePanels_1(Frame *frame) {
    Frame_clearPanels(frame);
}
