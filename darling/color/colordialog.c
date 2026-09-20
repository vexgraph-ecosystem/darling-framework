#include "darling/color/colordialog.h"

#include "annotation/definition.h"
#include "annotation/overview.h"
#include <stdlib.h>
#include <string.h>

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: ColorDialog
 * ============================================================================
 * Modal color picker frame inheriting Dialog: hosts a packed 0xAARRGGBB
 * selection with an HSV mirror, optional alpha editing, and a live change
 * callback fired on every setColor. Heap-allocated via calloc with
 * Dialog_init base construction; free tears down the Dialog base before
 * releasing the struct. A concrete Dialog subclass in the R4 modal family
 * alongside InputDialog, OptionDialog, and FileDialog.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: ColorDialog (inherits Dialog -> Frame)
 * LEVEL: L2 — Behavior (Modal color picker frame)
 * ============================================================================
 * Modal dialog for color selection, hosting RGBA/HSV color values with
 * live change callbacks and dialog chrome.
 *
 * STRUCT FIELDS (Mirroring darling/color/colordialog.h):
 * ----------------------------------------------------------------------------
 *   Dialog base;                                       // Inherited Dialog (which inherits Frame)
 *   uint32_t color;                                    // Packed 0xAARRGGBB selection
 *   float h;                                           // Hue [0.0, 360.0]
 *   float s;                                           // Saturation [0.0, 1.0]
 *   float v;                                           // Value/Brightness [0.0, 1.0]
 *   bool showAlpha;                                    // Alpha channel editing enabled
 *   void (*onChange)(uint32_t color, void *ctx);       // Color modification callback
 *   void *ctx;                                         // Callback context
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - ColorDialog_0(void)
 *   - ColorDialog_1(initialColor)
 *   - ColorDialog_2(title, initialColor)
 *   - ColorDialog_free(dialog)
 *
 * Setters:
 *   - ColorDialog_setColor(dialog, color)
 *   - ColorDialog_setHSV(dialog, h, s, v)
 *   - ColorDialog_setShowAlpha(dialog, showAlpha)
 *   - ColorDialog_setOnChange(dialog, onChange, ctx)
 *
 * Getters:
 *   - ColorDialog_getDialog(dialog)
 *   - ColorDialog_getColor(const dialog)
 *   - ColorDialog_getH(const dialog)
 *   - ColorDialog_getS(const dialog)
 *   - ColorDialog_getV(const dialog)
 *   - ColorDialog_isShowAlpha(const dialog)
 *   - ColorDialog_getCtx(const dialog)
 * ============================================================================
 */

// CONSTRUCTORS
// ============================================================================

ColorDialog *ColorDialog_0(void) {
    return ColorDialog_2("Select Color", 0xFFFFFFFFu);
}

ColorDialog *ColorDialog_1(uint32_t initialColor) {
    return ColorDialog_2("Select Color", initialColor);
}

ColorDialog *ColorDialog_2(const char *title, uint32_t initialColor) {
    ColorDialog *d = (ColorDialog*) calloc(1, sizeof(ColorDialog));
    if (d == nullptr)
        return nullptr;

    Dialog_init(&(*d).base, title, 500, 360);
    (*d).color = initialColor;
    (*d).h = 0.0f;
    (*d).s = 0.0f;
    (*d).v = 1.0f;
    (*d).showAlpha = true;
    (*d).onChange = nullptr;
    (*d).ctx = nullptr;
    return d;
}

void ColorDialog_free(ColorDialog *dialog) {
    if (dialog == nullptr)
        return;

    Dialog_destroy(&(*dialog).base);
    free(dialog);
}

// SETTERS
// ============================================================================

void ColorDialog_setColor(ColorDialog *dialog, uint32_t color) {
    if (dialog == nullptr)
        return;

    (*dialog).color = color;
    if ((*dialog).onChange != nullptr)
        (*dialog).onChange(color, (*dialog).ctx);
}

void ColorDialog_setHSV(ColorDialog *dialog, float h, float s, float v) {
    if (dialog == nullptr)
        return;

    (*dialog).h = h;
    (*dialog).s = s;
    (*dialog).v = v;
}

void ColorDialog_setShowAlpha(ColorDialog *dialog, bool showAlpha) {
    if (dialog == nullptr)
        return;
    (*dialog).showAlpha = showAlpha;
}

void ColorDialog_setOnChange(ColorDialog *dialog, void (*onChange)(uint32_t color, void *ctx), void *ctx) {
    if (dialog == nullptr)
        return;
    (*dialog).onChange = onChange;
    (*dialog).ctx = ctx;
}

// GETTERS
// ============================================================================

Dialog *ColorDialog_getDialog(ColorDialog *dialog) {
    if (dialog == nullptr)
        return nullptr;
    return &(*dialog).base;
}

uint32_t ColorDialog_getColor(const ColorDialog *dialog) {
    if (dialog == nullptr)
        return 0;
    return (*dialog).color;
}

float ColorDialog_getH(const ColorDialog *dialog) {
    if (dialog == nullptr)
        return 0.0f;
    return (*dialog).h;
}

float ColorDialog_getS(const ColorDialog *dialog) {
    if (dialog == nullptr)
        return 0.0f;
    return (*dialog).s;
}

float ColorDialog_getV(const ColorDialog *dialog) {
    if (dialog == nullptr)
        return 0.0f;
    return (*dialog).v;
}

bool ColorDialog_isShowAlpha(const ColorDialog *dialog) {
    if (dialog == nullptr)
        return false;
    return (*dialog).showAlpha;
}

void *ColorDialog_getCtx(const ColorDialog *dialog) {
    if (dialog == nullptr)
        return nullptr;
    return (*dialog).ctx;
}
