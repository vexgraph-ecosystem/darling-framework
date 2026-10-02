#ifndef DARLING_SCROLL_PANEL_H
#define DARLING_SCROLL_PANEL_H

#include <stdbool.h>

#include "ui/element.h"

// darling R4 — panel/scroll_panel.h
//
// A VIEWPORT over oversized content. The viewport Element clips (Property.clip);
// the borrowed content Element is offset by (-offsetX, -offsetY) inside it. The
// offset is the single source of truth and is end-clamped on every write, so a
// shrinking content or viewport can never maroon the view.
//
//   ScrollPanel *sp = ScrollPanel(400, 600);
//   Element *doc = Element(&(ElementDesc){ .width = 400, .height = 2000 });
//   ScrollPanel_setContent(sp, doc);
//   ScrollPanel_scrollBy(sp, 0, 120);      // wheel / drag deltas
//
//   ScrollPanel *sp = ScrollPanel_2(400, 600);
//   ScrollPanel *sp = ScrollPanel_0();

typedef struct ScrollPanel ScrollPanel;

ScrollPanel *ScrollPanel_0(void);
ScrollPanel *ScrollPanel_1(float viewSize);              // square viewport
ScrollPanel *ScrollPanel_2(float viewW, float viewH);
#define SCROLL_PANEL_CHOOSER(_0, _1, _2, NAME, ...) NAME
#define ScrollPanel(...) SCROLL_PANEL_CHOOSER(dummy __VA_OPT__(,) __VA_ARGS__, ScrollPanel_2, ScrollPanel_1, ScrollPanel_0)(__VA_ARGS__)
void ScrollPanel_destroy(ScrollPanel *scroll);

// The viewport Element (clipped) — add it to a Frame/tree; it owns the content.
Element *ScrollPanel_graphics(const ScrollPanel *scroll);
Element *ScrollPanel_content(const ScrollPanel *scroll);
void ScrollPanel_setContent(ScrollPanel *scroll, Element *content);   // takes ownership

// Viewport / content extents (native px).
void ScrollPanel_setViewportSize(ScrollPanel *scroll, float w, float h);
void ScrollPanel_setContentSize(ScrollPanel *scroll, float w, float h);
float ScrollPanel_viewportWidth(const ScrollPanel *scroll);
float ScrollPanel_viewportHeight(const ScrollPanel *scroll);
float ScrollPanel_contentWidth(const ScrollPanel *scroll);
float ScrollPanel_contentHeight(const ScrollPanel *scroll);

// The offset is end-clamped: [0, max(0, content - viewport)] per axis.
void ScrollPanel_setOffset(ScrollPanel *scroll, float x, float y);
void ScrollPanel_getOffset(const ScrollPanel *scroll, float *outX, float *outY);  // dest-last
void ScrollPanel_scrollBy(ScrollPanel *scroll, float dx, float dy);
float ScrollPanel_maxX(const ScrollPanel *scroll);
float ScrollPanel_maxY(const ScrollPanel *scroll);
bool ScrollPanel_canScrollY(const ScrollPanel *scroll);
bool ScrollPanel_canScrollX(const ScrollPanel *scroll);

// Reflect the offset into the tree (the viewport + content). Called by every
// setter; call it after editing the content size or the viewport directly.
void ScrollPanel_revalidate(ScrollPanel *scroll);

#endif // DARLING_SCROLL_PANEL_H
