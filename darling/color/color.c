#include "darling/color/color.h"

#include "annotation/overview.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Color
 * LEVEL: L2 — Behavior (Multi-Format Unified Color Representation)
 * ============================================================================
 * Unified color class holding normalized float RGBA values [0.0, 1.0].
 * Provides bidirectional conversions across ARGB32, RGBA32, HSV, HSL, and
 * hexadecimal string encodings while preserving identical color values.
 *
 * STRUCT FIELDS (Mirroring darling/color/color.h):
 * ----------------------------------------------------------------------------
 *   float r;    // Red normalized float [0.0f, 1.0f]
 *   float g;    // Green normalized float [0.0f, 1.0f]
 *   float b;    // Blue normalized float [0.0f, 1.0f]
 *   float a;    // Alpha normalized float [0.0f, 1.0f]
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Color_0(void)
 *   - Color_1(argb)
 *   - Color_2(rgb, a)
 *   - Color_3(r, g, b)
 *   - Color_4(r, g, b, a)
 *   - Color_init(r, g, b, a, dest)
 *   - Color_free(color)
 *
 * Factories:
 *   - Color_fromARGB(argb, dest)
 *   - Color_fromRGBA(r, g, b, a, dest)
 *   - Color_fromHSV(h, s, v, a, dest)
 *   - Color_fromHSL(h, s, l, a, dest)
 *   - Color_fromHex(hex, dest)
 *
 * Projections:
 *   - Color_toARGB(const color)
 *   - Color_toRGBA32(const color)
 *   - Color_toHSV(const color, outH, outS, outV)
 *   - Color_toHSL(const color, outH, outS, outL)
 *   - Color_toHex(const color, includeAlpha, dest, destSize)
 *
 * Core Functions:
 *   - Color_lerp(start, end, t, dest)
 *   - Color_equals(a, b)
 *
 * Setters:
 *   - Color_setRGBA(color, r, g, b, a)
 *   - Color_setARGB(color, argb)
 *   - Color_setHSV(color, h, s, v)
 *   - Color_setHSL(color, h, s, l)
 *   - Color_setHex(color, hex)
 *   - Color_setR(color, r)
 *   - Color_setG(color, g)
 *   - Color_setB(color, b)
 *   - Color_setA(color, a)
 *
 * Getters:
 *   - Color_getR(const color)
 *   - Color_getG(const color)
 *   - Color_getB(const color)
 *   - Color_getA(const color)
 *   - Color_getARGB(const color)
 *   - Color_getHSV(const color, outH, outS, outV)
 *   - Color_getHSL(const color, outH, outS, outL)
 * ============================================================================
 */

static inline float clampF(float val, float minVal, float maxVal) {
    if (val < minVal)
        return minVal;
    if (val > maxVal)
        return maxVal;
    return val;
}

// CONSTRUCTORS
// ============================================================================

bool Color_init(float r, float g, float b, float a, Color *dest) {
    if (dest == nullptr)
        return false;

    (*dest).r = clampF(r, 0.0f, 1.0f);
    (*dest).g = clampF(g, 0.0f, 1.0f);
    (*dest).b = clampF(b, 0.0f, 1.0f);
    (*dest).a = clampF(a, 0.0f, 1.0f);
    return true;
}

Color *Color_0(void) {
    Color *c = (Color*) calloc(1, sizeof(Color));
    if (c == nullptr)
        return nullptr;
    Color_init(1.0f, 1.0f, 1.0f, 1.0f, c);
    return c;
}

Color *Color_1(uint32_t argb) {
    Color *c = (Color*) calloc(1, sizeof(Color));
    if (c == nullptr)
        return nullptr;
    Color_fromARGB(argb, c);
    return c;
}

Color *Color_2(uint32_t rgb, float a) {
    Color *c = (Color*) calloc(1, sizeof(Color));
    if (c == nullptr)
        return nullptr;
    float r = ((rgb >> 16) & 0xFF) / 255.0f;
    float g = ((rgb >> 8) & 0xFF) / 255.0f;
    float b = (rgb & 0xFF) / 255.0f;
    Color_init(r, g, b, a, c);
    return c;
}

Color *Color_3(float r, float g, float b) {
    Color *c = (Color*) calloc(1, sizeof(Color));
    if (c == nullptr)
        return nullptr;
    Color_init(r, g, b, 1.0f, c);
    return c;
}

