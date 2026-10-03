#ifndef DARLING_FRAME_H
#define DARLING_FRAME_H

#include <stdbool.h>
#include "properties/set_cursor.h"

#include "graphics/graphics.h"   // graphvex R3: Rect, Color, DisplayList
#include "panel/panel.h"         // darling: Panel (wrapper over a GraphicsPanel)
#include "ui/element.h"          // graphvex R3: Element (the GraphicsPanel)
#include "image.h"               // graphvex R3: Image (CAPTURE returns one)
#include "vulkan/surface.h"      // graphvex R3: the present seam (Surface)
#include "window/window.h"       // hotcwap R1: the OS window
#include "properties/add.h"        // Frame_add(...), Frame_addPanel(...)
#include "properties/remove.h"     // Frame_removePanels(...)
#include "properties/revalidate.h" // Frame_revalidate(...)
#include "properties/set_size.h"   // Frame_setSize(...)

// darling R4 — frame.h
//
// THE JFRAME. A real, draggable OS window (owned by hotcwap, R1) that hosts a
// tree of Elements and presents them. Frame content IS an Element — the same
// node Panels and containers are; the tree is uniform.
//
//   Frame *f = Frame("darling", 760, 520);
//   Element *p = Frame_addPanel(f, &(ElementDesc){ .anchor = PART_CENTER, ... });
//
// Presentation goes through ONE graphvex Surface: the Surface's retained
// present Image is the capture target, and Frame_render hands it to the window
// via the Surface's host blit (Window_presentRGBA today; an IOSurface/Metal
// blit later, no call site changes here). Frame_setSize revalidates the
// Surface, so a resize resizes the render target and repaints in one step.

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
Surface *Frame_surface(const Frame *frame);   // the owned present seam
Board *Frame_contentBoard(const Frame *frame); // borrowed content callback/generation board
void Frame_setTitle(Frame *frame, const char *title);
// Paint color only: alpha never changes OS window transparency.
void Frame_setBackground(Frame *frame, Color color);
void Frame_setBackgroundColor(Frame *frame, Color color);   // alias
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
// Attachment lives in properties/: Frame_add(frame, panel[, index]),
// Frame_addPanel(frame, desc), and Frame_removePanels(frame).
Element *Frame_element(const Frame *frame);
int      Frame_count(const Frame *frame);
Panel   *Frame_panel(const Frame *frame, int index);

// The layout root in native px — what children anchor against. It IS the window:
// panels reflow on resize, keeping their own size, and anything past the window
// edge is clipped.
Rect Frame_root(const Frame *frame);

// THE one resize surface. The window's resize event calls ONLY this; it resizes
// everything (the render target, the layout root) and repaints. Call it with the
// window's native pixel size.
// Frame_setSize(...) is declared in properties/set_size.h.

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
// Surface revalidates its Boards; the content Board revalidates/paints the tree,
// publishes its generation, then Surface presents (GPU zero-copy or RGBA).
// Frame_revalidate lives in properties/revalidate.h (overloaded by arity).
void Frame_render(Frame *frame);        // compatibility entry to Frame_revalidate
void Frame_invalidate(Frame *frame);    // mark for repaint; the runner paints it
void Frame_invalidateElement(Element *root);   // invalidate the frame owning root
void Frame_run(Frame *frame);           // show the window and loop until it closes

// ── screenshots ─────────────────────────────────────────────────────────────
// Grab what a frame actually drew. Frame_capture re-renders and returns the
// Surface's present Image (0xRRGGBBAA) — the graphvex CAPTURE(&image) macro is
// the backend-level equivalent. Frame_savePNG writes it to disk for tests/agents.
Frame *Frame_active(void);
Image *Frame_capture(Frame *frame);
bool   Frame_savePNG(Frame *frame, const char *path);

#endif // DARLING_FRAME_H
