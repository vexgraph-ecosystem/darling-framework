#include "darling/field/inputotp.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "event/pointer.h"
#include "input/key.h"
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
 * DEFINITION: InputOTP
 * ============================================================================
 * One-time passcode segmented digit boxes: Panel layout plus an owned digit
 * buffer of fixed length. Draws N outlined boxes with an active-box focus
 * ring, centered digit or dot (password mode), auto-advance on keystroke,
 * paste auto-split, backspace retreat, and an onComplete callback. Per-box
 * raster textures are cached in fixed arrays (INPUTOTP_MAX_BOXES 16) and
 * re-baked only when the box character changes; the digit buffer is owned and
 * freed in InputOTP_free. Pointer and key events drive the state machine;
 * symmetric getters/setters cover every field.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: InputOTP (inherits Panel)
 * LEVEL: L2 — Behavior (one-time passcode input)
 * ============================================================================
 * One-time passcode segmented digit boxes.
 * Draws N outlined boxes with active box focus ring, centered digit or dot
 * (password mode), auto-advance on keystroke, paste auto-split, backspace
 * retreat, and onComplete callback.
 *
 * STRUCT FIELDS (Mirroring darling/field/inputotp.h):
 * ----------------------------------------------------------------------------
 *   Panel base;                  // Inherited layout, bounds, hierarchy state
 *   char *digits;                // Owned digit buffer (fixed length)
 *   int32_t length;              // Number of boxes (1..INPUTOTP_MAX_BOXES)
 *   float boxSize;               // Box edge length in px
 *   float gap;                   // Gap between boxes in px
 *   int32_t cursor;              // Active box index
 *   bool focused;                // Focus-request flag
 *   bool password;               // Mask digits as dots
 *   float fontSize;              // Digit font size in points
 *   uint32_t textColor;          // Digit color, packed 0xAARRGGBB
 *   uint32_t boxBackground;      // Box fill color, packed 0xAARRGGBB
 *   uint32_t boxBorderColor;     // Idle box border color, packed 0xAARRGGBB
 *   uint32_t boxActiveBorder;    // Active box border color, packed 0xAARRGGBB
 *   InputOTP_CompleteFn onComplete; // Completion callback; nullptr means none
 *   void *ctx;                   // Callback context pointer
 *   --- Raster cache per box ---
 *   int32_t boxTex[INPUTOTP_MAX_BOXES]; // Per-box texture handle (-1 = none)
 *   int32_t boxW[INPUTOTP_MAX_BOXES];   // Per-box raster width in px
 *   int32_t boxH[INPUTOTP_MAX_BOXES];   // Per-box raster height in px
 *   char boxChar[INPUTOTP_MAX_BOXES];   // Per-box cached character
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - InputOTP_1(length)
 *   - InputOTP_2(parent, length)
 *   - InputOTP_1_parent(parent)
 *
 * Core Functions:
 *   - InputOTP_pushDigit(otp, digit)
 *   - InputOTP_handlePointer(self, kind, localX, localY)
 *   - InputOTP_handleKey(self, ev)
 *   - InputOTP_free(otp)
 *
 * Setters:
 *   - InputOTP_setDigits(otp, digits)
 *   - InputOTP_setBoxSize(otp, size)
 *   - InputOTP_setGap(otp, gap)
 *   - InputOTP_setCursor(otp, cursor)
 *   - InputOTP_setPassword(otp, password)
 *   - InputOTP_setFontSize(otp, size)
 *   - InputOTP_setTextColor(otp, color)
 *   - InputOTP_setBoxBackground(otp, color)
 *   - InputOTP_setBoxBorderColor(otp, color)
 *   - InputOTP_setBoxActiveBorder(otp, color)
 *   - InputOTP_setOnComplete(otp, fn)
 *   - InputOTP_setCtx(otp, ctx)
 *
 * Getters:
 *   - InputOTP_getDigits(otp)
 *   - InputOTP_getLength(otp)
 *   - InputOTP_getBoxSize(otp)
 *   - InputOTP_getGap(otp)
 *   - InputOTP_getCursor(otp)
 *   - InputOTP_isFocused(otp)
 *   - InputOTP_isPassword(otp)
 *   - InputOTP_getFontSize(otp)
 *   - InputOTP_getTextColor(otp)
 *   - InputOTP_getBoxBackground(otp)
 *   - InputOTP_getBoxBorderColor(otp)
 *   - InputOTP_getBoxActiveBorder(otp)
 *   - InputOTP_getOnComplete(otp)
 *   - InputOTP_getCtx(otp)
 * ============================================================================
 */

