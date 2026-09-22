#include "annotation/definition.h"
#include "annotation/overview.h"
#include "label.h"
#include "vulkan/vk.h"
#include "nio/mem.h"
#include "oop/type.h"
#include "text/text_core.h"
#include "vulkan/texture/texture.h"
#include "input/key.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Label
 * ============================================================================
 * Lightweight retained-mode UI text view for sharp, single-styled typography,
 * inheriting Panel -> Container. Label renders through a dual path — native
 * CoreText line rasterization into a textured quad (sharp) with a multi-pass
 * SDF fallback — and keeps selection coordinates per glyph: the same CoreText
 * shaper that paints supplies one pen offset per UTF-8 byte, so proportional
 * type maps 1:1 to the highlight overlay. The raster is text-only stable:
 * selection edits markDirty only and the render handler paints the live span
 * per frame over the stable quad, never re-rastering. Text and fontFamily are
 * copied into the arena when owned (ownsText/ownsFontFamily); glyphX is a
 * per-byte pen-offset table with a uniform fallback when absent. Sibling
 * RichLabel keeps the SDF atlas path for styled runs.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Label (inherits Panel -> Container)
 * LEVEL: L2 — Behavior (UI text view behavior API)
 * ============================================================================
 * Lightweight retained-mode UI text view for sharp, single-styled typography.
 * Implements a dual-path rendering strategy:
 *   1. Sharp Path : Native CoreText line rasterization into a textured quad.
 *   2. SDF Path   : Multi-pass signed distance field fallback rendering.
 *
 * Selection coordinates are stated per glyph, not estimated: ensureRaster
 * asks the same CoreText shaper that paints for one pen offset per UTF-8
 * byte (glyphX, label space) and the pointer hit-test shares that table
 * with the highlight overlay, so proportional type maps 1:1. Absent table
 * (multiline, stub platform, raster failure) falls back to uniform.
 * Mnemonic `&` stripping is index-mapped both ways: the raster paints in
 * clean coordinates while TextSelect stays in label coordinates.
 * The raster is text-only stable: selection is never baked in. Selection
 * edits (setSelection, pointer drag) markDirty only; the render handler
 * paints the live span per frame as one Vk_fillRect over the stable quad,
 * mapped through glyphX (uniform fallback when absent). No new submit,
 * no resize, no re-raster on selection change.
 *
 * STRUCT FIELDS (Mirroring darling/label/label.h):
 * ----------------------------------------------------------------------------
 *   Panel base;              // Inherited layout, bounds, and hierarchy state
 *   char *text;              // UTF-8 string payload
 *   Font *font;              // Optional SDF font atlas descriptor
 *   char *fontFamily;        // CoreText typeface family name (e.g. "Helvetica")
 *   float fontSize;          // Font height in points
 *   uint32_t textColor;      // Packed 0xAARRGGBB color value
 *   float smoothness;        // SDF edge anti-aliasing sharpness factor
 *   int32_t rasterTex;       // GPU texture ID of CoreText cached raster quad
 *   int rasterW;             // Pixel width of CoreText raster
 *   int rasterH;             // Pixel height of CoreText raster
 *   float rasterBacking;     // Retina scale factor at rasterization time
 *   bool rasterDirty;        // True if string or font changed and needs re-raster
 *   float *glyphX;           // Per-byte CoreText pen offsets in points (strlen+1), NULL = uniform fallback
 *   int32_t glyphN;          // Entry count of glyphX (0 when absent)
 *   bool ownsText;           // True if text was copied and owned by label
 *   bool ownsFontFamily;     // True if fontFamily was copied and owned by label
 *   bool highlightable;      // Enables text selection drag (no caret; labels aren't editable)
 *   bool mnemonic;           // Parse '&' key accelerator prefix
 *   char mnemonicChar;       // Parsed accelerator character ('\0' if none)
 *   int mnemonicIndex;       // Index in display text (-1 if none)
 *   bool ligatures;          // Enable standard typography ligatures (default true)
 *   float spacingWidth;      // Letter tracking/kerning delta in points (default 0.0)
 *   float spacingHeight;     // Line leading delta in points (default 0.0)
 *   UnderlineStyle underline;// UNDERLINE_NONE, UNDERLINE_BASIC, etc.
 *   uint32_t underlineColor; // Packed 0xAARRGGBB (0 = inherit textColor)
 *   Cursor *cursor;          // Active mouse cursor style (I-beam when highlightable)
 *   TextSelect select;       // Shared selection part (anchor/active edge + hover lifecycle)
 *   float highlightRadius;   // Corner radius in points for selection rounded rect (default 3.0f)
 *   uint32_t highlightColor; // Packed 0xAARRGGBB selection background color (default 0x662563EB)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Label()                                : Label_0()
 *   - Label(text)                            : Label_1(text)
 *   - Label(parent, text)                    : Label_2(parent, text)
 *   - Label_1_parent(parent)
 *
 * Core Functions:
 *   - labelPaintText(panel, rend, cmd, ...) : Stage 2 sharp raster / SDF glyphs
 *     (registered via Panel_setTextFn in Label_0; background stays Panel default)
 *   - labelPaintHighlight(panel, rend, cmd, ...) : Stage 4 selection overlay
 *     (registered via Panel_setForegroundFn in Label_0; sharp path only)
 *   - Label_charIndexAt(const label, localX)                           : Byte offset from point (per-glyph table, uniform fallback)
 *   - Label_handlePointer(label, kind, localX, localY, window)         : Pointer event dispatcher
 *   - Label_onPointer(label, ev, window)                               : PointerEvent wrapper
 *   - Label_handleKey(label, ev)                                       : Key event (Cmd+C copy)
 *
 * Setters:
 *   - Label_setText(label, text)
 *   - Label_setTextBorrowed(label, text)
 *   - Label_setFont(label, font)
 *   - Label_setFontFamily(label, family)
 *   - Label_setFontFamilyBorrowed(label, family)
 *   - Label_setFontSize(label, size)
 *   - Label_setTextColor(label, color)
 *   - Label_setSmoothness(label, smoothness)
 *   - Label_setLocation(label, x, y)
 *   - Label_setSize(label, w, h)
 *   - Label_setBackgroundColor(label, color)
 *   - Label_setHighlightable(label, flag)
 *   - Label_setMnemonic(label, flag)
 *   - Label_setLigatures(label, flag)
 *   - Label_setSpacingWidth(label, width)
 *   - Label_setSpacingHeight(label, height)
 *   - Label_setSpacing(label, width, height)
 *   - Label_setUnderline(label, style)
 *   - Label_setUnderlineColor(label, color)
 *   - Label_setUnderlineColorRGBA(label, r, g, b, a)
 *   - Label_setCursor(label, cursor)
 *   - Label_setSelection(label, start, end)
 *   - Label_setHighlightRadius(label, radius)
 *   - Label_setHighlightColor(label, color)
 *   - Label_setHighlightColorRGBA(label, r, g, b, a)
 *   - Label_setHovered(label, hovered)
 *   - Label_free(label)
 *
 * Getters:
 *   - Label_getText(const label)
 *   - Label_getFont(const label)
 *   - Label_getFontFamily(const label)
 *   - Label_getFontSize(const label)
 *   - Label_getTextColor(const label)
 *   - Label_getSmoothness(const label)
 *   - Label_getRasterTexture(const label)
 *   - Label_getRasterSize(const label, outW, outH)
 *   - Label_getRasterBacking(const label)
 *   - Label_isRasterDirty(const label)
 *   - Label_getGlyphOffsets(const label)
 *   - Label_getGlyphOffsetCount(const label)
 *   - Label_isHighlightable(const label)
 *   - Label_isMnemonic(const label)
 *   - Label_getMnemonicChar(const label)
 *   - Label_getMnemonicIndex(const label)
 *   - Label_hasLigatures(const label)
 *   - Label_getSpacingWidth(const label)
 *   - Label_getSpacingHeight(const label)
 *   - Label_getSpacing(const label, outWidth, outHeight)
 *   - Label_getUnderline(const label)
 *   - Label_getUnderlineColor(const label)
 *   - Label_getUnderlineColorRGBA(const label, outR, outG, outB, outA)
 *   - Label_getCursor(const label)
 *   - Label_getSelection(const label, outStart, outEnd)
 *   - Label_getHighlightRadius(const label)
 *   - Label_getHighlightColor(const label)
 *   - Label_getHighlightColorRGBA(const label, outR, outG, outB, outA)
 *   - Label_isHovered(const label)
 * ============================================================================
 */

