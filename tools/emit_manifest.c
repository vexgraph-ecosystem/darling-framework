#include "annotation/definition.h"
#include "annotation/overview.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "c23/darling-type.h"
#include "c23/darling_parents.h"
#include "darling/component.h"
#include "darling/container.h"
#include "darling/frame.h"
#include "darling/panel/panel.h"
#include "darling/scene/canvas.h"
#include "darling/picture/picture.h"
#include "darling/label/label.h"
#include "darling/label/rich_label.h"
#include "darling/scene/scene.h"
#include "darling/panel/layered_container.h"
#include "darling/panel/section_container.h"
#include "darling/panel/list_container.h"
#include "darling/panel/scroll_container.h"
#include "darling/panel/grid_container.h"
#include "darling/panel/markdown_panel.h"
#include "darling/panel/richtext_panel.h"
#include "darling/panel/expandable_list_container.h"
#include "darling/panel/flex_container.h"
#include "darling/panel/scroll_panel.h"
#include "darling/button/button.h"
#include "darling/button/switch.h"
#include "darling/field/checkbox.h"
#include "darling/field/radiogroup.h"
#include "darling/field/slider.h"
#include "darling/field/knob.h"
#include "darling/field/scrollbar.h"
#include "darling/field/input.h"
#include "darling/field/textarea.h"
#include "darling/field/inputotp.h"
#include "darling/field/searchfield.h"
#include "darling/field/codefield.h"
#include "darling/field/select.h"
#include "darling/field/datepicker.h"
#include "darling/field/colorpicker.h"
#include "darling/field/colorswatch.h"
#include "darling/overlay/filedialog.h"
#include "darling/dialog/dialog.h"
#include "darling/dialog/alertdialog.h"
#include "darling/color/colordialog.h"
#include "darling/label/kbd.h"
#include "darling/plot/plot.h"
#include "darling/label/typography.h"
#include "text/rich_text.h"
#include "darling/anim/anim.h"
#include "darling/scene/object3d.h"
#include "darling/scene/viewer3d.h"
#include "darling/panel/material_panel.h"
#include "darling/picture/video_panel.h"
#include "event/pointer.h"
#include "event/keyevent.h"
#include "event/focus.h"
#include "event/action.h"
#include "event/value.h"
#include "event/tree.h"
#include "event/gesture.h"
#include "darling/cursor/cursor.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: EmitManifest
 * ============================================================================
 * Build-time tool (never linked into libdarling): prints one complete module
 * manifest JSON whose type_ids rows state every darling class as
 * {name, value, parent, size} — name/value from c23/darling-type.h macros,
 * parent from the shared c23/darling_parents.h chain (the same array
 * c23/add.c registers, so the swap contract can never disagree with the live
 * chain), size from sizeof(struct) (compiler-computed, cannot drift). Output
 * feeds Hot_manifest JSONs and parses with hotcwap's HotManifest_parse; every
 * stated row activates the swap gate. The EmitRow table is the single
 * extension point: new classes extend the registry, the parent table, AND
 * this table together — the _Static_assert turns a forgotten row into a
 * compile error instead of silent contract drift.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: EmitManifest (tools/emit_manifest.c — build-time manifest emitter)
 * LEVEL: L1 — File Metadata (declarative contract output, never linked in)
 * ============================================================================
 * Build-time tool (never linked into libdarling): prints one complete module
 * manifest JSON whose type_ids rows state every darling class as
 * {name, value, parent, size} — name/value from c23/darling-type.h macros,
 * parent from the shared c23/darling_parents.h chain (the same array
 * c23/add.c registers, so the swap contract can never disagree with the
 * live chain), size from sizeof(struct) (compiler-computed, cannot drift).
 * Output feeds Hot_manifest JSONs and parses with hotcwap's
 * HotManifest_parse; every stated row activates the swap gate.
 *
 * Usage: emit_darling_manifest [name] [version]  (defaults: darling 0.0.0)
 *
 * STRUCT FIELDS: none — procedural tool (no owned struct).
 *
 * PRIVATE HELPERS (kept file-local pure-data only, each with full fields):
 * ----------------------------------------------------------------------------
 *   EmitRow name;       // const char * + row name, e.g. "ID_LABEL"
 *   EmitRow value;      // uint64_t + full 64-bit TYPE_*_SINGLETON id
 *   EmitRow classNo;    // uint32_t + registry class number (parent lookup)
 *   EmitRow size;       // size_t + sizeof(struct), compiler-computed
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - main(argc, argv)
 * ============================================================================
 */

