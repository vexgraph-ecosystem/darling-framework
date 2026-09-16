#include "annotation/overview.h"

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "darling/color/color.h"

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: ColorTest (_tests/darling/color_test.c)
 * LEVEL: L3 — Module Code (headless verification harness)
 * ============================================================================
 * Verification suite for Color: format independence across ARGB32, RGBA32,
 * HSV, HSL, and Hex conversions while preserving identical underlying color.
 *
 * STRUCT FIELDS: none — procedural test harness.
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - main(void)
 * ============================================================================
 */

int main(void) {
    printf("=== Running Color Test Suite ===\n");

    // 1. Null safety
    assert(Color_getR(nullptr) == 0.0f);
    assert(Color_getG(nullptr) == 0.0f);
    assert(Color_getB(nullptr) == 0.0f);
    assert(Color_getA(nullptr) == 0.0f);
    assert(Color_getARGB(nullptr) == 0);
    Color_setRGBA(nullptr, 1.0f, 1.0f, 1.0f, 1.0f);
    Color_setARGB(nullptr, 0xFFFFFFFFu);
    Color_setHSV(nullptr, 180.0f, 0.5f, 0.5f);
    Color_free(nullptr);

    // 2. Constructors
    Color *c0 = Color_0();
    assert(c0 != nullptr);
    assert(Color_getR(c0) == 1.0f);
    assert(Color_getG(c0) == 1.0f);
    assert(Color_getB(c0) == 1.0f);
    assert(Color_getA(c0) == 1.0f);
    assert(Color_getARGB(c0) == 0xFFFFFFFFu);
    Color_free(c0);

    Color *c1 = Color_1(0xFF112233u);
    assert(Color_getARGB(c1) == 0xFF112233u);
    Color_free(c1);

    Color *c3 = Color_3(1.0f, 0.0f, 0.0f);
    assert(Color_getR(c3) == 1.0f);
    assert(Color_getG(c3) == 0.0f);
    assert(Color_getB(c3) == 0.0f);
    assert(Color_getA(c3) == 1.0f);
    assert(Color_getARGB(c3) == 0xFFFF0000u);
    Color_free(c3);

    // 3. Format Conversion: HSV roundtrip (same value in different formats)
    Color red = Color_red();
    float h = 0.0f, s = 0.0f, v = 0.0f;
    Color_toHSV(&red, &h, &s, &v);
    assert(fabsf(h - 0.0f) < 0.01f);
    assert(fabsf(s - 1.0f) < 0.01f);
    assert(fabsf(v - 1.0f) < 0.01f);

    Color reconstructedRed;
    Color_fromHSV(h, s, v, 1.0f, &reconstructedRed);
    assert(Color_equals(&red, &reconstructedRed));

    // Green HSV test
    Color green = Color_green();
    Color_toHSV(&green, &h, &s, &v);
    assert(fabsf(h - 120.0f) < 0.01f);
    assert(fabsf(s - 1.0f) < 0.01f);
    assert(fabsf(v - 1.0f) < 0.01f);

    // Blue HSV test
    Color blue = Color_blue();
    Color_toHSV(&blue, &h, &s, &v);
    assert(fabsf(h - 240.0f) < 0.01f);
    assert(fabsf(s - 1.0f) < 0.01f);
    assert(fabsf(v - 1.0f) < 0.01f);

    // 4. Format Conversion: HSL roundtrip
    float l = 0.0f;
    Color_toHSL(&blue, &h, &s, &l);
    assert(fabsf(h - 240.0f) < 0.01f);
    assert(fabsf(s - 1.0f) < 0.01f);
    assert(fabsf(l - 0.5f) < 0.01f);

    Color reconstructedBlue;
    Color_fromHSL(h, s, l, 1.0f, &reconstructedBlue);
    assert(Color_equals(&blue, &reconstructedBlue));

    // 5. Format Conversion: Hex string roundtrip
    Color cHex;
    assert(Color_fromHex("#336699", &cHex) == true);
    char hexBuf[16] = {0};
    Color_toHex(&cHex, false, hexBuf, sizeof(hexBuf));
    assert(strcmp(hexBuf, "#336699") == 0);

    // 6. Color mutation: setHSV changes value, reflected across ARGB and Hex
    Color col;
    Color_init(0.0f, 0.0f, 0.0f, 1.0f, &col);
    Color_setHSV(&col, 60.0f, 1.0f, 1.0f); // Yellow: R=1, G=1, B=0
    assert(fabsf(Color_getR(&col) - 1.0f) < 0.01f);
    assert(fabsf(Color_getG(&col) - 1.0f) < 0.01f);
    assert(fabsf(Color_getB(&col) - 0.0f) < 0.01f);
    assert(Color_getARGB(&col) == 0xFFFFFF00u);

    // 7. Lerp
    Color black = Color_black();
    Color white = Color_white();
    Color gray;
    Color_lerp(&black, &white, 0.5f, &gray);
    assert(fabsf(Color_getR(&gray) - 0.5f) < 0.01f);
    assert(fabsf(Color_getG(&gray) - 0.5f) < 0.01f);
    assert(fabsf(Color_getB(&gray) - 0.5f) < 0.01f);

    printf("=== Color Test Suite Passed! ===\n");
    return 0;
}
