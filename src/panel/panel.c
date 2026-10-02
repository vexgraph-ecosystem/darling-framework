#include "panel/panel.h"

#include <stdlib.h>

// darling R4 — panel.c
// The wrapper. A Panel owns its graphvex GraphicsPanel (Element); it adds the
// interaction/identity layer darling needs on top of pure geometry.

struct Panel {
    Element *graphics;   // the attached GraphicsPanel
};

Panel *Panel_0(void) { return Panel_1(NULL); }

Panel *Panel_1(const ElementDesc *desc) {
    Panel *p = calloc(1, sizeof *p);
    if (!p) return NULL;
    (*p).graphics = Element(desc);
    if (!(*p).graphics) { free(p); return NULL; }
    return p;
}

void Panel_destroy(Panel *panel) {
    if (!panel) return;
    Element_destroy((*panel).graphics);
    free(panel);
}

Element *Panel_graphics(const Panel *panel) {
    return panel ? (*panel).graphics : NULL;
}
