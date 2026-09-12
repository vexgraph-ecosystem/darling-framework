#include "event/dispatch.h"

#include "event/action.h"
#include "event/focus.h"
#include "event/gesture.h"
#include "event/keyevent.h"
#include "event/pointer.h"
#include "event/tree.h"
#include "event/value.h"
#include "darling/panel/panel.h"
#include "darling/panel/markdown_panel.h"
#include "c23/darling-type.h"
#include "lang/vec4.h"
#include "annotation/incomplete.h"
#include "annotation/overview.h"
#include "nio/mem.h"
#include "oop/type.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef ID_SCROLLBAR
#define ID_SCROLLBAR 0x009Fu
#endif

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: EventDispatch (procedural, no struct, no type id)
 * LEVEL: L2 — Behavior (procedural event delivery over the Panel tree)
 * ============================================================================
 * Seven fire functions, one per event family. Pointer + key delivery is
 * implemented below against the Pkg 1 contract; focus/action/value/tree/
 * gesture bodies stay stubs until their behavior pass lands.
 *
 * DELIVERY CONTRACT (Pkg 1, implemented as-is):
 * ----------------------------------------------------------------------------
 *   1. Hit-test walk: recursive reverse-child-order walk (top-most first)
 *      with Container_hitTest; first (deepest, top-most) hit wins.
 *   2. Local coords: screen minus the resolved origin of the hit panel
 *      (Container_resolve); handlers always observe target-local points.
 *   3. Type dispatch: the target's handlePointer rides Memory_type masked
 *      to class; no widget headers are included here — handlers arrive as
 *      weak externs (Pkg 2/3/4 define them later) and absent handlers are
 *      a safe no-op. Widgets never walk the tree.
 *   4. Capture: PTR_DOWN sets s_activePanel (origin cached); PTR_DRAG and
 *      PTR_UP/PTR_CANCEL route to the capture target until release, so
 *      scrubbing survives leaving the bounds.
 *   5. Hover: s_hoveredPanel tracking on MOVE/HOVER/DOWN; a change fires a
 *      PTR_LEAVE to the old panel plus a PTR_ENTER to the new one.
 *   6. Focus: PTR_DOWN on a focusable kind (Input, Textarea) sets
 *      s_focusedPanel; Darling_fireKey forwards to the focused panel only.
 *   7. Consumed short-circuits: an already-consumed event is dropped on
 *      entry, and no further delivery step runs once consumed is set.
 *
 * STRUCT FIELDS: none — procedural dispatch (operates on Panel tree + event structs).
 *
 * FILE STATICS:
 * ----------------------------------------------------------------------------
 *   Panel *s_activePanel;   // capture target between DOWN and UP/CANCEL
 *   float s_activeOX;       // cached resolved origin x of the capture target
 *   float s_activeOY;       // cached resolved origin y of the capture target
 *   Panel *s_hoveredPanel;  // panel currently under the pointer
 *   float s_hoveredLX;      // last local x delivered to the hovered panel
 *   float s_hoveredLY;      // last local y delivered to the hovered panel
 *   Panel *s_focusedPanel;  // key target set by DOWN on focusable kinds
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - Darling_firePointer(root, ev)   // implemented: hit-test + capture/hover/focus
 *   - Darling_fireKey(focused, ev)    // implemented: forward to focused panel
 *   - Darling_fireFocus(target, ev)   // stub
 *   - Darling_fireAction(source, ev)  // stub
 *   - Darling_fireValue(source, ev)   // stub
 *   - Darling_fireTree(parent, ev)    // stub
 *   - Darling_fireGesture(target, ev) // stub
 *
 * Static Helpers:
 *   - dispatchClass(p)                      // Memory_type masked to class
 *   - dispatchIsFocusable(p)                // true for Input/Textarea kinds
 *   - dispatchPick(node, ...)               // reverse-child-order hit-test walk
 *   - dispatchPointerTo(target, kind, lx, ly) // weak handlePointer type dispatch
 *   - dispatchKeyTo(target, ev)             // weak handleKey type dispatch
 *   - dispatchSyncHover(ev, hit, ...)       // ENTER/LEAVE pair tracking
 * ============================================================================
 */

// Widget handler externs: weak declarations only, never defined here and
// never called unconditionally — each call site null-checks first, so the
// translation unit links and runs before Pkg 2/3/4 define the handlers.
typedef struct Button Button;
typedef struct Switch Switch;
typedef struct Checkbox Checkbox;
typedef struct RadioGroup RadioGroup;
typedef struct Slider Slider;
typedef struct Knob Knob;
typedef struct ScrollBar ScrollBar;
typedef struct Input Input;
typedef struct Textarea Textarea;

