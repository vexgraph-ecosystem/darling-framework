#include "darling/field/searchfield.h"

#include <stdlib.h>
#include <string.h>

#include "event/pointer.h"
#include "nio/mem.h"
#include "oop/type.h"
#include "text/text_core.h"
#include "vulkan/texture/texture.h"
#include "vulkan/vk.h"
#include "annotation/definition.h"
#include "annotation/overview.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: SearchField
 * ============================================================================
 * Search input composite with a search icon, clear button, and shortcut
 * badge, wrapping an inner Input component and coordinating search events.
 * The composite paints only its own ordered part pipeline — background
 * (+ inner layout), image (icon + badge), border — while inner text and
 * caret paint through the child Input's own pipeline. The Input child is
 * owned and freed in SearchField_free; shortcut is an owned string;
 * onSearch(ctx) fires with the query on inner submit. A composite R4 field
 * widget over the Input base.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: SearchField (inherits Panel)
 * LEVEL: L2 — Behavior (search input composite)
 * ============================================================================
 * Search input composite with search icon, clear button, and shortcut badge.
 * Wraps an inner Input component and coordinates search events.
 *
 * Ordered part pipeline: background (+ inner layout) -> image (icon + badge)
 * -> border. Inner text/caret paint via the child Input's own pipeline.
 *
 * STRUCT FIELDS (Mirroring darling/field/searchfield.h):
 * ----------------------------------------------------------------------------
 *   Panel base;                  // Inherited layout, bounds, hierarchy state
 *   Input *input;                // Owned child Input (text editing)
 *   char *shortcut;              // Owned shortcut badge string; nullptr = none
 *   SearchField_SearchFn onSearch; // Search callback; nullptr means none
 *   void *ctx;                   // Callback context pointer
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - SearchField_0(void)
 *   - SearchField_1_parent(parent)
 *   - SearchField_1_placeholder(placeholder)
 *   - SearchField_2(parent, placeholder)
 *
 * Core Functions:
 *   - SearchField_free(self)
 *
 * Setters:
 *   - SearchField_setText(self, text)
 *   - SearchField_setPlaceholder(self, placeholder)
 *   - SearchField_setShortcut(self, shortcut)
 *   - SearchField_setOnSearch(self, fn)
 *   - SearchField_setCtx(self, ctx)
 *
 * Getters:
 *   - SearchField_getText(self)
 *   - SearchField_getPlaceholder(self)
 *   - SearchField_getShortcut(self)
 *   - SearchField_getOnSearch(self)
 *   - SearchField_getCtx(self)
 *   - SearchField_getInput(self)
 * ============================================================================
 */

static void onInnerSubmit(void *ctx) {
    SearchField *sf = (SearchField*) ctx;
    if (!sf) return;
    if (sf->onSearch) {
        sf->onSearch(Input_getText(sf->input), sf->ctx);
    }
}

static void onInnerChange(void *ctx) {
    SearchField *sf = (SearchField*) ctx;
    if (!sf) return;
    Panel *bp = &(*sf).base;
    Container_markDirty(&(*bp).base);
}

// Ordered part pipeline: background (+ inner layout) -> image (icon + badge)
// -> border. Icon/badge sit inside the padded content area and never touch
// the 1px edge, so the canonical border-over-content order is pixel-identical
// to the legacy background/border/icon sequence.

// Stage 0: field fill + inner Input layout (layout-in-paint preserved;
// future work moves this to the layout pass next to Container_resolve).
static bool searchPaintBackground(Panel *panel, void *renderer, void *cmdBuffer,
                                  float surfaceW, float surfaceH,
                                  float x, float y, float w, float h) {
    SearchField *sf = (SearchField*) panel;
    (void) renderer;
    if (!sf || !cmdBuffer)
        return false;
    if (w <= 0.0f || h <= 0.0f)
        return false;
    Container *c = &(*panel).base;
    float op = Container_getOpacity(c);
    if (op <= 0.0f)
        return false;
    Input *field = (*sf).input;
    if (field) {
        char *keys = (*sf).shortcut;
        float rightPad = (keys && keys[0] != '\0') ? 48.0f : 28.0f;
        float inW = w - 28.0f - rightPad;
        if (inW < 10.0f)
            inW = 10.0f;
        Panel *inner = &(*field).base;
        Container *ic = &(*inner).base;
        Container_setLocation(ic, 28.0f, 0.0f);
        Container_setSize(ic, inW, h);
    }
    uint32_t bg = Panel_getBackgroundColor(panel);
    if ((bg >> 24) == 0)
        bg = 0xFF18181Bu;
    float br = ((bg >> 16) & 0xFF) / 255.0f;
    float bgc = ((bg >> 8) & 0xFF) / 255.0f;
    float bb = (bg & 0xFF) / 255.0f;
    float ba = ((bg >> 24) & 0xFF) / 255.0f * op;
    if (ba <= 0.0f)
        return false;
    Vk_fillRect(cmdBuffer, surfaceW, surfaceH, x, y, w, h, br, bgc, bb, ba);
    return true;
}

