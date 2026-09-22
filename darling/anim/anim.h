#ifndef DARLING_ANIM_ANIM_H
#define DARLING_ANIM_ANIM_H

#include <stdbool.h>
#include <stddef.h>

#include "c23/constructor.h"
#include "darling/component.h"

// darling/anim/anim.h — keyframed 2D animation: timestamps + easing.
//
// An Anim is data: per-section key lists (location / size / scale /
// font-size / alpha), each key carrying a timestamp and ONE flat easing
// token (ANIM_EASE_OUT, ANIM_BOUNCE_OUT, ...). Playback is one call per
// widget class — Container_animate, Panel_animate, Label_animate,
// Button_animate, ... — and advances on Thread 0 via Anim_tick(dt),
// next to layout (bounds animate on Thread 0; draw just snapshots).
//
// Easing semantics (the sketch is the spec):
//   IN     = launch curve  (slow start, fast end)
//   OUT    = landing curve (fast start, slow end)
//   IN_OUT = merged hill   (IN for u<0.5, OUT for u>=0.5, continuous)
// Velocity profiles: EASE = triangle, SINE = dome, EXPONENTIAL = steeper
// hill (pow(x,2) applied twice), ELASTIC = snap + arc tail, BOUNCE = humps.
//
// Ownership: Anim_play BORROWS the Anim (never frees it) — one pop preset
// animates fifty toasts. Call Anim_free when the preset is retired.
// Replaying on the same container restarts that container's binding.

// Flat easing tokens: ANIM_<FAMILY>_<IN|OUT|IN_OUT>. NORMAL is
// directionless (a straight line has no launch/landing).
typedef enum AnimEase {
    ANIM_NORMAL = 0,

    ANIM_EASE_IN,
    ANIM_EASE_OUT,
    ANIM_EASE_IN_OUT,

    ANIM_SINE_IN,
    ANIM_SINE_OUT,
    ANIM_SINE_IN_OUT,

    ANIM_EXPONENTIAL_IN,
    ANIM_EXPONENTIAL_OUT,
    ANIM_EXPONENTIAL_IN_OUT,

    ANIM_ELASTIC_IN,
    ANIM_ELASTIC_OUT,
    ANIM_ELASTIC_IN_OUT,

    ANIM_BOUNCE_IN,
    ANIM_BOUNCE_OUT,
    ANIM_BOUNCE_IN_OUT
} AnimEase;

// Sections: each widget class animates the sections its struct owns.
// Container = location/size/scale; Panel adds alpha (bg channel);
// Label/Button add font size. Unknown sections for a kind are ignored.
typedef enum AnimSection {
    ANIM_SECTION_LOCATION = 0,
    ANIM_SECTION_SIZE,
    ANIM_SECTION_SCALE,
    ANIM_SECTION_FONT_SIZE,
    ANIM_SECTION_ALPHA,
    ANIM_SECTION_COUNT
} AnimSection;

// Playback kind: must match the target's real type (facades guarantee it).
typedef enum AnimKind {
    ANIM_KIND_CONTAINER = 0, // x/y/w/h/sx/sy
    ANIM_KIND_PANEL,         // + bg alpha
    ANIM_KIND_LABEL,         // + font size (Label)
    ANIM_KIND_BUTTON         // + font size (Button)
} AnimKind;

typedef struct AnimLocKey {
    float t;
    float x, y;
    AnimEase ease;
} AnimLocKey;

typedef struct AnimSizeKey {
    float t;
    float w, h;
    AnimEase ease;
} AnimSizeKey;

typedef struct AnimScaleKey {
    float t;
    float sx, sy;
    AnimEase ease;
} AnimScaleKey;

typedef struct AnimFontKey {
    float t;
    float size;
    AnimEase ease;
} AnimFontKey;

typedef struct AnimAlphaKey {
    float t;
    float alpha; // 0..1 over the Panel bg channel
    AnimEase ease;
} AnimAlphaKey;

typedef void (*Anim_DoneFn)(void *ctx);

typedef struct Anim {
    AnimLocKey *loc;
    size_t locCount, locCap;
    AnimSizeKey *size;
    size_t sizeCount, sizeCap;
    AnimScaleKey *scale;
    size_t scaleCount, scaleCap;
    AnimFontKey *font;
    size_t fontCount, fontCap;
    AnimAlphaKey *alpha;
    size_t alphaCount, alphaCap;
    bool loop;
    Anim_DoneFn onDone;
    void *ctx;
} Anim;

// Constructors:
 //   Anim() — empty timeline, no loop
Anim *Anim_0(void);

#define Anim(...) CONSTRUCTOR_DISPATCH(Anim, __VA_ARGS__)

void Anim_free(Anim *a);

