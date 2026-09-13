#ifndef OBJC_PANEL_COCOA_H
#define OBJC_PANEL_COCOA_H

#include <stdbool.h>

// objc/panel_cocoa.h — Metal pane compositor.
//
// Each Metal-backed panel owns:
//   - a CAMetalLayer      : AppKit composites this (presentsWithTransaction)
//   - a VkPane chain      : the panel's own Vulkan swapchain
//
// Rendering model (retained-mode UI, every panel a Vulkan rect):
//   - Board panes (scene/content via PanelCocoa_newBoard) paint their whole
//     subtree into their own chain every present.
//   - Child panes (nested scenes via PanelCocoa_newMetal) paint their panel
//     into their own chain every present.
//   - Plain UI paints into the board pass (paintChildIntoPass) — no backing.
//
// Lifecycle:
//   Panel *p = Panel();                    // existing darling API, unchanged
//   PanelCocoa *pc = PanelCocoa_newBoard(p, 640, 400); // full-window board
//   PanelCocoa_setSize(pc, 800, 600);     // pane/board resize (chain rebuild)
//   CALayer *layer = PanelCocoa_layer(pc); // add to window's layer tree
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

// Attach a full-window board backing: a CAMetalLayer + VkPane chain sized
// to the window, owned by the scene or content panel itself (the two named
// boards of the NSWindow -> Metal -> Vulkan-rect-children stack). Unlike a
// fixed pane, a board resizes with the window (VkPane_resize at settle) and
// stretches mid-drag (Resize gravity, restored TopLeft at settle).
// Returns nullptr on failure.
PanelCocoa *PanelCocoa_newBoard(void *panel, int width, int height);

// True when the backing is a CAMetalLayer pane (Vulkan swapchain host)
// rather than an IOSurface.
bool PanelCocoa_isMetal(const PanelCocoa *pc);

// True when the backing is a full-window board (scene/content), not a
// fixed child pane. Boards resize with the window; panes never do.
bool PanelCocoa_isBoard(const PanelCocoa *pc);

// Live-resize flip for boards only (thread 0): true stretches board
// drawables mid-drag (Resize gravity, transaction-decoupled presents),
// false restores the TopLeft transaction-synced pin at settle. Fixed
// panes are untouched — their exact-size drawables stay TopLeft throughout.
void PanelCocoa_setLiveResizingAll(bool live);

// The registered VkPane chain index, or -1 when not a metal pane.
int PanelCocoa_chain(const PanelCocoa *pc);

// Free the Metal backing. The Panel itself is owned by the caller.
void PanelCocoa_free(PanelCocoa *pc);

// Resize the pane/board (VkPane chain rebuild). No-op when unchanged —
// fixed panes never rebuild on window resize. Returns false on failure
// (old chain survives).
bool PanelCocoa_setSize(PanelCocoa *pc, int width, int height);

// The CAMetalLayer for this panel. Add it to the window's content view layer
// (or a parent panel's layer). WindowServer composites it with the
// WindowServer transaction — no CPU blit needed.
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
