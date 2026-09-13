#ifndef DARLING_PARENTS_H
#define DARLING_PARENTS_H

#include <stdint.h>

// c23/darling_parents.h — the darling project's parent chain table.
//
// Single source of truth for class parentage (index = class number, value =
// parent class number; 0 = root; index 0 unused). Must match
// c23/darling-type.h numbering. Shared by c23/add.c (runtime registration
// via Type_registerParents) and tools/emit_manifest.c (manifest parent
// rows), so the swap contract can never disagree with the live chain.

#define DARLING_PARENT_COUNT 55u

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
    1u,   // 53 ID_SEARCHFIELD -> ID_PANEL
    1u,   // 54 ID_CODEFIELD -> ID_PANEL
};

#endif
