#include "darling/dialog/dialog.h"

#include "annotation/overview.h"
#include "darling/frame.h"
#include "darling/panel/panel.h"
#include "event/bridge.h"
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
 *   Frame *handler;              // Owner/parent frame handling this dialog (nullable)
 *   char *title;                 // Owned dialog title (strdup on set)
 *   Panel *content;              // Body node attached on open (borrowed)
 *   bool modal;                  // True captures focus like clinging until closed
 *   bool clinging;               // Locks focus to dialog, blocks handler close
 *   bool open;                   // True while dialog is open/visible
 *   Panel *savedRoot;            // Bridge tree held before modal capture
 *   Panel *savedFocused;         // Bridge key target held before modal capture
 *   void *savedWindow;           // Bridge OS window held before modal capture
 *   bool eventsHeld;             // True while the bridge targets this dialog
 *   void (*onClose)(void *ctx);  // Dismiss callback
 *   void *ctx;                   // Callback context
 *
 * PRIVATE HELPERS (file-local, no API):
 * ----------------------------------------------------------------------------
 *   dialogHoldEvents(dialog)     // Retarget bridge to content, save prior wiring
 *   dialogReleaseEvents(dialog)  // Restore saved bridge wiring (close path)
 *   dialogSyncGateFor(handler)   // Key-gate rule: gate open iff no holder governs
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
 *   - Dialog_show(dialog, frame)            — attach handler, ensure window,
 *                                             present, capture focus + stack
 *                                             (handler below, dialog on top)
 *                                             while modal or clinging
 *   - Dialog_open(dialog)                   — show with no parent frame
 *   - Dialog_close(dialog)
 *   - Dialog_requestClose(dialog)           — red-close routing: blocked when
 *                                             governing holders, else full
 *                                             Dialog_close + cancel AppKit
 *   - Dialog_setHandler(dialog, frame)      — sets parentFrame bidirectional link
 *   - Dialog_removeHandler(dialog, frame)   — clears parentFrame bidirectional link
 *
 * Modality: modal implies focus capture exactly like clinging. A modal
 * dialog holds focus until closed even when clinging was never set, and
 * Frame_getActiveClingingDialog treats both flags as focus-holding.
 * Enforcement is threefold while held: (1) the handler OS key gate closes
 * (Window_setKeyEnabled false — AppKit refuses the parent key, so no focus
 * flash and no input gap; render + order unaffected); (2) the pair is glued
 * (Window_attachChild — handler below, dialog on top, moving as one unit);
 * (3) the event bridge retargets to the dialog content. A held dialog is
 * additionally not minimizable (yellow dimmed, restored on close) — a
 * minimized modal would park a live app with no usable windows. Close
 * reverses all of the above, restoring key only when no other holder still
 * governs the handler.
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
    (*dialog).savedRoot = nullptr;
    (*dialog).savedFocused = nullptr;
    (*dialog).savedWindow = nullptr;
    (*dialog).eventsHeld = false;
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

// Retarget the global event bridge to this dialog's content so pointer and
// key events drive the dialog — not the parent tree — while modal. The prior
// wiring is saved and restored on close; nested dialogs stack LIFO-clean.
static void dialogHoldEvents(Dialog *dialog) {
    if (dialog == nullptr || (*dialog).eventsHeld)
        return;
    (*dialog).savedRoot = Darling_bridgeGetRoot();
    (*dialog).savedFocused = Darling_bridgeGetFocused();
    (*dialog).savedWindow = Darling_bridgeGetWindow();
    (*dialog).eventsHeld = true;
    Darling_bridgeAttach((*dialog).content);
    Darling_bridgeSetFocused((*dialog).content);
    Darling_bridgeSetWindow((*dialog).frame.window);
}

// Key-gate rule, single place: a handler frame keeps the OS key gate open
// exactly while no modal/clinging dialog governs it. Closing the last
// holder re-enables key; any other state leaves the gate untouched.
static void dialogSyncGateFor(Frame *handler) {
    if (handler == nullptr || (*handler).window == nullptr)
        return;
    bool held = Frame_getActiveClingingDialog(handler) != nullptr;
    Window_setKeyEnabled((*handler).window, !held);
}

static void dialogReleaseEvents(Dialog *dialog) {
    if (dialog == nullptr || !(*dialog).eventsHeld)
        return;
    (*dialog).eventsHeld = false;
    Panel *root = (*dialog).savedRoot;
    Panel *focused = (*dialog).savedFocused;
    void *win = (*dialog).savedWindow;
    (*dialog).savedRoot = nullptr;
    (*dialog).savedFocused = nullptr;
    (*dialog).savedWindow = nullptr;
    Darling_bridgeAttach(root);
    Darling_bridgeSetFocused(focused);
    Darling_bridgeSetWindow(win);
}

