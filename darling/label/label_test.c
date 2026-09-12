#include "annotation/overview.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "darling/label/label.h"
#include "darling/cursor/cursor.h"
#include "text/text_core.h"
#include "nio/mem.h"

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: LabelTest (darling/label/label_test.c)
 * LEVEL: L3 — Module Code (headless verification harness)
 * ============================================================================
 * Headless suite for Label typography, styling, mnemonic, and Cursor APIs.
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
    if (cond) { printf("[label_test] PASS %s\n", name); } \
    else { printf("[label_test] FAIL %s\n", name); g_failures++; } \
} while (0)

int main(void) {
    printf("=== Running Label & Cursor Test Suite ===\n");

    // §1 Cursor class verification
    {
        Cursor *cDef = Cursor_getPredefined(CURSOR_DEFAULT);
        CHECK("Cursor predefined default", cDef != NULL && Cursor_getType(cDef) == CURSOR_DEFAULT);

        Cursor *cIBeam = Cursor_getPredefined(CURSOR_IBEAM);
        CHECK("Cursor predefined ibeam", cIBeam != NULL && Cursor_getType(cIBeam) == CURSOR_IBEAM);

        Cursor *cHand = Cursor_getPredefined(CURSOR_POINTING_HAND);
        CHECK("Cursor predefined hand", cHand != NULL && Cursor_getType(cHand) == CURSOR_POINTING_HAND);

        Cursor *dyn = Cursor_1(CURSOR_CROSSHAIR);
        CHECK("Cursor dynamic create", dyn != NULL && Cursor_getType(dyn) == CURSOR_CROSSHAIR);
        Cursor_setType(dyn, CURSOR_RESIZE_EW);
        CHECK("Cursor setType", Cursor_getType(dyn) == CURSOR_RESIZE_EW);
        int dummy = 42;
        Cursor_setCustomData(dyn, &dummy);
        CHECK("Cursor customData", Cursor_getCustomData(dyn) == &dummy);
        Cursor_free(dyn);
        // Predefined free is safe no-op
        Cursor_free(cIBeam);
    }

    // §2 Label construction & defaults
    {
        Label *lbl = Label_0();
        CHECK("Label_0 created", lbl != NULL);
        CHECK("Label default ligatures", Label_hasLigatures(lbl) == true);
        CHECK("Label default spacingWidth", Label_getSpacingWidth(lbl) == 0.0f);
        CHECK("Label default spacingHeight", Label_getSpacingHeight(lbl) == 0.0f);
        CHECK("Label default underline", Label_getUnderline(lbl) == UNDERLINE_NONE);
        CHECK("Label default underlineColor", Label_getUnderlineColor(lbl) == 0);
        CHECK("Label default highlightable", Label_isHighlightable(lbl) == false);
        CHECK("Label default mnemonic", Label_isMnemonic(lbl) == false);
        CHECK("Label default cursor", Label_getCursor(lbl) != NULL && Cursor_getType(Label_getCursor(lbl)) == CURSOR_DEFAULT);
        Label_free(lbl);
    }

    // §3 Highlightable & Caret & Cursor adaptation & Rounded Corner Richness
    {
        Label *lbl = Label_1("Selectable Text");
        CHECK("Label_1 created", lbl != NULL);
        CHECK("Label default highlightRadius", fabsf(Label_getHighlightRadius(lbl) - 3.0f) < 0.001f);
        CHECK("Label default highlightColor", Label_getHighlightColor(lbl) == 0x662563EBu);
        CHECK("Label default hovered", Label_isHovered(lbl) == false);

        Label_setHighlightRadius(lbl, 5.0f);
        CHECK("Label setHighlightRadius", fabsf(Label_getHighlightRadius(lbl) - 5.0f) < 0.001f);

        Label_setHighlightColor(lbl, 0x88112233u);
        CHECK("Label setHighlightColor", Label_getHighlightColor(lbl) == 0x88112233u);

        uint8_t hr = 0, hg = 0, hb = 0, ha = 0;
        Label_setHighlightColorRGBA(lbl, 40, 80, 160, 200);
        Label_getHighlightColorRGBA(lbl, &hr, &hg, &hb, &ha);
        CHECK("Label highlightColorRGBA r", hr == 40);
        CHECK("Label highlightColorRGBA g", hg == 80);
        CHECK("Label highlightColorRGBA b", hb == 160);
        CHECK("Label highlightColorRGBA a", ha == 200);

        Label_setHighlightable(lbl, true);
        CHECK("Label highlightable set", Label_isHighlightable(lbl) == true);
        CHECK("Label cursor adapted to I-beam", Label_getCursor(lbl) != NULL && Cursor_getType(Label_getCursor(lbl)) == CURSOR_IBEAM);

        Label_setSelection(lbl, 2, 7);
        int32_t sStart = -1, sEnd = -1;
        Label_getSelection(lbl, &sStart, &sEnd);
        CHECK("Label selection start", sStart == 2);
        CHECK("Label selection end", sEnd == 7);

        // Character offset mapping
        Label_setSize(lbl, 150.0f, 24.0f);
        int32_t i0 = Label_charIndexAt(lbl, 0.0f);
        CHECK("Label_charIndexAt 0", i0 == 0);
        int32_t iEnd = Label_charIndexAt(lbl, 1000.0f);
        CHECK("Label_charIndexAt end", iEnd == 15);
        int32_t iMid = Label_charIndexAt(lbl, 75.0f);
        CHECK("Label_charIndexAt mid", iMid > 0 && iMid < 15);

        // Pointer lifecycle simulation (no window, dummy pointer)
        // 1. Hover inside bounds
        Label_handlePointer(lbl, PTR_HOVER, 50.0f, 10.0f, NULL);
        CHECK("Label hovered inside", Label_isHovered(lbl) == true);

        // 2. Hover outside bounds -> leave
        Label_handlePointer(lbl, PTR_MOVE, 200.0f, 10.0f, NULL);
        CHECK("Label unhovered outside", Label_isHovered(lbl) == false);

        // 3. Pointer down -> start selection (collapsed anchor)
        Label_handlePointer(lbl, PTR_DOWN, 30.0f, 10.0f, NULL);
        int32_t dStart = -1, dEnd = -1;
        Label_getSelection(lbl, &dStart, &dEnd);
        CHECK("Label pointer down selection initialized", dStart >= 0 && dStart == dEnd);

        // 4. Pointer drag -> expand selection
        Label_handlePointer(lbl, PTR_DRAG, 120.0f, 10.0f, NULL);
        Label_getSelection(lbl, &dStart, &dEnd);
        CHECK("Label pointer drag selection expanded", dEnd > dStart);

        // 5. Pointer up -> selection preserved
        Label_handlePointer(lbl, PTR_UP, 120.0f, 10.0f, NULL);
        int32_t uStart = -1, uEnd = -1;
        Label_getSelection(lbl, &uStart, &uEnd);
        CHECK("Label pointer up preserves selection", uStart == dStart && uEnd == dEnd);

        // 5b. New session: down then drag BACKWARD -> ordered pair mid-drag
        Label_handlePointer(lbl, PTR_DOWN, 120.0f, 10.0f, NULL);    // anchor 12
        Label_handlePointer(lbl, PTR_DRAG, 30.0f, 10.0f, NULL);     // active 3 (inverted)
        Label_getSelection(lbl, &uStart, &uEnd);
        CHECK("Label backward drag ordered mid-drag", uStart == 3 && uEnd == 12);

        // 5c. Fixed anchor: drag right past the original anchor -> span [anchor, active]
        Label_handlePointer(lbl, PTR_DRAG, 150.0f, 10.0f, NULL);    // active 15 > anchor 12
        Label_getSelection(lbl, &uStart, &uEnd);
        CHECK("Label fixed anchor survives backward pass", uStart == 12 && uEnd == 15);

        // 5d. Release keeps the ordered selection
        Label_handlePointer(lbl, PTR_UP, 150.0f, 10.0f, NULL);
        Label_getSelection(lbl, &uStart, &uEnd);
        CHECK("Label backward release keeps selection", uStart == 12 && uEnd == 15);

        // 5e. Plain click (no drag) clears the selection
        Label_handlePointer(lbl, PTR_DOWN, 60.0f, 10.0f, NULL);
        Label_handlePointer(lbl, PTR_UP, 60.0f, 10.0f, NULL);
        Label_getSelection(lbl, &uStart, &uEnd);
        CHECK("Label plain click clears selection", uStart == -1 && uEnd == -1);

        // 6. Explicit PTR_LEAVE
        Label_handlePointer(lbl, PTR_HOVER, 50.0f, 10.0f, NULL);
        CHECK("Label re-hovered", Label_isHovered(lbl) == true);
        Label_handlePointer(lbl, PTR_LEAVE, 0.0f, 0.0f, NULL);
        CHECK("Label PTR_LEAVE unhovered", Label_isHovered(lbl) == false);

        // 7. PointerEvent struct dispatch via Label_onPointer
        PointerEvent *pev = PointerEvent_4(PTR_HOVER, 40.0f, 8.0f, 0);
        Label_onPointer(lbl, pev, NULL);
        CHECK("Label_onPointer hover", Label_isHovered(lbl) == true);
        PointerEvent_setKind(pev, PTR_LEAVE);
        Label_onPointer(lbl, pev, NULL);
        CHECK("Label_onPointer leave", Label_isHovered(lbl) == false);
        Memory_free(pev);

        Label_setHighlightable(lbl, false);
        CHECK("Label highlightable cleared", Label_isHighlightable(lbl) == false);
        CHECK("Label cursor reverted to default", Label_getCursor(lbl) != NULL && Cursor_getType(Label_getCursor(lbl)) == CURSOR_DEFAULT);
        CHECK("Label selection cleared on disable", Label_isHovered(lbl) == false);

        Label_free(lbl);
    }

    // §4 Spacing & Underline & Ligatures
    {
        Label *lbl = Label_1("Typography Text");
        Label_setLigatures(lbl, false);
        CHECK("Label ligatures false", Label_hasLigatures(lbl) == false);
        Label_setLigatures(lbl, true);
        CHECK("Label ligatures true", Label_hasLigatures(lbl) == true);

        Label_setSpacingWidth(lbl, 2.5f);
        CHECK("Label spacingWidth", fabsf(Label_getSpacingWidth(lbl) - 2.5f) < 0.001f);
        Label_setSpacingHeight(lbl, 4.0f);
        CHECK("Label spacingHeight", fabsf(Label_getSpacingHeight(lbl) - 4.0f) < 0.001f);

        float sw = 0, sh = 0;
        Label_setSpacing(lbl, 1.5f, 3.5f);
        Label_getSpacing(lbl, &sw, &sh);
        CHECK("Label getSpacing width", fabsf(sw - 1.5f) < 0.001f);
        CHECK("Label getSpacing height", fabsf(sh - 3.5f) < 0.001f);

        Label_setUnderline(lbl, UNDERLINE_BASIC);
        CHECK("Label underline basic", Label_getUnderline(lbl) == UNDERLINE_BASIC);
        Label_setUnderline(lbl, UNDERLINE_STRIKETHROUGH);
        CHECK("Label underline strikethrough", Label_getUnderline(lbl) == UNDERLINE_STRIKETHROUGH);
        Label_setUnderline(lbl, UNDERLINE_JAGGED);
        CHECK("Label underline jagged", Label_getUnderline(lbl) == UNDERLINE_JAGGED);

        Label_setUnderlineColor(lbl, 0xFFFF0000);
        CHECK("Label underline color packed", Label_getUnderlineColor(lbl) == 0xFFFF0000);

        uint8_t r = 0, g = 0, b = 0, a = 0;
        Label_setUnderlineColorRGBA(lbl, 128, 64, 32, 255);
        Label_getUnderlineColorRGBA(lbl, &r, &g, &b, &a);
        CHECK("Label underline RGBA r", r == 128);
        CHECK("Label underline RGBA g", g == 64);
        CHECK("Label underline RGBA b", b == 32);
        CHECK("Label underline RGBA a", a == 255);

        Label_free(lbl);
    }

    // §5 Mnemonic Parsing & CoreText Raster
    {
        Label *lbl = Label_1("&Open File...");
        Label_setMnemonic(lbl, true);
        CHECK("Label mnemonic enabled", Label_isMnemonic(lbl) == true);

        // Rasterization will trigger ensureRaster -> mnemonic parse & rounded highlight
        uint8_t *rgba = NULL;
        int rw = 0, rh = 0;
        TextStyleDescriptor desc = {
            .ligatures = true,
            .spacingWidth = 1.0f,
            .spacingHeight = 0.0f,
            .underline = UNDERLINE_BASIC,
            .underlineColor = 0xFF00FF00,
            .mnemonicIndex = 0,
            .selectionStart = 0,
            .selectionEnd = 4,
            .highlightRadius = 4.0f,
            .highlightColor = 0x662563EBu,
        };
        bool ok = TextCore_rasterStyled("Open File...", "Helvetica", 24.0f, 0xFFFFFFFF, &desc, &rgba, &rw, &rh);
#if defined(__APPLE__)
        CHECK("TextCore_rasterStyled on Apple", ok == true && rgba != NULL && rw > 0 && rh > 0);
        if (rgba) free(rgba);
#else
        (void) ok;
        (void) rw;
        (void) rh;
#endif

        Label_free(lbl);
    }

    // §6 Selection Substring & Clipboard
    {
        Label *lbl = Label_1("fn paint(w, h)");
        Label_setHighlightable(lbl, true);
        Label_setSelection(lbl, 3, 8);
        char *sel = Label_getSelectedText(lbl);
        CHECK("Label_getSelectedText valid", sel != nullptr && strcmp(sel, "paint") == 0);
        if (sel)
            Memory_free(sel);

        // Test replacement
        Label_setSelectedText(lbl, "render");
        CHECK("Label_setSelectedText replaces selection", strcmp(Label_getText(lbl), "fn render(w, h)") == 0);

        // Test outside click deselects
        Label_setSize(lbl, 200.0f, 20.0f);
        Label_setSelection(lbl, 3, 9);
        int32_t s0 = -1, s1 = -1;
        Label_getSelection(lbl, &s0, &s1);
        CHECK("Label has selection before outside click", s0 == 3 && s1 == 9);
        Label_handlePointer(lbl, PTR_DOWN, 300.0f, 50.0f, nullptr);
        Label_getSelection(lbl, &s0, &s1);
        CHECK("Label deselects on outside click", s0 == -1 && s1 == -1);

// Clipboard round-trip through the in-memory test seam — unit tests
        // must never write the real NSPasteboard.
        TextCore_setTestClipboard(true);
        TextCore_copyToClipboard("Darling Clipboard Test");
        char *pasted = TextCore_pasteFromClipboard();
        CHECK("TextCore test-clipboard round-trip", pasted != nullptr && strcmp(pasted, "Darling Clipboard Test") == 0);
        if (pasted)
            free(pasted);
        TextCore_setTestClipboard(false);

        Label_free(lbl);
    }

    // §7 Stated per-glyph positions: the hit-test shares the table with the
    // highlight. Synthetic tables (same pattern as rich_label_test's synthetic
    // quads — the shaper only runs on Apple raster); Label_free owns them.
    {
        Label *lbl = Label_1("Ga");
        CHECK("glyph table absent by default", Label_getGlyphOffsets(lbl) == nullptr);
        CHECK("glyph count absent by default", Label_getGlyphOffsetCount(lbl) == 0);
        float *gx = (float*) Memory_alloc(TYPE_ARRAY, 3 * sizeof(float));
        gx[0] = 0.0f;
        gx[1] = 30.0f;
        gx[2] = 38.0f;
        (*lbl).glyphX = gx;
        (*lbl).glyphN = 3;
        CHECK("glyph offsets getter", Label_getGlyphOffsets(lbl) == gx);
        CHECK("glyph count getter", Label_getGlyphOffsetCount(lbl) == 3);
        // Boundaries at 16 / 35 (midpoints + 1pt raster pad): a pointer over
        // the true wide G maps to 0, over the narrow a maps to 1. Uniform
        // math (qw = 2*6 = 12) would map x=20 to 2 — the reported bug.
        CHECK("proportional hit G", Label_charIndexAt(lbl, 5.0f) == 0);
        CHECK("proportional hit a", Label_charIndexAt(lbl, 20.0f) == 1);
        CHECK("proportional hit a right half", Label_charIndexAt(lbl, 34.0f) == 1);
        CHECK("proportional hit end", Label_charIndexAt(lbl, 35.0f) == 2);
        CHECK("proportional hit clamp right", Label_charIndexAt(lbl, 100.0f) == 2);
        CHECK("proportional hit clamp left", Label_charIndexAt(lbl, -5.0f) == 0);
        // Stale-length table is ignored (uniform fallback, no crash).
        (*lbl).glyphN = 99;
        int32_t fb = Label_charIndexAt(lbl, 20.0f);
        CHECK("mismatched table falls back", fb >= 0 && fb <= 2);
        (*lbl).glyphN = 3;
        Label_free(lbl);
    }
    {
        // Multibyte rewind: continuation bytes share their codepoint's offset
        // and must resolve to the codepoint start, never mid-codepoint.
        Label *lbl = Label_1("éx");
        float *gx = (float*) Memory_alloc(TYPE_ARRAY, 4 * sizeof(float));
        gx[0] = 0.0f;
        gx[1] = 0.0f;
        gx[2] = 14.0f;
        gx[3] = 22.0f;
        (*lbl).glyphX = gx;
        (*lbl).glyphN = 4;
        CHECK("multibyte hit first", Label_charIndexAt(lbl, 5.0f) == 0);
        CHECK("multibyte hit second", Label_charIndexAt(lbl, 10.0f) == 2);
        CHECK("multibyte hit end", Label_charIndexAt(lbl, 21.0f) == 3);
        Label_free(lbl);
    }
    {
        CHECK("null glyph offsets", Label_getGlyphOffsets(nullptr) == nullptr);
        CHECK("null glyph count", Label_getGlyphOffsetCount(nullptr) == 0);
    }

    printf("\n=== Label & Cursor Test Summary: %d failures ===\n", g_failures);
    return g_failures > 0 ? 1 : 0;
}
