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
    Panel *savedRoot;     // Bridge tree held before modal capture (restored on close)
    Panel *savedFocused;  // Bridge key target held before modal capture
    void *savedWindow;    // Bridge OS window held before modal capture
    bool eventsHeld;      // True while the bridge is retargeted to this dialog
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
//   Dialog_show(dialog, frame): present bound to a parent frame (nullable);
//     attaches the handler, ensures the window, and captures focus + stacks
//     the pair (handler below, dialog on top) while modal or clinging.
//   Dialog_open(dialog): present with no parent (show with nullptr frame).
void Dialog_show(Dialog *dialog, Frame *frame);
bool Dialog_open(Dialog *dialog);
// AppKit red-close routing: a dialog whose own window is being closed must
// run the full Dialog_close cleanup (not just the raw window close).
// Returns true when AppKit may proceed; false when the close is blocked
// (held children) or was already performed by Dialog_close.
bool Dialog_requestClose(Dialog *dialog);
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
