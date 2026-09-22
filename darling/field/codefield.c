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
#include "annotation/definition.h"
#include "annotation/overview.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: CodeField
 * ============================================================================
 * Code editor composite that inherits Panel and owns a child Textarea editor
 * configured for monospace source code, plus a line-number gutter and an
 * active-line indicator. The composite paints only its own ordered part
 * pipeline — background (gutter fill + divider + inner layout), text (line
 * numbers + active marker), border, foreground — while code text, selection
 * highlight, and caret paint through the child Textarea's own pipeline
 * (composite != render). The editor pointer is owned and freed in
 * CodeField_free; gutter/color fields are plain scalars with symmetric
 * getters/setters. onChange(ctx) fires through the child editor's change hook,
 * marking the composite dirty.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: CodeField (inherits Panel)
 * LEVEL: L2 — Behavior (code editor composite)
 * ============================================================================
 * Code editor widget with line number gutter and active line indicator.
 * Wraps a Textarea editor configured for monospace source code.
 *
 * Ordered part pipeline: background (gutter fill + divider + inner layout)
 * -> text (line numbers + active-line marker) -> border (reserved) ->
 * foreground (reserved: loc text / buttons / highlight). Code text, selection
 * highlight, and caret paint via the child Textarea's own pipeline — the seam
 * never re-invokes them here (composite != render).
 *
 * STRUCT FIELDS (Mirroring darling/field/codefield.h):
 * ----------------------------------------------------------------------------
 *   Panel base;                  // Inherited layout, bounds, hierarchy state
 *   Textarea *editor;            // Owned child editor (monospace text)
 *   float gutterWidth;           // Line-number gutter width in px
 *   uint32_t gutterBackground;   // Gutter fill color, packed 0xAARRGGBB
 *   uint32_t gutterTextColor;    // Line-number text color, packed 0xAARRGGBB
 *   uint32_t activeLineColor;    // Active-line marker color, packed 0xAARRGGBB
 *   CodeField_ChangeFn onChange; // Edit callback; nullptr means none
 *   void *ctx;                   // Callback context pointer
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - CodeField_0(void)
 *   - CodeField_1_parent(parent)
 *   - CodeField_2(parent, initialCode)
 *
 * Core Functions:
 *   - CodeField_free(self)
 *
 * Setters:
 *   - CodeField_setText(self, code)
 *   - CodeField_setGutterWidth(self, width)
 *   - CodeField_setGutterBackground(self, color)
 *   - CodeField_setGutterTextColor(self, color)
 *   - CodeField_setActiveLineColor(self, color)
 *   - CodeField_setOnChange(self, fn)
 *   - CodeField_setCtx(self, ctx)
 *
 * Getters:
 *   - CodeField_getText(self)
 *   - CodeField_getGutterWidth(self)
 *   - CodeField_getGutterBackground(self)
 *   - CodeField_getGutterTextColor(self)
 *   - CodeField_getActiveLineColor(self)
 *   - CodeField_getOnChange(self)
 *   - CodeField_getCtx(self)
 *   - CodeField_getEditor(self)
 * ============================================================================
 */

#define CODEFIELD_DEFAULT_GUTTER_W 40.0f

static void onEditorChange(void *ctx) {
    CodeField *cf = (CodeField*) ctx;
    if (!cf) return;
    Panel *bp = &(*cf).base;
    (void) bp;
    if (cf->onChange) {
        cf->onChange(cf->ctx);
    }
}

static void markDirty(CodeField *self) {
    if (!self) return;
    Panel *bp = &(*self).base;
    (void) bp;
}

// Ordered part pipeline: background (gutter + divider + layout) -> text
// (line numbers + active marker). Code text / highlight / caret belong to the
// child Textarea and are never re-invoked here.