// ============================================================================
// CORE FUNCTIONS
// ============================================================================

static void markRasterDirty(Label *lbl) {
    if (!lbl)
        return;
    (*lbl).rasterDirty = true;
}

// Drops the stated per-glyph positions. Called on every raster rebuild entry
// (before repopulating) and on free, so a failed rebuild never leaves a
// stale table from a previous text behind.
static void clearGlyphTable(Label *lbl) {
    if (!lbl)
        return;
    if ((*lbl).glyphX)
        Memory_free((*lbl).glyphX);
    (*lbl).glyphX = nullptr;
    (*lbl).glyphN = 0;
}

static void markDirty(Label *lbl) {
    if (!lbl)
        return;
    Panel *p = &(*lbl).base;
    (void) p;
}

int32_t Label_charIndexAt(const Label *label, float localX) {
    if (!label || !(*label).text)
        return 0;
    size_t len = strlen((*label).text);
    if (len == 0)
        return 0;
    // Stated positions (per-glyph table): the hit-test shares the exact
    // CoreText pen offsets the raster paints, so proportional type maps 1:1.
    // The boundary between glyph i and i+1 sits at their midpoint
    // (hemisphere); equal neighbors (continuation bytes, stripped markers)
    // rewind to the codepoint start so a span never opens mid-codepoint.
    // The raster carries a 1px left pad, folded in here in points.
    const float *gx = (*label).glyphX;
    if (gx && (*label).glyphN == (int32_t) len + 1) {
        float backing = (*label).rasterBacking > 0.0f ? (*label).rasterBacking : 1.0f;
        float pad = 1.0f / backing;
        if (localX <= gx[0] + pad)
            return 0;
        if (localX >= gx[len] + pad)
            return (int32_t) len;
        int32_t lo = 0, hi = (int32_t) len;
        while (lo < hi) {
            int32_t m = lo + (hi - lo) / 2;
            float b = (gx[m] + gx[m + 1]) * 0.5f + pad;
            if (b <= localX)
                lo = m + 1;
            else
                hi = m;
        }
        while (lo > 0 && gx[lo] == gx[lo - 1])
            lo--;
        return lo;
    }
    const Panel *p = &(*label).base;
    const Component *c = &(*p).component;
    float qw = (*c).w;
    if ((*label).rasterW > 0) {
        float backing = (*label).rasterBacking > 0.0f ? (*label).rasterBacking : 1.0f;
        qw = (float) (*label).rasterW / backing;
    }
    if (qw <= 0.0f)
        qw = (float) len * ((*label).fontSize * 0.5f);
    if (localX <= 0.0f)
        return 0;
    if (localX >= qw)
        return (int32_t) len;
    // Hemisphere rule (uniform fallback only: no stated table because the
    // text is multiline, the platform stub has no shaper, or the raster
    // failed). A pointer on the left half of a glyph (advance / 2, stable >=
    // split) selects that glyph; the right half selects the next. For a
    // uniform advance qw/len the boundary sits at the glyph midpoint, so
    // roundf(ratio * len) is exactly the hemisphere mapping.
    float ratio = localX / qw;
    int32_t idx = (int32_t) roundf(ratio * (float) len);
    if (idx < 0)
        idx = 0;
    if (idx > (int32_t) len)
        idx = (int32_t) len;
    return idx;
}

