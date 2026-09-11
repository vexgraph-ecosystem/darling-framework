#include "annotation/overview.h"
#include "label.h"
#include "vulkan/vk.h"
#include "nio/mem.h"
#include "oop/type.h"
#include "text/text_core.h"
#include "vulkan/texture/texture.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
 *   int32_t selectionStart;  // Fixed selection anchor (-1 = none); ordered via getSelection
 *   int32_t selectionEnd;    // Active drag edge (-1 = none); getters/raster order the pair
 *   float highlightRadius;   // Corner radius in points for selection rounded rect (default 3.0f)
 *   uint32_t highlightColor; // Packed 0xAARRGGBB selection background color (default 0x662563EB)
 *   bool hovered;            // True if pointer is currently hovering within label bounds
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
 *   - Label_renderFn(panel, rend, cmd, surfaceW, surfaceH, x, y, w, h) : Draw handler
 *   - Label_charIndexAt(const label, localX)                           : Character offset from point
 *   - Label_handlePointer(label, kind, localX, localY, window)         : Pointer event dispatcher
 *   - Label_onPointer(label, ev, window)                               : PointerEvent wrapper
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

static void markDirty(Label *lbl) {
    if (!lbl)
        return;
    Panel *p = &(*lbl).base;
    Container_markDirty(&(*p).base);
}

