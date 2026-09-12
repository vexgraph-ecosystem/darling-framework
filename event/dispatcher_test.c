#include "annotation/overview.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "darling/container.h"
#include "darling/label/label.h"
#include "darling/label/rich_label.h"
#include "darling/panel/markdown_panel.h"
#include "darling/panel/panel.h"
#include "event/bridge.h"
#include "event/dispatch.h"
#include "event/keyevent.h"
#include "event/pointer.h"
#include "input/key.h"
#include "nio/mem.h"
#include "text/text_core.h"

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: DispatcherTest (event/dispatcher_test.c)
 * LEVEL: L3 — Module Code (headless verification harness)
 * ============================================================================
 * Headless suite for the event dispatch layer (Darling_firePointer /
 * Darling_fireKey). Builds a small Panel tree (plain Label, highlightable
 * Label, RichLabel, MarkdownPanel) and proves end to end that:
 *   - hit-testing routes pointer DOWN/DRAG/UP to the right text handler and
 *     commits markdown document selections through the dispatch path;
 *   - the focus gate (Rule: Input/Textarea or highlightable text kinds only)
 *     sets s_focusedPanel, so Cmd+C reaches a focusable label but a plain
 *     non-highlightable label never becomes a key target;
 *   - Darling_fireKey explicit focused param routes keys without a DOWN;
 *   - an already-consumed pointer event is dropped (no focus, no copy);
 *   - the bridge window seam registers the OS window for cursor handlers;
 *   - getter/entry null-safety holds headless (window never mirrors a real
 *     NSWindow here, so all cursor lifecycle paths stay guarded).
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
    if (cond) { printf("[dispatcher_test] PASS %s\n", name); } \
    else { printf("[dispatcher_test] FAIL %s\n", name); g_failures++; } \
} while (0)

static void firePointerAt(Panel *root, int32_t kind, float x, float y) {
    PointerEvent *ev = PointerEvent_4(kind, x, y, 0);
    if (!ev)
        return;
    Darling_firePointer(root, ev);
    Memory_free(ev);
}

static void fireKeyTo(Panel *focused, int32_t code, uint32_t mods) {
    UIKeyEvent *ev = UIKeyEvent_3(code, true, false);
    if (!ev)
        return;
    UIKeyEvent_setMods(ev, mods);
    Darling_fireKey(focused, ev);
    Memory_free(ev);
}