#define INPUTOTP_DEFAULT_LENGTH 6
#define INPUTOTP_DEFAULT_BOX 40.0f
#define INPUTOTP_DEFAULT_GAP 8.0f

static void markDirty(InputOTP *otp) {
    if (!otp)
        return;
    Panel *bp = &(*otp).base;
    (void) bp;
}

static void ensureBoxRaster(InputOTP *otp, int32_t idx, char ch) {
    if (!otp || idx < 0 || idx >= INPUTOTP_MAX_BOXES)
        return;
    if ((*otp).boxChar[idx] == ch && (*otp).boxTex[idx] >= 0)
        return;

    if ((*otp).boxTex[idx] >= 0) {
        Texture_free((*otp).boxTex[idx]);
        (*otp).boxTex[idx] = -1;
    }
    (*otp).boxChar[idx] = ch;
    (*otp).boxW[idx] = 0;
    (*otp).boxH[idx] = 0;

    if (ch == '\0' || ch == ' ')
        return;

    char str[8];
    if ((*otp).password) {
        snprintf(str, sizeof(str), "•");
    } else {
        str[0] = ch;
        str[1] = '\0';
    }

    float backing = TextCore_backingScale();
    if (backing <= 0.0f) backing = 1.0f;
    float pxH = (*otp).fontSize * backing;
    if (pxH <= 0.0f) pxH = 18.0f * backing;

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
        .align = TEXT_ALIGN_CENTER,
        .boundsWidth = 0.0f,
    };

    uint8_t *rgba = nullptr;
    int w = 0, h = 0;
    bool ok = TextCore_rasterStyled(str, "Helvetica", pxH, (*otp).textColor, &style, &rgba, &w, &h);
    if (ok && rgba && w > 0 && h > 0) {
        (*otp).boxTex[idx] = Texture_loadRaw(rgba, (uint32_t) w, (uint32_t) h);
        (*otp).boxW[idx] = w;
        (*otp).boxH[idx] = h;
        free(rgba);
    }
}

// Ordered part pipeline, one loop per stage (boxes never overlap, so the
// grouped order is pixel-identical to the legacy per-box interleave):
// background (all boxes) -> border (all boxes) -> text (all digits) ->
// foreground (active-box caret). Layout math lives once in boxLayout.

// Shared box geometry: centers len boxes in the node rect. Dest-last outs.
static void boxLayout(InputOTP *otp, float x, float y, float w, float h,
                      int32_t *outLen, float *outBs, float *outStartX, float *outStartY) {
    int32_t len = (*otp).length;
    if (len <= 0)
        len = 1;
    if (len > INPUTOTP_MAX_BOXES)
        len = INPUTOTP_MAX_BOXES;
    float bs = (*otp).boxSize > 0.0f ? (*otp).boxSize : INPUTOTP_DEFAULT_BOX;
    float gap = (*otp).gap >= 0.0f ? (*otp).gap : INPUTOTP_DEFAULT_GAP;
    float totalW = (float)len * bs + (float)(len - 1) * gap;
    *outLen = len;
    *outBs = bs;
    *outStartX = x + (w > totalW ? (w - totalW) * 0.5f : 0.0f);
    *outStartY = y + (h > bs ? (h - bs) * 0.5f : 0.0f);
}

