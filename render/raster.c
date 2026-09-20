#include "render/raster.h"

#include <stdio.h>
#include "annotation/definition.h"
#include "annotation/overview.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Raster
 * ============================================================================
 * The software rasterizer — pixels before Vulkan: pure painters over any
 * 4-channel Buffer (ColorBuffer layout: R,G,B,A per pixel), clipped to
 * bounds, zero-alloc, no retained state. Every function takes its target
 * buffer first, geometry/colors after — dest-last is reserved for outputs,
 * and here the buffer IS both canvas and destination of record. All geometry
 * is clipped to the buffer bounds per the Forward Rendering & Bounded Surface
 * Law: nothing scribbles outside, so the forward-render scissor path can
 * trust each painter to stay inside its bounded surface. The local RGBA
 * helper is a stack-only packed pixel value with no behavior of its own.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Raster (stateless software rasterizer + local pixel type)
 * LEVEL: L2 — Behavior (software rasterizer behavior API)
 * ============================================================================
 * The software rasterizer (pixels before Vulkan): pure painters over any
 * 4-channel Buffer, clipped to bounds, zero-alloc, no retained state.
 *
 * STRUCT FIELDS (local to this file):
 * ----------------------------------------------------------------------------
 *   RGBA {                 // Stack-only packed pixel helper (no retained state)
 *     uint8_t r, g, b, a;  // Red, green, blue, alpha channels
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - Raster_rect(buf, x, y, w, h, r, g, b, a)
 *   - Raster_roundedRect(buf, x, y, w, h, radius, mode, r, g, b, a)
 *   - Raster_gradientH(buf, x, y, w, h, r0, g0, b0, a0, r1, g1, b1, a1)
 *   - Raster_line(buf, x0, y0, x1, y1, r, g, b, a)
 *   - Raster_triangle(buf, x0, y0, x1, y1, x2, y2, r, g, b, a)
 *   - Raster_dumpPPM(buf, path)
 * ============================================================================
 */


// render/raster.c — software rasterizer. Pure, clipped, zero-alloc.

typedef struct RGBA {
    uint8_t r, g, b, a;
} RGBA;

static void putPixel(Buffer *buf, int x, int y, RGBA c) {
    if (x < 0 || y < 0)
        return;
    if ((size_t)x >= Buffer_width(buf) || (size_t)y >= Buffer_height(buf))
        return;
    ColorBuffer_setRGBA(buf, (size_t)x, (size_t)y, c.r, c.g, c.b, c.a);
}

void Raster_rect(Buffer *buf, int x, int y, int w, int h,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    for (int row = y; row < y + h; row++)
        for (int col = x; col < x + w; col++)
            putPixel(buf, col, row, (RGBA){r, g, b, a});
}

void Raster_roundedRect(Buffer *buf, int x, int y, int w, int h, int radius, int mode,
                        uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!buf || w <= 0 || h <= 0)
        return;
    int half = w < h ? w : h;
    half /= 2;
    int rad = radius;
    if (rad < 0)
        rad = 0;
    if (rad > half)
        rad = half;
    if (rad == 0) {
        Raster_rect(buf, x, y, w, h, r, g, b, a);
        return;
    }
    RGBA c = {r, g, b, a};
    float fr = (float)rad;
    float left = (float)x;
    float top = (float)y;
    float right = (float)(x + w);
    float bottom = (float)(y + h);
    for (int row = y; row < y + h; row++) {
        for (int col = x; col < x + w; col++) {
            float cx = (float)col + 0.5f;
            float cy = (float)row + 0.5f;
            bool inMidX = cx >= left + fr && cx <= right - fr;
            bool inMidY = cy >= top + fr && cy <= bottom - fr;
            if (inMidX || inMidY) {
                putPixel(buf, col, row, c);
                continue;
            }
            float ccx = cx < left + fr ? left + fr : right - fr;
            float ccy = cy < top + fr ? top + fr : bottom - fr;
            float dx = (cx - ccx) / fr;
            float dy = (cy - ccy) / fr;
            bool inside = false;
            if (mode == 1) {
                float qx = dx * dx;
                float qy = dy * dy;
                inside = qx * qx + qy * qy <= 1.0f;
            } else {
                inside = dx * dx + dy * dy <= 1.0f;
            }
            if (inside)
                putPixel(buf, col, row, c);
        }
    }
}

