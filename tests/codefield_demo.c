#include "codefield_demo.h"
#include <stdio.h>
#include "darling/field/codefield.h"
#include "nio/mem.h"
#include "raster/raster_graphics.h"
_Static_assert((int) DEMO_KEY_LEFT == (int) CODEFIELD_KEY_LEFT, "key bridge mismatch");
_Static_assert((int) DEMO_KEY_RIGHT == (int) CODEFIELD_KEY_RIGHT, "key bridge mismatch");
_Static_assert((int) DEMO_KEY_UP == (int) CODEFIELD_KEY_UP, "key bridge mismatch");
_Static_assert((int) DEMO_KEY_DOWN == (int) CODEFIELD_KEY_DOWN, "key bridge mismatch");
_Static_assert((int) DEMO_KEY_HOME == (int) CODEFIELD_KEY_HOME, "key bridge mismatch");
_Static_assert((int) DEMO_KEY_END == (int) CODEFIELD_KEY_END, "key bridge mismatch");
_Static_assert((int) DEMO_KEY_BACKSPACE == (int) CODEFIELD_KEY_BACKSPACE, "key bridge mismatch");
_Static_assert((int) DEMO_KEY_DELETE == (int) CODEFIELD_KEY_DELETE, "key bridge mismatch");
_Static_assert((int) DEMO_KEY_ENTER == (int) CODEFIELD_KEY_ENTER, "key bridge mismatch");
_Static_assert((int) DEMO_KEY_TAB == (int) CODEFIELD_KEY_TAB, "key bridge mismatch");

// Standalone visual/input harness. The production OS host remains hotcwap.
// Bitmap ASCII text deliberately exposes the current Graphics-row capability.
static CodeField *field;
static Panel *documentation;

static bool paintDocumentation(Panel *panel, const Rectangle *rect) {
    (void) panel;
    Brush bg = {0x203348FFu, 1.0f};
    Graphics_fillRect(rect, &bg);
    Rectangle label = {(*rect).x + 12, (*rect).y + 12, (*rect).width - 24, 16};
    Brush ink = {0xA8D8FFFFu, 1.0f};
    Graphics_drawText(&label, "BORROWED DOCUMENTATION PANEL / not a source-file line", &ink);
    label.y += 20;
    Graphics_drawText(&label, "Variable height. No line number. Its button still receives the row index.", &ink);
    return true;
}

static void clicked(void *params, size_t index, size_t lineNumber) {
    (void) params;
    printf("button: index=%zu, displayed number=%zu\n", index, lineNumber);
    CodeField_setIncludeLineNumber(field, index, !CodeField_getIncludeLineNumber(field, index));
}

void CodeFieldDemo_setup(void) {
    field = CodeField_0();
    CodeField_setText(field,
        "// DARLING CODEFIELD\n"
        "// One indexed row model. Two synchronized visual panels.\n"
        "\n"
        "void configure(CodeField *field) {\n"
        "    CodeField_scrollVertical(field, deltaY);\n"
        "    CodeField_scrollHorizontal(field, deltaX);\n"
        "}\n"
        "\n"
        "\n"
        "CodeField_setIncludeLineNumber(field, index, false);\n"
        "// Numbers count included rows; geometry follows row heights.\n"
        "// Click and drag to select. Type to edit. Shift+arrows extends.\n"
        "// Use the wheel, trackpad or scrollbar tracks to scroll.\n"
        "// Click a gutter button to toggle that row's number.\n");
    CodeField_text_setScale(field, 2.0f);
    CodeField_text_setRowHeight(field, 34.0f);
    CodeField_gutter_setButtonWidth(field, 30.0f);
    CodeField_scroll_setBarSize(field, 14.0f);
    documentation = Panel_0();
    Panel_setSize(documentation, 1000, 78);
    Panel_setBackgroundFn(documentation, paintDocumentation);
    CodeField_setRowContent(field, 8, documentation);
    CodeField_setIncludeLineNumber(field, 8, false);
    CodeField_button_setFunction(field, 3, clicked, nullptr);
    CodeField_button_setFunction(field, 8, clicked, nullptr);
    for (size_t i = CodeField_getRowCount(field); i < 130; i++) {
        char text[120];
        snprintf(text, sizeof(text), "// row %zu -- scroll below to inspect 1-, 2- and 3-digit gutter alignment", i);
        CodeField_add(field, text);
    }
    CodeField_selection_setRange(field, 4, 4, 5, 46);
    CodeField_setFocused(field, true);
    Graphics_registerRow(RasterGraphics_getRow());
    Graphics_setGraphics(LANG_BACKEND_RASTER);
}

void CodeFieldDemo_render(unsigned width, unsigned height) {
    Graphics_resize(width, height);
    Graphics_clear(0x0A1019FFu);
    Panel_setLocation(&(*field).base, 24, 24);
    Panel_setSize(&(*field).base, (float) width - 48, (float) height - 48);
    CodeField_paint(field);
}


const unsigned char *CodeFieldDemo_pixels(void) { return Image_pixels(RasterGraphics_getFramebuffer()); }
void CodeFieldDemo_pointer(float x, float y, bool extend) { CodeField_pointer(field, x, y, extend); }
void CodeFieldDemo_scroll(float x, float y) {
    CodeField_scrollHorizontal(field, x);
    CodeField_scrollVertical(field, y);
}
void CodeFieldDemo_text(const char *text) { CodeField_insertText(field, text); }
void CodeFieldDemo_selectAll(void) { CodeField_selection_selectAll(field); }
void CodeFieldDemo_key(int key, bool extend) { CodeField_handleKey(field, (CodeFieldKey) key, extend); }
void CodeFieldDemo_close(void) {
    CodeField_free(field);
    Memory_free(documentation);
    RasterGraphics_shutdown();
    Memory_freeAll();
}
