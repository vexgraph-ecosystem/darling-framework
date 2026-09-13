#ifndef DARLING_FIELD_INPUT_H
#define DARLING_FIELD_INPUT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "c23/constructor.h"
#include "darling/panel/panel.h"
#include "event/keyevent.h"
#include "font/font.h"
#include "text/text_core.h"
#include "text/text_select.h"

// Single-line text input: Panel layout plus an owned bounded buffer.
//
// THE CARET (its own part, views only — never pierce it):
// The caret is Input's owned sub-object: mode, color, blink timing, and the
// gliding x position. It does NOT measure text — the owner (later the caret
// walker) places it with caret_setTarget after measuring the cursor run.
//   BLINK: terminal caret, 50%-duty blink at blinkPeriod.
//   SOLID: always on, snaps to target.
//   GLIDE: always on, eases to target (Word-style) via tick.
// The caret VISUAL is a replaceable view: caretView is a borrowed Panel*
// (null = default thin rect painted by the pump). Games swap in anything —
// a textured quad, an animated sprite, a Scene — and the caret centers it
// on caretX for them via caret_placeView. Opacity folds blink × user value
// into one effective number the pump paints with.
// Tick it on Thread 0 next to layout; paint at caret_getX when
// caret_isShown. Typing (setText/setCursor) restarts the blink phase shown.
#define INPUT_CARET_BLINK 0
#define INPUT_CARET_SOLID 1
#define INPUT_CARET_GLIDE 2

#define INPUT_CARET_DEFAULT_PERIOD 0.53f
#define INPUT_CARET_GLIDE_TIME 0.08f

typedef void (*Input_ChangeFn)(void *ctx);
typedef void (*Input_SubmitFn)(void *ctx);
// Measures the x of a cursor index (owner/walker fills this in; the caret
// walker will own it permanently). Null = no measurement, target holds.
typedef float (*Input_MeasureFn)(void *ctx, int32_t index);

typedef struct Input {
    // --- Input core (owner fields: text state, not any part) ---
    Panel base;
    char *text;
    size_t cap;
    char *placeholder;
    bool password;
    bool readonly;
    bool focused;       // Focus-request flag (DOWN sets, dispatch consumes later)
    int32_t cursor;
    Font *font;

    // --- Typography & Native Raster Styling ---
    float fontSize;           // font size in points (default 13.0f)
    uint32_t textColor;       // text color packed ARGB (default 0xFFFFFFFF)
    uint32_t placeholderColor;// placeholder color packed ARGB (default 0x88888888)
    TextAlign align;          // text alignment (TEXT_ALIGN_LEFT, TEXT_ALIGN_CENTER, TEXT_ALIGN_RIGHT)
    float spacingWidth;       // kerning/tracking delta in points (default 0.0f)
    bool ligatures;           // enable standard typography ligatures (default true)
    TextSelect select;        // text selection state
    uint32_t selectionColor;  // highlight fill (default 0x662563EB)
    int32_t rasterTex;
    int rasterW;
    int rasterH;
    float rasterBacking;
    bool rasterDirty;
    float *glyphX;
    int32_t glyphN;

    // --- Caret part (field->caret->verb; views only, never pierce) ---
    int caretMode;          // BLINK/SOLID/GLIDE (default BLINK)
    uint32_t caretColor;    // packed 0xAARRGGBB
    float caretBlinkPeriod; // half-cycle seconds (default 0.53)
    double caretClock;      // blink timer (tick advances)
    bool caretShown;        // current blink phase (view)
    float caretX;           // painted x (glides to target)
    float caretTargetX;     // owner-measured x (view target)
    Panel *caretView;       // borrowed visual (null = thin rect)
    float caretOpacity;     // user opacity 0..1 (× blink phase)
    // --- Input core callbacks (owner fields, continued) ---
    Input_ChangeFn onChange;
    Input_SubmitFn onSubmit;
    void *ctx;
    Input_MeasureFn measurer; // index->x hook (null until the walker lands)
    void *measureCtx;
} Input;

Input *Input_0(void);
Input *Input_2(Panel *parent, size_t cap);

#define Input(...) CONSTRUCTOR_DISPATCH(Input, __VA_ARGS__)

