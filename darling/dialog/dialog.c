#include "darling/dialog/dialog.h"

#include "annotation/overview.h"
#include "darling/frame.h"
#include "darling/panel/panel.h"
#include "kernel/application.h"
#include "window/window.h"

#include <stdlib.h>
#include <string.h>

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Dialog (inherits Frame)
 * LEVEL: L2 — Behavior (Modal Dialog Frame)
 * ============================================================================
 * Modal dialog shell inheriting Frame, embedding an R1 host window, R3 GPU
 * graphics context, and stacked FrameLayer FBOs within a CAMetalLayer.
 *
 * STRUCT FIELDS (Mirroring darling/dialog/dialog.h):
 * ----------------------------------------------------------------------------
 *   Frame frame;                 // Inherited Frame: { *window, *graphics, *layers, ... }
 *   char *title;                 // Owned dialog title (strdup on set)
 *   Panel *content;              // Body node attached on open (borrowed)
 *   bool modal;                  // True blocks input to background windows
 *   void (*onClose)(void *ctx);  // Dismiss callback
 *   void *ctx;                   // Callback context
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Dialog_0(void)
 *   - Dialog_1(title)
 *   - Dialog_2(title, width, height)
 *   - Dialog_init(dialog, title, width, height)
 *   - Dialog_destroy(dialog)
 *   - Dialog_free(dialog)
 *
 * Core Functions:
 *   - Dialog_show(dialog)
 *   - Dialog_close(dialog)
 *
 * Setters:
 *   - Dialog_setTitle(dialog, title)
 *   - Dialog_setContent(dialog, content)
 *   - Dialog_setModal(dialog, modal)
 *   - Dialog_setOnClose(dialog, onClose, ctx)
 *
 * Getters:
 *   - Dialog_getFrame(dialog)
 *   - Dialog_getTitle(const dialog)
 *   - Dialog_getContent(const dialog)
 *   - Dialog_isModal(const dialog)
 *   - Dialog_getCtx(const dialog)
 * ============================================================================
 */

// CONSTRUCTORS
// ============================================================================

bool Dialog_init(Dialog *dialog, const char *title, int width, int height) {
    if (dialog == nullptr)
        return false;

    memset(dialog, 0, sizeof(Dialog));
    Frame_init(nullptr, nullptr, &(*dialog).frame);
    (*dialog).frame.width = width > 0 ? width : 480;
    (*dialog).frame.height = height > 0 ? height : 320;
    (*dialog).frame.ownerDialog = dialog;
    (*dialog).title = title ? strdup(title) : nullptr;
    (*dialog).content = nullptr;
    (*dialog).modal = true;
    (*dialog).clinging = false;
    (*dialog).open = false;
    (*dialog).handler = nullptr;
    (*dialog).onClose = nullptr;
    (*dialog).ctx = nullptr;
    return true;
}

Dialog *Dialog_0(void) {
    Dialog *d = (Dialog*) calloc(1, sizeof(Dialog));
    if (d == nullptr)
        return nullptr;
    Dialog_init(d, "Dialog", 480, 320);
    return d;
}

Dialog *Dialog_1(const char *title) {
    Dialog *d = (Dialog*) calloc(1, sizeof(Dialog));
    if (d == nullptr)
        return nullptr;
    Dialog_init(d, title, 480, 320);
    return d;
}

Dialog *Dialog_2(const char *title, int width, int height) {
    Dialog *d = (Dialog*) calloc(1, sizeof(Dialog));
    if (d == nullptr)
        return nullptr;
    Dialog_init(d, title, width, height);
    return d;
}

void Dialog_destroy(Dialog *dialog) {
    if (dialog == nullptr)
        return;

    Dialog_close(dialog);

    if ((*dialog).handler != nullptr) {
        Frame_removeChildDialog((*dialog).handler, dialog);
        (*dialog).handler = nullptr;
    }

    if ((*dialog).title != nullptr) {
        free((*dialog).title);
        (*dialog).title = nullptr;
    }

    Frame_destroy(&(*dialog).frame);
}

void Dialog_free(Dialog *dialog) {
    if (dialog == nullptr)
        return;
    Dialog_destroy(dialog);
    free(dialog);
}

// CORE FUNCTIONS
// ============================================================================

void Dialog_show(Dialog *dialog) {
    if (dialog == nullptr)
        return;

    if ((*dialog).frame.window != nullptr)
        Window_show((*dialog).frame.window);

    Frame_render(&(*dialog).frame);
    Frame_present(&(*dialog).frame);
}

bool Dialog_open(Dialog *dialog) {
    if (dialog == nullptr)
        return false;

    (*dialog).open = true;

    if ((*dialog).frame.window == nullptr) {
        int w = (*dialog).frame.width > 0 ? (*dialog).frame.width : 480;
        int h = (*dialog).frame.height > 0 ? (*dialog).frame.height : 320;
        const char *t = (*dialog).title ? (*dialog).title : "Dialog";
        (*dialog).frame.window = Window_create(t, w, h);
        if ((*dialog).frame.window != nullptr) {
            Frame_platformAttach(&(*dialog).frame);
            if ((*dialog).handler != nullptr && (*(*dialog).handler).application != nullptr) {
                Frame_addFrameHandler(&(*dialog).frame, (*(*dialog).handler).application);
            } else if ((*dialog).frame.application != nullptr) {
                Application_addWindow((*dialog).frame.application, (*dialog).frame.window);
            }
        }
    }

    Dialog_show(dialog);
    if ((*dialog).clinging) {
        Dialog_focus(dialog);
        Dialog_bringToFront(dialog);
    }
    return true;
}

