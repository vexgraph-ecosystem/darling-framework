#ifndef DARLING_PROPERTIES_SET_MAXIMUM_SIZE_H
#define DARLING_PROPERTIES_SET_MAXIMUM_SIZE_H

#include "c23/overload.h"

// darling R4 — properties/set_maximum_size.h
//
// The MAXIMUM-SIZE property: the bound's ceiling. It lives on the shared
// Property, so the effective size (Property_width/Height) never exceeds it. A
// width or height <= 0 clears the ceiling on that axis.
//
//   Panel_setMaximumSize(panel, 640, 480);

typedef struct Panel Panel;

Panel *Panel_setMaximumSize_3(Panel *panel, float width, float height);

#define Panel_setMaximumSize(...) OVERLOAD_DISPATCH(Panel_setMaximumSize, __VA_ARGS__)

#endif // DARLING_PROPERTIES_SET_MAXIMUM_SIZE_H
