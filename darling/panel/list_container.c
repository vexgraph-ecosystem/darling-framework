#include "darling/panel/list_container.h"

#include "annotation/definition.h"
#include "annotation/overview.h"
#include "darling/panel/panel.h"
#include "nio/mem.h"
#include "oop/type.h"
#include "struct/list.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: ListContainer
 * ============================================================================
 * A vertical (or horizontal) stack owning ordered children where index IS
 * the API: append/insert/get/remove by index — text bubbles, chat logs,
 * file rows, settings groups. Children stay ordinary Panels in the
 * embedded base's child list (Panel_addContainer / Panel_getChild /
 * Panel_removeChild) — there is no second child list. Detach-only: the
 * list never frees children; every mutation re-runs the layout pass, which
 * stacks children along the axis with spacing and, on the cross axis,
 * either wraps the widest child or stretches children to the list's own
 * cross size when fillCross is set. Per the Container-vs-Panel Law it
 * embeds Panel as its first member; it is also the row container that
 * MarkdownPanel builds its document rows into.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: ListContainer (embeds Panel)
 * LEVEL: L2 — Behavior (indexed panel-stack layout behavior API)
 * ============================================================================
 * A vertical (or horizontal) stack owning ordered children where index IS
 * the API: append/insert/get/remove by index. Text bubbles, chat logs,
 * file rows, settings groups. Children stay ordinary Panels in the
 * embedded base's child list — no second child list. Detach-only: the
 * list never frees children. Every mutation re-runs the layout pass:
 * children stack along the axis with spacing; cross-axis each child
 * keeps its size and the list wraps the widest child, unless fillCross
 * stretches children to the list's own cross size.
 *
 * STRUCT FIELDS (Mirroring darling/panel/list_container.h):
 * ----------------------------------------------------------------------------
 *   Panel base;          // Inherited layout/tree/background state; children
 *                        // live in (*base).children via Panel_* tree API
 *   int32_t direction;   // LIST_PANEL_VERTICAL (0) or LIST_PANEL_HORIZONTAL (1)
 *   float spacing;       // Gap between adjacent children in parent units (>= 0)
 *   bool fillCross;      // True = stretch children across the cross axis
 *                        // to the list's own cross size; false = wrap widest
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - ListContainer_0(void)
 *   - ListContainer_1(direction)
 *
 * Core Functions:
 *   - ListContainer_add(lp, child)
 *   - ListContainer_insert(lp, index, child)
 *   - ListContainer_get(lp, index)
 *   - ListContainer_remove(lp, index)
 *   - ListContainer_count(lp)
 *   - ListContainer_layout(lp)
 *
 * Setters:
 *   - ListComponent_setLocation(lp, x, y)
 *   - ListComponent_setSize(lp, w, h)
 *   - ListContainer_setSpacing(lp, spacing)
 *   - ListContainer_setDirection(lp, direction)
 *   - ListContainer_setFillCross(lp, fill)
 *
 * Getters:
 *   - ListContainer_getSpacing(lp)
 *   - ListContainer_getDirection(lp)
 *   - ListContainer_isFillCross(lp)
 * ============================================================================
 */

// CONSTRUCTORS
// ============================================================================

ListContainer *ListContainer_0(void) {
    ListContainer *lp = (ListContainer*) Memory_alloc(TYPE_LIST_PANEL_SINGLETON, sizeof(ListContainer));
    if (!lp)
        return nullptr;
    Panel *b = Panel_0();
    if (!b) {
        Memory_free(lp);
        return nullptr;
    }
    (*lp).base = (*b);
    Memory_free(b);
    (*lp).direction = LIST_PANEL_VERTICAL;
    (*lp).spacing = 0.0f;
    (*lp).fillCross = false;
    return lp;
}

ListContainer *ListContainer_1(int32_t direction) {
    ListContainer *lp = ListContainer_0();
    if (lp)
        ListContainer_setDirection(lp, direction);
    return lp;
}

// CORE FUNCTIONS
// ============================================================================

static void markDirty(ListContainer *lp) {
    (void) lp;
    (void) 0;
}

static void layoutVertical(Panel *b, Component *c, size_t n, float spacing, bool fill) {
    float selfW = Component_getWidth(c);
    float cursor = 0.0f;
    float maxW = 0.0f;
    size_t placed = 0;
    for (size_t i = 0; i < n; i++) {
        Panel *kid = Panel_getChild(b, i);
        if (!kid)
            continue;
        Component *kb = &(*kid).component;
        float kw = Component_getWidth(kb);
        float kh = Component_getHeight(kb);
        if (kw > maxW)
            maxW = kw;
        if (fill)
            Component_setWidth(kb, selfW);
        Component_setLocation(kb, 0.0f, cursor);
        cursor += kh + spacing;
        placed++;
    }
    if (placed == 0)
        return;
    if (!fill)
        Component_setWidth(c, maxW);
    Component_setHeight(c, cursor - spacing);
}