void Dialog_close(Dialog *dialog) {
    if (dialog == nullptr)
        return;

    (*dialog).open = false;

    // Cascade close any child dialogs clinging or attached to this dialog
    Frame_closeChildDialogs(&(*dialog).frame);

    if ((*dialog).onClose != nullptr)
        (*dialog).onClose((*dialog).ctx);

    if ((*dialog).frame.window != nullptr) {
        Window_hide((*dialog).frame.window);
        Window_setShouldClose((*dialog).frame.window, true);
    }

    Frame *handler = (*dialog).handler;
    if (handler != nullptr) {
        Dialog_removeHandler(dialog, handler);
        Frame_focus(handler);
    }
}

bool Dialog_addDialogHolder(Dialog *dialog, Application *app) {
    if (dialog == nullptr || app == nullptr)
        return false;
    return Frame_addFrameHandler(&(*dialog).frame, app);
}

bool Dialog_removeDialogHolder(Dialog *dialog, Application *app) {
    if (dialog == nullptr || app == nullptr)
        return false;
    return Frame_removeFrameHandler(&(*dialog).frame, app);
}

// FRAME HANDLER & HIERARCHY
// ============================================================================

bool Dialog_setHandler(Dialog *dialog, Frame *frame) {
    if (dialog == nullptr)
        return false;

    if ((*dialog).handler == frame)
        return true;

    if ((*dialog).handler != nullptr) {
        Frame_removeChildDialog((*dialog).handler, dialog);
        (*dialog).handler = nullptr;
    }

    if (frame != nullptr) {
        (*dialog).handler = frame;
        Frame_addChildDialog(frame, dialog);

        if ((*frame).application != nullptr) {
            Frame_addFrameHandler(&(*dialog).frame, (*frame).application);
        }
    }

    return true;
}

bool Dialog_removeHandler(Dialog *dialog, Frame *frame) {
    if (dialog == nullptr || (*dialog).handler != frame)
        return false;

    if (frame != nullptr) {
        Frame_removeChildDialog(frame, dialog);
    }
    (*dialog).handler = nullptr;
    return true;
}

Frame *Dialog_getHandler(const Dialog *dialog) {
    if (dialog == nullptr)
        return nullptr;
    return (*dialog).handler;
}

Frame *Dialog_handler(const Dialog *dialog) {
    return Dialog_getHandler(dialog);
}

bool Dialog_setDialogHandler(Dialog *dialog, Dialog *parentDialog) {
    if (dialog == nullptr)
        return false;
    return Dialog_setHandler(dialog, parentDialog ? Dialog_getFrame(parentDialog) : nullptr);
}

// CLINGING MODE
// ============================================================================

void Dialog_setClinging(Dialog *dialog, bool clinging) {
    if (dialog == nullptr)
        return;
    (*dialog).clinging = clinging;
    if (clinging && (*dialog).open) {
        Dialog_focus(dialog);
        Dialog_bringToFront(dialog);
    }
}

bool Dialog_isClinging(const Dialog *dialog) {
    if (dialog == nullptr)
        return false;
    return (*dialog).clinging;
}

bool Dialog_isOpen(const Dialog *dialog) {
    if (dialog == nullptr || !(*dialog).open)
        return false;
    if ((*dialog).frame.window != nullptr) {
        if (Window_shouldClose((*dialog).frame.window))
            return false;
        if (!Frame_isVisible(&(*dialog).frame))
            return false;
    }
    return true;
}

// FOCUS & PRESENTATION
// ============================================================================

void Dialog_focus(Dialog *dialog) {
    if (dialog == nullptr || !Dialog_isOpen(dialog))
        return;
    if ((*dialog).frame.window != nullptr)
        Window_focus((*dialog).frame.window);
}

void Dialog_bringToFront(Dialog *dialog) {
    if (dialog == nullptr || !Dialog_isOpen(dialog))
        return;
    if ((*dialog).frame.window != nullptr)
        Window_bringToFront((*dialog).frame.window);
}

// SETTERS
// ============================================================================

void Dialog_setTitle(Dialog *dialog, const char *title) {
    if (dialog == nullptr)
        return;

    if ((*dialog).title != nullptr)
        free((*dialog).title);

    (*dialog).title = title ? strdup(title) : nullptr;
}

void Dialog_setContent(Dialog *dialog, Panel *content) {
    if (dialog == nullptr)
        return;

    (*dialog).content = content;
    Frame_setRootPanel(&(*dialog).frame, content);
}

void Dialog_setModal(Dialog *dialog, bool modal) {
    if (dialog == nullptr)
        return;
    (*dialog).modal = modal;
}

void Dialog_setOnClose(Dialog *dialog, void (*onClose)(void *ctx), void *ctx) {
    if (dialog == nullptr)
        return;
    (*dialog).onClose = onClose;
    (*dialog).ctx = ctx;
}

// GETTERS
// ============================================================================

Frame *Dialog_getFrame(Dialog *dialog) {
    if (dialog == nullptr)
        return nullptr;
    return &(*dialog).frame;
}

Frame *Dialog_frame(Dialog *dialog) {
    return Dialog_getFrame(dialog);
}

Window *Dialog_window(const Dialog *dialog) {
    if (dialog == nullptr)
        return nullptr;
    return (*dialog).frame.window;
}

const char *Dialog_getTitle(const Dialog *dialog) {
    if (dialog == nullptr)
        return nullptr;
    return (*dialog).title;
}

Panel *Dialog_getContent(const Dialog *dialog) {
    if (dialog == nullptr)
        return nullptr;
    return (*dialog).content;
}

bool Dialog_isModal(const Dialog *dialog) {
    if (dialog == nullptr)
        return false;
    return (*dialog).modal;
}

void *Dialog_getCtx(const Dialog *dialog) {
    if (dialog == nullptr)
        return nullptr;
    return (*dialog).ctx;
}