void Label_handlePointer(Label *label, int kind, float localX, float localY, void *window) {
    if (!label)
        return;
    Panel *p = &(*label).base;
    Component *c = &(*p).component;
    float w = (*c).w;
    float h = (*c).h;
    if (w <= 0.0f && (*label).rasterW > 0) {
        float backing = (*label).rasterBacking > 0.0f ? (*label).rasterBacking : 1.0f;
        w = (float) (*label).rasterW / backing;
    }
    if (h <= 0.0f && (*label).rasterH > 0) {
        float backing = (*label).rasterBacking > 0.0f ? (*label).rasterBacking : 1.0f;
        h = (float) (*label).rasterH / backing;
    }

    bool inside = (localX >= 0.0f && localX <= w && localY >= 0.0f && localY <= h);

    // Shared hover caret-cursor lifecycle (TextSelect part): ENTER/LEAVE/MOVE
    // flip the hovered flag and drive I-beam / default cursor. Hovering never
    // touches anchor/active and LEAVE never clears an in-progress selection.
    if (kind == PTR_LEAVE || (!inside && (kind == PTR_MOVE || kind == PTR_HOVER))) {
        if (TextSelect_setHovered(&(*label).select, false)) {
            if (window) {
                Cursor *defCursor = Cursor_getPredefined(CURSOR_DEFAULT);
                Cursor_apply(defCursor, window);
            }
            markDirty(label);
        }
        return;
    }

    if (inside && (kind == PTR_ENTER || kind == PTR_MOVE || kind == PTR_HOVER)) {
        if (TextSelect_setHovered(&(*label).select, true)) {
            if ((*label).highlightable && window)
                Cursor_apply((*label).cursor, window);
            markDirty(label);
        } else if ((*label).highlightable && window) {
            Cursor_apply((*label).cursor, window);
        }
    }

    if ((*label).highlightable) {
        if (kind == PTR_DOWN) {
            // Outside-down clears any in-progress selection.
            if (!inside) {
                if (TextSelect_isActive(&(*label).select)) {
                    TextSelect_cancel(&(*label).select);
                    markDirty(label);
                }
                return;
            }
            // Down = fixed anchor + collapsed selection. Drag then moves only the
            // active edge, so dragging left (backward) then right past the anchor
            // selects exactly [anchor, active] — never a rolling union.
            // Overlay-only: the stable raster is untouched, markDirty repaints.
            int32_t idx = Label_charIndexAt(label, localX);
            TextSelect_begin(&(*label).select, idx);
            markDirty(label);
        } else if (kind == PTR_DRAG) {
            if (TextSelect_drag(&(*label).select, Label_charIndexAt(label, localX))) {
                markDirty(label);
            }
        } else if (kind == PTR_UP) {
            int32_t lo = -1, hi = -1;
            TextSelect_end(&(*label).select, &lo, &hi);
            markDirty(label);
        }
    }
}

void Label_onPointer(Label *label, PointerEvent *ev, void *window) {
    if (!label || !ev)
        return;
    int kind = PointerEvent_getKind(ev);
    float x = PointerEvent_getX(ev);
    float y = PointerEvent_getY(ev);
    Label_handlePointer(label, kind, x, y, window);
}

// Key seam: Cmd/Ctrl+C copies the committed selection; V is a no-op on
// read-only labels. Detection is by keyCode + modifier bits (cmd=8, ctrl=2
// local bridge bits), never by the decoded character.
void Label_handleKey(Label *label, const UIKeyEvent *ev) {
    if (!label || !ev)
        return;
    if (!UIKeyEvent_isPressed(ev))
        return;
    if (UIKeyEvent_isRepeat(ev))
        return;
    uint32_t mods = UIKeyEvent_getMods(ev);
    if ((mods & (8u | 2u)) == 0)
        return;
    if (!(*label).highlightable)
        return;
    int32_t code = UIKeyEvent_getKeyCode(ev);
    if (code != KEY_C && code != KEY_V)
        return;
    if (code == KEY_C) {
        char *sel = Label_getSelectedText(label);
        if (sel) {
            TextCore_copyToClipboard(sel);
            Memory_free(sel);
            UIKeyEvent_consume((UIKeyEvent*) ev);
        }
    }
}

static bool ensureRaster(Label *lbl) {
    if (!lbl)
        return false;
    if (!(*lbl).rasterDirty)
        return (*lbl).rasterTex >= 0;
    (*lbl).rasterDirty = false;
    clearGlyphTable(lbl);
    if (!(*lbl).text || (*lbl).text[0] == '\0' || (*lbl).fontSize <= 0.0f) {
        (*lbl).rasterTex = -1;
        return false;
    }
    float backing = TextCore_backingScale();
    if (backing <= 0.0f)
        backing = 1.0f;
    float pxH = (*lbl).fontSize * backing;
    if (pxH <= 0.0f)
        return false;
    const char *family = (*lbl).fontFamily ? (*lbl).fontFamily : "Helvetica";

    const char *srcText = (*lbl).text;
    char cleanText[512];
    int mIndex = -1;
    char mChar = '\0';

    // Label-to-clean index map (mnemonic `&` stripping shifts everything
    // after the marker). toClean[i] is the clean coordinate of original byte
    // i; skipMark[i] flags the consumed marker byte itself, which hit-testing
    // folds onto the previous glyph (zero-width phantom, never addressable).
    size_t srcLen = strlen((*lbl).text);
    int32_t *toClean = nullptr;
    uint8_t *skipMark = nullptr;
    if ((*lbl).mnemonic && (*lbl).text) {
        toClean = (int32_t*) Memory_alloc(TYPE_ARRAY, (srcLen + 1) * sizeof(int32_t));
        skipMark = (uint8_t*) Memory_alloc(TYPE_ARRAY, srcLen + 1);
        if (!toClean || !skipMark) {
            if (toClean)
                Memory_free(toClean);
            if (skipMark)
                Memory_free(skipMark);
            toClean = nullptr;
            skipMark = nullptr;
        } else {
            for (size_t z = 0; z <= srcLen; z++)
                skipMark[z] = 0;
        }
    }

    if ((*lbl).mnemonic && (*lbl).text) {
        size_t dst = 0;
        for (size_t i = 0; i < srcLen && dst + 1 < sizeof(cleanText); i++) {
            if (toClean)
                toClean[i] = (int32_t) dst;
            if ((*lbl).text[i] == '&') {
                if (i + 1 < srcLen && (*lbl).text[i + 1] == '&') {
                    cleanText[dst++] = '&';
                    if (toClean)
                        toClean[i + 1] = (int32_t) dst - 1;
                    i++;
                } else if (i + 1 < srcLen && mIndex < 0) {
                    mChar = (*lbl).text[i + 1];
                    mIndex = (int) dst;
                    if (skipMark)
                        skipMark[i] = 1;
                } else {
                    cleanText[dst++] = (*lbl).text[i];
                }
            } else {
                cleanText[dst++] = (*lbl).text[i];
            }
        }
        if (toClean)
            toClean[srcLen] = (int32_t) dst;
        cleanText[dst] = '\0';
        srcText = cleanText;
    }
    (*lbl).mnemonicChar = mChar;
    (*lbl).mnemonicIndex = mIndex;

    // Text-only stability: selection is never baked into the raster. The
    // render handler paints the live span per frame as a Vk_fillRect overlay
    // mapped through glyphX, so span edits never re-rasterize.
    int32_t cleanLen = (int32_t) strlen(srcText);
    int32_t selStartClean = -1;
    int32_t selEndClean = -1;

    // Stated positions from the same shaper that paints (single line only,
    // bounded by the strip buffer). Installed below iff the raster succeeds,
    // so the table can never disagree with the pixels on screen.
    float cleanOff[512];
    int32_t offCount = -1;
    if (cleanLen + 1 <= 512)
        offCount = TextCore_lineOffsets(srcText, family, pxH, (*lbl).ligatures,
                                        (*lbl).spacingWidth, cleanOff, 512);

    Panel *rasterPanel = &(*lbl).base;
    Component *rasterBox = &(*rasterPanel).component;
    float boundsW = (*rasterBox).w;
    TextStyleDescriptor style = {
        .ligatures = (*lbl).ligatures,
        .spacingWidth = (*lbl).spacingWidth,
        .spacingHeight = (*lbl).spacingHeight,
        .underline = (*lbl).underline,
        .underlineColor = (*lbl).underlineColor,
        .mnemonicIndex = mIndex,
        .selectionStart = selStartClean,
        .selectionEnd = selEndClean,
        .highlightRadius = (*lbl).highlightRadius,
        .highlightColor = (*lbl).highlightColor,
        .align = (*lbl).textAlign,
        .boundsWidth = boundsW,
    };

    uint8_t *rgba = nullptr;
    int w = 0;
    int h = 0;
    bool painted = TextCore_rasterStyled(srcText, family, pxH, (*lbl).textColor, &style, &rgba, &w, &h);
    if (!painted) {
        if (toClean)
            Memory_free(toClean);
        if (skipMark)
            Memory_free(skipMark);
        return false;
    }
    if (!rgba || w <= 0 || h <= 0) {
        if (toClean)
            Memory_free(toClean);
        if (skipMark)
            Memory_free(skipMark);
        return false;
    }
    int32_t tex = -1;
    if ((*lbl).rasterTex >= 0) {
        tex = Texture_replaceRaw((*lbl).rasterTex, rgba, (uint32_t) w, (uint32_t) h);
    } else {
        tex = Texture_loadRaw(rgba, (uint32_t) w, (uint32_t) h);
    }
    free(rgba);
    if (tex < 0) {
        if (toClean)
            Memory_free(toClean);
        if (skipMark)
            Memory_free(skipMark);
        return false;
    }
    (*lbl).rasterTex = tex;
    (*lbl).rasterW = w;
    (*lbl).rasterH = h;
    (*lbl).rasterBacking = backing;
    // Install the stated positions (label space): clean offsets mapped back
    // through the strip map; consumed markers fold onto the previous glyph.
    // Installed only here — beside the pixels they describe — so the table
    // can never disagree with what is on screen.
    if (offCount == cleanLen + 1) {
        float *gx = (float*) Memory_alloc(TYPE_ARRAY, (srcLen + 1) * sizeof(float));
        if (gx) {
            float totalAdvPts = cleanOff[cleanLen];
            float alignOffsetPts = 0.0f;
            if ((*lbl).textAlign == TEXT_ALIGN_CENTER && boundsW > totalAdvPts) {
                alignOffsetPts = (boundsW - totalAdvPts) * 0.5f;
            } else if ((*lbl).textAlign == TEXT_ALIGN_RIGHT && boundsW > totalAdvPts) {
                alignOffsetPts = boundsW - totalAdvPts;
            }
            for (size_t i = 0; i <= srcLen; i++) {
                int32_t c = toClean ? toClean[i] : (int32_t) i;
                if (c < 0)
                    c = 0;
                if (c > cleanLen)
                    c = cleanLen;
                gx[i] = cleanOff[c] + alignOffsetPts;
            }
            if (skipMark) {
                for (size_t i = 0; i <= srcLen; i++) {
                    if (skipMark[i] && i > 0)
                        gx[i] = gx[i - 1];
                }
            }
            (*lbl).glyphX = gx;
            (*lbl).glyphN = (int32_t)(srcLen + 1);
        }
    }
    if (toClean)
        Memory_free(toClean);
    if (skipMark)
        Memory_free(skipMark);
    return true;
}

