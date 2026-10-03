#ifndef DARLING_PANEL_INTERNAL_H
#define DARLING_PANEL_INTERNAL_H

#include "panel/panel.h"

// darling R4 — panel/panel_internal.h
//
// The ownership seam between the widget (panel.c) and the ops layer (ops/add.c,
// ops/remove.c). The wrapper set stays panel-private; ops reach it only through
// these three calls, so panel.c remains a widget and ops never touch the struct.

// Take `child` into `parent`'s owned set and record the parent link.
void Panel_ownChild(Panel *parent, Panel *child);
// Drop `child` from `parent`'s owned set (does NOT free it). True if it was held.
bool Panel_disownChild(Panel *parent, Panel *child);

#endif // DARLING_PANEL_INTERNAL_H
