#ifndef DARLING_PANEL_H
#define DARLING_PANEL_H

#include <stdbool.h>

#include "graphics/graphics.h"   // Color
#include "ui/element.h"          // ElementDesc, Element, PART_*
#include "c23/event_invoke.h"    // DECLARE_EVENTS(Panel)
#include "properties/add.h"            // Panel_add(...)
#include "properties/remove.h"         // Panel_remove(...)
#include "properties/set_location.h"   // Panel_setLocation(...)
#include "properties/set_corner_radius.h"  // Panel_setCornerRadius(...)

// darling R4 — panel.h
//
// THE PANEL. A Panel is the WRAPPER around a graphvex Element (the visual:
// geometry + paint). The Element is pure data; the Panel is the thing an app
// and the input layer talk to — the WRAPPERS ARE THE INTERACTABLES. One Panel
// owns exactly one Element.
//
// Build one with the arity chooser, then shape it with symmetric setters:
//
//   Panel *p = Panel();                       // Panel_0()
//   Panel_setSize(p, 200, 80);
//   Panel_setAnchor(p, PART_CENTER);
//   Panel_setPivot(p, PART_CENTER);
//   Panel_setRadius(p, 16);
//   Panel_setBackground(p, COLOR_RGBA(120, 190, 220, 255));
//   Frame_add(frame, p);                      // the Frame takes ownership
//
// The `_N` spellings are implementation; call sites use `Panel(...)`.

typedef struct Panel Panel;

Panel *Panel_0(void);
Panel *Panel_1(const ElementDesc *desc);
Panel *Panel_2(float width, float height);
#define PANEL_CHOOSER(_0, _1, _2, NAME, ...) NAME
#define Panel(...) PANEL_CHOOSER(dummy __VA_OPT__(,) __VA_ARGS__, Panel_2, Panel_1, Panel_0)(__VA_ARGS__)
void   Panel_destroy(Panel *panel);            // frees the attached Element
Element *Panel_graphics(const Panel *panel);   // the attached Element (borrowed)

// ── tree (Panels nest; the child is owned by its parent) ────────────────────
// Attachment lives in properties/add.h; Panel_add(...) is overloaded by arity.
int    Panel_childCount(const Panel *panel);
Element *Panel_childElement(const Panel *panel, int index);
Panel *Panel_parent(const Panel *panel);

// ── geometry (forwards to the Element) ──────────────────────────────────────
void Panel_setSize(Panel *panel, float width, float height);
void Panel_setOffset(Panel *panel, float x, float y);
void Panel_setAnchor(Panel *panel, int anchor);     // PART_* on the parent
void Panel_setPivot(Panel *panel, int pivot);       // PART_* on the panel
void Panel_setTag(Panel *panel, const char *tag);

// ── visual ──────────────────────────────────────────────────────────────────
void Panel_setRadius(Panel *panel, float radius);
void Panel_setBackground(Panel *panel, Color color);
void Panel_setBorder(Panel *panel, Color color, float width);
void Panel_setShadow(Panel *panel, float offsetX, float offsetY, float blur);
void Panel_setShadowColor(Panel *panel, Color color);
void Panel_setVisible(Panel *panel, bool visible);

// ── queries (null-safe) ─────────────────────────────────────────────────────
float Panel_width(const Panel *panel);
float Panel_height(const Panel *panel);
float Panel_radius(const Panel *panel);
int   Panel_anchor(const Panel *panel);
int   Panel_pivot(const Panel *panel);
const char *Panel_tag(const Panel *panel);
bool  Panel_isVisible(const Panel *panel);

// ── events (per-kind adders, generated from the class name) ─────────────────
// Panel_addMouseEvent, Panel_addScrollEvent, Panel_addZoomEvent, Panel_addKeyEvent,
// Panel_addTouchEvent, Panel_addDocumentEvent — each takes its handler struct,
// registers on the panel's Element, and returns the panel.
DECLARE_EVENTS(Panel);

#endif // DARLING_PANEL_H