// Stage 0: every box fill.
static bool otpPaintBackground(Panel *panel, void *renderer, void *cmdBuffer,
                               float surfaceW, float surfaceH,
                               float x, float y, float w, float h) {
    InputOTP *otp = (InputOTP*) panel;
    (void) renderer;
    if (!otp || !cmdBuffer)
        return false;
    if (w <= 0.0f || h <= 0.0f)
        return false;
    Component *c = &(*panel).component;
    float op = Component_getOpacity(c);
    if (op <= 0.0f)
        return false;
    int32_t len = 0;
    float bs = 0.0f;
    float startX = 0.0f;
    float startY = 0.0f;
    boxLayout(otp, x, y, w, h, &len, &bs, &startX, &startY);
    float gap = (*otp).gap >= 0.0f ? (*otp).gap : INPUTOTP_DEFAULT_GAP;
    bool drew = false;
    for (int32_t i = 0; i < len; i++) {
        float bx = startX + (float)i * (bs + gap);
        float by = startY;
        uint32_t bg = (*otp).boxBackground;
        float br = ((bg >> 16) & 0xFF) / 255.0f;
        float bgc = ((bg >> 8) & 0xFF) / 255.0f;
        float bb = (bg & 0xFF) / 255.0f;
        float ba = ((bg >> 24) & 0xFF) / 255.0f * op;
        if (ba <= 0.0f)
            continue;
        Vk_fillRect(cmdBuffer, surfaceW, surfaceH, bx, by, bs, bs, br, bgc, bb, ba);
        drew = true;
    }
    return drew;
}

// Stage 3: every box stroke (active box gets the accent + wider stroke).
static bool otpPaintBorder(Panel *panel, void *renderer, void *cmdBuffer,
                           float surfaceW, float surfaceH,
                           float x, float y, float w, float h) {
    InputOTP *otp = (InputOTP*) panel;
    (void) renderer;
    if (!otp || !cmdBuffer)
        return false;
    if (w <= 0.0f || h <= 0.0f)
        return false;
    Component *c = &(*panel).component;
    float op = Component_getOpacity(c);
    if (op <= 0.0f)
        return false;
    int32_t len = 0;
    float bs = 0.0f;
    float startX = 0.0f;
    float startY = 0.0f;
    boxLayout(otp, x, y, w, h, &len, &bs, &startX, &startY);
    float gap = (*otp).gap >= 0.0f ? (*otp).gap : INPUTOTP_DEFAULT_GAP;
    bool drew = false;
    for (int32_t i = 0; i < len; i++) {
        float bx = startX + (float)i * (bs + gap);
        float by = startY;
        bool isActive = (*otp).focused && ((*otp).cursor == i);
        uint32_t borderColor = isActive ? (*otp).boxActiveBorder : (*otp).boxBorderColor;
        float b_r = ((borderColor >> 16) & 0xFF) / 255.0f;
        float b_g = ((borderColor >> 8) & 0xFF) / 255.0f;
        float b_b = (borderColor & 0xFF) / 255.0f;
        float b_a = ((borderColor >> 24) & 0xFF) / 255.0f * op;
        if (b_a <= 0.0f)
            continue;
        float stroke = isActive ? 1.5f : 1.0f;
        Vk_fillRect(cmdBuffer, surfaceW, surfaceH, bx, by, bs, stroke, b_r, b_g, b_b, b_a);
        Vk_fillRect(cmdBuffer, surfaceW, surfaceH, bx, by + bs - stroke, bs, stroke, b_r, b_g, b_b, b_a);
        Vk_fillRect(cmdBuffer, surfaceW, surfaceH, bx, by, stroke, bs, b_r, b_g, b_b, b_a);
        Vk_fillRect(cmdBuffer, surfaceW, surfaceH, bx + bs - stroke, by, stroke, bs, b_r, b_g, b_b, b_a);
        drew = true;
    }
    return drew;
}

