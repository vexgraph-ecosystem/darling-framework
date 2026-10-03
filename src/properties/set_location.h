#ifndef DARLING_PROPERTIES_SET_LOCATION_H
#define DARLING_PROPERTIES_SET_LOCATION_H

#include "c23/overload.h"

// darling R4 — properties/set_location.h
//
// The LOCATION operation: offset, then the anchor it resolves against, then its
// own pivot. Arity grows the shape from just a point to a fully anchored,
// pivoted placement (see c23/overload.h):
//
//   Panel_setLocation(panel, x, y);                // offset only (top-left)
//   Panel_setLocation(panel, x, y, anchor);        // + anchor on the parent
//   Panel_setLocation(panel, x, y, anchor, pivot); // + pivot on the panel

typedef struct Panel Panel;

Panel *Panel_setLocation_3(Panel *panel, float x, float y);
Panel *Panel_setLocation_4(Panel *panel, float x, float y, int anchor);
Panel *Panel_setLocation_5(Panel *panel, float x, float y, int anchor, int pivot);

#define Panel_setLocation(...) OVERLOAD_DISPATCH(Panel_setLocation, __VA_ARGS__)

#endif // DARLING_PROPERTIES_SET_LOCATION_H