static void drawSdfFallback(Panel *panel, void *cmdBuffer, float surfaceW, float surfaceH,
                            float x, float y, float w, float h) {
    Label *lbl = (Label*) panel;
    (void) w;
    (void) h;
    float op = GraphicsComponent_getOpacity(&(*panel).component);
    if (op <= 0.0f)
        return;
    // Background is stage 0 (Panel default) — never repainted here.
    if (!(*lbl).text || !(*lbl).font || (*lbl).fontSize <= 0)
        return;
    float cr = (((*lbl).textColor >> 16) & 0xFF) / 255.0f;
    float cg = (((*lbl).textColor >> 8) & 0xFF) / 255.0f;
    float cb = ((*lbl).textColor & 0xFF) / 255.0f;
    float ca = (((*lbl).textColor >> 24) & 0xFF) / 255.0f * op;
    float ascent = 0;
    float descent = 0;
    float lineGap = 0;
    Font_getVMetrics((*lbl).font, &ascent, &descent, &lineGap);
    float scale = Font_getScaleForPixelHeight((*lbl).font, (*lbl).fontSize);
    float lineHeight = (ascent - descent + lineGap) * scale;
    if (lineHeight <= 0.0f)
        lineHeight = (*lbl).fontSize * 1.2f;

    GlyphMetrics spaceGm = {0};
    float spaceAdvance = (*lbl).fontSize * 0.3f;
    if (Font_getGlyph((*lbl).font, (uint32_t) ' ', (*lbl).fontSize, &spaceGm) && spaceGm.advance > 0.0f)
        spaceAdvance = spaceGm.advance;
    float tabWidth = 4.0f * spaceAdvance;

    float cx = x;
    float cy = y + (ascent * scale);
    int32_t page0Tex = Font_getTextureId((*lbl).font);
    uint32_t prevChar = 0;
    int len = (int) strlen((*lbl).text);
    for (int i = 0; i < len; ) {
        uint32_t codepoint = 0;
        unsigned char c0 = (unsigned char) (*lbl).text[i];
        int charLen = 1;
        if (c0 < 0x80) {
            codepoint = c0;
        } else if ((c0 & 0xE0) == 0xC0) {
            if (i + 1 < len)
                codepoint = ((c0 & 0x1F) << 6) | ((*lbl).text[i + 1] & 0x3F);
            charLen = 2;
        } else if ((c0 & 0xF0) == 0xE0) {
            if (i + 2 < len)
                codepoint = ((c0 & 0x0F) << 12) | (((*lbl).text[i + 1] & 0x3F) << 6) | ((*lbl).text[i + 2] & 0x3F);
            charLen = 3;
        } else if ((c0 & 0xF8) == 0xF0) {
            if (i + 3 < len)
                codepoint = ((c0 & 0x07) << 18) | (((*lbl).text[i + 1] & 0x3F) << 12) | (((*lbl).text[i + 2] & 0x3F) << 6) | ((*lbl).text[i + 3] & 0x3F);
            charLen = 4;
        }
        if (codepoint == '\r') {
            prevChar = 0;
            i += charLen;
            continue;
        }
        if (codepoint == '\n') {
            cx = x;
            cy += lineHeight;
            prevChar = 0;
            i += charLen;
            continue;
        }
        if (codepoint == '\t') {
            if (tabWidth > 0.0f) {
                float relX = cx - x;
                cx = x + (floorf(relX / tabWidth) + 1.0f) * tabWidth;
            } else {
                cx += 4.0f * spaceAdvance;
            }
            prevChar = 0;
            i += charLen;
            continue;
        }
        GlyphMetrics gm = {0};
        if (Font_getGlyph((*lbl).font, codepoint, (*lbl).fontSize, &gm)) {
            if (prevChar != 0)
                cx += Font_getKerning((*lbl).font, prevChar, codepoint, (*lbl).fontSize);
            prevChar = codepoint;
            if (gm.width > 0.0f && gm.height > 0.0f) {
                float qx = cx + gm.xOffset;
                float qy = cy + gm.yOffset;
                // Multi-page atlas: each glyph draws from its own page.
                // Color glyphs (runtime emoji) take the raw-RGBA branch.
                int32_t texId = Font_pageTextureId((*lbl).font, (size_t)gm.page);
                if (texId < 0)
                    texId = page0Tex;
                // Rule 39: a page-less glyph with no page-0 fallback keeps a
                // negative id; skip the quad rather than sample OOB bindless.
                if (texId >= 0) {
                    if (gm.color)
                        Vk_drawColorGlyph(cmdBuffer, surfaceW, surfaceH, qx, qy, gm.width, gm.height, ca, texId, gm.u0, gm.v0, gm.u1, gm.v1);
                    else
                        Vk_drawSDFText(cmdBuffer, surfaceW, surfaceH, qx, qy, gm.width, gm.height, cr, cg, cb, ca, texId, 0.0f, (*lbl).smoothness, gm.u0, gm.v0, gm.u1, gm.v1);
                }
            }
            cx += gm.advance;
        }
        i += charLen;
    }
}

