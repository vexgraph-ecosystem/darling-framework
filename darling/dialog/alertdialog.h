#ifndef DARLING_DIALOG_ALERTDIALOG_H
#define DARLING_DIALOG_ALERTDIALOG_H

#include <stdbool.h>
#include <stdint.h>

#include "c23/constructor.h"
#include "darling/dialog/dialog.h"

// darling/dialog/alertdialog.h — severity dialog (struct only). Embeds
// Dialog: an alert IS-A dialog with a variant + confirm/cancel slots.

enum {
    ALERT_INFO = 0,
    ALERT_WARN = 1,
    ALERT_DANGER = 2
};

typedef struct AlertDialog {
    Dialog base;
    int32_t variant;      // ALERT_INFO/WARN/DANGER
    char *message;        // owned body text
    char *confirmLabel;   // owned confirm button text
    char *cancelLabel;    // owned cancel button text (nullptr = no cancel)
    void (*onConfirm)(void *ctx);
    void (*onCancel)(void *ctx);
    void *ctx;
} AlertDialog;

// Constructors (implemented with the overlay phase):
//   AlertDialog()          — bare shell
//   AlertDialog(parent)    — created and attached
AlertDialog *AlertDialog_0(void);
AlertDialog *AlertDialog_1(Panel *parent);

#define AlertDialog(...) CONSTRUCTOR_DISPATCH(AlertDialog, __VA_ARGS__)

#endif
