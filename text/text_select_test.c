#include "annotation/overview.h"

#include <stdio.h>
#include <string.h>

#include "text/text_select.h"

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: TextSelectTest (text/text_select_test.c)
 * LEVEL: L3 — Module Code (headless verification harness)
 * ============================================================================
 * Headless suite for the shared TextSelect part: fixed-anchor begin/drag
 * ordering, forward and backward drags, end-commit vs collapse, cancel,
 * hover state changes, zero-state safety, and null-safe getters.
 * Pure state machine — no panels, no arena, no window.
 *
 * STRUCT FIELDS: none — procedural test harness.
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - main(void)
 * ============================================================================
 */

static int g_failures = 0;

#define CHECK(name, cond) do { \
    if (cond) { printf("[text_select_test] PASS %s\n", name); } \
    else { printf("[text_select_test] FAIL %s\n", name); g_failures++; } \
} while (0)

int main(void) {
    TextSelect sel = TextSelect_default();
    CHECK("default inactive", !TextSelect_isActive(&sel));
    CHECK("default not hovered", !TextSelect_isHovered(&sel));
    CHECK("default anchor -1", TextSelect_getAnchor(&sel) == -1);
    CHECK("default active -1", TextSelect_getActive(&sel) == -1);

    int32_t lo = -2, hi = -2;
    CHECK("getSpan empty returns false", !TextSelect_getSpan(&sel, &lo, &hi));
    CHECK("end empty returns false", !TextSelect_end(&sel, &lo, &hi));

    TextSelect_begin(&sel, 4);
    CHECK("begin active", TextSelect_isActive(&sel));
    CHECK("begin collapsed", TextSelect_getAnchor(&sel) == 4 && TextSelect_getActive(&sel) == 4);

    CHECK("drag forward", TextSelect_drag(&sel, 9));
    CHECK("active edge moved", TextSelect_getActive(&sel) == 9);
    CHECK("anchor fixed", TextSelect_getAnchor(&sel) == 4);
    CHECK("span ordered forward", TextSelect_getSpan(&sel, &lo, &hi) && lo == 4 && hi == 9);

    CHECK("end commits range", TextSelect_end(&sel, &lo, &hi));
    CHECK("end lo/hi", lo == 4 && hi == 9);
    CHECK("committed span persists", TextSelect_getSpan(&sel, &lo, &hi) && lo == 4 && hi == 9);

    // Backward drag: anchor 6, drag to 2 → span [2,6).
    TextSelect_begin(&sel, 6);
    CHECK("drag backward", TextSelect_drag(&sel, 2));
    CHECK("span ordered backward", TextSelect_getSpan(&sel, &lo, &hi) && lo == 2 && hi == 6);
    CHECK("end ordered backward", TextSelect_end(&sel, &lo, &hi) && lo == 2 && hi == 6);

    // A new begin re-anchors (fresh drag replaces the committed span).
    TextSelect_begin(&sel, 11);
    CHECK("begin re-anchors active", TextSelect_getAnchor(&sel) == 11);
    TextSelect_cancel(&sel);
    CHECK("still inactive", !TextSelect_isActive(&sel));

    // Plain click collapses: end normalizes then reports zero-length.
    TextSelect_begin(&sel, 7);
    TextSelect_drag(&sel, 10);
    TextSelect_drag(&sel, 7);
    CHECK("collapse returns false", !TextSelect_end(&sel, &lo, &hi));
    CHECK("collapse span equal", lo == 7 && hi == 7);
    CHECK("collapse clears", !TextSelect_isActive(&sel));

    // Cancel clears without committing.
    TextSelect_begin(&sel, 3);
    TextSelect_drag(&sel, 8);
    TextSelect_cancel(&sel);
    CHECK("cancel clears", !TextSelect_isActive(&sel));
    CHECK("cancel span empty", !TextSelect_getSpan(&sel, &lo, &hi));

    // Hover lifecycle: set/repeat/clear + never touches anchor/active.
    TextSelect_begin(&sel, 5);
    CHECK("hover on changes", TextSelect_setHovered(&sel, true));
    CHECK("hover on repeat no change", !TextSelect_setHovered(&sel, true));
    CHECK("hover reflects", TextSelect_isHovered(&sel));
    CHECK("hover keeps anchor", TextSelect_getAnchor(&sel) == 5);
    CHECK("hover off changes", TextSelect_setHovered(&sel, false));
    CHECK("hover gone", !TextSelect_isHovered(&sel));
    TextSelect_cancel(&sel);

    // Zero-state safety.
    TextSelect *none = NULL;
    CHECK("null-safe isActive", !TextSelect_isActive(none));
    CHECK("null-safe drag", !TextSelect_drag(none, 3));
    CHECK("null-safe end", !TextSelect_end(none, &lo, &hi));
    CHECK("null-safe hover", !TextSelect_isHovered(none));
    CHECK("null-safe setHovered", !TextSelect_setHovered(none, true));
    CHECK("null-safe getters", TextSelect_getAnchor(none) == -1 && TextSelect_getActive(none) == -1);

    if (g_failures == 0) {
        printf("[text_select_test] ALL PASS\n");
        return 0;
    }
    printf("[text_select_test] %d FAILURES\n", g_failures);
    return 1;
}