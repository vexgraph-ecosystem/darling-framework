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

// Build the scene (images, pictures, both panels). Idempotent-ish: free
// first if rebuilt.
void ScrollScene_build(void);

// Free the scene (pictures + images; bars are arena-lifetime by design).
void ScrollScene_free(void);

// Overlay bars on/off on both panels (classic vs auto-hide).
void ScrollScene_setOverlay(bool overlay);

// Scroll verbs (explicit caller clock, no threads).
void ScrollScene_scrollOuter(float dy, uint64_t nowMs);
void ScrollScene_scrollChained(float dy, uint64_t nowMs);
void ScrollScene_toEnd(uint64_t nowMs);
void ScrollScene_tick(uint64_t nowMs);

// Paint one frame at the live framebuffer size (stagnant layout: the
// viewports never track the window, like the gallery cells).
void ScrollScene_paint(int fbW, int fbH);

#endif
