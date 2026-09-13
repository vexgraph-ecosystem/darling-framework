#include "darling/field/codefield.h"

#include <stdio.h>
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
 * CLASS: CodeField (inherits Panel, LEVEL L2 Behavior)
 * ============================================================================
 * Code editor widget with line number gutter and active line indicator.
 * Wraps a Textarea editor configured for monospace source code.
 * ============================================================================
 */

#define CODEFIELD_DEFAULT_GUTTER_W 40.0f

static void onEditorChange(void *ctx) {
    CodeField *cf = (CodeField*) ctx;
    if (!cf) return;
    Panel *bp = &(*cf).base;
    Container_markDirty(&(*bp).base);
    if (cf->onChange) {
        cf->onChange(cf->ctx);
    }
}

static void markDirty(CodeField *self) {
    if (!self) return;
    Panel *bp = &(*self).base;
    Container_markDirty(&(*bp).base);
}

static void CodeField_renderFn(Panel *panel, void *renderer, void *cmdBuffer, float surfaceW, float surfaceH,
                              float x, float y, float w, float h) {
    CodeField *cf = (CodeField*) panel;
    (void) renderer;
    if (!cf || w <= 0.0f || h <= 0.0f)
        return;
    float op = Container_getOpacity(&(*panel).base);
    if (op <= 0.0f)
        return;

    float gw = (*cf).gutterWidth > 0.0f ? (*cf).gutterWidth : CODEFIELD_DEFAULT_GUTTER_W;

    // 1. Gutter background
    uint32_t gbg = (*cf).gutterBackground;
    float gr = ((gbg >> 16) & 0xFF) / 255.0f;
    float gg = ((gbg >> 8) & 0xFF) / 255.0f;
    float gb = (gbg & 0xFF) / 255.0f;
    float ga = ((gbg >> 24) & 0xFF) / 255.0f * op;
    Vk_fillRect(cmdBuffer, surfaceW, surfaceH, x, y, gw, h, gr, gg, gb, ga);

    // 2. Gutter divider line
    Vk_fillRect(cmdBuffer, surfaceW, surfaceH, x + gw - 1.0f, y, 1.0f, h, 0.25f, 0.25f, 0.28f, op);

    // 3. Layout inner editor
    if (cf->editor) {
        float edW = w - gw;
        if (edW < 10.0f) edW = 10.0f;
        Container_setLocation(&(*cf->editor).base.base, gw, 0.0f);
        Container_setSize(&(*cf->editor).base.base, edW, h);
    }

    // 4. Draw line numbers in gutter
    if (cf->editor) {
        const char *txt = Textarea_getText(cf->editor);
        int32_t lines = 1;
        if (txt) {
            for (size_t i = 0; txt[i] != '\0'; i++) {
                if (txt[i] == '\n') lines++;
            }
        }

        // Active line
        int32_t cur = Textarea_getCursor(cf->editor);
        int32_t activeLine = 0;
        if (txt) {
            for (int32_t i = 0; i < cur && txt[i] != '\0'; i++) {
                if (txt[i] == '\n') activeLine++;
            }
        }

        float padY = 8.0f;
        float lineH = Textarea_getFontSize(cf->editor) * 1.35f + Textarea_getSpacingHeight(cf->editor);
        float scrollY = Textarea_getScrollY(cf->editor);

        float backing = TextCore_backingScale();
        if (backing <= 0.0f) backing = 1.0f;
        float pxH = 11.0f * backing;

        for (int32_t l = 0; l < lines; l++) {
            float lineY = y + h - padY - (float)(l + 1) * lineH + scrollY;
            if (lineY + lineH < y || lineY > y + h)
                continue;

            bool isActive = (l == activeLine) && Textarea_isFocused(cf->editor);
            uint32_t numCol = isActive ? (*cf).activeLineColor : (*cf).gutterTextColor;

            if (isActive) {
                // Active line gutter marker indicator
                Vk_fillRect(cmdBuffer, surfaceW, surfaceH, x + gw - 2.5f, lineY, 2.5f, lineH, 0.23f, 0.51f, 0.96f, op);
            }

            char numStr[16];
            snprintf(numStr, sizeof(numStr), "%d", l + 1);

            TextStyleDescriptor style = {
                .ligatures = false,
                .spacingWidth = 0.0f,
                .spacingHeight = 0.0f,
                .underline = UNDERLINE_NONE,
                .underlineColor = 0,
                .mnemonicIndex = -1,
                .selectionStart = -1,
                .selectionEnd = -1,
                .highlightRadius = 0.0f,
                .highlightColor = 0,
                .align = TEXT_ALIGN_RIGHT,
                .boundsWidth = gw - 8.0f,
            };

            uint8_t *rgba = nullptr;
            int rw = 0, rh = 0;
            bool ok = TextCore_rasterStyled(numStr, "Menlo", pxH, numCol, &style, &rgba, &rw, &rh);
            if (ok && rgba && rw > 0 && rh > 0) {
                int32_t tex = Texture_loadRaw(rgba, (uint32_t) rw, (uint32_t) rh);
                float qw = (float) rw / backing;
                float qh = (float) rh / backing;
                float qx = x + gw - 8.0f - qw;
                float qy = lineY + (lineH - qh) * 0.5f;
                Vk_drawTexture(cmdBuffer, surfaceW, surfaceH, qx, qy, qw, qh, 1.0f, 1.0f, 1.0f, op,
                               tex, PICTURE_MODE_FIT, (float) rw, (float) rh);
                Texture_free(tex);
                free(rgba);
            }
        }
    }
}

