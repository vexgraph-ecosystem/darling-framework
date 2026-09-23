#include "darling/panel/layered_container.h"

#include "darling/panel/panel.h"
#include "annotation/definition.h"
#include "annotation/intention.h"
#include "annotation/overview.h"
#include "nio/mem.h"
#include "oop/type.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: LayeredContainer
 * ============================================================================
 * N full-window pane slots stacked bottom-to-top like glass panes: each
 * occupied slot pins exactly one Panel (one panel lives in exactly one
 * pane — re-pinning elsewhere de-pins first), NULL slots hold nothing and
 * cost nothing. Panes show, hide, and re-sort independently of each other:
 * visibility flips the occupant's shown state through the Panel facade,
 * move/raise/lower reorder the slot array, and the layout pass stacks
 * every occupant over the full rect. Slot accounting and the Panel-tree
 * attach/detach live here; GPU layer allocation stays in the compositor
 * bridge — a pane with identity renders through the normal Panel paint
 * path, so this class never touches a native layer handle. Pane storage
 * is two exact-size parallel arrays (no 32-pane ceiling, no artificial
 * limits); occupants are attached into the base child list for the tree
 * but always looked up by slot, never by child position — filling panes
 * out of order can never scramble identity. Detach-only throughout:
 * shrinking, clearing, or re-pinning drops pointers without freeing them.
 * Per the Container-vs-Panel Law it embeds Panel as its first member.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: LayeredContainer (embeds Panel)
 * LEVEL: L2 — Behavior (layered pane container)
 * ============================================================================
 * N full-window pane slots, bottom-to-top. Occupied slots pin one Panel
 * each; NULL slots cost nothing. Panes show/hide/sort independently; the
 * layout pass stacks occupants over the full rect. Slot accounting and
 * tree attach live here; GPU layers stay in the compositor bridge.
 *
 * STRUCT FIELDS (Mirroring darling/panel/layered_container.h):
 * ----------------------------------------------------------------------------
 *   Panel base;          // Inherited layout/tree/background state; occupants
 *                        // are attached here for the tree, looked up by slot
 *   Panel **slots;       // paneCount occupant slots; nullptr = empty pane;
 *                        // nullptr whole-array = zero panes
 *   bool *visible;       // Parallel to slots; false = hidden pane
 *   int32_t paneCount;   // Slot extent (>= 0, exact-size, no ceiling)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Public Constructors: (.h)
 *   - LayeredContainer_0(void)
 *   - LayeredContainer_1(parent)
 *   - LayeredContainer_2(count)
 *
 * Private Constructors: (.c static)
 *   - (none)
 *
 * Public Core Functions: (.h)
 *   - LayeredContainer_setPaneCount(p, count)
 *   - LayeredContainer_setContainer(p, pane, container)
 *   - LayeredContainer_getContainer(p, pane)
 *   - LayeredContainer_setPaneVisible(p, pane, visible)
 *   - LayeredContainer_isPaneVisible(p, pane)
 *   - LayeredContainer_layout(p)
 *   - LayeredContainer_move(p, from, to)
 *   - LayeredContainer_raise(p, pane)
 *   - LayeredContainer_lower(p, pane)
 *   - LayeredContainer_occupiedCount(p)
 *   - LayeredContainer_toString(p, dest, cap, outTruncated)
 *   - LayeredContainer_toStringStruct(p, dest, cap, outTruncated)
 *
 * Private Core Functions: (.c static)
 *   - markDirty(p)                       : tree-dirty hook (no-op substrate)
 *
 * Public Setters: (.h)
 *   - LayeredGraphicsComponent_setLocation(p, x, y)
 *   - LayeredGraphicsComponent_setSize(p, w, h)
 *   - LayeredContainer_setPaneCount(p, count)
 *   - LayeredContainer_setContainer(p, pane, container)
 *   - LayeredContainer_setPaneVisible(p, pane, visible)
 *
 * Private Setters: (.c static)
 *   - (none)
 *
 * Public Getters: (.h)
 *   - LayeredContainer_getPaneCount(p)
 *   - LayeredContainer_getContainer(p, pane)
 *   - LayeredContainer_isPaneVisible(p, pane)
 *   - LayeredContainer_occupiedCount(p)
 *
 * Private Getters: (.c static)
 *   - (none)
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
    (*p).slots = nullptr;
    (*p).visible = nullptr;
    (*p).paneCount = 0;
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

