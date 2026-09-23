// tests/layered_container_test.c — headless proof for LayeredContainer.
//
// MODULE harness (procedural entry, no owned struct): cold-seam matrix
// (nullptr / negative / out-of-range / self-pin) plus slot-pinning
// (out-of-order fill keeps identity), show/hide, sort (move/raise/lower),
// shrink-detach, full-rect layout, and string forms. Pure Component math
// — no window, no GPU, no layer bridge.

#include "darling/component.h"
#include "darling/panel/layered_container.h"
#include "darling/panel/panel.h"
#include "lang/graphics_component.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

static int failures;

static void check(bool cond, const char *name) {
    if (!cond) {
        failures++;
        printf("FAIL %s\n", name);
    } else {
        printf("ok %s\n", name);
    }
}

int main(void) {
    // section 1 nullptr guards (cold-strict: never crash, fail closed)
    LayeredContainer_setPaneCount(nullptr, 3);
    LayeredContainer_setContainer(nullptr, 0, nullptr);
    LayeredContainer_setPaneVisible(nullptr, 0, true);
    LayeredContainer_layout(nullptr);
    check(LayeredContainer_getPaneCount(nullptr) == 0, "null-count");
    check(LayeredContainer_getContainer(nullptr, 0) == nullptr, "null-get");
    check(!LayeredContainer_isPaneVisible(nullptr, 0), "null-visible");
    check(!LayeredContainer_move(nullptr, 0, 1), "null-move");
    check(!LayeredContainer_raise(nullptr, 0), "null-raise");
    check(!LayeredContainer_lower(nullptr, 0), "null-lower");
    check(LayeredContainer_occupiedCount(nullptr) == 0, "null-occupied");
    char buf[256];
    bool trunc = false;
    check(LayeredContainer_toString(nullptr, buf, sizeof(buf), &trunc), "null-to-string");
    check(LayeredContainer_toStringStruct(nullptr, buf, sizeof(buf), &trunc), "null-to-struct");

    // section 2 pinning: out-of-order fill keeps identity by slot
    LayeredContainer *lc = LayeredContainer_0();
    check(lc != nullptr, "construct");
    LayeredContainer_setPaneCount(lc, 3);
    check(LayeredContainer_getPaneCount(lc) == 3, "three-panes");
    Panel *a = Panel_0();
    Panel *b = Panel_0();
    Panel *c = Panel_0();
    check(a && b && c, "child-panels");
    LayeredContainer_setContainer(lc, 2, c);
    LayeredContainer_setContainer(lc, 0, a);
    check(LayeredContainer_getContainer(lc, 2) == c, "pin-high");
    check(LayeredContainer_getContainer(lc, 0) == a, "pin-low");
    check(LayeredContainer_getContainer(lc, 1) == nullptr, "pin-empty");
    check(LayeredContainer_occupiedCount(lc) == 2, "occupied");
    check(LayeredContainer_getContainer(lc, 9) == nullptr, "pane-oob");
    LayeredContainer_setContainer(lc, 9, b);
    check(LayeredContainer_occupiedCount(lc) == 2, "oob-kept");

    // section 3 one panel, one pane: re-pin de-pins first
    LayeredContainer_setContainer(lc, 1, c);
    check(LayeredContainer_getContainer(lc, 2) == nullptr, "depin-old");
    check(LayeredContainer_getContainer(lc, 1) == c, "repin-new");
    check(LayeredContainer_occupiedCount(lc) == 2, "occupied-kept");

    // section 4 show/hide flips occupant state
    check(LayeredContainer_isPaneVisible(lc, 0), "shown-default");
    LayeredContainer_setPaneVisible(lc, 0, false);
    check(!LayeredContainer_isPaneVisible(lc, 0), "hidden-bit");
    check(!Panel_isVisible(a), "hidden-child");
    LayeredContainer_setPaneVisible(lc, 0, true);
    check(Panel_isVisible(a), "reshown-child");
    LayeredContainer_setPaneVisible(lc, 9, false);
    check(LayeredContainer_occupiedCount(lc) == 2, "oob-visible-kept");

    // section 5 layout stacks occupants over the full rect
    LayeredGraphicsComponent_setSize(lc, 200.0f, 100.0f);
    LayeredContainer_setContainer(lc, 1, b);
    LayeredContainer_layout(lc);
    Component *ac = &(*a).component;
    Component *bc = &(*b).component;
    check(GraphicsComponent_getX(ac) == 0.0f && GraphicsComponent_getY(ac) == 0.0f, "full-origin");
    check(GraphicsComponent_getWidth(ac) == 200.0f && GraphicsComponent_getHeight(ac) == 100.0f, "full-size-a");
    check(GraphicsComponent_getWidth(bc) == 200.0f && GraphicsComponent_getHeight(bc) == 100.0f, "full-size-b");

    // section 6 sort: move/raise/lower reorder the stack
    check(LayeredContainer_move(lc, 0, 2), "move");
    check(LayeredContainer_getContainer(lc, 2) == a, "moved-top");
    check(LayeredContainer_getContainer(lc, 0) == b, "moved-bottom");
    check(!LayeredContainer_move(lc, 0, 9), "move-oob");
    check(!LayeredContainer_move(lc, 9, 0), "move-oob-from");
    check(LayeredContainer_raise(lc, 0), "raise");
    check(LayeredContainer_getContainer(lc, 2) == b, "raised-top");
    check(LayeredContainer_lower(lc, 2), "lower");
    check(LayeredContainer_getContainer(lc, 0) == b, "lowered-bottom");
    check(!LayeredContainer_raise(lc, 9), "raise-oob");

    // section 7 shrink detaches dropped occupants, keeps the rest
    LayeredContainer_setPaneCount(lc, 1);
    check(LayeredContainer_getPaneCount(lc) == 1, "shrunk");
    check(LayeredContainer_occupiedCount(lc) == 1, "shrink-occupied");
    check(Panel_childCount(&(*lc).base) == 1u, "shrink-tree");
    LayeredContainer_setContainer(lc, 0, nullptr);
    check(LayeredContainer_occupiedCount(lc) == 0, "cleared");
    check(Panel_childCount(&(*lc).base) == 0u, "clear-tree");

    // section 8 strings
    LayeredContainer_setPaneCount(lc, 2);
    LayeredContainer_setContainer(lc, 0, a);
    check(LayeredContainer_toString(lc, buf, sizeof(buf), &trunc) && !trunc, "to-string");
    check(LayeredContainer_toStringStruct(lc, buf, sizeof(buf), &trunc) && !trunc, "to-struct");
    char tiny[4];
    bool t2 = false;
    check(!LayeredContainer_toString(lc, tiny, sizeof(tiny), &t2) && t2, "string-trunc");

    if (failures == 0)
        printf("layered_container_test: all green\n");
    return failures == 0 ? 0 : 1;
}
