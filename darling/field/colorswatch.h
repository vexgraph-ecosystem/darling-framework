#ifndef DARLING_COLORSWATCH_H
#define DARLING_COLORSWATCH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "c23/constructor.h"
#include "darling/panel/panel.h"
#include "oop/type.h"


#define COLORSWATCH_CAPACITY 16

// darling/field/colorswatch.h — fixed palette grid with a single selection.

typedef void (*ColorSwatchSelectFn)(void *ctx);

typedef struct ColorSwatch {
    Panel base;
    uint32_t palette[16];
    int32_t count;
    int32_t selected;
    void (*onSelect)(void *ctx);
    void *ctx;
} ColorSwatch;

// Constructors:
//   ColorSwatch()        — detached empty swatch
//   ColorSwatch(parent)  — created and attached
ColorSwatch *ColorSwatch_0(void);
ColorSwatch *ColorSwatch_1(Panel *parent);

#define ColorSwatch(...) CONSTRUCTOR_DISPATCH(ColorSwatch, __VA_ARGS__)

// Core (shell stub; palette append lands with the picker popup).
bool ColorSwatch_addColor(ColorSwatch *s, uint32_t color);

// Setters.
void ColorSwatch_setSelected(ColorSwatch *s, int32_t index);
void ColorSwatch_setColorAt(ColorSwatch *s, int32_t index, uint32_t color);
void ColorSwatch_setOnSelect(ColorSwatch *s, ColorSwatchSelectFn cb);
void ColorSwatch_setCtx(ColorSwatch *s, void *ctx);

// Getters.
int32_t ColorSwatch_getCount(const ColorSwatch *s);
int32_t ColorSwatch_getSelected(const ColorSwatch *s);
uint32_t ColorSwatch_getColorAt(const ColorSwatch *s, int32_t index);
ColorSwatchSelectFn ColorSwatch_getOnSelect(const ColorSwatch *s);
void *ColorSwatch_getCtx(const ColorSwatch *s);

#endif
