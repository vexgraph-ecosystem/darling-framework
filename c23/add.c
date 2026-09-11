#include "c23/add.h"

#include "annotation/overview.h"
#include "darling/panel/panel.h"
#include "oop/type.h"

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Add (darling/add.c)
 * LEVEL: L2 — Behavior (unified attach dispatch for the darling tree)
 * ============================================================================
 * One add() for every Panel-derived node: the compile-time _Generic in
 * c23/add.h picks the caster by child pointer type, this file validates by
 * class id at runtime and funnels everything through one static
 * addContainer(). Thin wrapper over Panel_addContainer — no new tree logic.
 * Darling_classOf/kindName live here too: darling-side type helpers that
 * map to vexspoke's registry without forking it.
 *
 * STRUCT FIELDS: none — procedural dispatch shim (operates on Panel tree).
 *
 * PRIVATE HELPERS:
 * ----------------------------------------------------------------------------
 *   addContainer(parent, child) — single attach funnel wrapping
 *     Panel_addContainer; the only place the tree is mutated.
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - Darling_addAny(parent, child, childClass)
 *
 * Getters:
 *   - Darling_kindName(classId)
 * ============================================================================
 */

// Single attach funnel: the only mutation point for Darling_add.
static void addContainer(Panel *parent, Panel *child) {
    Panel_addContainer(parent, child);
}

void Darling_addAny(Panel *parent, void *child, uint32_t childClass) {
    if (!parent)
        return;
    if (!child)
        return;
    // Architecture gate: darling attach accepts darling nodes only.
    if (Type_arch(childClass) != ARCH_DARLING)
        return;
    // Container/Canvas are darling-arch but not attachable nodes.
    if (childClass == ID_CONTAINER)
        return;
    if (childClass == ID_CANVAS)
        return;
    // Panel family: Panel itself + everything deriving from it.
    if (childClass == ID_PANEL)
        addContainer(parent, (Panel*) child);
    else if (Type_isA(childClass, ID_PANEL))
        addContainer(parent, (Panel*) child);
}

const char *Darling_kindName(uint32_t classId) {
    if (classId == ID_PANEL)
        return "Panel";
    if (classId == ID_LABEL)
        return "Label";
    if (classId == ID_PICTURE)
        return "Picture";
    if (classId == ID_RICH_LABEL)
        return "RichLabel";
    if (classId == ID_SCENE)
        return "Scene";
    if (classId == ID_SCENE2D)
        return "Scene2D";
    if (classId == ID_SCENE3D)
        return "Scene3D";
    if (classId == ID_CONTAINER)
        return "Container";
    if (classId == ID_CANVAS)
        return "Canvas";
    if (classId == ID_LAYERED_CONTAINER)
        return "LayeredContainer";
    if (classId == ID_SECTION_CONTAINER)
        return "SectionContainer";
    if (classId == ID_EXPANDABLE_LIST_CONTAINER)
        return "ExpandableListContainer";
    if (classId == ID_BUTTON)
        return "Button";
    if (classId == ID_SWITCH)
        return "Switch";
    if (classId == ID_CHECKBOX)
        return "Checkbox";
    if (classId == ID_RADIOGROUP)
        return "RadioGroup";
    if (classId == ID_SLIDER)
        return "Slider";
    if (classId == ID_KNOB)
        return "Knob";
    if (classId == ID_INPUT)
        return "Input";
    if (classId == ID_TEXTAREA)
        return "Textarea";
    if (classId == ID_INPUTOTP)
        return "InputOTP";
    if (classId == ID_SELECT)
        return "Select";
    if (classId == ID_DATEPICKER)
        return "DatePicker";
    if (classId == ID_COLORPICKER)
        return "ColorPicker";
    if (classId == ID_COLORSWATCH)
        return "ColorSwatch";
    if (classId == ID_FILEDIALOG)
        return "FileDialog";
    if (classId == ID_TYPOGRAPHY)
        return "Typography";
    if (classId == ID_KBD)
        return "Kbd";
    if (classId == ID_PLOT)
        return "Plot";
    if (classId == ID_DIALOG)
        return "Dialog";
    if (classId == ID_ALERTDIALOG)
        return "AlertDialog";
    if (classId == ID_COLORDIALOG)
        return "ColorDialog";
    if (classId == ID_POINTER_EVENT)
        return "PointerEvent";
    if (classId == ID_KEY_EVENT)
        return "UIKeyEvent";
    if (classId == ID_FOCUS_EVENT)
        return "FocusEvent";
    if (classId == ID_ACTION_EVENT)
        return "ActionEvent";
    if (classId == ID_VALUE_EVENT)
        return "ValueEvent";
    if (classId == ID_TREE_EVENT)
        return "TreeEvent";
    if (classId == ID_GESTURE_EVENT)
        return "GestureEvent";
    return "Unknown";
}
