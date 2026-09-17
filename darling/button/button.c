#include "darling/button/button.h"

#include "darling/panel/panel.h"
#include "annotation/overview.h"
#include "event/pointer.h"
#include "nio/mem.h"
#include "oop/type.h"
#include "text/text_core.h"
#include "vulkan/texture/texture.h"
#include "vulkan/vk.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Button (embeds Panel)
 * LEVEL: L2 — Behavior (pressable button shell)
 * ============================================================================
 * Panel shell for a pressable button with an owned label, font styling,
 * per-state colors, a press callback, and pointer event handling.
 *
 * STRUCT FIELDS (Mirroring darling/button/button.h):
 * ----------------------------------------------------------------------------
 *   Panel base;                        // Inherited layout/tree/background state
 *   char *label;                       // Owned label copy; nullptr = empty
 *   Font *font;                        // Borrowed font handle; nullptr = default
 *   float fontSize;                    // Label height in points
 *   uint32_t textColor;                // Packed 0xAARRGGBB label color
 *   uint32_t bg;                       // Packed 0xAARRGGBB idle fill
 *   uint32_t bgHover;                  // Packed 0xAARRGGBB hover fill
 *   uint32_t bgPressed;                // Packed 0xAARRGGBB pressed fill
 *   uint32_t borderColor;              // Packed 0xAARRGGBB border color
 *   float radius;                      // Corner radius in points
 *   float borderWidth;                 // Border width in points
 *   bool disabled;                     // True = non-interactive
 *   bool hovered;                      // True = pointer currently over
 *   bool pressed;                      // True = currently held down
 *   void (*onPress)(void *ctx);        // Press callback; nullptr = none
 *   void *ctx;                         // Callback context
 *
 *   // --- Label raster cache (label part) ---
 *   int32_t rasterTex;                 // cached native text quad texture id; -1 = none
 *   int rasterW;                       // raster pixel width
 *   int rasterH;                       // raster pixel height
 *   float rasterBacking;               // backing scale at raster time
 *   bool rasterDirty;                  // label/font/size changed -> re-raster on paint
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Button_0(void)
 *   - Button_1(label)
 *   - Button_2(parent, label)
 *
 * Core Functions:
 *   - Button_press(b)
 *   - Button_handlePointer(b, kind, localX, localY)
 *   - Button_renderFn(panel, rend, cmd, surfaceW, surfaceH, x, y, w, h) : Draw
 *     handler (registered via Panel_setRenderHandler in Button_0) — paints
 *     the state fill, border, and the cached native label quad
 *   - Button_free(b)
 *
 * Setters:
 *   - Button_setLabel(b, label)
 *   - Button_setFont(b, font)
 *   - Button_setFontSize(b, size)
 *   - Button_setTextColor(b, color)
 *   - Button_setBackground(b, color)
 *   - Button_setBackgroundHover(b, color)
 *   - Button_setBackgroundPressed(b, color)
 *   - Button_setBorderColor(b, color)
 *   - Button_setRadius(b, radius)
 *   - Button_setBorderWidth(b, width)
 *   - Button_setDisabled(b, disabled)
 *   - Button_setHovered(b, hovered)
 *   - Button_setPressed(b, pressed)
 *   - Button_setOnPress(b, fn, ctx)
 *
 * Getters:
 *   - Button_getLabel(b)
 *   - Button_getFont(b)
 *   - Button_getFontSize(b)
 *   - Button_getTextColor(b)
 *   - Button_getBackground(b)
 *   - Button_getBackgroundHover(b)
 *   - Button_getBackgroundPressed(b)
 *   - Button_getBorderColor(b)
 *   - Button_getRadius(b)
 *   - Button_getBorderWidth(b)
 *   - Button_isDisabled(b)
 *   - Button_isHovered(b)
 *   - Button_isPressed(b)
 *   - Button_getOnPress(b)
 *   - Button_getPressContext(b)
 *   - Button_getRasterTexture(b)
 *   - Button_getRasterSize(b, outW, outH)
 * ============================================================================
 */

// Draw handler body (defined below in CORE FUNCTIONS; forward-declared so
// Button_0 can install it on the Panel base at construction time).
static void Button_renderFn(Panel *panel, void *renderer, void *cmdBuffer, float surfaceW, float surfaceH,
                            float x, float y, float w, float h);

// CONSTRUCTORS
// ============================================================================

