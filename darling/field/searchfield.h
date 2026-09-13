#ifndef DARLING_FIELD_SEARCHFIELD_H
#define DARLING_FIELD_SEARCHFIELD_H

#include <stdbool.h>
#include <stdint.h>

#include "c23/constructor.h"
#include "darling/field/input.h"
#include "darling/panel/panel.h"

typedef void (*SearchField_SearchFn)(const char *query, void *ctx);

typedef struct SearchField {
    Panel base;
    Input *input;
    char *shortcut;
    SearchField_SearchFn onSearch;
    void *ctx;
} SearchField;

SearchField *SearchField_0(void);
SearchField *SearchField_1_parent(Panel *parent);
SearchField *SearchField_1_placeholder(const char *placeholder);
SearchField *SearchField_2(Panel *parent, const char *placeholder);

#define SearchField(...) CONSTRUCTOR_DISPATCH(SearchField, __VA_ARGS__)

void SearchField_free(SearchField *self);

void SearchField_setText(SearchField *self, const char *text);
void SearchField_setPlaceholder(SearchField *self, const char *placeholder);
void SearchField_setShortcut(SearchField *self, const char *shortcut);
void SearchField_setOnSearch(SearchField *self, SearchField_SearchFn fn);
void SearchField_setCtx(SearchField *self, void *ctx);

const char *SearchField_getText(const SearchField *self);
const char *SearchField_getPlaceholder(const SearchField *self);
const char *SearchField_getShortcut(const SearchField *self);
SearchField_SearchFn SearchField_getOnSearch(const SearchField *self);
void *SearchField_getCtx(const SearchField *self);
Input *SearchField_getInput(const SearchField *self);

#endif
