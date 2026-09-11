#ifndef DARLING_EXPANDABLE_LIST_CONTAINER_H
#define DARLING_EXPANDABLE_LIST_CONTAINER_H

#include <stdbool.h>
#include <stdint.h>

#include "annotation/intention.h"
#include "c23/constructor.h"
#include "darling/panel/panel.h"
#include "oop/type.h"

// darling/panel/expandable_list_container.h — node-oriented tree list with
// data-oriented flat storage.
//
// The tree ("expandable item with containers inside") lives as a flat
// pre-order array of ExpandableNode slot records. Every node owns a header
// row Panel (chevron + text label, plus a Checkbox part in checklist mode)
// and a collapsible child container Panel. Parent + child ranges are plain
// index numbers, so expansion/collapse never chases pointers: it flips a
// bool and re-runs the recursive layout over the array. Indentation is
// "move the offset right": each child container is positioned at
// indentSpacing inside the parent's line, and depthLevelValue = depth *
// indentSpacing rides on the slot record for inspection and doc space.
//
// File explorers, IDE sidebars, schema outlines, checklists. The flat
// array keeps thousands of files cache-friendly, the index API keeps tree
// semantics OOP-ergonomic (Rule 36).



// Root sentinel for ExpandableNode.parentIndex.
#define EXPANDABLE_LIST_ROOT UINT32_MAX

// SLOT RECORD (behaviorless row owned by ExpandableListContainer only; zero
// ExpandableNode_* verbs — all behavior hangs off the owner, Rule 3).
typedef struct ExpandableNode {
    uint32_t parentIndex;    // EXPANDABLE_LIST_ROOT = root; else an index < this one
    uint32_t childStart;     // first child index in the flat pre-order array
    uint32_t childCount;     // contiguous children after childStart
    uint32_t depth;          // nesting level (0 = root); recomputed on add/remove/setIndent
    float depthLevelValue;   // document px indent = depth * indentSpacing
    bool expanded;           // true = childPanel visible (chevron "^"); false = ">"
    bool checked;            // checklist state (source of truth lives in data, not the box)
    Panel *row;              // borrowed header row (chevron + label [+ checkbox])
    Panel *childPanel;       // borrowed collapsible child holder (sibling of row)
} ExpandableNode;

typedef struct ExpandableListContainer {
    Panel base;
    ExpandableNode *nodes;   // flat slot array (single source of truth)
    uint32_t nodeCount;
    uint32_t nodeCap;
    float indentSpacing;     // px per depth level (default 20)
    float rowHeight;         // px per header row (default 22)
    bool checklistMode;      // true = rows carry a Checkbox part
    uint32_t chevronCollapsed;  // chevron glyph when collapsed (default '>')
    uint32_t chevronExpanded;   // chevron glyph when expanded (default '^')
    uint32_t chevronColor;      // 0xAARRGGBB chevron text color
    uint32_t textColor;         // 0xAARRGGBB row label color
    uint32_t boxColor;          // 0xAARRGGBB checkbox box color
    uint32_t checkColor;        // 0xAARRGGBB checkbox check color
} ExpandableListContainer;

;;INTENTION("chevronCollapsed/chevronExpanded hold text glyphs until the darling icon/SVG seam lands; they become icon handles then.")

// Constructors:
//   ExpandableListContainer()                  — 20px indent, 22px rows
//   ExpandableListContainer(indentSpacing)     — custom indent, 22px rows
ExpandableListContainer *ExpandableListContainer_0(void);
ExpandableListContainer *ExpandableListContainer_1(float indentSpacing);

#define ExpandableListContainer(...) CONSTRUCTOR_DISPATCH(ExpandableListContainer, __VA_ARGS__)

