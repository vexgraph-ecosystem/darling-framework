#include "darling/field/textarea.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "annotation/incomplete.h"
#include "event/keyevent.h"
#include "event/pointer.h"
#include "input/key.h"
#include "nio/mem.h"
#include "oop/type.h"
#include "text/text_core.h"
#include "vulkan/texture/texture.h"
#include "vulkan/vk.h"
#include "annotation/overview.h"

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Textarea (inherits Panel, LEVEL L2 Behavior)
 * ============================================================================
 * Multi-line text area shell: Panel layout plus an owned text buffer with
 * visible-line count, word-wrap mode, and vertical scroll offset.
 *
 * LIVE EDITING (Pkg 4): byte-wise caret-addressed insert/erase, enter for
 * `\n`, backspace (joins across newlines), left/right by one char, up/down
 * across line boundaries by scanning `\n` (column clamped to the target
 * line length). Every edit fires onChange(ctx) — a notification only, state
 * is pulled via the getters. Caret-follow scroll clamps scrollY so the
 * caret line stays in [scrollY, scrollY + visibleLines - 1]; lineHeight is
 * 1 unit because no font metrics exist yet (documented assumption), so
 * scrollY is in lines.
 *
 * STRUCT FIELDS (Mirroring darling/field/textarea.h):
 * ----------------------------------------------------------------------------
 *   Panel base;            // Inherited layout, bounds, and hierarchy state
 *   char *text;            // Owned UTF-8 buffer
 *   int32_t visibleLines;  // Viewport height in lines
 *   int32_t wrap;          // Wrap mode (0 = off, 1 = word)
 *   float scrollY;         // Vertical scroll offset in lines (lineHeight = 1)
 *   Font *font;            // Optional SDF font descriptor (borrowed)
 *   int32_t cursor;        // Caret offset into text
 *   bool focused;          // True once PTR_DOWN lands (dispatch owns the rest)
 *   Textarea_ChangeFn onChange; // Edit callback; nullptr = none
 *   void *ctx;             // Callback context (borrowed)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Textarea()                     : Textarea_0()
 *   - Textarea(parent, visibleLines) : Textarea_2(parent, visibleLines)
 *
 * Core Functions:
 *   - Textarea_scrollTo(ta, y)
 *   - Textarea_handlePointer(self, kind, localX, localY)
 *   - Textarea_handleKey(self, ev)
 *
 * Setters:
 *   - Textarea_goTo(self, index)
 *   - Textarea_setText(ta, text)
 *   - Textarea_setVisibleLines(ta, lines)
 *   - Textarea_setWrap(ta, wrap)
 *   - Textarea_setScrollY(ta, y)
 *   - Textarea_setFont(ta, font)
 *   - Textarea_setCursor(self, cursor)
 *   - Textarea_setOnChange(self, fn)
 *   - Textarea_setCtx(self, ctx)
 *   - Textarea_free(ta)
 *
 * Getters:
 *   - Textarea_getText(ta)
 *   - Textarea_getVisibleLines(ta)
 *   - Textarea_getWrap(ta)
 *   - Textarea_getScrollY(ta)
 *   - Textarea_getFont(ta)
 *   - Textarea_getCursor(self)
 *   - Textarea_isFocused(self)
 *   - Textarea_getOnChange(self)
 *   - Textarea_getCtx(self)
 * ============================================================================
 */

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

static int32_t clampCursor(size_t len, int32_t cursor) {
    int32_t n = len > (size_t)INT32_MAX ? INT32_MAX : (int32_t)len;
    if (cursor < 0)
        return 0;
    if (cursor > n)
        return n;
    return cursor;
}

static size_t textLen(const Textarea *self) {
    char *cur = self ? (*self).text : nullptr;
    return cur ? strlen(cur) : 0;
}

// ============================================================================
// CONSTRUCTORS
// ============================================================================

#define TEXTAREA_DEFAULT_LINES 4

static void markRasterDirty(Textarea *ta) {
    if (!ta)
        return;
    (*ta).rasterDirty = true;
}

