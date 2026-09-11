#include "annotation/overview.h"

#include <stdio.h>
#include <string.h>

#include "darling/label/rich_label.h"
#include "darling/cursor/cursor.h"
#include "text/rich_text.h"
#include "nio/mem.h"

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: RichLabelTest (darling/label/rich_label_test.c)
 * LEVEL: L3 — Module Code (headless verification harness)
 * ============================================================================
 * Headless suite for RichLabel selection: char hit-testing against synthetic
 * glyph quads and fixed-anchor pointer drag selection (forward, backward,
 * backward-then-forward, click-to-clear, outside click).
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
    if (cond) { printf("[rich_label_test] PASS %s\n", name); } \
    else { printf("[rich_label_test] FAIL %s\n", name); g_failures++; } \
} while (0)

// Builds a RichText whose glyph quads are synthetic: byte index i sits at
// x = i*8 with advance 8 (11 chars, 88pt wide, 12pt tall). No real font.
static RichText *makeFakeModel(void) {
    RichText *rt = RichText_new();
    if (!rt)
        return NULL;
    RichText_setString(rt, "hello world");
    (*rt).quads = (TextQuad*) Memory_realloc((*rt).quads, 16 * sizeof(TextQuad));
    if (!(*rt).quads)
        return rt;
    (*rt).quadCapacity = 16;
    (*rt).quadCount = 11;
    for (size_t i = 0; i < 11; i++) {
        TextQuad *q = &(*rt).quads[i];
        (*q).x = (float) (i * 8);
        (*q).y = 0.0f;
        (*q).w = 7.0f;
        (*q).h = 12.0f;
        (*q).u0 = 0.0f; (*q).v0 = 0.0f; (*q).u1 = 1.0f; (*q).v1 = 1.0f;
        (*q).color = 0xFFFFFFFF;
        (*q).textureId = -1;
        (*q).bold = 0.0f;
        (*q).isColor = false;
        (*q).decor = DECOR_NONE;
        (*q).charIndex = (int32_t) i;
        (*q).advance = 8.0f;
    }
    (*rt).layoutWidth = 88.0f;
    (*rt).layoutHeight = 12.0f;
    return rt;
}

