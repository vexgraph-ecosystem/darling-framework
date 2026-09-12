#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#import <CoreText/CoreText.h>
#import <CoreGraphics/CoreGraphics.h>

#include <stdlib.h>
#include <string.h>

#include "text/text_core.h"

#include "annotation/draft.h"
#include "annotation/intention.h"
#include "annotation/overview.h"

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: Text_core (objc/text_core.m)
 * LEVEL: L4 — Self-Management (OS CoreText line raster shim)
 * ============================================================================
 * native line raster seam.
 *
 * STRUCT FIELDS: none — procedural (CoreText line raster shim, no struct).
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


;;DRAFT
;;INTENTION("Native CoreText line raster for sharp Label, active-mode backing")

// Headless test seam (mirrors text/text_core_stub.c): when enabled,
// copy/paste route through an in-memory buffer and never touch the board.
static bool s_testClip = false;
static char s_testClipBuf[8192];

void TextCore_setTestClipboard(bool enable) {
    s_testClip = enable;
    if (s_testClipBuf[0] != '\0')
        s_testClipBuf[0] = '\0';
}

float TextCore_backingScale(void) {
    CGFloat s = [[NSScreen mainScreen] backingScaleFactor];
    if (s > 0.0)
        return (float) s;
    return 1.0f;
}

bool TextCore_rasterLine(const char *utf8, const char *family, float pxHeight, uint32_t argb, uint8_t **outRgba, int *outW, int *outH) {
    return TextCore_rasterStyled(utf8, family, pxHeight, argb, nullptr, outRgba, outW, outH);
}

// Per-glyph pen offsets from the same shaper that paints: same font fallback
// chain, ligature flag, and tracking as TextCore_rasterStyled, so the table
// matches the baked highlight exactly. Single-line only; anything else fails
// closed and the caller keeps its uniform fallback.
int32_t TextCore_lineOffsets(const char *utf8, const char *family, float pxHeight,
                             bool ligatures, float kernPts,
                             float *outPts, int32_t cap) {
    if (!utf8 || !outPts)
        return -1;
    if (pxHeight <= 0.0f || cap <= 0)
        return -1;
    size_t byteLen = strlen(utf8);
    if (byteLen == 0)
        return -1;
    if (memchr(utf8, '\n', byteLen) != NULL)
        return -1;
    if (byteLen + 1 > (size_t) cap)
        return -1;

    @autoreleasepool {
        NSString *str = [NSString stringWithUTF8String:utf8];
        if (!str)
            return -1;
        NSString *fam = family ? [NSString stringWithUTF8String:family] : @"Helvetica";
        CTFontRef font = CTFontCreateWithName((__bridge CFStringRef) fam, pxHeight, NULL);
        if (!font) {
            font = CTFontCreateWithName(CFSTR("Helvetica"), pxHeight, NULL);
            if (!font)
                return -1;
        }
        CGGlyph testG[2] = {0};
        UniChar testC[2] = {'a', 'e'};
        if (CTFontGetGlyphsForCharacters(font, testC, testG, 2) && testG[0] == testG[1]) {
            CFRelease(font);
            font = CTFontCreateWithName(CFSTR("Menlo"), pxHeight, NULL);
            if (!font)
                font = CTFontCreateWithName(CFSTR("Helvetica"), pxHeight, NULL);
            if (!font)
                return -1;
        }

        float backing = TextCore_backingScale();
        if (backing <= 0.0f)
            backing = 1.0f;
        CGFloat kern = (kernPts != 0.0f) ? (kernPts * backing) : 0.0f;

        NSMutableDictionary *attrs = [NSMutableDictionary dictionaryWithCapacity:4];
        [attrs setObject:(__bridge id) font forKey:(id)kCTFontAttributeName];
        [attrs setObject:@(ligatures ? 1 : 0) forKey:(id)kCTLigatureAttributeName];
        if (kern != 0.0f) {
            [attrs setObject:@(kern) forKey:(id)kCTKernAttributeName];
        }
        NSAttributedString *rattr = [[NSAttributedString alloc] initWithString:str attributes:attrs];
        CTLineRef line = CTLineCreateWithAttributedString((__bridge CFAttributedStringRef) rattr);
        if (!line) {
            CFRelease(font);
            return -1;
        }
        // Byte index -> UTF-16 index map (NSString space). Continuation bytes
        // share their codepoint's entry; astral codepoints span two units.
        CFIndex u16 = 0;
        size_t i = 0;
        while (i < byteLen) {
            unsigned char c0 = (unsigned char) utf8[i];
            size_t charLen = 1;
            int u16Len = 1;
            if (c0 < 0x80) {
                charLen = 1;
            } else if ((c0 & 0xE0) == 0xC0) {
                charLen = 2;
            } else if ((c0 & 0xF0) == 0xE0) {
                charLen = 3;
            } else if ((c0 & 0xF8) == 0xF0) {
                charLen = 4;
                u16Len = 2;
            }
            if (i + charLen > byteLen)
                charLen = 1;
            CGFloat px = CTLineGetOffsetForStringIndex(line, u16, NULL);
            float pts = (float) (px / (double) backing);
            for (size_t k = 0; k < charLen && i + k < byteLen; k++)
                outPts[i + k] = pts;
            i += charLen;
            u16 += u16Len;
        }
        double adv = CTLineGetTypographicBounds(line, NULL, NULL, NULL);
        outPts[byteLen] = (float) (adv / (double) backing);
        CFRelease(line);
        CFRelease(font);
        return (int32_t) (byteLen + 1);
    }
}

