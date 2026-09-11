#ifndef DARLING_TYPE_H
#define DARLING_TYPE_H

#include "oop/type.h"

// darling-type.h — the darling project's type registry.
//
// OWNERSHIP: every darling class ID lives here, not in vexspoke's
// oop/type.h. vexspoke owns the layout (masks, forms, PROJ_*, ARCH_*,
// the Type_registerParents seam); each project owns its class numbers.
// Included by darling/panel/panel.h, so the whole darling tree inherits it.
//
// UNIFORM PER-PROJECT NUMBERING (uniform type rule): each project's
// registry numbers its own classes starting at 1; the project byte in the
// 64-bit type id disambiguates every repo's id #1. darling id #1
// (ID_PANEL) coexists with vexspoke id #1 (ID_INT) and graphvex id #1
// (ID_FONT) because PROJ_DARLING != PROJ_VEXSPOKE != PROJ_GRAPHVEX.
// Every TYPE_*_SINGLETON below carries PROJ_DARLING and is the ONLY form
// suited to runtime dispatch (Type_arch, Type_isA via the registered
// parent chain in c23/add.c). Bare ID_* constants name vexspoke's own
// class space, never darling's — always pass the full type id across the
// repo boundary. New darling classes take the next free number here;
// widget-local scattered id windows are a defect (they collide and break
// the central chain, e.g. the old 0x0069/0x006A scramble).

// --- DARLING UI TREE (structural subclasses, numbered 1..N) ---
#define ID_PANEL           1u
#define ID_CONTAINER       2u
#define ID_CANVAS          3u
#define ID_PICTURE         4u
#define ID_LABEL           5u
#define ID_RICH_LABEL      6u
#define ID_SCENE           7u
#define ID_SCENE2D         8u
#define ID_SCENE3D         9u

// --- DARLING CONTAINERS (panel-derived layout owners) ---
#define ID_LAYERED_CONTAINER          10u
#define ID_SECTION_CONTAINER          11u
#define ID_LIST_PANEL                 12u
#define ID_SCROLL_PANEL               13u
#define ID_GRID_PANEL                 14u
#define ID_MARKDOWN_PANEL             15u
#define ID_RICHTEXT_PANEL             16u
#define ID_EXPANDABLE_LIST_CONTAINER  17u

// --- DARLING COMPONENTS (fields + inputs + dialogs + content panels) ---
// One ID per component class; all descend from ID_PANEL (see the parent
// table in add.c, registered via Type_registerParents).
// Shell-first: struct + accessors land before behavior.
#define ID_BUTTON          18u
#define ID_SWITCH          19u
#define ID_CHECKBOX        20u
#define ID_RADIOGROUP      21u
#define ID_SLIDER          22u
#define ID_KNOB            23u
#define ID_SCROLLBAR       24u
#define ID_INPUT           25u
#define ID_TEXTAREA        26u
#define ID_INPUTOTP        27u
#define ID_SELECT          28u
#define ID_DATEPICKER      29u
#define ID_COLORPICKER     30u
#define ID_COLORSWATCH     31u
#define ID_FILEDIALOG      32u
#define ID_DIALOG          33u
#define ID_ALERTDIALOG     34u
#define ID_COLORDIALOG     35u
#define ID_KBD             36u
#define ID_PLOT            37u
#define ID_TYPOGRAPHY      38u
#define ID_RICHTEXT        39u
#define ID_ANIM            40u
#define ID_OBJECT3D        41u
#define ID_VIEWER3D        42u
#define ID_MATERIAL_PANEL  43u
#define ID_VIDEO_PANEL     44u

// --- DARLING EVENTS (transient messages, not nodes: no Panel base,
// no attach arms; parent chain registers them as roots) ---
#define ID_POINTER_EVENT  45u
#define ID_KEY_EVENT      46u
#define ID_FOCUS_EVENT    47u
#define ID_ACTION_EVENT   48u
#define ID_VALUE_EVENT    49u
#define ID_TREE_EVENT     50u
#define ID_GESTURE_EVENT  51u
#define ID_CURSOR         52u

