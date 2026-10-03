#ifndef DARLING_PROPERTIES_SET_MINIMUM_SIZE_H
#define DARLING_PROPERTIES_SET_MINIMUM_SIZE_H

#include "c23/overload.h"

// darling R4 — properties/set_minimum_size.h
//
// The MINIMUM-SIZE property: the bound's floor. It lives on the shared Property,
// so the effective size (Property_width/Height) never drops below it — a write
// after the size still applies, and aliasing elements share the floor.
//
//   Panel_setMinimumSize(panel, 80, 24);

typedef struct Panel Panel;

Panel *Panel_setMinimumSize_3(Panel *panel, float width, float height);

#define Panel_setMinimumSize(...) OVERLOAD_DISPATCH(Panel_setMinimumSize, __VA_ARGS__)

#endif // DARLING_PROPERTIES_SET_MINIMUM_SIZE_H
