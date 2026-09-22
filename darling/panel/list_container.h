#ifndef DARLING_LIST_PANEL_H
#define DARLING_LIST_PANEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "c23/constructor.h"
#include "darling/panel/panel.h"
#include "oop/type.h"

// darling/panel/list_container.h — indexed panel stack (a Panel whose children
// are ordered rows; index IS the API: append/insert/get/remove by index).
// Children stay ordinary Panels stored in the embedded base's child list
// (Panel_addContainer / Panel_getChild / Panel_removeChild) — no second
// child list. Bubbles are just child panels with radius + margin via the
// Phase-1 substrate (Panel_setMargin, Panel_setRadius). Detach-only: the
// list never frees children.

// Central registry owns these once landed; the guard keeps this shell
// compiling standalone until then.


#define LIST_PANEL_VERTICAL    0
#define LIST_PANEL_HORIZONTAL  1

typedef struct ListContainer {
    Panel base;
    int32_t direction;
    float spacing;
    bool fillCross;
} ListContainer;

// Constructors:
//   ListContainer()            — vertical stack, zero spacing, no cross fill
//   ListContainer(direction)   — LIST_PANEL_VERTICAL / LIST_PANEL_HORIZONTAL
ListContainer *ListContainer_0(void);
ListContainer *ListContainer_1(int32_t direction);

#define ListContainer(...) CONSTRUCTOR_DISPATCH(ListContainer, __VA_ARGS__)

// Layout facade (same pattern as Panel_* shims: forward over the prefix).
static inline void ListGraphicsComponent_setLocation(ListContainer *lp, float x, float y)
    { if (lp) Panel_setLocation(&(*lp).base, x, y); }
static inline void ListGraphicsComponent_setSize(ListContainer *lp, float w, float h)
    { if (lp) Panel_setSize(&(*lp).base, w, h); }

// Core (detach-only: add/insert attach, remove detaches, never frees;
// every mutation re-runs the layout pass).
void ListContainer_add(ListContainer *lp, Panel *child);
void ListContainer_insert(ListContainer *lp, int32_t index, Panel *child);
Panel *ListContainer_get(const ListContainer *lp, int32_t index);
bool ListContainer_remove(ListContainer *lp, int32_t index);
size_t ListContainer_count(const ListContainer *lp);
void ListContainer_layout(ListContainer *lp);

// Setters.
void ListContainer_setSpacing(ListContainer *lp, float spacing);
void ListContainer_setDirection(ListContainer *lp, int32_t direction);
void ListContainer_setFillCross(ListContainer *lp, bool fill);

// Getters.
float ListContainer_getSpacing(const ListContainer *lp);
int32_t ListContainer_getDirection(const ListContainer *lp);
bool ListContainer_isFillCross(const ListContainer *lp);

#endif