// One manifest row per registered class. New classes extend the registry,
// the parent table, AND this table together — the _Static_assert below turns
// a forgotten row into a compile error instead of silent contract drift.
typedef struct EmitRow {
    const char *name;
    uint64_t value;
    uint32_t classNo;
    size_t size;
} EmitRow;

static const EmitRow kRows[] = {
    { "ID_PANEL", (uint64_t) TYPE_PANEL_SINGLETON, ID_PANEL, sizeof(Panel) },
    { "ID_CONTAINER", (uint64_t) TYPE_CONTAINER_SINGLETON, ID_CONTAINER, sizeof(Container) },
    { "ID_CANVAS", (uint64_t) TYPE_CANVAS_SINGLETON, ID_CANVAS, sizeof(Canvas) },
    { "ID_PICTURE", (uint64_t) TYPE_PICTURE_SINGLETON, ID_PICTURE, sizeof(Picture) },
    { "ID_LABEL", (uint64_t) TYPE_LABEL_SINGLETON, ID_LABEL, sizeof(Label) },
    { "ID_RICH_LABEL", (uint64_t) TYPE_RICH_LABEL_SINGLETON, ID_RICH_LABEL, sizeof(RichLabel) },
    { "ID_SCENE", (uint64_t) TYPE_SCENE_SINGLETON, ID_SCENE, sizeof(Scene) },
    { "ID_SCENE2D", (uint64_t) TYPE_SCENE2D_SINGLETON, ID_SCENE2D, sizeof(Scene2D) },
    { "ID_SCENE3D", (uint64_t) TYPE_SCENE3D_SINGLETON, ID_SCENE3D, sizeof(Scene3D) },
    { "ID_LAYERED_CONTAINER", (uint64_t) TYPE_LAYERED_CONTAINER_SINGLETON, ID_LAYERED_CONTAINER, sizeof(LayeredContainer) },
    { "ID_SECTION_CONTAINER", (uint64_t) TYPE_SECTION_CONTAINER_SINGLETON, ID_SECTION_CONTAINER, sizeof(SectionContainer) },
    { "ID_LIST_PANEL", (uint64_t) TYPE_LIST_PANEL_SINGLETON, ID_LIST_PANEL, sizeof(ListContainer) },
    { "ID_SCROLL_PANEL", (uint64_t) TYPE_SCROLL_PANEL_SINGLETON, ID_SCROLL_PANEL, sizeof(ScrollContainer) },
    { "ID_GRID_PANEL", (uint64_t) TYPE_GRID_PANEL_SINGLETON, ID_GRID_PANEL, sizeof(GridContainer) },
    { "ID_MARKDOWN_PANEL", (uint64_t) TYPE_MARKDOWN_PANEL_SINGLETON, ID_MARKDOWN_PANEL, sizeof(MarkdownPanel) },
    { "ID_RICHTEXT_PANEL", (uint64_t) TYPE_RICHTEXT_PANEL_SINGLETON, ID_RICHTEXT_PANEL, sizeof(RichTextPanel) },
    { "ID_EXPANDABLE_LIST_CONTAINER", (uint64_t) TYPE_EXPANDABLE_LIST_CONTAINER_SINGLETON, ID_EXPANDABLE_LIST_CONTAINER, sizeof(ExpandableListContainer) },
    { "ID_BUTTON", (uint64_t) TYPE_BUTTON_SINGLETON, ID_BUTTON, sizeof(Button) },
    { "ID_SWITCH", (uint64_t) TYPE_SWITCH_SINGLETON, ID_SWITCH, sizeof(Switch) },
    { "ID_CHECKBOX", (uint64_t) TYPE_CHECKBOX_SINGLETON, ID_CHECKBOX, sizeof(Checkbox) },
    { "ID_RADIOGROUP", (uint64_t) TYPE_RADIOGROUP_SINGLETON, ID_RADIOGROUP, sizeof(RadioGroup) },
    { "ID_SLIDER", (uint64_t) TYPE_SLIDER_SINGLETON, ID_SLIDER, sizeof(Slider) },
    { "ID_KNOB", (uint64_t) TYPE_KNOB_SINGLETON, ID_KNOB, sizeof(Knob) },
    { "ID_SCROLLBAR", (uint64_t) TYPE_SCROLLBAR_SINGLETON, ID_SCROLLBAR, sizeof(ScrollBar) },
    { "ID_INPUT", (uint64_t) TYPE_INPUT_SINGLETON, ID_INPUT, sizeof(Input) },
    { "ID_TEXTAREA", (uint64_t) TYPE_TEXTAREA_SINGLETON, ID_TEXTAREA, sizeof(Textarea) },
    { "ID_INPUTOTP", (uint64_t) TYPE_INPUTOTP_SINGLETON, ID_INPUTOTP, sizeof(InputOTP) },
    { "ID_SELECT", (uint64_t) TYPE_SELECT_SINGLETON, ID_SELECT, sizeof(Select) },
    { "ID_DATEPICKER", (uint64_t) TYPE_DATEPICKER_SINGLETON, ID_DATEPICKER, sizeof(DatePicker) },
    { "ID_COLORPICKER", (uint64_t) TYPE_COLORPICKER_SINGLETON, ID_COLORPICKER, sizeof(ColorPicker) },
    { "ID_COLORSWATCH", (uint64_t) TYPE_COLORSWATCH_SINGLETON, ID_COLORSWATCH, sizeof(ColorSwatch) },
    { "ID_FILEDIALOG", (uint64_t) TYPE_FILEDIALOG_SINGLETON, ID_FILEDIALOG, sizeof(FileDialog) },
    { "ID_DIALOG", (uint64_t) TYPE_DIALOG_SINGLETON, ID_DIALOG, sizeof(Dialog) },
    { "ID_ALERTDIALOG", (uint64_t) TYPE_ALERTDIALOG_SINGLETON, ID_ALERTDIALOG, sizeof(AlertDialog) },
    { "ID_COLORDIALOG", (uint64_t) TYPE_COLORDIALOG_SINGLETON, ID_COLORDIALOG, sizeof(ColorDialog) },
    { "ID_KBD", (uint64_t) TYPE_KBD_SINGLETON, ID_KBD, sizeof(Kbd) },
    { "ID_PLOT", (uint64_t) TYPE_PLOT_SINGLETON, ID_PLOT, sizeof(Plot) },
    { "ID_TYPOGRAPHY", (uint64_t) TYPE_TYPOGRAPHY_SINGLETON, ID_TYPOGRAPHY, sizeof(Typography) },
    { "ID_RICHTEXT", (uint64_t) TYPE_RICHTEXT_SINGLETON, ID_RICHTEXT, sizeof(RichText) },
    { "ID_ANIM", (uint64_t) TYPE_ANIM_SINGLETON, ID_ANIM, sizeof(Anim) },
    { "ID_OBJECT3D", (uint64_t) TYPE_OBJECT3D_SINGLETON, ID_OBJECT3D, sizeof(Object3D) },
    { "ID_VIEWER3D", (uint64_t) TYPE_VIEWER3D_SINGLETON, ID_VIEWER3D, sizeof(Viewer3D) },
    { "ID_MATERIAL_PANEL", (uint64_t) TYPE_MATERIAL_PANEL_SINGLETON, ID_MATERIAL_PANEL, sizeof(MaterialPanel) },
    { "ID_VIDEO_PANEL", (uint64_t) TYPE_VIDEO_PANEL_SINGLETON, ID_VIDEO_PANEL, sizeof(VideoPanel) },
    { "ID_POINTER_EVENT", (uint64_t) TYPE_POINTER_EVENT_SINGLETON, ID_POINTER_EVENT, sizeof(PointerEvent) },
    { "ID_KEY_EVENT", (uint64_t) TYPE_KEY_EVENT_SINGLETON, ID_KEY_EVENT, sizeof(UIKeyEvent) },
    { "ID_FOCUS_EVENT", (uint64_t) TYPE_FOCUS_EVENT_SINGLETON, ID_FOCUS_EVENT, sizeof(FocusEvent) },
    { "ID_ACTION_EVENT", (uint64_t) TYPE_ACTION_EVENT_SINGLETON, ID_ACTION_EVENT, sizeof(ActionEvent) },
    { "ID_VALUE_EVENT", (uint64_t) TYPE_VALUE_EVENT_SINGLETON, ID_VALUE_EVENT, sizeof(ValueEvent) },
    { "ID_TREE_EVENT", (uint64_t) TYPE_TREE_EVENT_SINGLETON, ID_TREE_EVENT, sizeof(TreeEvent) },
    { "ID_GESTURE_EVENT", (uint64_t) TYPE_GESTURE_EVENT_SINGLETON, ID_GESTURE_EVENT, sizeof(GestureEvent) },
    { "ID_CURSOR", (uint64_t) TYPE_CURSOR_SINGLETON, ID_CURSOR, sizeof(Cursor) },
    { "ID_SEARCHFIELD", (uint64_t) TYPE_SEARCHFIELD_SINGLETON, ID_SEARCHFIELD, sizeof(SearchField) },
    { "ID_CODEFIELD", (uint64_t) TYPE_CODEFIELD_SINGLETON, ID_CODEFIELD, sizeof(CodeField) },
    { "ID_FRAME_FUNCTION", (uint64_t) TYPE_FRAME_FUNCTION_ARRAY, ID_FRAME_FUNCTION, sizeof(FrameFunction) },
    { "ID_COMPONENT", (uint64_t) TYPE_COMPONENT_SINGLETON, ID_COMPONENT, sizeof(Component) },
    { "ID_SCROLLPANEL", (uint64_t) TYPE_SCROLLPANEL_SINGLETON, ID_SCROLLPANEL, sizeof(ScrollPanel) },
    { "ID_FLEX_CONTAINER", (uint64_t) TYPE_FLEX_CONTAINER_SINGLETON, ID_FLEX_CONTAINER, sizeof(FlexContainer) },
};

