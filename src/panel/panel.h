#ifndef DARLING_PANEL_H
#define DARLING_PANEL_H

#include "ui/element.h"

// darling R4 — panel.h
//
// THE PANEL. A Panel is the WRAPPER around a graphvex GraphicsPanel (an
// `Element`). The Element is the visual (geometry + paint); the Panel is the
// thing the app and the input layer talk to — the WRAPPERS ARE THE
// INTERACTABLES. One Panel owns exactly one GraphicsPanel.
//
//   Panel *p = Panel_new(&(ElementDesc){ .width = 200, .height = 80, ... });
//   Element *g = Panel_graphics(p);   // the attached GraphicsPanel

typedef struct Panel Panel;

Panel *Panel_0(void);
Panel *Panel_1(const ElementDesc *desc);
#define PANEL_CHOOSER(_0, _1, NAME, ...) NAME
#define Panel(...) PANEL_CHOOSER(dummy __VA_OPT__(,) __VA_ARGS__, Panel_1, Panel_0)(__VA_ARGS__)
void   Panel_destroy(Panel *panel);            // frees the attached GraphicsPanel
Element *Panel_graphics(const Panel *panel);   // the attached GraphicsPanel

#endif // DARLING_PANEL_H