// Line 0 starts at 0; each '\n' at i starts a line at i + 1.
static int32_t lineStart(const char *text, int32_t line) {
    int32_t start = 0;
    int32_t cur = 0;
    if (!text)
        return 0;
    while (cur < line && text[start]) {
        if (text[start] == '\n')
            cur++;
        start++;
    }
    return start;
}

static int32_t lineLen(const char *text, int32_t start) {
    int32_t n = 0;
    if (!text)
        return 0;
    while (text[start + n] && text[start + n] != '\n')
        n++;
    return n;
}

static int32_t lineCount(const char *text) {
    int32_t n = 1;
    if (!text)
        return 1;
    for (int32_t i = 0; text[i]; i++)
        if (text[i] == '\n')
            n++;
    return n;
}

static bool ensureTextareaRaster(Textarea *ta, float innerW) {
    if (!ta || !(*ta).text || (*ta).text[0] == '\0')
        return false;
    if (!(*ta).rasterDirty && (*ta).rasterTex >= 0)
        return true;

    float backing = TextCore_backingScale();
    if (backing <= 0.0f)
        backing = 1.0f;
    float pxH = (*ta).fontSize * backing;
    if (pxH <= 0.0f)
        pxH = 13.0f * backing;

    int32_t selStart = -1, selEnd = -1;
    (void) TextSelect_getSpan(&(*ta).select, &selStart, &selEnd);
    if (selStart == selEnd) {
        selStart = -1;
        selEnd = -1;
    }

    TextStyleDescriptor style = {
        .ligatures = (*ta).ligatures,
        .spacingWidth = (*ta).spacingWidth,
        .spacingHeight = (*ta).spacingHeight,
        .underline = UNDERLINE_NONE,
        .underlineColor = 0,
        .mnemonicIndex = -1,
        .selectionStart = selStart,
        .selectionEnd = selEnd,
        .highlightRadius = 2.0f,
        .highlightColor = (*ta).selectionColor ? (*ta).selectionColor : 0x662563EBu,
        .align = (*ta).align,
        .boundsWidth = innerW,
    };

    uint8_t *rgba = nullptr;
    int w = 0, h = 0;
    bool ok = TextCore_rasterStyled((*ta).text, "Helvetica", pxH, (*ta).textColor, &style, &rgba, &w, &h);
    if (!ok || !rgba || w <= 0 || h <= 0)
        return false;

    if ((*ta).rasterTex >= 0) {
        (*ta).rasterTex = Texture_replaceRaw((*ta).rasterTex, rgba, (uint32_t) w, (uint32_t) h);
    } else {
        (*ta).rasterTex = Texture_loadRaw(rgba, (uint32_t) w, (uint32_t) h);
    }
    free(rgba);
    (*ta).rasterW = w;
    (*ta).rasterH = h;
    (*ta).rasterBacking = backing;
    (*ta).rasterDirty = false;
    return true;
}

