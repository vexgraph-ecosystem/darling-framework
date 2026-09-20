#include "darling/panel/layered_container.h"

#include "darling/panel/panel.h"
#include "annotation/incomplete.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include "nio/mem.h"
#include "oop/type.h"

#include <stdbool.h>
#include <stdint.h>

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: LayeredContainer
 * ============================================================================
 * Container that stacks a fixed number of full-window pane slots like glass
 * panes: each slot with an identity (a Container set into it) allocates one
 * full-window CAMetalLayer; NULL slots allocate zero layers and are skipped
 * at render (hidden = zero). Pane slots are index-based with a 32-bit
 * visibility mask (panes 0..31); setPaneCount clamps the mask and
 * setPaneVisible flips single bits, both marking the tree dirty. Layer
 * allocation and detachment land with the layer bridge, not here — this
 * class owns only the slot accounting. Per the Container-vs-Panel Law it
 * embeds Panel as its first member and inherits the layout/tree/background
 * state; constructors cover detached, parent-attached, and N-slot forms.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: LayeredContainer (embeds Panel)
 * LEVEL: L2 — Behavior (layered pane container)
 * ============================================================================
 * Container that stacks a fixed number of full-window pane slots. Each slot
 * with an identity (a Container set into it) allocates one full-window
 * CAMetalLayer; NULL slots allocate zero layers and are skipped at render.
 *
 * Layer accounting (bottom-to-top stack):
 *   pane slot with container ..... +1 CAMetalLayer (full window size)
 *   pane slot NULL ................ 0 layers (skipped)
 *
 * STRUCT FIELDS (Mirroring darling/panel/layered_container.h):
 * ----------------------------------------------------------------------------
 *   Panel base;               // Inherited layout/tree/background state
 *   int32_t paneCount;        // Number of pane slots (>= 0)
 *   uint32_t paneVisibleMask; // Bit i set = pane i visible (panes 0..31)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - LayeredContainer_0(void)
 *   - LayeredContainer_1(parent)
 *   - LayeredContainer_2(count)    — N pane slots, all NULL (zero layers)
 *
 * Core Functions:
 *   - (none — CAMetalLayer slot allocation lands with the layer bridge)
 *
 * Setters:
 *   - LayeredContainer_setPaneCount(p, count)
 *   - LayeredContainer_setPaneVisible(p, pane, visible)
 *   - LayeredContainer_setContainer(p, pane, container)
 *
 * Getters:
 *   - LayeredContainer_getPaneCount(p)
 *   - LayeredContainer_isPaneVisible(p, pane)
 *   - LayeredContainer_getContainer(p, pane)
 * ============================================================================
 */

// CONSTRUCTORS
// ============================================================================

LayeredContainer *LayeredContainer_0(void) {
    LayeredContainer *p = (LayeredContainer*) Memory_alloc(TYPE_LAYERED_CONTAINER_SINGLETON, sizeof(LayeredContainer));
    if (!p)
        return nullptr;
    Panel *b = Panel_0();
    if (!b) {
        Memory_free(p);
        return nullptr;
    }
    (*p).base = (*b);
    Memory_free(b);
    (*p).paneCount = 0;
    (*p).paneVisibleMask = 0u;
    return p;
}

LayeredContainer *LayeredContainer_1(Panel *parent) {
    LayeredContainer *p = LayeredContainer_0();
    if (p && parent) {
        Panel *b = &(*p).base;
        Panel_addContainer(parent, b);
    }
    return p;
}

LayeredContainer *LayeredContainer_2(int32_t count) {
    LayeredContainer *p = LayeredContainer_0();
    if (p)
        LayeredContainer_setPaneCount(p, count);
    return p;
}

// CORE FUNCTIONS
// ============================================================================

// (none — CAMetalLayer slot allocation and detachment land with the layer bridge)

// SETTERS
// ============================================================================

static void markDirty(LayeredContainer *p) {
    if (!p)
        return;
    Panel *b = &(*p).base;
    Container *c = &(*b).base;
    Container_markDirty(c);
}

void LayeredContainer_setPaneCount(LayeredContainer *p, int32_t count) {
    if (!p)
        return;
    if (count < 0)
        count = 0;
    (*p).paneCount = count;
    // slots beyond 31 mask bits are layout-only until the layer bridge lands
    if (count > 32)
        count = 32;
    (*p).paneVisibleMask &= (0xFFFFFFFFu >> (32u - (uint32_t) count));
    markDirty(p);
}

void LayeredContainer_setPaneVisible(LayeredContainer *p, int32_t pane, bool visible) {
    if (!p)
        return;
    if (pane < 0 || pane >= 32)
        return;
    uint32_t bit = 1u << (uint32_t) pane;
    if (visible)
        (*p).paneVisibleMask |= bit;
    else
        (*p).paneVisibleMask &= ~bit;
    markDirty(p);
}

void LayeredContainer_setContainer(LayeredContainer *p, int32_t pane, Panel *container) {
    ;;INCOMPLETE // full slot pinning + CAMetalLayer attach/detach lands with the layer bridge
    if (!p || pane < 0 || pane >= (*p).paneCount)
        return;
    Panel *b = &(*p).base;

    if (!container) {
        // NULL slot: detach whatever child lives at this pane slot.
        Panel *existing = (size_t) pane < Panel_childCount(b) ? Panel_getChild(b, (size_t) pane) : nullptr;
        if (existing)
            Panel_removeChild(b, existing);
        LayeredContainer_setPaneVisible(p, pane, false);
        markDirty(p);
        return;
    }

    // Identity slot: detach any existing occupant, then attach the new one.
    Panel *existing = (size_t) pane < Panel_childCount(b) ? Panel_getChild(b, (size_t) pane) : nullptr;
    if (existing)
        Panel_removeChild(b, existing);
    Panel_addContainer(b, container);
    LayeredContainer_setPaneVisible(p, pane, true);
    markDirty(p);
}

// GETTERS
// ============================================================================

int32_t LayeredContainer_getPaneCount(const LayeredContainer *p) {
    return p ? (*p).paneCount : 0;
}

bool LayeredContainer_isPaneVisible(const LayeredContainer *p, int32_t pane) {
    if (!p)
        return false;
    if (pane < 0 || pane >= 32)
        return false;
    uint32_t bit = 1u << (uint32_t) pane;
    return ((*p).paneVisibleMask & bit) != 0u;
}

Panel *LayeredContainer_getContainer(const LayeredContainer *p, int32_t pane) {
    if (!p || pane < 0)
        return nullptr;
    const Panel *b = &(*p).base;
    size_t n = Panel_childCount(b);
    if ((size_t) pane >= n)
        return nullptr;
    return Panel_getChild(b, (size_t) pane);
}