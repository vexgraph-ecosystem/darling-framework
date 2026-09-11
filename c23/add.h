#ifndef DARLING_ADD_H
#define DARLING_ADD_H

#include <stdint.h>

#include "c23/overload.h"
#include "darling/container.h"
#include "darling/label/label.h"
#include "darling/panel/panel.h"
#include "darling/picture/picture.h"
#include "darling/label/rich_label.h"
#include "darling/scene/scene.h"
#include "darling/panel/layered_container.h"
#include "darling/panel/section_container.h"
#include "darling/panel/expandable_list_container.h"
#include "darling/button/button.h"
#include "darling/button/switch.h"
#include "darling/field/checkbox.h"
#include "darling/field/radiogroup.h"
#include "darling/field/slider.h"
#include "darling/field/knob.h"
#include "darling/field/input.h"
#include "darling/field/textarea.h"
#include "darling/field/inputotp.h"
#include "darling/field/select.h"
#include "darling/field/datepicker.h"
#include "darling/field/colorpicker.h"
#include "darling/field/colorswatch.h"
#include "darling/overlay/filedialog.h"
#include "darling/label/typography.h"
#include "darling/label/kbd.h"
#include "darling/plot/plot.h"
#include "darling/dialog/dialog.h"
#include "darling/dialog/alertdialog.h"
#include "darling/color/colordialog.h"

// Forward only: Darling_classOf/Darling_add mention Canvas * in _Generic
// lists but never dereference it — full canvas.h stays out, oop/type.h IDs
// arrive via c23/darling-type.h. Keeps this header decoupled per lines 38-41.
typedef struct Canvas Canvas;

// darling/c23/add.h — unified add() for the darling tree (thin wrapper).
// Lives in darling's c23/ (this folder), included as "c23/add.h".
// Filename must NEVER collide with vexspoke's c23/*.h (constructor.h,
// overload.h, ...): both roots sit on the include path, so a duplicate
// name would shadow upstream by -I order. Same reason there is no
// darling oop/type.h — the registry stays single in vexspoke.
//
// Reuses vexspoke's c23/overload.h (ov_type_error for loud compile errors;
// math add/sub/mul/div untouched) and adds Panel-family dispatch on top:
// one name accepts every Panel-derived node as parent or child.
// Container/Canvas hit ov_type_error at compile time: a Container is
// layout data (no Panel to attach), a Canvas is a projection (not a node).
//
// Two layers, matching the codebase split:
//   compile time (_Generic here): picks the caster by child pointer TYPE.
//   runtime (add.c Darling_addAny): validates by class ID with if-cases on
//     Type_arch + Type_isA, then routes to a single static addContainer().
// Base-pointer calls (Panel *child holding a Label) skip the generic and go
// straight to Darling_addAny with an explicit ID — same if-cases apply.
//
// Attach semantics (Java add()): reparents the live child via
// Panel_addContainer. Deep-copy stays Panel_add(parent, node) — unchanged.

// Runtime attach with explicit class id (null-safe no-op on mismatch).
// childClass is a FULL type id (TYPE_*_SINGLETON: PROJ_DARLING | FORM |
// class) so Type_arch/Type_isA resolve in darling's own registry — bare
// ID_* numbers mean vexspoke's class space, never darling's.
void Darling_addAny(Panel *parent, void *child, uint64_t childClass);

// --- parent normalization (any Panel-derived pointer -> Panel *) ------------
static inline Panel *Darling_asPanel_Panel(Panel *p) { return p; }
static inline Panel *Darling_asPanel_Label(Label *p) { return (Panel*) (void*) p; }
static inline Panel *Darling_asPanel_Picture(Picture *p) { return (Panel*) (void*) p; }
static inline Panel *Darling_asPanel_RichLabel(RichLabel *p) { return (Panel*) (void*) p; }
static inline Panel *Darling_asPanel_Scene(Scene *s) { return &(*s).base; }

static inline Panel *Darling_asPanel_Scene2D(Scene2D *s) {
    Scene *m = &(*s).base;
    return &(*m).base;
}

static inline Panel *Darling_asPanel_Scene3D(Scene3D *s) {
    Scene *m = &(*s).base;
    return &(*m).base;
}

static inline Panel *Darling_asPanel_LayeredContainer(LayeredContainer *p) { return (Panel*) (void*) p; }
static inline Panel *Darling_asPanel_SectionContainer(SectionContainer *p) { return (Panel*) (void*) p; }
static inline Panel *Darling_asPanel_ExpandableListContainer(ExpandableListContainer *p) { return (Panel*) (void*) p; }

