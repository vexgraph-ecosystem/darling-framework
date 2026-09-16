#include "annotation/overview.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "darling/dialog/dialog.h"
#include "darling/dialog/optiondialog.h"
#include "darling/dialog/inputdialog.h"
#include "darling/overlay/filedialog.h"
#include "darling/color/colordialog.h"
#include "nio/mem.h"

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: DialogFamilyTest (_tests/darling/dialog_family_test.c)
 * LEVEL: L3 — Module Code (headless verification harness)
 * ============================================================================
 * Verification suite for the Dialog class family inheriting Frame:
 * Dialog, OptionDialog, InputDialog, FileDialog, and ColorDialog.
 *
 * STRUCT FIELDS: none — procedural test harness.
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - main(void)
 * ============================================================================
 */

static void onOptionSelected(int result, void *ctx) {
    (*(int*) ctx) = result;
}

static void onInputSubmitted(const char *text, void *ctx) {
    char *dest = (char*) ctx;
    strncpy(dest, text, 63);
    dest[63] = '\0';
}

static void onColorChanged(uint32_t color, void *ctx) {
    (*(uint32_t*) ctx) = color;
}

int main(void) {
    printf("=== Running Dialog Family Test Suite ===\n");

    // 1. Base Dialog (inherits Frame)
    Dialog *dlg = Dialog_2("Confirm Action", 500, 300);
    assert(dlg != nullptr);
    assert(strcmp(Dialog_getTitle(dlg), "Confirm Action") == 0);
    assert(Dialog_isModal(dlg) == true);

    // Verify embedded Frame
    Frame *frame = Dialog_getFrame(dlg);
    assert(frame != nullptr);
    assert(Frame_getWidth(frame) == 500);
    assert(Frame_getHeight(frame) == 300);
    assert(Frame_isPresentsWithTransaction(frame) == true);

    Dialog_setTitle(dlg, "Updated Title");
    assert(strcmp(Dialog_getTitle(dlg), "Updated Title") == 0);
    Dialog_free(dlg);

    // 2. OptionDialog
    OptionDialog *opt = OptionDialog_3("Save Changes?", "Do you want to save?", OPTION_BUTTON_YES | OPTION_BUTTON_NO | OPTION_BUTTON_CANCEL);
    assert(opt != nullptr);
    assert(strcmp(OptionDialog_getMessage(opt), "Do you want to save?") == 0);
    assert(OptionDialog_getButtonFlags(opt) == (OPTION_BUTTON_YES | OPTION_BUTTON_NO | OPTION_BUTTON_CANCEL));

    int selected = -1;
    OptionDialog_setOnSelect(opt, onOptionSelected, &selected);
    OptionDialog_select(opt, OPTION_RESULT_YES);
    assert(selected == OPTION_RESULT_YES);
    assert(OptionDialog_getResult(opt) == OPTION_RESULT_YES);
    OptionDialog_free(opt);

    // 3. InputDialog
    InputDialog *inp = InputDialog_3("Rename", "New name:", "document.txt");
    assert(inp != nullptr);
    assert(strcmp(InputDialog_getPrompt(inp), "New name:") == 0);
    assert(strcmp(InputDialog_getTextValue(inp), "document.txt") == 0);

    char submittedText[64] = {0};
    InputDialog_setOnSubmit(inp, onInputSubmitted, submittedText);
    InputDialog_setTextValue(inp, "photo.png");
    InputDialog_submit(inp);
    assert(strcmp(submittedText, "photo.png") == 0);
    InputDialog_free(inp);

    // 4. FileDialog (inherits Dialog -> Frame)
    FileDialog *fileDlg = FileDialog_0();
    assert(fileDlg != nullptr);
    FileDialog_setPath(fileDlg, "/Users/vexgraph/Documents");
    FileDialog_setFilter(fileDlg, "*.txt;*.md");
    assert(strcmp(FileDialog_getPath(fileDlg), "/Users/vexgraph/Documents") == 0);
    assert(strcmp(FileDialog_getFilter(fileDlg), "*.txt;*.md") == 0);

    Dialog *fileBaseDlg = FileDialog_getDialog(fileDlg);
    assert(fileBaseDlg != nullptr);
    Frame *fileFrame = Dialog_getFrame(fileBaseDlg);
    assert(fileFrame != nullptr);
    assert(Frame_isPresentsWithTransaction(fileFrame) == true);
    Memory_free(fileDlg);

    // 5. ColorDialog (inherits Dialog -> Frame)
    ColorDialog *colorDlg = ColorDialog_2("Pick Background", 0xFF00FF00u);
    assert(colorDlg != nullptr);
    assert(ColorDialog_getColor(colorDlg) == 0xFF00FF00u);

    uint32_t liveColor = 0;
    ColorDialog_setOnChange(colorDlg, onColorChanged, &liveColor);
    ColorDialog_setColor(colorDlg, 0xFFFF00FFu);
    assert(liveColor == 0xFFFF00FFu);
    assert(ColorDialog_getColor(colorDlg) == 0xFFFF00FFu);

    Dialog *colorBaseDlg = ColorDialog_getDialog(colorDlg);
    assert(colorBaseDlg != nullptr);
    Frame *colorFrame = Dialog_getFrame(colorBaseDlg);
    assert(colorFrame != nullptr);
    assert(Frame_isPresentsWithTransaction(colorFrame) == true);
    ColorDialog_free(colorDlg);

    printf("=== Dialog Family Test Suite Passed! ===\n");
    return 0;
}