CodeField *CodeField_0(void) {
    CodeField *cf = (CodeField*) Memory_alloc(TYPE_CODEFIELD_SINGLETON, sizeof(CodeField));
    if (!cf)
        return nullptr;
    Panel *bp = Panel_0();
    if (!bp) {
        Memory_free(cf);
        return nullptr;
    }
    (*cf).base = (*bp);
    Memory_free(bp);

    (*cf).gutterWidth = CODEFIELD_DEFAULT_GUTTER_W;
    (*cf).gutterBackground = 0xFF121214u;
    (*cf).gutterTextColor = 0xFF71717Au;
    (*cf).activeLineColor = 0xFF60A5FAu;
    (*cf).onChange = nullptr;
    (*cf).ctx = nullptr;

    // Create inner Textarea editor
    (*cf).editor = Textarea_0();
    if ((*cf).editor) {
        Panel_setBackgroundColor(&(*cf).editor->base, 0xFF18181Bu);
        Textarea_setFontSize((*cf).editor, 13.0f);
        Textarea_setTextColor((*cf).editor, 0xFFF4F4F5u);
        Textarea_setOnChange((*cf).editor, onEditorChange);
        Textarea_setCtx((*cf).editor, cf);
        Panel_addContainer(&(*cf).base, &(*cf).editor->base);
    }

    Panel_setRenderHandler(&(*cf).base, CodeField_renderFn);
    return cf;
}

CodeField *CodeField_1_parent(Panel *parent) {
    CodeField *cf = CodeField_0();
    if (cf && parent) {
        Panel_addContainer(parent, &(*cf).base);
    }
    return cf;
}

CodeField *CodeField_2(Panel *parent, const char *initialCode) {
    CodeField *cf = CodeField_0();
    if (cf) {
        if (initialCode)
            CodeField_setText(cf, initialCode);
        if (parent)
            Panel_addContainer(parent, &(*cf).base);
    }
    return cf;
}

void CodeField_free(CodeField *self) {
    if (!self) return;
    if ((*self).editor) {
        Textarea_free((*self).editor);
        (*self).editor = nullptr;
    }
    (*self).onChange = nullptr;
    (*self).ctx = nullptr;
    Memory_free(self);
}

void CodeField_setText(CodeField *self, const char *code) {
    if (!self || !(*self).editor) return;
    Textarea_setText((*self).editor, code);
    markDirty(self);
}

void CodeField_setGutterWidth(CodeField *self, float width) {
    if (!self) return;
    (*self).gutterWidth = width;
    markDirty(self);
}

void CodeField_setGutterBackground(CodeField *self, uint32_t color) {
    if (!self) return;
    (*self).gutterBackground = color;
    markDirty(self);
}

void CodeField_setGutterTextColor(CodeField *self, uint32_t color) {
    if (!self) return;
    (*self).gutterTextColor = color;
    markDirty(self);
}

void CodeField_setActiveLineColor(CodeField *self, uint32_t color) {
    if (!self) return;
    (*self).activeLineColor = color;
    markDirty(self);
}

void CodeField_setOnChange(CodeField *self, CodeField_ChangeFn fn) {
    if (!self) return;
    (*self).onChange = fn;
}

void CodeField_setCtx(CodeField *self, void *ctx) {
    if (!self) return;
    (*self).ctx = ctx;
}

const char *CodeField_getText(const CodeField *self) {
    return (self && (*self).editor) ? Textarea_getText((*self).editor) : nullptr;
}

float CodeField_getGutterWidth(const CodeField *self) {
    return self ? (*self).gutterWidth : CODEFIELD_DEFAULT_GUTTER_W;
}

uint32_t CodeField_getGutterBackground(const CodeField *self) {
    return self ? (*self).gutterBackground : 0xFF121214u;
}

uint32_t CodeField_getGutterTextColor(const CodeField *self) {
    return self ? (*self).gutterTextColor : 0xFF71717Au;
}

uint32_t CodeField_getActiveLineColor(const CodeField *self) {
    return self ? (*self).activeLineColor : 0xFF60A5FAu;
}

CodeField_ChangeFn CodeField_getOnChange(const CodeField *self) {
    return self ? (*self).onChange : nullptr;
}

void *CodeField_getCtx(const CodeField *self) {
    return self ? (*self).ctx : nullptr;
}

Textarea *CodeField_getEditor(const CodeField *self) {
    return self ? (*self).editor : nullptr;
}