static void Textarea_renderFn(Panel *panel, void *renderer, void *cmdBuffer, float surfaceW, float surfaceH,
                             float x, float y, float w, float h) {
    Textarea *ta = (Textarea*) panel;
    (void) renderer;
    if (!ta || w <= 0.0f || h <= 0.0f)
        return;
    float op = Container_getOpacity(&(*panel).base);
    if (op <= 0.0f)
        return;

    uint32_t bg = Panel_getBackgroundColor(panel);
    if ((bg >> 24) == 0)
        bg = 0xFF18181Bu;
    float br = ((bg >> 16) & 0xFF) / 255.0f;
    float bgc = ((bg >> 8) & 0xFF) / 255.0f;
    float bb = (bg & 0xFF) / 255.0f;
    float ba = ((bg >> 24) & 0xFF) / 255.0f * op;
    Vk_fillRect(cmdBuffer, surfaceW, surfaceH, x, y, w, h, br, bgc, bb, ba);

    uint32_t borderColor = (*ta).focused ? 0xFF3B82F6u : 0xFF3F3F46u;
    float b_r = ((borderColor >> 16) & 0xFF) / 255.0f;
    float b_g = ((borderColor >> 8) & 0xFF) / 255.0f;
    float b_b = (borderColor & 0xFF) / 255.0f;
    float b_a = ((borderColor >> 24) & 0xFF) / 255.0f * op;
    Vk_fillRect(cmdBuffer, surfaceW, surfaceH, x, y, w, 1.0f, b_r, b_g, b_b, b_a);
    Vk_fillRect(cmdBuffer, surfaceW, surfaceH, x, y + h - 1.0f, w, 1.0f, b_r, b_g, b_b, b_a);
    Vk_fillRect(cmdBuffer, surfaceW, surfaceH, x, y, 1.0f, h, b_r, b_g, b_b, b_a);
    Vk_fillRect(cmdBuffer, surfaceW, surfaceH, x + w - 1.0f, y, 1.0f, h, b_r, b_g, b_b, b_a);

    float padX = 8.0f;
    float padY = 8.0f;
    float innerW = w - padX * 2.0f;
    if (innerW < 10.0f)
        innerW = 10.0f;

    if ((*ta).text && (*ta).text[0] != '\0') {
        if (ensureTextareaRaster(ta, innerW)) {
            if ((*ta).rasterTex >= 0 && (*ta).rasterW > 0 && (*ta).rasterH > 0) {
                float backing = (*ta).rasterBacking > 0.0f ? (*ta).rasterBacking : 1.0f;
                float qw = (float) (*ta).rasterW / backing;
                float qh = (float) (*ta).rasterH / backing;
                float qx = x + padX;
                float qy = y + h - padY - qh + (*ta).scrollY;
                Vk_drawTexture(cmdBuffer, surfaceW, surfaceH, qx, qy, qw, qh, 1.0f, 1.0f, 1.0f, op,
                               (*ta).rasterTex, PICTURE_MODE_FIT, (float) (*ta).rasterW, (float) (*ta).rasterH);
            }
        }
    }

    if ((*ta).focused) {
        int32_t line = 0, col = 0;
        int32_t cur = (*ta).cursor;
        if ((*ta).text) {
            for (int32_t i = 0; i < cur && (*ta).text[i] != '\0'; i++) {
                if ((*ta).text[i] == '\n') {
                    line++;
                    col = 0;
                } else {
                    col++;
                }
            }
        }
        float lineH = (*ta).fontSize * 1.35f + (*ta).spacingHeight;
        float charW = (*ta).fontSize * 0.55f;
        float alignOff = 0.0f;
        if ((*ta).align != TEXT_ALIGN_LEFT && (*ta).text) {
            int32_t start = lineStart((*ta).text, line);
            int32_t len = lineLen((*ta).text, start);
            float totalW = (float)len * charW;
            if ((*ta).align == TEXT_ALIGN_CENTER && innerW > totalW)
                alignOff = (innerW - totalW) * 0.5f;
            else if ((*ta).align == TEXT_ALIGN_RIGHT && innerW > totalW)
                alignOff = innerW - totalW;
        }
        float caretX = padX + alignOff + (float) col * charW;
        float caretY = y + h - padY - (float)(line + 1) * lineH + (*ta).scrollY;
        if (caretY >= y && caretY + lineH <= y + h + 2.0f) {
            Vk_fillRect(cmdBuffer, surfaceW, surfaceH, x + caretX, caretY, 1.5f, lineH, 1.0f, 1.0f, 1.0f, op);
        }
    }
}