// --- STRUCTURAL SINGLETONS (darling tree) ---
#define TYPE_PANEL_SINGLETON       (PROJ_DARLING | FORM_SINGLETON | ID_PANEL)
#define TYPE_CONTAINER_SINGLETON   (PROJ_DARLING | FORM_SINGLETON | ID_CONTAINER)
#define TYPE_CANVAS_SINGLETON      (PROJ_DARLING | FORM_SINGLETON | ID_CANVAS)
#define TYPE_PICTURE_SINGLETON     (PROJ_DARLING | FORM_SINGLETON | ID_PICTURE)
#define TYPE_LABEL_SINGLETON       (PROJ_DARLING | FORM_SINGLETON | ID_LABEL)
#define TYPE_RICH_LABEL_SINGLETON  (PROJ_DARLING | FORM_SINGLETON | ID_RICH_LABEL)
#define TYPE_SCENE_SINGLETON       (PROJ_DARLING | FORM_SINGLETON | ID_SCENE)
#define TYPE_SCENE2D_SINGLETON     (PROJ_DARLING | FORM_SINGLETON | ID_SCENE2D)
#define TYPE_SCENE3D_SINGLETON     (PROJ_DARLING | FORM_SINGLETON | ID_SCENE3D)

// --- CONTAINER SINGLETONS ---
#define TYPE_LAYERED_CONTAINER_SINGLETON       (PROJ_DARLING | FORM_SINGLETON | ID_LAYERED_CONTAINER)
#define TYPE_SECTION_CONTAINER_SINGLETON       (PROJ_DARLING | FORM_SINGLETON | ID_SECTION_CONTAINER)
#define TYPE_LIST_PANEL_SINGLETON              (PROJ_DARLING | FORM_SINGLETON | ID_LIST_PANEL)
#define TYPE_SCROLL_PANEL_SINGLETON            (PROJ_DARLING | FORM_SINGLETON | ID_SCROLL_PANEL)
#define TYPE_GRID_PANEL_SINGLETON              (PROJ_DARLING | FORM_SINGLETON | ID_GRID_PANEL)
#define TYPE_MARKDOWN_PANEL_SINGLETON          (PROJ_DARLING | FORM_SINGLETON | ID_MARKDOWN_PANEL)
#define TYPE_RICHTEXT_PANEL_SINGLETON          (PROJ_DARLING | FORM_SINGLETON | ID_RICHTEXT_PANEL)
#define TYPE_EXPANDABLE_LIST_CONTAINER_SINGLETON (PROJ_DARLING | FORM_SINGLETON | ID_EXPANDABLE_LIST_CONTAINER)

