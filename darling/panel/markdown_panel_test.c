#include "annotation/overview.h"

#include <stdio.h>
#include <string.h>

#include "darling/panel/markdown_panel.h"
#include "darling/label/label.h"
#include "nio/mem.h"

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: MarkdownPanelTest (darling/panel/markdown_panel_test.c)
 * LEVEL: L3 — Module Code (headless verification harness)
 * ============================================================================
 * Headless suite for MarkdownPanel document selection over its stacked Label
 * rows (no Font, so every row is a Label). Proves the pointer seam end to
 * end: document index mapping in rendered-text space, fixed-anchor forward
 * and backward drags, cross-row span aggregation, click-to-clear, outside
 * click, hard unhighlightable lock, programmatic selection, highlight color
 * roundtrip, and getter null-safety.
 *
 * Source: "# Title\nsome paragraph\n- bullet one\n- **bullet two**\n```\ncode line\n```"
 * Rows (visible bytes, cell base, height at 400pt wide):
 *   row0 Label "Title"           cells [0,5)   h 37.80
 *   row1 Label "some paragraph"  cells [5,19)  h 18.90
 *   row2 Label "• bullet one"    cells [19,33) h 18.90
 *   row3 Label "• bullet two"    cells [33,47) h 18.90
 *   row4 Label "code line"       cells [47,56) h 16.20
 * Y bands add rowSpacing 4 (row0 0..37.8, row1 41.8..60.7, row2 64.7..83.6,
 * row3 87.6..106.5, row4 110.5..126.7). LocalX maps by ratio x/400 * textLen.
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
    if (cond) { printf("[markdown_panel_test] PASS %s\n", name); } \
    else { printf("[markdown_panel_test] FAIL %s\n", name); g_failures++; } \
} while (0)

static void rowSel(MarkdownPanel *mp, size_t i, int32_t *outStart, int32_t *outEnd) {
    Panel *rows = MarkdownPanel_getRows(mp);
    Panel *row = Panel_getChild(rows, (int32_t) i);
    Label_getSelection((const Label*) row, outStart, outEnd);
}

