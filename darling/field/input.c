#include "darling/field/input.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "annotation/incomplete.h"
#include "darling/anim/anim.h"
#include "event/keyevent.h"
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
 * DEFINITION: Input
 * ============================================================================
 * Single-line text input shell: Panel layout plus an owned text buffer bounded
 * by cap, with change/submit callback slots. The text and placeholder buffers
 * are owned (cap-bounded, setText truncates); font, caretView, and ctx are
 * borrowed views. The caret is its own part — a view over typing state
 * (BLINK/SOLID/GLIDE) that never measures text itself; the owner places the
 * target via caret_setTarget and ticks it on Thread 0 next to layout, marking
 * only the Input child dirty. Typography and native raster styling (fontSize,
 * textColor, align, selection, raster cache) live on the owner; editing is
 * byte-wise UTF-8 surgery with cap truncation, blink restart, and onChange
 * firing. Zero steady-state allocation in tick/render paths.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Input (inherits Panel)
 * LEVEL: L2 — Behavior (single-line text input)
 * ============================================================================
 * Single-line text input shell: Panel layout plus an owned text buffer
 * bounded by cap, with change/submit callback slots for later wiring.
 *
 * THE CARET (its own part — field->caret->verb):
 * ----------------------------------------------------------------------------
 * The caret is a VIEW over typing state, never pierced directly. It owns
 * mode (BLINK terminal / SOLID always-on / GLIDE Word-style eased slide),
 * color, blink half-period, the blink clock/phase, and the painted x which
 * eases toward the owner-measured target in GLIDE mode (ANIM_EASE_OUT over
 * 80ms via Anim_eval — the animation system, reused, not reinvented).
 * Position resolution (cursor index -> x) lands with the caret walker; the
 * owner places the target with caret_setTarget after measuring. Typing
 * restarts the blink phase shown. Tick on Thread 0 next to layout.
 *
 * STRUCT FIELDS (Mirroring darling/field/input.h — same part banners):
 * ----------------------------------------------------------------------------
 *   --- Input core (owner fields) ---
 *   Panel base;              // Inherited layout, bounds, and hierarchy state
 *   char *text;              // Owned UTF-8 buffer (bounded by cap)
 *   size_t cap;              // Max stored chars excluding NUL
 *   char *placeholder;       // Owned hint string shown when empty
 *   bool password;           // Mask glyphs at render time
 *   bool readonly;           // Reject edits, still selectable
 *   bool focused;            // Focus-request flag (DOWN sets, dispatch consumes later)
 *   int32_t cursor;          // Caret offset into text
 *   Font *font;              // Optional SDF font descriptor (borrowed)
 *   --- Typography & Native Raster Styling (owner fields, continued) ---
 *   float fontSize;          // Font size in points (default 13.0)
 *   uint32_t textColor;      // Text color packed ARGB (default white)
 *   uint32_t placeholderColor; // Placeholder color packed ARGB
 *   TextAlign align;         // LEFT/CENTER/RIGHT alignment
 *   float spacingWidth;      // Kerning/tracking delta in points
 *   bool ligatures;          // Standard typography ligatures (default true)
 *   TextSelect select;       // Text selection state (anchor/active edges)
 *   uint32_t selectionColor; // Highlight fill (default 0x662563EB)
 *   int32_t rasterTex;       // Cached text raster texture (-1 = none)
 *   int rasterW;             // Raster width in px
 *   int rasterH;             // Raster height in px
 *   float rasterBacking;     // Backing scale the raster was baked at
 *   bool rasterDirty;        // Raster needs re-bake
 *   float *glyphX;           // Per-byte pen offsets (len+1, arena-owned)
 *   int32_t glyphN;          // Glyph table length
 *   --- Caret part (views only) ---
 *   int caretMode;           // BLINK/SOLID/GLIDE (default BLINK)
 *   uint32_t caretColor;     // Packed 0xAARRGGBB (default white)
 *   float caretBlinkPeriod;  // Half-cycle seconds (default 0.53)
 *   double caretClock;       // Blink timer (tick advances)
 *   bool caretShown;         // Current blink phase (view)
 *   float caretX;            // Painted x (glides to target)
 *   float caretTargetX;      // Owner-measured x (view target)
 *   Panel *caretView;        // Borrowed visual (null = thin rect)
 *   float caretOpacity;      // User opacity 0..1 (× blink phase)
 *   --- Input core callbacks (owner fields, continued) ---
 *   Input_ChangeFn onChange; // Edit callback; nullptr = none
 *   Input_SubmitFn onSubmit; // Commit callback; nullptr = none
 *   void *ctx;               // Callback context (borrowed)
 *   Input_MeasureFn measurer;// Index->x hook (null until the walker lands)
 *   void *measureCtx;        // Measure context (borrowed)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Input()                : Input_0()
 *   - Input(parent, cap)     : Input_2(parent, cap)
 *
 * Core Functions:
 *   - Input_insertChar(inp, c)
 *   - Input_eraseChar(inp)
 *   - Input_handlePointer(self, kind, localX, localY)
 *   - Input_handleKey(self, ev)
 *   - inputPaintBackground(panel, ...) : Stage 0 field fill
 *   - inputPaintText(panel, ...) : Stage 2 raster / placeholder / mask
 *   - inputPaintBorder(panel, ...) : Stage 3 focused/idle stroke
 *   - inputPaintCaret(panel, ...) : Stage 4 caret rect / caret view
 *
 * Setters:
 *   - Input_goTo(inp, index)
 *   - Input_setText(inp, text)
 *   - Input_setCap(inp, cap)
 *   - Input_setPlaceholder(inp, placeholder)
 *   - Input_setPassword(inp, password)
 *   - Input_setReadonly(inp, readonly)
 *   - Input_setFocused(inp, focused)
 *   - Input_setCursor(inp, cursor)
 *   - Input_setFont(inp, font)
 *   - Input_setOnChange(inp, fn)
 *   - Input_setOnSubmit(inp, fn)
 *   - Input_setCtx(inp, ctx)
 *   - Input_setMeasurer(inp, fn, ctx)
 *   - Input_setFontSize(inp, size)
 *   - Input_setTextColor(inp, color)
 *   - Input_setPlaceholderColor(inp, color)
 *   - Input_setTextAlign(inp, align)
 *   - Input_setSpacingWidth(inp, width)
 *   - Input_setLigatures(inp, ligatures)
 *   - Input_setSelection(inp, start, end)
 *   - Input_setSelectionColor(inp, color)
 *   - Input_setSelectedText(inp, text)
 *   - Input_free(inp)
 *
 * Caret part:
 *   - Input_caret_setMode(inp, mode)
 *   - Input_caret_setColor(inp, color)
 *   - Input_caret_setBlinkPeriod(inp, seconds)
 *   - Input_caret_setTarget(inp, x)
 *   - Input_caret_setView(inp, view)
 *   - Input_caret_setOpacity(inp, opacity)
 *   - Input_caret_placeView(inp, view, centerY)
 *   - Input_caret_tick(inp, dt) (depth-1 proving element: blink flip marks
 *     ONLY the Input child dirty — never the board/tree — so Loop1
 *     re-collages the tiny child target at ~2Hz)
 *
 * Getters:
 *   - Input_getText(inp)
 *   - Input_getCap(inp)
 *   - Input_getPlaceholder(inp)
 *   - Input_isPassword(inp)
 *   - Input_isReadonly(inp)
 *   - Input_isFocused(inp)
 *   - Input_getCursor(inp)
 *   - Input_getFont(inp)
 *   - Input_getFontSize(inp)
 *   - Input_getTextColor(inp)
 *   - Input_getPlaceholderColor(inp)
 *   - Input_getTextAlign(inp)
 *   - Input_getSpacingWidth(inp)
 *   - Input_hasLigatures(inp)
 *   - Input_getSelection(inp, outStart, outEnd)
 *   - Input_getSelectionColor(inp)
 *   - Input_getSelectedText(inp)
 *   - Input_getOnChange(inp)
 *   - Input_getOnSubmit(inp)
 *   - Input_getCtx(inp)
 *   - Input_getMeasurer(inp)
 *   - Input_getMeasureContext(inp)
 *   - Input_caret_getMode(inp)
 *   - Input_caret_getColor(inp)
 *   - Input_caret_getBlinkPeriod(inp)
 *   - Input_caret_getTarget(inp)
 *   - Input_caret_getX(inp)
 *   - Input_caret_isShown(inp)
 *   - Input_caret_getView(inp)
 *   - Input_caret_getOpacity(inp)
 *   - Input_caret_getEffectiveOpacity(inp)
 * ============================================================================
 */