bool TextCore_rasterStyled(const char *utf8, const char *family, float pxHeight, uint32_t argb,
                           const TextStyleDescriptor *style, uint8_t **outRgba, int *outW, int *outH) {
    if (!utf8 || !outRgba || !outW || !outH)
        return false;
    if (pxHeight <= 0.0f)
        return false;
    (*outRgba) = nullptr;
    (*outW) = 0;
    (*outH) = 0;

    @autoreleasepool {
        NSString *str = [NSString stringWithUTF8String:utf8];
        if (!str)
            return false;
        NSString *fam = family ? [NSString stringWithUTF8String:family] : @"Helvetica";
        CTFontRef font = CTFontCreateWithName((__bridge CFStringRef) fam, pxHeight, NULL);
        if (!font) {
            font = CTFontCreateWithName(CFSTR("Helvetica"), pxHeight, NULL);
            if (!font)
                return false;
        }
        CGGlyph testG[2] = {0};
        UniChar testC[2] = {'a', 'e'};
        if (CTFontGetGlyphsForCharacters(font, testC, testG, 2) && testG[0] == testG[1]) {
            CFRelease(font);
            font = CTFontCreateWithName(CFSTR("Menlo"), pxHeight, NULL);
            if (!font)
                font = CTFontCreateWithName(CFSTR("Helvetica"), pxHeight, NULL);
            if (!font)
                return false;
        }

        uint8_t a = (uint8_t) ((argb >> 24) & 0xFF);
        uint8_t r = (uint8_t) ((argb >> 16) & 0xFF);
        uint8_t g = (uint8_t) ((argb >> 8) & 0xFF);
        uint8_t b = (uint8_t) (argb & 0xFF);

        float backing = TextCore_backingScale();
        if (backing <= 0.0f)
            backing = 1.0f;

        bool enableLigatures = style ? (*style).ligatures : true;
        CGFloat kern = (style && (*style).spacingWidth != 0.0f) ? ((*style).spacingWidth * backing) : 0.0f;
        CGFloat extraLineHeight = (style && (*style).spacingHeight != 0.0f) ? ((*style).spacingHeight * backing) : 0.0f;

        NSMutableDictionary *attrs = [NSMutableDictionary dictionaryWithCapacity:4];
        [attrs setObject:(__bridge id) font forKey:(id)kCTFontAttributeName];
        [attrs setObject:(id)[NSColor colorWithCalibratedRed:(r/255.0) green:(g/255.0) blue:(b/255.0) alpha:(a/255.0)].CGColor forKey:(id)kCTForegroundColorAttributeName];
        [attrs setObject:@(enableLigatures ? 1 : 0) forKey:(id)kCTLigatureAttributeName];
        if (kern != 0.0f) {
            [attrs setObject:@(kern) forKey:(id)kCTKernAttributeName];
        }

        // Multiline: split on \n, one CTLine per row, stacked top to bottom.
        NSArray<NSString *> *rows = [str componentsSeparatedByString:@"\n"];
        if ([rows count] == 0)
            rows = @[str];
        size_t nlines = [rows count];
        if (nlines > 64)
            nlines = 64;
        CTLineRef lines[64];
        CGFloat asc[64];
        CGFloat desc[64];
        CGFloat lead[64];
        double advs[64];
        for (size_t k = 0; k < nlines; k++) {
            NSString *row = [rows objectAtIndex:k];
            NSAttributedString *rattr = [[NSAttributedString alloc] initWithString:row attributes:attrs];
            CTLineRef line = CTLineCreateWithAttributedString((__bridge CFAttributedStringRef) rattr);
            lines[k] = line;
            asc[k] = 0;
            desc[k] = 0;
            lead[k] = 0;
            advs[k] = 0;
            if (line)
                advs[k] = CTLineGetTypographicBounds(line, &asc[k], &desc[k], &lead[k]);
        }
        CFRelease(font);
        double maxAdv = 0;
        double totalH = 0;
        for (size_t k = 0; k < nlines; k++) {
            if (advs[k] > maxAdv)
                maxAdv = advs[k];
            totalH += asc[k] + desc[k] + lead[k] + (k > 0 ? extraLineHeight : 0.0);
        }
        int w = (int) ceil(maxAdv) + 2;
        int h = (int) ceil(totalH) + (int) (2 * nlines);
        bool okLines = true;
        for (size_t k = 0; k < nlines; k++) {
            if (!lines[k])
                okLines = false;
        }
        if (!okLines) {
            for (size_t k = 0; k < nlines; k++) {
                if (lines[k])
                    CFRelease(lines[k]);
            }
            return false;
        }
        if (w <= 0 || h <= 0) {
            for (size_t k = 0; k < nlines; k++)
                CFRelease(lines[k]);
            return false;
        }
        if (w > 8192)
            w = 8192;
        if (h > 4096)
            h = 4096;

        size_t rowBytes = (size_t) w * 4;
        uint8_t *buf = (uint8_t*) calloc((size_t) h, rowBytes);
        if (!buf) {
            for (size_t k = 0; k < nlines; k++)
                CFRelease(lines[k]);
            return false;
        }

        CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
        CGContextRef ctx = CGBitmapContextCreate(buf, w, h, 8, rowBytes, cs,
            kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
        CGColorSpaceRelease(cs);
        if (!ctx) {
            free(buf);
            for (size_t k = 0; k < nlines; k++)
                CFRelease(lines[k]);
            return false;
        }

        CGContextSetShouldAntialias(ctx, true);
        CGContextSetAllowsFontSmoothing(ctx, false);
        CGContextSetShouldSmoothFonts(ctx, false);
        CGContextSetAllowsFontSubpixelPositioning(ctx, true);
        CGContextSetShouldSubpixelPositionFonts(ctx, true);
        CGContextSetAllowsFontSubpixelQuantization(ctx, true);
        CGContextSetShouldSubpixelQuantizeFonts(ctx, true);
        CGContextClearRect(ctx, CGRectMake(0, 0, w, h));

        // Determine underline color & stroke width
        UnderlineStyle ustyle = style ? (*style).underline : UNDERLINE_NONE;
        uint32_t ucolor = (style && (*style).underlineColor != 0) ? (*style).underlineColor : argb;
        CGFloat ur = ((ucolor >> 16) & 0xFF) / 255.0;
        CGFloat ug = ((ucolor >> 8) & 0xFF) / 255.0;
        CGFloat ub = (ucolor & 0xFF) / 255.0;
        CGFloat ua = ((ucolor >> 24) & 0xFF) / 255.0;
        CGFloat strokeW = fmax(1.0, floor(backing));

        // Stack lines from the bottom: last row at 1+descent, earlier above.
        // In CGBitmapContext, row 0 in memory is already visual top with upright glyphs.
        {
            double penY = 1.0;
            for (size_t k = nlines; k > 0; k--) {
                size_t idx = k - 1;
                CGFloat baselineY = penY + desc[idx];

                // Selection highlight rounded rectangle behind text
                if (style && (*style).selectionStart >= 0 && (*style).selectionEnd > (*style).selectionStart) {
                    int selStart = (*style).selectionStart;
                    int selEnd = (*style).selectionEnd;
                    if (selStart > selEnd) {
                        int tmp = selStart;
                        selStart = selEnd;
                        selEnd = tmp;
                    }
                    int rowLen = (int) [rows[idx] length];
                    int s0 = (int) fmax(0.0, (double) selStart);
                    int s1 = (int) fmin((double) rowLen, (double) selEnd);
                    if (s1 > s0) {
                        CGFloat sx0 = CTLineGetOffsetForStringIndex(lines[idx], s0, NULL);
                        CGFloat sx1 = CTLineGetOffsetForStringIndex(lines[idx], s1, NULL);
                        if (sx1 < sx0) {
                            CGFloat tmp = sx0;
                            sx0 = sx1;
                            sx1 = tmp;
                        }
                        CGFloat rectX = 1.0 + sx0;
                        CGFloat rectW = sx1 - sx0;
                        CGFloat rectY = fmax(0.0, penY - 1.0 * backing);
                        CGFloat rectH = asc[idx] + desc[idx] + 2.0 * backing;

                        float radPts = ((*style).highlightRadius > 0.0f) ? (*style).highlightRadius : 3.0f;
                        CGFloat rad = radPts * backing;
                        if (rad > rectH * 0.5)
                            rad = rectH * 0.5;
                        if (rad > rectW * 0.5)
                            rad = rectW * 0.5;

                        uint32_t hcol = ((*style).highlightColor != 0) ? (*style).highlightColor : 0x662563EBu;
                        CGFloat ha = ((hcol >> 24) & 0xFF) / 255.0;
                        CGFloat hr = ((hcol >> 16) & 0xFF) / 255.0;
                        CGFloat hg = ((hcol >> 8) & 0xFF) / 255.0;
                        CGFloat hb = (hcol & 0xFF) / 255.0;

                        CGContextSaveGState(ctx);
                        CGContextSetRGBFillColor(ctx, hr, hg, hb, ha);
                        CGRect selRect = CGRectMake(rectX, rectY, rectW, rectH);
                        if (rad > 0.0) {
                            CGPathRef rpath = CGPathCreateWithRoundedRect(selRect, rad, rad, NULL);
                            CGContextAddPath(ctx, rpath);
                            CGContextFillPath(ctx);
                            CGPathRelease(rpath);
                        } else {
                            CGContextFillRect(ctx, selRect);
                        }
                        CGContextRestoreGState(ctx);
                    }
                }

                CGContextSetTextPosition(ctx, 1.0, baselineY);
                CTLineDraw(lines[idx], ctx);

                // Underline decorations
                if (ustyle != UNDERLINE_NONE && advs[idx] > 0.0) {
                    CGContextSaveGState(ctx);
                    CGContextSetRGBStrokeColor(ctx, ur, ug, ub, ua);
                    CGContextSetLineWidth(ctx, strokeW);
                    CGFloat x0 = 1.0;
                    CGFloat x1 = 1.0 + advs[idx];

                    if (ustyle == UNDERLINE_BASIC) {
                        CGFloat lineY = baselineY - fmax(1.0 * backing, desc[idx] * 0.4);
                        CGContextMoveToPoint(ctx, x0, lineY);
                        CGContextAddLineToPoint(ctx, x1, lineY);
                        CGContextStrokePath(ctx);
                    } else if (ustyle == UNDERLINE_STRIKETHROUGH) {
                        CGFloat lineY = baselineY + asc[idx] * 0.35;
                        CGContextMoveToPoint(ctx, x0, lineY);
                        CGContextAddLineToPoint(ctx, x1, lineY);
                        CGContextStrokePath(ctx);
                    } else if (ustyle == UNDERLINE_JAGGED) {
                        CGFloat lineY = baselineY - fmax(1.0 * backing, desc[idx] * 0.4);
                        CGFloat waveLen = 4.0 * backing;
                        CGFloat amp = 1.2 * backing;
                        CGMutablePathRef path = CGPathCreateMutable();
                        CGPathMoveToPoint(path, NULL, x0, lineY);
                        CGFloat cx = x0;
                        BOOL up = YES;
                        while (cx < x1) {
                            CGFloat nx = fmin(cx + waveLen * 0.5, x1);
                            CGFloat ny = lineY + (up ? amp : -amp);
                            CGPathAddLineToPoint(path, NULL, nx, ny);
                            cx = nx;
                            up = !up;
                        }
                        CGContextAddPath(ctx, path);
                        CGContextStrokePath(ctx);
                        CGPathRelease(path);
                    }
                    CGContextRestoreGState(ctx);
                }

                // Mnemonic accelerator character underline
                if (style && (*style).mnemonicIndex >= 0 && ustyle == UNDERLINE_NONE) {
                    if ((*style).mnemonicIndex < (int)[rows[idx] length]) {
                        CGFloat mx0 = CTLineGetOffsetForStringIndex(lines[idx], (*style).mnemonicIndex, NULL);
                        CGFloat mx1 = CTLineGetOffsetForStringIndex(lines[idx], (*style).mnemonicIndex + 1, NULL);
                        if (mx1 > mx0) {
                            CGContextSaveGState(ctx);
                            CGContextSetRGBStrokeColor(ctx, ur, ug, ub, ua);
                            CGContextSetLineWidth(ctx, strokeW);
                            CGFloat lineY = baselineY - fmax(1.0 * backing, desc[idx] * 0.4);
                            CGContextMoveToPoint(ctx, 1.0 + mx0, lineY);
                            CGContextAddLineToPoint(ctx, 1.0 + mx1, lineY);
                            CGContextStrokePath(ctx);
                            CGContextRestoreGState(ctx);
                        }
                    }
                }

                penY += asc[idx] + desc[idx] + lead[idx] + 2.0 + extraLineHeight;
            }
        }
        CGContextRelease(ctx);
        for (size_t k = 0; k < nlines; k++)
            CFRelease(lines[k]);

        (*outRgba) = buf;
        (*outW) = w;
        (*outH) = h;
        return true;
    }
}

void TextCore_copyToClipboard(const char *utf8) {
    if (!utf8)
        return;
    if (s_testClip) {
        s_testClipBuf[0] = '\0';
        strncat(s_testClipBuf, utf8, sizeof(s_testClipBuf) - 1);
        return;
    }
    @autoreleasepool {
        NSPasteboard *pb = [NSPasteboard generalPasteboard];
        [pb clearContents];
        NSString *str = [NSString stringWithUTF8String:utf8];
        if (str) {
            [pb setString:str forType:NSPasteboardTypeString];
        }
    }
}

char *TextCore_pasteFromClipboard(void) {
    if (s_testClip) {
        if (s_testClipBuf[0] == '\0')
            return nullptr;
        return strdup(s_testClipBuf);
    }
    @autoreleasepool {
        NSPasteboard *pb = [NSPasteboard generalPasteboard];
        NSString *str = [pb stringForType:NSPasteboardTypeString];
        if (!str)
            return nullptr;
        const char *utf8 = [str UTF8String];
        return utf8 ? strdup(utf8) : nullptr;
    }
}
