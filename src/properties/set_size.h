#ifndef DARLING_PROPERTIES_SET_SIZE_H
#define DARLING_PROPERTIES_SET_SIZE_H

#include "c23/overload.h"

// darling R4 — properties/set_size.h
//
// The SIZE property: the bound's width/height. Grouped here rather than per
// widget, so every class that can be sized answers to one name and any extra
// arity (e.g. a size plus a fit rule) is one more _N function, not a new name.
//
//   Panel_setSize(panel, width, height);
//   Frame_setSize(frame, widthPx, heightPx); // resize targets, then revalidate

typedef struct Panel Panel;
typedef struct Frame Frame;

Panel *Panel_setSize_3(Panel *panel, float width, float height);
void Frame_setSize_3(Frame *frame, int widthPx, int heightPx);

#define Panel_setSize(...) OVERLOAD_DISPATCH(Panel_setSize, __VA_ARGS__)
#define Frame_setSize(...) OVERLOAD_DISPATCH(Frame_setSize, __VA_ARGS__)

#endif // DARLING_PROPERTIES_SET_SIZE_H