Color *Color_4(float r, float g, float b, float a) {
    Color *c = (Color*) calloc(1, sizeof(Color));
    if (c == nullptr)
        return nullptr;
    Color_init(r, g, b, a, c);
    return c;
}

void Color_free(Color *color) {
    if (color == nullptr)
        return;
    free(color);
}

// FACTORY INITIALIZERS
// ============================================================================

void Color_fromARGB(uint32_t argb, Color *dest) {
    if (dest == nullptr)
        return;

    float a = ((argb >> 24) & 0xFF) / 255.0f;
    float r = ((argb >> 16) & 0xFF) / 255.0f;
    float g = ((argb >> 8) & 0xFF) / 255.0f;
    float b = (argb & 0xFF) / 255.0f;
    Color_init(r, g, b, a, dest);
}

void Color_fromRGBA(float r, float g, float b, float a, Color *dest) {
    Color_init(r, g, b, a, dest);
}

void Color_fromHSV(float h, float s, float v, float a, Color *dest) {
    if (dest == nullptr)
        return;

    h = fmodf(h, 360.0f);
    if (h < 0.0f)
        h += 360.0f;

    s = clampF(s, 0.0f, 1.0f);
    v = clampF(v, 0.0f, 1.0f);
    a = clampF(a, 0.0f, 1.0f);

    float c = v * s;
    float hp = h / 60.0f;
    float x = c * (1.0f - fabsf(fmodf(hp, 2.0f) - 1.0f));
    float m = v - c;

    float r1 = 0.0f;
    float g1 = 0.0f;
    float b1 = 0.0f;

    if (hp >= 0.0f && hp < 1.0f) {
        r1 = c;
        g1 = x;
    } else if (hp >= 1.0f && hp < 2.0f) {
        r1 = x;
        g1 = c;
    } else if (hp >= 2.0f && hp < 3.0f) {
        g1 = c;
        b1 = x;
    } else if (hp >= 3.0f && hp < 4.0f) {
        g1 = x;
        b1 = c;
    } else if (hp >= 4.0f && hp < 5.0f) {
        r1 = x;
        b1 = c;
    } else if (hp >= 5.0f && hp < 6.0f) {
        r1 = c;
        b1 = x;
    }

    Color_init(r1 + m, g1 + m, b1 + m, a, dest);
}

void Color_fromHSL(float h, float s, float l, float a, Color *dest) {
    if (dest == nullptr)
        return;

    h = fmodf(h, 360.0f);
    if (h < 0.0f)
        h += 360.0f;

    s = clampF(s, 0.0f, 1.0f);
    l = clampF(l, 0.0f, 1.0f);
    a = clampF(a, 0.0f, 1.0f);

    float c = (1.0f - fabsf(2.0f * l - 1.0f)) * s;
    float hp = h / 60.0f;
    float x = c * (1.0f - fabsf(fmodf(hp, 2.0f) - 1.0f));
    float m = l - c * 0.5f;

    float r1 = 0.0f;
    float g1 = 0.0f;
    float b1 = 0.0f;

    if (hp >= 0.0f && hp < 1.0f) {
        r1 = c;
        g1 = x;
    } else if (hp >= 1.0f && hp < 2.0f) {
        r1 = x;
        g1 = c;
    } else if (hp >= 2.0f && hp < 3.0f) {
        g1 = c;
        b1 = x;
    } else if (hp >= 3.0f && hp < 4.0f) {
        g1 = x;
        b1 = c;
    } else if (hp >= 4.0f && hp < 5.0f) {
        r1 = x;
        b1 = c;
    } else if (hp >= 5.0f && hp < 6.0f) {
        r1 = c;
        b1 = x;
    }

    Color_init(r1 + m, g1 + m, b1 + m, a, dest);
}

bool Color_fromHex(const char *hex, Color *dest) {
    if (hex == nullptr || dest == nullptr)
        return false;

    if (*hex == '#')
        hex++;

    size_t len = strlen(hex);
    unsigned int r = 0;
    unsigned int g = 0;
    unsigned int b = 0;
    unsigned int a = 255;

    if (len == 6) {
        if (sscanf(hex, "%02x%02x%02x", &r, &g, &b) != 3)
            return false;
    } else if (len == 8) {
        if (sscanf(hex, "%02x%02x%02x%02x", &r, &g, &b, &a) != 4)
            return false;
    } else if (len == 3) {
        if (sscanf(hex, "%1x%1x%1x", &r, &g, &b) != 3)
            return false;
        r = r * 17;
        g = g * 17;
        b = b * 17;
    } else {
        return false;
    }

    Color_init(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f, dest);
    return true;
}

