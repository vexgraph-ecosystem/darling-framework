#ifndef DARLING_C23_EVENT_INVOKE_H
#define DARLING_C23_EVENT_INVOKE_H

#include <stdbool.h>
#include <stdint.h>

#include "ui/element.h"

// darling R4 — c23/event_invoke.h
//
// THE EVENT INVOKER. Every element is an event target. Each event KIND has a
// handler struct (its ()()s); each class opts in with IMPLEMENT_<KIND>_EVENT,
// which generates <Class>_add<Kind>Event from the class name — the `##` IS the
// class:
//
//   // textarea.h
//   DECLARE_DOCUMENT_EVENT(Textarea);
//   // textarea.c
//   IMPLEMENT_EVENT(Textarea);      // all kinds, or one:
//   IMPLEMENT_DOCUMENT_EVENT(Textarea);   // -> Textarea_addDocumentEvent(...)
//
// The generated wrapper finds the class's Element through <Class>_graphics and
// registers there. The base register is Element_add<Kind>Event (Element is
// itself a target). Dispatch hit-tests the tree and BUBBLES: the deepest element
// under the point runs first, then each ancestor that registered.

// ── event kinds ─────────────────────────────────────────────────────────────
enum {
    EV_MOUSE_DOWN = 0, EV_MOUSE_UP, EV_MOUSE_MOVE, EV_MOUSE_DRAG,
    EV_MOUSE_ENTER, EV_MOUSE_LEAVE,
    EV_SCROLL, EV_ZOOM,
    EV_KEY_DOWN, EV_KEY_UP,
    EV_TOUCH_DOWN, EV_TOUCH_MOVE, EV_TOUCH_UP,
    EV_DOCUMENT_CHANGED, EV_DOCUMENT_SELECTED,
    EV_KIND_COUNT
};

// ── payloads (what a handler receives) ──────────────────────────────────────
typedef struct Mouse { float x, y; int button; } Mouse;
typedef struct Scroll { float x, y, dx, dy; } Scroll;
typedef struct Zoom { float x, y, magnitude; } Zoom;
typedef struct Key { int code; } Key;
typedef struct Touch { float x, y; int id; } Touch;
typedef struct Document { const char *text; uint32_t start, end; } Document;

// ── handler structs (the ()()s) ─────────────────────────────────────────────
typedef struct MouseEvent {
    void (*onDown)(Element *element, const Mouse *mouse, void *userdata);
    void (*onUp)(Element *element, const Mouse *mouse, void *userdata);
    void (*onMove)(Element *element, const Mouse *mouse, void *userdata);
    void (*onDrag)(Element *element, const Mouse *mouse, void *userdata);
    void (*onEnter)(Element *element, const Mouse *mouse, void *userdata);
    void (*onLeave)(Element *element, const Mouse *mouse, void *userdata);
    void *userdata;
} MouseEvent;
typedef struct ScrollEvent {
    void (*onScroll)(Element *element, const Scroll *scroll, void *userdata);
    void *userdata;
} ScrollEvent;
typedef struct ZoomEvent {
    void (*onZoom)(Element *element, const Zoom *zoom, void *userdata);
    void *userdata;
} ZoomEvent;
typedef struct KeyEvent {
    void (*onDown)(Element *element, const Key *key, void *userdata);
    void (*onUp)(Element *element, const Key *key, void *userdata);
    void *userdata;
} KeyEvent;
typedef struct TouchEvent {
    void (*onDown)(Element *element, const Touch *touch, void *userdata);
    void (*onMove)(Element *element, const Touch *touch, void *userdata);
    void (*onUp)(Element *element, const Touch *touch, void *userdata);
    void *userdata;
} TouchEvent;
typedef struct DocumentEvent {
    void (*onChanged)(Element *element, const Document *document, void *userdata);
    void (*onSelected)(Element *element, const Document *document, void *userdata);
    void *userdata;
} DocumentEvent;

