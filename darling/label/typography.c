#include "darling/label/typography.h"

#include <string.h>

#include "annotation/definition.h"
#include "annotation/overview.h"
#include "nio/mem.h"
#include "oop/type.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Typography
 * ============================================================================
 * Role-styled display text node embedding Panel: an owned UTF-8 string plus
 * an optional font descriptor, rendered with a packed 0xAARRGGBB color under
 * one of five typographic roles (H1, H2, H3, Body, Caption). The node is
 * arena-allocated (TYPE_TYPOGRAPHY_SINGLETON) and copies its string into the
 * arena on set, so it never borrows caller text; the font is a borrowed
 * descriptor. Every setter marks the embedded Container dirty for the next
 * layout pass. Like Kbd, Typography is a pure display node in the label
 * family — no core logic, only presentation state.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Typography (embeds Panel)
 * LEVEL: L2 — Behavior (role-styled display text behavior API)
 * ============================================================================
 * Role-styled display text node with an owned string and optional font.
 *
 * STRUCT FIELDS (Mirroring darling/label/typography.h):
 * ----------------------------------------------------------------------------
 *   Panel base;     // Inherited layout, bounds, and hierarchy state
 *   char *text;     // Owned UTF-8 string payload
 *   int32_t role;   // 0 means H1, 1 means H2, 2 means H3, 3 Body, 4 Caption
 *   Font *font;     // Optional font descriptor
 *   uint32_t color; // Packed text color, 0xAARRGGBB
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Typography_0(void)
 *   - Typography_1(text)
 *   - Typography_2(parent, text)
 *
 * Core Functions:
 *   - (none: pure display node)
 *
 * Setters:
 *   - Typography_setText(t, text)
 *   - Typography_setRole(t, role)
 *   - Typography_setFont(t, font)
 *   - Typography_setColor(t, color)
 *   - Typography_free(t)
 *
 * Getters:
 *   - Typography_getText(t)
 *   - Typography_getRole(t)
 *   - Typography_getFont(t)
 *   - Typography_getColor(t)
 * ============================================================================
 */

// CONSTRUCTORS

Typography *Typography_0(void) {
    Typography *t = (Typography*) Memory_alloc(TYPE_TYPOGRAPHY_SINGLETON, sizeof(Typography));
    if (!t)
        return nullptr;
    Panel *base = Panel_0();
    if (!base) {
        Memory_free(t);
        return nullptr;
    }
    (*t).base = (*base);
    Memory_free(base);
    (*t).text = nullptr;
    (*t).role = TYPOGRAPHY_BODY;
    (*t).font = nullptr;
    (*t).color = 0xFFFFFFFFu;
    return t;
}

Typography *Typography_1(const char *text) {
    Typography *t = Typography_0();
    if (t)
        Typography_setText(t, text);
    return t;
}

Typography *Typography_2(Panel *parent, const char *text) {
    Typography *t = Typography_1(text);
    if (t && parent)
        Panel_addContainer(parent, &(*t).base);
    return t;
}

// CORE FUNCTIONS

// (none: pure display node)

// SETTERS

static void markDirty(Typography *t) {
    if (!t)
        return;
    Panel *b = &(*t).base;
    (void) b;
}

void Typography_setText(Typography *t, const char *text) {
    if (!t)
        return;
    if ((*t).text)
        Memory_free((*t).text);
    (*t).text = nullptr;
    if (text) {
        size_t len = strlen(text) + 1;
        (*t).text = (char*) Memory_alloc(TYPE_ARRAY, len);
        if ((*t).text)
            strcpy((*t).text, text);
    }
    markDirty(t);
}

void Typography_setRole(Typography *t, int32_t role) {
    if (!t)
        return;
    (*t).role = role;
    markDirty(t);
}

void Typography_setFont(Typography *t, Font *font) {
    if (!t)
        return;
    (*t).font = font;
    markDirty(t);
}

void Typography_setColor(Typography *t, uint32_t color) {
    if (!t)
        return;
    (*t).color = color;
    markDirty(t);
}

void Typography_free(Typography *t) {
    if (!t)
        return;
    if ((*t).text)
        Memory_free((*t).text);
    (*t).text = nullptr;
    (*t).font = nullptr;
    Memory_free(t);
}

// GETTERS

const char *Typography_getText(const Typography *t) {
    return t ? (*t).text : nullptr;
}

int32_t Typography_getRole(const Typography *t) {
    return t ? (*t).role : TYPOGRAPHY_BODY;
}

Font *Typography_getFont(const Typography *t) {
    return t ? (*t).font : nullptr;
}

uint32_t Typography_getColor(const Typography *t) {
    return t ? (*t).color : 0;
}