// PROJECTIONS / CONVERSIONS
// ============================================================================

uint32_t Color_toARGB(const Color *color) {
    if (color == nullptr)
        return 0;

    uint32_t a = (uint32_t) lroundf((*color).a * 255.0f);
    uint32_t r = (uint32_t) lroundf((*color).r * 255.0f);
    uint32_t g = (uint32_t) lroundf((*color).g * 255.0f);
    uint32_t b = (uint32_t) lroundf((*color).b * 255.0f);

    return (a << 24) | (r << 16) | (g << 8) | b;
}

uint32_t Color_toRGBA32(const Color *color) {
    if (color == nullptr)
        return 0;

    uint32_t r = (uint32_t) lroundf((*color).r * 255.0f);
    uint32_t g = (uint32_t) lroundf((*color).g * 255.0f);
    uint32_t b = (uint32_t) lroundf((*color).b * 255.0f);
    uint32_t a = (uint32_t) lroundf((*color).a * 255.0f);

    return (r << 24) | (g << 16) | (b << 8) | a;
}

void Color_toHSV(const Color *color, float *outH, float *outS, float *outV) {
    if (color == nullptr) {
        if (outH) *outH = 0.0f;
        if (outS) *outS = 0.0f;
        if (outV) *outV = 0.0f;
        return;
    }

    float r = (*color).r;
    float g = (*color).g;
    float b = (*color).b;

    float maxVal = fmaxf(r, fmaxf(g, b));
    float minVal = fminf(r, fminf(g, b));
    float delta = maxVal - minVal;

    float h = 0.0f;
    float s = (maxVal > 0.00001f) ? (delta / maxVal) : 0.0f;
    float v = maxVal;

    if (delta > 0.00001f) {
        if (r >= maxVal) {
            h = 60.0f * (g - b) / delta;
            if (h < 0.0f)
                h += 360.0f;
        } else if (g >= maxVal) {
            h = 60.0f * (2.0f + (b - r) / delta);
        } else {
            h = 60.0f * (4.0f + (r - g) / delta);
        }
    }

    if (outH != nullptr)
        *outH = h;
    if (outS != nullptr)
        *outS = s;
    if (outV != nullptr)
        *outV = v;
}

void Color_toHSL(const Color *color, float *outH, float *outS, float *outL) {
    if (color == nullptr) {
        if (outH) *outH = 0.0f;
        if (outS) *outS = 0.0f;
        if (outL) *outL = 0.0f;
        return;
    }

    float r = (*color).r;
    float g = (*color).g;
    float b = (*color).b;

    float maxVal = fmaxf(r, fmaxf(g, b));
    float minVal = fminf(r, fminf(g, b));
    float delta = maxVal - minVal;

    float l = (maxVal + minVal) * 0.5f;
    float s = 0.0f;
    float h = 0.0f;

    if (delta > 0.00001f) {
        s = (l > 0.5f) ? (delta / (2.0f - maxVal - minVal)) : (delta / (maxVal + minVal));
        if (r >= maxVal) {
            h = 60.0f * (g - b) / delta;
            if (h < 0.0f)
                h += 360.0f;
        } else if (g >= maxVal) {
            h = 60.0f * (2.0f + (b - r) / delta);
        } else {
            h = 60.0f * (4.0f + (r - g) / delta);
        }
    }

    if (outH != nullptr)
        *outH = h;
    if (outS != nullptr)
        *outS = s;
    if (outL != nullptr)
        *outL = l;
}

void Color_toHex(const Color *color, bool includeAlpha, char *dest, size_t destSize) {
    if (color == nullptr || dest == nullptr || destSize == 0)
        return;

    uint32_t r = (uint32_t) lroundf((*color).r * 255.0f);
    uint32_t g = (uint32_t) lroundf((*color).g * 255.0f);
    uint32_t b = (uint32_t) lroundf((*color).b * 255.0f);
    uint32_t a = (uint32_t) lroundf((*color).a * 255.0f);

    if (includeAlpha)
        snprintf(dest, destSize, "#%02x%02x%02x%02x", r, g, b, a);
    else
        snprintf(dest, destSize, "#%02x%02x%02x", r, g, b);
}

