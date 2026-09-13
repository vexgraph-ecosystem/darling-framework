#ifndef DARLING_FIELD_TEXTAREA_H
#define DARLING_FIELD_TEXTAREA_H

#include <stdbool.h>
#include <stdint.h>

#include "c23/constructor.h"
#include "darling/panel/panel.h"
#include "event/keyevent.h"
#include "font/font.h"
#include "text/text_core.h"
#include "text/text_select.h"

typedef void (*Textarea_ChangeFn)(void *ctx);

// Multi-line text area: Panel layout plus an owned buffer with scroll state,
// a caret cursor, a focus flag, and a change callback for live editing.
typedef struct Textarea {
    Panel base;
    char *text;
    int32_t visibleLines;
    int32_t wrap;
    float scrollY;
    Font *font;
    int32_t cursor;
    bool focused;
    Textarea_ChangeFn onChange;
    void *ctx;

    // --- Typography & Native Raster Styling ---
    float fontSize;           // default 13.0f
    uint32_t textColor;       // default 0xFFFFFFFF
    TextAlign align;          // default TEXT_ALIGN_LEFT
    float spacingWidth;       // default 0.0f
    float spacingHeight;      // default 0.0f
    bool ligatures;           // default true
    TextSelect select;        // multiline selection state
    uint32_t selectionColor;  // default 0x662563EB
    int32_t rasterTex;
    int rasterW;
    int rasterH;
    float rasterBacking;
    bool rasterDirty;
} Textarea;

Textarea *Textarea_0(void);
Textarea *Textarea_2(Panel *parent, int32_t visibleLines);

#define Textarea(...) CONSTRUCTOR_DISPATCH(Textarea, __VA_ARGS__)

// Core scroll (stub: clamping lands with the scroll walker).
void Textarea_scrollTo(Textarea *ta, float y);
// Caret placement: clamps the index into the buffer, then dirties.
void Textarea_goTo(Textarea *self, int32_t index);
// Live events (Pkg 4): DOWN focuses; keys edit and navigate (see textarea.c).
void Textarea_handlePointer(Textarea *self, int kind, float localX, float localY);
void Textarea_handleKey(Textarea *self, const UIKeyEvent *ev);

void Textarea_free(Textarea *ta);

void Textarea_setText(Textarea *ta, const char *text);
void Textarea_setVisibleLines(Textarea *ta, int32_t lines);
void Textarea_setWrap(Textarea *ta, int32_t wrap);
void Textarea_setScrollY(Textarea *ta, float y);
void Textarea_setFont(Textarea *ta, Font *font);
void Textarea_setCursor(Textarea *self, int32_t cursor);
void Textarea_setOnChange(Textarea *self, Textarea_ChangeFn fn);
void Textarea_setCtx(Textarea *self, void *ctx);

void Textarea_setFontSize(Textarea *ta, float size);
void Textarea_setTextColor(Textarea *ta, uint32_t color);
void Textarea_setTextAlign(Textarea *ta, TextAlign align);
void Textarea_setSpacingWidth(Textarea *ta, float width);
void Textarea_setSpacingHeight(Textarea *ta, float height);
void Textarea_setLigatures(Textarea *ta, bool flag);
void Textarea_setSelection(Textarea *ta, int32_t start, int32_t end);
void Textarea_setSelectionColor(Textarea *ta, uint32_t color);
char *Textarea_getSelectedText(const Textarea *ta);
void Textarea_setSelectedText(Textarea *ta, const char *newText);

const char *Textarea_getText(const Textarea *ta);
int32_t Textarea_getVisibleLines(const Textarea *ta);
int32_t Textarea_getWrap(const Textarea *ta);
float Textarea_getScrollY(const Textarea *ta);
Font *Textarea_getFont(const Textarea *ta);
int32_t Textarea_getCursor(const Textarea *self);
bool Textarea_isFocused(const Textarea *self);
Textarea_ChangeFn Textarea_getOnChange(const Textarea *self);
void *Textarea_getCtx(const Textarea *self);

float Textarea_getFontSize(const Textarea *ta);
uint32_t Textarea_getTextColor(const Textarea *ta);
TextAlign Textarea_getTextAlign(const Textarea *ta);
float Textarea_getSpacingWidth(const Textarea *ta);
float Textarea_getSpacingHeight(const Textarea *ta);
bool Textarea_hasLigatures(const Textarea *ta);
void Textarea_getSelection(const Textarea *ta, int32_t *outStart, int32_t *outEnd);
uint32_t Textarea_getSelectionColor(const Textarea *ta);

#endif