// Stage 2: every digit / dot raster quad.
static bool otpPaintText(Panel *panel, void *renderer, void *cmdBuffer,
                         float surfaceW, float surfaceH,
                         float x, float y, float w, float h) {
    InputOTP *otp = (InputOTP*) panel;
    (void) renderer;
    if (!otp || !cmdBuffer)
        return false;
    if (w <= 0.0f || h <= 0.0f)
        return false;
    Component *c = &(*panel).component;
    float op = Component_getOpacity(c);
    if (op <= 0.0f)
        return false;
    int32_t len = 0;
    float bs = 0.0f;
    float startX = 0.0f;
    float startY = 0.0f;
    boxLayout(otp, x, y, w, h, &len, &bs, &startX, &startY);
    float gap = (*otp).gap >= 0.0f ? (*otp).gap : INPUTOTP_DEFAULT_GAP;
    size_t dlen = (*otp).digits ? strlen((*otp).digits) : 0;
    bool drew = false;
    for (int32_t i = 0; i < len; i++) {
        float bx = startX + (float)i * (bs + gap);
        float by = startY;
        char ch = (i < (int32_t)dlen) ? (*otp).digits[i] : '\0';
        ensureBoxRaster(otp, i, ch);
        if ((*otp).boxTex[i] < 0 || (*otp).boxW[i] <= 0 || (*otp).boxH[i] <= 0)
            continue;
        float backing = TextCore_backingScale();
        if (backing <= 0.0f)
            backing = 1.0f;
        float qw = (float)(*otp).boxW[i] / backing;
        float qh = (float)(*otp).boxH[i] / backing;
        float qx = bx + (bs - qw) * 0.5f;
        float qy = by + (bs - qh) * 0.5f;
        Vk_drawTexture(cmdBuffer, surfaceW, surfaceH, qx, qy, qw, qh, 1.0f, 1.0f, 1.0f, op,
                       (*otp).boxTex[i], PICTURE_MODE_FIT, (float)(*otp).boxW[i], (float)(*otp).boxH[i]);
        drew = true;
    }
    return drew;
}

// Stage 4: caret block in the active empty box.
static bool otpPaintCaret(Panel *panel, void *renderer, void *cmdBuffer,
                          float surfaceW, float surfaceH,
                          float x, float y, float w, float h) {
    InputOTP *otp = (InputOTP*) panel;
    (void) renderer;
    if (!otp || !cmdBuffer)
        return false;
    if (!(*otp).focused)
        return false;
    Component *c = &(*panel).component;
    float op = Component_getOpacity(c);
    if (op <= 0.0f)
        return false;
    int32_t len = 0;
    float bs = 0.0f;
    float startX = 0.0f;
    float startY = 0.0f;
    boxLayout(otp, x, y, w, h, &len, &bs, &startX, &startY);
    float gap = (*otp).gap >= 0.0f ? (*otp).gap : INPUTOTP_DEFAULT_GAP;
    size_t dlen = (*otp).digits ? strlen((*otp).digits) : 0;
    int32_t i = (*otp).cursor;
    if (i < 0 || i >= len)
        return false;
    char ch = (i < (int32_t)dlen) ? (*otp).digits[i] : '\0';
    if (ch != '\0' && ch != ' ')
        return false;
    ensureBoxRaster(otp, i, ch);
    if ((*otp).boxTex[i] >= 0 && (*otp).boxW[i] > 0 && (*otp).boxH[i] > 0)
        return false;
    float bx = startX + (float)i * (bs + gap);
    float by = startY;
    float caretW = 1.5f;
    float caretH = bs * 0.45f;
    float cx = bx + (bs - caretW) * 0.5f;
    float cy = by + (bs - caretH) * 0.5f;
    Vk_fillRect(cmdBuffer, surfaceW, surfaceH, cx, cy, caretW, caretH, 1.0f, 1.0f, 1.0f, op);
    return true;
}