int32_t Label_charIndexAt(const Label *label, float localX) {
    if (!label || !(*label).text)
        return 0;
    size_t len = strlen((*label).text);
    if (len == 0)
        return 0;
    const Panel *p = &(*label).base;
    const Container *c = &(*p).base;
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
    Container *c = &(*p).base;
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

    if (kind == PTR_LEAVE || (!inside && (kind == PTR_MOVE || kind == PTR_HOVER))) {
        if ((*label).hovered) {
            (*label).hovered = false;
            if (window) {
                Cursor *defCursor = Cursor_getPredefined(CURSOR_DEFAULT);
                Cursor_apply(defCursor, window);
            }
            markDirty(label);
        }
        return;
    }

    if (inside && (kind == PTR_ENTER || kind == PTR_MOVE || kind == PTR_HOVER)) {
        if (!(*label).hovered) {
            (*label).hovered = true;
            if ((*label).highlightable && window)
                Cursor_apply((*label).cursor, window);
            markDirty(label);
        } else if ((*label).highlightable && window) {
            Cursor_apply((*label).cursor, window);
        }
    }

    if ((*label).highlightable) {
        if (kind == PTR_DOWN) {
            if (!inside) {
                if ((*label).selectionStart != -1 || (*label).selectionEnd != -1) {
                    (*label).selectionStart = -1;
                    (*label).selectionEnd = -1;
                    markRasterDirty(label);
                    markDirty(label);
                }
                return;
            }
            // Down = fixed anchor + collapsed selection. Drag then moves only the
            // active edge, so dragging left (backward) then right past the anchor
            // selects exactly [anchor, active] — never a rolling union.
            int32_t idx = Label_charIndexAt(label, localX);
            (*label).selectionStart = idx;
            (*label).selectionEnd = idx;
            markRasterDirty(label);
            markDirty(label);
        } else if (kind == PTR_DRAG) {
            if ((*label).selectionStart < 0)
                return;
            int32_t idx = Label_charIndexAt(label, localX);
            (*label).selectionEnd = idx;
            markRasterDirty(label);
            markDirty(label);
        } else if (kind == PTR_UP) {
            if ((*label).selectionStart < 0)
                return;
            if ((*label).selectionStart > (*label).selectionEnd) {
                int32_t tmp = (*label).selectionStart;
                (*label).selectionStart = (*label).selectionEnd;
                (*label).selectionEnd = tmp;
            }
            if ((*label).selectionStart == (*label).selectionEnd) {
                (*label).selectionStart = -1;
                (*label).selectionEnd = -1;
            }
            markRasterDirty(label);
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

static bool ensureRaster(Label *lbl) {
    if (!lbl)
        return false;
    if (!(*lbl).rasterDirty)
        return (*lbl).rasterTex >= 0;
    (*lbl).rasterDirty = false;
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

    if ((*lbl).mnemonic && (*lbl).text) {
        size_t srcLen = strlen((*lbl).text);
        size_t dst = 0;
        for (size_t i = 0; i < srcLen && dst + 1 < sizeof(cleanText); i++) {
            if ((*lbl).text[i] == '&') {
                if (i + 1 < srcLen && (*lbl).text[i + 1] == '&') {
                    cleanText[dst++] = '&';
                    i++;
                } else if (i + 1 < srcLen && mIndex < 0) {
                    mChar = (*lbl).text[i + 1];
                    mIndex = (int) dst;
                } else {
                    cleanText[dst++] = (*lbl).text[i];
                }
            } else {
                cleanText[dst++] = (*lbl).text[i];
            }
        }
        cleanText[dst] = '\0';
        srcText = cleanText;
    }
    (*lbl).mnemonicChar = mChar;
    (*lbl).mnemonicIndex = mIndex;

    int32_t selStart = (*lbl).highlightable ? (*lbl).selectionStart : -1;
    int32_t selEnd = (*lbl).highlightable ? (*lbl).selectionEnd : -1;
    if (selStart >= 0 && selEnd >= 0 && selStart > selEnd) {
        int32_t tmp = selStart;
        selStart = selEnd;
        selEnd = tmp;
    }

    TextStyleDescriptor style = {
        .ligatures = (*lbl).ligatures,
        .spacingWidth = (*lbl).spacingWidth,
        .spacingHeight = (*lbl).spacingHeight,
        .underline = (*lbl).underline,
        .underlineColor = (*lbl).underlineColor,
        .mnemonicIndex = mIndex,
        .selectionStart = selStart,
        .selectionEnd = selEnd,
        .highlightRadius = (*lbl).highlightRadius,
        .highlightColor = (*lbl).highlightColor,
    };

    uint8_t *rgba = nullptr;
    int w = 0;
    int h = 0;
    if (!TextCore_rasterStyled(srcText, family, pxH, (*lbl).textColor, &style, &rgba, &w, &h))
        return false;
    if (!rgba || w <= 0 || h <= 0)
        return false;
    int32_t tex = -1;
    if ((*lbl).rasterTex >= 0) {
        tex = Texture_replaceRaw((*lbl).rasterTex, rgba, (uint32_t) w, (uint32_t) h);
    } else {
        tex = Texture_loadRaw(rgba, (uint32_t) w, (uint32_t) h);
    }
    free(rgba);
    if (tex < 0)
        return false;
    (*lbl).rasterTex = tex;
    (*lbl).rasterW = w;
    (*lbl).rasterH = h;
    (*lbl).rasterBacking = backing;
    return true;
}

static void drawSdfFallback(Panel *panel, void *cmdBuffer, float surfaceW, float surfaceH,
                            float x, float y, float w, float h) {
    Label *lbl = (Label*) panel;
    float op = Container_getOpacity(&(*panel).base);
    if (op <= 0.0f)
        return;
    uint32_t bgColor = Panel_getBackgroundColor(panel);
    if ((bgColor >> 24) > 0) {
        float br = ((bgColor >> 16) & 0xFF) / 255.0f;
        float bg = ((bgColor >> 8) & 0xFF) / 255.0f;
        float bb = (bgColor & 0xFF) / 255.0f;
        float ba = ((bgColor >> 24) & 0xFF) / 255.0f * op;
        if (ba > 0.0f)
            Vk_fillRect(cmdBuffer, surfaceW, surfaceH, x, y, w, h, br, bg, bb, ba);
    }
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
                if (gm.color)
                    Vk_drawColorGlyph(cmdBuffer, surfaceW, surfaceH, qx, qy, gm.width, gm.height, ca, texId, gm.u0, gm.v0, gm.u1, gm.v1);
                else
                    Vk_drawSDFText(cmdBuffer, surfaceW, surfaceH, qx, qy, gm.width, gm.height, cr, cg, cb, ca, texId, 0.0f, (*lbl).smoothness, gm.u0, gm.v0, gm.u1, gm.v1);
            }
            cx += gm.advance;
        }
        i += charLen;
    }
}

