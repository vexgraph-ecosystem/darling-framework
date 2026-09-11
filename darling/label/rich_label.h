#ifndef DARLING_RICH_LABEL_H
#define DARLING_RICH_LABEL_H

#include "darling/panel/panel.h"
#include "darling/cursor/cursor.h"
#include "text/rich_text.h"
#include "event/pointer.h"
#include <stdint.h>

typedef struct RichLabel {
    Panel base;
    RichText *textModel;
    WrapMode wrapMode; // Inherited by the layout engine during validation

    // Highlight & cursor state
    Cursor *cursor;           // active mouse cursor style (I-beam when highlightable)
    bool highlightable;       // enables text selection drag (labels aren't editable)
    int32_t selectionStart;   // fixed selection anchor (byte index, -1 = none); ordered via getSelection
    int32_t selectionEnd;     // active drag edge (byte index, -1 = none); getters/raster order the pair
    uint32_t highlightColor;  // packed 0xAARRGGBB selection fill (default 0x662563EB)
    bool hovered;             // true if pointer is currently hovering within label bounds
} RichLabel;

RichLabel *RichLabel_0(void);
RichLabel *RichLabel_1(Panel *parent);
void RichLabel_free(RichLabel *label);

void RichLabel_setTextModel(RichLabel *label, RichText *model);
void RichLabel_setWrapMode(RichLabel *label, WrapMode mode);

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

#endif // DARLING_RICH_LABEL_H