#include "annotation/overview.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "darling/panel/expandable_list_container.h"
#include "darling/panel/panel.h"
#include "darling/field/checkbox.h"
#include "darling/label/label.h"
#include "nio/mem.h"

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: ExpandableListContainerTest (expandable_list_container_test.c)
 * LEVEL: L3 — Module Code (headless verification harness)
 * ============================================================================
 * Headless suite for ExpandableListContainer: null safety, add/expand/collapse,
 * hierarchy/depthLevelValue, checklist mode, subtree removal, clear, and parts.
 *
 * STRUCT FIELDS: none — procedural test harness.
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - main(void)
 * ============================================================================
 */

int main(void) {
    printf("=== Running ExpandableListContainer Test Suite ===\n");

    // §1 Null safety — every public API with nullptr self
    assert(ExpandableListContainer_nodeCount(nullptr) == 0);
    assert(ExpandableListContainer_addNode(nullptr, EXPANDABLE_LIST_ROOT, "X") == EXPANDABLE_LIST_ROOT);
    ExpandableListContainer_removeNode(nullptr, 0);
    ExpandableListContainer_expand(nullptr, 0);
    ExpandableListContainer_collapse(nullptr, 0);
    ExpandableListContainer_toggle(nullptr, 0);
    ExpandableListContainer_setChecked(nullptr, 0, true);
    assert(ExpandableListContainer_isChecked(nullptr, 0) == false);
    ExpandableListContainer_setChecklistMode(nullptr, true);
    assert(ExpandableListContainer_isChecklistMode(nullptr) == false);
    ExpandableListContainer_layout(nullptr);
    assert(ExpandableListContainer_isExpanded(nullptr, 0) == false);
    assert(ExpandableListContainer_getParent(nullptr, 0) == EXPANDABLE_LIST_ROOT);
    assert(ExpandableListContainer_getDepth(nullptr, 0) == 0);
    assert(ExpandableListContainer_getDepthLevelValue(nullptr, 0) == 0.0f);
    uint32_t outStartNull = 0;
    uint32_t outCountNull = 0;
    ExpandableListContainer_getChildren(nullptr, EXPANDABLE_LIST_ROOT, &outStartNull, &outCountNull);
    assert(outStartNull == 0);
    assert(outCountNull == 0);
    ExpandableListContainer_setIndentSpacing(nullptr, 20.0f);
    ExpandableListContainer_setRowHeight(nullptr, 22.0f);
    assert(ExpandableListContainer_getIndentSpacing(nullptr) == 0.0f);
    assert(ExpandableListContainer_getRowHeight(nullptr) == 0.0f);
    assert(ExpandableListContainer_getNode(nullptr, 0) == nullptr);
    assert(ExpandableListContainer_part_row(nullptr, 0) == nullptr);
    assert(ExpandableListContainer_part_childPanel(nullptr, 0) == nullptr);
    assert(ExpandableListContainer_part_chevron(nullptr, 0) == nullptr);
    assert(ExpandableListContainer_part_label(nullptr, 0) == nullptr);
    assert(ExpandableListContainer_part_checkbox(nullptr, 0) == nullptr);

    // §2 Instantiate and verify empty state
    ExpandableListContainer *elc = ExpandableListContainer_0();
    assert(elc != nullptr);
    assert(ExpandableListContainer_nodeCount(elc) == 0);
    assert(ExpandableListContainer_isChecklistMode(elc) == false);

    // §3 Add root nodes and verify hierarchy
    uint32_t iA = ExpandableListContainer_addNode(elc, EXPANDABLE_LIST_ROOT, "src");
    uint32_t iB = ExpandableListContainer_addNode(elc, EXPANDABLE_LIST_ROOT, "docs");
    assert(iA == 0);
    assert(iB == 1);
    assert(ExpandableListContainer_nodeCount(elc) == 2);
    assert(ExpandableListContainer_getParent(elc, 0) == EXPANDABLE_LIST_ROOT);
    assert(ExpandableListContainer_getParent(elc, 1) == EXPANDABLE_LIST_ROOT);
    assert(ExpandableListContainer_getDepth(elc, 0) == 0);
    assert(ExpandableListContainer_getDepth(elc, 1) == 0);
    assert(ExpandableListContainer_getDepthLevelValue(elc, 0) == 0.0f);
    assert(ExpandableListContainer_getDepthLevelValue(elc, 1) == 0.0f);

    // §4 Add children under src; verify depth and pre-order contiguity
    uint32_t iA1 = ExpandableListContainer_addNode(elc, 0, "a.c");
    uint32_t iA2 = ExpandableListContainer_addNode(elc, 0, "b.c");
    uint32_t iC = ExpandableListContainer_addNode(elc, EXPANDABLE_LIST_ROOT, "lib");
    assert(iA1 == 1);                // src's subtree is internal: [src, a.c, docs, …]
    assert(iA2 == 2);
    assert(iC == 4);                 // roots appended after src's subtree (pre-order)
    assert(ExpandableListContainer_nodeCount(elc) == 5);
    assert(ExpandableListContainer_getParent(elc, 1) == 0);
    assert(ExpandableListContainer_getParent(elc, 2) == 0);
    assert(ExpandableListContainer_getDepth(elc, 1) == 1);
    assert(ExpandableListContainer_getDepth(elc, 2) == 1);
    assert(ExpandableListContainer_getDepthLevelValue(elc, 1) == 20.0f);  // 1 * 20
    assert(ExpandableListContainer_getDepthLevelValue(elc, 2) == 20.0f);

    // §5 getChildren of src(0) returns range [1, 2]
    uint32_t childStart = 0;
    uint32_t childCount = 0;
    ExpandableListContainer_getChildren(elc, 0, &childStart, &childCount);
    assert(childStart == 1);
    assert(childCount == 2);

    // §6 expand/collapse src(0)
    assert(ExpandableListContainer_isExpanded(elc, 0) == false);
    assert(ExpandableListContainer_expand(elc, 0) == true);
    assert(ExpandableListContainer_isExpanded(elc, 0) == true);
    assert(ExpandableListContainer_expand(elc, 0) == true);  // no-op
    ExpandableListContainer_collapse(elc, 0);
    assert(ExpandableListContainer_isExpanded(elc, 0) == false);
    ExpandableListContainer_toggle(elc, 0);
    assert(ExpandableListContainer_isExpanded(elc, 0) == true);
    ExpandableListContainer_toggle(elc, 0);
    assert(ExpandableListContainer_isExpanded(elc, 0) == false);

    // §7 Parts: row/childPanel/chevron/label not null; checkbox null when not checklist
    assert(ExpandableListContainer_part_row(elc, 0) != nullptr);
    assert(ExpandableListContainer_part_childPanel(elc, 0) != nullptr);
    assert(ExpandableListContainer_part_chevron(elc, 0) != nullptr);
    assert(ExpandableListContainer_part_label(elc, 0) != nullptr);
    assert(ExpandableListContainer_part_checkbox(elc, 0) == nullptr);

    // §8 Checklist mode — enable, setChecked, isChecked, toggle checkbox part
    ExpandableListContainer_setChecklistMode(elc, true);
    assert(ExpandableListContainer_isChecklistMode(elc) == true);
    assert(ExpandableListContainer_part_checkbox(elc, 0) != nullptr);   // box now present
    ExpandableListContainer_setChecked(elc, 0, true);
    assert(ExpandableListContainer_isChecked(elc, 0) == true);
    ExpandableListContainer_setChecked(elc, 0, false);
    assert(ExpandableListContainer_isChecked(elc, 0) == false);

    // §9 Remove src(0) subtree [0,1,2]: docs(1)→index 0, lib(4)→index 1
    ExpandableListContainer_removeNode(elc, 0);
    assert(ExpandableListContainer_nodeCount(elc) == 2);
    assert(ExpandableListContainer_getParent(elc, 0) == EXPANDABLE_LIST_ROOT);
    assert(ExpandableListContainer_getParent(elc, 1) == EXPANDABLE_LIST_ROOT);
    assert(ExpandableListContainer_getDepth(elc, 0) == 0);
    assert(ExpandableListContainer_getDepth(elc, 1) == 0);

    // §10 Clear
    ExpandableListContainer_addNode(elc, EXPANDABLE_LIST_ROOT, "x");
    assert(ExpandableListContainer_nodeCount(elc) == 3);
    ExpandableListContainer_clear(elc);
    assert(ExpandableListContainer_nodeCount(elc) == 0);

    Memory_free(elc);
    printf("=== ExpandableListContainer Test PASS ===\n");
    return 0;
}