void Button_handlePointer(Button *self, int kind, float localX, float localY) __attribute__((weak));
void Switch_handlePointer(Switch *self, int kind, float localX, float localY) __attribute__((weak));
void Checkbox_handlePointer(Checkbox *self, int kind, float localX, float localY) __attribute__((weak));
void RadioGroup_handlePointer(RadioGroup *self, int kind, float localX, float localY) __attribute__((weak));
void Slider_handlePointer(Slider *self, int kind, float localX, float localY) __attribute__((weak));
void Knob_handlePointer(Knob *self, int kind, float localX, float localY) __attribute__((weak));
void ScrollBar_handlePointer(ScrollBar *self, int kind, float localX, float localY) __attribute__((weak));
void Input_handlePointer(Input *self, int kind, float localX, float localY) __attribute__((weak));
void Textarea_handlePointer(Textarea *self, int kind, float localX, float localY) __attribute__((weak));
void MarkdownPanel_handlePointer(MarkdownPanel *self, int kind, float localX, float localY, void *window) __attribute__((weak));
void Input_handleKey(Input *self, const UIKeyEvent *ev) __attribute__((weak));
void Textarea_handleKey(Textarea *self, const UIKeyEvent *ev) __attribute__((weak));

static Panel *s_activePanel = nullptr;
static float s_activeOX = 0.0f;
static float s_activeOY = 0.0f;
static Panel *s_hoveredPanel = nullptr;
static float s_hoveredLX = 0.0f;
static float s_hoveredLY = 0.0f;
static Panel *s_focusedPanel = nullptr;

static uint64_t dispatchClass(Panel *p) {
    if (!p)
        return 0u;
    return Memory_type(p) & MASK_CLASS;
}

static bool dispatchIsFocusable(Panel *p) {
    uint64_t cls = dispatchClass(p);
    if (cls == ID_INPUT)
        return true;
    return cls == ID_TEXTAREA;
}

static Panel *dispatchPick(Panel *node, float parentX, float parentY, float parentW, float parentH, float sx, float sy, Vec4 *outRect) {
    if (!node || !outRect)
        return nullptr;
    if (!Panel_isVisible(node))
        return nullptr;
    Vec4 rect;
    Container_resolve(&(*node).base, parentX, parentY, parentW, parentH, &rect);
    size_t n = Panel_childCount(node);
    if (n > 0) {
        uint64_t doc = dispatchClass(node);
        for (size_t k = n; k > 0; k--) {
            Panel *kid = Panel_getChild(node, k - 1);
            Vec4 kidRect;
            Panel *hit = dispatchPick(kid, rect.x, rect.y, rect.z, rect.w, sx, sy, &kidRect);
            if (hit) {
                if (doc == ID_MARKDOWN_PANEL) {
                    Vec4_copy(&rect, outRect);
                    return node;
                }
                Vec4_copy(&kidRect, outRect);
                return hit;
            }
        }
    }
    if (!Container_hitTest(&(*node).base, parentX, parentY, parentW, parentH, sx, sy))
        return nullptr;
    Vec4_copy(&rect, outRect);
    return node;
}

static void dispatchPointerTo(Panel *target, int kind, float lx, float ly) {
    if (!target)
        return;
    uint64_t cls = dispatchClass(target);
    if (cls == ID_BUTTON) {
        void (*fn)(Button *, int, float, float) = Button_handlePointer;
        if (fn)
            fn((Button*) target, kind, lx, ly);
        return;
    }
    if (cls == ID_SWITCH) {
        void (*fn)(Switch *, int, float, float) = Switch_handlePointer;
        if (fn)
            fn((Switch*) target, kind, lx, ly);
        return;
    }
    if (cls == ID_CHECKBOX) {
        void (*fn)(Checkbox *, int, float, float) = Checkbox_handlePointer;
        if (fn)
            fn((Checkbox*) target, kind, lx, ly);
        return;
    }
    if (cls == ID_RADIOGROUP) {
        void (*fn)(RadioGroup *, int, float, float) = RadioGroup_handlePointer;
        if (fn)
            fn((RadioGroup*) target, kind, lx, ly);
        return;
    }
    if (cls == ID_SLIDER) {
        void (*fn)(Slider *, int, float, float) = Slider_handlePointer;
        if (fn)
            fn((Slider*) target, kind, lx, ly);
        return;
    }
    if (cls == ID_KNOB) {
        void (*fn)(Knob *, int, float, float) = Knob_handlePointer;
        if (fn)
            fn((Knob*) target, kind, lx, ly);
        return;
    }
    if (cls == ID_SCROLLBAR) {
        void (*fn)(ScrollBar *, int, float, float) = ScrollBar_handlePointer;
        if (fn)
            fn((ScrollBar*) target, kind, lx, ly);
        return;
    }
    if (cls == ID_INPUT) {
        void (*fn)(Input *, int, float, float) = Input_handlePointer;
        if (fn)
            fn((Input*) target, kind, lx, ly);
        return;
    }
    if (cls == ID_TEXTAREA) {
        void (*fn)(Textarea *, int, float, float) = Textarea_handlePointer;
        if (fn)
            fn((Textarea*) target, kind, lx, ly);
        return;
    }
    if (cls == ID_MARKDOWN_PANEL) {
        void (*fn)(MarkdownPanel *, int, float, float, void *) = MarkdownPanel_handlePointer;
        if (fn)
            fn((MarkdownPanel*) target, kind, lx, ly, nullptr);
        return;
    }
}

