#ifndef DARLING_EVENT_DISPATCH_H
#define DARLING_EVENT_DISPATCH_H

#include "event/action.h"
#include "event/focus.h"
#include "event/gesture.h"
#include "event/keyevent.h"
#include "event/pointer.h"
#include "event/tree.h"
#include "event/value.h"
#include "darling/panel/panel.h"

// event/dispatch.h — procedural event delivery over the Panel tree.
//
// No struct, no type id: seven fire functions, one per event family. All
// bodies are stubs until the behavior phase wires the delivery contract.

void Darling_firePointer(Panel *root, PointerEvent *ev);
void Darling_fireKey(Panel *focused, UIKeyEvent *ev);
void Darling_fireFocus(Panel *target, FocusEvent *ev);
void Darling_fireAction(Panel *source, ActionEvent *ev);
void Darling_fireValue(Panel *source, ValueEvent *ev);
void Darling_fireTree(Panel *parent, TreeEvent *ev);
void Darling_fireGesture(Panel *target, GestureEvent *ev);

// Focused key target getter (dispatch's s_focusedPanel, set by PTR_DOWN on
// focusable kinds). Read by the GfxLoop demand probe to tick the focused
// Input's caret blink. Null = nothing focused.
Panel *Darling_getFocusedPanel(void);

#endif