static void layoutHorizontal(Panel *b, Component *c, size_t n, float spacing, bool fill) {
    float selfH = Component_getHeight(c);
    float cursor = 0.0f;
    float maxH = 0.0f;
    size_t placed = 0;
    for (size_t i = 0; i < n; i++) {
        Panel *kid = Panel_getChild(b, i);
        if (!kid)
            continue;
        Component *kb = &(*kid).component;
        float kw = Component_getWidth(kb);
        float kh = Component_getHeight(kb);
        if (kh > maxH)
            maxH = kh;
        if (fill)
            Component_setHeight(kb, selfH);
        Component_setLocation(kb, cursor, 0.0f);
        cursor += kw + spacing;
        placed++;
    }
    if (placed == 0)
        return;
    Component_setWidth(c, cursor - spacing);
    if (!fill)
        Component_setHeight(c, maxH);
}

void ListContainer_layout(ListContainer *lp) {
    if (!lp)
        return;
    Panel *b = &(*lp).base;
    Component *c = &(*b).component;
    size_t n = Panel_childCount(b);
    if (n == 0)
        return;
    int32_t dir = (*lp).direction;
    float spacing = (*lp).spacing;
    bool fill = (*lp).fillCross;
    if (dir == LIST_PANEL_HORIZONTAL)
        layoutHorizontal(b, c, n, spacing, fill);
    else
        layoutVertical(b, c, n, spacing, fill);
}

void ListContainer_add(ListContainer *lp, Panel *child) {
    if (!lp || !child)
        return;
    Panel *b = &(*lp).base;
    if (child == b)
        return;
    Panel_addContainer(b, child);
    ListContainer_layout(lp);
}

void ListContainer_insert(ListContainer *lp, int32_t index, Panel *child) {
    if (!lp || !child)
        return;
    Panel *b = &(*lp).base;
    if (child == b)
        return;
    if (Panel_containsChild(b, child))
        Panel_removeChild(b, child);
    size_t n = Panel_childCount(b);
    if (index < 0)
        index = 0;
    if ((size_t) index > n)
        index = (int32_t) n;
    Panel_addContainer(b, child);
    if (!Panel_containsChild(b, child))
        return;
    if ((size_t) index < n) {
        List *kids = (*b).children;
        if (kids) {
            size_t at = (size_t) index;
            for (size_t j = n; j > at; j--)
                List_set(kids, j, List_get(kids, j - 1));
            List_set(kids, at, (uint64_t)(uintptr_t) child);
        }
    }
    ListContainer_layout(lp);
}

Panel *ListContainer_get(const ListContainer *lp, int32_t index) {
    if (!lp || index < 0)
        return nullptr;
    const Panel *b = &(*lp).base;
    return Panel_getChild(b, (size_t) index);
}

bool ListContainer_remove(ListContainer *lp, int32_t index) {
    if (!lp || index < 0)
        return false;
    Panel *b = &(*lp).base;
    Panel *kid = Panel_getChild(b, (size_t) index);
    if (!kid)
        return false;
    bool out = Panel_removeChild(b, kid);
    ListContainer_layout(lp);
    return out;
}

size_t ListContainer_count(const ListContainer *lp) {
    if (!lp)
        return 0;
    const Panel *b = &(*lp).base;
    return Panel_childCount(b);
}

// SETTERS
// ============================================================================

void ListContainer_setSpacing(ListContainer *lp, float spacing) {
    if (!lp)
        return;
    if (spacing < 0.0f)
        spacing = 0.0f;
    (*lp).spacing = spacing;
    ListContainer_layout(lp);
    markDirty(lp);
}

void ListContainer_setDirection(ListContainer *lp, int32_t direction) {
    if (!lp)
        return;
    if (direction != LIST_PANEL_VERTICAL && direction != LIST_PANEL_HORIZONTAL)
        return;
    (*lp).direction = direction;
    ListContainer_layout(lp);
    markDirty(lp);
}

void ListContainer_setFillCross(ListContainer *lp, bool fill) {
    if (!lp)
        return;
    (*lp).fillCross = fill;
    ListContainer_layout(lp);
    markDirty(lp);
}

// GETTERS
// ============================================================================

float ListContainer_getSpacing(const ListContainer *lp) {
    return lp ? (*lp).spacing : 0.0f;
}

int32_t ListContainer_getDirection(const ListContainer *lp) {
    return lp ? (*lp).direction : LIST_PANEL_VERTICAL;
}

bool ListContainer_isFillCross(const ListContainer *lp) {
    return lp ? (*lp).fillCross : false;
}