void Raster_gradientH(Buffer *buf, int x, int y, int w, int h,
                      uint8_t r0, uint8_t g0, uint8_t b0, uint8_t a0,
                      uint8_t r1, uint8_t g1, uint8_t b1, uint8_t a1) {
    if (w <= 0)
        return;
    float denom = w > 1 ? (float)(w - 1) : 1.0f;
    for (int col = 0; col < w; col++) {
        float t = (float)col / denom;
        RGBA c = {
            .r = (uint8_t)(r0 + (r1 - r0) * t),
            .g = (uint8_t)(g0 + (g1 - g0) * t),
            .b = (uint8_t)(b0 + (b1 - b0) * t),
            .a = (uint8_t)(a0 + (a1 - a0) * t),
        };
        for (int row = y; row < y + h; row++)
            putPixel(buf, x + col, row, c);
    }
}

void Raster_line(Buffer *buf, int x0, int y0, int x1, int y1,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    int sx = dx > 0 ? 1 : -1;
    int sy = dy > 0 ? 1 : -1;
    int adx = dx > 0 ? dx : -dx;
    int ady = dy > 0 ? dy : -dy;
    int err = adx - ady;
    RGBA c = {r, g, b, a};

    for (;;) {
        putPixel(buf, x0, y0, c);
        if (x0 == x1 && y0 == y1)
            break;
        int e2 = err * 2;
        if (e2 > -ady) {
            err -= ady;
            x0 += sx;
        }
        if (e2 < adx) {
            err += adx;
            y0 += sy;
        }
    }
}

// Twice-signed area of the edge (a->b) crossed with point p.
static long edge(int ax, int ay, int bx, int by, int px, int py) {
    return (long)(px - ax) * (by - ay) - (long)(py - ay) * (bx - ax);
}

void Raster_triangle(Buffer *buf,
                     int x0, int y0, int x1, int y1, int x2, int y2,
                     uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    long area = edge(x0, y0, x1, y1, x2, y2);
    if (area == 0)
        return;

    int minX = x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2);
    int minY = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2);
    int maxX = x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2);
    int maxY = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);

    RGBA c = {r, g, b, a};
    for (int y = minY; y <= maxY; y++) {
        for (int x = minX; x <= maxX; x++) {
            long w0 = edge(x1, y1, x2, y2, x, y);
            long w1 = edge(x2, y2, x0, y0, x, y);
            long w2 = edge(x0, y0, x1, y1, x, y);
            bool inside = area > 0
                ? (w0 >= 0 && w1 >= 0 && w2 >= 0)
                : (w0 <= 0 && w1 <= 0 && w2 <= 0);
            if (inside)
                putPixel(buf, x, y, c);
        }
    }
}

bool Raster_dumpPPM(const Buffer *buf, const char *path) {
    if (!buf || !path)
        return false;
    FILE *out = fopen(path, "wb");
    if (!out)
        return false;
    size_t w = Buffer_width(buf);
    size_t h = Buffer_height(buf);
    fprintf(out, "P6\n%zu %zu\n255\n", w, h);
    for (size_t y = 0; y < h; y++) {
        for (size_t x = 0; x < w; x++) {
            uint8_t rgba[4];
            ColorBuffer_getRGBA(buf, x, y, &rgba[0], &rgba[1], &rgba[2], &rgba[3]);
            fwrite(rgba, 1, 3, out);
        }
    }
    fclose(out);
    return true;
}