// ============================================================================
// CONSTRUCTORS
// ============================================================================

#define INPUT_DEFAULT_CAP 256

static void markRasterDirty(Input *inp) {
    if (!inp)
        return;
    (*inp).rasterDirty = true;
}

static bool ensureInputRaster(Input *inp, const char *displayText, bool isPlaceholder, float innerW) {
    if (!inp || !displayText || displayText[0] == '\0')
        return false;
    if (!(*inp).rasterDirty && (*inp).rasterTex >= 0)
        return true;

    float backing = TextCore_backingScale();
    if (backing <= 0.0f)
        backing = 1.0f;
    float pxH = (*inp).fontSize * backing;
    if (pxH <= 0.0f)
        pxH = 13.0f * backing;

    int32_t selStart = -1, selEnd = -1;
    if (!isPlaceholder) {
        (void) TextSelect_getSpan(&(*inp).select, &selStart, &selEnd);
        if (selStart == selEnd) {
            selStart = -1;
            selEnd = -1;
        }
    }

    TextStyleDescriptor style = {
        .ligatures = (*inp).ligatures,
        .spacingWidth = (*inp).spacingWidth,
        .spacingHeight = 0.0f,
        .underline = UNDERLINE_NONE,
        .underlineColor = 0,
        .mnemonicIndex = -1,
        .selectionStart = selStart,
        .selectionEnd = selEnd,
        .highlightRadius = 2.0f,
        .highlightColor = (*inp).selectionColor ? (*inp).selectionColor : 0x662563EBu,
        .align = (*inp).align,
        .boundsWidth = innerW,
    };

    uint32_t col = isPlaceholder ? (*inp).placeholderColor : (*inp).textColor;
    uint8_t *rgba = nullptr;
    int w = 0, h = 0;
    bool ok = TextCore_rasterStyled(displayText, "Helvetica", pxH, col, &style, &rgba, &w, &h);
    if (!ok || !rgba || w <= 0 || h <= 0)
        return false;

    if ((*inp).rasterTex >= 0) {
        (*inp).rasterTex = Texture_replaceRaw((*inp).rasterTex, rgba, (uint32_t) w, (uint32_t) h);
    } else {
        (*inp).rasterTex = Texture_loadRaw(rgba, (uint32_t) w, (uint32_t) h);
    }
    free(rgba);
    (*inp).rasterW = w;
    (*inp).rasterH = h;
    (*inp).rasterBacking = backing;
    (*inp).rasterDirty = false;

    size_t len = strlen(displayText);
    if (!isPlaceholder && len < 512) {
        float cleanOff[512];
        int32_t n = TextCore_lineOffsets(displayText, "Helvetica", pxH, (*inp).ligatures, (*inp).spacingWidth, cleanOff, 512);
        if (n == (int32_t)(len + 1)) {
            if ((*inp).glyphX)
                Memory_free((*inp).glyphX);
            (*inp).glyphX = (float*) Memory_alloc(TYPE_ARRAY, (len + 1) * sizeof(float));
            if ((*inp).glyphX) {
                float totalAdv = cleanOff[len];
                float alignOff = 0.0f;
                if ((*inp).align == TEXT_ALIGN_CENTER && innerW > totalAdv)
                    alignOff = (innerW - totalAdv) * 0.5f;
                else if ((*inp).align == TEXT_ALIGN_RIGHT && innerW > totalAdv)
                    alignOff = innerW - totalAdv;
                for (size_t i = 0; i <= len; i++)
                    (*inp).glyphX[i] = cleanOff[i] + alignOff;
                (*inp).glyphN = (int32_t)(len + 1);
            }
        }
    }
    return true;
}

