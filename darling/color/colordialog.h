#ifndef DARLING_COLOR_COLORDIALOG_H
#define DARLING_COLOR_COLORDIALOG_H

#include <stdbool.h>
#include <stdint.h>

#include "c23/constructor.h"
#include "darling/dialog/dialog.h"

#ifdef __cplusplus
extern "C" {
#endif

// darling/color/colordialog.h — modal color editor inheriting Dialog.

typedef struct ColorDialog {
    Dialog base;          // Inherited Dialog (inherits Frame)
    uint32_t color;       // Packed 0xAARRGGBB selection
    float h, s, v;        // HSV mirror of color
    bool showAlpha;       // Alpha channel editing enabled
    void (*onChange)(uint32_t color, void *ctx);
    void *ctx;
} ColorDialog;

// Constructors:
//   ColorDialog()
//   ColorDialog(initialColor)
//   ColorDialog(title, initialColor)
ColorDialog *ColorDialog_0(void);
ColorDialog *ColorDialog_1(uint32_t initialColor);
ColorDialog *ColorDialog_2(const char *title, uint32_t initialColor);

#define ColorDialog(...) CONSTRUCTOR_DISPATCH(ColorDialog, __VA_ARGS__)

void ColorDialog_free(ColorDialog *dialog);

// Setters:
void ColorDialog_setColor(ColorDialog *dialog, uint32_t color);
void ColorDialog_setHSV(ColorDialog *dialog, float h, float s, float v);
void ColorDialog_setShowAlpha(ColorDialog *dialog, bool showAlpha);
void ColorDialog_setOnChange(ColorDialog *dialog, void (*onChange)(uint32_t color, void *ctx), void *ctx);

// Getters:
Dialog *ColorDialog_getDialog(ColorDialog *dialog);
uint32_t ColorDialog_getColor(const ColorDialog *dialog);
float ColorDialog_getH(const ColorDialog *dialog);
float ColorDialog_getS(const ColorDialog *dialog);
float ColorDialog_getV(const ColorDialog *dialog);
bool ColorDialog_isShowAlpha(const ColorDialog *dialog);
void *ColorDialog_getCtx(const ColorDialog *dialog);

#ifdef __cplusplus
}
#endif

#endif // DARLING_COLOR_COLORDIALOG_H
