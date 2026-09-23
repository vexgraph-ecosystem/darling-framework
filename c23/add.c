#include "c23/add.h"

#include "annotation/definition.h"
#include "annotation/overview.h"
#include "c23/darling_parents.h"
#include "darling/panel/panel.h"
#include "oop/type.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Add
 * ============================================================================
 * Unified attach dispatch for the darling tree: the compile-time _Generic in
 * c23/add.h picks the caster by child pointer type, this file validates by
 * class id at runtime, and every attach funnels through one static
 * addContainer() wrapping Panel_addContainer — no new tree logic. Also hosts
 * the darling-side type helpers Darling_classOf/kindName, which map to
 * vexspoke's registry without forking it. Operates on the Panel tree only
 * (zero struct fields, zero allocation); the parent chain is granted to
 * vexspoke once at first attach via Type_registerParents so Type_isA walks
 * stay project-aware. Sits at the R4 attach seam between Panel-derived
 * widgets and the R2 type registry.
 * ============================================================================
 */

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
 * Per-project type ids (uniform type rule): adders pass full
 * TYPE_*_SINGLETON ids (PROJ_DARLING | FORM_SINGLETON | class number), so
 * Type_arch and Type_isA resolve in darling's own registry. The parent
 * chain is granted to vexspoke once at first attach via
 * Type_registerParents(PROJ_DARLING, kDarlingParents, ...) — the central
 * walk stays project-aware without vexspoke ever including a downstream
 * header (Rule 17).
 *
 * STRUCT FIELDS: none — procedural dispatch shim (operates on Panel tree).
 *
 * PRIVATE HELPERS:
 * ----------------------------------------------------------------------------
 *   typesRegistered / registerTypes() — one-shot seam grant before the
 *     first Type_isA walk in Darling_addAny. The parent table itself lives
 *     in c23/darling_parents.h (shared with the manifest emitter).
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

// Parent chain: single source in c23/darling_parents.h (shared with the
// manifest emitter so the swap contract can never disagree with the live
// chain). Index 0 is unused.

static bool typesRegistered;

static void registerTypes(void) {
    if (typesRegistered)
        return;
    Type_registerParents(PROJ_DARLING, kDarlingParents, DARLING_PARENT_COUNT);
    typesRegistered = true;
}

// Single attach funnel: the only mutation point for Darling_add.
static void addContainer(Panel *parent, Panel *child) {
    Panel_addContainer(parent, child);
}

void Darling_addAny(Panel *parent, void *child, uint64_t childClass) {
    if (!parent)
        return;
    if (!child)
        return;
    registerTypes();
    // Architecture gate: darling attach accepts darling nodes only.
    if (Type_arch(childClass) != ARCH_DARLING)
        return;
    // Container/Canvas are darling-arch but not attachable nodes.
    uint64_t cls = Type_class(childClass);
    if (cls == ID_CONTAINER)
        return;
    if (cls == ID_CANVAS)
        return;
    // Panel family: Panel itself + everything deriving from it.
    if (cls == ID_PANEL)
        addContainer(parent, (Panel*) child);
    else if (Type_isA(childClass, ID_PANEL))
        addContainer(parent, (Panel*) child);
}

const char *Darling_kindName(uint64_t classId) {
    uint64_t cls = Type_class(classId);
    if (cls == ID_PANEL)
        return "Panel";
    if (cls == ID_LABEL)
        return "Label";
    if (cls == ID_PICTURE)
        return "Picture";
    if (cls == ID_RICH_LABEL)
        return "RichLabel";
    if (cls == ID_SCENE)
        return "Scene";
    if (cls == ID_SCENE2D)
        return "Scene2D";
    if (cls == ID_SCENE3D)
        return "Scene3D";
    if (cls == ID_CONTAINER)
        return "Container";
    if (cls == ID_COMPONENT)
        return "Component";
    if (cls == ID_CANVAS)
        return "Canvas";
    if (cls == ID_LAYERED_CONTAINER)
        return "LayeredContainer";
    if (cls == ID_SECTION_CONTAINER)
        return "SectionContainer";
    if (cls == ID_EXPANDABLE_LIST_CONTAINER)
        return "ExpandableListContainer";
    if (cls == ID_FLEX_CONTAINER)
        return "FlexContainer";
    if (cls == ID_BUTTON)
        return "Button";
    if (cls == ID_SWITCH)
        return "Switch";
    if (cls == ID_CHECKBOX)
        return "Checkbox";
    if (cls == ID_RADIOGROUP)
        return "RadioGroup";
    if (cls == ID_SLIDER)
        return "Slider";
    if (cls == ID_KNOB)
        return "Knob";
    if (cls == ID_INPUT)
        return "Input";
    if (cls == ID_TEXTAREA)
        return "Textarea";
    if (cls == ID_INPUTOTP)
        return "InputOTP";
    if (cls == ID_SELECT)
        return "Select";
    if (cls == ID_DATEPICKER)
        return "DatePicker";
    if (cls == ID_COLORPICKER)
        return "ColorPicker";
    if (cls == ID_COLORSWATCH)
        return "ColorSwatch";
    if (cls == ID_FILEDIALOG)
        return "FileDialog";
    if (cls == ID_TYPOGRAPHY)
        return "Typography";
    if (cls == ID_KBD)
        return "Kbd";
    if (cls == ID_PLOT)
        return "Plot";
    if (cls == ID_DIALOG)
        return "Dialog";
    if (cls == ID_ALERTDIALOG)
        return "AlertDialog";
    if (cls == ID_COLORDIALOG)
        return "ColorDialog";
    if (cls == ID_POINTER_EVENT)
        return "PointerEvent";
    if (cls == ID_KEY_EVENT)
        return "UIKeyEvent";
    if (cls == ID_FOCUS_EVENT)
        return "FocusEvent";
    if (cls == ID_ACTION_EVENT)
        return "ActionEvent";
    if (cls == ID_VALUE_EVENT)
        return "ValueEvent";
    if (cls == ID_TREE_EVENT)
        return "TreeEvent";
    if (cls == ID_GESTURE_EVENT)
        return "GestureEvent";
    return "Unknown";
}