static InputOTP *allocOtp(int32_t length) {
    if (length < 1)
        length = INPUTOTP_DEFAULT_LENGTH;
    if (length > INPUTOTP_MAX_BOXES)
        length = INPUTOTP_MAX_BOXES;
    InputOTP *otp = (InputOTP*) Memory_alloc(TYPE_INPUTOTP_SINGLETON, sizeof(InputOTP));
    if (!otp)
        return nullptr;
    Panel *bp = Panel_0();
    if (!bp) {
        Memory_free(otp);
        return nullptr;
    }
    (*otp).base = (*bp);
    Memory_free(bp);
    size_t cap = (size_t)length + 1;
    char *buf = (char*) Memory_alloc(TYPE_ARRAY, cap);
    if (!buf) {
        Memory_free(otp);
        return nullptr;
    }
    buf[0] = '\0';
    (*otp).digits = buf;
    (*otp).length = length;
    (*otp).boxSize = INPUTOTP_DEFAULT_BOX;
    (*otp).gap = INPUTOTP_DEFAULT_GAP;
    (*otp).cursor = 0;
    (*otp).focused = false;
    (*otp).password = false;
    (*otp).fontSize = 18.0f;
    (*otp).textColor = 0xFFFFFFFFu;
    (*otp).boxBackground = 0xFF18181Bu;
    (*otp).boxBorderColor = 0xFF3F3F46u;
    (*otp).boxActiveBorder = 0xFF3B82F6u;
    (*otp).onComplete = nullptr;
    (*otp).ctx = nullptr;

    for (int32_t i = 0; i < INPUTOTP_MAX_BOXES; i++) {
        (*otp).boxTex[i] = -1;
        (*otp).boxW[i] = 0;
        (*otp).boxH[i] = 0;
        (*otp).boxChar[i] = '\0';
    }

    Panel *op = &(*otp).base;
    Panel_setRenderHandler(op, nullptr);
    Panel_setBackgroundFn(op, otpPaintBackground);
    Panel_setTextFn(op, otpPaintText);
    Panel_setBorderFn(op, otpPaintBorder);
    Panel_setForegroundFn(op, otpPaintCaret);
    return otp;
}

InputOTP *InputOTP_1(int32_t length) {
    return allocOtp(length);
}

InputOTP *InputOTP_1_parent(Panel *parent) {
    InputOTP *otp = allocOtp(INPUTOTP_DEFAULT_LENGTH);
    if (otp && parent) {
        Panel *bp = &(*otp).base;
        Panel_addContainer(parent, bp);
    }
    return otp;
}

InputOTP *InputOTP_2(Panel *parent, int32_t length) {
    InputOTP *otp = allocOtp(length);
    if (otp && parent) {
        Panel *bp = &(*otp).base;
        Panel_addContainer(parent, bp);
    }
    return otp;
}

// ============================================================================
// CORE FUNCTIONS
// ============================================================================

void InputOTP_pushDigit(InputOTP *otp, char digit) {
    if (!otp || !(*otp).digits)
        return;
    int32_t len = (*otp).length;
    int32_t cur = (*otp).cursor;
    if (cur < 0) cur = 0;
    if (cur >= len) cur = len - 1;

    size_t dlen = strlen((*otp).digits);
    if ((size_t)cur >= dlen) {
        for (size_t i = dlen; i < (size_t)cur; i++)
            (*otp).digits[i] = ' ';
        (*otp).digits[cur] = digit;
        (*otp).digits[cur + 1] = '\0';
    } else {
        (*otp).digits[cur] = digit;
    }

    if (cur + 1 < len) {
        (*otp).cursor = cur + 1;
    } else {
        (*otp).cursor = len - 1;
    }

    markDirty(otp);

    // Check completion
    if ((int32_t)strlen((*otp).digits) >= len) {
        bool full = true;
        for (int32_t i = 0; i < len; i++) {
            if ((*otp).digits[i] == ' ' || (*otp).digits[i] == '\0') {
                full = false;
                break;
            }
        }
        if (full && (*otp).onComplete) {
            (*otp).onComplete((*otp).ctx);
        }
    }
}

void InputOTP_handlePointer(InputOTP *self, int kind, float localX, float localY) {
    if (!self)
        return;
    (void) localY;
    if (kind == PTR_DOWN) {
        (*self).focused = true;
        Panel *p = &(*self).base;
        float w = Component_getWidth(&(*p).component);
        float bs = (*self).boxSize > 0.0f ? (*self).boxSize : INPUTOTP_DEFAULT_BOX;
        float gap = (*self).gap >= 0.0f ? (*self).gap : INPUTOTP_DEFAULT_GAP;
        int32_t len = (*self).length;
        float totalW = (float)len * bs + (float)(len - 1) * gap;
        float startX = w > totalW ? (w - totalW) * 0.5f : 0.0f;

        int32_t chosen = -1;
        for (int32_t i = 0; i < len; i++) {
            float bx = startX + (float)i * (bs + gap);
            if (localX >= bx && localX <= bx + bs) {
                chosen = i;
                break;
            }
        }
        if (chosen >= 0) {
            (*self).cursor = chosen;
        } else {
            size_t dlen = strlen((*self).digits);
            int32_t firstEmpty = (int32_t)dlen;
            if (firstEmpty >= len) firstEmpty = len - 1;
            (*self).cursor = firstEmpty;
        }
        markDirty(self);
    }
}

