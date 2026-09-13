#import <QuartzCore/CALayer.h>
#import <QuartzCore/CAMetalLayer.h>
#import <Metal/Metal.h>
#import <IOSurface/IOSurface.h>
#import <stdatomic.h>
#include <string.h>

#include "panel_cocoa.h"
#include "annotation/overview.h"

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: PanelCocoa (IOSurface-backed panel shim)
 * LEVEL: L4 — Self-Management (OS IOSurface/CALayer panel shim)
 * ============================================================================
 * IOSurface-backed panel compositor: each cocoa-backed panel owns a GPU
 * buffer plus a CALayer target, with the panel subtree painted into the
 * surface and composited by AppKit. Metal panes (PanelCocoa_newMetal) own
 * a CAMetalLayer + VkPane swapchain pinned top-left (contentsGravity
 * TopLeft, anchorPoint (0,0), geometryFlipped YES) presenting with the
 * WindowServer transaction (presentsWithTransaction YES) — stack:
 * NSWindow -> CAMetalLayer -> Vulkan rect children, recursively.
 *
 * STRUCT FIELDS (local to this file):
 * ----------------------------------------------------------------------------
 *   PanelEntry {           // Panel * -> PanelCocoa * registry row
 *     void *panel;         // Panel * key (opaque to the ObjC side)
 *     PanelCocoa *pc;      // Backing value for the key
 *   }
 *   PanelCocoa {           // Opaque IOSurface backing (see panel_cocoa.h)
 *     void *panel;         // Panel * (opaque to ObjC side)
 *     IOSurfaceRef surface; // GPU buffer backing (max-size allocation)
 *     CALayer *layer;      // AppKit composite target
 *     int width, height;   // Current display size shown in the window
 *     int maxWidth, maxHeight; // Max IOSurface allocation (never reallocates)
 *     _Atomic bool dirty;  // Repaint-needed flag
 *     bool isMetal;        // CAMetalLayer pane (own VkPane swapchain)
 *     int chain;           // VkPane chain index (-1 when not metal)
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - PanelCocoa_new(panel, width, height)
 *   - PanelCocoa_newMetal(panel, width, height)
 *
 * Core Functions:
 *   - PanelCocoa_free(pc)
 *   - PanelCocoa_layer(pc)
 *   - PanelCocoa_width(pc)
 *   - PanelCocoa_height(pc)
 *   - PanelCocoa_surface(pc)
 *   - PanelCocoa_markDirty(pc)
 *   - PanelCocoa_fromPanel(panel)
 *   - PanelCocoa_isMetal(pc)
 *   - PanelCocoa_chain(pc)
 *
 * Setters:
 *   - PanelCocoa_setSize(pc, width, height)
 *   - PanelCocoa_setAnchors(pc, parentAnchor, selfAnchor)
 *
 * Getters:
 *   - PanelCocoa_isDirty(pc)
 * ============================================================================
 */


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

// objc/panel_cocoa.m — IOSurface-backed panel compositor.
//
// Each cocoa-backed panel owns an IOSurface (GPU buffer) + CALayer (AppKit
// composite target). The panel subtree is painted into the IOSurface; AppKit
// composites the layer. No Metal code — just IOSurface + CALayer.

struct PanelCocoa {
    void *panel;            // Panel * (opaque to ObjC side)
    IOSurfaceRef surface;   // GPU buffer backing (allocated at MAX size, never reallocates)
    CALayer *layer;         // AppKit composite target
    int width, height;      // current display size (what's shown in window)
    int maxWidth, maxHeight; // max IOSurface size (fixed allocation)
    _Atomic bool dirty;     // needs repaint
    bool isMetal;           // CAMetalLayer pane of glass (own VkPane chain)
    int chain;              // VkPane chain index (-1 when not metal)
};

// Pixel format: BGRA8 — safe for both Vulkan import/export and AppKit.
static const int kBytesPerPixel = 4;

static IOSurfaceRef makeSurface(int width, int height) {
    if (width <= 0 || height <= 0) return nullptr;
    CFMutableDictionaryRef props = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    if (!props) return nullptr;

    // IOSurface properties
    int bpr = width * kBytesPerPixel;
    CFNumberRef w = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &width);
    CFNumberRef h = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &height);
    CFNumberRef bprNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &bpr);
    int format = 'BGRA'; // kCVPixelFormatType_32BGRA
    CFNumberRef fmt = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &format);

    CFDictionarySetValue(props, kIOSurfaceWidth, w);
    CFDictionarySetValue(props, kIOSurfaceHeight, h);
    CFDictionarySetValue(props, kIOSurfaceBytesPerRow, bprNum);
    
    int bpe = kBytesPerPixel;
    CFNumberRef bpeNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &bpe);
    CFDictionarySetValue(props, kIOSurfaceBytesPerElement, bpeNum);
    
    CFDictionarySetValue(props, kIOSurfacePixelFormat, fmt);
    // Allocate in VRAM so Vulkan can import without a copy
    int pool = 1; // kIOSurfaceCacheModeWriteThrough (write-combined-ish)
    CFNumberRef cache = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &pool);
    CFDictionarySetValue(props, kIOSurfaceCacheMode, cache);

    IOSurfaceRef surface = IOSurfaceCreate(props);

    CFRelease(w); CFRelease(h); CFRelease(bprNum);
    CFRelease(fmt); CFRelease(cache); CFRelease(props);
    return surface;
}

