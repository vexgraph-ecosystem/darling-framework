#ifndef DARLING_RADIOGROUP_H
#define DARLING_RADIOGROUP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "c23/constructor.h"
#include "darling/panel/panel.h"
#include "event/pointer.h"
#include "oop/type.h"
#include "struct/list.h"

#ifndef TYPE_POINTER
#define TYPE_POINTER 0u
#endif

#define RADIOGROUP_VERTICAL    0
#define RADIOGROUP_HORIZONTAL  1

// darling/field/radiogroup.h — single-choice option list with owned labels.

typedef void (*RadioGroupSelectFn)(void *ctx);

typedef struct RadioGroup {
    Panel base;
    List *options;
    int32_t selected;
    int32_t orientation;
    void (*onSelect)(void *ctx);
    void *ctx;
} RadioGroup;

// Constructors:
//   RadioGroup()        — detached empty group
//   RadioGroup(parent)  — created and attached
RadioGroup *RadioGroup_0(void);
RadioGroup *RadioGroup_1(Panel *parent);

#define RadioGroup(...) CONSTRUCTOR_DISPATCH(RadioGroup, __VA_ARGS__)

// Core.
void RadioGroup_addOption(RadioGroup *g, const char *option);
void RadioGroup_clear(RadioGroup *g);
void RadioGroup_select(RadioGroup *g, int32_t index);

// Live events (Pkg 2).
void RadioGroup_handlePointer(RadioGroup *g, int kind, float localX, float localY);

// Setters.
void RadioGroup_setSelected(RadioGroup *g, int32_t index);
void RadioGroup_setOrientation(RadioGroup *g, int32_t orientation);
void RadioGroup_setOnSelect(RadioGroup *g, RadioGroupSelectFn cb);
void RadioGroup_setCtx(RadioGroup *g, void *ctx);
void RadioGroup_free(RadioGroup *g);

// Getters.
int32_t RadioGroup_getSelected(const RadioGroup *g);
int32_t RadioGroup_getOrientation(const RadioGroup *g);
int32_t RadioGroup_optionCount(const RadioGroup *g);
const char *RadioGroup_getOption(const RadioGroup *g, int32_t index);
RadioGroupSelectFn RadioGroup_getOnSelect(const RadioGroup *g);
void *RadioGroup_getCtx(const RadioGroup *g);

#endif
