#ifndef DARLING_PROPERTIES_REMOVE_H
#define DARLING_PROPERTIES_REMOVE_H

#include "c23/overload.h"

// darling R4 — properties/remove.h
//
// The REMOVE operation. `Panel_remove(child)` unlinks the child from its parent
// (both the Element tree and the wrapper's owned set) and hands it BACK — the
// caller owns it and decides whether to destroy it. Never frees.

typedef struct Panel Panel;

Panel *Panel_remove_1(Panel *child);

#define Panel_remove(...) OVERLOAD_DISPATCH(Panel_remove, __VA_ARGS__)

#endif // DARLING_PROPERTIES_REMOVE_H