static void markDirty(LayeredContainer *p) {
    (void) p;
    (void) 0;
}

void LayeredContainer_setPaneCount(LayeredContainer *p, int32_t count) {
    if (!p)
        return;
    if (count < 0)
        count = 0;
    if (count == (*p).paneCount)
        return;
    Panel **nextS = nullptr;
    bool *nextV = nullptr;
    if (count > 0) {
        nextS = (Panel**) Memory_alloc(TYPE_LAYERED_CONTAINER_SINGLETON, (size_t) count * sizeof(Panel *));
        if (!nextS)
            return;
        nextV = (bool*) Memory_alloc(TYPE_LAYERED_CONTAINER_SINGLETON, (size_t) count * sizeof(bool));
        if (!nextV) {
            Memory_free(nextS);
            return;
        }
        memset(nextS, 0, (size_t) count * sizeof(Panel *));
        memset(nextV, 0, (size_t) count * sizeof(bool));
        int32_t kept = count < (*p).paneCount ? count : (*p).paneCount;
        for (int32_t i = 0; i < kept; i++) {
            nextS[i] = (*p).slots[i];
            nextV[i] = (*p).visible[i];
        }
    }
    Panel *b = &(*p).base;
    Panel **oldS = (*p).slots;
    if (oldS) {
        for (int32_t i = count; i < (*p).paneCount; i++) {
            if (oldS[i])
                Panel_removeChild(b, oldS[i]);
        }
        Memory_free(oldS);
    }
    if ((*p).visible)
        Memory_free((*p).visible);
    (*p).slots = nextS;
    (*p).visible = nextV;
    (*p).paneCount = count;
    markDirty(p);
    LayeredContainer_layout(p);
}

void LayeredContainer_setContainer(LayeredContainer *p, int32_t pane, Panel *container) {
    ;;INTENTION("slot accounting + Panel-tree attach live here; GPU layer alloc stays in the compositor bridge")
    if (!p || pane < 0 || pane >= (*p).paneCount)
        return;
    Panel *b = &(*p).base;
    if (container == b)
        return;
    Panel **slots = (*p).slots;
    bool *vis = (*p).visible;
    if (!container) {
        if (slots[pane])
            Panel_removeChild(b, slots[pane]);
        slots[pane] = nullptr;
        vis[pane] = false;
        markDirty(p);
        return;
    }
    for (int32_t i = 0; i < (*p).paneCount; i++) {
        if (i != pane && slots[i] == container) {
            Panel_removeChild(b, container);
            slots[i] = nullptr;
            vis[i] = false;
        }
    }
    Panel *old = slots[pane];
    if (old && old != container)
        Panel_removeChild(b, old);
    slots[pane] = container;
    if (!Panel_containsChild(b, container))
        Panel_addContainer(b, container);
    vis[pane] = true;
    Panel_setVisible(container, true);
    markDirty(p);
    LayeredContainer_layout(p);
}

Panel *LayeredContainer_getContainer(const LayeredContainer *p, int32_t pane) {
    if (!p || pane < 0 || pane >= (*p).paneCount)
        return nullptr;
    return (*p).slots[pane];
}

void LayeredContainer_setPaneVisible(LayeredContainer *p, int32_t pane, bool visible) {
    if (!p || pane < 0 || pane >= (*p).paneCount)
        return;
    bool *vis = (*p).visible;
    vis[pane] = visible;
    Panel **slots = (*p).slots;
    if (slots[pane])
        Panel_setVisible(slots[pane], visible);
    markDirty(p);
}

bool LayeredContainer_isPaneVisible(const LayeredContainer *p, int32_t pane) {
    if (!p || pane < 0 || pane >= (*p).paneCount)
        return false;
    return (*p).visible[pane];
}

void LayeredContainer_layout(LayeredContainer *p) {
    if (!p)
        return;
    Panel *b = &(*p).base;
    Component *c = &(*b).component;
    float w = Component_getWidth(c);
    float h = Component_getHeight(c);
    Panel **slots = (*p).slots;
    bool *vis = (*p).visible;
    for (int32_t i = 0; i < (*p).paneCount; i++) {
        if (!slots[i])
            continue;
        Component *kb = &(*slots[i]).component;
        GraphicsComponent_setLocation(kb, 0.0f, 0.0f);
        GraphicsComponent_setSize(kb, w, h);
        Panel_setVisible(slots[i], vis[i]);
    }
}

