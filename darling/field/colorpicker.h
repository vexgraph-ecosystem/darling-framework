#ifndef DARLING_COLORPICKER_H
#define DARLING_COLORPICKER_H

#include <stdbool.h>
#include <stdint.h>

#include "c23/constructor.h"
#include "darling/panel/panel.h"
#include "oop/type.h"


// darling/field/colorpicker.h — packed color with an HSV editing mirror.

typedef void (*ColorPickerChangeFn)(void *ctx);

typedef struct ColorPicker {
    Panel base;
    uint32_t color;
    float h;
    float s;
    float v;
    bool showAlpha;
    bool showHex;
    void (*onChange)(void *ctx);
    void *ctx;
} ColorPicker;

// Constructors:
//   ColorPicker()        — detached white picker
//   ColorPicker(parent)  — created and attached
ColorPicker *ColorPicker_0(void);
ColorPicker *ColorPicker_1(Panel *parent);

#define ColorPicker(...) CONSTRUCTOR_DISPATCH(ColorPicker, __VA_ARGS__)

// Core (shell stub; HSV packing lands with the render pass).
void ColorPicker_setHSV(ColorPicker *c, float h, float s, float v);

// Setters.
void ColorPicker_setColor(ColorPicker *c, uint32_t color);
void ColorPicker_setH(ColorPicker *c, float h);
void ColorPicker_setS(ColorPicker *c, float s);
void ColorPicker_setV(ColorPicker *c, float v);
void ColorPicker_setShowAlpha(ColorPicker *c, bool show);
void ColorPicker_setShowHex(ColorPicker *c, bool show);
void ColorPicker_setOnChange(ColorPicker *c, ColorPickerChangeFn cb);
void ColorPicker_setCtx(ColorPicker *c, void *ctx);

// Getters.
uint32_t ColorPicker_getColor(const ColorPicker *c);
float ColorPicker_getH(const ColorPicker *c);
float ColorPicker_getS(const ColorPicker *c);
float ColorPicker_getV(const ColorPicker *c);
bool ColorPicker_isShowAlpha(const ColorPicker *c);
bool ColorPicker_isShowHex(const ColorPicker *c);
ColorPickerChangeFn ColorPicker_getOnChange(const ColorPicker *c);
void *ColorPicker_getCtx(const ColorPicker *c);

#endif
