#ifndef DARLING_FRAME_H
#define DARLING_FRAME_H

#include <stdbool.h>

#include "graphics/graphics.h"   // graphvex R3: Rect, Color, DisplayList
#include "panel/panel.h"         // darling: Panel (wrapper over a GraphicsPanel)
#include "ui/element.h"          // graphvex R3: Element (the GraphicsPanel)
#include "image.h"               // graphvex R3: Image (CAPTURE returns one)
#include "window/window.h"       // hotcwap R1: the OS window

// darling R4 — frame.h
//
// THE JFRAME. A real, draggable OS window (owned by hotcwap, R1) that hosts a
// tree of Elements and presents them. Frame content IS an Element — the same
// node Panels and containers are; the tree is uniform.
//
//   Frame *f = Frame("darling", 760, 520);
//   Element *p = Frame_addPanel(f, &(ElementDesc){ .anchor = PART_CENTER, ... });
//
// Presentation today is software: the frame is raster-rendered by graphvex and
// handed to Window_presentRGBA. The GPU (Metal) seam replaces this one call
// later without changing a single call site here.

typedef struct Frame Frame;

Frame *Frame_0(void);
Frame *Frame_1(const char *title);
Frame *Frame_3(const char *title, int widthPx, int heightPx);
// Public construction is `Frame(...)` — the arity chooser picks _0/_1/_3 by
// argument count. The `_N` spellings are implementation, not call sites.
#define FRAME_CHOOSER(_0, _1, _2, _3, NAME, ...) NAME
#define Frame(...) FRAME_CHOOSER(dummy __VA_OPT__(,) __VA_ARGS__, Frame_3, Frame_3, Frame_1, Frame_0)(__VA_ARGS__)
void Frame_destroy(Frame *frame);

Window *Frame_window(const Frame *frame);
void Frame_setTitle(Frame *frame, const char *title);
void Frame_setBackground(Frame *frame, Color color);
void Frame_setBackgroundColor(Frame *frame, Color color);   // alias (background may be transparent)
Color Frame_background(const Frame *frame);
void Frame_setTransparent(Frame *frame, bool transparent);  // OS window see-through
void Frame_setBlur(Frame *frame, float radius);             // frosted backdrop blur (0 = none)

// ── Liquid Glass (macOS-exclusive, capability-gated) ─────────────────────────
// Liquid Glass has no cross-platform equivalent, so it is a platform-exclusive
// API (the Frame_macOS_ infix is the lock), never a generic Frame_setTsomething.
// It forwards to the borrowed R1 window's AppKit chrome; where the OS/SDK lacks
// it, Frame_setBlur is the fallback (the Capability Gating Law). Probe first.
typedef WindowLiquidGlassDesc FrameLiquidGlassDesc;
#define FRAME_LIQUID_GLASS_STYLE_REGULAR WINDOW_LIQUID_GLASS_STYLE_REGULAR
#define FRAME_LIQUID_GLASS_STYLE_CLEAR   WINDOW_LIQUID_GLASS_STYLE_CLEAR
bool Frame_macOS_hasLiquidGlass(void);
void Frame_macOS_setLiquidGlass(Frame *frame, const FrameLiquidGlassDesc *desc);
bool Frame_macOS_getLiquidGlass(const Frame *frame, FrameLiquidGlassDesc *out);

// Children. The Frame's content is ONE Element (Frame_element); Frame_addPanel
// returns a Panel WRAPPER (the interactable) whose GraphicsPanel is a child.
Element *Frame_element(const Frame *frame);
Panel   *Frame_addPanel(Frame *frame, const ElementDesc *desc);
int      Frame_count(const Frame *frame);
Panel   *Frame_panel(const Frame *frame, int index);
void     Frame_removePanels(Frame *frame);

// The layout root in native px — what children anchor against. It IS the window:
// panels reflow on resize, keeping their own size, and anything past the window
// edge is clipped.
Rect Frame_root(const Frame *frame);

// THE one resize surface. The window's resize event calls ONLY this; it resizes
// everything (the render target, the layout root) and repaints. Call it with the
// window's native pixel size.
void Frame_setSize(Frame *frame, int widthPx, int heightPx);

// ── Windows ─────────────────────────────────────────────────────────────────
// A Frame is a real window. Frames form an OWNERSHIP TREE: closing an owner
// closes its children (and theirs). Frame_show/hide only toggle the OS window.
void Frame_show(Frame *frame);
void Frame_hide(Frame *frame);
void Frame_close(Frame *frame);            // destroy this frame + its children
bool Frame_isClosed(const Frame *frame);
void  Frame_setOwner(Frame *frame, Frame *owner);
Frame *Frame_owner(const Frame *frame);

// ── Lifecycle hook ──────────────────────────────────────────────────────────
// Fired just before a frame is destroyed (on Frame_close). Generic: the input
// layer uses it to drop its registrations, so frame.c never learns about input.
typedef void (*FrameCloseFn)(Frame *frame, void *userdata);
void Frame_onClose(Frame *frame, FrameCloseFn fn, void *userdata);

// Paint every live frame if anything changed, else park. Runs until the ROOT
// frame (the owner-less one, or the one passed) closes — which closes the rest.
void Frame_runAll(Frame *frame);

void Frame_paint(const Frame *frame, DisplayList *dl);
void Frame_render(Frame *frame);        // render + present one frame
void Frame_invalidate(Frame *frame);    // mark for repaint; the runner paints it
void Frame_invalidateElement(Element *root);   // invalidate the frame owning root
void Frame_run(Frame *frame);           // show the window and loop until it closes

// ── screenshots ─────────────────────────────────────────────────────────────
// Grab what a frame actually drew. Frame_capture re-renders and returns the
// frame's RGBA8 Image (0xRRGGBBAA) — the graphvex CAPTURE(&image) macro is the
// backend-level equivalent. Frame_savePNG writes it to disk for tests/agents.
Frame *Frame_active(void);
Image *Frame_capture(Frame *frame);
bool   Frame_savePNG(Frame *frame, const char *path);

#endif // DARLING_FRAME_H
