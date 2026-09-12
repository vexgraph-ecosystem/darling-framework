#include "text/text_core.h"

#include <stdlib.h>
#include <string.h>

#include "annotation/draft.h"
#include "annotation/intention.h"
#include "annotation/overview.h"

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: Text_core_stub (text/text_core_stub.c)
 * LEVEL: L2 — Behavior (portable text fallback behavior API)
 * ============================================================================
 * non-Apple fallback. No native raster here;
 *
 * STRUCT FIELDS: none — procedural (stateless TextCore seam fallback).
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - TextCore_backingScale(void)
 *   - TextCore_rasterLine(utf8, family, pxHeight, argb, outRgba, outW, outH)
 *   - TextCore_rasterStyled(utf8, family, pxHeight, argb, style, outRgba, outW, outH)
 *   - TextCore_lineOffsets(utf8, family, pxHeight, ligatures, kernPts, outPts, cap)
 * ============================================================================
 */


// text/text_core_stub.c — non-Apple fallback. No native raster here;
// Label falls back to the SDF atlas path.
;;DRAFT
;;INTENTION("Stub native raster off Apple, SDF fallback owns text")

float TextCore_backingScale(void) {
    return 1.0f;
}

bool TextCore_rasterLine(const char *utf8, const char *family, float pxHeight, uint32_t argb, uint8_t **outRgba, int *outW, int *outH) {
    if (!utf8 || !family)
        return false;
    if (pxHeight <= 0.0f)
        return false;
    if (!outRgba || !outW || !outH)
        return false;
    if (!argb)
        return false;
    (*outRgba) = nullptr;
    (*outW) = 0;
    (*outH) = 0;
    return false;
}

bool TextCore_rasterStyled(const char *utf8, const char *family, float pxHeight, uint32_t argb,
                           const TextStyleDescriptor *style, uint8_t **outRgba, int *outW, int *outH) {
    (void) style;
    return TextCore_rasterLine(utf8, family, pxHeight, argb, outRgba, outW, outH);
}

// No native shaper off Apple: per-glyph offsets are unavailable, so the
// caller keeps its uniform fallback. Fail closed, never partial.
int32_t TextCore_lineOffsets(const char *utf8, const char *family, float pxHeight,
                             bool ligatures, float kernPts,
                             float *outPts, int32_t cap) {
    (void) utf8;
    (void) family;
    (void) pxHeight;
    (void) ligatures;
    (void) kernPts;
    (void) outPts;
    (void) cap;
    return -1;
}

static bool s_testClip = false;
static char s_testClipBuf[8192];

void TextCore_setTestClipboard(bool enable) {
    s_testClip = enable;
    if (s_testClipBuf[0] != '\0')
        s_testClipBuf[0] = '\0';
}

void TextCore_copyToClipboard(const char *utf8) {
    if (s_testClip) {
        if (!utf8)
            return;
        s_testClipBuf[0] = '\0';
        strncat(s_testClipBuf, utf8, sizeof(s_testClipBuf) - 1);
        return;
    }
    (void) utf8;
}

char *TextCore_pasteFromClipboard(void) {
    if (s_testClip) {
        if (s_testClipBuf[0] == '\0')
            return nullptr;
        return strdup(s_testClipBuf);
    }
    return nullptr;
}
