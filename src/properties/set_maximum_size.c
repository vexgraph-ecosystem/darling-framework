#include "properties/set_maximum_size.h"

#include "panel/panel.h"
#include "ui/element.h"

#include "annotation/definition.h"
#include "annotation/overview.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: SetMaximumSize (properties/set_maximum_size.c)
 * ============================================================================
 * The MAXIMUM-SIZE property for the darling tree. It writes the ceiling on the
 * shared bound; the effective size is clamped at READ time (Property_width/
 * Height), so a ceiling set after a size still modulates layout and every
 * element aliasing the bound shares it. A non-positive extent clears the ceiling
 * on that axis. Order of setSize/setMaximumSize does not matter.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: SetMaximumSize (properties/set_maximum_size.c)
 * ============================================================================
 * Maximum-size entry points. No owned state.
 *
 * STRUCT FIELDS: none — no owned state.
 *
 * FUNCTION REGISTRY (exported by properties/set_maximum_size.h):
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - Panel_setMaximumSize_3(panel, width, height)
 * Public macro:
 *   - Panel_setMaximumSize(...)  -> OVERLOAD_DISPATCH (arity 3 today)
 * ============================================================================
 */

Panel *Panel_setMaximumSize_3(Panel *panel, float width, float height) {
    if (panel) Element_setMaximumSize(Panel_graphics(panel), width, height);
    return panel;
}