Textarea *Textarea_0(void) {
    Textarea *ta = (Textarea*) Memory_alloc(TYPE_TEXTAREA_SINGLETON, sizeof(Textarea));
    if (!ta)
        return nullptr;
    Panel *bp = Panel_0();
    if (!bp) {
        Memory_free(ta);
        return nullptr;
    }
    (*ta).base = (*bp);
    Memory_free(bp);
    (*ta).text = nullptr;
    (*ta).visibleLines = TEXTAREA_DEFAULT_LINES;
    (*ta).wrap = 1;
    (*ta).scrollY = 0.0f;
    (*ta).font = nullptr;
    (*ta).cursor = 0;
    (*ta).focused = false;
    (*ta).onChange = nullptr;
    (*ta).ctx = nullptr;

    (*ta).fontSize = 13.0f;
    (*ta).textColor = 0xFFFFFFFFu;
    (*ta).align = TEXT_ALIGN_LEFT;
    (*ta).spacingWidth = 0.0f;
    (*ta).spacingHeight = 0.0f;
    (*ta).ligatures = true;
    (*ta).select = TextSelect_default();
    (*ta).selectionColor = 0x662563EBu;
    (*ta).rasterTex = -1;
    (*ta).rasterW = 0;
    (*ta).rasterH = 0;
    (*ta).rasterBacking = 1.0f;
    (*ta).rasterDirty = true;
    Panel_setRenderHandler(&(*ta).base, Textarea_renderFn);
    return ta;
}

Textarea *Textarea_2(Panel *parent, int32_t visibleLines) {
    Textarea *ta = Textarea_0();
    if (!ta)
        return nullptr;
    (*ta).visibleLines = visibleLines;
    if (parent) {
        Panel *bp = &(*ta).base;
        Panel_addContainer(parent, bp);
    }
    return ta;
}

// ============================================================================
// CORE FUNCTIONS
// ============================================================================

void Textarea_scrollTo(Textarea *ta, float y) {
    ;;INCOMPLETE // clamped scroll lands with the scroll walker
    (void)ta;
    (void)y;
}

// ============================================================================
// SETTERS
// ============================================================================

static void markDirty(Textarea *ta) {
    if (!ta)
        return;
    Panel *bp = &(*ta).base;
    Container_markDirty(&(*bp).base);
}

void Textarea_setText(Textarea *ta, const char *text) {
    if (!ta)
        return;
    char *old = (*ta).text;
    if (old) {
        Memory_free(old);
        (*ta).text = nullptr;
    }
    if (text) {
        size_t len = strlen(text) + 1;
        char *buf = (char*) Memory_alloc(TYPE_ARRAY, len);
        if (buf)
            memcpy(buf, text, len);
        (*ta).text = buf;
    }
    (*ta).cursor = clampCursor(textLen(ta), (*ta).cursor);
    TextSelect_reset(&(*ta).select);
    markRasterDirty(ta);
    markDirty(ta);
}

void Textarea_setVisibleLines(Textarea *ta, int32_t lines) {
    if (!ta)
        return;
    (*ta).visibleLines = lines;
    markDirty(ta);
}

void Textarea_setWrap(Textarea *ta, int32_t wrap) {
    if (!ta)
        return;
    (*ta).wrap = wrap;
    markDirty(ta);
}

void Textarea_setScrollY(Textarea *ta, float y) {
    if (!ta)
        return;
    (*ta).scrollY = y;
    markDirty(ta);
}

void Textarea_setFont(Textarea *ta, Font *font) {
    if (!ta)
        return;
    (*ta).font = font;
    markDirty(ta);
}

void Textarea_goTo(Textarea *self, int32_t index) {
    if (!self)
        return;
    (*self).cursor = clampCursor(textLen(self), index);
    markDirty(self);
}

void Textarea_setCursor(Textarea *self, int32_t cursor) {
    Textarea_goTo(self, cursor);
}

void Textarea_setOnChange(Textarea *self, Textarea_ChangeFn fn) {
    if (!self)
        return;
    (*self).onChange = fn;
}

void Textarea_setCtx(Textarea *self, void *ctx) {
    if (!self)
        return;
    (*self).ctx = ctx;
}

void Textarea_free(Textarea *ta) {
    if (!ta)
        return;
    char *text = (*ta).text;
    if (text)
        Memory_free(text);
    if ((*ta).rasterTex >= 0) {
        Texture_free((*ta).rasterTex);
        (*ta).rasterTex = -1;
    }
    (*ta).text = nullptr;
    (*ta).font = nullptr;
    (*ta).onChange = nullptr;
    (*ta).ctx = nullptr;
    Memory_free(ta);
}

// ============================================================================
// LIVE EDITING (Pkg 4)
// ============================================================================