int main(void) {
    // --- Getter / entry null-safety (Rule 35) ---
    PointerEvent *nullDown = PointerEvent_4(PTR_DOWN, 0.0f, 0.0f, 0);
    Darling_firePointer(nullptr, nullDown);
    Memory_free(nullDown);
    CHECK("null root firePointer safe", true);
    Darling_firePointer(nullptr, nullptr);
    CHECK("null event firePointer safe", true);
    UIKeyEvent *nullKey = UIKeyEvent_3(KEY_C, true, false);
    UIKeyEvent_setMods(nullKey, 8u);
    Darling_fireKey(nullptr, nullKey);
    Memory_free(nullKey);
    CHECK("null focused fireKey safe", true);
    Darling_fireKey(nullptr, nullptr);
    CHECK("null event fireKey safe", true);

    // --- Bridge window seam (registered by the app; cursor handlers read it) ---
    CHECK("window default null", Darling_bridgeGetWindow() == nullptr);
    Darling_bridgeSetWindow((void*) 0x1F00u);
    CHECK("window seam round-trip", Darling_bridgeGetWindow() == (void*) 0x1F00u);
    // The test stays headless: everything below runs with a null window so no
    // cursor lifecycle path is ever exercised against a fabricated OS object.
    Darling_bridgeSetWindow(nullptr);

    // --- Tree: plain Label, highlightable Label, RichLabel, MarkdownPanel ---
    Panel *root = Panel_0();

    Label *plain = Label_1("plain text");
    Label_setSize(plain, 200.0f, 24.0f);
    Label_setLocation(plain, 0.0f, 0.0f);
    Panel_addContainer(root, &(*plain).base);

    Label *hl = Label_1("beta gamma");
    Label_setSize(hl, 200.0f, 24.0f);
    Label_setLocation(hl, 0.0f, 30.0f);
    Label_setHighlightable(hl, true);
    Panel_addContainer(root, &(*hl).base);

    RichLabel *rl = RichLabel_1(root);
    Container_setSize(&(*rl).base.base, 300.0f, 24.0f);
    Container_setLocation(&(*rl).base.base, 0.0f, 60.0f);
    RichLabel_setHighlightable(rl, true);

    MarkdownPanel *md = MarkdownPanel_0();
    MarkdownPanel_setSize(md, 400.0f, 120.0f);
    MarkdownPanel_setText(md, "one\ntwo lines\nthree");
    MarkdownPanel_setHighlightable(md, true);
    Container_setLocation(&(*md).base.base, 0.0f, 90.0f);
    Panel_addContainer(root, &(*md).base);

    // --- In-memory board; clean slate ---
    TextCore_setTestClipboard(true);
    CHECK("board starts empty", TextCore_pasteFromClipboard() == nullptr);

    // --- Consumed pointer events are dropped: no focus, no selection side effects ---
    Label_setSelection(hl, 1, 3);
    int32_t a = 0, b = 0;
    Label_getSelection(hl, &a, &b);
    CHECK("hl preseeded", a == 1 && b == 3);
    PointerEvent *consumed = PointerEvent_4(PTR_DOWN, 50.0f, 42.0f, 0);
    PointerEvent_consume(consumed);
    Darling_firePointer(root, consumed);
    Memory_free(consumed);
    fireKeyTo(nullptr, KEY_C, 8u);
    CHECK("consumed DOWN never focuses", TextCore_pasteFromClipboard() == nullptr);
    Label_getSelection(hl, &a, &b);
    CHECK("consumed DOWN leaves selection", a == 1 && b == 3);

    // --- Focus gate: a plain non-highlightable label is NOT a key target ---
    firePointerAt(root, PTR_DOWN, 50.0f, 12.0f);
    fireKeyTo(nullptr, KEY_C, 8u);
    CHECK("plain label not focusable", TextCore_pasteFromClipboard() == nullptr);

    // --- Focus gate: DOWN + drag on the highlightable label commits a span ---
    firePointerAt(root, PTR_DOWN, 50.0f, 42.0f);
    firePointerAt(root, PTR_DRAG, 140.0f, 42.0f);
    firePointerAt(root, PTR_UP, 140.0f, 42.0f);
    char *labelExpect = Label_getSelectedText(hl);
    CHECK("hl committed selection via dispatch", labelExpect != nullptr);
    fireKeyTo(nullptr, KEY_C, 8u);
    char *pasted = TextCore_pasteFromClipboard();
    CHECK("highlightable label focused + copy", pasted && labelExpect && strcmp(pasted, labelExpect) == 0);
    Memory_free(labelExpect);
    if (pasted)
        free(pasted);

    // --- RichLabel routes through dispatch (no crash; selection untouched) ---
    pasted = TextCore_pasteFromClipboard();
    const char *boardBefore = pasted ? strdup(pasted) : nullptr;
    if (pasted)
        free(pasted);
    firePointerAt(root, PTR_DOWN, 50.0f, 72.0f);
    fireKeyTo(nullptr, KEY_C, 8u);
    pasted = TextCore_pasteFromClipboard();
    CHECK("rich row routed, board unchanged", pasted && boardBefore && strcmp(pasted, boardBefore) == 0);
    if (boardBefore)
        free((char*) boardBefore);
    if (pasted)
        free(pasted);

    // --- Document selection committed through the dispatch path ---
    firePointerAt(root, PTR_DOWN, 20.0f, 120.0f);
    firePointerAt(root, PTR_DRAG, 390.0f, 200.0f);
    firePointerAt(root, PTR_UP, 390.0f, 200.0f);
    MarkdownPanel_getSelection(md, &a, &b);
    CHECK("markdown committed via dispatch", b > a);
    char *expect = MarkdownPanel_getSelectedText(md);
    CHECK("markdown selection has copy text", expect != nullptr);
    fireKeyTo(nullptr, KEY_C, 8u);
    pasted = TextCore_pasteFromClipboard();
    CHECK("markdown copy routed to board", pasted && expect && strcmp(pasted, expect) == 0);
    if (pasted)
        free(pasted);
    Memory_free(expect);

    // --- Explicit-focused Cmd+V replaces the markdown document ---
    pasted = TextCore_pasteFromClipboard();
    const char *mdClip = pasted ? strdup(pasted) : nullptr;
    if (pasted)
        free(pasted);
    fireKeyTo(&(*md).base, KEY_V, 8u);
    CHECK("md paste via explicit focus", MarkdownPanel_getRowCount(md) == 1);
    CHECK("md text replaced", mdClip && strcmp(MarkdownPanel_getText(md), mdClip) == 0);
    if (mdClip)
        free((char*) mdClip);

    if (g_failures == 0)
        printf("[dispatcher_test] ALL PASS\n");
    printf("=== Dispatcher Test Summary: %d failures ===\n", g_failures);
    return g_failures;
}