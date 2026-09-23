#ifndef DARLING_LAYERED_CONTAINER_H
#define DARLING_LAYERED_CONTAINER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "c23/constructor.h"
#include "darling/panel/panel.h"
#include "oop/type.h"

// darling/panel/layered_container.h — layered container.
//
// N full-window pane slots stacked bottom-to-top like glass panes. Each
// occupied slot holds one pinned Panel (one panel lives in exactly one
// pane); NULL slots hold nothing and cost nothing. Panes show, hide, and
// re-sort independently: visibility flips the occupant's shown state,
// move/raise/lower reorder the stack. Slot accounting and the Panel-tree
// attach live here; GPU layer allocation stays in the compositor bridge
// (a pane with identity renders through the normal Panel paint path).

typedef struct LayeredContainer {
    Panel base;
    Panel **slots;
    bool *visible;
    int32_t paneCount;
} LayeredContainer;

// Constructors:
//   LayeredContainer()          — detached container, zero panes
//   LayeredContainer(parent)    — created and attached
//   LayeredContainer(count)     — N pane slots, all NULL (zero layers)
LayeredContainer *LayeredContainer_0(void);
LayeredContainer *LayeredContainer_1(Panel *parent);
LayeredContainer *LayeredContainer_2(int32_t count);

#define LayeredContainer(...) CONSTRUCTOR_DISPATCH(LayeredContainer, __VA_ARGS__)

// Layout facade (same pattern as Panel_* shims: forward over the prefix).
static inline void LayeredGraphicsComponent_setLocation(LayeredContainer *p, float x, float y)
    { if (p) Panel_setLocation(&(*p).base, x, y); }
static inline void LayeredGraphicsComponent_setSize(LayeredContainer *p, float w, float h)
    { if (p) Panel_setSize(&(*p).base, w, h); }

// Core (detach-only: occupants are attached/detached, never freed;
// every mutation re-runs the layout pass; layout allocates nothing).
void LayeredContainer_setPaneCount(LayeredContainer *p, int32_t count);
void LayeredContainer_setContainer(LayeredContainer *p, int32_t pane, Panel *container);
Panel *LayeredContainer_getContainer(const LayeredContainer *p, int32_t pane);
void LayeredContainer_setPaneVisible(LayeredContainer *p, int32_t pane, bool visible);
bool LayeredContainer_isPaneVisible(const LayeredContainer *p, int32_t pane);
void LayeredContainer_layout(LayeredContainer *p);
bool LayeredContainer_move(LayeredContainer *p, int32_t from, int32_t to);
bool LayeredContainer_raise(LayeredContainer *p, int32_t pane);
bool LayeredContainer_lower(LayeredContainer *p, int32_t pane);
int32_t LayeredContainer_occupiedCount(const LayeredContainer *p);
bool LayeredContainer_toString(const LayeredContainer *p, char *dest, size_t cap, bool *outTruncated);
bool LayeredContainer_toStringStruct(const LayeredContainer *p, char *dest, size_t cap, bool *outTruncated);

// Getters.
int32_t LayeredContainer_getPaneCount(const LayeredContainer *p);

#endif
