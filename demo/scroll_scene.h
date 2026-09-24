// demo/scroll_scene.h — the scroll probe scene (outer + nested ScrollPanels).
//
// Procedural probe scaffold (the gallery.c precedent: file-static state, no
// struct): one outer ScrollPanel (400x260 viewport, 400x700 picture column)
// with a nested inner ScrollPanel (360x160 viewport, 360x412 picture column)
// riding in the outer content, over seven procedural cards (zero asset
// files). Both the headless BMP demo and the umbrella live window drive
// this one scene — no duplicated composition.

#ifndef DARLING_DEMO_SCROLL_SCENE_H
#define DARLING_DEMO_SCROLL_SCENE_H

#include <stdbool.h>
#include <stdint.h>

// Scene magnitudes (the No Hardcoding Law): every dimension, pitch, and
// procedural-card term is named once here — the .c carries no bare literals.
#define SCROLL_SCENE_CARD_COUNT          7
#define SCROLL_SCENE_CARD_PX_W           96u
#define SCROLL_SCENE_CARD_PX_H           72u
#define SCROLL_SCENE_CARD_HUE_STEP_R     67
#define SCROLL_SCENE_CARD_HUE_STEP_G     131
#define SCROLL_SCENE_CARD_HUE_STEP_B     197
#define SCROLL_SCENE_CARD_HUE_BASE_R     40
#define SCROLL_SCENE_CARD_HUE_BASE_G     90
#define SCROLL_SCENE_CARD_HUE_BASE_B     140
#define SCROLL_SCENE_CARD_GRADIENT_MIN   0.45f
#define SCROLL_SCENE_CARD_GRADIENT_MAX   0.55f
#define SCROLL_SCENE_CARD_STRIPE_ADD     26
#define SCROLL_SCENE_CARD_STRIPE_PERIOD  12u

#define SCROLL_SCENE_VIEW_X              40.0f
#define SCROLL_SCENE_VIEW_Y              40.0f
#define SCROLL_SCENE_OUTER_VIEW_W        400.0f
#define SCROLL_SCENE_OUTER_VIEW_H        260.0f
#define SCROLL_SCENE_OUTER_CARD_W        400.0f
#define SCROLL_SCENE_OUTER_CARD_H        132.0f
#define SCROLL_SCENE_OUTER_GAP           4.0f
#define SCROLL_SCENE_TAIL_CARD_H         120.0f
#define SCROLL_SCENE_TAIL_GAP            8.0f

#define SCROLL_SCENE_INNER_VIEW_W        360.0f
#define SCROLL_SCENE_INNER_VIEW_H        160.0f
#define SCROLL_SCENE_INNER_CARD_W        360.0f
#define SCROLL_SCENE_INNER_CARD_H        132.0f
#define SCROLL_SCENE_INNER_GAP           8.0f
#define SCROLL_SCENE_INNER_X             20.0f

#define SCROLL_SCENE_SHORT_LIMIT         0.15f
#define SCROLL_SCENE_FB_W                480
#define SCROLL_SCENE_FB_H                360

// Derived pitches (never re-typed): outer row pitch, inner row pitch, the
// nested panel's y, and the tail card's y — all from the named magnitudes.
#define SCROLL_SCENE_OUTER_PITCH   (SCROLL_SCENE_OUTER_CARD_H + SCROLL_SCENE_OUTER_GAP)
#define SCROLL_SCENE_INNER_PITCH   (SCROLL_SCENE_INNER_CARD_H + SCROLL_SCENE_INNER_GAP)
#define SCROLL_SCENE_INNER_Y       (2.0f * SCROLL_SCENE_OUTER_PITCH + SCROLL_SCENE_OUTER_CARD_H + SCROLL_SCENE_TAIL_GAP)
#define SCROLL_SCENE_TAIL_Y        (SCROLL_SCENE_INNER_Y + SCROLL_SCENE_INNER_VIEW_H + SCROLL_SCENE_TAIL_GAP)
#define SCROLL_SCENE_OUTER_CONTENT_H (SCROLL_SCENE_TAIL_Y + SCROLL_SCENE_TAIL_CARD_H)
#define SCROLL_SCENE_INNER_CONTENT_H (3.0f * SCROLL_SCENE_INNER_CARD_H + 2.0f * SCROLL_SCENE_INNER_GAP)

// Build the scene (images, pictures, both panels). Idempotent-ish: free
// first if rebuilt.
void ScrollScene_build(void);

// Free the scene (pictures + images; bars are arena-lifetime by design).
void ScrollScene_free(void);

// Overlay bars on/off on both panels (classic vs auto-hide).
void ScrollScene_setOverlay(bool overlay);

// Scroll behavior: smooth momentum (friction) on both bars, or step.
void ScrollScene_setSmooth(bool smooth, float friction);

// Scroll verbs (explicit caller clock, no threads).
void ScrollScene_scrollOuter(float dy, uint64_t nowMs);
void ScrollScene_scrollChained(float dy, uint64_t nowMs);
// Wheel/trackpad input through each bar's behavior (sensitivity + momentum).
void ScrollScene_scrollInput(float dy, uint64_t nowMs);
void ScrollScene_toEnd(uint64_t nowMs);
// Absolute scrub (the window loop interpolates these every frame).
void ScrollScene_setOffsets(float outerY, float innerY, uint64_t nowMs);
void ScrollScene_tick(uint64_t nowMs);

// Paint one frame at the live framebuffer size (stagnant layout: the
// viewports never track the window, like the gallery cells).
void ScrollScene_paint(int fbW, int fbH);

#endif
