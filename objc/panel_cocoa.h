#ifndef OBJC_PANEL_COCOA_H
#define OBJC_PANEL_COCOA_H

#include <stdbool.h>

// objc/panel_cocoa.h — Metal pane compositor.
//
// Two backing kinds:
//   - PANE (PanelCocoa_newMetal): a CAMetalLayer + VkPane swapchain — the
//     "pane of glass" for DIRECT scenes; the layer is parented into AppKit
//     and the pane presents its own chain (fixed pixel size, never rebuilt
//     on window resize — the Pane-of-Glass Law managed exception).
//   - BOARD (PanelCocoa_newBoard): the two named full-window layers —
//     scene (bottom) / content (top) — as RETAINED OFFSCREEN targets: a
//     fixed-pixel-size VkLayer dual-flight chain, never a CALayer, never
//     in the window tree. The window's single on-screen CAMetalLayer is
//     the Frame seam canvas; the seam pass composites the published board
//     images in z-order (scene below, content above) per the Window
//     Compositing Layer Order Law.
//
// Rendering model (retained-mode UI, every panel a Vulkan rect):
//   - Board panes (scene/content via PanelCocoa_newBoard) paint their whole
//     subtree into their retained offscreen target every visit.
//   - Child panes (nested DIRECT scenes via PanelCocoa_newMetal) paint their
//     panel into their own chain every present.
//   - Plain UI paints into the content board pass (paintChildIntoPass) — no
//     backing.
//
// Lifecycle:
//   Panel *p = Panel();                    // existing darling API, unchanged
//   PanelCocoa *pc = PanelCocoa_newBoard(p, 1600, 1200); // retained offscreen board
//   PanelCocoa_setSize(pc, 1600, 1200);   // board resize (flight-target rebuild)
//   CALayer *layer = PanelCocoa_layer(pc); // pane: add to window's layer tree;
//                                          // board: nullptr (offscreen target)
//
// The Panel handle is what darling code already holds; PanelCocoa is the
// platform-specific backing that makes it display natively.

typedef struct PanelCocoa PanelCocoa;

// Attach CAMetalLayer "pane of glass" backing to an existing Panel: the
// panel owns its OWN Vulkan swapchain (registered with VkPane) and presents
// independently of the window board — live window resize only moves the
// layer frame (WindowServer composites), the pane never rebuilds. Size is
// the pane's fixed pixel size. Returns nullptr on failure.
PanelCocoa *PanelCocoa_newMetal(void *panel, int width, int height);

// Attach board backing to one of the two named full-window panels (scene or
// content): a RETAINED OFFSCREEN target — a fixed-pixel-size VkLayer
// dual-flight chain, never a CALayer, never in the window tree. The single
// on-screen CAMetalLayer (the Frame seam canvas) composites the published
// board images in z-order (scene below, content above) per the Window
// Compositing Layer Order Law. Boards never present; the pixel size is FIXED
// at register time (the Pane-of-Glass Law — no rebuild on window resize).
// Returns nullptr on failure.
PanelCocoa *PanelCocoa_newBoard(void *panel, int width, int height);

// True when the backing is a CAMetalLayer pane (Vulkan swapchain host)
// rather than an IOSurface.
bool PanelCocoa_isMetal(const PanelCocoa *pc);

// True when the backing is a full-window board (scene/content), not a
// fixed child pane. Boards resize with the window; panes never do.
bool PanelCocoa_isBoard(const PanelCocoa *pc);

// Live-resize pin for boards only (thread 0): INERT — boards own no
// CALayer (retained offscreen VkLayer targets); the Frame seam layer is the
// window's only on-screen layer and its frame tracks the window natively
// (autoresizingMask + the Native Pixel Law drawableSize contract). Kept as
// a documented no-op for bridge compatibility.
void PanelCocoa_setLiveResizingAll(bool live);

// The registered backing index: the VkPane chain index for a pane, the
// VkLayer index for a board; -1 when not backed.
int PanelCocoa_chain(const PanelCocoa *pc);

// Free the Metal backing. The Panel itself is owned by the caller.
void PanelCocoa_free(PanelCocoa *pc);

// Resize the backing: pane -> VkPane chain rebuild; board -> VkLayer
// flight-target rebuild. Both no-op when unchanged (fixed panes and boards
// never rebuild on window resize — the Pane-of-Glass Law). Returns false on
// failure (old chain survives).
bool PanelCocoa_setSize(PanelCocoa *pc, int width, int height);

// The CAMetalLayer for a PANE. Add it to the window's content view layer
// (or a parent panel's layer). WindowServer composites it with the
// WindowServer transaction — no CPU blit needed. BOARDS return nullptr
// (retained offscreen VkLayer targets — never in the window tree).
void *PanelCocoa_layer(PanelCocoa *pc);  // CALayer * (void * to avoid ObjC in C headers)

// Current backing size.
int PanelCocoa_width(const PanelCocoa *pc);
int PanelCocoa_height(const PanelCocoa *pc);

// Set darling anchor+pivot settings on the CALayer.
//   anchor (9-grid):  maps to layer.autoresizingMask (resize tracking).
//   pivot (5 points): maps to layer.anchorPoint + layer.contentsGravity.
void PanelCocoa_setAnchors(PanelCocoa *pc, int anchor, int pivot);

// Lookup: retrieve the PanelCocoa backing for a Panel. Returns nullptr if the
// panel has no Metal backing. Used by the window bridge.
void *PanelCocoa_fromPanel(void *panel); // Panel * → PanelCocoa *

#endif