static void fireChange(Textarea *self) {
    if (!self)
        return;
    Textarea_ChangeFn fn = (*self).onChange;
    if (fn)
        fn((*self).ctx);
}

// Caret line = count of '\n' before the cursor.
static int32_t caretLine(const Textarea *self) {
    if (!self)
        return 0;
    char *cur = (*self).text;
    int32_t at = (*self).cursor;
    int32_t line = 0;
    for (int32_t i = 0; i < at; i++) {
        if (!cur || !cur[i])
            break;
        if (cur[i] == '\n')
            line++;
    }
    return line;
}

// Caret-follow scroll: clamp scrollY so the caret line stays in
// [scrollY, scrollY + visibleLines - 1]. Line height is 1 unit — no font
// metrics exist yet, so scrollY is counted in lines (documented assumption).
static void followCaret(Textarea *self) {
    if (!self)
        return;
    int32_t line = caretLine(self);
    int32_t vis = (*self).visibleLines;
    if (vis < 1)
        vis = 1;
    float top = (*self).scrollY;
    if ((float)line < top)
        top = (float)line;
    if ((float)line > top + (float)(vis - 1))
        top = (float)line - (float)(vis - 1);
    if (top < 0.0f)
        top = 0.0f;
    if (top != (*self).scrollY) {
        (*self).scrollY = top;
        markDirty(self);
    }
}

static void insertAt(Textarea *self, char c) {
    if (!self)
        return;
    char *cur = (*self).text;
    size_t len = cur ? strlen(cur) : 0;
    int32_t at = clampCursor(len, (*self).cursor);
    char *buf = (char*) Memory_alloc(TYPE_ARRAY, len + 2);
    if (!buf)
        return;
    if (cur && (size_t)at > 0)
        memcpy(buf, cur, (size_t)at);
    buf[at] = c;
    if (cur)
        memcpy(buf + at + 1, cur + at, len - (size_t)at + 1);
    else
        buf[at + 1] = '\0';
    if (cur)
        Memory_free(cur);
    (*self).text = buf;
    (*self).cursor = at + 1;
    markRasterDirty(self);
    followCaret(self);
    markDirty(self);
    fireChange(self);
}

static void eraseAt(Textarea *self) {
    if (!self)
        return;
    char *cur = (*self).text;
    if (!cur)
        return;
    size_t len = strlen(cur);
    int32_t at = clampCursor(len, (*self).cursor);
    if (at <= 0)
        return;
    memmove(cur + at - 1, cur + at, len - (size_t)at + 1);
    (*self).cursor = at - 1;
    markRasterDirty(self);
    followCaret(self);
    markDirty(self);
    fireChange(self);
}

static void moveVertical(Textarea *self, int32_t dir) {
    if (!self)
        return;
    char *cur = (*self).text;
    int32_t line = caretLine(self);
    int32_t col = clampCursor(textLen(self), (*self).cursor) - lineStart(cur, line);
    int32_t next = line + dir;
    if (next < 0)
        return;
    if (next >= lineCount(cur))
        return;
    int32_t start = lineStart(cur, next);
    int32_t n = lineLen(cur, start);
    if (col > n)
        col = n;
    Textarea_goTo(self, start + col);
    followCaret(self);
}