// Core. addNode returns the node index in the flat array, or UINT32_MAX when
// the parent is invalid or allocation fails. detach-only: nodes never free
// their row/childPanel views (the arena owns panels).
uint32_t ExpandableListContainer_addNode(ExpandableListContainer *self, uint32_t parentIndex, const char *label);
bool ExpandableListContainer_removeNode(ExpandableListContainer *self, uint32_t nodeIndex);
void ExpandableListContainer_clear(ExpandableListContainer *self);
bool ExpandableListContainer_expand(ExpandableListContainer *self, uint32_t nodeIndex);
bool ExpandableListContainer_collapse(ExpandableListContainer *self, uint32_t nodeIndex);
void ExpandableListContainer_toggle(ExpandableListContainer *self, uint32_t nodeIndex);
void ExpandableListContainer_layout(ExpandableListContainer *self);
size_t ExpandableListContainer_nodeCount(const ExpandableListContainer *self);

// Checklist (checklistMode = rows carry a Checkbox part; state is the slot).
void ExpandableListContainer_setChecked(ExpandableListContainer *self, uint32_t nodeIndex, bool checked);
bool ExpandableListContainer_isChecked(const ExpandableListContainer *self, uint32_t nodeIndex);
void ExpandableListContainer_setChecklistMode(ExpandableListContainer *self, bool mode);
bool ExpandableListContainer_isChecklistMode(const ExpandableListContainer *self);

// Tree inspection (data-oriented; children are contiguous after childStart).
bool ExpandableListContainer_isExpanded(const ExpandableListContainer *self, uint32_t nodeIndex);
uint32_t ExpandableListContainer_getParent(const ExpandableListContainer *self, uint32_t nodeIndex);
uint32_t ExpandableListContainer_getDepth(const ExpandableListContainer *self, uint32_t nodeIndex);
float ExpandableListContainer_getDepthLevelValue(const ExpandableListContainer *self, uint32_t nodeIndex);
void ExpandableListContainer_getChildren(const ExpandableListContainer *self, uint32_t nodeIndex,
                                         uint32_t *outStart, uint32_t *outCount);

// Setters.
void ExpandableListContainer_setIndentSpacing(ExpandableListContainer *self, float spacing);
void ExpandableListContainer_setRowHeight(ExpandableListContainer *self, float height);
void ExpandableListContainer_setChevronGlyphCollapsed(ExpandableListContainer *self, uint32_t glyph);
void ExpandableListContainer_setChevronGlyphExpanded(ExpandableListContainer *self, uint32_t glyph);
void ExpandableListContainer_setChevronColor(ExpandableListContainer *self, uint32_t color);
void ExpandableListContainer_setTextColor(ExpandableListContainer *self, uint32_t color);
void ExpandableListContainer_setBoxColor(ExpandableListContainer *self, uint32_t color);
void ExpandableListContainer_setCheckColor(ExpandableListContainer *self, uint32_t color);

// Getters (Rule 24 safe defaults when self is null).
float ExpandableListContainer_getIndentSpacing(const ExpandableListContainer *self);
float ExpandableListContainer_getRowHeight(const ExpandableListContainer *self);
uint32_t ExpandableListContainer_getChevronGlyphCollapsed(const ExpandableListContainer *self);
uint32_t ExpandableListContainer_getChevronGlyphExpanded(const ExpandableListContainer *self);
uint32_t ExpandableListContainer_getChevronColor(const ExpandableListContainer *self);
uint32_t ExpandableListContainer_getTextColor(const ExpandableListContainer *self);
uint32_t ExpandableListContainer_getBoxColor(const ExpandableListContainer *self);
uint32_t ExpandableListContainer_getCheckColor(const ExpandableListContainer *self);
const ExpandableNode *ExpandableListContainer_getNode(const ExpandableListContainer *self, uint32_t nodeIndex);

// Parts (Rule 29: sub-objects are borrowed views, reached by part verbs only).
// The row owns chevron(0), checkbox(1 when in checklist mode), label(last);
// part_* resolve them without piercing container internals.
Panel *ExpandableListContainer_part_row(ExpandableListContainer *self, uint32_t nodeIndex);
Panel *ExpandableListContainer_part_childPanel(ExpandableListContainer *self, uint32_t nodeIndex);
Panel *ExpandableListContainer_part_chevron(ExpandableListContainer *self, uint32_t nodeIndex);
Panel *ExpandableListContainer_part_label(ExpandableListContainer *self, uint32_t nodeIndex);
Panel *ExpandableListContainer_part_checkbox(ExpandableListContainer *self, uint32_t nodeIndex);

#endif