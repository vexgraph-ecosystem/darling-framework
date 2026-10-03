#include "properties/set_size.h"

#include "panel/panel.h"
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
 * Grouped by property so classes with a size (Panel now, Frame's content and
 * others as they opt in) share one entry point whose arity can grow.
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
 * Public macro:
 *   - Panel_setSize(...)  -> OVERLOAD_DISPATCH (arity 3 today)
 * ============================================================================
 */

Panel *Panel_setSize_3(Panel *panel, float width, float height) {
    if (panel) Element_setSize(Panel_graphics(panel), width, height);
    return panel;
}