void Textarea_handlePointer(Textarea *self, int kind, float localX, float localY) {
    if (!self)
        return;
    float padX = 8.0f;
    float padY = 8.0f;
    Panel *p = &(*self).base;
    float h = Container_getHeight(&(*p).base);
    float w = Container_getWidth(&(*p).base);
    float innerW = w - padX * 2.0f;
    if (innerW < 10.0f) innerW = 10.0f;

    float lineH = (*self).fontSize * 1.35f + (*self).spacingHeight;
    float charW = (*self).fontSize * 0.55f;
    float topY = h - padY + (*self).scrollY;
    float relY = topY - localY;
    int32_t targetLine = (int32_t)(relY / lineH);
    if (targetLine < 0) targetLine = 0;
    int32_t totalLines = lineCount((*self).text);
    if (targetLine >= totalLines) targetLine = totalLines - 1;

    int32_t start = lineStart((*self).text, targetLine);
    int32_t lLen = lineLen((*self).text, start);
    float alignOff = 0.0f;
    if ((*self).align != TEXT_ALIGN_LEFT) {
        float totalW = (float)lLen * charW;
        if ((*self).align == TEXT_ALIGN_CENTER && innerW > totalW)
            alignOff = (innerW - totalW) * 0.5f;
        else if ((*self).align == TEXT_ALIGN_RIGHT && innerW > totalW)
            alignOff = innerW - totalW;
    }
    float innerX = localX - padX - alignOff;
    int32_t col = (int32_t) roundf(innerX / charW);
    if (col < 0) col = 0;
    if (col > lLen) col = lLen;
    int32_t best = start + col;

    if (kind == PTR_DOWN) {
        (*self).focused = true;
        TextSelect_begin(&(*self).select, best);
        Textarea_goTo(self, best);
        markRasterDirty(self);
        return;
    }
    if (kind == PTR_DRAG) {
        if (!(*self).focused) return;
        TextSelect_drag(&(*self).select, best);
        Textarea_goTo(self, best);
        markRasterDirty(self);
        return;
    }
    if (kind == PTR_UP) {
        int32_t s0 = -1, s1 = -1;
        if (TextSelect_getSpan(&(*self).select, &s0, &s1) && s0 == s1) {
            TextSelect_reset(&(*self).select);
        }
        markRasterDirty(self);
        return;
    }
}

void Textarea_handleKey(Textarea *self, const UIKeyEvent *ev) {
    if (!self || !ev)
        return;
    if (!UIKeyEvent_isPressed(ev))
        return;
    int32_t code = UIKeyEvent_getKeyCode(ev);
    int32_t ch = UIKeyEvent_getCh(ev);
    uint32_t mods = UIKeyEvent_getMods(ev);
    bool cmdOrCtrl = (mods & (8u | 2u)) != 0;
    bool shift = (mods & 1u) != 0;

    if (cmdOrCtrl) {
        char *cur = (*self).text;
        size_t len = cur ? strlen(cur) : 0;
        if (code == KEY_A) {
            TextSelect_begin(&(*self).select, 0);
            TextSelect_drag(&(*self).select, (int32_t) len);
            Textarea_goTo(self, (int32_t) len);
            markRasterDirty(self);
            markDirty(self);
            return;
        }
        if (code == KEY_C) {
            char *sel = Textarea_getSelectedText(self);
            if (sel) {
                TextCore_copyToClipboard(sel);
                Memory_free(sel);
            }
            return;
        }
        if (code == KEY_X) {
            char *sel = Textarea_getSelectedText(self);
            if (sel) {
                TextCore_copyToClipboard(sel);
                Memory_free(sel);
                Textarea_setSelectedText(self, "");
            }
            return;
        }
        if (code == KEY_V) {
            char *clip = TextCore_pasteFromClipboard();
            if (clip) {
                Textarea_setSelectedText(self, clip);
                free(clip);
            }
            return;
        }
    }

    if (code == KEY_BACKSPACE || ch == 8) {
        int32_t s0 = -1, s1 = -1;
        if (TextSelect_getSpan(&(*self).select, &s0, &s1) && s0 != s1) {
            Textarea_setSelectedText(self, "");
        } else {
            eraseAt(self);
        }
        return;
    }
    if (code == KEY_ENTER || ch == '\n' || ch == '\r') {
        int32_t s0 = -1, s1 = -1;
        if (TextSelect_getSpan(&(*self).select, &s0, &s1) && s0 != s1) {
            Textarea_setSelectedText(self, "\n");
        } else {
            insertAt(self, '\n');
        }
        return;
    }
    if (code == KEY_LEFT) {
        int32_t next = (*self).cursor - 1;
        if (next < 0) next = 0;
        if (shift) {
            int32_t s0 = -1, s1 = -1;
            if (!TextSelect_getSpan(&(*self).select, &s0, &s1)) {
                TextSelect_begin(&(*self).select, (*self).cursor);
            }
            TextSelect_drag(&(*self).select, next);
        } else {
            TextSelect_reset(&(*self).select);
        }
        Textarea_goTo(self, next);
        followCaret(self);
        markRasterDirty(self);
        return;
    }
    if (code == KEY_RIGHT) {
        char *cur = (*self).text;
        size_t len = cur ? strlen(cur) : 0;
        int32_t next = (*self).cursor + 1;
        if ((size_t)next > len) next = (int32_t) len;
        if (shift) {
            int32_t s0 = -1, s1 = -1;
            if (!TextSelect_getSpan(&(*self).select, &s0, &s1)) {
                TextSelect_begin(&(*self).select, (*self).cursor);
            }
            TextSelect_drag(&(*self).select, next);
        } else {
            TextSelect_reset(&(*self).select);
        }
        Textarea_goTo(self, next);
        followCaret(self);
        markRasterDirty(self);
        return;
    }
    if (code == KEY_UP) {
        TextSelect_reset(&(*self).select);
        moveVertical(self, -1);
        markRasterDirty(self);
        return;
    }
    if (code == KEY_DOWN) {
        TextSelect_reset(&(*self).select);
        moveVertical(self, 1);
        markRasterDirty(self);
        return;
    }
    if (ch >= 32 && ch <= 126) {
        int32_t s0 = -1, s1 = -1;
        if (TextSelect_getSpan(&(*self).select, &s0, &s1) && s0 != s1) {
            char ins[2] = {(char)ch, '\0'};
            Textarea_setSelectedText(self, ins);
        } else {
            insertAt(self, (char)ch);
        }
        return;
    }
}

