#ifndef DARLING_TYPOGRAPHY_H
#define DARLING_TYPOGRAPHY_H

#include <stdint.h>

#include "c23/constructor.h"
#include "darling/panel/panel.h"
#include "font/font.h"
#include "oop/type.h"


#define TYPOGRAPHY_H1       0
#define TYPOGRAPHY_H2       1
#define TYPOGRAPHY_H3       2
#define TYPOGRAPHY_BODY     3
#define TYPOGRAPHY_CAPTION  4

// darling/label/typography.h — role-styled display text (pure display node).

typedef struct Typography {
    Panel base;
    char *text;
    int32_t role;
    Font *font;
    uint32_t color;
} Typography;

// Constructors:
//   Typography()               — detached empty body text
//   Typography(text)           — detached text with the given string
//   Typography(parent, text)   — created, attached, and set
Typography *Typography_0(void);
Typography *Typography_1(const char *text);
Typography *Typography_2(Panel *parent, const char *text);

#define Typography(...) CONSTRUCTOR_DISPATCH(Typography, __VA_ARGS__)

// Setters.
void Typography_setText(Typography *t, const char *text);
void Typography_setRole(Typography *t, int32_t role);
void Typography_setFont(Typography *t, Font *font);
void Typography_setColor(Typography *t, uint32_t color);
void Typography_free(Typography *t);

// Getters.
const char *Typography_getText(const Typography *t);
int32_t Typography_getRole(const Typography *t);
Font *Typography_getFont(const Typography *t);
uint32_t Typography_getColor(const Typography *t);

#endif
