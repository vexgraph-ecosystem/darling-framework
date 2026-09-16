#include "darling/dialog/optiondialog.h"

#include "annotation/overview.h"
#include <stdlib.h>
#include <string.h>

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: OptionDialog (inherits Dialog -> Frame)
 * LEVEL: L2 — Behavior (Option selection modal frame)
 * ============================================================================
 * Modal dialog for options and prompts, providing action buttons (OK, Cancel,
 * Yes, No, Retry) with asynchronous dispatch to selection handlers.
 *
 * STRUCT FIELDS (Mirroring darling/dialog/optiondialog.h):
 * ----------------------------------------------------------------------------
 *   Dialog base;                             // Inherited Dialog (which inherits Frame)
 *   char *message;                           // Owned message string
 *   int buttonFlags;                         // Bitfield of OptionButtonFlags
 *   int result;                              // Last selected OptionResult
 *   void (*onSelect)(int result, void *ctx); // Action callback
 *   void *ctx;                               // Callback context
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - OptionDialog_0(void)
 *   - OptionDialog_1(message)
 *   - OptionDialog_2(title, message)
 *   - OptionDialog_3(title, message, buttonFlags)
 *   - OptionDialog_free(dialog)
 *
 * Core Functions:
 *   - OptionDialog_select(dialog, result)
 *
 * Setters:
 *   - OptionDialog_setMessage(dialog, message)
 *   - OptionDialog_setButtonFlags(dialog, buttonFlags)
 *   - OptionDialog_setOnSelect(dialog, onSelect, ctx)
 *
 * Getters:
 *   - OptionDialog_getDialog(dialog)
 *   - OptionDialog_getMessage(const dialog)
 *   - OptionDialog_getButtonFlags(const dialog)
 *   - OptionDialog_getResult(const dialog)
 *   - OptionDialog_getCtx(const dialog)
 * ============================================================================
 */

// CONSTRUCTORS
// ============================================================================

OptionDialog *OptionDialog_0(void) {
    return OptionDialog_3("Options", "", OPTION_BUTTON_OK | OPTION_BUTTON_CANCEL);
}

OptionDialog *OptionDialog_1(const char *message) {
    return OptionDialog_3("Options", message, OPTION_BUTTON_OK | OPTION_BUTTON_CANCEL);
}

OptionDialog *OptionDialog_2(const char *title, const char *message) {
    return OptionDialog_3(title, message, OPTION_BUTTON_OK | OPTION_BUTTON_CANCEL);
}

OptionDialog *OptionDialog_3(const char *title, const char *message, int buttonFlags) {
    OptionDialog *d = (OptionDialog*) calloc(1, sizeof(OptionDialog));
    if (d == nullptr)
        return nullptr;

    Dialog_init(&(*d).base, title, 440, 200);
    (*d).message = message ? strdup(message) : nullptr;
    (*d).buttonFlags = buttonFlags;
    (*d).result = OPTION_RESULT_CANCEL;
    (*d).onSelect = nullptr;
    (*d).ctx = nullptr;
    return d;
}

void OptionDialog_free(OptionDialog *dialog) {
    if (dialog == nullptr)
        return;

    if ((*dialog).message != nullptr) {
        free((*dialog).message);
        (*dialog).message = nullptr;
    }

    Dialog_destroy(&(*dialog).base);
    free(dialog);
}

// CORE FUNCTIONS
// ============================================================================

void OptionDialog_select(OptionDialog *dialog, int result) {
    if (dialog == nullptr)
        return;

    (*dialog).result = result;
    if ((*dialog).onSelect != nullptr)
        (*dialog).onSelect(result, (*dialog).ctx);

    Dialog_close(&(*dialog).base);
}

// SETTERS
// ============================================================================

void OptionDialog_setMessage(OptionDialog *dialog, const char *message) {
    if (dialog == nullptr)
        return;

    if ((*dialog).message != nullptr)
        free((*dialog).message);

    (*dialog).message = message ? strdup(message) : nullptr;
}

void OptionDialog_setButtonFlags(OptionDialog *dialog, int buttonFlags) {
    if (dialog == nullptr)
        return;
    (*dialog).buttonFlags = buttonFlags;
}

void OptionDialog_setOnSelect(OptionDialog *dialog, void (*onSelect)(int result, void *ctx), void *ctx) {
    if (dialog == nullptr)
        return;
    (*dialog).onSelect = onSelect;
    (*dialog).ctx = ctx;
}

// GETTERS
// ============================================================================

Dialog *OptionDialog_getDialog(OptionDialog *dialog) {
    if (dialog == nullptr)
        return nullptr;
    return &(*dialog).base;
}

const char *OptionDialog_getMessage(const OptionDialog *dialog) {
    if (dialog == nullptr)
        return nullptr;
    return (*dialog).message;
}

int OptionDialog_getButtonFlags(const OptionDialog *dialog) {
    if (dialog == nullptr)
        return 0;
    return (*dialog).buttonFlags;
}

int OptionDialog_getResult(const OptionDialog *dialog) {
    if (dialog == nullptr)
        return OPTION_RESULT_CANCEL;
    return (*dialog).result;
}

void *OptionDialog_getCtx(const OptionDialog *dialog) {
    if (dialog == nullptr)
        return nullptr;
    return (*dialog).ctx;
}
