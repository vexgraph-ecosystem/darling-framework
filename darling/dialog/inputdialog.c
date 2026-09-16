#include "darling/dialog/inputdialog.h"

#include "annotation/overview.h"
#include <stdlib.h>
#include <string.h>

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: InputDialog (inherits Dialog -> Frame)
 * LEVEL: L2 — Behavior (Single-line input modal frame)
 * ============================================================================
 * Modal dialog for text input, prompting the user with a descriptive label
 * and returning the typed value upon submission.
 *
 * STRUCT FIELDS (Mirroring darling/dialog/inputdialog.h):
 * ----------------------------------------------------------------------------
 *   Dialog base;                                     // Inherited Dialog (which inherits Frame)
 *   char *prompt;                                    // Owned prompt label
 *   char *textValue;                                 // Owned user input string
 *   char *placeholder;                               // Owned placeholder string
 *   void (*onSubmit)(const char *text, void *ctx);   // Submit callback
 *   void *ctx;                                       // Callback context
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - InputDialog_0(void)
 *   - InputDialog_1(prompt)
 *   - InputDialog_2(title, prompt)
 *   - InputDialog_3(title, prompt, defaultVal)
 *   - InputDialog_free(dialog)
 *
 * Core Functions:
 *   - InputDialog_submit(dialog)
 *
 * Setters:
 *   - InputDialog_setPrompt(dialog, prompt)
 *   - InputDialog_setTextValue(dialog, text)
 *   - InputDialog_setPlaceholder(dialog, placeholder)
 *   - InputDialog_setOnSubmit(dialog, onSubmit, ctx)
 *
 * Getters:
 *   - InputDialog_getDialog(dialog)
 *   - InputDialog_getPrompt(const dialog)
 *   - InputDialog_getTextValue(const dialog)
 *   - InputDialog_getPlaceholder(const dialog)
 *   - InputDialog_getCtx(const dialog)
 * ============================================================================
 */

// CONSTRUCTORS
// ============================================================================

InputDialog *InputDialog_0(void) {
    return InputDialog_3("Input", "Enter value:", "");
}

InputDialog *InputDialog_1(const char *prompt) {
    return InputDialog_3("Input", prompt, "");
}

InputDialog *InputDialog_2(const char *title, const char *prompt) {
    return InputDialog_3(title, prompt, "");
}

InputDialog *InputDialog_3(const char *title, const char *prompt, const char *defaultVal) {
    InputDialog *d = (InputDialog*) calloc(1, sizeof(InputDialog));
    if (d == nullptr)
        return nullptr;

    Dialog_init(&(*d).base, title, 440, 180);
    (*d).prompt = prompt ? strdup(prompt) : nullptr;
    (*d).textValue = defaultVal ? strdup(defaultVal) : strdup("");
    (*d).placeholder = strdup("");
    (*d).onSubmit = nullptr;
    (*d).ctx = nullptr;
    return d;
}

void InputDialog_free(InputDialog *dialog) {
    if (dialog == nullptr)
        return;

    if ((*dialog).prompt != nullptr) {
        free((*dialog).prompt);
        (*dialog).prompt = nullptr;
    }
    if ((*dialog).textValue != nullptr) {
        free((*dialog).textValue);
        (*dialog).textValue = nullptr;
    }
    if ((*dialog).placeholder != nullptr) {
        free((*dialog).placeholder);
        (*dialog).placeholder = nullptr;
    }

    Dialog_destroy(&(*dialog).base);
    free(dialog);
}

// CORE FUNCTIONS
// ============================================================================

void InputDialog_submit(InputDialog *dialog) {
    if (dialog == nullptr)
        return;

    if ((*dialog).onSubmit != nullptr)
        (*dialog).onSubmit((*dialog).textValue ? (*dialog).textValue : "", (*dialog).ctx);

    Dialog_close(&(*dialog).base);
}

// SETTERS
// ============================================================================

void InputDialog_setPrompt(InputDialog *dialog, const char *prompt) {
    if (dialog == nullptr)
        return;

    if ((*dialog).prompt != nullptr)
        free((*dialog).prompt);

    (*dialog).prompt = prompt ? strdup(prompt) : nullptr;
}

void InputDialog_setTextValue(InputDialog *dialog, const char *text) {
    if (dialog == nullptr)
        return;

    if ((*dialog).textValue != nullptr)
        free((*dialog).textValue);

    (*dialog).textValue = text ? strdup(text) : strdup("");
}

void InputDialog_setPlaceholder(InputDialog *dialog, const char *placeholder) {
    if (dialog == nullptr)
        return;

    if ((*dialog).placeholder != nullptr)
        free((*dialog).placeholder);

    (*dialog).placeholder = placeholder ? strdup(placeholder) : strdup("");
}

void InputDialog_setOnSubmit(InputDialog *dialog, void (*onSubmit)(const char *text, void *ctx), void *ctx) {
    if (dialog == nullptr)
        return;
    (*dialog).onSubmit = onSubmit;
    (*dialog).ctx = ctx;
}

// GETTERS
// ============================================================================

Dialog *InputDialog_getDialog(InputDialog *dialog) {
    if (dialog == nullptr)
        return nullptr;
    return &(*dialog).base;
}

const char *InputDialog_getPrompt(const InputDialog *dialog) {
    if (dialog == nullptr)
        return nullptr;
    return (*dialog).prompt;
}

const char *InputDialog_getTextValue(const InputDialog *dialog) {
    if (dialog == nullptr)
        return nullptr;
    return (*dialog).textValue;
}

const char *InputDialog_getPlaceholder(const InputDialog *dialog) {
    if (dialog == nullptr)
        return nullptr;
    return (*dialog).placeholder;
}

void *InputDialog_getCtx(const InputDialog *dialog) {
    if (dialog == nullptr)
        return nullptr;
    return (*dialog).ctx;
}