int main(void) {
    MarkdownPanel *mp = MarkdownPanel_0();
    CHECK("constructor", mp != NULL);

    MarkdownPanel_setSize(mp, 400.0f, 400.0f);
    MarkdownPanel_setText(mp, "# Title\nsome paragraph\n- bullet one\n- **bullet two**\n```\ncode line\n```");

    // 5 rows, cell starts [0,5,19,33,47], text lens [5,14,14,14,9].
    CHECK("row count", MarkdownPanel_getRowCount(mp) == 5);
    CHECK("not highlightable by default", !MarkdownPanel_isHighlightable(mp));

    int32_t s0 = 0, s1 = 0;
    MarkdownPanel_getSelection(mp, &s0, &s1);
    CHECK("selection empty initially", s0 == -1 && s1 == -1);

    MarkdownPanel_setHighlightable(mp, true);
    CHECK("highlightable now", MarkdownPanel_isHighlightable(mp));

    int32_t r0 = 0, r1 = 0;
    int32_t sel0 = 0, sel1 = 0;

    // Forward drag across two rows: down mid-row0, active edge lands in row1.
    MarkdownPanel_handlePointer(mp, PTR_DOWN, 100.0f, 20.0f);
    MarkdownPanel_handlePointer(mp, PTR_DRAG, 200.0f, 50.0f);
    MarkdownPanel_getSelection(mp, &sel0, &sel1);
    CHECK("forward drag doc range", sel0 == 1 && sel1 == 12);
    rowSel(mp, 0, &r0, &r1);
    CHECK("forward drag row0 span", r0 == 1 && r1 == 5);
    rowSel(mp, 1, &r0, &r1);
    CHECK("forward drag row1 span", r0 == 0 && r1 == 7);
    MarkdownPanel_handlePointer(mp, PTR_UP, 200.0f, 50.0f);
    MarkdownPanel_getSelection(mp, &sel0, &sel1);
    CHECK("release keeps forward selection", sel0 == 1 && sel1 == 12);

    // Backward drag: anchor row3, active edge pulled back to row0.
    MarkdownPanel_handlePointer(mp, PTR_DOWN, 300.0f, 100.0f);
    rowSel(mp, 3, &r0, &r1);
    MarkdownPanel_getSelection(mp, &sel0, &sel1);
    CHECK("backward anchor mid-row3", sel0 == sel1 && sel0 == 44);
    MarkdownPanel_handlePointer(mp, PTR_DRAG, 50.0f, 10.0f);
    MarkdownPanel_getSelection(mp, &sel0, &sel1);
    CHECK("backward drag ordered doc range", sel0 == 1 && sel1 == 44);
    rowSel(mp, 0, &r0, &r1);
    CHECK("backward drag row0 span", r0 == 1 && r1 == 5);
    rowSel(mp, 1, &r0, &r1);
    CHECK("backward drag row1 full", r0 == 0 && r1 == 14);
    rowSel(mp, 2, &r0, &r1);
    CHECK("backward drag row2 full", r0 == 0 && r1 == 14);
    rowSel(mp, 3, &r0, &r1);
    CHECK("backward drag row3 up to anchor", r0 == 0 && r1 == 11);
    rowSel(mp, 4, &r0, &r1);
    CHECK("backward drag row4 untouched", r0 == -1 && r1 == -1);

    // Fixed anchor survives a forward pass past it.
    MarkdownPanel_handlePointer(mp, PTR_DRAG, 200.0f, 120.0f);
    MarkdownPanel_getSelection(mp, &sel0, &sel1);
    CHECK("forward past anchor keeps anchor", sel0 == 44 && sel1 == 52);
    rowSel(mp, 4, &r0, &r1);
    CHECK("pulled row4 tail selected", r0 == 0 && r1 == 5);
    MarkdownPanel_handlePointer(mp, PTR_UP, 200.0f, 120.0f);

    // A plain click collapses to no selection.
    MarkdownPanel_handlePointer(mp, PTR_DOWN, 200.0f, 200.0f);
    MarkdownPanel_handlePointer(mp, PTR_UP, 200.0f, 200.0f);
    MarkdownPanel_getSelection(mp, &sel0, &sel1);
    CHECK("click clears selection", sel0 == -1 && sel1 == -1);
    rowSel(mp, 1, &r0, &r1);
    CHECK("click clears rows", r0 == -1 && r1 == -1);

    // Outside click (left of panel) clears.
    MarkdownPanel_handlePointer(mp, PTR_DOWN, -20.0f, 40.0f);
    MarkdownPanel_handlePointer(mp, PTR_UP, -20.0f, 40.0f);
    MarkdownPanel_handlePointer(mp, PTR_DOWN, 100.0f, 20.0f);
    MarkdownPanel_getSelection(mp, &sel0, &sel1);
    CHECK("down anchors after outside reset", sel0 == sel1 && sel0 == 1);
    MarkdownPanel_handlePointer(mp, PTR_UP, 100.0f, 20.0f);
    MarkdownPanel_getSelection(mp, &sel0, &sel1);
    CHECK("outside click cleared last session", sel0 == -1 && sel1 == -1);

    // Hard lock: unhighlightable ignores pointer events.
    MarkdownPanel_setHighlightable(mp, false);
    MarkdownPanel_handlePointer(mp, PTR_DOWN, 100.0f, 20.0f);
    MarkdownPanel_handlePointer(mp, PTR_DRAG, 200.0f, 50.0f);
    MarkdownPanel_getSelection(mp, &sel0, &sel1);
    CHECK("unhighlightable swallows pointer", sel0 == -1 && sel1 == -1);
    MarkdownPanel_setHighlightable(mp, true);

    // Programmatic document-wide selection mirrors every row.
    MarkdownPanel_setSelection(mp, 5, 47);
    MarkdownPanel_getSelection(mp, &sel0, &sel1);
    CHECK("programmatic range stored", sel0 == 5 && sel1 == 47);
    rowSel(mp, 1, &r0, &r1);
    CHECK("programmatic row1 full", r0 == 0 && r1 == 14);
    rowSel(mp, 3, &r0, &r1);
    CHECK("programmatic row3 full", r0 == 0 && r1 == 14);
    rowSel(mp, 4, &r0, &r1);
    CHECK("programmatic row4 outside range", r0 == -1 && r1 == -1);

    MarkdownPanel_setSelection(mp, 30, 10);
    MarkdownPanel_getSelection(mp, &sel0, &sel1);
    CHECK("getSelection orders reversed input", sel0 == 10 && sel1 == 30);

    // Highlight color roundtrip (packs AARRGGBB).
    MarkdownPanel_setHighlightColorRGBA(mp, 255, 0, 0, 255);
    CHECK("packed color", MarkdownPanel_getHighlightColor(mp) == 0xFFFF0000u);
    uint8_t r = 0, g = 0, b = 0, a = 0;
    MarkdownPanel_getHighlightColorRGBA(mp, &r, &g, &b, &a);
    CHECK("color channel rd", r == 255 && g == 0 && b == 0 && a == 255);

    // Null-safety (Rule 35).
    MarkdownPanel_getSelection(NULL, &s0, &s1);
    CHECK("null getSelection safe", s0 == -1 && s1 == -1);
    CHECK("null isHighlightable", !MarkdownPanel_isHighlightable(NULL));
    CHECK("null getHighlightColor", MarkdownPanel_getHighlightColor(NULL) == 0u);
    rowSel(mp, 1, &r0, &r1);
    CHECK("row1 span before null call", r0 == 5 && r1 == 14);
    MarkdownPanel_handlePointer(NULL, PTR_DOWN, 100.0f, 20.0f);
    rowSel(mp, 1, &r0, &r1);
    CHECK("null handlePointer leaves selection", r0 == 5 && r1 == 14);

    MarkdownPanel_free(mp);

    if (g_failures == 0)
        printf("\n=== MarkdownPanel Test Summary: 0 failures ===\n");
    else
        printf("\n=== MarkdownPanel Test Summary: %d failures ===\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}