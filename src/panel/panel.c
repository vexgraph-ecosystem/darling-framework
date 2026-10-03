#include "panel/panel.h"

#include <stdlib.h>

// darling R4 — panel.c
// The wrapper. A Panel owns its graphvex Element (the visual); it adds the
// identity/interaction layer darling needs on top of pure geometry. Every
// setter forwards to the Element and every getter reads it back, so a Panel
// never invites a caller to pierce the struct.

struct Panel {
    Element *graphics;      // the attached Element (owned)
    struct Panel **owned;   // child wrappers this panel owns (flat, descendants included)
    int ownedCount, ownedCap;
};

Panel *Panel_0(void) { return Panel_1(NULL); }

Panel *Panel_1(const ElementDesc *desc) {
    Panel *p = calloc(1, sizeof *p);
    if (!p) return NULL;
    (*p).graphics = Element(desc);
    if (!(*p).graphics) { free(p); return NULL; }
    return p;
}

Panel *Panel_2(float width, float height) {
    ElementDesc d = {0};
    d.width = width;
    d.height = height;
    return Panel_1(&d);
}

// A child wrapper is owned by its parent. Free wrappers recursively; the
// Element tree is freed exactly once by the root's Panel_destroy.
static void panel_freeWrapper(Panel *panel) {
    for (int i = 0; i < (*panel).ownedCount; i++) panel_freeWrapper((*panel).owned[i]);
    free((*panel).owned);
    free(panel);
}

void Panel_destroy(Panel *panel) {
    if (!panel) return;
    for (int i = 0; i < (*panel).ownedCount; i++) panel_freeWrapper((*panel).owned[i]);
    free((*panel).owned);
    Element_destroy((*panel).graphics);   // the Element tree, once
    free(panel);
}

Element *Panel_graphics(const Panel *panel) {
    return panel ? (*panel).graphics : NULL;
}

// ── tree ────────────────────────────────────────────────────────────────────
static bool panel_own(Panel *p, Panel *w) {
    if ((*p).ownedCount == (*p).ownedCap) {
        int cap = (*p).ownedCap ? (*p).ownedCap * 2 : 4;
        struct Panel **grown = realloc((*p).owned, (size_t)cap * sizeof *grown);
        if (!grown) return false;
        (*p).owned = grown;
        (*p).ownedCap = cap;
    }
    (*p).owned[(*p).ownedCount++] = w;
    return true;
}

Panel *Panel_add(Panel *parent, Panel *child) {
    if (!parent || !child) return NULL;
    Element_add((*parent).graphics, (*child).graphics);
    panel_own(parent, child);   // the parent owns the child wrapper
    return child;
}

int Panel_childCount(const Panel *panel) {
    return panel ? Element_count((*panel).graphics) : 0;
}

Element *Panel_childElement(const Panel *panel, int index) {
    return panel ? Element_child((*panel).graphics, index) : NULL;
}

// ── geometry ────────────────────────────────────────────────────────────────
void Panel_setSize(Panel *panel, float width, float height) {
    if (panel) Element_setSize((*panel).graphics, width, height);
}
void Panel_setOffset(Panel *panel, float x, float y) {
    if (panel) Element_setOffset((*panel).graphics, x, y);
}
void Panel_setAnchor(Panel *panel, int anchor) {
    if (panel) Element_setAnchor((*panel).graphics, anchor);
}
void Panel_setPivot(Panel *panel, int pivot) {
    if (panel) Element_setPivot((*panel).graphics, pivot);
}
void Panel_setTag(Panel *panel, const char *tag) {
    if (panel) Element_setTag((*panel).graphics, tag);
}

// ── visual ──────────────────────────────────────────────────────────────────
void Panel_setRadius(Panel *panel, float radius) {
    if (panel) Element_setRadius((*panel).graphics, radius);
}
void Panel_setBackground(Panel *panel, Color color) {
    if (panel) Element_setBackground((*panel).graphics, color);
}
void Panel_setBorder(Panel *panel, Color color, float width) {
    if (panel) Element_setBorder((*panel).graphics, color, width);
}
void Panel_setShadow(Panel *panel, float offsetX, float offsetY, float blur) {
    if (panel) Element_setShadow((*panel).graphics, offsetX, offsetY, blur);
}
void Panel_setShadowColor(Panel *panel, Color color) {
    if (panel) Element_setShadowColor((*panel).graphics, color);
}
void Panel_setVisible(Panel *panel, bool visible) {
    if (panel) Element_setVisible((*panel).graphics, visible);
}

// ── queries ─────────────────────────────────────────────────────────────────
float Panel_width(const Panel *panel) { return panel ? Element_width((*panel).graphics) : 0.0f; }
float Panel_height(const Panel *panel) { return panel ? Element_height((*panel).graphics) : 0.0f; }
float Panel_radius(const Panel *panel) { return panel ? Element_radius((*panel).graphics) : 0.0f; }
int Panel_anchor(const Panel *panel) { return panel ? Element_anchor((*panel).graphics) : 0; }
int Panel_pivot(const Panel *panel) { return panel ? Element_pivot((*panel).graphics) : 0; }
const char *Panel_tag(const Panel *panel) { return panel ? Element_tag((*panel).graphics) : NULL; }
bool Panel_isVisible(const Panel *panel) { return panel ? Element_isVisible((*panel).graphics) : false; }

// ── events ──────────────────────────────────────────────────────────────────
// The `##` IS the class: these become Panel_addMouseEvent, Panel_addScrollEvent,
// Panel_addZoomEvent, Panel_addKeyEvent, Panel_addTouchEvent, Panel_addDocumentEvent.
IMPLEMENT_EVENTS(Panel)