// Per-frame selection highlight over the stable text-only raster quad.
// Reads the live TextSelect span (label coordinates, ordered) and maps each
// edge through the stated glyphX table (label space, mnemonic-folded, 1px
// pad in points); an absent table falls back to uniform advance. Paints one
// Vk_fillRect like RichLabel drawSelectionSpans — no new submit, no resize,
// no raster work. Hot-minimal: entry guards + early returns, no log/alloc.
static void drawSelectionOverlay(Label *lbl, void *cmdBuffer, float surfaceW, float surfaceH,
                                 float qx, float qy, float qh, float op) {
    if (!lbl)
        return;
    if (!(*lbl).highlightable)
        return;
    int32_t s0 = -1, s1 = -1;
    if (!TextSelect_getSpan(&(*lbl).select, &s0, &s1))
        return;
    if (s1 <= s0)
        return;
    const char *text = (*lbl).text;
    if (!text)
        return;
    int32_t len = (int32_t) strlen(text);
    if (len <= 0)
        return;
    int32_t lo = s0 < 0 ? 0 : s0;
    int32_t hi = s1 > len ? len : s1;
    if (hi <= lo)
        return;
    uint32_t hl = (*lbl).highlightColor;
    float ba = ((hl >> 24) & 0xFF) / 255.0f * op;
    if (ba <= 0.0f)
        return;
    float br = ((hl >> 16) & 0xFF) / 255.0f;
    float bgc = ((hl >> 8) & 0xFF) / 255.0f;
    float bb = (hl & 0xFF) / 255.0f;
    float x0 = 0.0f, x1 = 0.0f;
    const float *gx = (*lbl).glyphX;
    if (gx && (*lbl).glyphN == len + 1) {
        float backing = (*lbl).rasterBacking > 0.0f ? (*lbl).rasterBacking : 1.0f;
        float pad = 1.0f / backing;
        x0 = gx[lo] + pad;
        x1 = gx[hi] + pad;
    } else {
        float qw = (float) len * ((*lbl).fontSize * 0.5f);
        if ((*lbl).rasterW > 0) {
            float backing = (*lbl).rasterBacking > 0.0f ? (*lbl).rasterBacking : 1.0f;
            qw = (float) (*lbl).rasterW / backing;
        }
        if (qw <= 0.0f)
            return;
        x0 = qw * (float) lo / (float) len;
        x1 = qw * (float) hi / (float) len;
    }
    if (x1 <= x0)
        return;
    Vk_fillRect(cmdBuffer, surfaceW, surfaceH, qx + x0, qy, x1 - x0, qh, br, bgc, bb, ba);
}

// Stage 2: sharp raster quad, else SDF per-glyph fallback.
// Background stays the Panel default (stage 0); selection is stage 4.
static bool labelPaintText(Panel *panel, void *renderer, void *cmdBuffer,
                           float surfaceW, float surfaceH,
                           float x, float y, float w, float h) {
    Label *lbl = (Label*) panel;
    (void) renderer;
    if (!lbl || !cmdBuffer)
        return false;
    float op = GraphicsComponent_getOpacity(&(*panel).component);
    if (op <= 0.0f)
        return false;
    if (!(*lbl).text || (*lbl).text[0] == '\0' || (*lbl).fontSize <= 0.0f)
        return false;
    // Sharp path: one native raster quad, top-left anchored in panel.
    // macOS panels are bottom-up (AppKit): panel origin is bottom-left,
    // so pin quad top to y + h - qh. Falls back to SDF per-glyph below.
    if (ensureRaster(lbl) && (*lbl).rasterTex >= 0 && (*lbl).rasterW > 0 && (*lbl).rasterH > 0) {
        float backing = (*lbl).rasterBacking;
        if (backing <= 0.0f)
            backing = 1.0f;
        float qw = (float) (*lbl).rasterW;
        float qh = (float) (*lbl).rasterH;
        Panel *basePanel = &(*lbl).base;
        Component *container = &(*basePanel).component;
        if (w <= (*container).w * 1.25f && backing > 1.0f) {
            qw /= backing;
            qh /= backing;
        }
        float qx = x;
        float qy = y + h - qh;
        Vk_drawTexture(cmdBuffer, surfaceW, surfaceH, qx, qy, qw, qh, 1.0f, 1.0f, 1.0f, op,
            (*lbl).rasterTex, PICTURE_MODE_FIT, (float) (*lbl).rasterW, (float) (*lbl).rasterH);
        return true;
    }
    drawSdfFallback(panel, cmdBuffer, surfaceW, surfaceH, x, y, w, h);
    return true;
}

