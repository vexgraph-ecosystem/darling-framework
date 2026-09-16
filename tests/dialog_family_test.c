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
#include "kernel/application.h"
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
    assert(Dialog_frame(dlg) == frame);
    assert(Dialog_window(dlg) == nullptr);

    Application *dlgApp = Application_1("DialogHolderApp");
    assert(dlgApp != nullptr);
    assert(Dialog_addDialogHolder(dlg, dlgApp) == true);
    assert(Frame_application(frame) == dlgApp);
    assert(Dialog_removeDialogHolder(dlg, dlgApp) == true);
    assert(Frame_application(frame) == nullptr);
    Application_free(dlgApp);

    // Frame Handler, Clinging & Chaining Hierarchy tests
    Frame *rootFrame = Frame_0();
    assert(rootFrame != nullptr);
    assert(Frame_canClose(rootFrame) == true);
    assert(Frame_getChildDialogCount(rootFrame) == 0);

    // Attach dlg to rootFrame
    assert(Dialog_setHandler(dlg, rootFrame) == true);
    assert(Dialog_getHandler(dlg) == rootFrame);
    assert(Frame_getChildDialogCount(rootFrame) == 1);
    assert(Frame_getChildDialog(rootFrame, 0) == dlg);

    // Non-clinging dialog does not block frame close
    assert(Dialog_isClinging(dlg) == false);
    assert(Frame_canClose(rootFrame) == true);

    // Set dlg as clinging and open it
    Dialog_setClinging(dlg, true);
    assert(Dialog_isClinging(dlg) == true);
    (*dlg).open = true; // simulate open
    assert(Frame_canClose(rootFrame) == false); // Clinging dialog prevents frame close!
    assert(Frame_getActiveClingingDialog(rootFrame) == dlg);

    // Dialog chaining: childDlg handled by parent dlg
    Dialog *childDlg = Dialog_1("Chained Child Dialog");
    assert(childDlg != nullptr);
    assert(Dialog_setDialogHandler(childDlg, dlg) == true);
    assert(Dialog_getHandler(childDlg) == Dialog_getFrame(dlg));
    assert(Frame_getChildDialogCount(Dialog_getFrame(dlg)) == 1);

    // Set childDlg clinging and open it
    Dialog_setClinging(childDlg, true);
    (*childDlg).open = true; // simulate open

    // Root frame active clinging traverses all the way to childDlg!
    assert(Frame_getActiveClingingDialog(rootFrame) == childDlg);
    assert(Frame_canClose(rootFrame) == false);
    assert(Frame_canClose(Dialog_getFrame(dlg)) == false);

    // Close childDlg -> dlg remains active clinging
    Dialog_close(childDlg);
    assert(Dialog_isOpen(childDlg) == false);
    assert(Frame_canClose(Dialog_getFrame(dlg)) == true); // parent can close if child closed
    assert(Frame_getActiveClingingDialog(rootFrame) == dlg);
    assert(Frame_canClose(rootFrame) == false); // root still blocked by dlg

    // Close dlg -> root frame unblocked!
    Dialog_close(dlg);
    assert(Dialog_isOpen(dlg) == false);
    assert(Frame_canClose(rootFrame) == true); // unblocked!
    assert(Frame_getActiveClingingDialog(rootFrame) == nullptr);

    // Modal implies focus capture exactly like clinging (clinging never set)
    Dialog *modalDlg = Dialog_1("Modal Without Clinging");
    assert(modalDlg != nullptr);
    assert(Dialog_isModal(modalDlg) == true); // modal by default
    assert(Dialog_isClinging(modalDlg) == false);
    assert(Dialog_setHandler(modalDlg, rootFrame) == true);
    Frame *modalFrame = Dialog_getFrame(modalDlg);
    assert(modalFrame != nullptr);
    assert((*modalFrame).parentFrame == rootFrame); // bidirectional link set
    (*modalDlg).open = true; // simulate open
    assert(Frame_getActiveClingingDialog(rootFrame) == modalDlg);
    assert(Frame_canClose(rootFrame) == false); // modal blocks parent close
    Dialog_close(modalDlg);
    assert(Frame_getActiveClingingDialog(rootFrame) == nullptr);
    assert(Frame_canClose(rootFrame) == true);
    assert(Dialog_getHandler(modalDlg) == nullptr);
    assert((*modalFrame).parentFrame == nullptr); // bidirectional link cleared
    Dialog_free(modalDlg);

    // Event capture pairing: a held bridge releases cleanly on close
    Dialog *evDlg = Dialog_1("Event Hold");
    assert(evDlg != nullptr);
    assert(Dialog_setHandler(evDlg, rootFrame) == true);
    (*evDlg).open = true; // simulate open
    (*evDlg).eventsHeld = true; // simulate the hold Dialog_show leaves
    (*evDlg).savedRoot = nullptr;
    (*evDlg).savedFocused = nullptr;
    (*evDlg).savedWindow = nullptr;
    Dialog_close(evDlg);
    assert((*evDlg).eventsHeld == false);
    assert(Frame_canClose(rootFrame) == true);
    Dialog_free(evDlg);

    // Red-close routing: requestClose runs full cleanup, and its false means
    // "AppKit close cancelled, already handled" — not failure
    Dialog *reqDlg = Dialog_1("Red Close");
    assert(reqDlg != nullptr);
    assert(Dialog_setHandler(reqDlg, rootFrame) == true);
    (*reqDlg).open = true; // simulate open
    (*Dialog_getFrame(reqDlg)).visible = true; // simulate shown
    assert(Frame_canClose(rootFrame) == false); // modal default holds parent
    assert(Dialog_requestClose(reqDlg) == false); // cleaned up, cancel AppKit
    assert(Dialog_isOpen(reqDlg) == false);
    assert(Frame_isVisible(Dialog_getFrame(reqDlg)) == false); // close clears visible
    assert(Dialog_getHandler(reqDlg) == nullptr);
    assert(Frame_canClose(rootFrame) == true); // parent unblocked
    Dialog_free(reqDlg);

    // Blocked case: a dialog governing an open holder refuses, staying open
    Dialog *outerDlg = Dialog_1("Outer");
    Dialog *innerDlg = Dialog_1("Inner");
    assert(outerDlg != nullptr && innerDlg != nullptr);
    assert(Dialog_setHandler(outerDlg, rootFrame) == true);
    assert(Dialog_setDialogHandler(innerDlg, outerDlg) == true);
    (*outerDlg).open = true; // simulate open
    (*innerDlg).open = true; // simulate open
    assert(Dialog_requestClose(outerDlg) == false); // held child blocks
    assert(Dialog_isOpen(outerDlg) == true); // stays fully open
    assert(Dialog_requestClose(innerDlg) == false); // cleans inner up
    assert(Dialog_isOpen(innerDlg) == false);
    assert(Dialog_requestClose(outerDlg) == false); // cleans outer up
    assert(Dialog_isOpen(outerDlg) == false);
    assert(Frame_canClose(rootFrame) == true);
    Dialog_free(innerDlg);
    Dialog_free(outerDlg);

    Dialog_free(childDlg);
    Dialog_free(dlg);
    Frame_free(rootFrame);

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
