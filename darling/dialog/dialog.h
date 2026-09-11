#ifndef DARLING_DIALOG_DIALOG_H
#define DARLING_DIALOG_DIALOG_H

#include <stdbool.h>
#include <stdint.h>

#include "c23/constructor.h"
#include "darling/panel/panel.h"

// darling/dialog/dialog.h — modal dialog shell (struct only; behavior lands
// with the overlay phase). Family root for AlertDialog and ColorDialog:
// they embed Dialog the way Dialog embeds Panel.

typedef struct Dialog {
    Panel base;
    char *title;          // owned dialog title (strdup on set)
    Panel *content;       // body node attached on open (borrowed)
    bool modal;           // true blocks input to siblings while open
    void (*onClose)(void *ctx);
    void *ctx;
} Dialog;

// Constructors (implemented with the overlay phase):
//   Dialog()          — bare shell, no content
//   Dialog(parent)    — created and attached
Dialog *Dialog_0(void);
Dialog *Dialog_1(Panel *parent);

#define Dialog(...) CONSTRUCTOR_DISPATCH(Dialog, __VA_ARGS__)

#endif