// Stage 0: gutter fill + divider + inner editor layout (layout-in-paint
// preserved; future work moves this next to Container_resolve).
static bool codePaintBackground(Panel *panel, void *renderer, void *cmdBuffer,
                                float surfaceW, float surfaceH,
                                float x, float y, float w, float h) {
    CodeField *cf = (CodeField*) panel;
    (void) renderer;
    if (!cf || !cmdBuffer)
        return false;
    if (w <= 0.0f || h <= 0.0f)
        return false;
    Component *c = &(*panel).component;
    float op = GraphicsComponent_getOpacity(c);
    if (op <= 0.0f)
        return false;
    float gw = (*cf).gutterWidth > 0.0f ? (*cf).gutterWidth : CODEFIELD_DEFAULT_GUTTER_W;
    Textarea *ed = (*cf).editor;
    if (ed) {
        float edW = w - gw;
        if (edW < 10.0f)
            edW = 10.0f;
        Panel *ebp = &(*ed).base;
        Component *ec = &(*ebp).component;
        GraphicsComponent_setLocation(ec, gw, 0.0f);
        GraphicsComponent_setSize(ec, edW, h);
    }
    uint32_t gbg = (*cf).gutterBackground;
    float gr = ((gbg >> 16) & 0xFF) / 255.0f;
    float gg = ((gbg >> 8) & 0xFF) / 255.0f;
    float gb = (gbg & 0xFF) / 255.0f;
    float ga = ((gbg >> 24) & 0xFF) / 255.0f * op;
    if (ga > 0.0f)
        Vk_fillRect(cmdBuffer, surfaceW, surfaceH, x, y, gw, h, gr, gg, gb, ga);
    Vk_fillRect(cmdBuffer, surfaceW, surfaceH, x + gw - 1.0f, y, 1.0f, h, 0.25f, 0.25f, 0.28f, op);
    return true;
}

// Stage 2: gutter line numbers + active-line marker.
static bool codePaintText(Panel *panel, void *renderer, void *cmdBuffer,
                          float surfaceW, float surfaceH,
                          float x, float y, float w, float h) {
    CodeField *cf = (CodeField*) panel;
    (void) renderer;
    (void) w;
    if (!cf || !cmdBuffer)
        return false;
    if (w <= 0.0f || h <= 0.0f)
        return false;
    Component *c = &(*panel).component;
    float op = GraphicsComponent_getOpacity(c);
    if (op <= 0.0f)
        return false;
    Textarea *ed = (*cf).editor;
    if (!ed)
        return false;
    float gw = (*cf).gutterWidth > 0.0f ? (*cf).gutterWidth : CODEFIELD_DEFAULT_GUTTER_W;
    const char *txt = Textarea_getText(ed);
    int32_t lines = 1;
    if (txt) {
        for (size_t i = 0; txt[i] != '\0'; i++) {
            if (txt[i] == '\n')
                lines++;
        }
    }
    int32_t cur = Textarea_getCursor(ed);
    int32_t activeLine = 0;
    if (txt) {
        for (int32_t i = 0; i < cur && txt[i] != '\0'; i++) {
            if (txt[i] == '\n')
                activeLine++;
        }
    }
    float padY = 8.0f;
    float lineH = Textarea_getFontSize(ed) * 1.35f + Textarea_getSpacingHeight(ed);
    float scrollY = Textarea_getScrollY(ed);
    float backing = TextCore_backingScale();
    if (backing <= 0.0f)
        backing = 1.0f;
    float pxH = 11.0f * backing;
    bool drew = false;
    bool edFocused = Textarea_isFocused(ed);
    uint32_t activeCol = (*cf).activeLineColor;
    uint32_t idleCol = (*cf).gutterTextColor;
    for (int32_t l = 0; l < lines; l++) {
        float lineY = y + h - padY - (float)(l + 1) * lineH + scrollY;
        if (lineY + lineH < y || lineY > y + h)
            continue;
        bool isActive = (l == activeLine) && edFocused;
        uint32_t numCol = isActive ? activeCol : idleCol;
        if (isActive) {
            Vk_fillRect(cmdBuffer, surfaceW, surfaceH, x + gw - 2.5f, lineY, 2.5f, lineH, 0.23f, 0.51f, 0.96f, op);
            drew = true;
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
            drew = true;
        }
    }
    return drew;
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

    Panel *cp = &(*cf).base;
    Panel_setRenderHandler(cp, nullptr);
    Panel_setBackgroundFn(cp, codePaintBackground);
    Panel_setTextFn(cp, codePaintText);
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