Button *Button_0(void) {
    Button *b = (Button*) Memory_alloc(TYPE_BUTTON_SINGLETON, sizeof(Button));
    if (!b)
        return nullptr;
    Panel *p = Panel_0();
    if (!p) {
        Memory_free(b);
        return nullptr;
    }
    (*b).base = (*p);
    Memory_free(p);
    (*b).label = nullptr;
    (*b).font = nullptr;
    (*b).fontSize = 12.0f;
    (*b).textColor = 0xFFFFFFFFu;
    (*b).bg = 0xFF3A3A3Au;
    (*b).bgHover = 0xFF4A4A4Au;
    (*b).bgPressed = 0xFF2A2A2Au;
    (*b).borderColor = 0xFF888888u;
    (*b).radius = 4.0f;
    (*b).borderWidth = 1.0f;
    (*b).disabled = false;
    (*b).hovered = false;
    (*b).pressed = false;
    (*b).onPress = nullptr;
    (*b).ctx = nullptr;
    (*b).rasterTex = -1;
    (*b).rasterW = 0;
    (*b).rasterH = 0;
    (*b).rasterBacking = 1.0f;
    (*b).rasterDirty = true;
    // Paint handler: installed so the board pass (paintChildIntoPass) can
    // draw the button — state fill, border, and the native label quad.
    Panel_setRenderHandler(&(*b).base, Button_renderFn);
    return b;
}

Button *Button_1(const char *label) {
    Button *b = Button_0();
    if (b)
        Button_setLabel(b, label);
    return b;
}

Button *Button_2(Panel *parent, const char *label) {
    Button *b = Button_1(label);
    if (b && parent) {
        Panel *p = &(*b).base;
        Panel_addContainer(parent, p);
    }
    return b;
}

// CORE FUNCTIONS
// ============================================================================

static void markDirty(Button *b) {
    if (!b)
        return;
    Panel *p = &(*b).base;
    Container *c = &(*p).base;
    Container_markDirty(c);
}

// Re-raster demand: text-shape inputs (label/font/size/color) stale the
// cached native quad AND dirty the tree — the color is baked into the
// raster, so a color change is a re-raster, not a tint.
static void markRasterDirty(Button *b) {
    if (!b)
        return;
    (*b).rasterDirty = true;
    markDirty(b);
}

// Rebuild the native label raster quad (mirror of Input's sharp path with
// the same fixed Helvetica family — TextCore_rasterStyled -> Texture upload,
// centered). Runs only when rasterDirty; a failed rebuild keeps the old
// quad (drop-degrade per the Cold-Strict, Hot-Minimal Validation Law).
static bool buttonEnsureRaster(Button *b, float boundsW) {
    if (!b)
        return false;
    if (!(*b).rasterDirty)
        return (*b).rasterTex >= 0;
    (*b).rasterDirty = false;
    if (!(*b).label || (*b).label[0] == '\0' || (*b).fontSize <= 0.0f) {
        (*b).rasterTex = -1;
        return false;
    }
    extern float TextCore_backingScale(void);
    float backing = TextCore_backingScale();
    if (backing <= 0.0f)
        backing = 1.0f;
    float pxH = (*b).fontSize * backing;
    if (pxH <= 0.0f)
        pxH = 13.0f * backing;

    TextStyleDescriptor style = {
        .ligatures = true,
        .spacingWidth = 0.0f,
        .spacingHeight = 0.0f,
        .underline = UNDERLINE_NONE,
        .underlineColor = 0,
        .mnemonicIndex = -1,
        .selectionStart = -1,
        .selectionEnd = -1,
        .highlightRadius = 2.0f,
        .highlightColor = 0u,
        .align = TEXT_ALIGN_CENTER,
        .boundsWidth = boundsW,
    };

    uint8_t *rgba = nullptr;
    int w = 0, h = 0;
    bool ok = TextCore_rasterStyled((*b).label, "Helvetica", pxH, (*b).textColor, &style, &rgba, &w, &h);
    if (!ok || !rgba || w <= 0 || h <= 0)
        return false;

    if ((*b).rasterTex >= 0) {
        (*b).rasterTex = Texture_replaceRaw((*b).rasterTex, rgba, (uint32_t) w, (uint32_t) h);
    } else {
        (*b).rasterTex = Texture_loadRaw(rgba, (uint32_t) w, (uint32_t) h);
    }
    free(rgba);
    if ((*b).rasterTex < 0)
        return false;
    (*b).rasterW = w;
    (*b).rasterH = h;
    (*b).rasterBacking = backing;
    return true;
}