char *Textarea_getSelectedText(const Textarea *ta) {
    if (!ta) return nullptr;
    int32_t s0 = -1, s1 = -1;
    if (!TextSelect_getSpan(&(*ta).select, &s0, &s1) || s0 == s1)
        return nullptr;
    const char *txt = (*ta).text;
    if (!txt) return nullptr;
    int32_t len = s1 - s0;
    if (len <= 0) return nullptr;
    char *res = (char*) Memory_alloc(TYPE_ARRAY, (size_t)len + 1);
    if (!res) return nullptr;
    memcpy(res, txt + s0, (size_t)len);
    res[len] = '\0';
    return res;
}

void Textarea_setSelectedText(Textarea *ta, const char *newText) {
    if (!ta) return;
    int32_t s0 = -1, s1 = -1;
    if (!TextSelect_getSpan(&(*ta).select, &s0, &s1) || s0 == s1) {
        s0 = (*ta).cursor;
        s1 = (*ta).cursor;
    }
    const char *old = (*ta).text ? (*ta).text : "";
    size_t oldLen = strlen(old);
    if (s0 < 0) s0 = 0;
    if ((size_t)s0 > oldLen) s0 = (int32_t)oldLen;
    if (s1 < s0) s1 = s0;
    if ((size_t)s1 > oldLen) s1 = (int32_t)oldLen;

    const char *ins = newText ? newText : "";
    size_t insLen = strlen(ins);
    size_t newLen = (size_t)s0 + insLen + (oldLen - (size_t)s1);

    char *buf = (char*) Memory_alloc(TYPE_ARRAY, newLen + 1);
    if (!buf) return;
    if (s0 > 0)
        memcpy(buf, old, (size_t)s0);
    if (insLen > 0)
        memcpy(buf + s0, ins, insLen);
    if (oldLen > (size_t)s1)
        memcpy(buf + s0 + insLen, old + s1, oldLen - (size_t)s1);
    buf[newLen] = '\0';

    if ((*ta).text)
        Memory_free((*ta).text);
    (*ta).text = buf;
    (*ta).cursor = s0 + (int32_t)insLen;
    TextSelect_reset(&(*ta).select);
    markRasterDirty(ta);
    followCaret(ta);
    markDirty(ta);
    fireChange(ta);
}

void Textarea_setFontSize(Textarea *ta, float size) {
    if (!ta) return;
    (*ta).fontSize = size;
    markRasterDirty(ta);
    markDirty(ta);
}

