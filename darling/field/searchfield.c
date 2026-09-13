#include "darling/field/searchfield.h"

#include <stdlib.h>
#include <string.h>

#include "event/pointer.h"
#include "nio/mem.h"
#include "oop/type.h"
#include "text/text_core.h"
#include "vulkan/texture/texture.h"
#include "vulkan/vk.h"
#include "annotation/overview.h"

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: SearchField (inherits Panel, LEVEL L2 Behavior)
 * ============================================================================
 * Search input composite with search icon, clear button, and shortcut badge.
 * Wraps an inner Input component and coordinates search events.
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

static void SearchField_renderFn(Panel *panel, void *renderer, void *cmdBuffer, float surfaceW, float surfaceH,
                                float x, float y, float w, float h) {
    SearchField *sf = (SearchField*) panel;
    (void) renderer;
    if (!sf || w <= 0.0f || h <= 0.0f)
        return;
    float op = Container_getOpacity(&(*panel).base);
    if (op <= 0.0f)
        return;

    // Background
    uint32_t bg = Panel_getBackgroundColor(panel);
    if ((bg >> 24) == 0)
        bg = 0xFF18181Bu;
    float br = ((bg >> 16) & 0xFF) / 255.0f;
    float bgc = ((bg >> 8) & 0xFF) / 255.0f;
    float bb = (bg & 0xFF) / 255.0f;
    float ba = ((bg >> 24) & 0xFF) / 255.0f * op;
    Vk_fillRect(cmdBuffer, surfaceW, surfaceH, x, y, w, h, br, bgc, bb, ba);

    // Border
    bool focused = sf->input && Input_isFocused(sf->input);
    uint32_t borderColor = focused ? 0xFF3B82F6u : 0xFF3F3F46u;
    float b_r = ((borderColor >> 16) & 0xFF) / 255.0f;
    float b_g = ((borderColor >> 8) & 0xFF) / 255.0f;
    float b_b = (borderColor & 0xFF) / 255.0f;
    float b_a = ((borderColor >> 24) & 0xFF) / 255.0f * op;
    Vk_fillRect(cmdBuffer, surfaceW, surfaceH, x, y, w, 1.0f, b_r, b_g, b_b, b_a);
    Vk_fillRect(cmdBuffer, surfaceW, surfaceH, x, y + h - 1.0f, w, 1.0f, b_r, b_g, b_b, b_a);
    Vk_fillRect(cmdBuffer, surfaceW, surfaceH, x, y, 1.0f, h, b_r, b_g, b_b, b_a);
    Vk_fillRect(cmdBuffer, surfaceW, surfaceH, x + w - 1.0f, y, 1.0f, h, b_r, b_g, b_b, b_a);

    // Layout inner input
    if (sf->input) {
        float rightPad = (sf->shortcut && sf->shortcut[0] != '\0') ? 48.0f : 28.0f;
        float inW = w - 28.0f - rightPad;
        if (inW < 10.0f) inW = 10.0f;
        Container_setLocation(&(*sf->input).base.base, 28.0f, 0.0f);
        Container_setSize(&(*sf->input).base.base, inW, h);
    }

    // Leading search icon: draw magnifying glass vector
    float iconCx = x + 14.0f;
    float iconCy = y + h * 0.5f + 1.0f;
    float iconR = 4.0f;
    float iconCol_r = 0.6f, iconCol_g = 0.6f, iconCol_b = 0.65f, iconCol_a = 0.9f * op;
    // Glass circle outline (approximate with rects)
    Vk_fillRect(cmdBuffer, surfaceW, surfaceH, iconCx - iconR, iconCy - iconR, iconR * 2.0f, 1.0f, iconCol_r, iconCol_g, iconCol_b, iconCol_a);
    Vk_fillRect(cmdBuffer, surfaceW, surfaceH, iconCx - iconR, iconCy + iconR, iconR * 2.0f, 1.0f, iconCol_r, iconCol_g, iconCol_b, iconCol_a);
    Vk_fillRect(cmdBuffer, surfaceW, surfaceH, iconCx - iconR, iconCy - iconR, 1.0f, iconR * 2.0f, iconCol_r, iconCol_g, iconCol_b, iconCol_a);
    Vk_fillRect(cmdBuffer, surfaceW, surfaceH, iconCx + iconR, iconCy - iconR, 1.0f, iconR * 2.0f, iconCol_r, iconCol_g, iconCol_b, iconCol_a);
    // Handle
    Vk_fillRect(cmdBuffer, surfaceW, surfaceH, iconCx + 3.0f, iconCy - 5.0f, 3.0f, 1.5f, iconCol_r, iconCol_g, iconCol_b, iconCol_a);

    // Trailing shortcut badge
    if (sf->shortcut && sf->shortcut[0] != '\0') {
        float badgeW = 32.0f;
        float badgeH = 18.0f;
        float badgeX = x + w - badgeW - 8.0f;
        float badgeY = y + (h - badgeH) * 0.5f;
        Vk_fillRect(cmdBuffer, surfaceW, surfaceH, badgeX, badgeY, badgeW, badgeH, 0.2f, 0.2f, 0.23f, op);
    }
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

    Panel_setRenderHandler(&(*sf).base, SearchField_renderFn);
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