void InputOTP_handleKey(InputOTP *self, const UIKeyEvent *ev) {
    if (!self || !ev)
        return;
    if (!UIKeyEvent_isPressed(ev))
        return;
    int32_t code = UIKeyEvent_getKeyCode(ev);
    int32_t ch = UIKeyEvent_getCh(ev);
    uint32_t mods = UIKeyEvent_getMods(ev);
    bool cmdOrCtrl = (mods & (8u | 2u)) != 0;

    if (cmdOrCtrl) {
        if (code == KEY_C) {
            if ((*self).digits && (*self).digits[0] != '\0') {
                TextCore_copyToClipboard((*self).digits);
            }
            return;
        }
        if (code == KEY_V) {
            char *clip = TextCore_pasteFromClipboard();
            if (clip) {
                int32_t len = (*self).length;
                for (int32_t i = 0; clip[i] != '\0' && (*self).cursor < len; i++) {
                    char c = clip[i];
                    if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
                        InputOTP_pushDigit(self, c);
                    }
                }
                free(clip);
                markDirty(self);
            }
            return;
        }
    }

    if (code == KEY_BACKSPACE || ch == 8) {
        int32_t cur = (*self).cursor;
        size_t dlen = strlen((*self).digits);
        if ((size_t)cur < dlen && (*self).digits[cur] != '\0' && (*self).digits[cur] != ' ') {
            (*self).digits[cur] = '\0';
        } else if (cur > 0) {
            cur--;
            (*self).cursor = cur;
            if ((size_t)cur < dlen)
                (*self).digits[cur] = '\0';
        }
        markDirty(self);
        return;
    }
    if (code == KEY_LEFT) {
        if ((*self).cursor > 0) {
            (*self).cursor--;
            markDirty(self);
        }
        return;
    }
    if (code == KEY_RIGHT) {
        if ((*self).cursor + 1 < (*self).length) {
            (*self).cursor++;
            markDirty(self);
        }
        return;
    }
    if ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')) {
        InputOTP_pushDigit(self, (char)ch);
        return;
    }
}

// ============================================================================
// SETTERS
// ============================================================================

void InputOTP_setDigits(InputOTP *otp, const char *digits) {
    if (!otp)
        return;
    char *buf = (*otp).digits;
    if (!buf)
        return;
    int32_t cap = (*otp).length;
    if (cap < 0)
        cap = 0;
    buf[0] = '\0';
    if (digits) {
        size_t len = strlen(digits);
        if (len > (size_t)cap)
            len = (size_t)cap;
        memcpy(buf, digits, len);
        buf[len] = '\0';
        (*otp).cursor = (int32_t)len >= cap ? cap - 1 : (int32_t)len;
    }
    for (int32_t i = 0; i < INPUTOTP_MAX_BOXES; i++)
        (*otp).boxChar[i] = '\0';
    markDirty(otp);
}

void InputOTP_setBoxSize(InputOTP *otp, float size) {
    if (!otp)
        return;
    (*otp).boxSize = size;
    markDirty(otp);
}

void InputOTP_setGap(InputOTP *otp, float gap) {
    if (!otp)
        return;
    (*otp).gap = gap;
    markDirty(otp);
}

void InputOTP_setCursor(InputOTP *otp, int32_t cursor) {
    if (!otp)
        return;
    if (cursor < 0) cursor = 0;
    if (cursor >= (*otp).length) cursor = (*otp).length - 1;
    (*otp).cursor = cursor;
    markDirty(otp);
}

void InputOTP_setPassword(InputOTP *otp, bool password) {
    if (!otp)
        return;
    (*otp).password = password;
    for (int32_t i = 0; i < INPUTOTP_MAX_BOXES; i++)
        (*otp).boxChar[i] = '\0';
    markDirty(otp);
}