// CORE FUNCTIONS
// ============================================================================

void Color_lerp(const Color *start, const Color *end, float t, Color *dest) {
    if (start == nullptr || end == nullptr || dest == nullptr)
        return;

    t = clampF(t, 0.0f, 1.0f);
    float r = (*start).r + ((*end).r - (*start).r) * t;
    float g = (*start).g + ((*end).g - (*start).g) * t;
    float b = (*start).b + ((*end).b - (*start).b) * t;
    float a = (*start).a + ((*end).a - (*start).a) * t;
    Color_init(r, g, b, a, dest);
}

bool Color_equals(const Color *a, const Color *b) {
    if (a == b)
        return true;
    if (a == nullptr || b == nullptr)
        return false;

    return (fabsf((*a).r - (*b).r) < 0.0001f &&
            fabsf((*a).g - (*b).g) < 0.0001f &&
            fabsf((*a).b - (*b).b) < 0.0001f &&
            fabsf((*a).a - (*b).a) < 0.0001f);
}

// SETTERS
// ============================================================================

void Color_setRGBA(Color *color, float r, float g, float b, float a) {
    if (color == nullptr)
        return;
    Color_init(r, g, b, a, color);
}

void Color_setARGB(Color *color, uint32_t argb) {
    if (color == nullptr)
        return;
    Color_fromARGB(argb, color);
}

void Color_setHSV(Color *color, float h, float s, float v) {
    if (color == nullptr)
        return;
    Color_fromHSV(h, s, v, (*color).a, color);
}

void Color_setHSL(Color *color, float h, float s, float l) {
    if (color == nullptr)
        return;
    Color_fromHSL(h, s, l, (*color).a, color);
}

bool Color_setHex(Color *color, const char *hex) {
    if (color == nullptr)
        return false;
    return Color_fromHex(hex, color);
}

void Color_setR(Color *color, float r) {
    if (color == nullptr)
        return;
    (*color).r = clampF(r, 0.0f, 1.0f);
}

void Color_setG(Color *color, float g) {
    if (color == nullptr)
        return;
    (*color).g = clampF(g, 0.0f, 1.0f);
}

void Color_setB(Color *color, float b) {
    if (color == nullptr)
        return;
    (*color).b = clampF(b, 0.0f, 1.0f);
}

void Color_setA(Color *color, float a) {
    if (color == nullptr)
        return;
    (*color).a = clampF(a, 0.0f, 1.0f);
}

// GETTERS
// ============================================================================

float Color_getR(const Color *color) {
    if (color == nullptr)
        return 0.0f;
    return (*color).r;
}

float Color_getG(const Color *color) {
    if (color == nullptr)
        return 0.0f;
    return (*color).g;
}

float Color_getB(const Color *color) {
    if (color == nullptr)
        return 0.0f;
    return (*color).b;
}

float Color_getA(const Color *color) {
    if (color == nullptr)
        return 0.0f;
    return (*color).a;
}

uint32_t Color_getARGB(const Color *color) {
    return Color_toARGB(color);
}

void Color_getHSV(const Color *color, float *outH, float *outS, float *outV) {
    Color_toHSV(color, outH, outS, outV);
}

void Color_getHSL(const Color *color, float *outH, float *outS, float *outL) {
    Color_toHSL(color, outH, outS, outL);
}

// STATIC PRESETS
// ============================================================================

Color Color_white(void) {
    Color c = {1.0f, 1.0f, 1.0f, 1.0f};
    return c;
}

Color Color_black(void) {
    Color c = {0.0f, 0.0f, 0.0f, 1.0f};
    return c;
}

Color Color_clear(void) {
    Color c = {0.0f, 0.0f, 0.0f, 0.0f};
    return c;
}

Color Color_red(void) {
    Color c = {1.0f, 0.0f, 0.0f, 1.0f};
    return c;
}

Color Color_green(void) {
    Color c = {0.0f, 1.0f, 0.0f, 1.0f};
    return c;
}

Color Color_blue(void) {
    Color c = {0.0f, 0.0f, 1.0f, 1.0f};
    return c;
}
