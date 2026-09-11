#ifndef DARLING_CHECKBOX_H
#define DARLING_CHECKBOX_H

#include <stdbool.h>
#include <stdint.h>

#include "c23/constructor.h"
#include "darling/panel/panel.h"
#include "event/pointer.h"
#include "oop/type.h"


// darling/field/checkbox.h — boolean toggle with an indeterminate state.

typedef void (*CheckboxChangeFn)(void *ctx);

typedef struct Checkbox {
    Panel base;
    bool checked;
    bool indeterminate;
    uint32_t box;
    uint32_t check;
    void (*onChange)(void *ctx);
    void *ctx;
} Checkbox;

// Constructors:
//   Checkbox()        — detached unchecked box
//   Checkbox(parent)  — created and attached
Checkbox *Checkbox_0(void);
Checkbox *Checkbox_1(Panel *parent);

#define Checkbox(...) CONSTRUCTOR_DISPATCH(Checkbox, __VA_ARGS__)

// Core.
void Checkbox_toggle(Checkbox *c);
void Checkbox_handlePointer(Checkbox *c, int kind, float localX, float localY);

// Setters.
void Checkbox_setChecked(Checkbox *c, bool checked);
void Checkbox_setIndeterminate(Checkbox *c, bool value);
void Checkbox_setBox(Checkbox *c, uint32_t color);
void Checkbox_setCheck(Checkbox *c, uint32_t color);
void Checkbox_setOnChange(Checkbox *c, CheckboxChangeFn cb);
void Checkbox_setCtx(Checkbox *c, void *ctx);

// Getters.
bool Checkbox_isChecked(const Checkbox *c);
bool Checkbox_isIndeterminate(const Checkbox *c);
uint32_t Checkbox_getBox(const Checkbox *c);
uint32_t Checkbox_getCheck(const Checkbox *c);
CheckboxChangeFn Checkbox_getOnChange(const Checkbox *c);
void *Checkbox_getCtx(const Checkbox *c);

#endif