static inline Panel *Darling_asPanel_Button(Button *p) { return (Panel*) (void*) p; }
static inline Panel *Darling_asPanel_Switch(Switch *p) { return (Panel*) (void*) p; }
static inline Panel *Darling_asPanel_Checkbox(Checkbox *p) { return (Panel*) (void*) p; }
static inline Panel *Darling_asPanel_RadioGroup(RadioGroup *p) { return (Panel*) (void*) p; }
static inline Panel *Darling_asPanel_Slider(Slider *p) { return (Panel*) (void*) p; }
static inline Panel *Darling_asPanel_Knob(Knob *p) { return (Panel*) (void*) p; }
static inline Panel *Darling_asPanel_Input(Input *p) { return (Panel*) (void*) p; }
static inline Panel *Darling_asPanel_Textarea(Textarea *p) { return (Panel*) (void*) p; }
static inline Panel *Darling_asPanel_InputOTP(InputOTP *p) { return (Panel*) (void*) p; }
static inline Panel *Darling_asPanel_Select(Select *p) { return (Panel*) (void*) p; }
static inline Panel *Darling_asPanel_DatePicker(DatePicker *p) { return (Panel*) (void*) p; }
static inline Panel *Darling_asPanel_ColorPicker(ColorPicker *p) { return (Panel*) (void*) p; }
static inline Panel *Darling_asPanel_ColorSwatch(ColorSwatch *p) { return (Panel*) (void*) p; }
static inline Panel *Darling_asPanel_FileDialog(FileDialog *p) { return (Panel*) (void*) p; }
static inline Panel *Darling_asPanel_Typography(Typography *p) { return (Panel*) (void*) p; }
static inline Panel *Darling_asPanel_Kbd(Kbd *p) { return (Panel*) (void*) p; }
static inline Panel *Darling_asPanel_Plot(Plot *p) { return (Panel*) (void*) p; }
static inline Panel *Darling_asPanel_Dialog(Dialog *p) { return &(*p).base; }

static inline Panel *Darling_asPanel_AlertDialog(AlertDialog *a) {
    Dialog *d = &(*a).base;
    return &(*d).base;
}

static inline Panel *Darling_asPanel_ColorDialog(ColorDialog *c) {
    Dialog *d = &(*c).base;
    return &(*d).base;
}

#define Darling_asPanel(p) _Generic((p), \
    Panel *: Darling_asPanel_Panel, \
    Label *: Darling_asPanel_Label, \
    Picture *: Darling_asPanel_Picture, \
    RichLabel *: Darling_asPanel_RichLabel, \
    Scene *: Darling_asPanel_Scene, \
    Scene2D *: Darling_asPanel_Scene2D, \
    Scene3D *: Darling_asPanel_Scene3D, \
    LayeredContainer *: Darling_asPanel_LayeredContainer, \
    SectionContainer *: Darling_asPanel_SectionContainer, \
    ExpandableListContainer *: Darling_asPanel_ExpandableListContainer, \
    Button *: Darling_asPanel_Button, \
    Switch *: Darling_asPanel_Switch, \
    Checkbox *: Darling_asPanel_Checkbox, \
    RadioGroup *: Darling_asPanel_RadioGroup, \
    Slider *: Darling_asPanel_Slider, \
    Knob *: Darling_asPanel_Knob, \
    Input *: Darling_asPanel_Input, \
    Textarea *: Darling_asPanel_Textarea, \
    InputOTP *: Darling_asPanel_InputOTP, \
    Select *: Darling_asPanel_Select, \
    DatePicker *: Darling_asPanel_DatePicker, \
    ColorPicker *: Darling_asPanel_ColorPicker, \
    ColorSwatch *: Darling_asPanel_ColorSwatch, \
    FileDialog *: Darling_asPanel_FileDialog, \
    Typography *: Darling_asPanel_Typography, \
    Kbd *: Darling_asPanel_Kbd, \
    Plot *: Darling_asPanel_Plot, \
    Dialog *: Darling_asPanel_Dialog, \
    AlertDialog *: Darling_asPanel_AlertDialog, \
    ColorDialog *: Darling_asPanel_ColorDialog \
)(p)

// --- per-child attach shims (one per accepted child type) -------------------
static inline void Darling_addPanel(Panel *parent, Panel *child) {
    Darling_addAny(parent, (void*) child, TYPE_PANEL_SINGLETON);
}

static inline void Darling_addLabel(Panel *parent, Label *child) {
    Darling_addAny(parent, (void*) child, TYPE_LABEL_SINGLETON);
}

static inline void Darling_addPicture(Panel *parent, Picture *child) {
    Darling_addAny(parent, (void*) child, TYPE_PICTURE_SINGLETON);
}

static inline void Darling_addRichLabel(Panel *parent, RichLabel *child) {
    Darling_addAny(parent, (void*) child, TYPE_RICH_LABEL_SINGLETON);
}

