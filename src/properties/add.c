#include "properties/add.h"

#include "frame/frame_internal.h"   // Frame_ownPanel
#include "panel/panel_internal.h"
#include "ui/element.h"

#include "annotation/definition.h"
#include "annotation/overview.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Add (properties/add.c)
 * ============================================================================
 * The ATTACH operation for the darling tree, grouped by property rather than
 * scattered through each widget file. Every `Class_add(...)` for darling lives
 * here so a widget file holds only that widget's own functions.
 *
 * Attaching is two writes that must agree: link the child's Element under the
 * parent's Element (the visual tree), then take the child wrapper into the
 * parent's owned set (the interface/ownership tree). Arity picks the shape —
 * append (2 args) or insert at an index (3 args). Ownership travels with the
 * parent: destroying the parent frees the subtree; removing a child hands the
 * wrapper back without freeing it.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: Add (properties/add.c)
 * ============================================================================
 * Attachment entry points. No owned state; each call links an existing child.
 *
 * STRUCT FIELDS: none — no owned state.
 *
 * FUNCTION REGISTRY (exported by properties/add.h):
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - Panel_add_2(parent, child)          // append
 *   - Panel_add_3(parent, child, index)   // insert at index
 *   - Frame_add_2(frame, panel)           // append to the frame's content
 *   - Frame_add_3(frame, panel, index)    // insert at index
 *   - Frame_addPanel(frame, desc)         // build a Panel, then attach
 * Public macros:
 *   - Panel_add(...), Frame_add(...)  -> OVERLOAD_DISPATCH picks _2 / _3
 * ============================================================================
 */

Panel *Panel_add_2(Panel *parent, Panel *child) {
    return Panel_add_3(parent, child, -1);
}

Panel *Panel_add_3(Panel *parent, Panel *child, int index) {
    if (!parent || !child) return nullptr;
    Element *pg = Panel_graphics(parent);
    Element *cg = Panel_graphics(child);
    if (!pg || !cg) return nullptr;
    if (index < 0) Element_add(pg, cg);
    else           Element_addAt(pg, cg, index);
    Panel_ownChild(parent, child);
    return child;
}

// A Frame owns its panels directly (the frame's content Element is the root).
Panel *Frame_add_2(Frame *frame, Panel *panel) {
    return Frame_add_3(frame, panel, -1);
}

Panel *Frame_add_3(Frame *frame, Panel *panel, int index) {
    if (!frame || !panel) return nullptr;
    if (!Frame_ownPanel(frame, panel, index)) { Panel_destroy(panel); return nullptr; }
    return panel;
}

Panel *Frame_addPanel(Frame *frame, const ElementDesc *desc) {
    if (!frame) return nullptr;
    Panel *panel = Panel_1(desc);
    if (!panel) return nullptr;
    return Frame_add_2(frame, panel);
}