void Textarea_setTextColor(Textarea *ta, uint32_t color) {
    if (!ta) return;
    (*ta).textColor = color;
    markRasterDirty(ta);
    markDirty(ta);
}

void Textarea_setTextAlign(Textarea *ta, TextAlign align) {
    if (!ta) return;
    (*ta).align = align;
    markRasterDirty(ta);
    markDirty(ta);
}

void Textarea_setSpacingWidth(Textarea *ta, float width) {
    if (!ta) return;
    (*ta).spacingWidth = width;
    markRasterDirty(ta);
    markDirty(ta);
}

void Textarea_setSpacingHeight(Textarea *ta, float height) {
    if (!ta) return;
    (*ta).spacingHeight = height;
    markRasterDirty(ta);
    markDirty(ta);
}

void Textarea_setLigatures(Textarea *ta, bool flag) {
    if (!ta) return;
    (*ta).ligatures = flag;
    markRasterDirty(ta);
    markDirty(ta);
}

void Textarea_setSelection(Textarea *ta, int32_t start, int32_t end) {
    if (!ta) return;
    if (start < 0 || end < 0 || start == end) {
        TextSelect_reset(&(*ta).select);
    } else {
        TextSelect_begin(&(*ta).select, start);
        TextSelect_drag(&(*ta).select, end);
    }
    markRasterDirty(ta);
    markDirty(ta);
}

void Textarea_setSelectionColor(Textarea *ta, uint32_t color) {
    if (!ta) return;
    (*ta).selectionColor = color;
    markRasterDirty(ta);
    markDirty(ta);
}

// ============================================================================
// GETTERS
// ============================================================================

const char *Textarea_getText(const Textarea *ta) {
    return ta ? (*ta).text : nullptr;
}

int32_t Textarea_getVisibleLines(const Textarea *ta) {
    return ta ? (*ta).visibleLines : 0;
}

int32_t Textarea_getWrap(const Textarea *ta) {
    return ta ? (*ta).wrap : 0;
}

float Textarea_getScrollY(const Textarea *ta) {
    return ta ? (*ta).scrollY : 0.0f;
}

Font *Textarea_getFont(const Textarea *ta) {
    return ta ? (*ta).font : nullptr;
}

int32_t Textarea_getCursor(const Textarea *self) {
    return self ? (*self).cursor : 0;
}

bool Textarea_isFocused(const Textarea *self) {
    return self && (*self).focused;
}

Textarea_ChangeFn Textarea_getOnChange(const Textarea *self) {
    return self ? (*self).onChange : nullptr;
}

void *Textarea_getCtx(const Textarea *self) {
    return self ? (*self).ctx : nullptr;
}

float Textarea_getFontSize(const Textarea *ta) {
    return ta ? (*ta).fontSize : 13.0f;
}

uint32_t Textarea_getTextColor(const Textarea *ta) {
    return ta ? (*ta).textColor : 0xFFFFFFFFu;
}

TextAlign Textarea_getTextAlign(const Textarea *ta) {
    return ta ? (*ta).align : TEXT_ALIGN_LEFT;
}

float Textarea_getSpacingWidth(const Textarea *ta) {
    return ta ? (*ta).spacingWidth : 0.0f;
}

float Textarea_getSpacingHeight(const Textarea *ta) {
    return ta ? (*ta).spacingHeight : 0.0f;
}

bool Textarea_hasLigatures(const Textarea *ta) {
    return ta ? (*ta).ligatures : true;
}

void Textarea_getSelection(const Textarea *ta, int32_t *outStart, int32_t *outEnd) {
    if (!ta) {
        if (outStart) *outStart = -1;
        if (outEnd) *outEnd = -1;
        return;
    }
    if (!TextSelect_getSpan(&(*ta).select, outStart, outEnd)) {
        if (outStart) *outStart = -1;
        if (outEnd) *outEnd = -1;
    }
}

uint32_t Textarea_getSelectionColor(const Textarea *ta) {
    return ta ? (*ta).selectionColor : 0x662563EBu;
}
