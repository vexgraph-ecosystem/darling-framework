#ifndef DARLING_SWITCH_H
#define DARLING_SWITCH_H

#include <stdbool.h>
#include <stdint.h>

#include "c23/constructor.h"
#include "darling/panel/panel.h"
#include "event/pointer.h"
#include "oop/type.h"

// darling/button/switch.h — on/off toggle switch shell
// (a Panel with a boolean state, track/knob colors, and a change callback).

// Central registry owns these once landed; the guard keeps this shell
// compiling standalone until then.

typedef struct Switch {
    Panel base;
    bool on;
    uint32_t trackOn;
    uint32_t trackOff;
    uint32_t knob;
    void (*onChange)(void *ctx);
    void *ctx;
} Switch;

// Constructors:
//   Switch()           — detached shell, off
//   Switch(parent)     — created and attached
Switch *Switch_0(void);
Switch *Switch_1(Panel *parent);

#define Switch(...) CONSTRUCTOR_DISPATCH(Switch, __VA_ARGS__)

// State flip (real: routes through setOn so the callback fires).
void Switch_toggle(Switch *s);
void Switch_handlePointer(Switch *s, int kind, float localX, float localY);

bool Switch_isOn(const Switch *s);
void Switch_setOn(Switch *s, bool on);
uint32_t Switch_getTrackOn(const Switch *s);
void Switch_setTrackOn(Switch *s, uint32_t color);
uint32_t Switch_getTrackOff(const Switch *s);
void Switch_setTrackOff(Switch *s, uint32_t color);
uint32_t Switch_getKnob(const Switch *s);
void Switch_setKnob(Switch *s, uint32_t color);
void Switch_setOnChange(Switch *s, void (*fn)(void *ctx), void *ctx);
void (*Switch_getOnChange(const Switch *s))(void *ctx);
void *Switch_getChangeContext(const Switch *s);

#endif
