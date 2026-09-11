#ifndef DARLING_MARKDOWN_PANEL_H
#define DARLING_MARKDOWN_PANEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "c23/constructor.h"
#include "darling/panel/list_container.h"
#include "darling/panel/panel.h"
#include "font/font.h"
#include "oop/type.h"

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

// Symmetric Getters (Java-library standard)
const char *MarkdownPanel_getText(const MarkdownPanel *s);
Font *MarkdownPanel_getFont(const MarkdownPanel *s);
uint32_t MarkdownPanel_getCodeBackground(const MarkdownPanel *s);
Panel *MarkdownPanel_getRows(const MarkdownPanel *s);
float MarkdownPanel_getRowSpacing(const MarkdownPanel *s);
size_t MarkdownPanel_getRowCount(const MarkdownPanel *s);
Panel *MarkdownPanel_getRow(const MarkdownPanel *s, size_t index);

#endif
