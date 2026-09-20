#include "event/tree.h"

#include "darling/panel/panel.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include "nio/mem.h"
#include "oop/type.h"

#include <stdbool.h>
#include <stdint.h>

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: TreeEvent
 * ============================================================================
 * Transient tree-structure message for child add/remove: the parent panel,
 * the child panel, and the added/removed direction. Not a node and never
 * attachable — central wiring delivers it through the Panel tree, where
 * consume() short-circuits the bubble walk.
 *
 * Arena-allocated through Memory_alloc with the TYPE_TREE_EVENT_SINGLETON
 * type id; parent and child are borrowed Panel references, never owned.
 * nanos is a plain settable timestamp (clock wiring is behavior phase).
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: TreeEvent (plain struct, no Panel base)
 * LEVEL: L2 — Behavior (transient tree-structure message)
 * ============================================================================
 * Transient tree-structure message for child add/remove: parent, child,
 * and direction. Not a node, never attachable; central wiring delivers
 * it through the Panel tree.
 *
 * STRUCT FIELDS (Mirroring event/tree.h):
 * ----------------------------------------------------------------------------
 *   Panel *parent;      // Parent gaining or losing the child; nullptr = none yet
 *   Panel *child;       // Child added or removed; nullptr = none yet
 *   bool added;         // True = added, false = removed
 *   bool consumed;      // True = stop the bubble walk
 *   uint64_t nanos;     // Plain settable timestamp (clock wiring is behavior phase)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - TreeEvent_0(void)
 *   - TreeEvent_3(parent, child, added)
 *
 * Core Functions:
 *   - TreeEvent_consume(ev)
 *
 * Setters:
 *   - TreeEvent_setParent(ev, parent)
 *   - TreeEvent_setChild(ev, child)
 *   - TreeEvent_setAdded(ev, added)
 *   - TreeEvent_setNanos(ev, nanos)
 *
 * Getters:
 *   - TreeEvent_getParent(ev)
 *   - TreeEvent_getChild(ev)
 *   - TreeEvent_isAdded(ev)
 *   - TreeEvent_getNanos(ev)
 *   - TreeEvent_isConsumed(ev)
 * ============================================================================
 */

// CONSTRUCTORS
// ============================================================================

TreeEvent *TreeEvent_0(void) {
    TreeEvent *ev = (TreeEvent*) Memory_alloc(TYPE_TREE_EVENT_SINGLETON, sizeof(TreeEvent));
    if (!ev)
        return nullptr;
    (*ev).parent = nullptr;
    (*ev).child = nullptr;
    (*ev).added = false;
    (*ev).consumed = false;
    (*ev).nanos = 0u;
    return ev;
}

TreeEvent *TreeEvent_3(Panel *parent, Panel *child, bool added) {
    TreeEvent *ev = TreeEvent_0();
    if (ev) {
        (*ev).parent = parent;
        (*ev).child = child;
        (*ev).added = added;
    }
    return ev;
}

// CORE FUNCTIONS
// ============================================================================

void TreeEvent_consume(TreeEvent *ev) {
    if (!ev)
        return;
    (*ev).consumed = true;
}

// SETTERS
// ============================================================================

void TreeEvent_setParent(TreeEvent *ev, Panel *parent) {
    if (!ev)
        return;
    (*ev).parent = parent;
}

void TreeEvent_setChild(TreeEvent *ev, Panel *child) {
    if (!ev)
        return;
    (*ev).child = child;
}

void TreeEvent_setAdded(TreeEvent *ev, bool added) {
    if (!ev)
        return;
    (*ev).added = added;
}

void TreeEvent_setNanos(TreeEvent *ev, uint64_t nanos) {
    if (!ev)
        return;
    (*ev).nanos = nanos;
}

// GETTERS
// ============================================================================

Panel *TreeEvent_getParent(const TreeEvent *ev) {
    return ev ? (*ev).parent : nullptr;
}

Panel *TreeEvent_getChild(const TreeEvent *ev) {
    return ev ? (*ev).child : nullptr;
}

bool TreeEvent_isAdded(const TreeEvent *ev) {
    return ev ? (*ev).added : false;
}

uint64_t TreeEvent_getNanos(const TreeEvent *ev) {
    return ev ? (*ev).nanos : 0u;
}

bool TreeEvent_isConsumed(const TreeEvent *ev) {
    return ev ? (*ev).consumed : false;
}
