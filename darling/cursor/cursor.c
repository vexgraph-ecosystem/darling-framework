#include "annotation/definition.h"
#include "annotation/overview.h"
#include "darling/cursor/cursor.h"
#include "window/window.h"
#include <stdlib.h>

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Cursor
 * ============================================================================
 * Standard mouse cursor presentation API bridging darling to native OS window
 * cursors: a style enum plus a reserved custom-data pointer, applied to an R1
 * Window via Window_setCursorType. Eight predefined styles live in a static
 * singleton table — Cursor_getPredefined returns a borrowed pointer and
 * Cursor_free refuses to free it; heap-allocated instances are freed normally.
 * Null-safe getters return CURSOR_DEFAULT. A thin R4-to-R1 seam with zero
 * steady-state allocation.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Cursor
 * LEVEL: L2 — Behavior (standard mouse cursor presentation API)
 * ============================================================================
 * Represents a mouse cursor style, bridging to native OS window cursors.
 *
 * STRUCT FIELDS (Mirroring darling/cursor/cursor.h):
 * ----------------------------------------------------------------------------
 *   int type;            // Cursor style enum (CURSOR_DEFAULT, CURSOR_IBEAM, etc.)
 *   void *customData;    // Reserved pointer for custom cursor icon data
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Cursor()                                : Cursor_0()
 *   - Cursor(type)                            : Cursor_1(type)
 *   - Cursor_getPredefined(type)
 *
 * Core Functions:
 *   - Cursor_apply(cursor, window)
 *   - Cursor_free(cursor)
 *
 * Setters:
 *   - Cursor_setType(cursor, type)
 *   - Cursor_setCustomData(cursor, customData)
 *
 * Getters:
 *   - Cursor_getType(cursor)
 *   - Cursor_getCustomData(cursor)
 * ============================================================================
 */

// ============================================================================
// PREDEFINED SINGLETONS
// ============================================================================

static Cursor s_predefined[8] = {
    { .type = CURSOR_DEFAULT,       .customData = nullptr },
    { .type = CURSOR_IBEAM,         .customData = nullptr },
    { .type = CURSOR_POINTING_HAND, .customData = nullptr },
    { .type = CURSOR_CROSSHAIR,     .customData = nullptr },
    { .type = CURSOR_RESIZE_EW,     .customData = nullptr },
    { .type = CURSOR_RESIZE_NS,     .customData = nullptr },
    { .type = CURSOR_NOT_ALLOWED,   .customData = nullptr },
    { .type = CURSOR_HIDDEN,        .customData = nullptr },
};

// ============================================================================
// CONSTRUCTORS
// ============================================================================

Cursor *Cursor_0(void) {
    return Cursor_1(CURSOR_DEFAULT);
}

Cursor *Cursor_1(int type) {
    Cursor *c = (Cursor*) malloc(sizeof(Cursor));
    if (!c)
        return nullptr;
    (*c).type = type;
    (*c).customData = nullptr;
    return c;
}

Cursor *Cursor_getPredefined(int type) {
    if (type >= 0 && type < 8)
        return &s_predefined[type];
    return &s_predefined[CURSOR_DEFAULT];
}

// ============================================================================
// CORE FUNCTIONS
// ============================================================================

void Cursor_apply(const Cursor *cursor, void *window) {
    if (!cursor || !window)
        return;
    Window_setCursorType((Window*) window, (WindowCursorType) (*cursor).type);
}

void Cursor_free(Cursor *cursor) {
    if (!cursor)
        return;
    if (cursor >= &s_predefined[0] && cursor <= &s_predefined[7])
        return;
    free(cursor);
}

// ============================================================================
// SETTERS
// ============================================================================

void Cursor_setType(Cursor *cursor, int type) {
    if (!cursor)
        return;
    (*cursor).type = type;
}

void Cursor_setCustomData(Cursor *cursor, void *customData) {
    if (!cursor)
        return;
    (*cursor).customData = customData;
}

// ============================================================================
// GETTERS
// ============================================================================

int Cursor_getType(const Cursor *cursor) {
    if (!cursor)
        return CURSOR_DEFAULT;
    return (*cursor).type;
}

void *Cursor_getCustomData(const Cursor *cursor) {
    if (!cursor)
        return nullptr;
    return (*cursor).customData;
}