void InputOTP_setFontSize(InputOTP *otp, float size) {
    if (!otp)
        return;
    (*otp).fontSize = size;
    for (int32_t i = 0; i < INPUTOTP_MAX_BOXES; i++)
        (*otp).boxChar[i] = '\0';
    markDirty(otp);
}

void InputOTP_setTextColor(InputOTP *otp, uint32_t color) {
    if (!otp)
        return;
    (*otp).textColor = color;
    for (int32_t i = 0; i < INPUTOTP_MAX_BOXES; i++)
        (*otp).boxChar[i] = '\0';
    markDirty(otp);
}

void InputOTP_setBoxBackground(InputOTP *otp, uint32_t color) {
    if (!otp)
        return;
    (*otp).boxBackground = color;
    markDirty(otp);
}

void InputOTP_setBoxBorderColor(InputOTP *otp, uint32_t color) {
    if (!otp)
        return;
    (*otp).boxBorderColor = color;
    markDirty(otp);
}

void InputOTP_setBoxActiveBorder(InputOTP *otp, uint32_t color) {
    if (!otp)
        return;
    (*otp).boxActiveBorder = color;
    markDirty(otp);
}

void InputOTP_setOnComplete(InputOTP *otp, InputOTP_CompleteFn fn) {
    if (!otp)
        return;
    (*otp).onComplete = fn;
}

void InputOTP_setCtx(InputOTP *otp, void *ctx) {
    if (!otp)
        return;
    (*otp).ctx = ctx;
}

void InputOTP_free(InputOTP *otp) {
    if (!otp)
        return;
    char *digits = (*otp).digits;
    if (digits)
        Memory_free(digits);
    for (int32_t i = 0; i < INPUTOTP_MAX_BOXES; i++) {
        if ((*otp).boxTex[i] >= 0) {
            Texture_free((*otp).boxTex[i]);
            (*otp).boxTex[i] = -1;
        }
    }
    (*otp).digits = nullptr;
    (*otp).onComplete = nullptr;
    (*otp).ctx = nullptr;
    Memory_free(otp);
}

// ============================================================================
// GETTERS
// ============================================================================

const char *InputOTP_getDigits(const InputOTP *otp) {
    return otp ? (*otp).digits : nullptr;
}

int32_t InputOTP_getLength(const InputOTP *otp) {
    return otp ? (*otp).length : 0;
}

float InputOTP_getBoxSize(const InputOTP *otp) {
    return otp ? (*otp).boxSize : 0.0f;
}

float InputOTP_getGap(const InputOTP *otp) {
    return otp ? (*otp).gap : 0.0f;
}

int32_t InputOTP_getCursor(const InputOTP *otp) {
    return otp ? (*otp).cursor : 0;
}

bool InputOTP_isFocused(const InputOTP *otp) {
    return otp && (*otp).focused;
}

bool InputOTP_isPassword(const InputOTP *otp) {
    return otp && (*otp).password;
}

float InputOTP_getFontSize(const InputOTP *otp) {
    return otp ? (*otp).fontSize : 18.0f;
}

uint32_t InputOTP_getTextColor(const InputOTP *otp) {
    return otp ? (*otp).textColor : 0xFFFFFFFFu;
}

uint32_t InputOTP_getBoxBackground(const InputOTP *otp) {
    return otp ? (*otp).boxBackground : 0xFF18181Bu;
}

uint32_t InputOTP_getBoxBorderColor(const InputOTP *otp) {
    return otp ? (*otp).boxBorderColor : 0xFF3F3F46u;
}

uint32_t InputOTP_getBoxActiveBorder(const InputOTP *otp) {
    return otp ? (*otp).boxActiveBorder : 0xFF3B82F6u;
}

InputOTP_CompleteFn InputOTP_getOnComplete(const InputOTP *otp) {
    return otp ? (*otp).onComplete : nullptr;
}

void *InputOTP_getCtx(const InputOTP *otp) {
    return otp ? (*otp).ctx : nullptr;
}