// Stage 1: leading magnifier vector + trailing shortcut badge.
static bool searchPaintImage(Panel *panel, void *renderer, void *cmdBuffer,
                             float surfaceW, float surfaceH,
                             float x, float y, float w, float h) {
    SearchField *sf = (SearchField*) panel;
    (void) renderer;
    if (!sf || !cmdBuffer)
        return false;
    if (w <= 0.0f || h <= 0.0f)
        return false;
    Container *c = &(*panel).base;
    float op = Container_getOpacity(c);
    if (op <= 0.0f)
        return false;
    float iconCx = x + 14.0f;
    float iconCy = y + h * 0.5f + 1.0f;
    float iconR = 4.0f;
    float iconCol_r = 0.6f, iconCol_g = 0.6f, iconCol_b = 0.65f, iconCol_a = 0.9f * op;
    Vk_fillRect(cmdBuffer, surfaceW, surfaceH, iconCx - iconR, iconCy - iconR, iconR * 2.0f, 1.0f, iconCol_r, iconCol_g, iconCol_b, iconCol_a);
    Vk_fillRect(cmdBuffer, surfaceW, surfaceH, iconCx - iconR, iconCy + iconR, iconR * 2.0f, 1.0f, iconCol_r, iconCol_g, iconCol_b, iconCol_a);
    Vk_fillRect(cmdBuffer, surfaceW, surfaceH, iconCx - iconR, iconCy - iconR, 1.0f, iconR * 2.0f, iconCol_r, iconCol_g, iconCol_b, iconCol_a);
    Vk_fillRect(cmdBuffer, surfaceW, surfaceH, iconCx + iconR, iconCy - iconR, 1.0f, iconR * 2.0f, iconCol_r, iconCol_g, iconCol_b, iconCol_a);
    Vk_fillRect(cmdBuffer, surfaceW, surfaceH, iconCx + 3.0f, iconCy - 5.0f, 3.0f, 1.5f, iconCol_r, iconCol_g, iconCol_b, iconCol_a);
    if ((*sf).shortcut && (*sf).shortcut[0] != '\0') {
        float badgeW = 32.0f;
        float badgeH = 18.0f;
        float badgeX = x + w - badgeW - 8.0f;
        float badgeY = y + (h - badgeH) * 0.5f;
        Vk_fillRect(cmdBuffer, surfaceW, surfaceH, badgeX, badgeY, badgeW, badgeH, 0.2f, 0.2f, 0.23f, op);
    }
    return true;
}

// Stage 3: focused/idle stroke over content.
static bool searchPaintBorder(Panel *panel, void *renderer, void *cmdBuffer,
                              float surfaceW, float surfaceH,
                              float x, float y, float w, float h) {
    SearchField *sf = (SearchField*) panel;
    (void) renderer;
    if (!sf || !cmdBuffer)
        return false;
    if (w <= 0.0f || h <= 0.0f)
        return false;
    Container *c = &(*panel).base;
    float op = Container_getOpacity(c);
    if (op <= 0.0f)
        return false;
    Input *inner = (*sf).input;
    bool focused = inner && Input_isFocused(inner);
    uint32_t borderColor = focused ? 0xFF3B82F6u : 0xFF3F3F46u;
    float b_r = ((borderColor >> 16) & 0xFF) / 255.0f;
    float b_g = ((borderColor >> 8) & 0xFF) / 255.0f;
    float b_b = (borderColor & 0xFF) / 255.0f;
    float b_a = ((borderColor >> 24) & 0xFF) / 255.0f * op;
    if (b_a <= 0.0f)
        return false;
    Vk_fillRect(cmdBuffer, surfaceW, surfaceH, x, y, w, 1.0f, b_r, b_g, b_b, b_a);
    Vk_fillRect(cmdBuffer, surfaceW, surfaceH, x, y + h - 1.0f, w, 1.0f, b_r, b_g, b_b, b_a);
    Vk_fillRect(cmdBuffer, surfaceW, surfaceH, x, y, 1.0f, h, b_r, b_g, b_b, b_a);
    Vk_fillRect(cmdBuffer, surfaceW, surfaceH, x + w - 1.0f, y, 1.0f, h, b_r, b_g, b_b, b_a);
    return true;
}

