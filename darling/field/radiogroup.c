#include "darling/field/radiogroup.h"

#include <string.h>

#include "annotation/definition.h"
#include "annotation/overview.h"
#include "event/pointer.h"
#include "nio/mem.h"
#include "oop/type.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: RadioGroup
 * ============================================================================
 * Single-choice option list holding owned label strings and a select hook.
 * Option storage allocates copies on add and frees them on clear/free — the
 * only heap traffic in the class, confined to cold edits. RadioGroup_select
 * enforces mutual exclusion and fires onSelect(ctx); RadioGroup_handlePointer
 * divides panel bounds across options by orientation and selects the clicked
 * option on PTR_UP. A leaf R4 field widget with symmetric getters/setters.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: RadioGroup (embeds Panel)
 * LEVEL: L2 — Behavior (single-choice option behavior API)
 * ============================================================================
 * Single-choice option list holding owned label strings and a select hook.
 * Option storage allocates copies on add and frees them on clear/free.
 * RadioGroup_select enforces mutual exclusion and fires onSelect(ctx).
 * RadioGroup_handlePointer divides panel bounds across options by orientation
 * and selects the clicked option on PTR_UP.
 *
 * STRUCT FIELDS (Mirroring darling/field/radiogroup.h):
 * ----------------------------------------------------------------------------
 *   Panel base;                  // Inherited layout, bounds, hierarchy state
 *   List *options;               // Owned C strings; nullptr means empty
 *   int32_t selected;            // Active option index; -1 means none
 *   int32_t orientation;         // 0 means vertical, 1 means horizontal
 *   void (*onSelect)(void *ctx); // Select callback; nullptr means none
 *   void *ctx;                   // Callback context pointer
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - RadioGroup_0(void)
 *   - RadioGroup_1(parent)
 *
 * Core Functions:
 *   - RadioGroup_addOption(g, option)
 *   - RadioGroup_clear(g)
 *   - RadioGroup_select(g, index)
 *   - RadioGroup_handlePointer(g, kind, localX, localY)
 *
 * Setters:
 *   - RadioGroup_setSelected(g, index)
 *   - RadioGroup_setOrientation(g, orientation)
 *   - RadioGroup_setOnSelect(g, cb)
 *   - RadioGroup_setCtx(g, ctx)
 *   - RadioGroup_free(g)
 *
 * Getters:
 *   - RadioGroup_getSelected(g)
 *   - RadioGroup_getOrientation(g)
 *   - RadioGroup_optionCount(g)
 *   - RadioGroup_getOption(g, index)
 *   - RadioGroup_getOnSelect(g)
 *   - RadioGroup_getCtx(g)
 * ============================================================================
 */

// CONSTRUCTORS

RadioGroup *RadioGroup_0(void) {
    RadioGroup *g = (RadioGroup*) Memory_alloc(TYPE_RADIOGROUP_SINGLETON, sizeof(RadioGroup));
    if (!g)
        return nullptr;
    Panel *base = Panel_0();
    if (!base) {
        Memory_free(g);
        return nullptr;
    }
    (*g).base = (*base);
    Memory_free(base);
    (*g).options = List_1(TYPE_POINTER);
    (*g).selected = -1;
    (*g).orientation = RADIOGROUP_VERTICAL;
    (*g).onSelect = nullptr;
    (*g).ctx = nullptr;
    return g;
}

RadioGroup *RadioGroup_1(Panel *parent) {
    RadioGroup *g = RadioGroup_0();
    if (g && parent)
        Panel_addContainer(parent, &(*g).base);
    return g;
}

static void markDirty(RadioGroup *g) {
    if (!g)
        return;
    Panel *b = &(*g).base;
    (void) b;
}

// CORE FUNCTIONS

void RadioGroup_addOption(RadioGroup *g, const char *option) {
    if (!g || !option || !(*g).options)
        return;
    size_t len = strlen(option) + 1;
    char *copy = (char*) Memory_alloc(TYPE_ARRAY, len);
    if (copy) {
        strcpy(copy, option);
        List_add((*g).options, (uint64_t) (uintptr_t) copy);
    }
    markDirty(g);
}

