#ifndef DARLING_COMPONENT_H
#define DARLING_COMPONENT_H

#include <stdbool.h>
#include <stdint.h>

#include "lang/graphics_component.h"

// darling/component.h — THE FUSE: darling has no layout record of its own.
//
// The layout/paint metadata struct is graphvex's GraphicsComponent, embedded
// by value in every widget (Panel/Container/Label/...). There is exactly ONE
// layout record in the ecosystem. This header is the burn-down compat layer
// that keeps the old spellings compiling while the widget layer migrates:
//
//   typedef GraphicsComponent Component;          — the struct itself is graphvex's
//   COMPONENT_*                  — constant aliases onto GRAPHICS_COMPONENT_*
//   Component_getWidth/Height    — LAYOUT-CURRENCY reads: declared-or-resolved,
//                                  so stacking code NEVER sees the åuto sentinel
//   Component_get/setRadius      — the one naming delta (corner radius)
//
// Everything else that used to be Component_* is now the identically-named
// GraphicsComponent_* function. The alias + these helpers die as each widget
// file moves onto GraphicsComponent_* directly.
//
// The previous 40+ function .c (component.c) is DELETED: recompute, hitTest,
// contentRect, viewMap, every setter/getter already live in the language.

// The layout record: one type, graphvex's. A dug-up `Component component;`
// field in any widget IS a GraphicsComponent by value.
typedef GraphicsComponent Component;

// Constant aliases — the widget layer keeps its old spellings; the VALUES are
// the language's (single source of truth, identical numbers).
#define COMPONENT_ORIGIN_TOP_LEFT      GRAPHICS_COMPONENT_ORIGIN_TOP_LEFT
#define COMPONENT_ORIGIN_TOP_RIGHT     GRAPHICS_COMPONENT_ORIGIN_TOP_RIGHT
#define COMPONENT_ORIGIN_BOTTOM_LEFT   GRAPHICS_COMPONENT_ORIGIN_BOTTOM_LEFT
#define COMPONENT_ORIGIN_BOTTOM_RIGHT  GRAPHICS_COMPONENT_ORIGIN_BOTTOM_RIGHT

#define COMPONENT_ANCHOR_TOP_LEFT      GRAPHICS_COMPONENT_ANCHOR_TOP_LEFT
#define COMPONENT_ANCHOR_TOP_CENTER    GRAPHICS_COMPONENT_ANCHOR_TOP_CENTER
#define COMPONENT_ANCHOR_TOP_RIGHT     GRAPHICS_COMPONENT_ANCHOR_TOP_RIGHT
#define COMPONENT_ANCHOR_MIDDLE_LEFT   GRAPHICS_COMPONENT_ANCHOR_MIDDLE_LEFT
#define COMPONENT_ANCHOR_MIDDLE_CENTER GRAPHICS_COMPONENT_ANCHOR_MIDDLE_CENTER
#define COMPONENT_ANCHOR_MIDDLE_RIGHT  GRAPHICS_COMPONENT_ANCHOR_MIDDLE_RIGHT
#define COMPONENT_ANCHOR_BOTTOM_LEFT   GRAPHICS_COMPONENT_ANCHOR_BOTTOM_LEFT
#define COMPONENT_ANCHOR_BOTTOM_CENTER GRAPHICS_COMPONENT_ANCHOR_BOTTOM_CENTER
#define COMPONENT_ANCHOR_BOTTOM_RIGHT  GRAPHICS_COMPONENT_ANCHOR_BOTTOM_RIGHT

#define COMPONENT_PIVOT_TOP_LEFT       GRAPHICS_COMPONENT_PIVOT_TOP_LEFT
#define COMPONENT_PIVOT_TOP_RIGHT      GRAPHICS_COMPONENT_PIVOT_TOP_RIGHT
#define COMPONENT_PIVOT_BOTTOM_LEFT    GRAPHICS_COMPONENT_PIVOT_BOTTOM_LEFT
#define COMPONENT_PIVOT_BOTTOM_RIGHT   GRAPHICS_COMPONENT_PIVOT_BOTTOM_RIGHT
#define COMPONENT_PIVOT_CENTER         GRAPHICS_COMPONENT_PIVOT_CENTER

#define COMPONENT_CORNER_ARC           GRAPHICS_COMPONENT_CORNER_ARC
#define COMPONENT_CORNER_SUPERELLIPSE  GRAPHICS_COMPONENT_CORNER_SUPERELLIPSE
#define COMPONENT_COLOR_CLEAR          GRAPHICS_COMPONENT_COLOR_CLEAR

// LAYOUT-CURRENCY reads: an AUTO component resolves through its equivalence
// (0 for a dumb element, measured text for a Label), so layout code that
// stacks children never sees the åuto sentinel. Concrete components read
// identically to the language's declared getter.
static inline float Component_getWidth(const Component *self) {
    if (self == nullptr)
        return 0.0f;
    return GraphicsComponent_isAutoWidth(self)
        ? GraphicsComponent_getResolvedWidth(self)
        : GraphicsComponent_getWidth(self);
}

static inline float Component_getHeight(const Component *self) {
    if (self == nullptr)
        return 0.0f;
    return GraphicsComponent_isAutoHeight(self)
        ? GraphicsComponent_getResolvedHeight(self)
        : GraphicsComponent_getHeight(self);
}

// The one naming delta: the language calls it the corner radius.
static inline float Component_getRadius(const Component *self) {
    return GraphicsComponent_getCornerRadius(self);
}

static inline void Component_setRadius(Component *self, float radius) {
    GraphicsComponent_setCornerRadius(self, radius);
}

#endif // DARLING_COMPONENT_H