// Draw handler (the Panel_RenderFn registered in Button_0; board and pane
// passes invoke it through Panel_getRenderHandler). Paints:
//   1. state fill — pressed > hovered > idle; disabled dims via alpha;
//   2. border stroke — borderWidth-thick Input-style 4-edge rects;
//   3. the centered native label quad.
// (radius is a reserved layout hint for the future SDF rounded-corner path;
// the current painter draws square corners, like Input's field.)
static void Button_renderFn(Panel *panel, void *renderer, void *cmdBuffer, float surfaceW, float surfaceH,
                            float x, float y, float w, float h) {
    Button *b = (Button*) panel;
    (void) renderer;
    if (!b || w <= 0.0f || h <= 0.0f)
        return;
    float op = Container_getOpacity(&(*panel).base);
    if (op <= 0.0f)
        return;

    uint32_t fill = (*b).bg;
    if ((*b).pressed)
        fill = (*b).bgPressed;
    else if ((*b).hovered)
        fill = (*b).bgHover;
    float alphaScale = (*b).disabled ? 0.6f : 1.0f;
    float fr = ((fill >> 16) & 0xFF) / 255.0f;
    float fg = ((fill >> 8) & 0xFF) / 255.0f;
    float fb = (fill & 0xFF) / 255.0f;
    float fa = ((fill >> 24) & 0xFF) / 255.0f * op * alphaScale;
    if (fa > 0.0f)
        Vk_fillRect(cmdBuffer, surfaceW, surfaceH, x, y, w, h, fr, fg, fb, fa);

    float btw = (*b).borderWidth > 0.0f ? (*b).borderWidth : 1.0f;
    uint32_t bc = (*b).borderColor;
    float br = ((bc >> 16) & 0xFF) / 255.0f;
    float bg2 = ((bc >> 8) & 0xFF) / 255.0f;
    float bb = (bc & 0xFF) / 255.0f;
    float ba = ((bc >> 24) & 0xFF) / 255.0f * op;
    if (ba > 0.0f) {
        Vk_fillRect(cmdBuffer, surfaceW, surfaceH, x, y, w, btw, br, bg2, bb, ba);
        Vk_fillRect(cmdBuffer, surfaceW, surfaceH, x, y + h - btw, w, btw, br, bg2, bb, ba);
        Vk_fillRect(cmdBuffer, surfaceW, surfaceH, x, y, btw, h, br, bg2, bb, ba);
        Vk_fillRect(cmdBuffer, surfaceW, surfaceH, x + w - btw, y, btw, h, br, bg2, bb, ba);
    }

    if (!(*b).label || (*b).label[0] == '\0')
        return;
    float innerW = w - 8.0f;
    if (innerW < 10.0f)
        innerW = 10.0f;
    if (buttonEnsureRaster(b, innerW) && (*b).rasterTex >= 0 && (*b).rasterW > 0 && (*b).rasterH > 0) {
        float backing = (*b).rasterBacking > 0.0f ? (*b).rasterBacking : 1.0f;
        float qw = (float) (*b).rasterW / backing;
        float qh = (float) (*b).rasterH / backing;
        float qx = x + (w - qw) * 0.5f;
        float qy = y + (h - qh) * 0.5f;
        Vk_drawTexture(cmdBuffer, surfaceW, surfaceH, qx, qy, qw, qh, 1.0f, 1.0f, 1.0f, op,
                       (*b).rasterTex, PICTURE_MODE_FIT, (float) (*b).rasterW, (float) (*b).rasterH);
    }
}

void Button_press(Button *b) {
    if (!b || (*b).disabled)
        return;
    void (*fn)(void *ctx) = (*b).onPress;
    void *ctx = (*b).ctx;
    if (fn)
        fn(ctx);
}

void Button_handlePointer(Button *b, int kind, float localX, float localY) {
    if (!b || (*b).disabled)
        return;
    Panel *p = &(*b).base;
    Container *c = &(*p).base;
    float w = (*c).w > 0.0f ? (*c).w : 80.0f;
    float h = (*c).h > 0.0f ? (*c).h : 30.0f;
    bool inside = (localX >= 0.0f && localX <= w && localY >= 0.0f && localY <= h);
    if (kind == PTR_ENTER || kind == PTR_HOVER) {
        if (inside) {
            (*b).hovered = true;
            markDirty(b);
        }
        return;
    }
    if (kind == PTR_LEAVE) {
        (*b).hovered = false;
        (*b).pressed = false;
        markDirty(b);
        return;
    }
    if (kind == PTR_DOWN) {
        if (inside) {
            (*b).pressed = true;
            markDirty(b);
        }
        return;
    }
    if (kind == PTR_UP) {
        if ((*b).pressed) {
            (*b).pressed = false;
            markDirty(b);
            if (inside)
                Button_press(b);
        }
        return;
    }
}

void Button_free(Button *b) {
    if (!b)
        return;
    if ((*b).label)
        Memory_free((*b).label);
    (*b).label = nullptr;
    if ((*b).rasterTex >= 0)
        Texture_free((*b).rasterTex);
    (*b).rasterTex = -1;
    Memory_free(b);
}

