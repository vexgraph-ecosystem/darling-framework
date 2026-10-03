#ifndef DARLING_PROPERTIES_ADD_H
#define DARLING_PROPERTIES_ADD_H

#include "c23/overload.h"

// darling R4 — properties/add.h
//
// The ADD operation for the darling tree. Every `Class_add(...)` lives here,
// grouped by property instead of scattered through each widget, so a class file
// holds only that class's OWN functions and there is exactly one place to read
// all attachment shapes.
//
// Arity picks the shape (see c23/overload.h):
//
//   Panel_add(parent, child);         // append
//   Panel_add(parent, child, index);  // insert at index

typedef struct Panel Panel;

Panel *Panel_add_2(Panel *parent, Panel *child);
Panel *Panel_add_3(Panel *parent, Panel *child, int index);

#define Panel_add(...) OVERLOAD_DISPATCH(Panel_add, __VA_ARGS__)

#endif // DARLING_PROPERTIES_ADD_H