void RadioGroup_clear(RadioGroup *g) {
    if (!g || !(*g).options)
        return;
    List *opts = (*g).options;
    size_t count = List_size(opts);
    for (size_t i = 0; i < count; i++) {
        char *item = (char*) (uintptr_t) List_get(opts, i);
        if (item)
            Memory_free(item);
    }
    List_free(opts);
    (*g).options = List_1(TYPE_POINTER);
    (*g).selected = -1;
    markDirty(g);
}

void RadioGroup_select(RadioGroup *g, int32_t index) {
    if (!g || index == (*g).selected)
        return;
    int32_t count = RadioGroup_optionCount(g);
    if (index < 0 || index >= count)
        return;
    (*g).selected = index;
    markDirty(g);
    void (*fn)(void *ctx) = (*g).onSelect;
    void *ctx = (*g).ctx;
    if (fn)
        fn(ctx);
}

void RadioGroup_handlePointer(RadioGroup *g, int kind, float localX, float localY) {
    if (!g)
        return;
    Panel *p = &(*g).base;
    Component *cnt = &(*p).component;
    float w = (*cnt).w;
    float h = (*cnt).h;
    if (w <= 0.0f)
        w = 120.0f;
    if (h <= 0.0f)
        h = 60.0f;
    bool inside = (localX >= 0.0f && localX <= w && localY >= 0.0f && localY <= h);
    if (kind == PTR_UP && inside) {
        int32_t count = RadioGroup_optionCount(g);
        if (count > 0) {
            int32_t clickedIndex = 0;
            if ((*g).orientation == 1) {
                float colW = w / (float) count;
                clickedIndex = (int32_t) (localX / colW);
            } else {
                float rowH = h / (float) count;
                clickedIndex = (int32_t) (localY / rowH);
            }
            if (clickedIndex >= count)
                clickedIndex = count - 1;
            if (clickedIndex >= 0 && clickedIndex < count)
                RadioGroup_select(g, clickedIndex);
        }
    }
}

// SETTERS

void RadioGroup_setSelected(RadioGroup *g, int32_t index) {
    RadioGroup_select(g, index);
}

void RadioGroup_setOrientation(RadioGroup *g, int32_t orientation) {
    if (!g)
        return;
    (*g).orientation = (orientation == RADIOGROUP_HORIZONTAL) ? 1 : 0;
    markDirty(g);
}

void RadioGroup_setOnSelect(RadioGroup *g, RadioGroupSelectFn cb) {
    if (!g)
        return;
    (*g).onSelect = cb;
}

void RadioGroup_setCtx(RadioGroup *g, void *ctx) {
    if (!g)
        return;
    (*g).ctx = ctx;
}

void RadioGroup_free(RadioGroup *g) {
    if (!g)
        return;
    List *opts = (*g).options;
    if (opts) {
        size_t n = List_size(opts);
        for (size_t i = 0; i < n; i++) {
            char *item = (char*) (uintptr_t) List_get(opts, i);
            if (item)
                Memory_free(item);
        }
        List_free(opts);
        (*g).options = nullptr;
    }
    Memory_free(g);
}

// GETTERS

int32_t RadioGroup_getSelected(const RadioGroup *g) {
    return g ? (*g).selected : -1;
}

int32_t RadioGroup_getOrientation(const RadioGroup *g) {
    return g ? (*g).orientation : RADIOGROUP_VERTICAL;
}

int32_t RadioGroup_optionCount(const RadioGroup *g) {
    if (!g || !(*g).options)
        return 0;
    List *opts = (*g).options;
    return (int32_t) List_size(opts);
}

const char *RadioGroup_getOption(const RadioGroup *g, int32_t index) {
    if (!g || !(*g).options || index < 0)
        return nullptr;
    List *opts = (*g).options;
    if ((size_t) index >= List_size(opts))
        return nullptr;
    return (const char*) (uintptr_t) List_get(opts, (size_t) index);
}

RadioGroupSelectFn RadioGroup_getOnSelect(const RadioGroup *g) {
    return g ? (*g).onSelect : nullptr;
}

void *RadioGroup_getCtx(const RadioGroup *g) {
    return g ? (*g).ctx : nullptr;
}
