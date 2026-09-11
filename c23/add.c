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
 *   kDarlingParents[DARLING_PARENT_COUNT] — parent table for vexspoke:
 *     parents[i] = parent class NUMBER of class # i, 0 = root; mirrors the
 *     legacy hardcoded chain (panel family -> Panel, scene2d/3d -> Scene,
 *     dialogs -> Dialog, richtext/canvas/container roots) at the new
 *     1..N per-project numbers. Main class has no fields here.
 *   typesRegistered / registerTypes() — one-shot seam grant before the
 *     first Type_isA walk in Darling_addAny.
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

// Per-project parent table (index = class number, value = parent class
// number; 0 = root). Index 0 is unused. Must match c23/darling-type.h.
#define DARLING_PARENT_COUNT 53u
static const uint32_t kDarlingParents[DARLING_PARENT_COUNT] = {
    0u,   //  0 unused
    2u,   //  1 ID_PANEL -> ID_CONTAINER
    0u,   //  2 ID_CONTAINER (root)
    0u,   //  3 ID_CANVAS (root)
    1u,   //  4 ID_PICTURE
    1u,   //  5 ID_LABEL
    1u,   //  6 ID_RICH_LABEL
    1u,   //  7 ID_SCENE
    7u,   //  8 ID_SCENE2D -> ID_SCENE
    7u,   //  9 ID_SCENE3D -> ID_SCENE
    1u,   // 10 ID_LAYERED_CONTAINER
    1u,   // 11 ID_SECTION_CONTAINER
    1u,   // 12 ID_LIST_PANEL
    1u,   // 13 ID_SCROLL_PANEL
    1u,   // 14 ID_GRID_PANEL
    1u,   // 15 ID_MARKDOWN_PANEL
    1u,   // 16 ID_RICHTEXT_PANEL
    1u,   // 17 ID_EXPANDABLE_LIST_CONTAINER
    1u,   // 18 ID_BUTTON
    1u,   // 19 ID_SWITCH
    1u,   // 20 ID_CHECKBOX
    1u,   // 21 ID_RADIOGROUP
    1u,   // 22 ID_SLIDER
    1u,   // 23 ID_KNOB
    1u,   // 24 ID_SCROLLBAR
    1u,   // 25 ID_INPUT
    1u,   // 26 ID_TEXTAREA
    1u,   // 27 ID_INPUTOTP
    1u,   // 28 ID_SELECT
    1u,   // 29 ID_DATEPICKER
    1u,   // 30 ID_COLORPICKER
    1u,   // 31 ID_COLORSWATCH
    1u,   // 32 ID_FILEDIALOG
    1u,   // 33 ID_DIALOG
    33u,  // 34 ID_ALERTDIALOG -> ID_DIALOG
    33u,  // 35 ID_COLORDIALOG -> ID_DIALOG
    1u,   // 36 ID_KBD
    1u,   // 37 ID_PLOT
    1u,   // 38 ID_TYPOGRAPHY
    0u,   // 39 ID_RICHTEXT (root, text engine object, not a node)
    1u,   // 40 ID_ANIM
    1u,   // 41 ID_OBJECT3D
    1u,   // 42 ID_VIEWER3D
    1u,   // 43 ID_MATERIAL_PANEL
    1u,   // 44 ID_VIDEO_PANEL
    1u,   // 45 ID_POINTER_EVENT
    1u,   // 46 ID_KEY_EVENT
    1u,   // 47 ID_FOCUS_EVENT
    1u,   // 48 ID_ACTION_EVENT
    1u,   // 49 ID_VALUE_EVENT
    1u,   // 50 ID_TREE_EVENT
    1u,   // 51 ID_GESTURE_EVENT
    1u,   // 52 ID_CURSOR
};

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
    if (cls == ID_CANVAS)
        return "Canvas";
    if (cls == ID_LAYERED_CONTAINER)
        return "LayeredContainer";
    if (cls == ID_SECTION_CONTAINER)
        return "SectionContainer";
    if (cls == ID_EXPANDABLE_LIST_CONTAINER)
        return "ExpandableListContainer";
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
