#include "properties/set_location.h"

#include "panel/panel.h"
#include "ui/element.h"

#include "annotation/definition.h"
#include "annotation/overview.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: SetLocation (properties/set_location.c)
 * ============================================================================
 * The LOCATION property, expressed as one name whose shape grows with arity.
 * A placement is three writes on the bound — the offset from the anchor, the
 * anchor part it resolves against on the parent, and the pivot part on the
 * element itself. Rather than three setters a caller must order correctly,
 * set_location applies them together: point, point+anchor, or full
 * point+anchor+pivot. Each richer form is the simpler one plus one write, so
 * there is exactly one path and no half-updated placement.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: SetLocation (properties/set_location.c)
 * ============================================================================
 * Placement entry points, layered by arity. No owned state.
 *
 * STRUCT FIELDS: none — no owned state.
 *
 * FUNCTION REGISTRY (exported by properties/set_location.h):
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - Panel_setLocation_3(panel, x, y)                  // offset only
 *   - Panel_setLocation_4(panel, x, y, anchor)          // + anchor on parent
 *   - Panel_setLocation_5(panel, x, y, anchor, pivot)   // + pivot on panel
 * Public macro:
 *   - Panel_setLocation(...)  -> OVERLOAD_DISPATCH picks _3 / _4 / _5
 * ============================================================================
 */

// Sets the panel offset without changing its anchor or pivot.
Panel *Panel_setLocation_3(Panel *panel, float x, float y) {
    if (panel) Element_setOffset(Panel_graphics(panel), x, y);
    return panel;
}

// Sets the panel offset and the anchor used against its parent.
Panel *Panel_setLocation_4(Panel *panel, float x, float y, int anchor) {
    Panel_setLocation_3(panel, x, y);
    if (panel) Element_setAnchor(Panel_graphics(panel), anchor);
    return panel;
}

// Sets the panel offset, parent anchor, and panel pivot together.
Panel *Panel_setLocation_5(Panel *panel, float x, float y, int anchor, int pivot) {
    Panel_setLocation_4(panel, x, y, anchor);
    if (panel) Element_setPivot(Panel_graphics(panel), pivot);
    return panel;
}
