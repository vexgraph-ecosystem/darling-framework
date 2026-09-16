#ifndef DARLING_DIALOG_INPUTDIALOG_H
#define DARLING_DIALOG_INPUTDIALOG_H

#include <stdbool.h>
#include <stdint.h>

#include "c23/constructor.h"
#include "darling/dialog/dialog.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct InputDialog {
    Dialog base;                 // Inherited Dialog (which inherits Frame)
    char *prompt;                // Owned prompt label
    char *textValue;             // Owned user input string
    char *placeholder;           // Owned placeholder string
    void (*onSubmit)(const char *text, void *ctx);
    void *ctx;
} InputDialog;

// Constructors:
//   InputDialog()
//   InputDialog(prompt)
//   InputDialog(title, prompt)
//   InputDialog(title, prompt, defaultVal)
InputDialog *InputDialog_0(void);
InputDialog *InputDialog_1(const char *prompt);
InputDialog *InputDialog_2(const char *title, const char *prompt);
InputDialog *InputDialog_3(const char *title, const char *prompt, const char *defaultVal);

#define InputDialog(...) CONSTRUCTOR_DISPATCH(InputDialog, __VA_ARGS__)

void InputDialog_free(InputDialog *dialog);

// Core Functions:
void InputDialog_submit(InputDialog *dialog);

// Setters:
void InputDialog_setPrompt(InputDialog *dialog, const char *prompt);
void InputDialog_setTextValue(InputDialog *dialog, const char *text);
void InputDialog_setPlaceholder(InputDialog *dialog, const char *placeholder);
void InputDialog_setOnSubmit(InputDialog *dialog, void (*onSubmit)(const char *text, void *ctx), void *ctx);

// Getters:
Dialog *InputDialog_getDialog(InputDialog *dialog);
const char *InputDialog_getPrompt(const InputDialog *dialog);
const char *InputDialog_getTextValue(const InputDialog *dialog);
const char *InputDialog_getPlaceholder(const InputDialog *dialog);
void *InputDialog_getCtx(const InputDialog *dialog);

#ifdef __cplusplus
}
#endif

#endif // DARLING_DIALOG_INPUTDIALOG_H
