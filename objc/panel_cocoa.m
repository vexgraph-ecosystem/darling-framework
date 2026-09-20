#import <Foundation/Foundation.h>
#include <string.h>

#include "panel_cocoa.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include "annotation/intention.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: PanelCocoa
 * ============================================================================
 * Retained offscreen board backing for the seam canvas compositor. BOARD
 * (PanelCocoa_newBoard) is the two named full-window layers — scene (bottom)
 * / content (top) — as RETAINED OFFSCREEN targets: a fixed-pixel-size VkLayer
 * dual-flight chain, never a CALayer, never parented into the window tree;
 * the Frame's single on-screen CAMetalLayer (the seam canvas) composites the
 * published board images in z-order per the Window Compositing Layer Order
 * Law (the Single-Seam Canvas Law). Boards never present — Darling_layerRender
 * paints each board's subtree into its offscreen target on VkLayer_visit, and
 * the canvas samples the published flight image. A dynamically sized registry
 * (linear scan) maps Panel * -> PanelCocoa * so the darling Panel handle
 * stays the only handle darling code holds.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: PanelCocoa (retained offscreen board targets)
 * LEVEL: L4 — Self-Management (OS seam canvas board backing)
 * ============================================================================
 * Retained offscreen board backing. One backing kind:
 *   - BOARD (PanelCocoa_newBoard): the two named full-window layers —
 *     scene (bottom) / content (top) — as RETAINED OFFSCREEN targets: a
 *     fixed-pixel-size VkLayer dual-flight chain, NEVER a CALayer, NEVER
 *     parented into the window tree. The window's single on-screen
 *     CAMetalLayer is the Frame's seam canvas; the seam pass composites
 *     the two published board images in z-order (scene below, content
 *     above) per the Window Compositing Layer Order Law (the Single-Seam
 *     Canvas Law). Boards never present — Darling_layerRender paints each
 *     board's subtree into its offscreen target on VkLayer_visit; the
 *     canvas samples the published flight image.
 *
 * STRUCT FIELDS (local to this file — mirror of panel_cocoa.h):
 * ----------------------------------------------------------------------------
 *   PanelEntry {           // Panel * -> PanelCocoa * registry row
 *     void *panel;         // Panel * key (opaque to the ObjC side)
 *     PanelCocoa *pc;      // Backing value for the key
 *   }
 *   PanelCocoa {           // Opaque board backing (see panel_cocoa.h)
 *     void *panel;         // Panel * (opaque to ObjC side)
 *     int width, height;   // Current display size in native pixels
 *     bool isBoard;        // Full-window retained board (scene/content)
 *     int chain;           // VkLayer index (board)
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - PanelCocoa_newBoard(panel, width, height) : retained offscreen VkLayer target
 *
 * Core Functions:
 *   - PanelCocoa_free(pc)                  : unregister layer target
 *   - PanelCocoa_width(pc) / height(pc)
 *   - PanelCocoa_fromPanel(panel)
 *   - PanelCocoa_isBoard(pc)
 *   - PanelCocoa_chain(pc)                 : VkLayer index
 *
 * Setters:
 *   - PanelCocoa_setSize(pc, width, height) : board -> VkLayer_resize (no-op unchanged)
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

// objc/panel_cocoa.m — retained offscreen board backing.
//
// Each board panel (scene/content) owns a fixed-pixel-size VkLayer
// dual-flight target. The panel subtree paints into the chain via
// Darling_layerRender on VkLayer_visit; the single on-screen CAMetalLayer
// (the Frame seam canvas) composites the published board images in z-order.
// No IOSurface remains — every panel is a Vulkan rect.

struct PanelCocoa {
    void *panel;            // Panel * (opaque to ObjC side)
    int width, height;      // current display size in native pixels
    bool isBoard;           // full-window retained board (scene/content)
    int chain;              // VkLayer index (board)
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

void PanelCocoa_free(PanelCocoa *pc) {
    if (!pc) return;
    // Retained offscreen board: unregister its flight target. Runs
    // thread-0 teardown; the chain owns no CALayer.
    extern bool VkLayer_unregister(int index);
    if ((*pc).chain >= 0)
        VkLayer_unregister((*pc).chain);
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
    free(pc);
}

bool PanelCocoa_setSize(PanelCocoa *pc, int width, int height) {
    if (!pc || width <= 0 || height <= 0) return false;
    if (width == (*pc).width && height == (*pc).height) return true;

    // Retained offscreen board: rebuild the flight targets only on true
    // pixel drift (the Single-Seam Canvas Law — no-op when unchanged). The
    // board owns no CALayer, so there is no drawableSize to touch.
    extern bool VkLayer_resize(int index, int width, int height);
    bool ok = VkLayer_resize((*pc).chain, width, height);
    if (ok) {
        (*pc).width = width;
        (*pc).height = height;
    }
    return ok;
}

int PanelCocoa_width(const PanelCocoa *pc) { return pc ? (*pc).width : 0; }
int PanelCocoa_height(const PanelCocoa *pc) { return pc ? (*pc).height : 0; }
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
    (*pc).isBoard = true;
    (*pc).chain = index;

    registerPanelEntry(panel, pc);
    return pc;
}

// Lookup: retrieve the PanelCocoa backing for a Panel. Returns nullptr if the
// panel has no board backing. Used by the window bridge.
void *PanelCocoa_fromPanel(void *panel) {
    if (!panel || !s_registry) return nullptr;
    for (size_t i = 0; i < s_registryCount; i++) {
        if (s_registry[i].panel == panel) return s_registry[i].pc;
    }
    return nullptr;
}