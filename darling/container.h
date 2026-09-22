#ifndef DARLING_CONTAINER_H
#define DARLING_CONTAINER_H

#include <stdbool.h>
#include <stdint.h>

#include "darling/component.h"

// darling/container.h — a node of Component[].
//
// A Container is just a Component array node: it owns no layout of its own,
// it stores the element metadata list. All geometry, anchor/pivot/origin,
// constraints, margin/padding, and presentation state live on each Component.
// Owners (Panel trees, boards) feed each item its parent box via
// Component_setParentAbs once per layout; Components recompute eagerly.
//
// Growth doubles capacity per the Dynamic Scalability & Anti-Hardcoding Law.
// Items are values (memcpy on add); Container_get returns a borrowed pointer
// into the array (stable until the next growing add — hoist, do not hold
// across adds).

typedef struct Container {
    Component *items;
    uint32_t count;
    uint32_t capacity;
} Container;

// Constructor: Container() — empty node, no items.
Container *Container_0(void);

// Value initializer for embedded members: empty node in place.
void Container_init(Container *self);

// Core functions:
bool Container_reserve(Container *self, uint32_t capacity);
bool Container_add(Container *self, const Component *src);
bool Container_removeAt(Container *self, uint32_t index);
void Container_clear(Container *self);
uint32_t Container_count(const Container *self);
Component *Container_get(const Container *self, uint32_t index);

#endif
