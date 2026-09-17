#import <QuartzCore/CALayer.h>
#import <QuartzCore/CAMetalLayer.h>
#import <QuartzCore/CATransaction.h>
#import <Metal/Metal.h>
#include <string.h>

#include "panel_cocoa.h"
#include "annotation/overview.h"
#include "annotation/intention.h"

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: PanelCocoa (Metal pane shim + retained board targets)
 * LEVEL: L4 — Self-Management (OS CAMetalLayer pane shim)
 * ============================================================================
 * Metal pane compositor. Two backing kinds:
 *   - PANE (PanelCocoa_newMetal): a CAMetalLayer plus a VkPane swapchain —
 *     the "pane of glass" for DIRECT scenes, own chain, own presents,
 *     layer parented into AppKit (fixed pixel size, never rebuilt on
 *     window resize; the Pane-of-Glass Law managed exception).
 *   - BOARD (PanelCocoa_newBoard): the two named full-window layers —
 *     scene (bottom) / content (top) — as RETAINED OFFSCREEN targets: a
 *     fixed-pixel-size VkLayer dual-flight chain, NEVER a CALayer, NEVER
 *     parented into the window tree. The window's single on-screen
 *     CAMetalLayer is the Frame's seam canvas; the seam pass composites
 *     the two published board images in z-order (scene below, content
 *     above) per the Window Compositing Layer Order Law. Boards never
 *     present — Darling_layerRender paints each board's subtree into its
 *     offscreen target on VkLayer_visit; the canvas samples the published
 *     flight image.
 *
 * STRUCT FIELDS (local to this file — mirror of panel_cocoa.h):
 * ----------------------------------------------------------------------------
 *   PanelEntry {           // Panel * -> PanelCocoa * registry row
 *     void *panel;         // Panel * key (opaque to the ObjC side)
 *     PanelCocoa *pc;      // Backing value for the key
 *   }
 *   PanelCocoa {           // Opaque Metal backing (see panel_cocoa.h)
 *     void *panel;         // Panel * (opaque to ObjC side)
 *     CALayer *layer;      // AppKit composite target (PANE only; board = nullptr)
 *     int width, height;   // Current display size in native pixels
 *     bool isMetal;        // Vulkan-backed (pane: VkPane chain; board: VkLayer target)
 *     bool isBoard;        // Full-window retained board (scene/content)
 *     int chain;           // VkPane chain index (pane) / VkLayer index (board)
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - PanelCocoa_newMetal(panel, width, height) : DIRECT pane (CAMetalLayer + VkPane)
 *   - PanelCocoa_newBoard(panel, width, height) : retained offscreen VkLayer target
 *
 * Core Functions:
 *   - PanelCocoa_free(pc)                  : unregister pane chain / layer target
 *   - PanelCocoa_layer(pc)                 : CALayer (pane) / nullptr (board)
 *   - PanelCocoa_width(pc) / height(pc)
 *   - PanelCocoa_fromPanel(panel)
 *   - PanelCocoa_isMetal(pc) / isBoard(pc)
 *   - PanelCocoa_chain(pc)                 : VkPane or VkLayer index
 *
 * Setters:
 *   - PanelCocoa_setSize(pc, width, height) : pane -> VkPane_resize;
 *                                             board -> VkLayer_resize (no-op unchanged)
 *   - PanelCocoa_setAnchors(pc, parentAnchor, selfAnchor)
 *   - PanelCocoa_setLiveResizingAll(live)   : inert (boards own no CALayer;
 *                                             the Frame seam layer is the
 *                                             window's only on-screen layer)
 * ============================================================================
 */

;;INTENTION("boards are retained offscreen VkLayer targets — no CALayer, no presentsWithTransaction (the retained-board overhaul, the Conflict Triage Law managed exception to the two-CAMetalLayer stack: the Frame seam layer is the window's single on-screen CAMetalLayer; the seam pass composites the published board images in z-order)")


// Forward declare to avoid any ObjC umbrella header pulling in a Collection
// typedef that collides with our struct Collection (collection.h).
struct Panel;
uint32_t Panel_getBackgroundColor(const struct Panel *p);

// Registry: maps Panel * → PanelCocoa *. Dynamically sized array with linear scan on lookup.
typedef struct {
    void *panel;        // Panel *
    PanelCocoa *pc;
} PanelEntry;

static PanelEntry *s_registry = nullptr;
static size_t s_registryCount = 0;
static size_t s_registryCapacity = 0;

// objc/panel_cocoa.m — Metal pane compositor.
//
// Each Metal-backed panel owns a CAMetalLayer + VkPane swapchain (AppKit
// composite target). The panel subtree paints into the chain via
// Darling_layerRender; AppKit composites the layer. No IOSurface remains —
// every panel is a Vulkan rect.

struct PanelCocoa {
    void *panel;            // Panel * (opaque to ObjC side)
    CALayer *layer;         // AppKit composite target (PANE only; board = nullptr)
    int width, height;      // current display size in native pixels
    bool isMetal;           // Vulkan-backed (pane: VkPane chain; board: VkLayer target)
    bool isBoard;           // full-window retained board (scene/content)
    int chain;              // VkPane chain index (pane) / VkLayer index (board)
};

// Register (or reuse) the Panel -> PanelCocoa lookup row.
static void registerPanelEntry(void *panel, PanelCocoa *pc) {
    size_t slot = SIZE_MAX;
    for (size_t i = 0; i < s_registryCount; i++) {
        if (s_registry[i].panel == nullptr) {
            slot = i;
            break;
        }
    }
    if (slot == SIZE_MAX) {
        if (s_registryCount >= s_registryCapacity) {
            size_t newCap = s_registryCapacity == 0 ? 16 : s_registryCapacity * 2;
            PanelEntry *newArr = (PanelEntry*) realloc(s_registry, newCap * sizeof(PanelEntry));
            if (newArr) {
                s_registry = newArr;
                memset(s_registry + s_registryCapacity, 0, (newCap - s_registryCapacity) * sizeof(PanelEntry));
                s_registryCapacity = newCap;
            }
        }
        if (s_registryCount < s_registryCapacity) {
            slot = s_registryCount++;
        }
    }
    if (slot != SIZE_MAX) {
        s_registry[slot].panel = panel;
        s_registry[slot].pc = pc;
    }
}

PanelCocoa *PanelCocoa_newMetal(void *panel, int width, int height) {
    if (!panel || width <= 0 || height <= 0) return nullptr;

    PanelCocoa *pc = (PanelCocoa*) calloc(1, sizeof(PanelCocoa));
    if (!pc) return nullptr;

    (*pc).panel = panel;
    (*pc).width = width;
    (*pc).height = height;
    (*pc).isMetal = true;
    (*pc).chain = -1;

    // The "pane of glass": a CAMetalLayer with its own Vulkan swapchain.
    // LAYER CONTRACT (window stack, bottom to top):
    //   NSWindow -> CAMetalLayer (this pane, the canvas) -> Vulkan rect child
    //   -> Vulkan rect child, recursively. Each pane presents its OWN chain;
    //   the window board never stamps panes (Rule 14 double-render law).
    CAMetalLayer *layer = [CAMetalLayer layer];
    if (!layer) {
        free(pc);
        return nullptr;
    }
    layer.device = MTLCreateSystemDefaultDevice();
    layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    layer.geometryFlipped = YES;            // Vulkan top-down space: (0,0) is
                                            // top-left, (+,+) runs bottom-right
    layer.opaque = NO;                      // blur show-through
    // Top-left pivot: the fixed-size pane pins its exact-size drawable to
    // the layer's top-left corner — never stretched (Resize gravity would
    // smear the frozen drawable; TopLeft crops nothing on an exact fit).
    layer.contentsGravity = kCAGravityTopLeft;
    // Rule 11 / Continuous Live Resize: presentsWithTransaction = NO so MoltenVK
    // presents immediately to the display compositor independent of thread 0 AppKit modal transactions.
    layer.presentsWithTransaction = NO;
    // Rule 12: contentsScale = backingScaleFactor so native physical pixels
    // of the pane's swapchain map 1:1 to logical points. drawableSize is
    // points * scale (physical pixels).
    extern float TextCore_backingScale(void);
    float scale = TextCore_backingScale();
    if (scale <= 0.0f) scale = 1.0f;
    layer.contentsScale = (CGFloat) scale;
    layer.drawableSize = CGSizeMake((CGFloat) width, (CGFloat) height);
    layer.anchorPoint = CGPointMake(0, 0);
    (*pc).layer = (CALayer*) layer;

    // Own swapchain: the pane renders into ITS chain at ITS fixed size. The
    // window board swapchain is untouched — live resize only moves the layer
    // frame, never rebuilds the pane (Rule 11 / Rule 14).
    extern int VkPane_register(void *layer, int width, int height, void *owner);
    int chain = VkPane_register((__bridge void*) layer, width, height, panel);
    if (chain < 0) {
        free(pc);
        return nullptr;
    }
    (*pc).chain = chain;

    registerPanelEntry(panel, pc);
    return pc;
}

void PanelCocoa_free(PanelCocoa *pc) {
    if (!pc) return;
    if ((*pc).isMetal) {
        if ((*pc).isBoard) {
            // Retained offscreen board: unregister its flight target. Runs
            // thread-0 teardown; the chain owns no CALayer.
            extern bool VkLayer_unregister(int index);
            if ((*pc).chain >= 0)
                VkLayer_unregister((*pc).chain);
        } else {
            // Metal pane: detach its swapchain registry entry. Unregistering runs
            // thread-0 teardown; the engine owns the layer's Vulkan surface.
            extern int VkPane_unregister(int index);
            if ((*pc).chain >= 0)
                VkPane_unregister((*pc).chain);
        }
    }
    // Unregister from dynamic lookup table
    if (s_registry) {
        for (size_t i = 0; i < s_registryCount; i++) {
            if (s_registry[i].pc == pc) {
                s_registry[i].panel = nullptr;
                s_registry[i].pc = nullptr;
                break;
            }
        }
    }
    if ((*pc).layer) [(*pc).layer removeFromSuperlayer];
    free(pc);
}

bool PanelCocoa_setSize(PanelCocoa *pc, int width, int height) {
    if (!pc || width <= 0 || height <= 0) return false;
    if (width == (*pc).width && height == (*pc).height) return true;

    if (!(*pc).isMetal)
        return false;
    if ((*pc).isBoard) {
        // Retained offscreen board: rebuild the flight targets only on true
        // pixel drift (the Pane-of-Glass Law — no-op when unchanged). The
        // board owns no CALayer, so there is no drawableSize to touch.
        extern bool VkLayer_resize(int index, int width, int height);
        bool ok = VkLayer_resize((*pc).chain, width, height);
        if (ok) {
            (*pc).width = width;
            (*pc).height = height;
        }
        return ok;
    }
    // Pane of glass: the swapchain extent follows the pane's OWN size.
    // Fixed panes never reach here with a changed size (anchored panes keep
    // their rect while the window moves); boards rebuild at settle. Either
    // way the chain rebuilds only on true pane drift.
    extern bool VkPane_resize(int index, int width, int height);
    bool ok = VkPane_resize((*pc).chain, width, height);
    if (ok) {
        (*pc).width = width;
        (*pc).height = height;
        if ((*pc).layer && [(*pc).layer isKindOfClass:[CAMetalLayer class]]) {
            ((CAMetalLayer*) (*pc).layer).drawableSize = CGSizeMake((CGFloat) width, (CGFloat) height);
        }
    }
    return ok;
}

void *PanelCocoa_layer(PanelCocoa *pc) {
    return pc ? (__bridge void*) (*pc).layer : nullptr;
}

int PanelCocoa_width(const PanelCocoa *pc) { return pc ? (*pc).width : 0; }
int PanelCocoa_height(const PanelCocoa *pc) { return pc ? (*pc).height : 0; }
bool PanelCocoa_isMetal(const PanelCocoa *pc) { return pc ? (*pc).isMetal : false; }
int PanelCocoa_chain(const PanelCocoa *pc) { return pc ? (*pc).chain : -1; }
bool PanelCocoa_isBoard(const PanelCocoa *pc) { return pc ? (*pc).isBoard : false; }

// Board backing: a RETAINED OFFSCREEN target for the two named full-window
// layers — scene (bottom) / content (top). NOT a CALayer: the board owns a
// fixed-pixel-size VkLayer dual-flight chain that Darling_layerRender paints
// on VkLayer_visit; the single on-screen CAMetalLayer (the Frame seam
// canvas) composites the published board images in z-order (scene below,
// content above) per the Window Compositing Layer Order Law. Nothing here
// touches AppKit — the board lives entirely inside Vulkan.
PanelCocoa *PanelCocoa_newBoard(void *panel, int width, int height) {
    if (!panel || width <= 0 || height <= 0)
        return nullptr;
    extern int VkLayer_register(int width, int height, void *owner);
    int index = VkLayer_register(width, height, panel);
    if (index < 0)
        return nullptr;

    PanelCocoa *pc = (PanelCocoa*) calloc(1, sizeof(PanelCocoa));
    if (!pc) {
        extern bool VkLayer_unregister(int index);
        VkLayer_unregister(index);
        return nullptr;
    }
    (*pc).panel = panel;
    (*pc).width = width;
    (*pc).height = height;
    (*pc).isMetal = true;
    (*pc).isBoard = true;
    (*pc).chain = index;
    (*pc).layer = nullptr;

    registerPanelEntry(panel, pc);
    return pc;
}

// Board live-resize pin: INERT — boards own no CALayer (retained offscreen
// VkLayer targets); the Frame seam layer is the window's only on-screen
// layer and its frame tracks the window natively (autoresizingMask + the
// Native Pixel Law drawableSize contract). Kept as a documented no-op for
// bridge compatibility.
void PanelCocoa_setLiveResizingAll(bool live) {
    (void) live;
}

// Lookup: retrieve the PanelCocoa backing for a Panel. Returns nullptr if the
// panel has no Metal backing. Used by the window bridge.
void *PanelCocoa_fromPanel(void *panel) {
    if (!panel || !s_registry) return nullptr;
    for (size_t i = 0; i < s_registryCount; i++) {
        if (s_registry[i].panel == panel) return s_registry[i].pc;
    }
    return nullptr;
}

void PanelCocoa_setAnchors(PanelCocoa *pc, int anchor, int pivot) {
    if (!pc || !(*pc).layer) return;

    // Rule 13: Port pivot to CoreAnimation anchorPoint and contentsGravity.
    // Pivot has 5 points: 4 corners + center.
    // (0,0) is top-left in flipped coordinates, (1,1) is bottom-right
    CGPoint anchorPoint = CGPointMake(0.0, 0.0);
    CALayerContentsGravity gravity = kCAGravityTopLeft;
    switch (pivot) {
        case 0: // PIVOT_TOP_LEFT
            anchorPoint = CGPointMake(0.0, 0.0);
            gravity = kCAGravityTopLeft;
            break;
        case 1: // PIVOT_TOP_RIGHT
            anchorPoint = CGPointMake(1.0, 0.0);
            gravity = kCAGravityTopRight;
            break;
        case 2: // PIVOT_BOTTOM_LEFT
            anchorPoint = CGPointMake(0.0, 1.0);
            gravity = kCAGravityBottomLeft;
            break;
        case 3: // PIVOT_BOTTOM_RIGHT
            anchorPoint = CGPointMake(1.0, 1.0);
            gravity = kCAGravityBottomRight;
            break;
        case 4: // PIVOT_CENTER
            anchorPoint = CGPointMake(0.5, 0.5);
            gravity = kCAGravityCenter;
            break;
        default:
            anchorPoint = CGPointMake(0.0, 0.0);
            gravity = kCAGravityTopLeft;
            break;
    }

    // Port anchor to AppKit autoresizingMask
    // (Flexible margins push from the opposite side. e.g. MinXMargin pushes from left -> anchors to right)
    // LIVE-RESIZE CONTRACT (Rule 11.6): this mask + anchorPoint is the
    // WindowServer-accelerated anchor — CA lays the layer out inside the
    // window-resize transaction, in lockstep with the window edge. Explicit
    // frames (anti_GetChildLayout) are applied at attach/settle only, never
    // per drag event (Window_compositePanes early-returns while
    // Window_isLiveResizing).
    CAAutoresizingMask mask = kCALayerMaxXMargin | kCALayerMaxYMargin; // Default: Top-Left (Right & Bottom flexible)
    switch (anchor) {
        case 0: mask = kCALayerMaxXMargin | kCALayerMaxYMargin; break; // TOP_LEFT
        case 1: mask = kCALayerMinXMargin | kCALayerMaxXMargin | kCALayerMaxYMargin; break; // TOP_CENTER
        case 2: mask = kCALayerMinXMargin | kCALayerMaxYMargin; break; // TOP_RIGHT
        case 3: mask = kCALayerMaxXMargin | kCALayerMinYMargin | kCALayerMaxYMargin; break; // MIDDLE_LEFT
        case 4: mask = kCALayerMinXMargin | kCALayerMaxXMargin | kCALayerMinYMargin | kCALayerMaxYMargin; break; // MIDDLE_CENTER
        case 5: mask = kCALayerMinXMargin | kCALayerMinYMargin | kCALayerMaxYMargin; break; // MIDDLE_RIGHT
        case 6: mask = kCALayerMaxXMargin | kCALayerMinYMargin; break; // BOTTOM_LEFT
        case 7: mask = kCALayerMinXMargin | kCALayerMaxXMargin | kCALayerMinYMargin; break; // BOTTOM_CENTER
        case 8: mask = kCALayerMinXMargin | kCALayerMinYMargin; break; // BOTTOM_RIGHT
        default: break;
    }

    (*pc).layer.anchorPoint = anchorPoint;
    (*pc).layer.contentsGravity = gravity;
    (*pc).layer.autoresizingMask = mask;
}

