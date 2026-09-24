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
static Image *s_cards[SCROLL_SCENE_CARD_COUNT] = { nullptr };
static Picture *s_pics[SCROLL_SCENE_CARD_COUNT] = { nullptr };

static Image *makeCard(int idx) {
    const uint32_t w = SCROLL_SCENE_CARD_PX_W;
    const uint32_t h = SCROLL_SCENE_CARD_PX_H;
    uint8_t *rgba = (uint8_t*) malloc((size_t) w * (size_t) h * 4u);
    if (!rgba)
        return nullptr;
    uint8_t r0 = (uint8_t) ((idx * SCROLL_SCENE_CARD_HUE_STEP_R + SCROLL_SCENE_CARD_HUE_BASE_R) % 256);
    uint8_t g0 = (uint8_t) ((idx * SCROLL_SCENE_CARD_HUE_STEP_G + SCROLL_SCENE_CARD_HUE_BASE_G) % 256);
    uint8_t b0 = (uint8_t) ((idx * SCROLL_SCENE_CARD_HUE_STEP_B + SCROLL_SCENE_CARD_HUE_BASE_B) % 256);
    for (uint32_t y = 0; y < h; y++) {
        for (uint32_t x = 0; x < w; x++) {
            uint8_t *p = rgba + ((size_t) y * w + x) * 4u;
            float t = (float) y / (float) (h - 1u);
            bool stripe = ((x + y) / SCROLL_SCENE_CARD_STRIPE_PERIOD) % 2u == 0u;
            uint8_t add = stripe ? (uint8_t) SCROLL_SCENE_CARD_STRIPE_ADD : 0u;
            float ramp = SCROLL_SCENE_CARD_GRADIENT_MIN + SCROLL_SCENE_CARD_GRADIENT_MAX * t;
            p[0] = (uint8_t) (r0 * ramp + add);
            p[1] = (uint8_t) (g0 * ramp + add);
            p[2] = (uint8_t) (b0 * ramp + add);
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
    if (slot < 0 || slot >= SCROLL_SCENE_CARD_COUNT)
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
    ScrollPanel *outer = ScrollPanel_2(SCROLL_SCENE_OUTER_VIEW_W, SCROLL_SCENE_OUTER_VIEW_H);
    Panel *outerContent = Panel_0();
    Panel_setSize(outerContent, SCROLL_SCENE_OUTER_CARD_W, SCROLL_SCENE_OUTER_CONTENT_H);
    Panel_setBackgroundColor(outerContent, 0x1E1E24FFu);
    ScrollPanel_setContent(outer, outerContent);
    ScrollPanel *inner = ScrollPanel_2(SCROLL_SCENE_INNER_VIEW_W, SCROLL_SCENE_INNER_VIEW_H);
    Panel *innerContent = Panel_0();
    Panel_setSize(innerContent, SCROLL_SCENE_INNER_CARD_W, SCROLL_SCENE_INNER_CONTENT_H);
    Panel_setBackgroundColor(innerContent, 0x23232BFFu);
    ScrollPanel_setContent(inner, innerContent);
    Panel *innerBase = &(*inner).base;
    Panel_setLocation(innerBase, SCROLL_SCENE_INNER_X, SCROLL_SCENE_INNER_Y);
    Panel_setBackgroundColor(innerBase, 0x101014FFu);

    float ow = SCROLL_SCENE_OUTER_CARD_W;
    float oh = SCROLL_SCENE_OUTER_CARD_H;
    float op = SCROLL_SCENE_OUTER_PITCH;
    float iw = SCROLL_SCENE_INNER_CARD_W;
    float ih = SCROLL_SCENE_INNER_CARD_H;
    float ip = SCROLL_SCENE_INNER_PITCH;
    makePic(0, 0.0f, 0.0f, ow, oh, outerContent);
    makePic(1, 0.0f, op, ow, oh, outerContent);
    makePic(2, 0.0f, 2.0f * op, ow, oh, outerContent);
    Panel_addContainer(outerContent, innerBase);
    makePic(3, 0.0f, SCROLL_SCENE_TAIL_Y, ow, SCROLL_SCENE_TAIL_CARD_H, outerContent);
    makePic(4, 0.0f, 0.0f, iw, ih, innerContent);
    makePic(5, 0.0f, ip, iw, ih, innerContent);
    makePic(6, 0.0f, 2.0f * ip, iw, ih, innerContent);

    ScrollPanel_verticalScroll_setShortLengthLimit(outer, SCROLL_SCENE_SHORT_LIMIT);
    ScrollPanel_verticalScroll_setShortLengthLimit(inner, SCROLL_SCENE_SHORT_LIMIT);
    ScrollPanel_layoutBars(outer);
    ScrollPanel_layoutBars(inner);
    s_outer = outer;
    s_inner = inner;
}

void ScrollScene_free(void) {
    for (int i = 0; i < SCROLL_SCENE_CARD_COUNT; i++) {
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

void ScrollScene_setSmooth(bool smooth, float friction) {
    if (!s_outer || !s_inner)
        return;
    int mode = smooth ? SCROLL_BAR_SMOOTH : SCROLL_BAR_STEP;
    ScrollPanel_verticalScroll_setScrollMode(s_outer, mode);
    ScrollPanel_verticalScroll_setScrollMode(s_inner, mode);
    ScrollPanel_verticalScroll_setScrollFriction(s_outer, friction);
    ScrollPanel_verticalScroll_setScrollFriction(s_inner, friction);
}

void ScrollScene_scrollInput(float dy, uint64_t nowMs) {
    if (!s_outer)
        return;
    ScrollPanel_scrollInputAt(s_outer, 0.0f, dy, nowMs);
    ScrollPanel_tick(s_outer, nowMs);
    ScrollPanel_tick(s_inner, nowMs);
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

void ScrollScene_setOffsets(float outerY, float innerY, uint64_t nowMs) {
    if (!s_outer || !s_inner)
        return;
    ScrollPanel_setOffsetAt(s_outer, 0.0f, outerY, nowMs);
    ScrollPanel_setOffsetAt(s_inner, 0.0f, innerY, nowMs);
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
    Rectangle outerRect = { SCROLL_SCENE_VIEW_X, SCROLL_SCENE_VIEW_Y,
                            SCROLL_SCENE_OUTER_VIEW_W, SCROLL_SCENE_OUTER_VIEW_H };
    Panel *innerBase = &(*s_inner).base;
    ScrollPanel_paint(s_outer, &outerRect, innerBase);
    Panel *outerContent = ScrollPanel_getContentPanel(s_outer);
    float ix = outerRect.x + GraphicsComponent_getX(&(*outerContent).component)
        + GraphicsComponent_getX(&(*innerBase).component);
    float iy = outerRect.y + GraphicsComponent_getY(&(*outerContent).component)
        + GraphicsComponent_getY(&(*innerBase).component);
    Rectangle innerRect = { ix, iy, SCROLL_SCENE_INNER_VIEW_W, SCROLL_SCENE_INNER_VIEW_H };
    Graphics_clip(&outerRect);
    ScrollPanel_paint(s_inner, &innerRect, nullptr);
    Graphics_clip(nullptr);
}
