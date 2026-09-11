#ifndef DARLING_SLIDER_H
#define DARLING_SLIDER_H

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#include "c23/constructor.h"
#include "darling/panel/panel.h"
#include "event/pointer.h"
#include "oop/type.h"


// darling/field/slider.h — linear value track with optional range thumb.

typedef void (*SliderChangeFn)(void *ctx);

typedef struct Slider {
    Panel base;
    float min;
    float max;
    float value;
    float step;
    float value2;
    bool vertical;
    bool showValue;
    bool range;
    uint32_t fill;
    uint32_t knob;
    void (*onChange)(void *ctx);
    void *ctx;
} Slider;

// Constructors:
//   Slider()        — detached slider on [0, 1]
//   Slider(parent)  — created and attached
Slider *Slider_0(void);
Slider *Slider_1(Panel *parent);

#define Slider(...) CONSTRUCTOR_DISPATCH(Slider, __VA_ARGS__)

// Core.
void Slider_setRange(Slider *s, float min, float max);
void Slider_handlePointer(Slider *s, int kind, float localX, float localY);

// Setters.
void Slider_setMin(Slider *s, float min);
void Slider_setMax(Slider *s, float max);
void Slider_setValue(Slider *s, float value);
void Slider_setStep(Slider *s, float step);
void Slider_setValue2(Slider *s, float value2);
void Slider_setVertical(Slider *s, bool vertical);
void Slider_setShowValue(Slider *s, bool show);
void Slider_setRangeEnabled(Slider *s, bool enabled);
void Slider_setFill(Slider *s, uint32_t color);
void Slider_setKnob(Slider *s, uint32_t color);
void Slider_setOnChange(Slider *s, SliderChangeFn cb);
void Slider_setCtx(Slider *s, void *ctx);

// Getters.
float Slider_getMin(const Slider *s);
float Slider_getMax(const Slider *s);
float Slider_getValue(const Slider *s);
float Slider_getStep(const Slider *s);
float Slider_getValue2(const Slider *s);
bool Slider_isVertical(const Slider *s);
bool Slider_isShowValue(const Slider *s);
bool Slider_isRange(const Slider *s);
uint32_t Slider_getFill(const Slider *s);
uint32_t Slider_getKnob(const Slider *s);
SliderChangeFn Slider_getOnChange(const Slider *s);
void *Slider_getCtx(const Slider *s);

#endif