// Core editing (live: byte-wise UTF-8 surgery at the cursor, cap-truncated
// like setText, caret re-measured, blink restarted, onChange fired).
void Input_insertChar(Input *inp, char c);
void Input_eraseChar(Input *inp);
// Live events (Pkg 4): pointer DOWN requests focus + places the caret,
// pressed keys type/erase/move/submit. Null-safe no-ops on null self.
void Input_handlePointer(Input *self, int kind, float localX, float localY);
void Input_handleKey(Input *self, const UIKeyEvent *ev);
// Move to an index: clamps, restarts blink, re-measures the caret target,
// then blits (BLINK/SOLID) or glides (GLIDE) — the Word-inspired goTo.
void Input_goTo(Input *inp, int32_t index);

void Input_free(Input *inp);

void Input_setText(Input *inp, const char *text);
void Input_setCap(Input *inp, size_t cap);
void Input_setPlaceholder(Input *inp, const char *placeholder);
void Input_setPassword(Input *inp, bool password);
void Input_setReadonly(Input *inp, bool readonly);
void Input_setFocused(Input *inp, bool focused);
void Input_setCursor(Input *inp, int32_t cursor);
void Input_setFont(Input *inp, Font *font);
void Input_setOnChange(Input *inp, Input_ChangeFn fn);
void Input_setOnSubmit(Input *inp, Input_SubmitFn fn);
void Input_setCtx(Input *inp, void *ctx);
void Input_setMeasurer(Input *inp, Input_MeasureFn fn, void *ctx);
void Input_setFontSize(Input *inp, float size);
void Input_setTextColor(Input *inp, uint32_t color);
void Input_setPlaceholderColor(Input *inp, uint32_t color);
void Input_setTextAlign(Input *inp, TextAlign align);
void Input_setSpacingWidth(Input *inp, float width);
void Input_setLigatures(Input *inp, bool ligatures);
void Input_setSelection(Input *inp, int32_t start, int32_t end);
void Input_setSelectionColor(Input *inp, uint32_t color);
char *Input_getSelectedText(const Input *inp);
void Input_setSelectedText(Input *inp, const char *text);

// Caret part (field->caret->verb, ergonomic — the caret is a view).
void Input_caret_setMode(Input *inp, int mode);
void Input_caret_setColor(Input *inp, uint32_t color);
void Input_caret_setBlinkPeriod(Input *inp, float seconds);
void Input_caret_setTarget(Input *inp, float x);
void Input_caret_setView(Input *inp, Panel *view);
void Input_caret_setOpacity(Input *inp, float opacity);
void Input_caret_placeView(Input *inp, Panel *view, float centerY);
void Input_caret_tick(Input *inp, double dt);

const char *Input_getText(const Input *inp);
size_t Input_getCap(const Input *inp);
const char *Input_getPlaceholder(const Input *inp);
bool Input_isPassword(const Input *inp);
bool Input_isReadonly(const Input *inp);
bool Input_isFocused(const Input *inp);
int32_t Input_getCursor(const Input *inp);
Font *Input_getFont(const Input *inp);
float Input_getFontSize(const Input *inp);
uint32_t Input_getTextColor(const Input *inp);
uint32_t Input_getPlaceholderColor(const Input *inp);
TextAlign Input_getTextAlign(const Input *inp);
float Input_getSpacingWidth(const Input *inp);
bool Input_hasLigatures(const Input *inp);
void Input_getSelection(const Input *inp, int32_t *outStart, int32_t *outEnd);
uint32_t Input_getSelectionColor(const Input *inp);
Input_ChangeFn Input_getOnChange(const Input *inp);
Input_SubmitFn Input_getOnSubmit(const Input *inp);
void *Input_getCtx(const Input *inp);
Input_MeasureFn Input_getMeasurer(const Input *inp);
void *Input_getMeasureContext(const Input *inp);

// Caret-part getters (views over caret state).
int Input_caret_getMode(const Input *inp);
uint32_t Input_caret_getColor(const Input *inp);
float Input_caret_getBlinkPeriod(const Input *inp);
float Input_caret_getTarget(const Input *inp);
float Input_caret_getX(const Input *inp);
bool Input_caret_isShown(const Input *inp);
Panel *Input_caret_getView(const Input *inp);
float Input_caret_getOpacity(const Input *inp);
float Input_caret_getEffectiveOpacity(const Input *inp);

#endif
