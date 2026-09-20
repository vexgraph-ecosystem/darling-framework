#ifndef TEXT_CORE_H
#define TEXT_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// text/text_core.h — native line raster seam.
//
// Label (sharp native path) asks the platform to rasterize one line of UTF-8
// into RGBA8 at native physical pixels. RichLabel stays on the SDF atlas path
// for mask/fill effects.
//
// Backing comes from the active pixel mode (currentWidth / pointWidth), not
// the panel max and not a hardcoded 2.0.

// Underline styling modes
typedef enum UnderlineStyle {
    UNDERLINE_NONE          = 0,
    UNDERLINE_BASIC         = 1,
    UNDERLINE_STRIKETHROUGH = 2,
    UNDERLINE_JAGGED        = 3,
} UnderlineStyle;

// Text alignment modes
typedef enum TextAlign {
    TEXT_ALIGN_LEFT    = 0,
    TEXT_ALIGN_CENTER  = 1,
    TEXT_ALIGN_RIGHT   = 2,
    TEXT_ALIGN_JUSTIFY = 3,
    ALIGN_LEFT         = 0,
    ALIGN_CENTER       = 1,
    ALIGN_RIGHT        = 2,
    ALIGN_JUSTIFY      = 3,
} TextAlign;

// Rich typography style descriptor passed to native rasterizer
typedef struct TextStyleDescriptor {
    bool ligatures;           // true = enable ligatures (default true)
    float spacingWidth;       // tracking/kerning delta in points (default 0.0)
    float spacingHeight;      // extra line leading in points (default 0.0)
    UnderlineStyle underline; // UNDERLINE_NONE, UNDERLINE_BASIC, etc.
    uint32_t underlineColor;  // packed 0xAARRGGBB (0 = match text color)
    int mnemonicIndex;        // -1 = none; character index to underline for mnemonic
    int selectionStart;       // -1 = none; character start index of selection
    int selectionEnd;         // -1 = none; character end index of selection
    float highlightRadius;    // corner radius of selection rounded rect in points (default 3.0f)
    uint32_t highlightColor;  // packed 0xAARRGGBB selection background fill (0 = default 0x662563EB)
    TextAlign align;          // text alignment (default TEXT_ALIGN_LEFT)
    float boundsWidth;        // container bounding width in points (0 = auto / text width)
} TextStyleDescriptor;

// Active backing scale: NSScreen backingScaleFactor (Retina points to pixels).
// For active-mode currentWidth/pointWidth, combine with DisplayInfo on top.
// During a live-resize drag the frame hook pins this seam to the window's
// live backingScaleFactor via TextCore_setBackingScaleOverride, so button /
// label / input raster and retained-layer px math track the
// dragged window — never [[NSScreen mainScreen]] mid-drag. Cleared on settle.
float TextCore_backingScale(void);

// Live scale override (sticky seam pin): positive pins backingScale, cleared
// (<= 0) resyncs to mainScreen. Set every drag step from the resize hook,
// cleared on the settle step. Cold seam only, never from tick/render paths.
void TextCore_setBackingScaleOverride(float scale);
void TextCore_clearBackingScaleOverride(void);

// Rasterize one UTF-8 line. pxHeight is native pixels (points * backing).
// Returns malloc'd RGBA8 (caller frees with free), or nullptr on failure.
// outW/outH receive native pixel dimensions. Last param is dest-last.
bool TextCore_rasterLine(const char *utf8, const char *family, float pxHeight, uint32_t argb, uint8_t **outRgba, int *outW, int *outH);

// Rasterize one UTF-8 string with explicit typography style descriptor.
bool TextCore_rasterStyled(const char *utf8, const char *family, float pxHeight, uint32_t argb,
                           const TextStyleDescriptor *style, uint8_t **outRgba, int *outW, int *outH);

// Per-glyph pen offsets for single-line text, in points: one entry per UTF-8
// byte plus a trailing total advance (strlen(utf8)+1 entries). Entry i is the
// CoreText pen x of the glyph covering byte i (continuation bytes share their
// codepoint's offset). Same font/ligature/tracking inputs as the raster, so
// offsets match paint exactly; the label hit-test shares this table with the
// highlight it paints. Cold path only (label raster rebuild). Multiline,
// empty, bad params, or cap < strlen+1 fail closed with -1 (never partial).
// The stub always returns -1 and the caller keeps its uniform fallback.
int32_t TextCore_lineOffsets(const char *utf8, const char *family, float pxHeight,
                             bool ligatures, float kernPts,
                             float *outPts, int32_t cap);

// Native clipboard integration
void TextCore_copyToClipboard(const char *utf8);
char *TextCore_pasteFromClipboard(void);

// Headless test seam: when enabled, copy/paste route through an in-memory
// buffer instead of the OS board so unit tests never touch (or pollute) the
// real NSPasteboard. Defaults to disabled; cold-test-only, never called from
// tick/render paths.
void TextCore_setTestClipboard(bool enable);

#endif