// --- COMPONENT SINGLETONS ---
#define TYPE_BUTTON_SINGLETON          (PROJ_DARLING | FORM_SINGLETON | ID_BUTTON)
#define TYPE_SWITCH_SINGLETON          (PROJ_DARLING | FORM_SINGLETON | ID_SWITCH)
#define TYPE_CHECKBOX_SINGLETON        (PROJ_DARLING | FORM_SINGLETON | ID_CHECKBOX)
#define TYPE_RADIOGROUP_SINGLETON      (PROJ_DARLING | FORM_SINGLETON | ID_RADIOGROUP)
#define TYPE_SLIDER_SINGLETON          (PROJ_DARLING | FORM_SINGLETON | ID_SLIDER)
#define TYPE_KNOB_SINGLETON            (PROJ_DARLING | FORM_SINGLETON | ID_KNOB)
#define TYPE_SCROLLBAR_SINGLETON       (PROJ_DARLING | FORM_SINGLETON | ID_SCROLLBAR)
#define TYPE_INPUT_SINGLETON           (PROJ_DARLING | FORM_SINGLETON | ID_INPUT)
#define TYPE_TEXTAREA_SINGLETON        (PROJ_DARLING | FORM_SINGLETON | ID_TEXTAREA)
#define TYPE_INPUTOTP_SINGLETON        (PROJ_DARLING | FORM_SINGLETON | ID_INPUTOTP)
#define TYPE_SELECT_SINGLETON          (PROJ_DARLING | FORM_SINGLETON | ID_SELECT)
#define TYPE_DATEPICKER_SINGLETON      (PROJ_DARLING | FORM_SINGLETON | ID_DATEPICKER)
#define TYPE_COLORPICKER_SINGLETON     (PROJ_DARLING | FORM_SINGLETON | ID_COLORPICKER)
#define TYPE_COLORSWATCH_SINGLETON     (PROJ_DARLING | FORM_SINGLETON | ID_COLORSWATCH)
#define TYPE_FILEDIALOG_SINGLETON      (PROJ_DARLING | FORM_SINGLETON | ID_FILEDIALOG)
#define TYPE_DIALOG_SINGLETON          (PROJ_DARLING | FORM_SINGLETON | ID_DIALOG)
#define TYPE_ALERTDIALOG_SINGLETON     (PROJ_DARLING | FORM_SINGLETON | ID_ALERTDIALOG)
#define TYPE_COLORDIALOG_SINGLETON     (PROJ_DARLING | FORM_SINGLETON | ID_COLORDIALOG)
#define TYPE_KBD_SINGLETON             (PROJ_DARLING | FORM_SINGLETON | ID_KBD)
#define TYPE_PLOT_SINGLETON            (PROJ_DARLING | FORM_SINGLETON | ID_PLOT)
#define TYPE_TYPOGRAPHY_SINGLETON      (PROJ_DARLING | FORM_SINGLETON | ID_TYPOGRAPHY)
#define TYPE_RICHTEXT_SINGLETON        (PROJ_DARLING | FORM_SINGLETON | ID_RICHTEXT)
#define TYPE_ANIM_SINGLETON            (PROJ_DARLING | FORM_SINGLETON | ID_ANIM)
#define TYPE_OBJECT3D_SINGLETON        (PROJ_DARLING | FORM_SINGLETON | ID_OBJECT3D)
#define TYPE_VIEWER3D_SINGLETON        (PROJ_DARLING | FORM_SINGLETON | ID_VIEWER3D)
#define TYPE_MATERIAL_PANEL_SINGLETON  (PROJ_DARLING | FORM_SINGLETON | ID_MATERIAL_PANEL)
#define TYPE_VIDEO_PANEL_SINGLETON     (PROJ_DARLING | FORM_SINGLETON | ID_VIDEO_PANEL)

// --- EVENT SINGLETONS (transient; no Panel base, never attached) ---
#define TYPE_POINTER_EVENT_SINGLETON  (PROJ_DARLING | FORM_SINGLETON | ID_POINTER_EVENT)
#define TYPE_KEY_EVENT_SINGLETON      (PROJ_DARLING | FORM_SINGLETON | ID_KEY_EVENT)
#define TYPE_FOCUS_EVENT_SINGLETON    (PROJ_DARLING | FORM_SINGLETON | ID_FOCUS_EVENT)
#define TYPE_ACTION_EVENT_SINGLETON   (PROJ_DARLING | FORM_SINGLETON | ID_ACTION_EVENT)
#define TYPE_VALUE_EVENT_SINGLETON    (PROJ_DARLING | FORM_SINGLETON | ID_VALUE_EVENT)
#define TYPE_TREE_EVENT_SINGLETON     (PROJ_DARLING | FORM_SINGLETON | ID_TREE_EVENT)
#define TYPE_GESTURE_EVENT_SINGLETON  (PROJ_DARLING | FORM_SINGLETON | ID_GESTURE_EVENT)
#define TYPE_CURSOR_SINGLETON         (PROJ_DARLING | FORM_SINGLETON | ID_CURSOR)

#endif