// Ordered part pipeline: background -> text -> border -> caret.
// Border sits over content per the canonical order (edge pixels never
// overlap padded text, so the reorder from the legacy
// bg/border/text/caret sequence is pixel-identical).

// Stage 0: field fill (Panel color, dark fallback when transparent).
static bool inputPaintBackground(Panel *panel, void *renderer, void *cmdBuffer,
                                 float surfaceW, float surfaceH,
                                 float x, float y, float w, float h) {
    (void) renderer;
    if (!panel || !cmdBuffer)
        return false;
    if (w <= 0.0f || h <= 0.0f)
        return false;
    Component *c = &(*panel).component;
    float op = Component_getOpacity(c);
    if (op <= 0.0f)
        return false;
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

// Stage 2: raster / placeholder / password-mask text quad.
static bool inputPaintText(Panel *panel, void *renderer, void *cmdBuffer,
                           float surfaceW, float surfaceH,
                           float x, float y, float w, float h) {
    Input *inp = (Input*) panel;
    (void) renderer;
    if (!inp || !cmdBuffer)
        return false;
    if (w <= 0.0f || h <= 0.0f)
        return false;
    Component *c = &(*panel).component;
    float op = Component_getOpacity(c);
    if (op <= 0.0f)
        return false;
    const char *displayText = (*inp).text;
    bool isPlace = false;
    if (!displayText || displayText[0] == '\0') {
        displayText = (*inp).placeholder;
        isPlace = true;
    }
    float padX = 8.0f;
    float innerW = w - padX * 2.0f;
    if (innerW < 10.0f)
        innerW = 10.0f;
    char maskBuf[256];
    if (displayText && (*inp).password && !isPlace) {
        size_t dlen = strlen(displayText);
        if (dlen > sizeof(maskBuf) - 1)
            dlen = sizeof(maskBuf) - 1;
        memset(maskBuf, '*', dlen);
        maskBuf[dlen] = '\0';
        displayText = maskBuf;
    }
    if (!displayText || displayText[0] == '\0')
        return false;
    if (!ensureInputRaster(inp, displayText, isPlace, innerW))
        return false;
    if ((*inp).rasterTex < 0 || (*inp).rasterW <= 0 || (*inp).rasterH <= 0)
        return false;
    float backing = (*inp).rasterBacking > 0.0f ? (*inp).rasterBacking : 1.0f;
    float qw = (float) (*inp).rasterW / backing;
    float qh = (float) (*inp).rasterH / backing;
    float qx = x + padX;
    float qy = y + (h - qh) * 0.5f;
    Vk_drawTexture(cmdBuffer, surfaceW, surfaceH, qx, qy, qw, qh, 1.0f, 1.0f, 1.0f, op,
                   (*inp).rasterTex, PICTURE_MODE_FIT, (float) (*inp).rasterW, (float) (*inp).rasterH);
    return true;
}

// Stage 3: focused/idle stroke over content.
static bool inputPaintBorder(Panel *panel, void *renderer, void *cmdBuffer,
                             float surfaceW, float surfaceH,
                             float x, float y, float w, float h) {
    Input *inp = (Input*) panel;
    (void) renderer;
    if (!inp || !cmdBuffer)
        return false;
    if (w <= 0.0f || h <= 0.0f)
        return false;
    Component *c = &(*panel).component;
    float op = Component_getOpacity(c);
    if (op <= 0.0f)
        return false;
    uint32_t borderColor = (*inp).focused ? 0xFF3B82F6u : 0xFF3F3F46u;
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

// Stage 4: caret rect (or borrowed caret view, placed by the owner).
static bool inputPaintCaret(Panel *panel, void *renderer, void *cmdBuffer,
                            float surfaceW, float surfaceH,
                            float x, float y, float w, float h) {
    Input *inp = (Input*) panel;
    (void) renderer;
    (void) w;
    if (!inp || !cmdBuffer)
        return false;
    if (!(*inp).focused || !(*inp).caretShown)
        return false;
    Component *c = &(*panel).component;
    float op = Component_getOpacity(c);
    if (op <= 0.0f)
        return false;
    float padX = 8.0f;
    float caretX = padX;
    if ((*inp).glyphX && (*inp).cursor >= 0 && (*inp).cursor < (*inp).glyphN) {
        caretX = padX + (*inp).glyphX[(*inp).cursor];
    } else if ((*inp).measurer) {
        caretX = padX + (*inp).measurer((*inp).measureCtx, (*inp).cursor);
    } else {
        float charW = (*inp).fontSize > 0.0f ? (*inp).fontSize * 0.55f : 8.0f;
        caretX = padX + (float) (*inp).cursor * charW;
    }
    uint32_t cColor = (*inp).caretColor ? (*inp).caretColor : 0xFFFFFFFFu;
    float ca_r = ((cColor >> 16) & 0xFF) / 255.0f;
    float ca_g = ((cColor >> 8) & 0xFF) / 255.0f;
    float ca_b = (cColor & 0xFF) / 255.0f;
    float ca_a = ((cColor >> 24) & 0xFF) / 255.0f * op * (*inp).caretOpacity;
    if (ca_a <= 0.0f)
        return false;
    float caretH = (*inp).fontSize > 0.0f ? (*inp).fontSize * 1.2f : 16.0f;
    if (caretH > h - 4.0f)
        caretH = h - 4.0f;
    float caretY = y + (h - caretH) * 0.5f;
    Vk_fillRect(cmdBuffer, surfaceW, surfaceH, x + caretX, caretY, 1.5f, caretH, ca_r, ca_g, ca_b, ca_a);
    return true;
}

Input *Input_0(void) {
    Input *inp = (Input*) Memory_alloc(TYPE_INPUT_SINGLETON, sizeof(Input));
    if (!inp)
        return nullptr;
    Panel *bp = Panel_0();
    if (!bp) {
        Memory_free(inp);
        return nullptr;
    }
    (*inp).base = (*bp);
    Memory_free(bp);
    (*inp).text = nullptr;
    (*inp).cap = INPUT_DEFAULT_CAP;
    (*inp).placeholder = nullptr;
    (*inp).password = false;
    (*inp).readonly = false;
    (*inp).focused = false;
    (*inp).cursor = 0;
    (*inp).font = nullptr;

    (*inp).fontSize = 13.0f;
    (*inp).textColor = 0xFFFFFFFFu;
    (*inp).placeholderColor = 0xFF888888u;
    (*inp).align = TEXT_ALIGN_LEFT;
    (*inp).spacingWidth = 0.0f;
    (*inp).ligatures = true;
    (*inp).select = TextSelect_default();
    (*inp).selectionColor = 0x662563EBu;
    (*inp).rasterTex = -1;
    (*inp).rasterW = 0;
    (*inp).rasterH = 0;
    (*inp).rasterBacking = 1.0f;
    (*inp).rasterDirty = true;
    (*inp).glyphX = nullptr;
    (*inp).glyphN = 0;

    (*inp).caretMode = INPUT_CARET_BLINK;
    (*inp).caretColor = 0xFFFFFFFFu;
    (*inp).caretBlinkPeriod = INPUT_CARET_DEFAULT_PERIOD;
    (*inp).caretClock = 0.0;
    (*inp).caretShown = true;
    (*inp).caretX = 0.0f;
    (*inp).caretTargetX = 0.0f;
    (*inp).caretView = nullptr;
    (*inp).caretOpacity = 1.0f;
    (*inp).onChange = nullptr;
    (*inp).onSubmit = nullptr;
    (*inp).ctx = nullptr;
    (*inp).measurer = nullptr;
    (*inp).measureCtx = nullptr;
    Panel *ip = &(*inp).base;
    Panel_setRenderHandler(ip, nullptr);
    Panel_setBackgroundFn(ip, inputPaintBackground);
    Panel_setTextFn(ip, inputPaintText);
    Panel_setBorderFn(ip, inputPaintBorder);
    Panel_setForegroundFn(ip, inputPaintCaret);
    return inp;
}

Input *Input_2(Panel *parent, size_t cap) {
    Input *inp = Input_0();
    if (!inp)
        return nullptr;
    (*inp).cap = cap;
    if (parent) {
        Panel *bp = &(*inp).base;
        Panel_addContainer(parent, bp);
    }
    return inp;
}

// ============================================================================
// CORE FUNCTIONS
// ============================================================================

static int32_t clampCursor(size_t len, int32_t cursor);
static void markDirty(Input *inp);

void Input_insertChar(Input *inp, char c) {
    if (!inp) return;
    if ((*inp).readonly) return; // reject edits, still selectable
    char *cur = (*inp).text;
    size_t len = cur ? strlen(cur) : 0;
    if (len >= (*inp).cap) return; // cap-truncate like setText: byte does not fit
    int32_t at = clampCursor(len, (*inp).cursor);
    size_t nlen = len + 1;
    char *buf = (char*) Memory_alloc(TYPE_ARRAY, nlen + 1);
    if (!buf) return;
    if (cur && at > 0) memcpy(buf, cur, (size_t)at);
    buf[at] = c;
    if (cur && (size_t)at < len) memcpy(buf + at + 1, cur + at, len - (size_t)at);
    buf[nlen] = '\0';
    if (cur) Memory_free(cur);
    (*inp).text = buf;
    (*inp).cursor = at + 1;
    if ((*inp).measurer)
        Input_caret_setTarget(inp, (*inp).measurer((*inp).measureCtx, (*inp).cursor));
    (*inp).caretClock = 0.0; // typing restarts the blink phase shown
    (*inp).caretShown = true;
    markDirty(inp);
    Input_ChangeFn fn = (*inp).onChange;
    void *ctx = (*inp).ctx;
    if (fn) fn(ctx);
}

void Input_eraseChar(Input *inp) {
    if (!inp) return;
    if ((*inp).readonly) return; // reject edits, still selectable
    char *cur = (*inp).text;
    size_t len = cur ? strlen(cur) : 0;
    int32_t at = clampCursor(len, (*inp).cursor);
    if (at <= 0) return; // nothing before the caret: no change, no fire
    size_t nlen = len - 1;
    char *buf = (char*) Memory_alloc(TYPE_ARRAY, nlen + 1);
    if (!buf) return;
    if (at > 1) memcpy(buf, cur, (size_t)(at - 1));
    if ((size_t)at < len) memcpy(buf + at - 1, cur + at, len - (size_t)at);
    buf[nlen] = '\0';
    Memory_free(cur);
    (*inp).text = buf;
    (*inp).cursor = at - 1;
    if ((*inp).measurer)
        Input_caret_setTarget(inp, (*inp).measurer((*inp).measureCtx, (*inp).cursor));
    (*inp).caretClock = 0.0; // typing restarts the blink phase shown
    (*inp).caretShown = true;
    markDirty(inp);
    Input_ChangeFn fn = (*inp).onChange;
    void *ctx = (*inp).ctx;
    if (fn) fn(ctx);
}

// Pointer handling: DOWN anchors selection & places caret, DRAG expands, UP commits.
void Input_handlePointer(Input *self, int kind, float localX, float localY) {
    if (!self) return;
    (void) localY;
    char *cur = (*self).text;
    size_t len = cur ? strlen(cur) : 0;
    float padX = 8.0f;
    float innerX = localX - padX;

    int32_t best = 0;
    if ((*self).glyphX && (*self).glyphN > 0) {
        float minD = 1e9f;
        for (int32_t i = 0; i < (*self).glyphN; i++) {
            float d = fabsf((*self).glyphX[i] - innerX);
            if (d < minD) {
                minD = d;
                best = i;
            }
        }
    } else if ((*self).measurer) {
        best = 0;
        float bestD = fabsf((*self).measurer((*self).measureCtx, 0) - innerX);
        for (int32_t i = 1; (size_t)i <= len; i++) {
            float d = fabsf((*self).measurer((*self).measureCtx, i) - innerX);
            if (d < bestD) {
                bestD = d;
                best = i;
            }
        }
    } else {
        best = (int32_t) len;
    }

    if (kind == PTR_DOWN) {
        (*self).focused = true;
        TextSelect_begin(&(*self).select, best);
        Input_goTo(self, best);
        markRasterDirty(self);
        return;
    }
    if (kind == PTR_DRAG) {
        if (!(*self).focused) return;
        TextSelect_drag(&(*self).select, best);
        Input_goTo(self, best);
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

// Pressed keys: Cmd+A/C/X/V, Left/Right (with Shift for range), Backspace, Enter, typing
void Input_handleKey(Input *self, const UIKeyEvent *ev) {
    if (!self || !ev) return;
    if (!UIKeyEvent_isPressed(ev)) return;
    int32_t code = UIKeyEvent_getKeyCode(ev);
    int32_t ch = UIKeyEvent_getCh(ev);
    uint32_t mods = UIKeyEvent_getMods(ev);
    bool cmdOrCtrl = (mods & (8u | 2u)) != 0;
    bool shift = (mods & 1u) != 0;

    // Cmd/Ctrl combinations
    if (cmdOrCtrl) {
        char *cur = (*self).text;
        size_t len = cur ? strlen(cur) : 0;
        if (code == KEY_A) {
            TextSelect_begin(&(*self).select, 0);
            TextSelect_drag(&(*self).select, (int32_t) len);
            Input_goTo(self, (int32_t) len);
            markRasterDirty(self);
            markDirty(self);
            return;
        }
        if (code == KEY_C) {
            char *sel = Input_getSelectedText(self);
            if (sel) {
                TextCore_copyToClipboard(sel);
                Memory_free(sel);
            }
            return;
        }
        if (code == KEY_X) {
            if (!(*self).readonly) {
                char *sel = Input_getSelectedText(self);
                if (sel) {
                    TextCore_copyToClipboard(sel);
                    Memory_free(sel);
                    Input_setSelectedText(self, "");
                }
            }
            return;
        }
        if (code == KEY_V) {
            if (!(*self).readonly) {
                char *clip = TextCore_pasteFromClipboard();
                if (clip) {
                    Input_setSelectedText(self, clip);
                    free(clip);
                }
            }
            return;
        }
    }

    if (code == KEY_BACKSPACE || ch == 8) {
        int32_t s0 = -1, s1 = -1;
        if (TextSelect_getSpan(&(*self).select, &s0, &s1) && s0 != s1) {
            Input_setSelectedText(self, "");
        } else {
            Input_eraseChar(self);
        }
        return;
    }
    if (code == KEY_ENTER || ch == '\r' || ch == '\n') {
        Input_SubmitFn fn = (*self).onSubmit;
        void *ctx = (*self).ctx;
        if (fn) fn(ctx);
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
        Input_goTo(self, next);
        markRasterDirty(self);
        markDirty(self);
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
        Input_goTo(self, next);
        markRasterDirty(self);
        markDirty(self);
        return;
    }
    if (ch >= 32) {
        int32_t s0 = -1, s1 = -1;
        if (TextSelect_getSpan(&(*self).select, &s0, &s1) && s0 != s1) {
            char ins[2] = {(char)ch, '\0'};
            Input_setSelectedText(self, ins);
        } else {
            Input_insertChar(self, (char)ch);
        }
    }
}

// ============================================================================
// SETTERS
// ============================================================================

static void markDirty(Input *inp) {
    if (!inp)
        return;
    Panel *bp = &(*inp).base;
    (void) bp;
}

static int32_t clampCursor(size_t len, int32_t cursor) {
    int32_t n = len > (size_t)INT32_MAX ? INT32_MAX : (int32_t)len;
    if (cursor < 0)
        return 0;
    if (cursor > n)
        return n;
    return cursor;
}

void Input_setText(Input *inp, const char *text) {
    if (!inp)
        return;
    char *old = (*inp).text;
    if (old) {
        Memory_free(old);
        (*inp).text = nullptr;
    }
    if (text) {
        size_t len = strlen(text);
        size_t cap = (*inp).cap;
        if (len > cap)
            len = cap;
        char *buf = (char*) Memory_alloc(TYPE_ARRAY, len + 1);
        if (buf) {
            memcpy(buf, text, len);
            buf[len] = '\0';
        }
        (*inp).text = buf;
    }
    char *cur = (*inp).text;
    size_t curLen = cur ? strlen(cur) : 0;
    (*inp).cursor = clampCursor(curLen, (*inp).cursor);
    if ((*inp).measurer)
        Input_caret_setTarget(inp, (*inp).measurer((*inp).measureCtx, (*inp).cursor));
    (*inp).caretClock = 0.0; // typing restarts the blink phase shown
    (*inp).caretShown = true;
    markDirty(inp);
}

void Input_setCap(Input *inp, size_t cap) {
    if (!inp)
        return;
    (*inp).cap = cap;
    char *cur = (*inp).text;
    if (cur && strlen(cur) > cap)
        Input_setText(inp, cur);
    else
        markDirty(inp);
}

void Input_setPlaceholder(Input *inp, const char *placeholder) {
    if (!inp)
        return;
    char *old = (*inp).placeholder;
    if (old) {
        Memory_free(old);
        (*inp).placeholder = nullptr;
    }
    if (placeholder) {
        size_t len = strlen(placeholder) + 1;
        char *buf = (char*) Memory_alloc(TYPE_ARRAY, len);
        if (buf)
            memcpy(buf, placeholder, len);
        (*inp).placeholder = buf;
    }
    markDirty(inp);
}

void Input_setPassword(Input *inp, bool password) {
    if (!inp)
        return;
    (*inp).password = password;
    markDirty(inp);
}

void Input_setReadonly(Input *inp, bool readonly) {
    if (!inp)
        return;
    (*inp).readonly = readonly;
    markDirty(inp);
}

void Input_setFocused(Input *inp, bool focused) {
    if (!inp)
        return;
    (*inp).focused = focused;
    markDirty(inp);
}

void Input_setCursor(Input *inp, int32_t cursor) {
    Input_goTo(inp, cursor); // moving to an index IS going to it
}

// The Word-inspired goTo: every index move re-measures the caret target
// and either blits (BLINK/SOLID) or glides (GLIDE) to it.
void Input_goTo(Input *inp, int32_t index) {
    if (!inp)
        return;
    char *cur = (*inp).text;
    size_t len = cur ? strlen(cur) : 0;
    (*inp).cursor = clampCursor(len, index);
    if ((*inp).measurer)
        Input_caret_setTarget(inp, (*inp).measurer((*inp).measureCtx, (*inp).cursor));
    (*inp).caretClock = 0.0; // arriving restarts the blink phase shown
    (*inp).caretShown = true;
    markDirty(inp);
}

void Input_setFont(Input *inp, Font *font) {
    if (!inp)
        return;
    (*inp).font = font;
    markDirty(inp);
}

void Input_setOnChange(Input *inp, Input_ChangeFn fn) {
    if (!inp)
        return;
    (*inp).onChange = fn;
}

void Input_setOnSubmit(Input *inp, Input_SubmitFn fn) {
    if (!inp)
        return;
    (*inp).onSubmit = fn;
}

void Input_setCtx(Input *inp, void *ctx) {
    if (!inp)
        return;
    (*inp).ctx = ctx;
}

void Input_setMeasurer(Input *inp, Input_MeasureFn fn, void *ctx) {
    if (!inp)
        return;
    (*inp).measurer = fn;
    (*inp).measureCtx = ctx;
}

// ============================================================================
// CARET PART
// ============================================================================

void Input_caret_setMode(Input *inp, int mode) {
    if (!inp)
        return;
    if (mode != INPUT_CARET_BLINK && mode != INPUT_CARET_SOLID && mode != INPUT_CARET_GLIDE)
        return;
    (*inp).caretMode = mode;
    if (mode != INPUT_CARET_BLINK) {
        (*inp).caretShown = true; // SOLID/GLIDE never blink away
        if (mode == INPUT_CARET_SOLID)
            (*inp).caretX = (*inp).caretTargetX;
    }
    markDirty(inp);
}

void Input_caret_setColor(Input *inp, uint32_t color) {
    if (!inp)
        return;
    (*inp).caretColor = color;
    markDirty(inp);
}

void Input_caret_setBlinkPeriod(Input *inp, float seconds) {
    if (!inp || seconds <= 0.0f)
        return;
    (*inp).caretBlinkPeriod = seconds;
    markDirty(inp);
}

void Input_caret_setTarget(Input *inp, float x) {
    if (!inp)
        return;
    (*inp).caretTargetX = x;
    if ((*inp).caretMode != INPUT_CARET_GLIDE)
        (*inp).caretX = x;
    markDirty(inp);
}

void Input_caret_setView(Input *inp, Panel *view) {
    if (!inp)
        return;
    // Borrowed view, detach-only: never freed, never reparented here.
    // Null restores the default thin rect painted by the pump.
    (*inp).caretView = view;
    markDirty(inp);
}

void Input_caret_setOpacity(Input *inp, float opacity) {
    if (!inp)
        return;
    if (opacity < 0.0f)
        opacity = 0.0f;
    if (opacity > 1.0f)
        opacity = 1.0f;
    (*inp).caretOpacity = opacity;
    markDirty(inp);
}

void Input_caret_placeView(Input *inp, Panel *view, float centerY) {
    if (!inp || !view)
        return;
    // Centered, of course: the view's middle lands on (caretX, centerY).
    Component *c = &(*view).component;
    float w = Component_getWidth(c);
    float h = Component_getHeight(c);
    Component_setLocation(c, (*inp).caretX - w * 0.5f, centerY - h * 0.5f);
}

void Input_caret_tick(Input *inp, double dt) {
    // Depth-1 proving element (depth-1 collage doctrine): the caret stays
    // painted inside the Input child's own retained flight target at local
    // coords (no cross-loop geometry — the board pass only collages the
    // child's published frame). A blink phase change marks ONLY the Input
    // base) — never the board, never the tree — so a blink re-renders a tiny
    // target at ~2Hz and Loop1 re-collages. No path from here reaches
    // Panel_markTreeDirty or board dirty; board demand arms one hop through
    // Darling_propagatePaneDirty (child publish -> board dirty).
    if (!inp || dt <= 0.0)
        return;
    if ((*inp).caretMode == INPUT_CARET_BLINK) {
        (*inp).caretClock += dt;
        float period = (*inp).caretBlinkPeriod;
        if (period <= 0.0f)
            period = INPUT_CARET_DEFAULT_PERIOD;
        float phase = fmodf((float)(*inp).caretClock, period * 2.0f);
        bool shown = phase < period; // float-exact: == period hides
        if (shown != (*inp).caretShown) {
            (*inp).caretShown = shown; // dirty ONLY on flip: rest is silence
            markDirty(inp);
        }
    } else if ((*inp).caretMode == INPUT_CARET_GLIDE) {
        float k = (float)(dt / (double)INPUT_CARET_GLIDE_TIME);
        if (k > 1.0f)
            k = 1.0f;
        float e = Anim_eval(ANIM_EASE_OUT, k);
        float next = (*inp).caretX + ((*inp).caretTargetX - (*inp).caretX) * e;
        if (next != (*inp).caretX) {
            (*inp).caretX = next;
            markDirty(inp);
        }
    }
}

void Input_free(Input *inp) {
    if (!inp)
        return;
    char *text = (*inp).text;
    if (text)
        Memory_free(text);
    char *holder = (*inp).placeholder;
    if (holder)
        Memory_free(holder);
    if ((*inp).rasterTex >= 0) {
        Texture_free((*inp).rasterTex);
        (*inp).rasterTex = -1;
    }
    if ((*inp).glyphX) {
        Memory_free((*inp).glyphX);
        (*inp).glyphX = nullptr;
    }
    (*inp).text = nullptr;
    (*inp).placeholder = nullptr;
    (*inp).font = nullptr;
    (*inp).onChange = nullptr;
    (*inp).onSubmit = nullptr;
    (*inp).ctx = nullptr;
    (*inp).measurer = nullptr;
    (*inp).measureCtx = nullptr;
    Memory_free(inp);
}

// ============================================================================
// GETTERS
// ============================================================================

const char *Input_getText(const Input *inp) {
    return inp ? (*inp).text : nullptr;
}

size_t Input_getCap(const Input *inp) {
    return inp ? (*inp).cap : 0;
}

const char *Input_getPlaceholder(const Input *inp) {
    return inp ? (*inp).placeholder : nullptr;
}

bool Input_isPassword(const Input *inp) {
    return inp ? (*inp).password : false;
}

bool Input_isReadonly(const Input *inp) {
    return inp ? (*inp).readonly : false;
}

bool Input_isFocused(const Input *inp) {
    return inp ? (*inp).focused : false;
}

int32_t Input_getCursor(const Input *inp) {
    return inp ? (*inp).cursor : 0;
}

Font *Input_getFont(const Input *inp) {
    return inp ? (*inp).font : nullptr;
}

Input_ChangeFn Input_getOnChange(const Input *inp) {
    return inp ? (*inp).onChange : nullptr;
}

Input_SubmitFn Input_getOnSubmit(const Input *inp) {
    return inp ? (*inp).onSubmit : nullptr;
}

void *Input_getCtx(const Input *inp) {
    return inp ? (*inp).ctx : nullptr;
}

Input_MeasureFn Input_getMeasurer(const Input *inp) {
    return inp ? (*inp).measurer : nullptr;
}

void *Input_getMeasureContext(const Input *inp) {
    return inp ? (*inp).measureCtx : nullptr;
}

int Input_caret_getMode(const Input *inp) {
    return inp ? (*inp).caretMode : INPUT_CARET_BLINK;
}

uint32_t Input_caret_getColor(const Input *inp) {
    return inp ? (*inp).caretColor : 0xFFFFFFFFu;
}

float Input_caret_getBlinkPeriod(const Input *inp) {
    return inp ? (*inp).caretBlinkPeriod : INPUT_CARET_DEFAULT_PERIOD;
}

float Input_caret_getTarget(const Input *inp) {
    return inp ? (*inp).caretTargetX : 0.0f;
}

float Input_caret_getX(const Input *inp) {
    return inp ? (*inp).caretX : 0.0f;
}

bool Input_caret_isShown(const Input *inp) {
    return inp && (*inp).caretShown;
}

Panel *Input_caret_getView(const Input *inp) {
    return inp ? (*inp).caretView : nullptr;
}

float Input_caret_getOpacity(const Input *inp) {
    return inp ? (*inp).caretOpacity : 1.0f;
}

float Input_caret_getEffectiveOpacity(const Input *inp) {
    if (!inp || !(*inp).caretShown)
        return 0.0f;
    return (*inp).caretOpacity;
}

char *Input_getSelectedText(const Input *inp) {
    if (!inp || !(*inp).text)
        return nullptr;
    int32_t s0 = -1, s1 = -1;
    if (!TextSelect_getSpan(&(*inp).select, &s0, &s1) || s0 == s1)
        return nullptr;
    int32_t len = (int32_t) strlen((*inp).text);
    if (s0 < 0) s0 = 0;
    if (s1 > len) s1 = len;
    if (s1 <= s0) return nullptr;
    int32_t subLen = s1 - s0;
    char *res = (char*) Memory_alloc(TYPE_ARRAY, (size_t)(subLen + 1));
    if (!res) return nullptr;
    memcpy(res, (*inp).text + s0, (size_t) subLen);
    res[subLen] = '\0';
    return res;
}

void Input_setSelectedText(Input *inp, const char *newText) {
    if (!inp || !newText)
        return;
    if ((*inp).readonly)
        return;
    const char *orig = (*inp).text ? (*inp).text : "";
    int32_t origLen = (int32_t) strlen(orig);
    int32_t s0 = -1, s1 = -1;
    (void) TextSelect_getSpan(&(*inp).select, &s0, &s1);
    if (s0 < 0 || s1 < 0 || s0 == s1) {
        s0 = (*inp).cursor;
        s1 = (*inp).cursor;
    }
    if (s0 < 0) s0 = 0;
    if (s0 > origLen) s0 = origLen;
    if (s1 < 0) s1 = 0;
    if (s1 > origLen) s1 = origLen;

    size_t insLen = strlen(newText);
    size_t cap = (*inp).cap;
    size_t finalLen = (size_t)s0 + insLen + (size_t)(origLen - s1);
    if (finalLen > cap) {
        if ((size_t)s0 + (size_t)(origLen - s1) >= cap) {
            insLen = 0;
        } else {
            insLen = cap - (size_t)s0 - (size_t)(origLen - s1);
        }
        finalLen = (size_t)s0 + insLen + (size_t)(origLen - s1);
    }

    char *buf = (char*) Memory_alloc(TYPE_ARRAY, finalLen + 1);
    if (!buf) return;
    if (s0 > 0) memcpy(buf, orig, (size_t) s0);
    if (insLen > 0) memcpy(buf + s0, newText, insLen);
    if (origLen - s1 > 0) memcpy(buf + s0 + insLen, orig + s1, (size_t)(origLen - s1));
    buf[finalLen] = '\0';

    if ((*inp).text) Memory_free((*inp).text);
    (*inp).text = buf;
    (*inp).cursor = s0 + (int32_t) insLen;
    TextSelect_reset(&(*inp).select);
    markRasterDirty(inp);
    markDirty(inp);
    if ((*inp).onChange) (*inp).onChange((*inp).ctx);
}

void Input_setFontSize(Input *inp, float size) {
    if (!inp || (*inp).fontSize == size) return;
    (*inp).fontSize = size;
    markRasterDirty(inp);
    markDirty(inp);
}

float Input_getFontSize(const Input *inp) {
    return inp ? (*inp).fontSize : 13.0f;
}

void Input_setTextColor(Input *inp, uint32_t color) {
    if (!inp || (*inp).textColor == color) return;
    (*inp).textColor = color;
    markRasterDirty(inp);
    markDirty(inp);
}

uint32_t Input_getTextColor(const Input *inp) {
    return inp ? (*inp).textColor : 0xFFFFFFFFu;
}

void Input_setPlaceholderColor(Input *inp, uint32_t color) {
    if (!inp || (*inp).placeholderColor == color) return;
    (*inp).placeholderColor = color;
    markRasterDirty(inp);
    markDirty(inp);
}

uint32_t Input_getPlaceholderColor(const Input *inp) {
    return inp ? (*inp).placeholderColor : 0xFF888888u;
}

void Input_setTextAlign(Input *inp, TextAlign align) {
    if (!inp || (*inp).align == align) return;
    (*inp).align = align;
    markRasterDirty(inp);
    markDirty(inp);
}

TextAlign Input_getTextAlign(const Input *inp) {
    return inp ? (*inp).align : TEXT_ALIGN_LEFT;
}

void Input_setSpacingWidth(Input *inp, float width) {
    if (!inp || (*inp).spacingWidth == width) return;
    (*inp).spacingWidth = width;
    markRasterDirty(inp);
    markDirty(inp);
}

float Input_getSpacingWidth(const Input *inp) {
    return inp ? (*inp).spacingWidth : 0.0f;
}

void Input_setLigatures(Input *inp, bool ligatures) {
    if (!inp || (*inp).ligatures == ligatures) return;
    (*inp).ligatures = ligatures;
    markRasterDirty(inp);
    markDirty(inp);
}

bool Input_hasLigatures(const Input *inp) {
    return inp ? (*inp).ligatures : true;
}

void Input_setSelection(Input *inp, int32_t start, int32_t end) {
    if (!inp) return;
    if (start < 0 || end < 0 || start == end) {
        TextSelect_reset(&(*inp).select);
    } else {
        TextSelect_begin(&(*inp).select, start);
        TextSelect_drag(&(*inp).select, end);
    }
    markRasterDirty(inp);
    markDirty(inp);
}

void Input_getSelection(const Input *inp, int32_t *outStart, int32_t *outEnd) {
    int32_t s0 = -1, s1 = -1;
    if (inp) (void) TextSelect_getSpan(&(*inp).select, &s0, &s1);
    if (outStart) *outStart = s0;
    if (outEnd) *outEnd = s1;
}

void Input_setSelectionColor(Input *inp, uint32_t color) {
    if (!inp || (*inp).selectionColor == color) return;
    (*inp).selectionColor = color;
    markRasterDirty(inp);
    markDirty(inp);
}

uint32_t Input_getSelectionColor(const Input *inp) {
    return inp ? (*inp).selectionColor : 0x662563EBu;
}
