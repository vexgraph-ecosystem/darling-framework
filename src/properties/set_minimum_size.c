#include "properties/set_minimum_size.h"

#include "panel/panel.h"
#include "ui/element.h"

#include "annotation/definition.h"
#include "annotation/overview.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: SetMinimumSize (properties/set_minimum_size.c)
 * ============================================================================
 * The MINIMUM-SIZE property for the darling tree. It writes the floor on the
 * shared bound; the effective size is clamped at READ time (Property_width/
 * Height), so a floor set after a size still modulates layout and every element
 * aliasing the bound shares it. Order of setSize/setMinimumSize does not matter.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: SetMinimumSize (properties/set_minimum_size.c)
 * ============================================================================
 * Minimum-size entry points. No owned state.
 *
 * STRUCT FIELDS: none — no owned state.
 *
 * FUNCTION REGISTRY (exported by properties/set_minimum_size.h):
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - Panel_setMinimumSize_3(panel, width, height)
 * Public macro:
 *   - Panel_setMinimumSize(...)  -> OVERLOAD_DISPATCH (arity 3 today)
 * ============================================================================
 */

Panel *Panel_setMinimumSize_3(Panel *panel, float width, float height) {
    if (panel) Element_setMinimumSize(Panel_graphics(panel), width, height);
    return panel;
}
