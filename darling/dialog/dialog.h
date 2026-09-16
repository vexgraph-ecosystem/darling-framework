#ifndef DARLING_DIALOG_DIALOG_H
#define DARLING_DIALOG_DIALOG_H

#include <stdbool.h>
#include <stdint.h>

#include "c23/constructor.h"
#include "darling/frame.h"
#include "darling/panel/panel.h"

#ifdef __cplusplus
extern "C" {
#endif

// darling/dialog/dialog.h — modal dialog frame shell inheriting Frame.
// Glues R1 (window) and R3 (graphics) via embedded Frame.
// Base class for OptionDialog, InputDialog, FileDialog, ColorDialog.

typedef struct Dialog {
    Frame frame;          // Inherited Frame: { *window, *graphics, *layers, ... }
    Frame *handler;       // Owner/parent frame handling this dialog (nullable)
    char *title;          // Owned dialog title (strdup on set)
    Panel *content;       // Body node attached on open (borrowed)
    bool modal;           // True blocks input to background windows while open
    bool clinging;        // Clinging: locks focus to dialog and prevents handler frame from closing
    bool open;            // True while dialog is open/visible
    void (*onClose)(void *ctx);
    void *ctx;
} Dialog;

// Constructors:
//   Dialog()
//   Dialog(title)
//   Dialog(title, width, height)
Dialog *Dialog_0(void);
Dialog *Dialog_1(const char *title);
Dialog *Dialog_2(const char *title, int width, int height);

#define Dialog(...) CONSTRUCTOR_DISPATCH(Dialog, __VA_ARGS__)

bool Dialog_init(Dialog *dialog, const char *title, int width, int height);
void Dialog_destroy(Dialog *dialog);
void Dialog_free(Dialog *dialog);

// Core Functions:
void Dialog_show(Dialog *dialog);
bool Dialog_open(Dialog *dialog);
void Dialog_close(Dialog *dialog);
bool Dialog_addDialogHolder(Dialog *dialog, Application *app);
bool Dialog_removeDialogHolder(Dialog *dialog, Application *app);

// Frame Handler & Hierarchy:
bool Dialog_setHandler(Dialog *dialog, Frame *frame);
bool Dialog_removeHandler(Dialog *dialog, Frame *frame);
Frame *Dialog_getHandler(const Dialog *dialog);
Frame *Dialog_handler(const Dialog *dialog);
bool Dialog_setDialogHandler(Dialog *dialog, Dialog *parentDialog);

// Clinging Mode:
void Dialog_setClinging(Dialog *dialog, bool clinging);
bool Dialog_isClinging(const Dialog *dialog);
bool Dialog_isOpen(const Dialog *dialog);

// Focus & Presentation:
void Dialog_focus(Dialog *dialog);
void Dialog_bringToFront(Dialog *dialog);

// Setters:
void Dialog_setTitle(Dialog *dialog, const char *title);
void Dialog_setContent(Dialog *dialog, Panel *content);
void Dialog_setModal(Dialog *dialog, bool modal);
void Dialog_setOnClose(Dialog *dialog, void (*onClose)(void *ctx), void *ctx);

// Getters:
Frame *Dialog_getFrame(Dialog *dialog);
Frame *Dialog_frame(Dialog *dialog);
Window *Dialog_window(const Dialog *dialog);
const char *Dialog_getTitle(const Dialog *dialog);
Panel *Dialog_getContent(const Dialog *dialog);
bool Dialog_isModal(const Dialog *dialog);
void *Dialog_getCtx(const Dialog *dialog);

#ifdef __cplusplus
}
#endif

#endif // DARLING_DIALOG_DIALOG_H
