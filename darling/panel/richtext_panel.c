#include "darling/panel/richtext_panel.h"

#include "darling/panel/panel.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include "nio/mem.h"
#include "oop/type.h"
#include "text/rich_text.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: RichTextPanel
 * ============================================================================
 * Document panel aliasing a caller-owned RichText source — the styled-span
 * model: setters drive the real RichText_setString/setStyle/layout
 * pipeline, and layout is never reimplemented here. setSource and
 * setMaxWidth re-run RichText_layout on the aliased source
 * (relayout-on-attach) and resize the panel height to the laid-out height;
 * contentHeight exposes that height for ScrollContainer pairing. The source
 * is aliased, never owned or freed (RichTextPanel_free only clears the
 * pointer and frees the panel itself); maxWidth clamps to >= 0. Per the
 * Container-vs-Panel Law it embeds Panel as its first member and inherits
 * the layout/tree/background state, with symmetric getters/setters over
 * the two state fields.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: RichTextPanel (embeds Panel)
 * LEVEL: L2 — Behavior (rich-string document panel behavior API)
 * ============================================================================
 * Document panel aliasing a caller-owned RichText source. Setters drive the
 * real RichText_setString/setStyle/layout pipeline — layout is never
 * reimplemented here. setSource and setMaxWidth re-run RichText_layout on
 * the aliased source (relayout-on-attach) and resize the panel height to
 * the laid-out height; contentHeight exposes that height for ScrollContainer
 * pairing. The source is aliased, never owned or freed.
 *
 * STRUCT FIELDS (Mirroring darling/panel/richtext_panel.h):
 * ----------------------------------------------------------------------------
 *   Panel base;                        // Inherited layout/tree/background state
 *   RichText *source;                  // Aliased styled-text model; nullable
 *   float maxWidth;                    // Wrap width handed to RichText_layout
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - RichTextPanel_0(void)
 *   - RichTextPanel_2(rt, maxWidth)
 *
 * Core Functions:
 *   - RichTextPanel_free(s)
 *
 * Setters:
 *   - RichTextPanel_setSource(s, rt)
 *   - RichTextPanel_setMaxWidth(s, maxWidth)
 *   - RichTextPanel_setLocation(s, x, y)
 *   - RichTextPanel_setSize(s, w, h)
 *   - RichTextPanel_setBackgroundColor(s, color)
 *
 * Getters:
 *   - RichTextPanel_getSource(s)
 *   - RichTextPanel_getMaxWidth(s)
 *   - RichTextPanel_contentHeight(s)
 * ============================================================================
 */

// CONSTRUCTORS
// ============================================================================

RichTextPanel *RichTextPanel_0(void) {
    RichTextPanel *s = (RichTextPanel*) Memory_alloc(TYPE_RICHTEXT_PANEL_SINGLETON, sizeof(RichTextPanel));
    if (!s)
        return nullptr;
    Panel *b = Panel_0();
    if (!b) {
        Memory_free(s);
        return nullptr;
    }
    (*s).base = (*b);
    Memory_free(b);
    (*s).source = nullptr;
    (*s).maxWidth = 0.0f;
    return s;
}

RichTextPanel *RichTextPanel_2(RichText *rt, float maxWidth) {
    RichTextPanel *s = RichTextPanel_0();
    if (!s)
        return nullptr;
    if (maxWidth < 0.0f)
        maxWidth = 0.0f;
    (*s).maxWidth = maxWidth;
    RichTextPanel_setSource(s, rt);
    return s;
}

// CORE FUNCTIONS
// ============================================================================

static void markDirty(RichTextPanel *s) {
    (void) s;
    (void) 0;
}

static void relayout(RichTextPanel *s) {
    if (!s)
        return;
    RichText *src = (*s).source;
    if (!src)
        return;
    RichText_layout(src, (*s).maxWidth);
    Panel *b = &(*s).base;
    Component *c = &(*b).component;
    float w = Component_getWidth(c);
    if (w < 0.0f)
        w = 0.0f;
    Panel_setSize(b, w, (*src).layoutHeight);
    markDirty(s);
}

void RichTextPanel_free(RichTextPanel *s) {
    if (!s)
        return;
    (*s).source = nullptr;
    Memory_free(s);
}

// SETTERS
// ============================================================================

void RichTextPanel_setSource(RichTextPanel *s, RichText *rt) {
    if (!s)
        return;
    (*s).source = rt;
    relayout(s);
}

void RichTextPanel_setMaxWidth(RichTextPanel *s, float maxWidth) {
    if (!s)
        return;
    if (maxWidth < 0.0f)
        maxWidth = 0.0f;
    (*s).maxWidth = maxWidth;
    relayout(s);
}

void RichTextPanel_setLocation(RichTextPanel *s, float x, float y) {
    if (!s)
        return;
    Panel *b = &(*s).base;
    Panel_setLocation(b, x, y);
}

void RichTextPanel_setSize(RichTextPanel *s, float w, float h) {
    if (!s)
        return;
    Panel *b = &(*s).base;
    Panel_setSize(b, w, h);
}

void RichTextPanel_setBackgroundColor(RichTextPanel *s, uint32_t color) {
    if (!s)
        return;
    Panel *b = &(*s).base;
    Panel_setBackgroundColor(b, color);
}

// GETTERS
// ============================================================================

RichText *RichTextPanel_getSource(const RichTextPanel *s) {
    return s ? (*s).source : nullptr;
}

float RichTextPanel_getMaxWidth(const RichTextPanel *s) {
    return s ? (*s).maxWidth : 0.0f;
}

float RichTextPanel_contentHeight(const RichTextPanel *s) {
    if (!s || !(*s).source)
        return 0.0f;
    RichText *src = (*s).source;
    return (*src).layoutHeight;
}