PanelCocoa *PanelCocoa_new(void *panel, int width, int height) {
    if (!panel || width <= 0 || height <= 0) return nullptr;

    PanelCocoa *pc = (PanelCocoa*) calloc(1, sizeof(PanelCocoa));
    if (!pc) return nullptr;

    (*pc).panel = panel;
    (*pc).width = width;
    (*pc).height = height;
    (*pc).maxWidth = width;
    (*pc).maxHeight = height;
    atomic_init(&(*pc).dirty, true);

    // Allocate IOSurface at MAX size (fixed, never reallocates)
    (*pc).surface = makeSurface(width, height);
    if (!(*pc).surface) {
        free(pc);
        return nullptr;
    }

    (*pc).layer = [[CALayer alloc] init];
    (*pc).layer.contentsGravity = kCAGravityTopLeft;
    (*pc).layer.geometryFlipped = YES; // Top-down coordinate space matching Vulkan
    (*pc).layer.contents = (__bridge id)(*pc).surface;
    extern float TextCore_backingScale(void);
    float scale = TextCore_backingScale();
    if (scale <= 0.0f) scale = 1.0f;
    (*pc).layer.contentsScale = (CGFloat) scale;
    (*pc).layer.opaque = NO;
    (*pc).layer.anchorPoint = CGPointMake(0, 0);
    (*pc).layer.drawsAsynchronously = NO;
    (*pc).layer.contentsRect = CGRectMake(0, 0, 1, 1); // show full surface

    // Register in dynamic lookup table
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

    return pc;
}

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
    (*pc).maxWidth = width;
    (*pc).maxHeight = height;
    (*pc).isMetal = true;
    (*pc).chain = -1;
    atomic_init(&(*pc).dirty, false);

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
    // WindowServer sync: presents join the CoreAnimation transaction so pane
    // anchoring (autoresizingMask + anchorPoint from PanelCocoa_setAnchors)
    // lands on the same vsync as the window edge — edge-locked, zero CPU
    // catch-up. Mirrors the board VulkanView steady-state contract.
    layer.presentsWithTransaction = YES;
    // Rule 12: contentsScale = backingScaleFactor so native physical pixels
    // of the pane's swapchain map 1:1 to logical points. drawableSize is
    // points * scale (physical pixels), matching the IOSurface path.
    extern float TextCore_backingScale(void);
    float scale = TextCore_backingScale();
    if (scale <= 0.0f) scale = 1.0f;
    layer.contentsScale = (CGFloat) scale;
    layer.drawableSize = CGSizeMake((CGFloat) width * scale, (CGFloat) height * scale);
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
    // Metal pane: detach its swapchain registry entry. Unregistering runs
    // thread-0 teardown; the engine owns the layer's Vulkan surface.
    if ((*pc).isMetal) {
        extern int VkPane_unregister(int index);
        if ((*pc).chain >= 0)
            VkPane_unregister((*pc).chain);
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
    if ((*pc).surface) CFRelease((*pc).surface);
    free(pc);
}

bool PanelCocoa_setSize(PanelCocoa *pc, int width, int height) {
    if (!pc || width <= 0 || height <= 0) return false;
    if (width == (*pc).width && height == (*pc).height) return true;

    if ((*pc).isMetal) {
        // Pane of glass: the swapchain extent follows the pane's OWN size.
        // The window-resize path never reaches here with a changed pane size
        // (anchored panes keep their rect while the window moves), so the
        // chain rebuilds only on true pane drift.
        extern bool VkPane_resize(int index, int width, int height);
        bool ok = VkPane_resize((*pc).chain, width, height);
        if (ok) {
            (*pc).width = width;
            (*pc).height = height;
        }
        return ok;
    }

    // IOSurface path: Update display size (IOSurface stays at max size, never reallocates)
    (*pc).width = width;
    (*pc).height = height;

    // Update contentsRect to show only the current-size portion of the max-size IOSurface
    if ((*pc).maxWidth > 0 && (*pc).maxHeight > 0) {
        CGFloat rectW = (CGFloat)width / (CGFloat)(*pc).maxWidth;
        CGFloat rectH = (CGFloat)height / (CGFloat)(*pc).maxHeight;
        CGFloat rectX = 0.0f;
        CGFloat rectY = 0.0f;
        (*pc).layer.contentsRect = CGRectMake(rectX, rectY, rectW, rectH);
    }

    atomic_store(&(*pc).dirty, true);
    return true;
}

void *PanelCocoa_layer(PanelCocoa *pc) {
    return pc ? (__bridge void*) (*pc).layer : nullptr;
}

int PanelCocoa_width(const PanelCocoa *pc) { return pc ? (*pc).width : 0; }
int PanelCocoa_height(const PanelCocoa *pc) { return pc ? (*pc).height : 0; }
void *PanelCocoa_surface(PanelCocoa *pc) { return pc ? (void*) (*pc).surface : nullptr; }

bool PanelCocoa_isMetal(const PanelCocoa *pc) { return pc ? (*pc).isMetal : false; }
int PanelCocoa_chain(const PanelCocoa *pc) { return pc ? (*pc).chain : -1; }

void PanelCocoa_markDirty(PanelCocoa *pc) {
    if (pc) atomic_store(&(*pc).dirty, true);
}

// Lookup: retrieve the PanelCocoa backing for a Panel. Returns nullptr if the
// panel has no IOSurface backing. Used by the window bridge.
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
    // per drag event (Window_compositeIOSurfaceChildren early-returns while
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

bool PanelCocoa_isDirty(const PanelCocoa *pc) {
    return pc ? atomic_load(&(*pc).dirty) : false;
}
