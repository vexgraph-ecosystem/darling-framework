#ifndef DARLING_BUTTON_H
#define DARLING_BUTTON_H

#include <stdbool.h>
#include <stdint.h>

#include "c23/constructor.h"
#include "darling/panel/panel.h"
#include "event/pointer.h"
#include "font/font.h"
#include "oop/type.h"

// darling/button/button.h — pressable button shell
// (a Panel with an owned label, font styling, state colors, and a press
// callback; hit-testing and dispatch land in a later pass).

// Central registry owns these once landed; the guard keeps this shell
// compiling standalone until then.

typedef struct Button {
    Panel base;
    char *label;
    Font *font;
    float fontSize;
    uint32_t textColor;
    uint32_t bg;
    uint32_t bgHover;
    uint32_t bgPressed;
    uint32_t borderColor;
    float radius;
    float borderWidth;
    bool disabled;
    bool hovered;
    bool pressed;
    void (*onPress)(void *ctx);
    void *ctx;
} Button;

// Constructors:
//   Button()                 — detached shell, empty label
//   Button(label)            — detached shell with copied label
//   Button(parent, label)    — created, labeled, and attached
Button *Button_0(void);
Button *Button_1(const char *label);
Button *Button_2(Panel *parent, const char *label);

#define Button(...) CONSTRUCTOR_DISPATCH(Button, __VA_ARGS__)

// Press dispatch.
void Button_press(Button *b);
void Button_handlePointer(Button *b, int kind, float localX, float localY);
void Button_free(Button *b);

const char *Button_getLabel(const Button *b);
void Button_setLabel(Button *b, const char *label);
Font *Button_getFont(const Button *b);
void Button_setFont(Button *b, Font *font);
float Button_getFontSize(const Button *b);
void Button_setFontSize(Button *b, float size);
uint32_t Button_getTextColor(const Button *b);
void Button_setTextColor(Button *b, uint32_t color);
uint32_t Button_getBackground(const Button *b);
void Button_setBackground(Button *b, uint32_t color);
uint32_t Button_getBackgroundHover(const Button *b);
void Button_setBackgroundHover(Button *b, uint32_t color);
uint32_t Button_getBackgroundPressed(const Button *b);
void Button_setBackgroundPressed(Button *b, uint32_t color);
uint32_t Button_getBorderColor(const Button *b);
void Button_setBorderColor(Button *b, uint32_t color);
float Button_getRadius(const Button *b);
void Button_setRadius(Button *b, float radius);
float Button_getBorderWidth(const Button *b);
void Button_setBorderWidth(Button *b, float width);
bool Button_isDisabled(const Button *b);
void Button_setDisabled(Button *b, bool disabled);
bool Button_isHovered(const Button *b);
void Button_setHovered(Button *b, bool hovered);
bool Button_isPressed(const Button *b);
void Button_setPressed(Button *b, bool pressed);
void Button_setOnPress(Button *b, void (*fn)(void *ctx), void *ctx);
void (*Button_getOnPress(const Button *b))(void *ctx);
void *Button_getPressContext(const Button *b);

#endif
