#ifndef DARLING_MARKDOWN_PANEL_H
#define DARLING_MARKDOWN_PANEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "c23/constructor.h"
#include "darling/panel/list_container.h"
#include "darling/panel/panel.h"
#include "event/keyevent.h"
#include "font/font.h"
#include "oop/type.h"
#include "text/text_core.h"
#include "text/text_select.h"

// darling/panel/markdown_panel.h — markdown-fed document panel
// (a Panel owning a row container rebuilt from a markdown source string).
//
// v1 syntax: #/##/### headings, -/* bullets, `inline code`, fenced blocks,
// **bold**/*italic* via RichText styles. Rebuild is detach-all + re-layout
// (document panels are cold paths). Rows live in a ListContainer (vertical,
// spacing mirrors rowSpacing); getRows returns its Panel base.

// Central registry owns these once landed; the guard keeps this shell
// compiling standalone until then.


struct MarkdownRowSlot;

typedef struct MarkdownPanel {
    Panel base;
    uint8_t *textBlock;
    Font *font;
    uint32_t codeBackground;
    ListContainer *rows;
    float rowSpacing;
    struct MarkdownRowSlot *slots;
    size_t rowCount;
    size_t rowCapacity;
    bool highlightable;
    TextSelect select;      // shared fixed-anchor selection part (doc-byte space)
    uint32_t highlightColor;
    TextAlign textAlign;
    float spacingWidth;
    float spacingHeight;
    bool ligatures;
} MarkdownPanel;

// Constructors:
//   MarkdownPanel()       — empty document, no text
//   MarkdownPanel(text)   — scanned and laid out immediately
MarkdownPanel *MarkdownPanel_0(void);
MarkdownPanel *MarkdownPanel_1(const char *text);

#define MarkdownPanel(...) CONSTRUCTOR_DISPATCH(MarkdownPanel, __VA_ARGS__)

void MarkdownPanel_free(MarkdownPanel *s);

void MarkdownPanel_setText(MarkdownPanel *s, const char *text);
void MarkdownPanel_setFont(MarkdownPanel *s, Font *font);
void MarkdownPanel_setCodeBackground(MarkdownPanel *s, uint32_t color);
void MarkdownPanel_setRowSpacing(MarkdownPanel *s, float spacing);
void MarkdownPanel_setLocation(MarkdownPanel *s, float x, float y);
void MarkdownPanel_setSize(MarkdownPanel *s, float w, float h);
void MarkdownPanel_setBackgroundColor(MarkdownPanel *s, uint32_t color);
void MarkdownPanel_setHighlightable(MarkdownPanel *s, bool flag);
void MarkdownPanel_setSelection(MarkdownPanel *s, int32_t start, int32_t end);
void MarkdownPanel_setHighlightColor(MarkdownPanel *s, uint32_t color);
void MarkdownPanel_setHighlightColorRGBA(MarkdownPanel *s, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void MarkdownPanel_setTextAlign(MarkdownPanel *s, TextAlign align);
void MarkdownPanel_setSpacingWidth(MarkdownPanel *s, float width);
void MarkdownPanel_setSpacingHeight(MarkdownPanel *s, float height);
void MarkdownPanel_setLigatures(MarkdownPanel *s, bool flag);

// Symmetric Getters (Java-library standard)
const char *MarkdownPanel_getText(const MarkdownPanel *s);
Font *MarkdownPanel_getFont(const MarkdownPanel *s);
uint32_t MarkdownPanel_getCodeBackground(const MarkdownPanel *s);
Panel *MarkdownPanel_getRows(const MarkdownPanel *s);
float MarkdownPanel_getRowSpacing(const MarkdownPanel *s);
size_t MarkdownPanel_getRowCount(const MarkdownPanel *s);
Panel *MarkdownPanel_getRow(const MarkdownPanel *s, size_t index);
bool MarkdownPanel_isHighlightable(const MarkdownPanel *s);
void MarkdownPanel_getSelection(const MarkdownPanel *s, int32_t *outStart, int32_t *outEnd);
uint32_t MarkdownPanel_getHighlightColor(const MarkdownPanel *s);
void MarkdownPanel_getHighlightColorRGBA(const MarkdownPanel *s, uint8_t *outR, uint8_t *outG, uint8_t *outB, uint8_t *outA);
TextAlign MarkdownPanel_getTextAlign(const MarkdownPanel *s);
float MarkdownPanel_getSpacingWidth(const MarkdownPanel *s);
float MarkdownPanel_getSpacingHeight(const MarkdownPanel *s);
bool MarkdownPanel_hasLigatures(const MarkdownPanel *s);

// Pointer seam (driven by event/dispatch when the panel owns a document's
// text rows): down anchors, drag moves the active edge, up orders and
// COMMITS a nonzero range (or collapses a plain click). The extra window
// argument carries the OS window for the hover I-beam caret-cursor
// lifecycle (nullptr in headless tests). Coordinates are local to the panel.
void MarkdownPanel_handlePointer(MarkdownPanel *s, int32_t kind, float localX, float localY, void *window);

// Key seam: Cmd/Ctrl+C copies the committed selection; Cmd/Ctrl+V pastes the
// clipboard into the document (whole-document text replacement, cold rebuild).
void MarkdownPanel_handleKey(MarkdownPanel *s, const UIKeyEvent *ev);

// Display-accurate plain copy of the committed selection (arena-allocated):
// label rows slice their rendered string (bullets included when selected),
// rich rows emit tag-stripped plain text.
char *MarkdownPanel_getSelectedText(const MarkdownPanel *s);

#endif