_Static_assert(sizeof(kRows) / sizeof(kRows[0]) == DARLING_PARENT_COUNT - 1u,
    "emit table must cover every registered class");

static bool arg_clean(const char *s) {
    if (!s || s[0] == '\0')
        return false;
    for (size_t i = 0; s[i] != '\0'; i++) {
        unsigned char c = (unsigned char) s[i];
        if (c == '"' || c == '\\' || c < 0x20)
            return false;
    }
    return true;
}

int main(int argc, char **argv) {
    const char *name = argc > 1 ? argv[1] : "darling";
    const char *version = argc > 2 ? argv[2] : "0.0.0";
    if (!arg_clean(name) || !arg_clean(version)) {
        printf("usage: emit_darling_manifest [name] [version]\n");
        return 1;
    }
    size_t rowCount = sizeof(kRows) / sizeof(kRows[0]);
    printf("{\"name\": \"%s\", \"version\": \"%s\", \"type_ids\": [", name, version);
    for (size_t i = 0; i < rowCount; i++) {
        uint32_t cls = kRows[i].classNo;
        uint32_t parent = cls < DARLING_PARENT_COUNT ? kDarlingParents[cls] : 0;
        printf("%s{\"name\": \"%s\", \"value\": %llu, \"parent\": %u, \"size\": %zu}",
            i == 0 ? "" : ", ",
            kRows[i].name,
            (unsigned long long) kRows[i].value,
            parent,
            kRows[i].size);
    }
    printf("], \"exports\": [], \"dependencies\": []}\n");
    return 0;
}
