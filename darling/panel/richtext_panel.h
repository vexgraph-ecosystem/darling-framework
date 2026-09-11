#ifndef DARLING_RICHTEXT_PANEL_H
#define DARLING_RICHTEXT_PANEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "c23/constructor.h"
#include "darling/panel/panel.h"
#include "oop/type.h"
#include "text/rich_text.h"

// darling/panel/richtext_panel.h — rich-string document panel
// (a Panel aliasing a caller-owned RichText source laid out to maxWidth).
//
// The source is aliased, never owned: setSource re-runs RichText_layout on
// the caller's model (relayout-on-attach) and resizes the panel height to
// the laid-out content height. Pair with a ScrollContainer via contentHeight.

// Central registry owns these once landed; the guard keeps this shell
// compiling standalone until then.


typedef struct RichTextPanel {
    Panel base;
    RichText *source;
    float maxWidth;
} RichTextPanel;

// Constructors:
//   RichTextPanel()              — no source, zero width
//   RichTextPanel(rt, maxWidth)  — laid out immediately
RichTextPanel *RichTextPanel_0(void);
RichTextPanel *RichTextPanel_2(RichText *rt, float maxWidth);

#define RichTextPanel(...) CONSTRUCTOR_DISPATCH(RichTextPanel, __VA_ARGS__)

void RichTextPanel_free(RichTextPanel *s);

void RichTextPanel_setSource(RichTextPanel *s, RichText *rt);
void RichTextPanel_setMaxWidth(RichTextPanel *s, float maxWidth);
void RichTextPanel_setLocation(RichTextPanel *s, float x, float y);
void RichTextPanel_setSize(RichTextPanel *s, float w, float h);
void RichTextPanel_setBackgroundColor(RichTextPanel *s, uint32_t color);

// Symmetric Getters (Java-library standard)
RichText *RichTextPanel_getSource(const RichTextPanel *s);
float RichTextPanel_getMaxWidth(const RichTextPanel *s);
float RichTextPanel_contentHeight(const RichTextPanel *s);

#endif
