#include "properties/set_corner_radius.h"

#include "panel/panel.h"
#include "ui/element.h"

#include "annotation/definition.h"
#include "annotation/overview.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: SetCornerRadius (properties/set_corner_radius.c)
 * ============================================================================
 * The CORNER-RADIUS property. The radius lives on the shared bound (Property),
 * where it both rounds the fill and — when greater than zero — clips children to
 * the rounded shape. That makes it a single write with nothing to keep in step;
 * and because the bound is pooled and aliased, two elements sharing one Property
 * round together. Clamping (negative -> 0) happens in the Element setter.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: SetCornerRadius (properties/set_corner_radius.c)
 * ============================================================================
 * Corner-radius entry points. No owned state.
 *
 * STRUCT FIELDS: none — no owned state.
 *
 * FUNCTION REGISTRY (exported by properties/set_corner_radius.h):
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - Panel_setCornerRadius_2(panel, radius)
 * Public macro:
 *   - Panel_setCornerRadius(...)  -> OVERLOAD_DISPATCH (arity 2 today)
 * ============================================================================
 */

Panel *Panel_setCornerRadius_2(Panel *panel, float radius) {
    if (panel) Element_setRadius(Panel_graphics(panel), radius);
    return panel;
}
