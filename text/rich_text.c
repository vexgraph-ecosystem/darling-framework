#include "nio/mem.h"
#include "oop/type.h"
#include "../c23/darling-type.h"
#include "rich_text.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "annotation/overview.h"

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: RichText (styled text layout model)
 * LEVEL: L2 — Behavior (rich text layout behavior API)
 * ============================================================================
 * Styled rich-text layout model: a raw UTF-8 string plus style dictionary,
 * run list, and laid-out glyph quads rendered through the SDF atlas path.
 *
 * STRUCT FIELDS (Mirroring text/rich_text.h):
 * ----------------------------------------------------------------------------
 *   RichText {             // Owned layout model (see text/rich_text.h)
 *     char *rawString;     // Owned UTF-8 source string
 *     TextStyle *styles;   // Style dictionary entries
 *     size_t styleCapacity; // Style dictionary capacity
 *     TextRun *runs;       // Shaped run list
 *     size_t runCount;     // Active run count
 *     size_t runCapacity;  // Run array capacity
 *     TextQuad *quads;     // Laid-out glyph quads for the renderer
 *     size_t quadCount;    // Active quad count
 *     size_t quadCapacity; // Quad array capacity
 *     float layoutWidth;   // Last laid-out width
 *     float layoutHeight;  // Last laid-out height
 *     WrapMode wrapMode;   // WRAP_NONE/WORD/CHAR line-wrap policy
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - RichText_new(void)
 *
 * Core Functions:
 *   - RichText_free(rt)
 *   - RichText_layout(rt, maxWidth)
 *
 * Setters:
 *   - RichText_setStyle(rt, id, font, size, color, bold, decor)
 *   - RichText_setShadow(rt, id, offsetX, offsetY, color)
 *   - RichText_setWrapMode(rt, mode)
 *   - RichText_setString(rt, str)
 * ============================================================================
 */


RichText *RichText_new(void) {
    RichText *rt = Memory_alloc(TYPE_RICHTEXT_SINGLETON, sizeof(RichText));
    if(rt) { memset(rt, 0, sizeof(RichText)); };
    (*rt).styleCapacity = 16;
    (*rt).styles = Memory_alloc(TYPE_ARRAY, (*rt).styleCapacity * sizeof(TextStyle));
    if((*rt).styles) { memset((*rt).styles, 0, (*rt).styleCapacity * sizeof(TextStyle)); };
    (*rt).wrapMode = WRAP_WORD;
    return rt;
}

void RichText_free(RichText *rt) {
    if (!rt) return;
    if ((*rt).rawString) Memory_free((*rt).rawString);
    if ((*rt).styles) Memory_free((*rt).styles);
    if ((*rt).runs) Memory_free((*rt).runs);
    if ((*rt).quads) Memory_free((*rt).quads);
    Memory_free(rt);
}

void RichText_setStyle(RichText *rt, int id, Font *font, float size, uint32_t color, bool bold, TextDecoration decor) {
    if (!rt || id < 0) return;
    if ((size_t)id >= (*rt).styleCapacity) {
        size_t newCap = (size_t)id + 16;
        TextStyle *newStyles = (TextStyle*) Memory_realloc((*rt).styles, newCap * sizeof(TextStyle));
        if (!newStyles) return;
        (*rt).styles = newStyles;
        memset((*rt).styles + (*rt).styleCapacity, 0, (newCap - (*rt).styleCapacity) * sizeof(TextStyle));
        (*rt).styleCapacity = newCap;
    }
    (*rt).styles[id].setFlags = STYLE_HAS_FONT | STYLE_HAS_SIZE | STYLE_HAS_COLOR | STYLE_HAS_BOLD | STYLE_HAS_DECOR;
    (*rt).styles[id].font = font;
    (*rt).styles[id].size = size;
    (*rt).styles[id].color = color;
    (*rt).styles[id].isBold = bold;
    (*rt).styles[id].decor = decor;
    (*rt).styles[id].decorColor = color; // Default to same color
}


void RichText_setShadow(RichText *rt, int id, float offsetX, float offsetY, uint32_t color) {
    if (!rt || id < 0 || (size_t)id >= (*rt).styleCapacity) return;
    (*rt).styles[id].setFlags |= STYLE_HAS_SHADOW;
    (*rt).styles[id].hasShadow = true;
    (*rt).styles[id].shadowX = offsetX;
    (*rt).styles[id].shadowY = offsetY;
    (*rt).styles[id].shadowColor = color;
}

void RichText_setWrapMode(RichText *rt, WrapMode mode) {
    if (rt) (*rt).wrapMode = mode;
}

// Helper to merge a style layer into the computed style (CSS cascade)
static void mergeStyle(TextStyle *target, const TextStyle *layer) {
    if ((*layer).setFlags & STYLE_HAS_FONT) (*target).font = (*layer).font;
    if ((*layer).setFlags & STYLE_HAS_SIZE) (*target).size = (*layer).size;
    if ((*layer).setFlags & STYLE_HAS_COLOR) (*target).color = (*layer).color;
    if ((*layer).setFlags & STYLE_HAS_BOLD) (*target).isBold = (*layer).isBold;
    if ((*layer).setFlags & STYLE_HAS_DECOR) {
        (*target).decor = (*layer).decor;
        (*target).decorColor = (*layer).decorColor;
    }
    if ((*layer).setFlags & STYLE_HAS_LSPACING) {
        (*target).letterSpacing = (*layer).letterSpacing;
        (*target).lineSpacing = (*layer).lineSpacing;
    }
    if ((*layer).setFlags & STYLE_HAS_SHADOW) {
        (*target).hasShadow = (*layer).hasShadow;
        (*target).shadowX = (*layer).shadowX;
        (*target).shadowY = (*layer).shadowY;
        (*target).shadowColor = (*layer).shadowColor;
    }

}

// Push a run into the array
static void pushRun(RichText *rt, RunType type, int start, int len, TextStyle style, TextAlign align) {
    if ((*rt).runCount >= (*rt).runCapacity) {
        size_t newCap = (*rt).runCapacity == 0 ? 16 : (*rt).runCapacity * 2;
        TextRun *newRuns = (TextRun*) Memory_realloc((*rt).runs, newCap * sizeof(TextRun));
        if (!newRuns) return;
        (*rt).runs = newRuns;
        (*rt).runCapacity = newCap;
    }
    (*rt).runs[(*rt).runCount].type = type;
    (*rt).runs[(*rt).runCount].startChar = start;
    (*rt).runs[(*rt).runCount].length = len;
    (*rt).runs[(*rt).runCount].computedStyle = style;
    (*rt).runs[(*rt).runCount].align = align;
    (*rt).runCount++;
}

static bool ensureQuadsCapacity(RichText *rt) {
    if ((*rt).quadCount >= (*rt).quadCapacity) {
        size_t newCap = (*rt).quadCapacity == 0 ? 64 : (*rt).quadCapacity * 2;
        TextQuad *newQuads = (TextQuad*) Memory_realloc((*rt).quads, newCap * sizeof(TextQuad));
        if (!newQuads) return false;
        (*rt).quads = newQuads;
        (*rt).quadCapacity = newCap;
    }
    return true;
}

void RichText_setString(RichText *rt, const char *str) {
    if (!rt) return;
    if ((*rt).rawString) Memory_free((*rt).rawString);
    (*rt).rawString = str ? str ? strcpy((char*) Memory_alloc(TYPE_ARRAY, strlen(str)+1), str) : NULL : NULL;
    (*rt).runCount = 0;
    if (!(*rt).rawString) return;

    // Default style
    TextStyle computed = {0};
    TextAlign align = ALIGN_LEFT;

    int len = strlen((*rt).rawString);
    int currentRunStart = 0;

    for (int i = 0; i < len; ) {
        if ((*rt).rawString[i] == '\\' && i + 1 < len && (*rt).rawString[i+1] == '[') {
            i += 2; // Skip escaped bracket
            continue;
        }

        if ((*rt).rawString[i] == '[') {
            // Close current text run
            if (i > currentRunStart) {
                pushRun(rt, RUN_TEXT, currentRunStart, i - currentRunStart, computed, align);
            }
            
            i++;
            int tagStart = i;
            while (i < len && (*rt).rawString[i] != ']') i++;
            int tagEnd = i;
            
            // Parse contents split by '|'
            int cursor = tagStart;
                        
            // Let's implement full cascade: start from default for this block.
            TextStyle newComputed = {0}; // Start fresh for this tag block
            
            while (cursor < tagEnd) {
                int nextPipe = cursor;
                while (nextPipe < tagEnd && (*rt).rawString[nextPipe] != '|') nextPipe++;
                
                int tokLen = nextPipe - cursor;
                if (tokLen == 1) {
                    char c = (*rt).rawString[cursor];
                    if (c == 'n') {
                        pushRun(rt, RUN_NEWLINE, 0, 0, newComputed, align);
                    } else if (c == 'l') { align = ALIGN_LEFT; }
                    else if (c == 'c') { align = ALIGN_CENTER; }
                    else if (c == 'r') { align = ALIGN_RIGHT; }
                    else if (c == 'j') { align = ALIGN_JUSTIFY; }
                } else if (tokLen > 0) {
                    // Try parsing as ID
                    int id = 0;
                    bool isNum = true;
                    for (int k = 0; k < tokLen; k++) {
                        char c = (*rt).rawString[cursor + k];
                        if (c >= '0' && c <= '9') {
                            id = id * 10 + (c - '0');
                        } else {
                            isNum = false; break;
                        }
                    }
                    if (isNum && id >= 0 && (size_t)id < (*rt).styleCapacity) {
                        mergeStyle(&newComputed, &(*rt).styles[id]);
                    }
                }
                cursor = nextPipe + 1;
            }
            computed = newComputed;
            
            if (i < len && (*rt).rawString[i] == ']') i++;
            currentRunStart = i;
            continue;
        }
        i++;
    }
    
    if (len > currentRunStart) {
        pushRun(rt, RUN_TEXT, currentRunStart, len - currentRunStart, computed, align);
    }
}

// Helper to apply alignment shifts to a completed line of quads
static void applyLineAlignment(RichText *rt, int firstQuad, int lastQuad, float lineWidth, float maxWidth, TextAlign align) {
    if (firstQuad > lastQuad || align == ALIGN_LEFT || maxWidth <= 0) return;
    
    float shiftX = 0;
    if (align == ALIGN_CENTER) {
        shiftX = (maxWidth - lineWidth) / 2.0f;
    } else if (align == ALIGN_RIGHT) {
        shiftX = maxWidth - lineWidth;
    }
    
    if (shiftX > 0) {
        for (int i = firstQuad; i <= lastQuad; i++) {
            (*rt).quads[i].x += shiftX;
        }
    }
}

void RichText_layout(RichText *rt, float maxWidth) {
    if (!rt) return;
    (*rt).quadCount = 0;
    
    float cursorX = 0;
    float cursorY = 0;
    float maxLineHeight = 0;
    float globalMaxX = 0;
    
    int lineStartQuad = 0;
    TextAlign currentAlign = ALIGN_LEFT;
    
    for (size_t r = 0; r < (*rt).runCount; r++) {
        TextRun *run = &(*rt).runs[r];
        currentAlign = (*run).align;
        
        if ((*run).type == RUN_NEWLINE) {
            applyLineAlignment(rt, lineStartQuad, (*rt).quadCount - 1, cursorX, maxWidth, currentAlign);
            cursorX = 0;
            cursorY += maxLineHeight;
            maxLineHeight = 0;
            lineStartQuad = (*rt).quadCount;
            continue;
        }
        
        TextStyle style = (*run).computedStyle;
        if (!style.font) continue;
        
        float ascent, descent, lineGap;
        Font_getVMetrics(style.font, &ascent, &descent, &lineGap);
        float scale = Font_getScaleForPixelHeight(style.font, style.size);
        float runAscent = ascent * scale;
        float runLineHeight = (ascent - descent + lineGap) * scale + style.lineSpacing;
        
        if (runLineHeight > maxLineHeight) maxLineHeight = runLineHeight;
        
        float runStartX = cursorX;
        uint32_t prevChar = 0;
        
        for (int i = 0; i < (*run).length; ) {
            uint32_t c = 0;
            unsigned char c0 = (unsigned char)(*rt).rawString[(*run).startChar + i];
            int charLen = 1;
            if (c0 < 0x80) { c = c0; }
            else if ((c0 & 0xE0) == 0xC0) {
                if (i+1 < (*run).length) c = ((c0 & 0x1F) << 6) | ((*rt).rawString[(*run).startChar + i + 1] & 0x3F);
                charLen = 2;
            } else if ((c0 & 0xF0) == 0xE0) {
                if (i+2 < (*run).length) c = ((c0 & 0x0F) << 12) | (((*rt).rawString[(*run).startChar + i + 1] & 0x3F) << 6) | ((*rt).rawString[(*run).startChar + i + 2] & 0x3F);
                charLen = 3;
            } else if ((c0 & 0xF8) == 0xF0) {
                if (i+3 < (*run).length) c = ((c0 & 0x07) << 18) | (((*rt).rawString[(*run).startChar + i + 1] & 0x3F) << 12) | (((*rt).rawString[(*run).startChar + i + 2] & 0x3F) << 6) | ((*rt).rawString[(*run).startChar + i + 3] & 0x3F);
                charLen = 4;
            }

            if (c == '\\' && i + charLen < (*run).length && (*rt).rawString[(*run).startChar + i + charLen] == '[') {
                c = '[';
                charLen += 1;
            }
            if (c == '\n') {
                prevChar = 0;
                applyLineAlignment(rt, lineStartQuad, (*rt).quadCount - 1, cursorX, maxWidth, currentAlign);
                cursorX = 0;
                cursorY += maxLineHeight;
                maxLineHeight = runLineHeight;
                runStartX = 0;
                lineStartQuad = (*rt).quadCount;
                i += charLen;
                continue;
            }

            
            GlyphMetrics gm;
            if (Font_getGlyph(style.font, c, style.size, &gm)) {
                if (prevChar != 0) {
                    cursorX += Font_getKerning(style.font, prevChar, c, style.size);
                }
                prevChar = c;
                
                // Wrap logic
                if (maxWidth > 0 && cursorX + gm.advance > maxWidth && (*rt).wrapMode != WRAP_NONE) {
                    if ((*rt).wrapMode == WRAP_CHAR || ((*rt).wrapMode == WRAP_WORD && c == ' ')) {
                        applyLineAlignment(rt, lineStartQuad, (*rt).quadCount - 1, cursorX, maxWidth, currentAlign);
                        cursorX = 0;
                        cursorY += maxLineHeight;
                        maxLineHeight = runLineHeight;
                        runStartX = 0;
                        lineStartQuad = (*rt).quadCount;
                        if (c == ' ') continue; // Consume space
                    }
                }
                
                if (style.hasShadow) {
                    if (!ensureQuadsCapacity(rt)) break;
                    TextQuad *sq = &(*rt).quads[(*rt).quadCount++];
                    (*sq).x = cursorX + gm.xOffset + style.shadowX;
                    (*sq).y = cursorY + runAscent + gm.yOffset + style.shadowY;
                    (*sq).w = gm.width; (*sq).h = gm.height;
                    (*sq).u0 = gm.u0; (*sq).v0 = gm.v0; (*sq).u1 = gm.u1; (*sq).v1 = gm.v1;
                    (*sq).color = style.shadowColor; // Stored as shadow color
                    int32_t sqTex = Font_pageTextureId(style.font, (size_t)gm.page);
                    (*sq).textureId = sqTex >= 0 ? sqTex : Font_getTextureId(style.font);
                    // Pass a negative bold value to signal the shader to blur
                    (*sq).bold = -0.5f;
                    (*sq).isColor = false; // shadows render through the SDF path 
                    (*sq).decor = DECOR_NONE;
                    (*sq).charIndex = -1;
                    (*sq).advance = 0.0f;
                }
                
                if (!ensureQuadsCapacity(rt)) break;
                TextQuad *q = &(*rt).quads[(*rt).quadCount++];
                (*q).x = cursorX + gm.xOffset;
                (*q).y = cursorY + runAscent + gm.yOffset;
                (*q).w = gm.width;
                (*q).h = gm.height;
                (*q).u0 = gm.u0; (*q).v0 = gm.v0; (*q).u1 = gm.u1; (*q).v1 = gm.v1;
                (*q).color = style.color;
                int32_t qTex = Font_pageTextureId(style.font, (size_t)gm.page);
                (*q).textureId = qTex >= 0 ? qTex : Font_getTextureId(style.font);
                (*q).isColor = gm.color != 0;
                (*q).bold = style.isBold ? 0.04f : 0.0f;
                (*q).decor = DECOR_NONE;
                (*q).charIndex = (*run).startChar + i;
                (*q).advance = gm.advance + style.letterSpacing;
                
                cursorX += gm.advance + style.letterSpacing;
                if (cursorX > globalMaxX) globalMaxX = cursorX;
            }
            i += charLen;
        }
        
        // Add decoration quad if needed
        if (style.decor != DECOR_NONE && cursorX > runStartX) {
            float decorY = cursorY + runAscent + 2.0f;
            float decorH = style.size * 0.08f;
            if (decorH < 1.0f) decorH = 1.0f;
            
            if (!ensureQuadsCapacity(rt)) break;
            TextQuad *q = &(*rt).quads[(*rt).quadCount++];
            (*q).x = runStartX; (*q).y = decorY; (*q).w = cursorX - runStartX; (*q).h = decorH;
            (*q).color = style.decorColor;
            (*q).decor = style.decor;
            (*q).textureId = -1; // solid quad
            (*q).isColor = false;
            (*q).charIndex = -1;
            (*q).advance = 0.0f;
        }
    }
    
    applyLineAlignment(rt, lineStartQuad, (*rt).quadCount - 1, cursorX, maxWidth, currentAlign);
    (*rt).layoutWidth = globalMaxX;
    (*rt).layoutHeight = cursorY + maxLineHeight;
}
