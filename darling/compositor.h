#ifndef DARLING_COMPOSITOR_H
#define DARLING_COMPOSITOR_H

#include <stdbool.h>
#include <stdint.h>
#include "window/window.h"

// darling/compositor.h — Retained-mode UI compositor bridge to Vulkan swapchain.
//
// Bridges darling UI panels, IOSurface overlays, and 3D scenes onto the
// OS-stable window and Vulkan presentation pipeline.
//
// STACK LAW (front to back): scrollbar IOSurface -> 1st-gen children
// IOSurfaces -> window CAMetalLayer. Layer 1 = window, layer 2 =
// contentPanel placeholder, layer 3 = first-gen panels (own surface each;
// deeper nesting paints inside the parent surface). Vulkan renders INSIDE
// the IOSurfaces only and never presents the window — WindowServer owns
// the composite. Resize moves layers via anchors; no repaint.

// Initialize the darling compositor for the given window and register
// frame rendering callbacks on the Vulkan presentation engine.
void Darling_initCompositor(Window *window);

// Shutdown compositor modules and unregister frame callbacks.
void Darling_shutdownCompositor(void);

// Pre-frame callback invoked before swapchain acquisition (runs offscreen IOSurface passes).
void Darling_preFrame(Window *window, int drawW, int drawH, void *userdata);

// Batch-drain query: true when the IOSurface re-record batch has no flight
// pending. Present-on-demand loops gate their tree-dirty clear on this so a
// timed-out batch's unexported work is retried next tick, never dropped.
bool Darling_compositorSettled(void);

// Frame rendering callback invoked by Vk_clearPresent inside active swapchain pass.
void Darling_renderFrame(void *cmdBuffer, int drawW, int drawH, void *userdata);

#endif
