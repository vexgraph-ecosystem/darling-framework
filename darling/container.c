#include "darling/container.h"

#include <string.h>

#include "../c23/darling-type.h"
#include "nio/mem.h"
#include "oop/type.h"
#include "annotation/definition.h"
#include "annotation/overview.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Container
 * ============================================================================
 * A node of Component[]: the only container in the darling architecture. It
 * owns no layout of its own — no x/y/w/h, no anchor/pivot, no resolve, no
 * percent, no dirty, no lock. It stores element metadata values in a flat
 * doubling array; every Component carries its own geometry, constraints,
 * spacing, presentation state, and eager absolute rect. Owners feed each
 * item its parent box via GraphicsComponent_setParentAbs once per layout.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Container
 * LEVEL: L2 — Behavior (Component array node)
 * ============================================================================
 * SUMMARY:
 *   Just a Component[] node. Flat value storage, doubling growth, borrowed
 *   item pointers. No layout state of its own.
 *
 * STRUCT FIELDS (Mirroring darling/container.h):
 * ----------------------------------------------------------------------------
 *   Component *items;    // Flat Component values (null = empty)
 *   uint32_t count;      // Live item count
 *   uint32_t capacity;   // Allocated slots (doubling growth)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Public Constructors: (.h)
 *   - Container_0(void)
 *   - Container_init(self)
 *
 * Private Constructors: (.c static)
 *   - (none)
 *
 * Public Core Functions: (.h)
 *   - Container_reserve(self, capacity)
 *   - Container_add(self, src)
 *   - Container_removeAt(self, index)
 *   - Container_clear(self)
 *   - Container_count(self)
 *   - Container_get(self, index)
 *
 * Private Core Functions: (.c static)
 *   - (none)
 *
 * Public Setters: (.h)
 *   - (none — items mutate through their own Component setters)
 *
 * Private Setters: (.c static)
 *   - (none)
 *
 * Public Getters: (.h)
 *   - Container_count(self)
 *   - Container_get(self, index)
 *
 * Private Getters: (.c static)
 *   - (none)
 * ============================================================================
 */

// darling/container.c — a node of Component[].

void Container_init(Container *self) {
    if (!self)
        return;
    (*self).items = nullptr;
    (*self).count = 0u;
    (*self).capacity = 0u;
}

Container *Container_0(void) {
    Container *self = (Container*) Memory_alloc(TYPE_CONTAINER_SINGLETON, sizeof(Container));
    if (!self)
        return nullptr;
    Container_init(self);
    return self;
}

bool Container_reserve(Container *self, uint32_t capacity) {
    if (!self)
        return false;
    if (capacity <= (*self).capacity)
        return true;
    Component *next = (Component*) Memory_alloc(TYPE_CONTAINER_SINGLETON, sizeof(Component) * capacity);
    if (!next)
        return false;
    if ((*self).items && (*self).count > 0u)
        memcpy(next, (*self).items, sizeof(Component) * (*self).count);
    (*self).items = next;
    (*self).capacity = capacity;
    return true;
}

bool Container_add(Container *self, const Component *src) {
    if (!self || !src)
        return false;
    if ((*self).count >= (*self).capacity) {
        uint32_t grown = (*self).capacity == 0u ? 8u : (*self).capacity * 2u;
        if (!Container_reserve(self, grown))
            return false;
    }
    Component *slot = &(*self).items[(*self).count];
    memcpy(slot, src, sizeof(Component));
    (*self).count++;
    return true;
}

bool Container_removeAt(Container *self, uint32_t index) {
    if (!self || index >= (*self).count || !(*self).items)
        return false;
    for (uint32_t i = index; i + 1u < (*self).count; ++i)
        (*self).items[i] = (*self).items[i + 1u];
    (*self).count--;
    return true;
}

void Container_clear(Container *self) {
    if (!self)
        return;
    (*self).count = 0u;
}

uint32_t Container_count(const Container *self) {
    return self ? (*self).count : 0u;
}

Component *Container_get(const Container *self, uint32_t index) {
    if (!self || index >= (*self).count || !(*self).items)
        return nullptr;
    return &(*self).items[index];
}
