#ifndef DARLING_KNOB_H
#define DARLING_KNOB_H

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#include "c23/constructor.h"
#include "darling/panel/panel.h"
#include "event/pointer.h"
#include "oop/type.h"


// darling/field/knob.h — rotary value dial over an angular sweep.

typedef void (*KnobChangeFn)(void *ctx);

typedef struct Knob {
    Panel base;
    float min;
    float max;
    float value;
    float startAngle;
    float sweep;
    float diameter;
    void (*onChange)(void *ctx);
    void *ctx;
} Knob;

// Constructors:
//   Knob()        — detached dial on [0, 1]
//   Knob(parent)  — created and attached
Knob *Knob_0(void);
Knob *Knob_1(Panel *parent);

#define Knob(...) CONSTRUCTOR_DISPATCH(Knob, __VA_ARGS__)

// Core.
void Knob_setNormalized(Knob *k, float t);
void Knob_handlePointer(Knob *k, int kind, float localX, float localY);

// Setters.
void Knob_setMin(Knob *k, float min);
void Knob_setMax(Knob *k, float max);
void Knob_setValue(Knob *k, float value);
void Knob_setStartAngle(Knob *k, float angle);
void Knob_setSweep(Knob *k, float sweep);
void Knob_setDiameter(Knob *k, float diameter);
void Knob_setOnChange(Knob *k, KnobChangeFn cb);
void Knob_setCtx(Knob *k, void *ctx);

// Getters.
float Knob_getMin(const Knob *k);
float Knob_getMax(const Knob *k);
float Knob_getValue(const Knob *k);
float Knob_getStartAngle(const Knob *k);
float Knob_getSweep(const Knob *k);
float Knob_getDiameter(const Knob *k);
KnobChangeFn Knob_getOnChange(const Knob *k);
void *Knob_getCtx(const Knob *k);

#endif