bool LayeredContainer_move(LayeredContainer *p, int32_t from, int32_t to) {
    if (!p || from < 0 || to < 0 || from >= (*p).paneCount || to >= (*p).paneCount)
        return false;
    if (from == to)
        return true;
    Panel **slots = (*p).slots;
    bool *vis = (*p).visible;
    Panel *held = slots[from];
    bool heldV = vis[from];
    if (from < to) {
        memmove(&slots[from], &slots[from + 1], (size_t) (to - from) * sizeof(Panel *));
        memmove(&vis[from], &vis[from + 1], (size_t) (to - from) * sizeof(bool));
    } else {
        memmove(&slots[to + 1], &slots[to], (size_t) (from - to) * sizeof(Panel *));
        memmove(&vis[to + 1], &vis[to], (size_t) (from - to) * sizeof(bool));
    }
    slots[to] = held;
    vis[to] = heldV;
    markDirty(p);
    LayeredContainer_layout(p);
    return true;
}

bool LayeredContainer_raise(LayeredContainer *p, int32_t pane) {
    if (!p || pane < 0 || pane >= (*p).paneCount)
        return false;
    return LayeredContainer_move(p, pane, (*p).paneCount - 1);
}

bool LayeredContainer_lower(LayeredContainer *p, int32_t pane) {
    if (!p || pane < 0 || pane >= (*p).paneCount)
        return false;
    return LayeredContainer_move(p, pane, 0);
}

int32_t LayeredContainer_occupiedCount(const LayeredContainer *p) {
    if (!p)
        return 0;
    int32_t busy = 0;
    for (int32_t i = 0; i < (*p).paneCount; i++) {
        if ((*p).slots[i])
            busy++;
    }
    return busy;
}

bool LayeredContainer_toString(const LayeredContainer *p, char *dest, size_t cap, bool *outTruncated) {
    if (!p) {
        if (!dest || cap == 0u) {
            if (outTruncated)
                (*outTruncated) = true;
            return false;
        }
        snprintf(dest, cap, "nullptr");
        if (outTruncated)
            (*outTruncated) = false;
        return true;
    }
    if (!dest || cap == 0u) {
        if (outTruncated)
            (*outTruncated) = true;
        return false;
    }
    int need = snprintf(dest, cap, "LayeredContainer(panes=%d, occupied=%d)",
        (*p).paneCount, LayeredContainer_occupiedCount(p));
    if (need < 0) {
        if (outTruncated)
            (*outTruncated) = true;
        return false;
    }
    bool trunc = (size_t) need >= cap;
    if (outTruncated)
        (*outTruncated) = trunc;
    return !trunc;
}

bool LayeredContainer_toStringStruct(const LayeredContainer *p, char *dest, size_t cap, bool *outTruncated) {
    if (!p) {
        if (!dest || cap == 0u) {
            if (outTruncated)
                (*outTruncated) = true;
            return false;
        }
        snprintf(dest, cap, "nullptr");
        if (outTruncated)
            (*outTruncated) = false;
        return true;
    }
    if (!dest || cap == 0u) {
        if (outTruncated)
            (*outTruncated) = true;
        return false;
    }
    int need = snprintf(dest, cap, "LayeredContainer{panes=%d, occupied=%d}",
        (*p).paneCount, LayeredContainer_occupiedCount(p));
    if (need < 0) {
        if (outTruncated)
            (*outTruncated) = true;
        return false;
    }
    bool trunc = (size_t) need >= cap;
    if (outTruncated)
        (*outTruncated) = trunc;
    return !trunc;
}

// SETTERS (PUBLIC & PRIVATE)
// ============================================================================

// (Setters are the Core mutators above: setPaneCount, setContainer,
// setPaneVisible — plus the Location/Size facades in the header.)

// GETTERS (PUBLIC & PRIVATE)
// ============================================================================

int32_t LayeredContainer_getPaneCount(const LayeredContainer *p) {
    return p ? (*p).paneCount : 0;
}
