#ifndef DARLING_TEXT_SELECT_H
#define DARLING_TEXT_SELECT_H

#include <stdbool.h>
#include <stdint.h>

// text/text_select.h — shared text-highlight part for Label / RichLabel /
// MarkdownPanel.
//
// Every text-handling container drives the same fixed-anchor selection state
// machine and the same hover caret-cursor lifecycle through this embedded
// value part: PTR_DOWN anchors, PTR_DRAG moves only the active edge, PTR_UP
// orders and COMMITS a nonzero range as the new fixed selection (or collapses
// a plain click), PTR_CANCEL clears.
// The part is plain state — it never allocates and never touches panels or
// windows; the owning class maps geometry to byte offsets (charIndexAt) and
// applies the resulting span to its own highlight raster.

typedef struct TextSelect {
    int32_t anchor;   // fixed press edge (byte offset; -1 = inactive)
    int32_t active;   // live drag edge (byte offset; -1 = inactive)
    bool hovered;     // pointer inside the widget bounds (caret-cursor lifecycle)
} TextSelect;

// Constructors:
//   TextSelect_default()  — fresh part: anchor -1, active -1, not hovered
TextSelect TextSelect_default(void);

// Core Functions:
//   TextSelect_begin(sel, idx)                : down — anchor + active collapse
//   TextSelect_drag(sel, idx)                 : move the active edge only; true iff the edge moved
//   TextSelect_end(sel, outLo, outHi)         : order + commit; outLo/outHi dest-last
//   TextSelect_cancel(sel)                    : clear without committing
//   TextSelect_getSpan(sel, outLo, outHi)     : ordered span while active
//
// Setters:
//   TextSelect_setHovered(sel, flag)          : hover caret-cursor lifecycle
//
// Getters:
//   TextSelect_isActive(sel)
//   TextSelect_isHovered(sel)
//   TextSelect_getAnchor(sel)
//   TextSelect_getActive(sel)

void TextSelect_reset(TextSelect *sel);

bool TextSelect_isActive(const TextSelect *sel);
bool TextSelect_isHovered(const TextSelect *sel);
bool TextSelect_setHovered(TextSelect *sel, bool flag);

void TextSelect_begin(TextSelect *sel, int32_t idx);
bool TextSelect_drag(TextSelect *sel, int32_t idx);
bool TextSelect_end(TextSelect *sel, int32_t *outLo, int32_t *outHi);
void TextSelect_cancel(TextSelect *sel);
bool TextSelect_getSpan(const TextSelect *sel, int32_t *outLo, int32_t *outHi);

int32_t TextSelect_getAnchor(const TextSelect *sel);
int32_t TextSelect_getActive(const TextSelect *sel);

#endif // DARLING_TEXT_SELECT_H