// Keys (insertion-sorted by t; the segment INTO a key uses that key's ease).
void Anim_addLocation(Anim *a, float t, float x, float y, AnimEase ease);
void Anim_addSize(Anim *a, float t, float w, float h, AnimEase ease);
void Anim_addScale(Anim *a, float t, float sx, float sy, AnimEase ease);
void Anim_addFontSize(Anim *a, float t, float size, AnimEase ease);
void Anim_addAlpha(Anim *a, float t, float alpha, AnimEase ease);

// Pure easing evaluator (u in [0,1]; elastic may overshoot outside it).
float Anim_eval(AnimEase ease, float u);

// Timeline queries.
float Anim_duration(const Anim *a); // max key t across sections (0 = instant)
size_t Anim_keyCount(const Anim *a, int section);

// Loop + completion.
void Anim_setLoop(Anim *a, bool loop);
bool Anim_isLoop(const Anim *a);
void Anim_setOnDone(Anim *a, Anim_DoneFn fn, void *ctx);
Anim_DoneFn Anim_getOnDone(const Anim *a);
void *Anim_getDoneContext(const Anim *a);

// Player (Thread 0 only). Binds a borrowed Anim to a component; the first
// key of each section interpolates FROM the value captured at play time,
// so one preset is reusable across widgets.
void Anim_play(Component *c, Anim *a, int kind);
void Anim_tick(double dt); // advance all live bindings; fires onDone on finish
void Anim_cancel(Component *c);
void Anim_cancelAll(void);
bool Anim_isPlaying(const Component *c);
size_t Anim_liveCount(void);

// Forward declarations (defined in their own headers; declared here so the
// facade prototypes below share one struct tag at file scope).
struct Panel;
struct Label;
struct Button;
struct ListContainer;
struct GridContainer;
struct ScrollContainer;
struct SectionContainer;
struct LayeredContainer;
struct MarkdownPanel;
struct RichTextPanel;
struct Switch;
struct Checkbox;
struct RadioGroup;
struct Slider;
struct Knob;
struct Input;
struct Textarea;
struct InputOTP;
struct Select;
struct ScrollBar;
struct DatePicker;
struct ColorPicker;
struct ColorSwatch;
struct Dialog;
struct AlertDialog;
struct ColorDialog;
struct FileDialog;
struct Picture;
struct Plot;
struct Kbd;
struct RichLabel;
struct Typography;
struct Scene;
struct Canvas;

// Per-class facades: ContainerClass_animate(ptr, anim). Pointer casts to
// the embedded base are safe (base is always the first member); kind
// selects which sections apply.
void Anim_animate(Component *c, Anim *a);
void Container_animate(Component *c, Anim *a);
void Panel_animate(struct Panel *p, Anim *a);
void Label_animate(struct Label *l, Anim *a);
void Button_animate(struct Button *b, Anim *a);

void ListContainer_animate(struct ListContainer *p, Anim *a);
void GridContainer_animate(struct GridContainer *p, Anim *a);
void ScrollContainer_animate(struct ScrollContainer *p, Anim *a);
void SectionContainer_animate(struct SectionContainer *p, Anim *a);
void LayeredContainer_animate(struct LayeredContainer *p, Anim *a);
void MarkdownPanel_animate(struct MarkdownPanel *p, Anim *a);
void RichTextPanel_animate(struct RichTextPanel *p, Anim *a);
void Switch_animate(struct Switch *s, Anim *a);
void Checkbox_animate(struct Checkbox *c, Anim *a);
void RadioGroup_animate(struct RadioGroup *r, Anim *a);
void Slider_animate(struct Slider *s, Anim *a);
void Knob_animate(struct Knob *k, Anim *a);
void Input_animate(struct Input *i, Anim *a);
void Textarea_animate(struct Textarea *t, Anim *a);
void InputOTP_animate(struct InputOTP *o, Anim *a);
void Select_animate(struct Select *s, Anim *a);
void ScrollBar_animate(struct ScrollBar *s, Anim *a);
void DatePicker_animate(struct DatePicker *d, Anim *a);
void ColorPicker_animate(struct ColorPicker *c, Anim *a);
void ColorSwatch_animate(struct ColorSwatch *c, Anim *a);
void Dialog_animate(struct Dialog *d, Anim *a);
void AlertDialog_animate(struct AlertDialog *d, Anim *a);
void ColorDialog_animate(struct ColorDialog *d, Anim *a);
void FileDialog_animate(struct FileDialog *d, Anim *a);
void Picture_animate(struct Picture *p, Anim *a);
void Plot_animate(struct Plot *p, Anim *a);
void Kbd_animate(struct Kbd *k, Anim *a);
void RichLabel_animate(struct RichLabel *l, Anim *a);
void Typography_animate(struct Typography *t, Anim *a);
void Scene_animate(struct Scene *s, Anim *a);
void Canvas_animate(struct Canvas *c, Anim *a);

#endif
