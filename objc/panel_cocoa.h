#ifndef OBJC_PANEL_COCOA_H
#define OBJC_PANEL_COCOA_H

#include <stdbool.h>

// objc/panel_cocoa.h — retained offscreen board backing (the seam canvas compositor).
//
// Single backing kind:
//   BOARD (PanelCocoa_newBoard): the two named full-window layers —
//   scene (bottom) / content (top) — as RETAINED OFFSCREEN targets: a
//   fixed-pixel-size VkLayer dual-flight chain, never a CALayer, never
//   in the window tree. The window's single on-screen CAMetalLayer is
//   the Frame seam canvas; the seam pass composites the published board
//   images in z-order (scene below, content above) per the Window
//   Compositing Layer Order Law (the Single-Seam Canvas Law).
//
// Rendering model (retained-mode UI, every panel a Vulkan rect):
//   - Board panes (scene/content via PanelCocoa_newBoard) paint their whole
//     subtree into their retained offscreen target every visit.
//   - Scenes are COMPOSITED: they paint into a retained offscreen VkLayer
//     target (VkLayer, own timer) that the canvas samples as a textured
//     quad at the scene's anchor rect — no per-scene CAMetalLayer.
//   - Plain UI paints into the content board pass (paintChildIntoPass) — no
//     backing.
//
// Lifecycle:
//   Panel *p = Panel();                    // existing darling API, unchanged
//   PanelCocoa *pc = PanelCocoa_newBoard(p, 1600, 1200); // retained offscreen board
//   PanelCocoa_setSize(pc, 1600, 1200);   // board resize (flight-target rebuild)
//
// The Panel handle is what darling code already holds; PanelCocoa is the
// platform-specific backing that makes it display natively.

typedef struct PanelCocoa PanelCocoa;

// Attach board backing to one of the two named full-window panels (scene or
// content): a RETAINED OFFSCREEN target — a fixed-pixel-size VkLayer
// dual-flight chain, never a CALayer, never in the window tree. The single
// on-screen CAMetalLayer (the Frame seam canvas) composites the published
// board images in z-order (scene below, content above) per the Window
// Compositing Layer Order Law (the Single-Seam Canvas Law). Boards never
// present; the pixel size is FIXED at register time (no rebuild on window
// resize). Returns nullptr on failure.
PanelCocoa *PanelCocoa_newBoard(void *panel, int width, int height);

// True when the backing is a full-window board (scene/content). Every
// PanelCocoa backing is a board — kept as the symmetric probe for bridge
// compatibility and null-safety.
bool PanelCocoa_isBoard(const PanelCocoa *pc);

// The registered backing index: the VkLayer index for a board; -1 when not
// backed.
int PanelCocoa_chain(const PanelCocoa *pc);

// Free the board backing. The Panel itself is owned by the caller.
void PanelCocoa_free(PanelCocoa *pc);

// Resize the backing: board -> VkLayer flight-target rebuild. No-op when
// unchanged (boards never rebuild on window resize — the Single-Seam Canvas
// Law). Returns false on failure (old chain survives).
bool PanelCocoa_setSize(PanelCocoa *pc, int width, int height);

// Current backing size.
int PanelCocoa_width(const PanelCocoa *pc);
int PanelCocoa_height(const PanelCocoa *pc);

// Lookup: retrieve the PanelCocoa backing for a Panel. Returns nullptr if the
// panel has no board backing. Used by the window bridge.
void *PanelCocoa_fromPanel(void *panel); // Panel * → PanelCocoa *

#endif