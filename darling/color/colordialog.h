#ifndef DARLING_COLOR_COLORDIALOG_H
#define DARLING_COLOR_COLORDIALOG_H

#include <stdbool.h>
#include <stdint.h>

#include "c23/constructor.h"
#include "darling/dialog/dialog.h"

// darling/color/colordialog.h — modal color editor (struct only). Embeds
// Dialog: a color dialog IS-A dialog carrying picker state. The inline
// widget twin lives in field/colorpicker.h; this is the modal shell that
// hosts picker-grade state with dialog chrome (title, confirm, cancel).

typedef struct ColorDialog {
    Dialog base;
    uint32_t color;       // packed 0xAARRGGBB selection
    float h, s, v;        // HSV mirror of color (kept in sync on set)
    bool showAlpha;       // alpha channel editing enabled
    void (*onChange)(void *ctx);
    void *ctx;
} ColorDialog;

// Constructors (implemented with the overlay phase):
//   ColorDialog()          — bare shell
//   ColorDialog(parent)    — created and attached
ColorDialog *ColorDialog_0(void);
ColorDialog *ColorDialog_1(Panel *parent);

#define ColorDialog(...) CONSTRUCTOR_DISPATCH(ColorDialog, __VA_ARGS__)

#endif