static void Label_renderFn(Panel *panel, void *renderer, void *cmdBuffer, float surfaceW, float surfaceH,
                           float x, float y, float w, float h) {
    Label *lbl = (Label*) panel;
    (void) renderer;
    float op = panel ? Container_getOpacity(&(*panel).base) : 1.0f;
    if (op <= 0.0f)
        return;
    uint32_t bgColor = Panel_getBackgroundColor(panel);
    if ((bgColor >> 24) > 0) {
        float br = ((bgColor >> 16) & 0xFF) / 255.0f;
        float bgg = ((bgColor >> 8) & 0xFF) / 255.0f;
        float bb = (bgColor & 0xFF) / 255.0f;
        float ba = ((bgColor >> 24) & 0xFF) / 255.0f * op;
        if (ba > 0.0f)
            Vk_fillRect(cmdBuffer, surfaceW, surfaceH, x, y, w, h, br, bgg, bb, ba);
    }
    if (!lbl || !(*lbl).text || (*lbl).text[0] == '\0' || (*lbl).fontSize <= 0.0f)
        return;
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
        Container *container = &(*basePanel).base;
        if (w <= (*container).w * 1.25f && backing > 1.0f) {
            qw /= backing;
            qh /= backing;
        }
        float qx = x;
        float qy = y + h - qh;

        Vk_drawTexture(cmdBuffer, surfaceW, surfaceH, qx, qy, qw, qh, 1.0f, 1.0f, 1.0f, op,
            (*lbl).rasterTex, PICTURE_MODE_FIT, (float) (*lbl).rasterW, (float) (*lbl).rasterH);
        return;
    }
    drawSdfFallback(panel, cmdBuffer, surfaceW, surfaceH, x, y, w, h);
}

// ============================================================================
// CONSTRUCTORS
// ============================================================================

Label *Label_0(void) {
    Label *lbl = (Label*) Memory_alloc(TYPE_PANEL_SINGLETON, sizeof(Label));
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
    (*lbl).highlightable = false;
    (*lbl).mnemonic = false;
    (*lbl).mnemonicChar = '\0';
    (*lbl).mnemonicIndex = -1;
    (*lbl).ligatures = true;
    (*lbl).spacingWidth = 0.0f;
    (*lbl).spacingHeight = 0.0f;
    (*lbl).underline = UNDERLINE_NONE;
    (*lbl).underlineColor = 0;
    (*lbl).cursor = Cursor_getPredefined(CURSOR_DEFAULT);
    (*lbl).selectionStart = -1;
    (*lbl).selectionEnd = -1;
    (*lbl).highlightRadius = 3.0f;
    (*lbl).highlightColor = 0x662563EBu;
    (*lbl).hovered = false;
    {
        const char *defFamily = "Helvetica";
        size_t defLen = strlen(defFamily) + 1;
        (*lbl).fontFamily = (char*) Memory_alloc(TYPE_ARRAY, defLen);
        if ((*lbl).fontFamily) {
            strcpy((*lbl).fontFamily, defFamily);
            (*lbl).ownsFontFamily = true;
        }
    }
    Panel_setRenderHandler(&(*lbl).base, Label_renderFn);
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
        (*label).selectionStart = -1;
        (*label).selectionEnd = -1;
        (*label).hovered = false;
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
    (*label).selectionStart = start;
    (*label).selectionEnd = end;
    markRasterDirty(label);
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

void Label_setHovered(Label *label, bool hovered) {
    if (!label)
        return;
    (*label).hovered = hovered;
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
    if (!label) {
        (*outStart) = -1;
        (*outEnd) = -1;
        return;
    }
    int32_t s0 = (*label).selectionStart;
    int32_t s1 = (*label).selectionEnd;
    if (s0 >= 0 && s1 >= 0 && s0 > s1) {
        int32_t tmp = s0;
        s0 = s1;
        s1 = tmp;
    }
    (*outStart) = s0;
    (*outEnd) = s1;
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
    return label ? (*label).hovered : false;
}

char *Label_getSelectedText(const Label *label) {
    if (!label || !(*label).text)
        return nullptr;
    int32_t s0 = (*label).selectionStart;
    int32_t s1 = (*label).selectionEnd;
    if (s0 < 0 || s1 < 0)
        return nullptr;
    if (s0 > s1) {
        int32_t tmp = s0;
        s0 = s1;
        s1 = tmp;
    }
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
    int32_t s0 = (*label).selectionStart;
    int32_t s1 = (*label).selectionEnd;
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
    (*label).selectionStart = -1;
    (*label).selectionEnd = -1;
    markRasterDirty(label);
    markDirty(label);
}