static inline void Darling_addScene(Panel *parent, Scene *child) {
    Darling_addAny(parent, (void*) child, TYPE_SCENE_SINGLETON);
}

static inline void Darling_addScene2D(Panel *parent, Scene2D *child) {
    Darling_addAny(parent, (void*) child, TYPE_SCENE2D_SINGLETON);
}

static inline void Darling_addScene3D(Panel *parent, Scene3D *child) {
    Darling_addAny(parent, (void*) child, TYPE_SCENE3D_SINGLETON);
}

static inline void Darling_addLayeredContainer(Panel *parent, LayeredContainer *child) {
    Darling_addAny(parent, (void*) child, TYPE_LAYERED_CONTAINER_SINGLETON);
}

static inline void Darling_addSectionContainer(Panel *parent, SectionContainer *child) {
    Darling_addAny(parent, (void*) child, TYPE_SECTION_CONTAINER_SINGLETON);
}

static inline void Darling_addExpandableListContainer(Panel *parent, ExpandableListContainer *child) {
    Darling_addAny(parent, (void*) child, TYPE_EXPANDABLE_LIST_CONTAINER_SINGLETON);
}

static inline void Darling_addButton(Panel *parent, Button *child) {
    Darling_addAny(parent, (void*) child, TYPE_BUTTON_SINGLETON);
}

static inline void Darling_addSwitch(Panel *parent, Switch *child) {
    Darling_addAny(parent, (void*) child, TYPE_SWITCH_SINGLETON);
}

static inline void Darling_addCheckbox(Panel *parent, Checkbox *child) {
    Darling_addAny(parent, (void*) child, TYPE_CHECKBOX_SINGLETON);
}

static inline void Darling_addRadioGroup(Panel *parent, RadioGroup *child) {
    Darling_addAny(parent, (void*) child, TYPE_RADIOGROUP_SINGLETON);
}

static inline void Darling_addSlider(Panel *parent, Slider *child) {
    Darling_addAny(parent, (void*) child, TYPE_SLIDER_SINGLETON);
}

static inline void Darling_addKnob(Panel *parent, Knob *child) {
    Darling_addAny(parent, (void*) child, TYPE_KNOB_SINGLETON);
}

static inline void Darling_addInput(Panel *parent, Input *child) {
    Darling_addAny(parent, (void*) child, TYPE_INPUT_SINGLETON);
}

static inline void Darling_addTextarea(Panel *parent, Textarea *child) {
    Darling_addAny(parent, (void*) child, TYPE_TEXTAREA_SINGLETON);
}

static inline void Darling_addInputOTP(Panel *parent, InputOTP *child) {
    Darling_addAny(parent, (void*) child, TYPE_INPUTOTP_SINGLETON);
}

static inline void Darling_addSelect(Panel *parent, Select *child) {
    Darling_addAny(parent, (void*) child, TYPE_SELECT_SINGLETON);
}

static inline void Darling_addDatePicker(Panel *parent, DatePicker *child) {
    Darling_addAny(parent, (void*) child, TYPE_DATEPICKER_SINGLETON);
}

static inline void Darling_addColorPicker(Panel *parent, ColorPicker *child) {
    Darling_addAny(parent, (void*) child, TYPE_COLORPICKER_SINGLETON);
}

static inline void Darling_addColorSwatch(Panel *parent, ColorSwatch *child) {
    Darling_addAny(parent, (void*) child, TYPE_COLORSWATCH_SINGLETON);
}

static inline void Darling_addFileDialog(Panel *parent, FileDialog *child) {
    Darling_addAny(parent, (void*) child, TYPE_FILEDIALOG_SINGLETON);
}

static inline void Darling_addTypography(Panel *parent, Typography *child) {
    Darling_addAny(parent, (void*) child, TYPE_TYPOGRAPHY_SINGLETON);
}

static inline void Darling_addKbd(Panel *parent, Kbd *child) {
    Darling_addAny(parent, (void*) child, TYPE_KBD_SINGLETON);
}

static inline void Darling_addPlot(Panel *parent, Plot *child) {
    Darling_addAny(parent, (void*) child, TYPE_PLOT_SINGLETON);
}

static inline void Darling_addDialog(Panel *parent, Dialog *child) {
    Darling_addAny(parent, (void*) child, TYPE_DIALOG_SINGLETON);
}

static inline void Darling_addAlertDialog(Panel *parent, AlertDialog *child) {
    Darling_addAny(parent, (void*) child, TYPE_ALERTDIALOG_SINGLETON);
}

static inline void Darling_addColorDialog(Panel *parent, ColorDialog *child) {
    Darling_addAny(parent, (void*) child, TYPE_COLORDIALOG_SINGLETON);
}

