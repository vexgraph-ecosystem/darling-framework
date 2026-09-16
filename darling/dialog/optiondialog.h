#ifndef DARLING_DIALOG_OPTIONDIALOG_H
#define DARLING_DIALOG_OPTIONDIALOG_H

#include <stdbool.h>
#include <stdint.h>

#include "c23/constructor.h"
#include "darling/dialog/dialog.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum OptionButtonFlags {
    OPTION_BUTTON_OK = 1 << 0,
    OPTION_BUTTON_CANCEL = 1 << 1,
    OPTION_BUTTON_YES = 1 << 2,
    OPTION_BUTTON_NO = 1 << 3,
    OPTION_BUTTON_RETRY = 1 << 4
} OptionButtonFlags;

typedef enum OptionResult {
    OPTION_RESULT_CANCEL = 0,
    OPTION_RESULT_OK = 1,
    OPTION_RESULT_YES = 2,
    OPTION_RESULT_NO = 3,
    OPTION_RESULT_RETRY = 4
} OptionResult;

typedef struct OptionDialog {
    Dialog base;                 // Inherited Dialog (which inherits Frame)
    char *message;               // Owned message string
    int buttonFlags;             // Bitfield of OptionButtonFlags
    int result;                  // Last selected OptionResult
    void (*onSelect)(int result, void *ctx);
    void *ctx;
} OptionDialog;

// Constructors:
//   OptionDialog()
//   OptionDialog(message)
//   OptionDialog(title, message)
//   OptionDialog(title, message, buttonFlags)
OptionDialog *OptionDialog_0(void);
OptionDialog *OptionDialog_1(const char *message);
OptionDialog *OptionDialog_2(const char *title, const char *message);
OptionDialog *OptionDialog_3(const char *title, const char *message, int buttonFlags);

#define OptionDialog(...) CONSTRUCTOR_DISPATCH(OptionDialog, __VA_ARGS__)

void OptionDialog_free(OptionDialog *dialog);

// Core Functions:
void OptionDialog_select(OptionDialog *dialog, int result);

// Setters:
void OptionDialog_setMessage(OptionDialog *dialog, const char *message);
void OptionDialog_setButtonFlags(OptionDialog *dialog, int buttonFlags);
void OptionDialog_setOnSelect(OptionDialog *dialog, void (*onSelect)(int result, void *ctx), void *ctx);

// Getters:
Dialog *OptionDialog_getDialog(OptionDialog *dialog);
const char *OptionDialog_getMessage(const OptionDialog *dialog);
int OptionDialog_getButtonFlags(const OptionDialog *dialog);
int OptionDialog_getResult(const OptionDialog *dialog);
void *OptionDialog_getCtx(const OptionDialog *dialog);

#ifdef __cplusplus
}
#endif

#endif // DARLING_DIALOG_OPTIONDIALOG_H