static void dispatchKeyTo(Panel *target, const UIKeyEvent *ev) {
    if (!target || !ev)
        return;
    uint64_t cls = dispatchClass(target);
    if (cls == ID_INPUT) {
        void (*fn)(Input *, const UIKeyEvent *) = Input_handleKey;
        if (fn)
            fn((Input*) target, ev);
        return;
    }
    if (cls == ID_TEXTAREA) {
        void (*fn)(Textarea *, const UIKeyEvent *) = Textarea_handleKey;
        if (fn)
            fn((Textarea*) target, ev);
        return;
    }
}

static void dispatchSyncHover(PointerEvent *ev, Panel *hit, float sx, float sy, float ox, float oy) {
    if (!ev)
        return;
    if (s_hoveredPanel == hit) {
        if (hit) {
            s_hoveredLX = sx - ox;
            s_hoveredLY = sy - oy;
        }
        return;
    }
    Panel *old = s_hoveredPanel;
    s_hoveredPanel = hit;
    if (old)
        dispatchPointerTo(old, PTR_LEAVE, s_hoveredLX, s_hoveredLY);
    if ((*ev).consumed)
        return;
    if (hit) {
        float lx = sx - ox;
        float ly = sy - oy;
        s_hoveredLX = lx;
        s_hoveredLY = ly;
        dispatchPointerTo(hit, PTR_ENTER, lx, ly);
    }
}

// CORE FUNCTIONS
// ============================================================================

void Darling_firePointer(Panel *root, PointerEvent *ev) {
    if (!root || !ev)
        return;
    if ((*ev).consumed)
        return;
    int kind = (*ev).kind;
    float sx = (*ev).x;
    float sy = (*ev).y;
    if (s_activePanel && (kind == PTR_DRAG || kind == PTR_UP || kind == PTR_CANCEL)) {
        Panel *cap = s_activePanel;
        float lx = sx - s_activeOX;
        float ly = sy - s_activeOY;
        if (kind != PTR_DRAG)
            s_activePanel = nullptr;
        (*ev).target = cap;
        (*ev).phase = 1;
        (*ev).x = lx;
        (*ev).y = ly;
        dispatchPointerTo(cap, kind, lx, ly);
        return;
    }
    Vec4 rect;
    Panel *hit = dispatchPick(root, 0.0f, 0.0f, 0.0f, 0.0f, sx, sy, &rect);
    if (kind == PTR_DOWN) {
        if (hit) {
            s_activePanel = hit;
            s_activeOX = rect.x;
            s_activeOY = rect.y;
            if (dispatchIsFocusable(hit))
                s_focusedPanel = hit;
        }
        dispatchSyncHover(ev, hit, sx, sy, rect.x, rect.y);
        if ((*ev).consumed)
            return;
        if (!hit)
            return;
        float lx = sx - rect.x;
        float ly = sy - rect.y;
        (*ev).target = hit;
        (*ev).phase = 1;
        (*ev).x = lx;
        (*ev).y = ly;
        dispatchPointerTo(hit, PTR_DOWN, lx, ly);
        return;
    }
    if (kind == PTR_MOVE || kind == PTR_HOVER) {
        dispatchSyncHover(ev, hit, sx, sy, rect.x, rect.y);
        if ((*ev).consumed)
            return;
        if (!hit)
            return;
        float lx = sx - rect.x;
        float ly = sy - rect.y;
        (*ev).target = hit;
        (*ev).phase = 1;
        (*ev).x = lx;
        (*ev).y = ly;
        dispatchPointerTo(hit, kind, lx, ly);
        return;
    }
    if (!hit)
        return;
    float lx = sx - rect.x;
    float ly = sy - rect.y;
    (*ev).target = hit;
    (*ev).phase = 1;
    (*ev).x = lx;
    (*ev).y = ly;
    dispatchPointerTo(hit, kind, lx, ly);
}

void Darling_fireKey(Panel *focused, UIKeyEvent *ev) {
    if (!ev)
        return;
    if ((*ev).consumed)
        return;
    Panel *target = focused;
    if (!target)
        target = s_focusedPanel;
    if (!target)
        return;
    (*ev).target = target;
    dispatchKeyTo(target, ev);
}

void Darling_fireFocus(Panel *target, FocusEvent *ev) {
    ;;INCOMPLETE // capture walk root to target, target phase, bubble walk target to root
    if (!target || !ev)
        return;
}

void Darling_fireAction(Panel *source, ActionEvent *ev) {
    ;;INCOMPLETE // capture walk root to target, target phase, bubble walk target to root
    if (!source || !ev)
        return;
}

void Darling_fireValue(Panel *source, ValueEvent *ev) {
    ;;INCOMPLETE // capture walk root to target, target phase, bubble walk target to root
    if (!source || !ev)
        return;
}

void Darling_fireTree(Panel *parent, TreeEvent *ev) {
    ;;INCOMPLETE // capture walk root to target, target phase, bubble walk target to root
    if (!parent || !ev)
        return;
}

void Darling_fireGesture(Panel *target, GestureEvent *ev) {
    ;;INCOMPLETE // capture walk root to target, target phase, bubble walk target to root
    if (!target || !ev)
        return;
}