// Stage 4: live selection overlay over the stable sharp quad.
// Sharp path only (matches the pre-split contract: SDF path paints none).
static bool labelPaintHighlight(Panel *panel, void *renderer, void *cmdBuffer,
                                float surfaceW, float surfaceH,
                                float x, float y, float w, float h) {
    Label *lbl = (Label*) panel;
    (void) renderer;
    (void) surfaceW;
    (void) surfaceH;
    (void) x;
    (void) y;
    (void) w;
    (void) h;
    if (!lbl || !cmdBuffer)
        return false;
    if (!(*lbl).highlightable)
        return false;
    if (!(*lbl).text || (*lbl).text[0] == '\0')
        return false;
    if ((*lbl).rasterTex < 0 || (*lbl).rasterW <= 0 || (*lbl).rasterH <= 0)
        return false;
    float op = GraphicsComponent_getOpacity(&(*panel).component);
    if (op <= 0.0f)
        return false;
    float backing = (*lbl).rasterBacking;
    if (backing <= 0.0f)
        backing = 1.0f;
    float qh = (float) (*lbl).rasterH;
    Panel *basePanel = &(*lbl).base;
    Component *container = &(*basePanel).component;
    if (w <= (*container).w * 1.25f && backing > 1.0f)
        qh /= backing;
    float qx = x;
    float qy = y + h - qh;
    drawSelectionOverlay(lbl, cmdBuffer, surfaceW, surfaceH, qx, qy, qh, op);
    return true;
}

// ============================================================================
// CONSTRUCTORS
// ============================================================================

Label *Label_0(void) {
    Label *lbl = (Label*) Memory_alloc(TYPE_LABEL_SINGLETON, sizeof(Label));
    if (!lbl)
        return NULL;
    Panel *p = Panel_0();
    if (!p) {
        Memory_free(lbl);
        return NULL;
    }
    (*lbl).base = *p;
    Memory_free(p);
    (*lbl).text = NULL;
    (*lbl).ownsText = false;
    (*lbl).font = NULL;
    (*lbl).fontFamily = nullptr;
    (*lbl).ownsFontFamily = false;
    (*lbl).fontSize = 12.0f;
    (*lbl).textColor = 0xFFFFFFFF;
    (*lbl).smoothness = 0.5f;
    (*lbl).rasterTex = -1;
    (*lbl).rasterW = 0;
    (*lbl).rasterH = 0;
    (*lbl).rasterBacking = 1.0f;
    (*lbl).rasterDirty = true;
    (*lbl).glyphX = nullptr;
    (*lbl).glyphN = 0;
    (*lbl).highlightable = false;
    (*lbl).mnemonic = false;
    (*lbl).mnemonicChar = '\0';
    (*lbl).mnemonicIndex = -1;
    (*lbl).ligatures = true;
    (*lbl).spacingWidth = 0.0f;
    (*lbl).spacingHeight = 0.0f;
    (*lbl).underline = UNDERLINE_NONE;
    (*lbl).underlineColor = 0;
    (*lbl).textAlign = TEXT_ALIGN_LEFT;
    (*lbl).cursor = Cursor_getPredefined(CURSOR_DEFAULT);
    (*lbl).select = TextSelect_default();
    (*lbl).highlightRadius = 3.0f;
    (*lbl).highlightColor = 0x662563EBu;
    {
        const char *defFamily = "Helvetica";
        size_t defLen = strlen(defFamily) + 1;
        (*lbl).fontFamily = (char*) Memory_alloc(TYPE_ARRAY, defLen);
        if ((*lbl).fontFamily) {
            strcpy((*lbl).fontFamily, defFamily);
            (*lbl).ownsFontFamily = true;
        }
    }
    Panel *lb = &(*lbl).base;
    Panel_setRenderHandler(lb, nullptr);
    Panel_setTextFn(lb, labelPaintText);
    Panel_setForegroundFn(lb, labelPaintHighlight);
    return lbl;
}

Label *Label_1(const char *text) {
    Label *lbl = Label_0();
    if (lbl)
        Label_setText(lbl, text);
    return lbl;
}

Label *Label_1_parent(Panel *parent) {
    Label *lbl = Label_0();
    if (lbl && parent)
        Panel_addContainer(parent, &(*lbl).base);
    return lbl;
}

Label *Label_2(Panel *parent, const char *text) {
    Label *lbl = Label_1(text);
    if (lbl && parent)
        Panel_addContainer(parent, &(*lbl).base);
    return lbl;
}

// ============================================================================
// SETTERS
// ============================================================================


void Label_setText(Label *label, const char *text) {
    if (!label)
        return;
    if ((*label).text && (*label).ownsText)
        Memory_free((*label).text);
    if (text) {
        size_t len = strlen(text) + 1;
        (*label).text = (char*) Memory_alloc(TYPE_ARRAY, len);
        if ((*label).text)
            memcpy((*label).text, text, len);
        (*label).ownsText = true;
    } else {
        (*label).text = NULL;
        (*label).ownsText = false;
    }
    markRasterDirty(label);
    markDirty(label);
}

void Label_setTextBorrowed(Label *label, const char *text) {
    if (!label)
        return;
    // Always-on lifetime guard: a transient string must not outlive the transient
    // arena.  Transient_contains is two pointer comparisons — essentially free
    // compared to the raster work that follows.  abort() here is intentional:
    // a dangling borrowed pointer is a silent use-after-free in the next frame.
    if (Transient_contains(text) && !Transient_contains(label)) {
        fprintf(stderr, "[LIFETIME ESCAPE] Label_setTextBorrowed: transient string %p cannot be borrowed by non-transient label %p\n",
                text, (void*) label);
        abort();
    }
    if ((*label).text && (*label).ownsText)
        Memory_free((*label).text);
    (*label).text = (char*) text;
    (*label).ownsText = false;
    markRasterDirty(label);
    markDirty(label);
}

void Label_setFont(Label *label, Font *font) {
    if (!label)
        return;
    (*label).font = font;
    markRasterDirty(label);
    markDirty(label);
}

void Label_setFontFamily(Label *label, const char *family) {
    if (!label)
        return;
    if ((*label).fontFamily && (*label).ownsFontFamily)
        Memory_free((*label).fontFamily);
    (*label).fontFamily = nullptr;
    (*label).ownsFontFamily = false;
    if (family) {
        size_t len = strlen(family) + 1;
        (*label).fontFamily = (char*) Memory_alloc(TYPE_ARRAY, len);
        if ((*label).fontFamily) {
            memcpy((*label).fontFamily, family, len);
            (*label).ownsFontFamily = true;
        }
    }
    markRasterDirty(label);
    markDirty(label);
}

