#ifndef DARLING_PROPERTIES_SET_CORNER_RADIUS_H
#define DARLING_PROPERTIES_SET_CORNER_RADIUS_H

#include "c23/overload.h"

// darling R4 — properties/set_corner_radius.h
//
// The CORNER-RADIUS property. The bound's radius both rounds the fill and (when
// > 0) clips children to the rounded shape, so it is one write with nothing to
// keep in step:
//
//   Panel_setCornerRadius(panel, 12.0f);

typedef struct Panel Panel;

Panel *Panel_setCornerRadius_2(Panel *panel, float radius);

#define Panel_setCornerRadius(...) OVERLOAD_DISPATCH(Panel_setCornerRadius, __VA_ARGS__)

#endif // DARLING_PROPERTIES_SET_CORNER_RADIUS_H