// ── the base register (Element is itself a target) ──────────────────────────
Element *Element_addMouseEvent(Element *element, MouseEvent handlers);
Element *Element_addScrollEvent(Element *element, ScrollEvent handlers);
Element *Element_addZoomEvent(Element *element, ZoomEvent handlers);
Element *Element_addKeyEvent(Element *element, KeyEvent handlers);
Element *Element_addTouchEvent(Element *element, TouchEvent handlers);
Element *Element_addDocumentEvent(Element *element, DocumentEvent handlers);

// ── declarations (put in a class header) ────────────────────────────────────
#define DECLARE_MOUSE_EVENT(Class)    Class *Class##_addMouseEvent(Class *self, MouseEvent handlers)
#define DECLARE_SCROLL_EVENT(Class)   Class *Class##_addScrollEvent(Class *self, ScrollEvent handlers)
#define DECLARE_ZOOM_EVENT(Class)     Class *Class##_addZoomEvent(Class *self, ZoomEvent handlers)
#define DECLARE_KEY_EVENT(Class)      Class *Class##_addKeyEvent(Class *self, KeyEvent handlers)
#define DECLARE_TOUCH_EVENT(Class)    Class *Class##_addTouchEvent(Class *self, TouchEvent handlers)
#define DECLARE_DOCUMENT_EVENT(Class) Class *Class##_addDocumentEvent(Class *self, DocumentEvent handlers)
#define DECLARE_EVENTS(Class)                                                       \
    DECLARE_MOUSE_EVENT(Class); DECLARE_SCROLL_EVENT(Class); DECLARE_ZOOM_EVENT(Class); \
    DECLARE_KEY_EVENT(Class); DECLARE_TOUCH_EVENT(Class); DECLARE_DOCUMENT_EVENT(Class)

// ── implementations (put in a class .c; the `##` IS the class) ──────────────
#define IMPLEMENT_MOUSE_EVENT(Class)    Class *Class##_addMouseEvent(Class *self, MouseEvent h)    { Element_addMouseEvent(Class##_graphics(self), h);    return self; }
#define IMPLEMENT_SCROLL_EVENT(Class)   Class *Class##_addScrollEvent(Class *self, ScrollEvent h)  { Element_addScrollEvent(Class##_graphics(self), h);   return self; }
#define IMPLEMENT_ZOOM_EVENT(Class)     Class *Class##_addZoomEvent(Class *self, ZoomEvent h)      { Element_addZoomEvent(Class##_graphics(self), h);     return self; }
#define IMPLEMENT_KEY_EVENT(Class)      Class *Class##_addKeyEvent(Class *self, KeyEvent h)        { Element_addKeyEvent(Class##_graphics(self), h);      return self; }
#define IMPLEMENT_TOUCH_EVENT(Class)    Class *Class##_addTouchEvent(Class *self, TouchEvent h)    { Element_addTouchEvent(Class##_graphics(self), h);    return self; }
#define IMPLEMENT_DOCUMENT_EVENT(Class) Class *Class##_addDocumentEvent(Class *self, DocumentEvent h) { Element_addDocumentEvent(Class##_graphics(self), h); return self; }
#define IMPLEMENT_EVENTS(Class)                                                     \
    IMPLEMENT_MOUSE_EVENT(Class) IMPLEMENT_SCROLL_EVENT(Class) IMPLEMENT_ZOOM_EVENT(Class) \
    IMPLEMENT_KEY_EVENT(Class) IMPLEMENT_TOUCH_EVENT(Class) IMPLEMENT_DOCUMENT_EVENT(Class)

// ── dispatch ────────────────────────────────────────────────────────────────
typedef struct Event {
    int   kind;
    float x, y;         // pointer (native px, top-left origin)
    float dx, dy;       // scroll / drag delta
    float magnitude;    // zoom scale (1.0 = none)
    int   key;          // key code or drag button
    void *native;       // borrowed OS event (nullable)
} Event;

bool Element_dispatchEvent(Element *root, const Event *event);
void Element_clearEvents(Element *root);
int  Element_eventBindingCount(void);

#endif // DARLING_C23_EVENT_INVOKE_H
