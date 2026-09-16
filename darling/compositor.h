#ifndef DARLING_COMPOSITOR_H
#define DARLING_COMPOSITOR_H

#include <stdbool.h>
#include <stdint.h>
#include "darling/frame.h"

// darling/compositor.h — Retained-mode UI compositor bridge to Vulkan swapchain.
//
// Bridges darling UI panels and 3D scenes onto the OS-stable window and
// Vulkan presentation pipeline.
//
// STACK LAW (front to back): content board Metal -> scene board Metal ->
// legacy board Metal (clear-transparent glass). Every panel is a Vulkan
// rect: boards paint their whole subtree into their own VkPane chain,
// nested scenes own child panes. WindowServer composites the layer tree;
// no IOSurface transport remains. Resize moves layers via anchors;
// swapchains rebuild at settle only.

// Initialize the darling compositor for the given frame and register
// frame rendering callbacks on the Vulkan presentation engine. The window
// is resolved from the frame; panes come from the frame alone (the Window
// Decoupling Law: the Window holds zero Panels).
void Darling_initCompositor(Frame *frame);

// Shutdown compositor modules and unregister frame callbacks.
void Darling_shutdownCompositor(void);

// Pre-frame callback invoked before swapchain acquisition (attaches boards
// and child panes, composites the layer tree). The graphvex seam passes its
// own Window* as arg one; the borrowing Frame arrives as userdata and owns
// every pane decision below. A nullptr frame means a dumb window: no-op.
void Darling_preFrame(Window *window, int drawW, int drawH, void *userdata);

// Drain query: true when no pane submit flies. Present-on-demand loops gate
// their tree-dirty clear on this so in-flight pane work is never dropped
// by a clear.
bool Darling_compositorSettled(void);

// Resize gate: true when no pane submit flies. Resize-class work (a
// Texture_replaceRaw that changes dimensions) runs only when idle;
// otherwise the caller defers to a same-size update or skips the tick.
// Headless-safe: true with no flight.
bool Darling_compositorIdleForResize(void);

// Frame rendering callback invoked by Vk_clearPresent inside active swapchain pass.
// userdata is the borrowing Frame (panes resolve from it, never the Window).
void Darling_renderFrame(void *cmdBuffer, int drawW, int drawH, void *userdata);

// Hit-test query: returns true if (px, py) hits any visible child control/panel in p.
// Returns false on empty or transparent areas to allow hit passthrough.
bool Darling_hitTest(Panel *p, float px, float py);

#endif