void Dialog_show(Dialog *dialog, Frame *frame) {
    if (dialog == nullptr)
        return;

    if (frame != nullptr)
        Dialog_setHandler(dialog, frame);

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

    // Via Frame_show (not raw Window_show): the frame visible flag must track
    // reality, or Dialog_isOpen reads closed at runtime and the whole modality
    // system (gate, redirect, canClose) silently never matches.
    Frame_show(&(*dialog).frame);

    if ((*dialog).modal || (*dialog).clinging) {
        // A held dialog is not minimizable: minimizing a modal would park a
        // live app with zero usable windows (platform convention dims yellow
        // on modals; the parent-child glue would take the parent with it).
        if ((*dialog).frame.window != nullptr)
            Window_setMiniaturizable((*dialog).frame.window, false);
        Frame *handler = (*dialog).handler;
        if (handler != nullptr) {
            // Refuse the handler OS key and glue the pair before focusing:
            // AppKit then cannot hand the parent focus at all.
            dialogSyncGateFor(handler);
            if ((*handler).window != nullptr && (*dialog).frame.window != nullptr)
                Window_attachChild((*handler).window, (*dialog).frame.window);
        }
        dialogHoldEvents(dialog);
        Dialog_focus(dialog);
        Dialog_bringToFront(dialog);
    }
}

bool Dialog_open(Dialog *dialog) {
    if (dialog == nullptr)
        return false;

    (*dialog).open = true;
    Dialog_show(dialog, nullptr);
    return true;
}

// Red-close routing for the dialog's own window. An AppKit raw close would
// bypass Dialog_close, leaving open=true with the bridge held, the key gate
// shut, and the pair glued — a zombie holder that poisons every later focus,
// gate, and shutdown decision. So a permitted close runs the full cleanup
// here and cancels the AppKit close (already hidden + flagged). A dialog
// still governing open holders refuses, staying fully open.
bool Dialog_requestClose(Dialog *dialog) {
    if (dialog == nullptr)
        return true;
    if (!Frame_canClose(&(*dialog).frame))
        return false;
    Dialog_close(dialog);
    return false;
}

void Dialog_close(Dialog *dialog) {
    if (dialog == nullptr)
        return;

    (*dialog).open = false;

    // Cascade close any child dialogs clinging or attached to this dialog
    Frame_closeChildDialogs(&(*dialog).frame);

    // Release the event bridge before onClose: a replacement dialog opened
    // from onClose must save the handler wiring — not this closing dialog.
    dialogReleaseEvents(dialog);

    if ((*dialog).onClose != nullptr)
        (*dialog).onClose((*dialog).ctx);

    // Unconditional hide: a closed dialog is not visible, window or not.
    Frame_hide(&(*dialog).frame);
    if ((*dialog).frame.window != nullptr) {
        if ((*dialog).modal || (*dialog).clinging)
            Window_setMiniaturizable((*dialog).frame.window, true);
        Window_setShouldClose((*dialog).frame.window, true);
    }

    Frame *handler = (*dialog).handler;
    if (handler != nullptr) {
        // Unglue the pair and re-open the handler key gate first: self
        // already reads closed, so the sync restores key only when no other
        // modal/clinging dialog still governs the handler.
        if ((*handler).window != nullptr && (*dialog).frame.window != nullptr)
            Window_detachChild((*handler).window, (*dialog).frame.window);
        dialogSyncGateFor(handler);
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

    Frame *old = (*dialog).handler;
    if (old != nullptr) {
        Frame_removeChildDialog(old, dialog);
        (*dialog).handler = nullptr;
    }

    if (frame != nullptr) {
        (*dialog).handler = frame;
        Frame_addChildDialog(frame, dialog);
        (*dialog).frame.parentFrame = frame;

        if ((*frame).application != nullptr) {
            Frame_addFrameHandler(&(*dialog).frame, (*frame).application);
        }
    }

    // Reparenting a live dialog must not leak the old handler's key gate or
    // pair glue: unglue + resync the old side, gate + glue the new side.
    if (Dialog_isOpen(dialog)) {
        Window *dw = Dialog_window(dialog);
        if (old != nullptr && old != frame) {
            Window *ow = Frame_getWindow(old);
            if (ow != nullptr && dw != nullptr)
                Window_detachChild(ow, dw);
            dialogSyncGateFor(old);
        }
        if (frame != nullptr) {
            dialogSyncGateFor(frame);
            Window *nw = Frame_getWindow(frame);
            if (nw != nullptr && dw != nullptr && ((*dialog).modal || (*dialog).clinging))
                Window_attachChild(nw, dw);
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
    (*dialog).frame.parentFrame = nullptr;
    dialogSyncGateFor(frame);
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
    if ((*dialog).open) {
        // Recompute the handler gate on every toggle: enabling holds it,
        // disabling restores it unless another dialog still governs.
        dialogSyncGateFor((*dialog).handler);
        if ((*dialog).frame.window != nullptr)
            Window_setMiniaturizable((*dialog).frame.window, !((*dialog).modal || clinging));
    }
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
    // Stack the handler directly below the dialog first, so the pair moves
    // as one unit above every other app: other apps, then handler frame,
    // then dialog on top — never an app sandwiched between frame and dialog.
    Frame *handler = (*dialog).handler;
    if (handler != nullptr)
        Frame_bringToFront(handler);
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
    if (!Dialog_isOpen(dialog))
        return;
    dialogSyncGateFor((*dialog).handler);
    if ((*dialog).frame.window != nullptr)
        Window_setMiniaturizable((*dialog).frame.window, !((*dialog).clinging || modal));
    if (modal) {
        Dialog_focus(dialog);
        Dialog_bringToFront(dialog);
    }
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