// Darling-side type helpers (no registry fork: IDs + Type_* live in
// vexspoke's oop/type.h; these just map darling pointers to them).
// classOf: pointer TYPE -> class ID at compile time (for Darling_addAny).
// kindName: class ID -> short name for logging/debugging.
#define Darling_classOf(child) _Generic((child), \
    Panel *: TYPE_PANEL_SINGLETON, \
    Label *: TYPE_LABEL_SINGLETON, \
    Picture *: TYPE_PICTURE_SINGLETON, \
    RichLabel *: TYPE_RICH_LABEL_SINGLETON, \
    Scene *: TYPE_SCENE_SINGLETON, \
    Scene2D *: TYPE_SCENE2D_SINGLETON, \
    Scene3D *: TYPE_SCENE3D_SINGLETON, \
    LayeredContainer *: TYPE_LAYERED_CONTAINER_SINGLETON, \
    SectionContainer *: TYPE_SECTION_CONTAINER_SINGLETON, \
    ExpandableListContainer *: TYPE_EXPANDABLE_LIST_CONTAINER_SINGLETON, \
    Button *: TYPE_BUTTON_SINGLETON, \
    Switch *: TYPE_SWITCH_SINGLETON, \
    Checkbox *: TYPE_CHECKBOX_SINGLETON, \
    RadioGroup *: TYPE_RADIOGROUP_SINGLETON, \
    Slider *: TYPE_SLIDER_SINGLETON, \
    Knob *: TYPE_KNOB_SINGLETON, \
    Input *: TYPE_INPUT_SINGLETON, \
    Textarea *: TYPE_TEXTAREA_SINGLETON, \
    InputOTP *: TYPE_INPUTOTP_SINGLETON, \
    Select *: TYPE_SELECT_SINGLETON, \
    DatePicker *: TYPE_DATEPICKER_SINGLETON, \
    ColorPicker *: TYPE_COLORPICKER_SINGLETON, \
    ColorSwatch *: TYPE_COLORSWATCH_SINGLETON, \
    FileDialog *: TYPE_FILEDIALOG_SINGLETON, \
    Typography *: TYPE_TYPOGRAPHY_SINGLETON, \
    Kbd *: TYPE_KBD_SINGLETON, \
    Plot *: TYPE_PLOT_SINGLETON, \
    Dialog *: TYPE_DIALOG_SINGLETON, \
    AlertDialog *: TYPE_ALERTDIALOG_SINGLETON, \
    ColorDialog *: TYPE_COLORDIALOG_SINGLETON, \
    Container *: TYPE_CONTAINER_SINGLETON, \
    Canvas *: TYPE_CANVAS_SINGLETON \
)

const char *Darling_kindName(uint64_t classId);

// Unified attach: Darling_add(parent, child). Parent accepts any
// Panel-derived pointer via Darling_asPanel; child picks the shim.
// Container/Canvas children are compile errors (ov_type_error), never silent.
#define Darling_add(parent, child) _Generic((child), \
    Panel *: Darling_addPanel, \
    Label *: Darling_addLabel, \
    Picture *: Darling_addPicture, \
    RichLabel *: Darling_addRichLabel, \
    Scene *: Darling_addScene, \
    Scene2D *: Darling_addScene2D, \
    Scene3D *: Darling_addScene3D, \
    LayeredContainer *: Darling_addLayeredContainer, \
    SectionContainer *: Darling_addSectionContainer, \
    ExpandableListContainer *: Darling_addExpandableListContainer, \
    Button *: Darling_addButton, \
    Switch *: Darling_addSwitch, \
    Checkbox *: Darling_addCheckbox, \
    RadioGroup *: Darling_addRadioGroup, \
    Slider *: Darling_addSlider, \
    Knob *: Darling_addKnob, \
    Input *: Darling_addInput, \
    Textarea *: Darling_addTextarea, \
    InputOTP *: Darling_addInputOTP, \
    Select *: Darling_addSelect, \
    DatePicker *: Darling_addDatePicker, \
    ColorPicker *: Darling_addColorPicker, \
    ColorSwatch *: Darling_addColorSwatch, \
    FileDialog *: Darling_addFileDialog, \
    Typography *: Darling_addTypography, \
    Kbd *: Darling_addKbd, \
    Plot *: Darling_addPlot, \
    Dialog *: Darling_addDialog, \
    AlertDialog *: Darling_addAlertDialog, \
    ColorDialog *: Darling_addColorDialog, \
    Container *: ov_type_error, \
    Canvas *: ov_type_error, \
    default: ov_type_error \
)(Darling_asPanel(parent), (child))

#endif
