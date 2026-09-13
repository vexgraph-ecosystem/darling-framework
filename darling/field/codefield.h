#ifndef DARLING_FIELD_CODEFIELD_H
#define DARLING_FIELD_CODEFIELD_H

#include <stdbool.h>
#include <stdint.h>

#include "c23/constructor.h"
#include "darling/field/textarea.h"
#include "darling/panel/panel.h"

typedef void (*CodeField_ChangeFn)(void *ctx);

typedef struct CodeField {
    Panel base;
    Textarea *editor;
    float gutterWidth;
    uint32_t gutterBackground;
    uint32_t gutterTextColor;
    uint32_t activeLineColor;
    CodeField_ChangeFn onChange;
    void *ctx;
} CodeField;

CodeField *CodeField_0(void);
CodeField *CodeField_1_parent(Panel *parent);
CodeField *CodeField_2(Panel *parent, const char *initialCode);

#define CodeField(...) CONSTRUCTOR_DISPATCH(CodeField, __VA_ARGS__)

void CodeField_free(CodeField *self);

void CodeField_setText(CodeField *self, const char *code);
void CodeField_setGutterWidth(CodeField *self, float width);
void CodeField_setGutterBackground(CodeField *self, uint32_t color);
void CodeField_setGutterTextColor(CodeField *self, uint32_t color);
void CodeField_setActiveLineColor(CodeField *self, uint32_t color);
void CodeField_setOnChange(CodeField *self, CodeField_ChangeFn fn);
void CodeField_setCtx(CodeField *self, void *ctx);

const char *CodeField_getText(const CodeField *self);
float CodeField_getGutterWidth(const CodeField *self);
uint32_t CodeField_getGutterBackground(const CodeField *self);
uint32_t CodeField_getGutterTextColor(const CodeField *self);
uint32_t CodeField_getActiveLineColor(const CodeField *self);
CodeField_ChangeFn CodeField_getOnChange(const CodeField *self);
void *CodeField_getCtx(const CodeField *self);
Textarea *CodeField_getEditor(const CodeField *self);

#endif
