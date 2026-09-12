#ifndef DARLING_LABEL_H
#define DARLING_LABEL_H

#include "darling/panel/panel.h"
#include "font/font.h"
#include "text/text_core.h"
#include "text/text_select.h"
#include "darling/cursor/cursor.h"
#include <stdint.h>
#include "c23/constructor.h"

#include "event/pointer.h"
#include "event/keyevent.h"

// A lightweight View component for simple, single-styled text.
// Sharp path: one native CoreText raster per line (single textured quad).
// RichLabel keeps the SDF atlas path for mask/fill effects.
typedef struct Label {
    Panel base;
    char *text;
    Font *font;
    char *fontFamily;
    float fontSize;
    uint32_t textColor;
    float smoothness;
    int32_t rasterTex;
    int rasterW;
    int rasterH;
    float rasterBacking;
    bool rasterDirty;
    float *glyphX;          // Per-byte CoreText pen offsets in points (strlen+1), NULL = uniform fallback
    int32_t glyphN;         // Entry count of glyphX (0 when absent)
    bool ownsText;            // true if text was copied and owned by label
    bool ownsFontFamily;      // true if fontFamily was copied and owned by label

    // Typography & text styling
    bool highlightable;       // enables text selection drag (no caret; labels aren't editable)
    bool mnemonic;            // parse '&' key accelerator prefix
    char mnemonicChar;        // parsed accelerator character ('\0' if none)
    int mnemonicIndex;        // index in display text (-1 if none)
    bool ligatures;           // enable standard typography ligatures (default true)
    float spacingWidth;       // letter tracking/kerning delta in points (default 0.0)
    float spacingHeight;      // line leading delta in points (default 0.0)
    UnderlineStyle underline; // UNDERLINE_NONE, UNDERLINE_BASIC, etc.
    uint32_t underlineColor;  // packed 0xAARRGGBB (0 = inherit textColor)

    // Highlight & cursor state
    Cursor *cursor;           // active mouse cursor style (I-beam when highlightable)
    TextSelect select;        // shared selection part (anchor/active edge + hover lifecycle)
    float highlightRadius;    // corner radius in points for selection rounded rect (default 3.0f)
    uint32_t highlightColor;  // packed 0xAARRGGBB selection background color (default 0x662563EB)
} Label;

Label *Label_0(void);
Label *Label_1(const char *text);
Label *Label_2(Panel *parent, const char *text);
Label *Label_1_parent(Panel *parent);

#define Label(...) CONSTRUCTOR_DISPATCH(Label, __VA_ARGS__)

void Label_setText(Label *label, const char *text);
void Label_setTextBorrowed(Label *label, const char *text);
void Label_setFont(Label *label, Font *font);
void Label_setFontFamily(Label *label, const char *family);
void Label_setFontFamilyBorrowed(Label *label, const char *family);
void Label_setFontSize(Label *label, float size);
void Label_setTextColor(Label *label, uint32_t color);
void Label_setSmoothness(Label *label, float smoothness);
void Label_free(Label *label);
void Label_setLocation(Label *label, float x, float y);
void Label_setSize(Label *label, float w, float h);
void Label_setBackgroundColor(Label *label, uint32_t color);

// Typography & styling setters
void Label_setHighlightable(Label *label, bool flag);
void Label_setMnemonic(Label *label, bool flag);
void Label_setLigatures(Label *label, bool flag);
void Label_setSpacingWidth(Label *label, float width);
void Label_setSpacingHeight(Label *label, float height);
void Label_setSpacing(Label *label, float width, float height);
void Label_setUnderline(Label *label, UnderlineStyle style);
void Label_setUnderlineColor(Label *label, uint32_t color);
void Label_setUnderlineColorRGBA(Label *label, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void Label_setCursor(Label *label, Cursor *cursor);
void Label_setSelection(Label *label, int32_t start, int32_t end);
void Label_setHighlightRadius(Label *label, float radius);
void Label_setHighlightColor(Label *label, uint32_t color);
void Label_setHighlightColorRGBA(Label *label, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void Label_setHovered(Label *label, bool hovered);

// Interactive pointer & hit-test functions
int32_t Label_charIndexAt(const Label *label, float localX);
void Label_handlePointer(Label *label, int kind, float localX, float localY, void *window);
void Label_onPointer(Label *label, PointerEvent *ev, void *window);
void Label_handleKey(Label *label, const UIKeyEvent *ev);

// Symmetric Getters (Java-library standard)
const char *Label_getText(const Label *label);
Font *Label_getFont(const Label *label);
const char *Label_getFontFamily(const Label *label);
float Label_getFontSize(const Label *label);
uint32_t Label_getTextColor(const Label *label);
float Label_getSmoothness(const Label *label);
int32_t Label_getRasterTexture(const Label *label);
void Label_getRasterSize(const Label *label, int *outW, int *outH);
float Label_getRasterBacking(const Label *label);
bool Label_isRasterDirty(const Label *label);
const float *Label_getGlyphOffsets(const Label *label);
int32_t Label_getGlyphOffsetCount(const Label *label);

bool Label_isHighlightable(const Label *label);
bool Label_isMnemonic(const Label *label);
char Label_getMnemonicChar(const Label *label);
int Label_getMnemonicIndex(const Label *label);
bool Label_hasLigatures(const Label *label);
float Label_getSpacingWidth(const Label *label);
float Label_getSpacingHeight(const Label *label);
void Label_getSpacing(const Label *label, float *outWidth, float *outHeight);
UnderlineStyle Label_getUnderline(const Label *label);
uint32_t Label_getUnderlineColor(const Label *label);
void Label_getUnderlineColorRGBA(const Label *label, uint8_t *outR, uint8_t *outG, uint8_t *outB, uint8_t *outA);
Cursor *Label_getCursor(const Label *label);
void Label_getSelection(const Label *label, int32_t *outStart, int32_t *outEnd);
float Label_getHighlightRadius(const Label *label);
uint32_t Label_getHighlightColor(const Label *label);
void Label_getHighlightColorRGBA(const Label *label, uint8_t *outR, uint8_t *outG, uint8_t *outB, uint8_t *outA);
bool Label_isHovered(const Label *label);

// Selection and clipboard helpers
char *Label_getSelectedText(const Label *label);
void Label_setSelectedText(Label *label, const char *newText);

#endif // DARLING_LABEL_H
