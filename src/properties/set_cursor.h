#ifndef DARLING_PROPERTIES_SET_CURSOR_H
#define DARLING_PROPERTIES_SET_CURSOR_H

#include "ui/element.h"
#include "window/window.h"

// Per-class hover preference, not an immediate global cursor change.
// Inherit walks ancestors; arrow explicitly overrides an ancestor preference.
typedef enum CursorType {
    CURSOR_INHERIT = -1,
    CURSOR_ARROW = WINDOW_CURSOR_DEFAULT,
    CURSOR_TEXT = WINDOW_CURSOR_IBEAM,
    CURSOR_POINTER = WINDOW_CURSOR_POINTING_HAND,
    CURSOR_CROSSHAIR = WINDOW_CURSOR_CROSSHAIR,
    CURSOR_RESIZE_EW = WINDOW_CURSOR_RESIZE_EW,
    CURSOR_RESIZE_NS = WINDOW_CURSOR_RESIZE_NS,
    CURSOR_NOT_ALLOWED = WINDOW_CURSOR_NOT_ALLOWED,
    CURSOR_HIDDEN = WINDOW_CURSOR_HIDDEN
} CursorType;

typedef struct Panel Panel;
typedef struct ScrollPanel ScrollPanel;
typedef struct Frame Frame;

Element *Element_setCursor(Element *element, CursorType cursor);
// Deepest visible hit wins; absent/inherited preferences walk up to root only.
// A miss or no preference resolves to arrow. Rounded/viewport clips apply.
CursorType Element_cursorAt(Element *root, float x, float y);

#define DECLARE_CURSOR(Class) Class *Class##_setCursor(Class *self, CursorType cursor)
#define IMPLEMENT_CURSOR_WITH(Class, ELEMENT_OF)                         \
    Class *Class##_setCursor(Class *self, CursorType cursor) {            \
        Element_setCursor(ELEMENT_OF(self), cursor);                     \
        return self;                                                    \
    }
#define IMPLEMENT_CURSOR(Class) IMPLEMENT_CURSOR_WITH(Class, Class##_graphics)

DECLARE_CURSOR(Panel);
DECLARE_CURSOR(ScrollPanel);
DECLARE_CURSOR(Frame);

// Future widgets opt in once they have a real Element accessor, e.g.
// DECLARE_CURSOR(TextArea); IMPLEMENT_CURSOR(TextArea);
// This does not fabricate runtime APIs for draft-only widget types.

#endif
