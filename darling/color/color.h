#ifndef DARLING_COLOR_COLOR_H
#define DARLING_COLOR_COLOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "c23/constructor.h"

#ifdef __cplusplus
extern "C" {
#endif

// darling/color/color.h — multi-format unified color representation.
// Stores canonical RGBA normalized float components [0.0, 1.0] while
// supporting lossless projection into and ingestion from ARGB32, RGBA32,
// HSV, HSL, and hexadecimal string formats.

typedef struct Color {
    float r;      // Red component [0.0f, 1.0f]
    float g;      // Green component [0.0f, 1.0f]
    float b;      // Blue component [0.0f, 1.0f]
    float a;      // Alpha component [0.0f, 1.0f]
} Color;

// Constructors:
//   Color()                           — default opaque white (1, 1, 1, 1)
//   Color(argb)                       — packed 0xAARRGGBB
//   Color(rgb, a)                     — packed 0x00RRGGBB + normalized alpha
//   Color(r, g, b)                    — normalized floats, alpha = 1.0
//   Color(r, g, b, a)                 — normalized floats
Color *Color_0(void);
Color *Color_1(uint32_t argb);
Color *Color_2(uint32_t rgb, float a);
Color *Color_3(float r, float g, float b);
Color *Color_4(float r, float g, float b, float a);

#define Color(...) CONSTRUCTOR_DISPATCH(Color, __VA_ARGS__)

bool Color_init(float r, float g, float b, float a, Color *dest);
void Color_free(Color *color);

// Factory Initializers (Dest-Last):
void Color_fromARGB(uint32_t argb, Color *dest);
void Color_fromRGBA(float r, float g, float b, float a, Color *dest);
void Color_fromHSV(float h, float s, float v, float a, Color *dest);
void Color_fromHSL(float h, float s, float l, float a, Color *dest);
bool Color_fromHex(const char *hex, Color *dest);

// Projections / Conversions:
uint32_t Color_toARGB(const Color *color);
uint32_t Color_toRGBA32(const Color *color);
void Color_toHSV(const Color *color, float *outH, float *outS, float *outV);
void Color_toHSL(const Color *color, float *outH, float *outS, float *outL);
void Color_toHex(const Color *color, bool includeAlpha, char *dest, size_t destSize);

// Core Functions:
void Color_lerp(const Color *start, const Color *end, float t, Color *dest);
bool Color_equals(const Color *a, const Color *b);

// Setters:
void Color_setRGBA(Color *color, float r, float g, float b, float a);
void Color_setARGB(Color *color, uint32_t argb);
void Color_setHSV(Color *color, float h, float s, float v);
void Color_setHSL(Color *color, float h, float s, float l);
bool Color_setHex(Color *color, const char *hex);
void Color_setR(Color *color, float r);
void Color_setG(Color *color, float g);
void Color_setB(Color *color, float b);
void Color_setA(Color *color, float a);

// Getters:
float Color_getR(const Color *color);
float Color_getG(const Color *color);
float Color_getB(const Color *color);
float Color_getA(const Color *color);
uint32_t Color_getARGB(const Color *color);
void Color_getHSV(const Color *color, float *outH, float *outS, float *outV);
void Color_getHSL(const Color *color, float *outH, float *outS, float *outL);

// Static Color Constants:
Color Color_white(void);
Color Color_black(void);
Color Color_clear(void);
Color Color_red(void);
Color Color_green(void);
Color Color_blue(void);

#ifdef __cplusplus
}
#endif

#endif // DARLING_COLOR_COLOR_H