void Label_setFontFamilyBorrowed(Label *label, const char *family) {
    if (!label)
        return;
    // Always-on lifetime guard — see Label_setTextBorrowed for rationale.
    if (Transient_contains(family) && !Transient_contains(label)) {
        fprintf(stderr, "[LIFETIME ESCAPE] Label_setFontFamilyBorrowed: transient family %p cannot be borrowed by non-transient label %p\n",
                family, (void*) label);
        abort();
    }
    if ((*label).fontFamily && (*label).ownsFontFamily)
        Memory_free((*label).fontFamily);
    (*label).fontFamily = (char*) family;
    (*label).ownsFontFamily = false;
    markRasterDirty(label);
    markDirty(label);
}

void Label_setFontSize(Label *label, float size) {
    if (!label)
        return;
    (*label).fontSize = size;
    markRasterDirty(label);
    markDirty(label);
}

void Label_setTextColor(Label *label, uint32_t color) {
    if (!label)
        return;
    (*label).textColor = color;
    markRasterDirty(label);
    markDirty(label);
}

void Label_setSmoothness(Label *label, float smoothness) {
    if (!label)
        return;
    if (smoothness < 0.0f)
        smoothness = 0.0f;
    if (smoothness > 1.0f)
        smoothness = 1.0f;
    (*label).smoothness = smoothness;
    markDirty(label);
}

void Label_setLocation(Label *label, float x, float y) {
    if (!label)
        return;
    Panel_setLocation(&(*label).base, x, y);
}

void Label_setSize(Label *label, float w, float h) {
    if (!label)
        return;
    Panel_setSize(&(*label).base, w, h);
    if ((*label).textAlign != TEXT_ALIGN_LEFT)
        markRasterDirty(label);
}

void Label_setBackgroundColor(Label *label, uint32_t color) {
    if (!label)
        return;
    Panel_setBackgroundColor(&(*label).base, color);
}

void Label_setHighlightable(Label *label, bool flag) {
    if (!label)
        return;
    (*label).highlightable = flag;
    if (flag) {
        (*label).cursor = Cursor_getPredefined(CURSOR_IBEAM);
    } else {
        (*label).cursor = Cursor_getPredefined(CURSOR_DEFAULT);
        TextSelect_reset(&(*label).select);
    }
    markRasterDirty(label);
    markDirty(label);
}

void Label_setMnemonic(Label *label, bool flag) {
    if (!label)
        return;
    (*label).mnemonic = flag;
    markRasterDirty(label);
    markDirty(label);
}

void Label_setLigatures(Label *label, bool flag) {
    if (!label)
        return;
    (*label).ligatures = flag;
    markRasterDirty(label);
    markDirty(label);
}

void Label_setSpacingWidth(Label *label, float width) {
    if (!label)
        return;
    (*label).spacingWidth = width;
    markRasterDirty(label);
    markDirty(label);
}

void Label_setSpacingHeight(Label *label, float height) {
    if (!label)
        return;
    (*label).spacingHeight = height;
    markRasterDirty(label);
    markDirty(label);
}

void Label_setSpacing(Label *label, float width, float height) {
    if (!label)
        return;
    (*label).spacingWidth = width;
    (*label).spacingHeight = height;
    markRasterDirty(label);
    markDirty(label);
}

void Label_setUnderline(Label *label, UnderlineStyle style) {
    if (!label)
        return;
    (*label).underline = style;
    markRasterDirty(label);
    markDirty(label);
}

void Label_setUnderlineColor(Label *label, uint32_t color) {
    if (!label)
        return;
    (*label).underlineColor = color;
    markRasterDirty(label);
    markDirty(label);
}

void Label_setUnderlineColorRGBA(Label *label, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!label)
        return;
    uint32_t packed = ((uint32_t) a << 24) | ((uint32_t) r << 16) | ((uint32_t) g << 8) | (uint32_t) b;
    Label_setUnderlineColor(label, packed);
}

void Label_setCursor(Label *label, Cursor *cursor) {
    if (!label)
        return;
    (*label).cursor = cursor;
}

void Label_setSelection(Label *label, int32_t start, int32_t end) {
    if (!label)
        return;
    if (start < 0 || end < 0) {
        TextSelect_reset(&(*label).select);
    } else {
        TextSelect_begin(&(*label).select, start);
        TextSelect_drag(&(*label).select, end);
    }
    markDirty(label);
}

void Label_setHighlightRadius(Label *label, float radius) {
    if (!label)
        return;
    if (radius < 0.0f)
        radius = 0.0f;
    (*label).highlightRadius = radius;
    markRasterDirty(label);
    markDirty(label);
}

void Label_setHighlightColor(Label *label, uint32_t color) {
    if (!label)
        return;
    (*label).highlightColor = color;
    markRasterDirty(label);
    markDirty(label);
}

void Label_setHighlightColorRGBA(Label *label, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!label)
        return;
    uint32_t packed = ((uint32_t) a << 24) | ((uint32_t) r << 16) | ((uint32_t) g << 8) | (uint32_t) b;
    Label_setHighlightColor(label, packed);
}

void Label_setTextAlign(Label *label, TextAlign align) {
    if (!label || (*label).textAlign == align)
        return;
    (*label).textAlign = align;
    markRasterDirty(label);
    markDirty(label);
}

void Label_setHovered(Label *label, bool hovered) {
    if (!label)
        return;
    TextSelect_setHovered(&(*label).select, hovered);
    markDirty(label);
}

void Label_free(Label *label) {
    if (!label)
        return;
    if ((*label).text && (*label).ownsText)
        Memory_free((*label).text);
    if ((*label).fontFamily && (*label).ownsFontFamily)
        Memory_free((*label).fontFamily);
    Cursor_free((*label).cursor);
    (*label).text = nullptr;
    (*label).fontFamily = nullptr;
    (*label).ownsText = false;
    (*label).ownsFontFamily = false;
    (*label).cursor = nullptr;
    if ((*label).rasterTex >= 0) {
        Texture_free((*label).rasterTex);
        (*label).rasterTex = -1;
    }
    clearGlyphTable(label);
    Memory_free(label);
}

// ============================================================================
// GETTERS
// ============================================================================

const char *Label_getText(const Label *label) {
    return label ? (*label).text : nullptr;
}

Font *Label_getFont(const Label *label) {
    return label ? (*label).font : nullptr;
}

const char *Label_getFontFamily(const Label *label) {
    return label ? (*label).fontFamily : nullptr;
}

