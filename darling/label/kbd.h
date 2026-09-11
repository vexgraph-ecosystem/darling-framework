#ifndef DARLING_KBD_H
#define DARLING_KBD_H

#include <stdint.h>

#include "c23/constructor.h"
#include "darling/panel/panel.h"
#include "oop/type.h"


// darling/label/kbd.h — keyboard-shortcut chip (pure display node).

typedef struct Kbd {
    Panel base;
    char *keys;
    uint32_t bg;
    uint32_t fg;
} Kbd;

// Constructors:
//   Kbd()               — detached empty chip
//   Kbd(keys)           — detached chip with the given keys
//   Kbd(parent, keys)   — created, attached, and set
Kbd *Kbd_0(void);
Kbd *Kbd_1(const char *keys);
Kbd *Kbd_2(Panel *parent, const char *keys);

#define Kbd(...) CONSTRUCTOR_DISPATCH(Kbd, __VA_ARGS__)

// Setters.
void Kbd_setKeys(Kbd *k, const char *keys);
void Kbd_setBg(Kbd *k, uint32_t color);
void Kbd_setFg(Kbd *k, uint32_t color);
void Kbd_free(Kbd *k);

// Getters.
const char *Kbd_getKeys(const Kbd *k);
uint32_t Kbd_getBg(const Kbd *k);
uint32_t Kbd_getFg(const Kbd *k);

#endif
