#ifndef DARLING_FRAME_H
#define DARLING_FRAME_H

#include <stdbool.h>
#include <stdint.h>

#include "effect/visual_effect.h"
#include "graphics/graphics_loop.h"
#include "lang/board.h"
#include "lang/device.h"
#include "lang/surface.h"
#include "window/window.h"

// darling/frame.h — the host frame (R4): a window, its one seam, its boards.
//
// The Frame assembles the pieces of the two-loop model into one object:
//
//   NSWindow ─▶ NSVisualEffectView (VisualEffect, the blur chrome)
//                └─ CAMetalLayer   (the ONE seam — the Single-Seam Canvas Law)
//                     ├─ Board: scene   (retained offscreen, bottom)
//                     └─ Board: content (retained offscreen, top)
//
// It registers as a client of the demand loop (GraphicsLoop), so a window-state
// event, input, a content change, or a Board's new generation wakes exactly one
// present — no fixed cadence (the Present-On-Demand Law).
//
// The Frame owns its Device, its Surface (the seam), and its two Boards; it
// borrows the window handle from the platform layer. Present is driven by
// Frame_present (the loop's present hook): composite the boards, draw the UI
// through the Graphics row, present the seam.

typedef struct Frame Frame;

// --- Constructors (the arity-overloaded chooser idiom) ---
//   Frame()          -> an offscreen frame (no window; the device + boards are usable)
//   Frame(window)    -> a frame that BORROWS a hotcwap R1 Window (R1 owns windows;
//                       the Frame builds the VisualEffect + seam into its content
//                       view and never creates or closes the window)
Frame *Frame_0(void);
Frame *Frame_1(Window *window);
#define Frame(...) CONSTRUCTOR_DISPATCH(Frame, __VA_ARGS__)

void Frame_free(Frame *frame);
bool Frame_isValid(const Frame *frame);

// --- Window (logical points; the seam resizes to native pixels) ---
void Frame_setSize(Frame *frame, int width, int height);
void Frame_setTitle(Frame *frame, const char *title);
int  Frame_getWidth(const Frame *frame);
int  Frame_getHeight(const Frame *frame);

// The backing scale (native px per logical point) the platform derived on the
// current display. The UI draw fn uses it to map the tree's logical layout onto
// the native-pixel target (the Native Pixel Law).
void  Frame_setScale(Frame *frame, float scale);
float Frame_getScale(const Frame *frame);

// Present the platform window (the AppKit hierarchy: NSWindow -> NSVisualEffectView
// -> CAMetalLayer). Off Apple this is a no-op that returns false. After show, the
// Surface is live and the frame presents into the seam.
bool Frame_show(Frame *frame);

// --- Boards (content top, scene bottom; either may be null) ---
void Frame_setBoards(Frame *frame, Board *content, Board *scene);
Board *Frame_getContentBoard(const Frame *frame);
Board *Frame_getSceneBoard(const Frame *frame);

// --- The UI draw callback: run inside the present hook, after the boards are
// composited, drawing through the active Graphics row in native pixels. ---
void Frame_setDrawFn(Frame *frame, GraphicsFrameFn draw, void *userdata);

// The demand probe: called every demand-loop step BEFORE the demand decision. A
// client whose content changed out-of-band (a Reactive tick, an animation, a
// caret) marks the frame dirty here. This is what wakes a present with no input.
void Frame_setFrameFn(Frame *frame, GraphicsFrameFn probe, void *userdata);

// --- The present hook (registered with the demand loop; also callable directly) ---
// Composite scene -> content -> the UI draw fn into the seam and present.
bool Frame_present(Frame *frame);

// Arm demand for this frame's client (a window-state/input/content event).
void Frame_markDirty(Frame *frame);

// --- Getters (the Symmetric Getter/Setter Completeness Law: null-safe) ---
Surface      *Frame_getSurface(const Frame *frame);
Device       *Frame_getDevice(const Frame *frame);
VisualEffect *Frame_getVisualEffect(const Frame *frame);
GraphicsLoop *Frame_getLoop(const Frame *frame);

// Dialect-internal (objc/frame_cocoa.m fills these once the window + seam exist;
// never called by a widget). Offscreen, a Frame has no surface and no window.
void Frame_setSurface(Frame *frame, Surface *surface);
void Frame_setDevice(Frame *frame, Device *device);
void Frame_setPlatformWindow(Frame *frame, void *window);
void *Frame_getPlatformWindow(const Frame *frame);

#endif // DARLING_FRAME_H
