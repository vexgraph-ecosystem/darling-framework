// demo/scroll_scene.c — the scroll probe scene (shared composition).
//
// Own TU: the scene owns darling Panels (the R4 tree world) and paints them
// through the raster row. Callers (headless demo, umbrella window) only
// drive the verbs below.

#include "demo/scroll_scene.h"

#include "annotation/overview.h"
#include "darling/component.h"
#include "darling/panel/panel.h"
#include "darling/panel/scroll_panel.h"
#include "darling/picture/picture.h"
#include "lang/graphics.h"
#include "lang/graphics_component.h"
#include "lang/image.h"
#include "lang/rect/rectangle.h"

#include <stdlib.h>

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: ScrollScene (demo/scroll_scene.c — shared scroll probe scene)
 * ============================================================================
 * SUMMARY:
 *   Outer ScrollPanel (400x260 over a 400x700 picture column) + nested inner
 *   ScrollPanel (360x160 over a 360x412 picture column) over seven procedural
 *   cards. Verbs script offsets (outer, chained inner-then-bubble, both-end)
 *   and paint one stagnant-layout frame. No window, no threads, no assets.
 *
 * STRUCT FIELDS: none — procedural probe (the gallery.c precedent).
 * ============================================================================
 */

static ScrollPanel *s_outer = nullptr;
static ScrollPanel *s_inner = nullptr;
static Image *s_cards[7] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
static Picture *s_pics[7] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

static Image *makeCard(int idx) {
    const uint32_t w = 96u;
    const uint32_t h = 72u;
    uint8_t *rgba = (uint8_t*) malloc((size_t) w * (size_t) h * 4u);
    if (!rgba)
        return nullptr;
    uint8_t r0 = (uint8_t) ((idx * 67 + 40) % 256);
    uint8_t g0 = (uint8_t) ((idx * 131 + 90) % 256);
    uint8_t b0 = (uint8_t) ((idx * 197 + 140) % 256);
    for (uint32_t y = 0; y < h; y++) {
        for (uint32_t x = 0; x < w; x++) {
            uint8_t *p = rgba + ((size_t) y * w + x) * 4u;
            float t = (float) y / (float) (h - 1u);
            bool stripe = ((x + y) / 12u) % 2u == 0u;
            p[0] = (uint8_t) (r0 * (0.45f + 0.55f * t) + (stripe ? 26 : 0));
            p[1] = (uint8_t) (g0 * (0.45f + 0.55f * t) + (stripe ? 26 : 0));
            p[2] = (uint8_t) (b0 * (0.45f + 0.55f * t) + (stripe ? 26 : 0));
            p[3] = 255;
        }
    }
    Image *img = Image_2(w, h);
    if (img)
        Image_upload(rgba, w, h, img);
    free(rgba);
    return img;
}

static Picture *makePic(int slot, float x, float y, float w, float h, Panel *parent) {
    if (slot < 0 || slot >= 7)
        return nullptr;
    s_cards[slot] = makeCard(slot);
    if (!s_cards[slot])
        return nullptr;
    Picture *pic = Picture_1(s_cards[slot]);
    if (!pic)
        return nullptr;
    Picture_setMode(pic, PICTURE_MODE_ZOOM_FILL);
    Picture_setBackgroundColor(pic, 0x202028FFu);
    Picture_setLocation(pic, x, y);
    Picture_setSize(pic, w, h);
    if (parent)
        Panel_addContainer(parent, &(*pic).base);
    s_pics[slot] = pic;
    return pic;
}

