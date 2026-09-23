// tests/flex_container_test.c — headless proof for FlexContainer.
//
// MODULE harness (procedural entry, no owned struct): cold-seam matrix
// (nullptr / empty / bad-char / mixed-op / unbalanced / duplicate /
// missing / out-of-range / truncation) plus hot-layout geometry checks
// ({1>{2v3}} and 1>2>{4v5}>3 goldens, grip ratios, drag deltas, spacing).
// Pure Component math — no window, no GPU.

#include "darling/component.h"
#include "darling/panel/flex_container.h"
#include "darling/panel/panel.h"
#include "lang/graphics_component.h"

#include <math.h>
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

static bool near(float a, float b) {
    float d = a - b;
    if (d < 0.0f)
        d = -d;
    return d < 0.01f;
}

static float kidX(Panel *kid) {
    return GraphicsComponent_getX(&(*kid).component);
}

static float kidY(Panel *kid) {
    return GraphicsComponent_getY(&(*kid).component);
}

static float kidW(Panel *kid) {
    return GraphicsComponent_getWidth(&(*kid).component);
}

static float kidH(Panel *kid) {
    return GraphicsComponent_getHeight(&(*kid).component);
}

int main(void) {
    // section 1 nullptr guards (cold-strict: never crash, fail closed)
    check(!FlexContainer_setSpec(nullptr, "1"), "null-self-spec");
    check(!FlexContainer_setSpec(nullptr, nullptr), "null-self-null-spec");
    FlexContainer_layout(nullptr);
    check(FlexContainer_count(nullptr) == 0u, "null-count");
    check(FlexContainer_groupCount(nullptr) == 0u, "null-groups");
    check(!FlexContainer_setRatio(nullptr, 0u, 0u, 1.0f), "null-set-ratio");
    check(FlexContainer_getRatio(nullptr, 0u, 0u) == 0.0f, "null-get-ratio");
    check(!FlexContainer_dragGrip(nullptr, 0u, 0u, 1.0f, 10.0f), "null-drag");
    check(!FlexContainer_getSpec(nullptr, nullptr, 0u, nullptr), "null-get-spec");
    char buf[256];
    bool trunc = false;
    check(FlexContainer_toString(nullptr, buf, sizeof(buf), &trunc), "null-to-string");
    check(FlexContainer_toStringStruct(nullptr, buf, sizeof(buf), &trunc), "null-to-struct");

    // section 2 empty flex
    FlexContainer *f = FlexContainer_0();
    check(f != nullptr, "construct");
    check(FlexContainer_setSpec(f, ""), "empty-spec-empty-flex");
    check(!FlexContainer_setSpec(f, "1"), "id-spec-no-children");
    check(!FlexContainer_setSpec(f, nullptr), "null-spec-text");

    // section 3 flat row golden: 1>2>3 over 300x200
    Panel *p1 = Panel_0();
    Panel *p2 = Panel_0();
    Panel *p3 = Panel_0();
    check(p1 && p2 && p3, "child-panels");
    FlexContainer_add(f, p1);
    FlexContainer_add(f, p2);
    FlexContainer_add(f, p3);
    FlexGraphicsComponent_setSize(f, 300.0f, 200.0f);
    check(FlexContainer_setSpec(f, "1>2>3"), "flat-row-spec");
    check(near(kidX(p1), 0.0f) && near(kidW(p1), 100.0f), "row-c1");
    check(near(kidX(p2), 100.0f) && near(kidW(p2), 100.0f), "row-c2");
    check(near(kidX(p3), 200.0f) && near(kidW(p3), 100.0f), "row-c3");
    check(near(kidY(p1), 0.0f) && near(kidH(p1), 200.0f), "row-fill");

    // section 4 bad specs fail closed, layout untouched
    check(!FlexContainer_setSpec(f, "1>2"), "missing-id");
    check(!FlexContainer_setSpec(f, "1>2>3>4"), "out-of-range");
    check(!FlexContainer_setSpec(f, "1>2>2"), "duplicate-id");
    check(!FlexContainer_setSpec(f, "1>2v3"), "mixed-ops");
    check(!FlexContainer_setSpec(f, "{1>2"), "unbalanced-open");
    check(!FlexContainer_setSpec(f, "1>2}"), "unbalanced-close");
    check(!FlexContainer_setSpec(f, ""), "empty-with-children");
    check(!FlexContainer_setSpec(f, "abc"), "bad-chars");
    check(!FlexContainer_setSpec(f, "0>1>2"), "zero-id");
    check(near(kidX(p1), 0.0f) && near(kidW(p1), 100.0f), "layout-kept");

    // section 5 nested golden: {1>{2v3}} over 300x200
    check(FlexContainer_setSpec(f, "{1>{2v3}}"), "nested-spec");
    check(FlexContainer_groupCount(f) == 2u, "two-groups");
    check(near(kidX(p1), 0.0f) && near(kidW(p1), 150.0f), "nest-c1");
    check(near(kidX(p2), 150.0f) && near(kidY(p2), 0.0f), "nest-c2-pos");
    check(near(kidW(p2), 150.0f) && near(kidH(p2), 100.0f), "nest-c2-size");
    check(near(kidX(p3), 150.0f) && near(kidY(p3), 100.0f), "nest-c3-pos");
    check(near(kidW(p3), 150.0f) && near(kidH(p3), 100.0f), "nest-c3-size");
    check(FlexContainer_getSpec(f, buf, sizeof(buf), &trunc) && !trunc, "spec-emit");
    check(buf[0] == '1' && buf[1] == '>', "spec-roundtrip");

    // section 6 grips: ratios + drag deltas on the flat row
    check(FlexContainer_setSpec(f, "1>2>3"), "row-again");
    check(FlexContainer_setRatio(f, 0u, 0u, 3.0f), "set-ratio");
    check(near(kidW(p1), 180.0f) && near(kidW(p2), 60.0f), "ratio-widths");
    check(!FlexContainer_setRatio(f, 9u, 0u, 1.0f), "bad-group-ratio");
    check(!FlexContainer_setRatio(f, 0u, 9u, 1.0f), "bad-grip-ratio");
    check(FlexContainer_dragGrip(f, 0u, 0u, -60.0f, 300.0f), "drag");
    check(near(kidW(p1), 150.0f) && near(kidW(p2), 75.0f), "drag-widths");
    check(!FlexContainer_dragGrip(f, 0u, 0u, 1.0f, 0.0f), "zero-extent");

    // section 7 spacing
    FlexContainer_setSpacing(f, 10.0f);
    check(FlexContainer_getSpacing(f) == 10.0f, "spacing-get");
    check(FlexContainer_setSpec(f, "1>2>3"), "row-spaced");
    FlexGraphicsComponent_setSize(f, 320.0f, 200.0f);
    FlexContainer_layout(f);
    check(near(kidW(p1), 100.0f) && near(kidX(p2), 110.0f), "spacing-layout");
    FlexContainer_setSpacing(f, -5.0f);
    check(FlexContainer_getSpacing(f) == 0.0f, "spacing-clamp");

    // section 8 five-panel golden: 1>2>{4v5}>3 over 400x200
    FlexContainer *g = FlexContainer_0();
    Panel *q1 = Panel_0();
    Panel *q2 = Panel_0();
    Panel *q3 = Panel_0();
    Panel *q4 = Panel_0();
    Panel *q5 = Panel_0();
    FlexContainer_add(g, q1);
    FlexContainer_add(g, q2);
    FlexContainer_add(g, q3);
    FlexContainer_add(g, q4);
    FlexContainer_add(g, q5);
    FlexGraphicsComponent_setSize(g, 400.0f, 200.0f);
    check(FlexContainer_setSpec(g, "1>2>{4v5}>3"), "five-spec");
    check(near(kidX(q1), 0.0f) && near(kidW(q1), 100.0f), "five-c1");
    check(near(kidX(q2), 100.0f) && near(kidW(q2), 100.0f), "five-c2");
    check(near(kidX(q4), 200.0f) && near(kidY(q4), 0.0f), "five-c4-pos");
    check(near(kidW(q4), 100.0f) && near(kidH(q4), 100.0f), "five-c4-size");
    check(near(kidX(q5), 200.0f) && near(kidY(q5), 100.0f), "five-c5-pos");
    check(near(kidX(q3), 300.0f) && near(kidW(q3), 100.0f), "five-c3");

    // section 9 truncation + strings
    char tiny[4];
    bool t2 = false;
    check(!FlexContainer_getSpec(g, tiny, sizeof(tiny), &t2) && t2, "spec-trunc");
    check(FlexContainer_toString(g, buf, sizeof(buf), &trunc) && !trunc, "to-string");
    check(FlexContainer_toStringStruct(g, buf, sizeof(buf), &trunc) && !trunc, "to-struct");

    if (failures == 0)
        printf("flex_container_test: all green\n");
    return failures == 0 ? 0 : 1;
}
