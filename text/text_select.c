#include "text/text_select.h"

#include <stddef.h>

#include "annotation/definition.h"
#include "annotation/overview.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: TextSelect
 * ============================================================================
 * Shared text-highlight part embedded by value in every text-handling
 * container (Label, RichLabel, MarkdownPanel) so all three behave
 * identically: DOWN anchors, DRAG moves only the active edge (backward then
 * forward past the anchor selects exactly [anchor, active] — never a rolling
 * union), UP orders and COMMITS a nonzero range as the new fixed selection
 * (or collapses a plain click and clears entirely), PTR_CANCEL clears. The
 * part stores byte offsets only — geometry-to-index mapping stays in the
 * owning class (its charIndexAt hands the part indices), and the owning
 * class renders the resulting span onto its own highlight raster. TextSelect
 * is plain state: it never allocates, never touches panels, windows, or the
 * arena, and never blocks — the hot-path contract is one nullptr guard, and
 * ordered span outputs are dest-last per the Dest-Last Law.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: TextSelect (shared embedded part; no parents)
 * LEVEL: L2 — Behavior (text selection state machine, zero allocation)
 * ============================================================================
 * Shared text-highlight part embedded by value in every text-handling
 * container (Label, RichLabel, MarkdownPanel). It owns the fixed-anchor
 * selection state machine and the hover caret-cursor lifecycle so all three
 * containers behave identically: DOWN anchors, DRAG moves only the active
 * edge (backward then forward past the anchor selects exactly
 * [anchor, active] — never a rolling union), UP orders and COMMITS a nonzero
 * range as the new fixed selection (or collapses a plain click and clears
 * entirely), PTR_CANCEL clears. Hovered flag drives the
 * I-beam cursor lifecycle; hovering never touches anchor/active and LEAVE
 * never clears an in-progress selection.
 *
 * The part stores byte offsets only — geometry-to-index mapping stays in the
 * owning class (its charIndexAt hands the part indices), and the owning
 * class renders the resulting span onto its own highlight raster. TextSelect
 * is plain state: it never allocates, never touches panels, windows, or the
 * arena, and never blocks (Rule 35 hot-path contract: one nullptr guard).
 *
 * STRUCT FIELDS (Mirroring text/text_select.h — exactly this file's class):
 * ----------------------------------------------------------------------------
 *   int32_t anchor;    // Fixed press edge (byte offset; -1 = inactive)
 *   int32_t active;    // Live drag edge (byte offset; -1 = inactive)
 *   bool    hovered;   // Pointer inside the bounds (caret-cursor lifecycle)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - TextSelect_default()                     : fresh part, anchor/active -1
 *
 * Core Functions:
 *   - TextSelect_begin(sel, idx)               : down — anchor + active collapse
 *   - TextSelect_drag(sel, idx)                : move the active edge only; true iff the edge moved
 *   - TextSelect_end(sel, outLo, outHi)        : order + commit, dest-last
 *   - TextSelect_cancel(sel)                   : clear without committing
 *   - TextSelect_getSpan(sel, outLo, outHi)    : ordered span while active
 *
 * Setters:
 *   - TextSelect_setHovered(sel, flag)         : hover caret-cursor lifecycle
 *
 * Getters:
 *   - TextSelect_isActive(sel)
 *   - TextSelect_isHovered(sel)
 *   - TextSelect_getAnchor(sel)
 *   - TextSelect_getActive(sel)
 * ============================================================================
 */

// CONSTRUCTORS
// ============================================================================

TextSelect TextSelect_default(void) {
    TextSelect sel;
    sel.anchor = -1;
    sel.active = -1;
    sel.hovered = false;
    return sel;
}

// CORE FUNCTIONS
// ============================================================================

void TextSelect_reset(TextSelect *sel) {
    if (!sel)
        return;
    TextSelect fresh = TextSelect_default();
    (*sel) = fresh;
}

bool TextSelect_isActive(const TextSelect *sel) {
    if (!sel)
        return false;
    return (*sel).anchor >= 0 && (*sel).active >= 0;
}

void TextSelect_begin(TextSelect *sel, int32_t idx) {
    if (!sel || idx < 0)
        return;
    (*sel).anchor = idx;
    (*sel).active = idx;
}

bool TextSelect_drag(TextSelect *sel, int32_t idx) {
    if (!sel)
        return false;
    if (!TextSelect_isActive(sel))
        return false;
    if (idx < 0)
        idx = 0;
    if ((*sel).active == idx)
        return false;
    (*sel).active = idx;
    return true;
}

bool TextSelect_end(TextSelect *sel, int32_t *outLo, int32_t *outHi) {
    if (outLo)
        *outLo = -1;
    if (outHi)
        *outHi = -1;
    if (!sel)
        return false;
    if (!TextSelect_isActive(sel))
        return false;
    int32_t a = (*sel).anchor;
    int32_t b = (*sel).active;
    int32_t lo = a < b ? a : b;
    int32_t hi = a < b ? b : a;
    // A nonzero range COMMITS: the ordered span becomes the new fixed
    // selection and stays readable through getSpan until the next begin or
    // cancel. A collapsed range (plain click) clears entirely.
    if (hi <= lo) {
        (*sel).anchor = -1;
        (*sel).active = -1;
    } else {
        (*sel).anchor = lo;
        (*sel).active = hi;
    }
    if (outLo)
        *outLo = lo;
    if (outHi)
        *outHi = hi;
    return hi > lo;
}

void TextSelect_cancel(TextSelect *sel) {
    if (!sel)
        return;
    (*sel).anchor = -1;
    (*sel).active = -1;
}

bool TextSelect_getSpan(const TextSelect *sel, int32_t *outLo, int32_t *outHi) {
    if (outLo)
        *outLo = -1;
    if (outHi)
        *outHi = -1;
    if (!sel)
        return false;
    if (!TextSelect_isActive(sel))
        return false;
    int32_t a = (*sel).anchor;
    int32_t b = (*sel).active;
    if (outLo)
        *outLo = a < b ? a : b;
    if (outHi)
        *outHi = a < b ? b : a;
    return true;
}

// SETTERS
// ============================================================================

bool TextSelect_setHovered(TextSelect *sel, bool flag) {
    if (!sel)
        return false;
    if ((*sel).hovered == flag)
        return false;
    (*sel).hovered = flag;
    return true;
}

// GETTERS
// ============================================================================

bool TextSelect_isHovered(const TextSelect *sel) {
    if (!sel)
        return false;
    return (*sel).hovered;
}

int32_t TextSelect_getAnchor(const TextSelect *sel) {
    if (!sel)
        return -1;
    return (*sel).anchor;
}

int32_t TextSelect_getActive(const TextSelect *sel) {
    if (!sel)
        return -1;
    return (*sel).active;
}