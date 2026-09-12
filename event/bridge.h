#ifndef DARLING_EVENT_BRIDGE_H
#define DARLING_EVENT_BRIDGE_H

#include <stdint.h>

#include "darling/panel/panel.h"

// event/bridge.h — raw pipeline to dispatch handoff (struct-less MODULE).
//
// Attaches vexspoke Key/Mouse listeners (global or window-scoped) and translates each
// raw callback into a darling UI event that is fired immediately:
//   key down/up/repeat -> UIKeyEvent (nanos = exactNanos from the ring)
//   mouse down/up      -> PointerEvent DOWN/UP (nanos = exactNanos)
//   mouse move/drag    -> PointerEvent MOVE/DRAG (stamped at delivery;
//                          motion is never judgment-critical)
// Char, scroll, zoom, delta, and touch arrive in a later turn.
// Focus is explicit (no auto-focus yet): the app sets the key target.

// --- Window-scoped API (closing BUG-016, windowId 1..7) ---
void   Darling_bridgeAttachWindow(uint32_t windowId, Panel *root);
void   Darling_bridgeDetachWindow(uint32_t windowId);
void   Darling_bridgeSetFocusedWindow(uint32_t windowId, Panel *p);
Panel *Darling_bridgeGetFocusedWindow(uint32_t windowId);
Panel *Darling_bridgeGetRootWindow(uint32_t windowId);

// --- Global / Single-window backward-compatible API ---
// Bind the tree root for pointer delivery + install listeners (idempotent).
void   Darling_bridgeAttach(Panel *root);
// Remove listeners, clear root and focus.
void   Darling_bridgeDetach(void);
// Explicit key target for Darling_fireKey (nullptr = keys go nowhere).
void   Darling_bridgeSetFocused(Panel *p);
Panel *Darling_bridgeGetFocused(void);
// Tree root used for pointer delivery (nullptr when detached).
Panel *Darling_bridgeGetRoot(void);
// OS window registered for cursor/pointer seams (I-beam caret, etc.); the
// text handlePointer handlers read it via Darling_bridgeGetWindow. The app
// registers its native window once at startup.
void   Darling_bridgeSetWindow(void *window);
void  *Darling_bridgeGetWindow(void);

#endif