int main(void) {
    printf("=== Running RichLabel Test Suite ===\n");

    // §1 Construction & defaults
    {
        RichLabel *rl = RichLabel_0();
        CHECK("RichLabel_0 created", rl != NULL);
        CHECK("RichLabel not highlightable by default", RichLabel_isHighlightable(rl) == false);
        CHECK("RichLabel default cursor", RichLabel_getCursor(rl) != NULL && Cursor_getType(RichLabel_getCursor(rl)) == CURSOR_DEFAULT);
        CHECK("RichLabel default highlight color", RichLabel_getHighlightColor(rl) == 0x662563EBu);
        CHECK("RichLabel default hovered", RichLabel_isHovered(rl) == false);
        int32_t s0 = -1, s1 = -1;
        RichLabel_getSelection(rl, &s0, &s1);
        CHECK("RichLabel default selection cleared", s0 == -1 && s1 == -1);

        uint8_t r = 0, g = 0, b = 0, a = 0;
        RichLabel_getHighlightColorRGBA(rl, &r, &g, &b, &a);
        CHECK("RichLabel highlightColorRGBA r", r == ((0x662563EBu >> 16) & 0xFF));
        CHECK("RichLabel highlightColorRGBA g", g == ((0x662563EBu >> 8) & 0xFF));
        CHECK("RichLabel highlightColorRGBA b", b == (0x662563EBu & 0xFF));
        CHECK("RichLabel highlightColorRGBA a", a == ((0x662563EBu >> 24) & 0xFF));
        RichLabel_free(rl);
    }

    // §2 Char hit-testing against synthetic glyph quads
    {
        RichLabel *rl = RichLabel_0();
        RichText *tm = makeFakeModel();
        CHECK("RichLabel fake model", tm != NULL && (*tm).quads != NULL);
        RichLabel_setTextModel(rl, tm);

        int32_t c;
        c = RichLabel_charIndexAt(rl, 0.0f, 0.0f);
        CHECK("RichLabel_charIndexAt first", c == 0);
        c = RichLabel_charIndexAt(rl, 3.5f, 0.0f);
        CHECK("RichLabel_charIndexAt near glyph0", c == 0);
        c = RichLabel_charIndexAt(rl, 20.0f, 0.0f);
        CHECK("RichLabel_charIndexAt glyph2 boundary", c == 2);
        c = RichLabel_charIndexAt(rl, 75.0f, 0.0f);
        CHECK("RichLabel_charIndexAt glyph9", c == 9);
        c = RichLabel_charIndexAt(rl, 1000.0f, 0.0f);
        CHECK("RichLabel_charIndexAt clamps to len", c == 11);
        // Vertical distance should not change the horizontal pick
        c = RichLabel_charIndexAt(rl, 3.5f, -50.0f);
        CHECK("RichLabel_charIndexAt vertical invariant", c == 0);

        RichLabel_free(rl);
        RichText_free(tm);
    }

    // §3 Highlightable toggle + cursor adaptation
    {
        RichLabel *rl = RichLabel_0();
        RichText *tm = makeFakeModel();
        RichLabel_setTextModel(rl, tm);
        RichLabel_setHighlightable(rl, true);
        CHECK("RichLabel highlightable set", RichLabel_isHighlightable(rl) == true);
        CHECK("RichLabel cursor I-beam", RichLabel_getCursor(rl) != NULL && Cursor_getType(RichLabel_getCursor(rl)) == CURSOR_IBEAM);

        RichLabel_setSelection(rl, 2, 7);
        int32_t s0 = -1, s1 = -1;
        RichLabel_getSelection(rl, &s0, &s1);
        CHECK("RichLabel setSelection start", s0 == 2);
        CHECK("RichLabel setSelection end", s1 == 7);

        RichLabel_setHighlightable(rl, false);
        CHECK("RichLabel highlightable cleared", RichLabel_isHighlightable(rl) == false);
        CHECK("RichLabel cursor reverted", RichLabel_getCursor(rl) != NULL && Cursor_getType(RichLabel_getCursor(rl)) == CURSOR_DEFAULT);
        RichLabel_getSelection(rl, &s0, &s1);
        CHECK("RichLabel selection cleared on disable", s0 == -1 && s1 == -1);

        RichLabel_free(rl);
        RichText_free(tm);
    }

    // §4 Fixed-anchor pointer drag selection (forward, backward, then forward)
    {
        RichLabel *rl = RichLabel_0();
        RichText *tm = makeFakeModel();
        RichLabel_setTextModel(rl, tm);
        RichLabel_setHighlightable(rl, true);

        // Forward drag: down at glyph2, drag to glyph9
        RichLabel_handlePointer(rl, PTR_DOWN, 20.0f, 6.0f, NULL);
        int32_t s0 = -1, s1 = -1;
        RichLabel_getSelection(rl, &s0, &s1);
        CHECK("RichLabel down collapses anchor", s0 == 2 && s1 == 2);
        RichLabel_handlePointer(rl, PTR_DRAG, 75.0f, 6.0f, NULL);
        RichLabel_getSelection(rl, &s0, &s1);
        CHECK("RichLabel forward drag expanded", s0 == 2 && s1 == 9);
        RichLabel_handlePointer(rl, PTR_UP, 75.0f, 6.0f, NULL);
        RichLabel_getSelection(rl, &s0, &s1);
        CHECK("RichLabel release preserves selection", s0 == 2 && s1 == 9);

        // Backward drag: down at glyph9, drag left to glyph2 -> ordered mid-drag
        RichLabel_handlePointer(rl, PTR_DOWN, 75.0f, 6.0f, NULL);   // anchor 9
        RichLabel_handlePointer(rl, PTR_DRAG, 20.0f, 6.0f, NULL);   // active 2 (inverted)
        RichLabel_getSelection(rl, &s0, &s1);
        CHECK("RichLabel backward drag ordered mid-drag", s0 == 2 && s1 == 9);

        // Fixed anchor: drag right past anchor -> [anchor, active], not rolling union
        RichLabel_handlePointer(rl, PTR_DRAG, 84.0f, 6.0f, NULL);   // active glyph10
        RichLabel_getSelection(rl, &s0, &s1);
        CHECK("RichLabel fixed anchor survives backward pass", s0 == 9 && s1 == 10);
        RichLabel_handlePointer(rl, PTR_UP, 84.0f, 6.0f, NULL);
        RichLabel_getSelection(rl, &s0, &s1);
        CHECK("RichLabel backward release keeps selection", s0 == 9 && s1 == 10);

        // Plain click clears
        RichLabel_handlePointer(rl, PTR_DOWN, 40.0f, 6.0f, NULL);
        RichLabel_handlePointer(rl, PTR_UP, 40.0f, 6.0f, NULL);
        RichLabel_getSelection(rl, &s0, &s1);
        CHECK("RichLabel plain click clears selection", s0 == -1 && s1 == -1);

        // Outside click clears
        RichLabel_setSelection(rl, 2, 9);
        RichLabel_handlePointer(rl, PTR_DOWN, 2000.0f, 2000.0f, NULL);
        RichLabel_getSelection(rl, &s0, &s1);
        CHECK("RichLabel outside click clears selection", s0 == -1 && s1 == -1);

        RichLabel_free(rl);
        RichText_free(tm);
    }

    // §5 Symmetric highlight-color setters, hover, null-safety
    {
        RichLabel *rl = RichLabel_0();
        RichLabel_setHighlightColor(rl, 0x88AABBCC);
        CHECK("RichLabel set/get highlight color", RichLabel_getHighlightColor(rl) == 0x88AABBCC);
        RichLabel_setHighlightColorRGBA(rl, 10, 20, 30, 40);
        uint8_t r = 0, g = 0, b = 0, a = 0;
        RichLabel_getHighlightColorRGBA(rl, &r, &g, &b, &a);
        CHECK("RichLabel highlightColorRGBA round-trip", r == 10 && g == 20 && b == 30 && a == 40);

        RichLabel_setHovered(rl, true);
        CHECK("RichLabel setHovered", RichLabel_isHovered(rl) == true);

        Cursor *dyn = Cursor_1(CURSOR_CROSSHAIR);
        RichLabel_setCursor(rl, dyn);
        CHECK("RichLabel setCursor", RichLabel_getCursor(rl) == dyn);

        CHECK("RichLabel null-safe isHighlightable", RichLabel_isHighlightable(NULL) == false);
        int32_t s0 = 0, s1 = 0;
        RichLabel_getSelection(NULL, &s0, &s1);
        CHECK("RichLabel null-safe getSelection", s0 == -1 && s1 == -1);
        CHECK("RichLabel null-safe getHighlightColor", RichLabel_getHighlightColor(NULL) == 0);
        CHECK("RichLabel null-safe charIndexAt", RichLabel_charIndexAt(NULL, 10.0f, 10.0f) == 0);
        Cursor_free(dyn);
        RichLabel_free(rl);
    }

    printf("\n=== RichLabel Test Summary: %d failures ===\n", g_failures);
    return g_failures > 0 ? 1 : 0;
}