float Label_getFontSize(const Label *label) {
    return label ? (*label).fontSize : 0.0f;
}

uint32_t Label_getTextColor(const Label *label) {
    return label ? (*label).textColor : 0;
}

float Label_getSmoothness(const Label *label) {
    return label ? (*label).smoothness : 0.0f;
}

int32_t Label_getRasterTexture(const Label *label) {
    return label ? (*label).rasterTex : -1;
}

void Label_getRasterSize(const Label *label, int *outW, int *outH) {
    if (outW) (*outW) = label ? (*label).rasterW : 0;
    if (outH) (*outH) = label ? (*label).rasterH : 0;
}

float Label_getRasterBacking(const Label *label) {
    return label ? (*label).rasterBacking : 1.0f;
}

bool Label_isRasterDirty(const Label *label) {
    return label ? (*label).rasterDirty : false;
}

const float *Label_getGlyphOffsets(const Label *label) {
    return label ? (*label).glyphX : nullptr;
}

int32_t Label_getGlyphOffsetCount(const Label *label) {
    return label ? (*label).glyphN : 0;
}

bool Label_isHighlightable(const Label *label) {
    return label ? (*label).highlightable : false;
}

bool Label_isMnemonic(const Label *label) {
    return label ? (*label).mnemonic : false;
}

char Label_getMnemonicChar(const Label *label) {
    return label ? (*label).mnemonicChar : '\0';
}

int Label_getMnemonicIndex(const Label *label) {
    return label ? (*label).mnemonicIndex : -1;
}

bool Label_hasLigatures(const Label *label) {
    return label ? (*label).ligatures : true;
}

float Label_getSpacingWidth(const Label *label) {
    return label ? (*label).spacingWidth : 0.0f;
}

float Label_getSpacingHeight(const Label *label) {
    return label ? (*label).spacingHeight : 0.0f;
}

void Label_getSpacing(const Label *label, float *outWidth, float *outHeight) {
    if (outWidth) (*outWidth) = label ? (*label).spacingWidth : 0.0f;
    if (outHeight) (*outHeight) = label ? (*label).spacingHeight : 0.0f;
}

UnderlineStyle Label_getUnderline(const Label *label) {
    return label ? (*label).underline : UNDERLINE_NONE;
}

uint32_t Label_getUnderlineColor(const Label *label) {
    return label ? (*label).underlineColor : 0;
}

void Label_getUnderlineColorRGBA(const Label *label, uint8_t *outR, uint8_t *outG, uint8_t *outB, uint8_t *outA) {
    uint32_t c = label ? (*label).underlineColor : 0;
    if (outA) (*outA) = (uint8_t) ((c >> 24) & 0xFF);
    if (outR) (*outR) = (uint8_t) ((c >> 16) & 0xFF);
    if (outG) (*outG) = (uint8_t) ((c >> 8) & 0xFF);
    if (outB) (*outB) = (uint8_t) (c & 0xFF);
}

Cursor *Label_getCursor(const Label *label) {
    return label ? (*label).cursor : nullptr;
}

void Label_getSelection(const Label *label, int32_t *outStart, int32_t *outEnd) {
    int32_t s0 = -1, s1 = -1;
    if (label)
        (void) TextSelect_getSpan(&(*label).select, &s0, &s1);
    if (outStart) (*outStart) = s0;
    if (outEnd) (*outEnd) = s1;
}

float Label_getHighlightRadius(const Label *label) {
    return label ? (*label).highlightRadius : 0.0f;
}

uint32_t Label_getHighlightColor(const Label *label) {
    return label ? (*label).highlightColor : 0;
}

void Label_getHighlightColorRGBA(const Label *label, uint8_t *outR, uint8_t *outG, uint8_t *outB, uint8_t *outA) {
    uint32_t c = label ? (*label).highlightColor : 0;
    if (outA) (*outA) = (uint8_t) ((c >> 24) & 0xFF);
    if (outR) (*outR) = (uint8_t) ((c >> 16) & 0xFF);
    if (outG) (*outG) = (uint8_t) ((c >> 8) & 0xFF);
    if (outB) (*outB) = (uint8_t) (c & 0xFF);
}

bool Label_isHovered(const Label *label) {
    return label ? TextSelect_isHovered(&(*label).select) : false;
}

TextAlign Label_getTextAlign(const Label *label) {
    return label ? (*label).textAlign : TEXT_ALIGN_LEFT;
}

char *Label_getSelectedText(const Label *label) {
    if (!label || !(*label).text)
        return nullptr;
    int32_t s0 = -1, s1 = -1;
    if (!TextSelect_getSpan(&(*label).select, &s0, &s1))
        return nullptr;
    int32_t len = (int32_t) strlen((*label).text);
    if (s0 < 0)
        s0 = 0;
    if (s1 > len)
        s1 = len;
    if (s1 <= s0)
        return nullptr;
    int32_t subLen = s1 - s0;
    char *res = (char*) Memory_alloc(TYPE_ARRAY, (size_t) (subLen + 1));
    if (!res)
        return nullptr;
    memcpy(res, (*label).text + s0, (size_t) subLen);
    res[subLen] = '\0';
    return res;
}

void Label_setSelectedText(Label *label, const char *newText) {
    if (!label || !newText)
        return;
    const char *orig = (*label).text ? (*label).text : "";
    int32_t origLen = (int32_t) strlen(orig);
    int32_t s0 = -1, s1 = -1;
    (void) TextSelect_getSpan(&(*label).select, &s0, &s1);
    if (s0 < 0 || s1 < 0) {
        s0 = origLen;
        s1 = origLen;
    }
    if (s0 > s1) {
        int32_t tmp = s0;
        s0 = s1;
        s1 = tmp;
    }
    if (s0 < 0)
        s0 = 0;
    if (s1 > origLen)
        s1 = origLen;

    int32_t insertLen = (int32_t) strlen(newText);
    int32_t newTotalLen = s0 + insertLen + (origLen - s1);
    char *buf = (char*) Memory_alloc(TYPE_ARRAY, (size_t) (newTotalLen + 1));
    if (!buf)
        return;
    if (s0 > 0)
        memcpy(buf, orig, (size_t) s0);
    if (insertLen > 0)
        memcpy(buf + s0, newText, (size_t) insertLen);
    if (origLen - s1 > 0)
        memcpy(buf + s0 + insertLen, orig + s1, (size_t) (origLen - s1));
    buf[newTotalLen] = '\0';

    Label_setText(label, buf);
    Memory_free(buf);
    TextSelect_reset(&(*label).select);
    markRasterDirty(label);
    markDirty(label);
}