void ScrollScene_build(void) {
    ScrollScene_free();
    ScrollPanel *outer = ScrollPanel_2(400.0f, 260.0f);
    Panel *outerContent = Panel_0();
    Panel_setSize(outerContent, 400.0f, 700.0f);
    Panel_setBackgroundColor(outerContent, 0x1E1E24FFu);
    ScrollPanel_setContent(outer, outerContent);
    ScrollPanel *inner = ScrollPanel_2(360.0f, 160.0f);
    Panel *innerContent = Panel_0();
    Panel_setSize(innerContent, 360.0f, 412.0f);
    Panel_setBackgroundColor(innerContent, 0x23232BFFu);
    ScrollPanel_setContent(inner, innerContent);
    Panel *innerBase = &(*inner).base;
    Panel_setLocation(innerBase, 20.0f, 412.0f);
    Panel_setBackgroundColor(innerBase, 0x101014FFu);

    makePic(0, 0.0f, 0.0f, 400.0f, 132.0f, outerContent);
    makePic(1, 0.0f, 136.0f, 400.0f, 132.0f, outerContent);
    makePic(2, 0.0f, 272.0f, 400.0f, 132.0f, outerContent);
    Panel_addContainer(outerContent, innerBase);
    makePic(3, 0.0f, 580.0f, 400.0f, 120.0f, outerContent);
    makePic(4, 0.0f, 0.0f, 360.0f, 132.0f, innerContent);
    makePic(5, 0.0f, 140.0f, 360.0f, 132.0f, innerContent);
    makePic(6, 0.0f, 280.0f, 360.0f, 132.0f, innerContent);

    ScrollPanel_verticalScroll_setShortLengthLimit(outer, 0.15f);
    ScrollPanel_verticalScroll_setShortLengthLimit(inner, 0.15f);
    ScrollPanel_layoutBars(outer);
    ScrollPanel_layoutBars(inner);
    s_outer = outer;
    s_inner = inner;
}

void ScrollScene_free(void) {
    for (int i = 0; i < 7; i++) {
        if (s_pics[i])
            Picture_free(s_pics[i]);
        s_pics[i] = nullptr;
        if (s_cards[i])
            Image_destroy(s_cards[i]);
        s_cards[i] = nullptr;
    }
    s_outer = nullptr;
    s_inner = nullptr;
}

void ScrollScene_setOverlay(bool overlay) {
    if (!s_outer || !s_inner)
        return;
    ScrollPanel_verticalScroll_setHideWhenUnused(s_outer, overlay);
    ScrollPanel_verticalScroll_setHideWhenUnused(s_inner, overlay);
}

void ScrollScene_scrollOuter(float dy, uint64_t nowMs) {
    if (!s_outer)
        return;
    ScrollPanel_scrollByAt(s_outer, 0.0f, dy, nowMs);
    ScrollPanel_tick(s_outer, nowMs);
    ScrollPanel_tick(s_inner, nowMs);
}

void ScrollScene_scrollChained(float dy, uint64_t nowMs) {
    if (!s_outer || !s_inner)
        return;
    float leftX = 0.0f, leftY = 0.0f;
    ScrollPanel_scrollByChained(s_inner, 0.0f, dy, nowMs, &leftX, &leftY);
    ScrollPanel_scrollByAt(s_outer, leftX, leftY, nowMs);
    ScrollPanel_tick(s_outer, nowMs);
    ScrollPanel_tick(s_inner, nowMs);
}

void ScrollScene_toEnd(uint64_t nowMs) {
    if (!s_outer || !s_inner)
        return;
    ScrollPanel_scrollByAt(s_inner, 0.0f, 500.0f, nowMs);
    ScrollPanel_scrollByAt(s_outer, 0.0f, 500.0f, nowMs);
    ScrollPanel_tick(s_outer, nowMs);
    ScrollPanel_tick(s_inner, nowMs);
}

void ScrollScene_tick(uint64_t nowMs) {
    if (!s_outer || !s_inner)
        return;
    ScrollPanel_tick(s_outer, nowMs);
    ScrollPanel_tick(s_inner, nowMs);
}

void ScrollScene_paint(int fbW, int fbH) {
    if (!s_outer || !s_inner)
        return;
    if (!Graphics_resize((uint32_t) fbW, (uint32_t) fbH))
        return;
    Graphics_clear(0x14141AFFu);
    Rectangle outerRect = { 40.0f, 40.0f, 400.0f, 260.0f };
    Panel *innerBase = &(*s_inner).base;
    ScrollPanel_paint(s_outer, &outerRect, innerBase);
    Panel *outerContent = ScrollPanel_getContentPanel(s_outer);
    float ix = outerRect.x + GraphicsComponent_getX(&(*outerContent).component)
        + GraphicsComponent_getX(&(*innerBase).component);
    float iy = outerRect.y + GraphicsComponent_getY(&(*outerContent).component)
        + GraphicsComponent_getY(&(*innerBase).component);
    Rectangle innerRect = { ix, iy, 360.0f, 160.0f };
    Graphics_clip(&outerRect);
    ScrollPanel_paint(s_inner, &innerRect, nullptr);
    Graphics_clip(nullptr);
}