static void markDirty(SearchField *self) {
    if (!self) return;
    Panel *bp = &(*self).base;
    Container_markDirty(&(*bp).base);
}

SearchField *SearchField_0(void) {
    SearchField *sf = (SearchField*) Memory_alloc(TYPE_SEARCHFIELD_SINGLETON, sizeof(SearchField));
    if (!sf)
        return nullptr;
    Panel *bp = Panel_0();
    if (!bp) {
        Memory_free(sf);
        return nullptr;
    }
    (*sf).base = (*bp);
    Memory_free(bp);

    (*sf).shortcut = nullptr;
    (*sf).onSearch = nullptr;
    (*sf).ctx = nullptr;

    // Create inner input
    (*sf).input = Input_0();
    if ((*sf).input) {
        Panel_setBackgroundColor(&(*sf).input->base, 0x00000000u); // transparent
        Input_setPlaceholder((*sf).input, "Search...");
        Input_setOnSubmit((*sf).input, onInnerSubmit);
        Input_setOnChange((*sf).input, onInnerChange);
        Input_setCtx((*sf).input, sf);
        Panel_addContainer(&(*sf).base, &(*sf).input->base);
    }

    Panel *sp = &(*sf).base;
    Panel_setRenderHandler(sp, nullptr);
    Panel_setBackgroundFn(sp, searchPaintBackground);
    Panel_setImageFn(sp, searchPaintImage);
    Panel_setBorderFn(sp, searchPaintBorder);
    return sf;
}

SearchField *SearchField_1_parent(Panel *parent) {
    SearchField *sf = SearchField_0();
    if (sf && parent) {
        Panel_addContainer(parent, &(*sf).base);
    }
    return sf;
}

SearchField *SearchField_1_placeholder(const char *placeholder) {
    SearchField *sf = SearchField_0();
    if (sf && placeholder) {
        SearchField_setPlaceholder(sf, placeholder);
    }
    return sf;
}

SearchField *SearchField_2(Panel *parent, const char *placeholder) {
    SearchField *sf = SearchField_0();
    if (sf) {
        if (placeholder)
            SearchField_setPlaceholder(sf, placeholder);
        if (parent)
            Panel_addContainer(parent, &(*sf).base);
    }
    return sf;
}

void SearchField_free(SearchField *self) {
    if (!self) return;
    if ((*self).shortcut) {
        Memory_free((*self).shortcut);
        (*self).shortcut = nullptr;
    }
    if ((*self).input) {
        Input_free((*self).input);
        (*self).input = nullptr;
    }
    (*self).onSearch = nullptr;
    (*self).ctx = nullptr;
    Memory_free(self);
}

void SearchField_setText(SearchField *self, const char *text) {
    if (!self || !(*self).input) return;
    Input_setText((*self).input, text);
    markDirty(self);
}

void SearchField_setPlaceholder(SearchField *self, const char *placeholder) {
    if (!self || !(*self).input) return;
    Input_setPlaceholder((*self).input, placeholder);
    markDirty(self);
}

void SearchField_setShortcut(SearchField *self, const char *shortcut) {
    if (!self) return;
    if ((*self).shortcut) {
        Memory_free((*self).shortcut);
        (*self).shortcut = nullptr;
    }
    if (shortcut) {
        size_t len = strlen(shortcut) + 1;
        char *buf = (char*) Memory_alloc(TYPE_ARRAY, len);
        if (buf) {
            memcpy(buf, shortcut, len);
            (*self).shortcut = buf;
        }
    }
    markDirty(self);
}

void SearchField_setOnSearch(SearchField *self, SearchField_SearchFn fn) {
    if (!self) return;
    (*self).onSearch = fn;
}

void SearchField_setCtx(SearchField *self, void *ctx) {
    if (!self) return;
    (*self).ctx = ctx;
}

const char *SearchField_getText(const SearchField *self) {
    return (self && (*self).input) ? Input_getText((*self).input) : nullptr;
}

const char *SearchField_getPlaceholder(const SearchField *self) {
    return (self && (*self).input) ? Input_getPlaceholder((*self).input) : nullptr;
}

const char *SearchField_getShortcut(const SearchField *self) {
    return self ? (*self).shortcut : nullptr;
}

SearchField_SearchFn SearchField_getOnSearch(const SearchField *self) {
    return self ? (*self).onSearch : nullptr;
}

void *SearchField_getCtx(const SearchField *self) {
    return self ? (*self).ctx : nullptr;
}

Input *SearchField_getInput(const SearchField *self) {
    return self ? (*self).input : nullptr;
}
