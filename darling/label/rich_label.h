#ifndef DARLING_RICH_LABEL_H
#define DARLING_RICH_LABEL_H

#include "darling/panel/panel.h"
#include "darling/cursor/cursor.h"
#include "text/rich_text.h"
#include "text/text_core.h"
#include "text/text_select.h"
#include "event/keyevent.h"
#include "event/pointer.h"
#include <stdint.h>

typedef struct RichLabel {
    Panel base;
    RichText *textModel;
    WrapMode wrapMode; // Inherited by the layout engine during validation

    // Typography additions
    TextAlign textAlign;
    float spacingWidth;
    float spacingHeight;
    bool ligatures;

    // Highlight & cursor state
    Cursor *cursor;           // active mouse cursor style (I-beam when highlightable)
    bool highlightable;       // enables text selection drag (labels aren't editable)
    TextSelect select;        // shared fixed-anchor selection part (anchor/active edge + hover)
    uint32_t highlightColor;  // packed 0xAARRGGBB selection fill (default 0x662563EB)
} RichLabel;

RichLabel *RichLabel_0(void);
RichLabel *RichLabel_1(Panel *parent);
void RichLabel_free(RichLabel *label);

void RichLabel_setTextModel(RichLabel *label, RichText *model);
void RichLabel_setWrapMode(RichLabel *label, WrapMode mode);
void RichLabel_setTextAlign(RichLabel *label, TextAlign align);
TextAlign RichLabel_getTextAlign(const RichLabel *label);
void RichLabel_setSpacingWidth(RichLabel *label, float width);
float RichLabel_getSpacingWidth(const RichLabel *label);
void RichLabel_setSpacingHeight(RichLabel *label, float height);
float RichLabel_getSpacingHeight(const RichLabel *label);
void RichLabel_setLigatures(RichLabel *label, bool flag);
bool RichLabel_hasLigatures(const RichLabel *label);

// Selection & interaction
void RichLabel_setHighlightable(RichLabel *label, bool flag);
bool RichLabel_isHighlightable(const RichLabel *label);
void RichLabel_setSelection(RichLabel *label, int32_t start, int32_t end);
void RichLabel_getSelection(const RichLabel *label, int32_t *outStart, int32_t *outEnd);
void RichLabel_setHighlightColor(RichLabel *label, uint32_t color);
uint32_t RichLabel_getHighlightColor(const RichLabel *label);
void RichLabel_setHighlightColorRGBA(RichLabel *label, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void RichLabel_getHighlightColorRGBA(const RichLabel *label, uint8_t *outR, uint8_t *outG, uint8_t *outB, uint8_t *outA);
void RichLabel_setHovered(RichLabel *label, bool hovered);
bool RichLabel_isHovered(const RichLabel *label);
void RichLabel_setCursor(RichLabel *label, Cursor *cursor);
Cursor *RichLabel_getCursor(const RichLabel *label);

// Interactive pointer & hit-test functions
int32_t RichLabel_charIndexAt(const RichLabel *label, float localX, float localY);
void RichLabel_handlePointer(RichLabel *label, int kind, float localX, float localY, void *window);
void RichLabel_onPointer(RichLabel *label, PointerEvent *ev, void *window);

// Key seam: Cmd/Ctrl+C copies the committed selection.
void RichLabel_handleKey(RichLabel *label, const UIKeyEvent *ev);

// Tag-stripped plain copy of the committed selection (arena-allocated).
char *RichLabel_getSelectedText(const RichLabel *label);

#endif // DARLING_RICH_LABEL_H