// SETTERS
// ============================================================================

void Button_setLabel(Button *b, const char *label) {
    if (!b)
        return;
    if ((*b).label)
        Memory_free((*b).label);
    (*b).label = nullptr;
    if (label) {
        size_t len = strlen(label) + 1;
        (*b).label = (char*) Memory_alloc(TYPE_ARRAY, len);
        if ((*b).label)
            strcpy((*b).label, label);
    }
    markRasterDirty(b);
}

void Button_setFont(Button *b, Font *font) {
    if (!b)
        return;
    (*b).font = font;
    markRasterDirty(b);
}

void Button_setFontSize(Button *b, float size) {
    if (!b)
        return;
    (*b).fontSize = size;
    markRasterDirty(b);
}

void Button_setTextColor(Button *b, uint32_t color) {
    if (!b)
        return;
    (*b).textColor = color;
    markRasterDirty(b);
}

void Button_setBackground(Button *b, uint32_t color) {
    if (!b)
        return;
    (*b).bg = color;
    markDirty(b);
}

void Button_setBackgroundHover(Button *b, uint32_t color) {
    if (!b)
        return;
    (*b).bgHover = color;
    markDirty(b);
}

void Button_setBackgroundPressed(Button *b, uint32_t color) {
    if (!b)
        return;
    (*b).bgPressed = color;
    markDirty(b);
}

void Button_setBorderColor(Button *b, uint32_t color) {
    if (!b)
        return;
    (*b).borderColor = color;
    markDirty(b);
}

void Button_setRadius(Button *b, float radius) {
    if (!b)
        return;
    (*b).radius = radius;
    markDirty(b);
}

void Button_setBorderWidth(Button *b, float width) {
    if (!b)
        return;
    (*b).borderWidth = width;
    markDirty(b);
}

void Button_setDisabled(Button *b, bool disabled) {
    if (!b)
        return;
    (*b).disabled = disabled;
    markDirty(b);
}

void Button_setHovered(Button *b, bool hovered) {
    if (!b)
        return;
    (*b).hovered = hovered;
    markDirty(b);
}

void Button_setPressed(Button *b, bool pressed) {
    if (!b)
        return;
    (*b).pressed = pressed;
    markDirty(b);
}

void Button_setOnPress(Button *b, void (*fn)(void *ctx), void *ctx) {
    if (!b)
        return;
    (*b).onPress = fn;
    (*b).ctx = ctx;
}

// GETTERS
// ============================================================================

const char *Button_getLabel(const Button *b) {
    return b ? (*b).label : nullptr;
}

Font *Button_getFont(const Button *b) {
    return b ? (*b).font : nullptr;
}

float Button_getFontSize(const Button *b) {
    return b ? (*b).fontSize : 0.0f;
}

uint32_t Button_getTextColor(const Button *b) {
    return b ? (*b).textColor : 0u;
}

uint32_t Button_getBackground(const Button *b) {
    return b ? (*b).bg : 0u;
}

uint32_t Button_getBackgroundHover(const Button *b) {
    return b ? (*b).bgHover : 0u;
}

uint32_t Button_getBackgroundPressed(const Button *b) {
    return b ? (*b).bgPressed : 0u;
}

uint32_t Button_getBorderColor(const Button *b) {
    return b ? (*b).borderColor : 0u;
}

float Button_getRadius(const Button *b) {
    return b ? (*b).radius : 0.0f;
}

float Button_getBorderWidth(const Button *b) {
    return b ? (*b).borderWidth : 0.0f;
}

bool Button_isDisabled(const Button *b) {
    return b ? (*b).disabled : false;
}

bool Button_isHovered(const Button *b) {
    return b ? (*b).hovered : false;
}

bool Button_isPressed(const Button *b) {
    return b ? (*b).pressed : false;
}

void (*Button_getOnPress(const Button *b))(void *ctx) {
    return b ? (*b).onPress : nullptr;
}

void *Button_getPressContext(const Button *b) {
    return b ? (*b).ctx : nullptr;
}

// Label raster cache getters: GPU texture id (-1 = none) and the raster's
// pixel size, safe on null (Symmetric Getter/Setter Completeness Law).
int32_t Button_getRasterTexture(const Button *b) {
    return b ? (*b).rasterTex : -1;
}

void Button_getRasterSize(const Button *b, int *outW, int *outH) {
    if (outW)
        (*outW) = b ? (*b).rasterW : 0;
    if (outH)
        (*outH) = b ? (*b).rasterH : 0;
}
