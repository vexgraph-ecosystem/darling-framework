#ifndef DARLING_LAYEREDCONTAINER_H
#define DARLING_LAYEREDCONTAINER_H

#include <stdbool.h>
#include <stdint.h>

#include "c23/constructor.h"
#include "darling/panel/panel.h"
#include "oop/type.h"

// darling/panel/layeredcontainer.h — layered container
// (a Container with N full-window pane slots; each pane is a CAMetalLayer
//  stacked like glass panes. NULL slots allocate zero layers.)

// Central registry owns these once landed; the guard keeps this shell
// compiling standalone until then.


typedef struct LayeredContainer {
    Panel base;
    int32_t paneCount;
    uint32_t paneVisibleMask; // bit i set = pane i visible (panes 0..31)
    // pane slots with identity allocate one full-window CAMetalLayer;
    // NULL slots allocate zero layers and are skipped at render.
} LayeredContainer;

// Constructors:
//   LayeredContainer()          — detached container, zero panes
//   LayeredContainer(parent)    — created and attached
//   LayeredContainer(count)     — N pane slots, all NULL (zero layers)
LayeredContainer *LayeredContainer_0(void);
LayeredContainer *LayeredContainer_1(Panel *parent);
LayeredContainer *LayeredContainer_2(int32_t count);

#define LayeredContainer(...) CONSTRUCTOR_DISPATCH(LayeredContainer, __VA_ARGS__)

int32_t LayeredContainer_getPaneCount(const LayeredContainer *p);
void LayeredContainer_setPaneCount(LayeredContainer *p, int32_t count);

// Set a container into a pane slot. NULL detaches and frees the slot's layer.
// A container with identity allocates one full-window CAMetalLayer.
void LayeredContainer_setContainer(LayeredContainer *p, int32_t pane, Panel *container);
Panel *LayeredContainer_getContainer(const LayeredContainer *p, int32_t pane);

#endif