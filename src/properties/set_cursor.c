#include "properties/set_cursor.h"

#include "frame/frame.h"
#include "panel/panel.h"
#include "panel/scroll_panel.h"
#include "annotation/definition.h"
#include "annotation/overview.h"

;;DEFINITION
/**
 * Hover cursors are per-node preferences, not global window state and not
 * shared paint bounds. Class-named setters are generated here, outside widget
 * implementations. The input bridge resolves the deepest visible hit and
 * walks inherited preferences to the supplied root, then applies one native
 * cursor. No registry, callbacks or borrowed keys survive node destruction.
 * Invalid values are ignored; resetting to inherit removes the override.
 */
;;OVERVIEW
/**
 * MODULE: SetCursor (properties/set_cursor.c)
 * No owned fields or file-static state.
 * Public API (set_cursor.h): Element_setCursor, Element_cursorAt;
 * Panel_setCursor, ScrollPanel_setCursor, Frame_setCursor (macro-generated).
 * Macros: DECLARE_CURSOR, IMPLEMENT_CURSOR, IMPLEMENT_CURSOR_WITH.
 * Native application belongs to input/pointer.c and the R1 Window host.
 */

Element *Element_setCursor(Element *element, CursorType cursor) {
    if (cursor >= CURSOR_INHERIT && cursor <= CURSOR_HIDDEN)
        Element_setCursorPreference(element, cursor);
    return element;
}

// Resolves the cursor preference at the deepest hit, inheriting through ancestors.
CursorType Element_cursorAt(Element *root, float x, float y) {
    for (Element *hit = Element_hit(root, x, y); hit; hit = Element_parent(hit)) {
        int cursor = Element_cursorPreference(hit);
        if (cursor >= CURSOR_ARROW && cursor <= CURSOR_HIDDEN) return (CursorType) cursor;
        if (hit == root) break;
    }
    return CURSOR_ARROW;
}

IMPLEMENT_CURSOR(Panel)
IMPLEMENT_CURSOR(ScrollPanel)
IMPLEMENT_CURSOR_WITH(Frame, Frame_element)
