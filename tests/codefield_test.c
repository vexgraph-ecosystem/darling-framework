#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "darling/field/codefield.h"
#include "lang/size.h"
#include "nio/mem.h"
#include "raster/raster_graphics.h"

static size_t calledIndex = SIZE_MAX, calledNumber = SIZE_MAX;
static void action(void *params, size_t index, size_t number) {
    assert(params == &calledIndex);
    calledIndex = index;
    calledNumber = number;
}

static void dump(const char *path) {
    Image *image = RasterGraphics_getFramebuffer();
    FILE *f = fopen(path, "wb");
    assert(f);
    unsigned w = Image_width(image), h = Image_height(image);
    fprintf(f, "P6\n%u %u\n255\n", w, h);
    uint8_t *pixels = Image_pixels(image);
    for (size_t i = 0; i < (size_t) w * h; i++)
        assert(fwrite(pixels + i * 4, 1, 3, f) == 3);
    fclose(f);
}

int main(int argc, char **argv) {
    CodeField *cf = CodeField_0();
    assert(cf);
    Panel_setLocation(&(*cf).base, 20, 20);
    Panel_setSize(&(*cf).base, 620, 340);
    assert(CodeField_setText(cf, "alpha\nbeta\ngamma"));
    assert(CodeField_getRowCount(cf) == 3);
    assert(CodeField_setIncludeLineNumber(cf, 1, false));
    assert(CodeField_getLineNumber(cf, 0) == 1);
    assert(CodeField_getLineNumber(cf, 1) == 0);
    assert(CodeField_getLineNumber(cf, 2) == 2);
    assert(CodeField_setRowHeight(cf, 1, 72));
    Rectangle first, second, third;
    assert(CodeField_getRowAbsoluteRect(cf, 0, &first));
    assert(CodeField_getRowAbsoluteRect(cf, 1, &second));
    assert(CodeField_getRowAbsoluteRect(cf, 2, &third));
    assert(second.y == first.y + first.height);
    assert(third.y == second.y + 72);
    size_t row, column;
    assert(CodeField_hitTest(cf, second.x + 8, second.y + 50, &row, &column));
    assert(row == 1 && column == 0);
    assert(CodeField_button_setFunction(cf, 1, action, &calledIndex));
    void *params;
    assert(CodeField_button_getFunction(cf, 1, &params) == action);
    assert(params == &calledIndex);
    assert(CodeField_button_invoke(cf, 1));
    assert(calledIndex == 1 && calledNumber == 0);
    assert(CodeField_selection_setRange(cf, 0, 2, 2, 2));
    char selected[64]; bool truncated;
    assert(CodeField_copySelection(cf, selected, sizeof(selected), &truncated));
    assert(!strcmp(selected, "pha\nbeta\nga"));
    assert(!CodeField_copySelection(cf, selected, 3, &truncated) && truncated);
    assert(!strcmp(selected, "ph"));
    assert(CodeField_insertText(cf, "X\nY"));
    assert(CodeField_getRowCount(cf) == 2);
    assert(!strcmp(CodeField_getRowText(cf, 0), "alX"));
    assert(!strcmp(CodeField_getRowText(cf, 1), "Ymma"));
    assert(CodeField_selection_setRange(cf, 1, 0, 1, 0));
    assert(CodeField_deleteBackward(cf));
    assert(!strcmp(CodeField_getRowText(cf, 0), "alXYmma"));
    CodeField_setReadOnly(cf, true);
    assert(!CodeField_insertText(cf, "no"));
    CodeField_setReadOnly(cf, false);
    assert(!CodeField_setRowText(cf, 0, "\xFF"));
    assert(!CodeField_setText(cf, "valid\n\xFF"));
    assert(!strcmp(CodeField_getRowText(cf, 0), "alXYmma"));
    assert(!CodeField_setIncludeLineNumber(cf, SIZE_MAX, true));
    assert(!CodeField_setRowHeight(cf, 0, NAN));
    assert(!CodeField_selection_setRange(cf, 0, SIZE_MAX, 0, 0));
    assert(CodeField_setText(cf, ""));
    assert(CodeField_getRowCount(cf) == 1);
    assert(!CodeField_deleteBackward(cf));
    assert(CodeField_insertText(cf, "\n"));
    assert(CodeField_getRowCount(cf) == 2);
    assert(CodeField_selection_setRange(cf, 0, 0, 0, 0));
    assert(CodeField_deleteForward(cf));
    assert(CodeField_getRowCount(cf) == 1);

    CodeField_setFocused(cf, true);
    assert(CodeField_insertText(cf, "a"));
    assert(CodeField_handleKey(cf, CODEFIELD_KEY_ENTER, false));
    assert(CodeField_insertText(cf, "b"));
    assert(CodeField_handleKey(cf, CODEFIELD_KEY_LEFT, true));
    assert(CodeField_copySelection(cf, selected, sizeof(selected), &truncated));
    assert(!strcmp(selected, "b"));
    assert(CodeField_removeRow(cf, 1));
    assert(CodeField_removeRow(cf, 0));
    assert(CodeField_insertText(cf, "a"));
    assert(CodeField_insertText(cf, "b"));
    assert(!strcmp(CodeField_getRowText(cf, 0), "ab"));
    Panel *borrowed = Panel_0();
    Panel_setSize(borrowed, 200, 90);
    assert(CodeField_setRowContent(cf, 0, borrowed));
    assert(CodeField_getRowAbsoluteRect(cf, 0, &first));
    assert(first.height == 90);
    assert(!CodeField_insertText(cf, "blocked"));
    assert(!strcmp(CodeField_getRowText(cf, 0), "ab"));
    assert(CodeField_setRowContent(cf, 0, nullptr));
    assert(Panel_getBackgroundColor(borrowed) == PANEL_COLOR_CLEAR);
    Memory_free(borrowed);
    Panel *base = &(*cf).base;
    GraphicsComponent_setScale(&(*base).component, 2, 2);
    assert(CodeField_getRowAbsoluteRect(cf, 0, &first));
    assert(first.height == CodeField_text_getRowHeight(cf) * 2);
    assert(CodeField_hitTest(cf, first.x + 16 + 16, first.y + 8, &row, &column));
    assert(row == 0 && column == 1);
    GraphicsComponent_setScale(&(*base).component, 1, 1);

    assert(CodeField_setText(cf, "// INDEXED CODEFIELD / DARLING + GRAPHVEX\n\nvoid renderDocument(CodeField *field) {\n    CodeField_scrollVertical(field, deltaY);\n    CodeField_scrollHorizontal(field, deltaX);\n}\n\nDOCUMENTATION / this row is not a source line\nIts panel is taller. Numbering resumes below.\n\nCodeField_setIncludeLineNumber(field, index, false);\n// selection spans rows without changing their identity"));
    assert(CodeField_setIncludeLineNumber(cf, 7, false));
    assert(CodeField_setIncludeLineNumber(cf, 8, false));
    assert(CodeField_setRowHeight(cf, 7, 42));
    assert(CodeField_setRowHeight(cf, 8, 34));
    for (size_t i = CodeField_getRowCount(cf); i < 120; i++)
        assert(CodeField_add(cf, "// additional indexed row -- scroll to inspect"));
    assert(CodeField_button_setFunction(cf, 2, action, &calledIndex));
    assert(CodeField_button_setFunction(cf, 7, action, &calledIndex));
    assert(CodeField_selection_setRange(cf, 3, 4, 4, 44));
    CodeField_scroll_setOffset(cf, 0, 0);
    CodeField_setFocused(cf, true);
    assert(Graphics_registerRow(RasterGraphics_getRow()));
    assert(Graphics_setGraphics(LANG_BACKEND_RASTER));
    assert(Graphics_resize(660, 380));
    assert(Graphics_clear(0x090E16FFu));
    size_t beforePaint = MemoryArena_activeBytes(Memory_defaultArena());
    assert(CodeField_paint(cf));
    assert(MemoryArena_activeBytes(Memory_defaultArena()) == beforePaint);
    if (argc > 1)
        dump(argv[1]);
    float x, y;
    CodeField_scrollVertical(cf, 100000);
    CodeField_scrollHorizontal(cf, 100000);
    CodeField_scroll_getOffset(cf, &x, &y);
    assert(y > 0 && y < 100000 && x < 100000);
    assert(CodeField_getLineNumber(cf, 119) == 118);
    Rectangle clip = {0, 0, 30, 30}, restored;
    assert(Graphics_clip(&clip));
    assert(CodeField_paint(cf));
    assert(Graphics_getClip(&restored) && Rectangle_equals(&clip, &restored));
    assert(Graphics_clip(nullptr));
    CodeField_toStringStruct(cf, selected, sizeof(selected), &truncated);
    assert(truncated);
    CodeField_free(cf);
    RasterGraphics_shutdown();
    Memory_freeAll();
    puts("PASS codefield_test: indexed rows, numbering, variable heights, edits, selection, buttons, scrolling, raster paint");
}
