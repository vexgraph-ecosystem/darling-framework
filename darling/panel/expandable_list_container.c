#include "darling/panel/expandable_list_container.h"

#include "annotation/definition.h"
#include "annotation/overview.h"
#include "darling/field/checkbox.h"
#include "darling/label/label.h"
#include "darling/panel/panel.h"
#include "nio/mem.h"
#include "oop/type.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: ExpandableListContainer
 * ============================================================================
 * Node-oriented tree list built on data-oriented flat storage: the tree
 * ("expandable items with containers inside") lives as a flat pre-order
 * array of behaviorless ExpandableNode slot records — the single source of
 * truth. Each node owns a borrowed header row Panel (chevron + text label,
 * plus a Checkbox part in checklist mode) and a borrowed collapsible
 * child-holder Panel positioned as the row's sibling; parent/child ranges
 * are plain index numbers, so expansion flips a bool and re-runs the
 * recursive layout over the array — it never chases pointers. The nodes
 * array is arena-grown by doubling (Memory_alloc, zero steady-state
 * allocation in layout); the container is detach-only and never frees
 * row/childPanel views — the arena owns panels. Per the Container-vs-Panel
 * Law it embeds Panel as its first member and inherits the layout/tree/
 * background state; indentation shifts the child container right by
 * indentSpacing per level, with depthLevelValue = depth * indentSpacing
 * riding on the slot for inspection. Part verbs (part_row, part_chevron,
 * part_label, part_checkbox, part_childPanel) expose the borrowed views
 * without piercing internals; getChildren is dest-last.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: ExpandableListContainer (embeds Panel)
 * LEVEL: L2 — Behavior (node-oriented tree list, data-oriented flat storage)
 * ============================================================================
 * A tree list whose nodes ("expandable items with containers inside") are
 * behaviorless ExpandableNode slot records in a flat pre-order array — the
 * single source of truth. Each node owns a borrowed header row Panel
 * (chevron + text label, plus a Checkbox part in checklist mode) and a
 * borrowed collapsible child-holder Panel positioned as the row's sibling.
 * Expansion flips a bool and re-runs the recursive layout; it never chases
 * pointers (Rule 36). Indentation shifts the child container right by
 * indentSpacing per level; depthLevelValue = depth * indentSpacing rides on
 * the slot for inspection. Detach-only: this container never frees views.
 *
 * STRUCT FIELDS (Mirroring darling/panel/expandable_list_container.h):
 * ----------------------------------------------------------------------------
 *   Panel base;            // Inherited layout/tree/bg; owns the root rows
 *   ExpandableNode *nodes; // Flat pre-order slot array (source of truth)
 *   uint32_t nodeCount;    // Used slots
 *   uint32_t nodeCap;      // Allocated slots (grown by doubling)
 *   float indentSpacing;   // Px per depth level (default 20)
 *   float rowHeight;       // Px per header row (default 22)
 *   bool checklistMode;    // True = rows carry a Checkbox part
 *   uint32_t chevronCollapsed; // '>' by default (see INTENTION: svg later)
 *   uint32_t chevronExpanded;  // '^' by default (see INTENTION: svg later)
 *   uint32_t chevronColor; // 0xAARRGGBB chevron text color
 *   uint32_t textColor;    // 0xAARRGGBB row label color
 *   uint32_t boxColor;     // 0xAARRGGBB checkbox box color
 *   uint32_t checkColor;   // 0xAARRGGBB checkbox check color
 *
 * SLOT RECORD (public, behaviorless, owned by ExpandableListContainer):
 * ----------------------------------------------------------------------------
 *   ExpandableNode (see header — zero ExpandableNode_* verbs):
 *   uint32_t parentIndex;  // EXPANDABLE_LIST_ROOT = root
 *   uint32_t childStart;   // First child index in the flat pre-order array
 *   uint32_t childCount;   // Contiguous children after childStart
 *   uint32_t depth;        // Nesting level (0 = root)
 *   float depthLevelValue; // Document px indent = depth * indentSpacing
 *   bool expanded;         // True = childPanel visible ("^"); false = ">"
 *   bool checked;          // Checklist state (source of truth in data)
 *   Panel *row;            // Borrowed header row view
 *   Panel *childPanel;     // Borrowed collapsible child-holder view
 *
 * PRIVATE HELPERS:
 * ----------------------------------------------------------------------------
 *   rowLabel(row)          — text Label inside a row (last child)
 *   rowCheckbox(row, mode) — Checkbox inside a row (child 1 when mode)
 *   glyphString(cp, out)   — UTF-8 encode one codepoint into a 4-byte buffer
 *   setChevron(node)       — swap the chevron glyph to the expanded/collapsed one
 *   findParentPanel(self, parentIndex) — where a new row/childPanel attaches
 *   compHierarchy(self)    — recompute depth + depthLevelValue (ascending)
 *   alignChildren(self)    — recompute childStart/childCount for every node
 *   subtreeSize(self, idx) — count of a node + all descendants (contiguous)
 *   layoutWalk(self, idx, y) — recursive layout; returns height consumed
 *   ensureCapacity(self)   — grow the flat nodes array (doubling)
 *   detachSubtree(self, idx, size) — remove childPanel panels from the tree
 *   removeRange(self, idx, size) — memmove down + fix parent/child indices
 *   applyChecklist(self)   — retro-add or detach Checkbox parts on all rows
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - ExpandableListContainer()                : ExpandableListContainer_0()
 *   - ExpandableListContainer(indentSpacing)   : ExpandableListContainer_1()
 *
 * Core Functions:
 *   - ExpandableListContainer_addNode(self, parentIndex, label) -> nodeIndex
 *   - ExpandableListContainer_removeNode(self, nodeIndex)
 *   - ExpandableListContainer_clear(self)
 *   - ExpandableListContainer_expand(self, nodeIndex)
 *   - ExpandableListContainer_collapse(self, nodeIndex)
 *   - ExpandableListContainer_toggle(self, nodeIndex)
 *   - ExpandableListContainer_layout(self)
 *   - ExpandableListContainer_nodeCount(self)
 *   - ExpandableListContainer_setChecked(self, nodeIndex, checked)
 *   - ExpandableListContainer_isChecked(self, nodeIndex)
 *   - ExpandableListContainer_setChecklistMode(self, mode)
 *   - ExpandableListContainer_isChecklistMode(self)
 *   - ExpandableListContainer_isExpanded(self, nodeIndex)
 *   - ExpandableListContainer_getParent(self, nodeIndex)
 *   - ExpandableListContainer_getDepth(self, nodeIndex)
 *   - ExpandableListContainer_getDepthLevelValue(self, nodeIndex)
 *   - ExpandableListContainer_getChildren(self, nodeIndex, outStart, outCount)
 *
 * Setters:
 *   - ExpandableListContainer_setIndentSpacing(self, spacing)
 *   - ExpandableListContainer_setRowHeight(self, height)
 *   - ExpandableListContainer_setChevronGlyphCollapsed(self, glyph)
 *   - ExpandableListContainer_setChevronGlyphExpanded(self, glyph)
 *   - ExpandableListContainer_setChevronColor(self, color)
 *   - ExpandableListContainer_setTextColor(self, color)
 *   - ExpandableListContainer_setBoxColor(self, color)
 *   - ExpandableListContainer_setCheckColor(self, color)
 *
 * Getters:
 *   - ExpandableListContainer_getIndentSpacing(self)
 *   - ExpandableListContainer_getRowHeight(self)
 *   - ExpandableListContainer_getChevronGlyphCollapsed(self)
 *   - ExpandableListContainer_getChevronGlyphExpanded(self)
 *   - ExpandableListContainer_getChevronColor(self)
 *   - ExpandableListContainer_getTextColor(self)
 *   - ExpandableListContainer_getBoxColor(self)
 *   - ExpandableListContainer_getCheckColor(self)
 *   - ExpandableListContainer_getNode(self, nodeIndex)
 *   - ExpandableListContainer_part_row(self, nodeIndex)
 *   - ExpandableListContainer_part_childPanel(self, nodeIndex)
 *   - ExpandableListContainer_part_chevron(self, nodeIndex)
 *   - ExpandableListContainer_part_label(self, nodeIndex)
 *   - ExpandableListContainer_part_checkbox(self, nodeIndex)
 * ============================================================================
 */

#define EXPANDABLE_LIST_INITIAL_CAP 16u

static bool ensureCapacity(ExpandableListContainer *self);

// CONSTRUCTORS
// ============================================================================

ExpandableListContainer *ExpandableListContainer_0(void) {
    ExpandableListContainer *self = (ExpandableListContainer*) Memory_alloc(TYPE_EXPANDABLE_LIST_CONTAINER_SINGLETON, sizeof(ExpandableListContainer));
    if (!self)
        return nullptr;
    Panel *b = Panel_0();
    if (!b) {
        Memory_free(self);
        return nullptr;
    }
    (*self).base = (*b);
    Memory_free(b);
    (*self).nodes = nullptr;
    (*self).nodeCount = 0;
    (*self).nodeCap = 0;
    (*self).indentSpacing = 20.0f;
    (*self).rowHeight = 22.0f;
    (*self).checklistMode = false;
    (*self).chevronCollapsed = 0x003Eu;
    (*self).chevronExpanded = 0x005Eu;
    (*self).chevronColor = 0x99FFFFFFu;
    (*self).textColor = 0xFFEEEEEEu;
    (*self).boxColor = 0xFF555555u;
    (*self).checkColor = 0xFF4CD964u;
    if (!ensureCapacity(self))
        return self; // empty tree is still a valid container
    return self;
}

ExpandableListContainer *ExpandableListContainer_1(float indentSpacing) {
    ExpandableListContainer *self = ExpandableListContainer_0();
    if (self)
        ExpandableListContainer_setIndentSpacing(self, indentSpacing);
    return self;
}

// PRIVATE HELPERS
// ============================================================================

static Label *rowLabel(Panel *row) {
    if (!row || Panel_childCount(row) == 0)
        return nullptr;
    return (Label*) (void*) Panel_getChild(row, Panel_childCount(row) - 1);
}

static Checkbox *rowCheckbox(const Panel *row, bool mode) { return (row && mode) ? (Checkbox*) (void*) Panel_getChild(row, 1) : nullptr; }

static void glyphString(uint32_t cp, char *out) {
    if (!out)
        return;
    if (cp < 0x80u) {
        (*out) = (char) cp;
        (*(out + 1)) = '\0';
        return;
    }
    if (cp < 0x800u) {
        (*(out + 0)) = (char) (0xC0u | (cp >> 6));
        (*(out + 1)) = (char) (0x80u | (cp & 0x3Fu));
        (*(out + 2)) = '\0';
        return;
    }
    if (cp < 0x10000u) {
        (*(out + 0)) = (char) (0xE0u | (cp >> 12));
        (*(out + 1)) = (char) (0x80u | ((cp >> 6) & 0x3Fu));
        (*(out + 2)) = (char) (0x80u | (cp & 0x3Fu));
        (*(out + 3)) = '\0';
        return;
    }
    (*out) = '?';
    (*(out + 1)) = '\0';
}

static void setChevron(ExpandableListContainer *self, ExpandableNode *node) {
    if (!self || !node)
        return;
    Label *chev = (Label*) (void*) Panel_getChild((*node).row, 0);
    if (!chev)
        return;
    char buf[4];
    glyphString((*node).expanded ? (*self).chevronExpanded : (*self).chevronCollapsed, buf);
    Label_setText(chev, buf);
    Label_setTextColor(chev, (*self).chevronColor);
}

static Panel *findParentPanel(ExpandableListContainer *self, uint32_t parentIndex) {
    if (parentIndex == EXPANDABLE_LIST_ROOT)
        return &(*self).base;
    if (parentIndex >= (*self).nodeCount)
        return nullptr;
    return (*self).nodes[parentIndex].childPanel;
}

static void compHierarchy(ExpandableListContainer *self) {
    if (!self)
        return;
    for (uint32_t i = 0; i < (*self).nodeCount; i++) {
        uint32_t p = (*self).nodes[i].parentIndex;
        if (p == EXPANDABLE_LIST_ROOT || p >= (*self).nodeCount || p >= i) {
            (*self).nodes[i].depth = 0;
        } else {
            (*self).nodes[i].depth = (*self).nodes[p].depth + 1;
        }
        (*self).nodes[i].depthLevelValue = (float) (*self).nodes[i].depth * (*self).indentSpacing;
    }
}

static void alignChildren(ExpandableListContainer *self) {
    if (!self)
        return;
    for (uint32_t i = 0; i < (*self).nodeCount; i++) {
        (*self).nodes[i].childStart = 0;
        (*self).nodes[i].childCount = 0;
    }
    for (uint32_t i = 0; i < (*self).nodeCount; i++) {
        uint32_t p = (*self).nodes[i].parentIndex;
        if (p == EXPANDABLE_LIST_ROOT || p >= (*self).nodeCount)
            continue;
        if ((*self).nodes[p].childCount == 0)
            (*self).nodes[p].childStart = i;
        (*self).nodes[p].childCount++;
    }
}

static uint32_t subtreeSize(const ExpandableListContainer *self, uint32_t idx) {
    if (!self || idx >= (*self).nodeCount)
        return 0;
    uint32_t total = 1;
    for (uint32_t i = idx + 1; i < (*self).nodeCount; i++) {
        uint32_t p = (*self).nodes[i].parentIndex;
        if (p == EXPANDABLE_LIST_ROOT || p < idx)
            break; // pre-order invariant: subtree closed at the next root or a parent above idx
        total++;
    }
    return total;
}

static float layoutWalk(ExpandableListContainer *self, uint32_t idx, float yInParent) {
    ExpandableNode *node = &(*self).nodes[idx];
    Panel *row = (*node).row;
    Component *rowC = &(*row).component;
    GraphicsComponent_setLocation(rowC, 0.0f, yInParent);
    GraphicsComponent_setSize(rowC, Component_getWidth(&(*self).base.component), (*self).rowHeight);
    Label *txt = rowLabel(row);
    if (txt) {
        float labelX = (*self).checklistMode ? 34.0f : 16.0f;
        GraphicsComponent_setLocation(&(*txt).base.component, labelX, 0.0f);
        GraphicsComponent_setSize(&(*txt).base.component, Component_getWidth(rowC) - labelX, (*self).rowHeight);
    }
    float h = (*self).rowHeight;
    if ((*node).expanded) {
        Panel *childPanel = (*node).childPanel;
        GraphicsComponent_setVisible(&(*childPanel).component, true);
        GraphicsComponent_setLocation(&(*childPanel).component, (*self).indentSpacing, yInParent + (*self).rowHeight);
        float yy = 0.0f;
        for (uint32_t i = 0; i < (*node).childCount; i++) {
            uint32_t c = (*node).childStart + i;
            yy += layoutWalk(self, c, yy);
        }
        GraphicsComponent_setSize(&(*childPanel).component, Component_getWidth(&(*self).base.component) - (*self).indentSpacing, yy);
        h += yy;
    } else {
        Panel *childPanel = (*node).childPanel;
        GraphicsComponent_setVisible(&(*childPanel).component, false);
    }
    return h;
}

static bool ensureCapacity(ExpandableListContainer *self) {
    if (!self)
        return false;
    if ((*self).nodeCount < (*self).nodeCap)
        return true;
    uint32_t next = (*self).nodeCap == 0 ? EXPANDABLE_LIST_INITIAL_CAP : (*self).nodeCap * 2;
    ExpandableNode *grown = (ExpandableNode*) Memory_realloc((*self).nodes, sizeof(ExpandableNode) * next);
    if (!grown)
        return false;
    (*self).nodes = grown;
    (*self).nodeCap = next;
    return true;
}

static void detachSubtree(ExpandableListContainer *self, uint32_t idx, uint32_t size) {
    for (uint32_t i = idx; i < idx + size; i++) {
        Panel *parentRow = (*self).nodes[i].row ? Panel_getParent((*self).nodes[i].row) : nullptr;
        if ((*self).nodes[i].row && parentRow)
            Panel_removeChild(parentRow, (*self).nodes[i].row);
        Panel *parentPanel = (*self).nodes[i].childPanel ? Panel_getParent((*self).nodes[i].childPanel) : nullptr;
        if ((*self).nodes[i].childPanel && parentPanel)
            Panel_removeChild(parentPanel, (*self).nodes[i].childPanel);
    }
}

static void removeRange(ExpandableListContainer *self, uint32_t idx, uint32_t size) {
    if (!self || size == 0)
        return;
    uint32_t rem = (*self).nodeCount - (idx + size);
    if (rem > 0)
        memmove((*self).nodes + idx, (*self).nodes + idx + size, sizeof(ExpandableNode) * rem);
    (*self).nodeCount -= size;
    for (uint32_t i = idx; i < (*self).nodeCount; i++) {
        uint32_t p = (*self).nodes[i].parentIndex;
        if (p != EXPANDABLE_LIST_ROOT && p >= idx + size)
            (*self).nodes[i].parentIndex = p - size;
        uint32_t s = (*self).nodes[i].childStart;
        if (s >= idx + size)
            (*self).nodes[i].childStart = s - size;
    }
    compHierarchy(self);
    alignChildren(self);
}

static Panel *buildRow(ExpandableListContainer *self, const char *label) {
    if (!self || !label)
        return nullptr;
    Panel *row = Panel_0();
    if (!row)
        return nullptr;
    Label *chev = Label_1(">");
    Label *txt = Label_1(label);
    if (chev)
        Panel_addContainer(row, &(*chev).base);
    Checkbox *box = nullptr;
    if ((*self).checklistMode) {
        box = Checkbox_0();
        if (box) {
            Panel_addContainer(row, &(*box).base);
            Checkbox_setBox(box, (*self).boxColor);
            Checkbox_setCheck(box, (*self).checkColor);
            Checkbox_setChecked(box, false);
        }
    }
    if (txt) {
        Panel_addContainer(row, &(*txt).base);
        Label_setFontSize(txt, 12.0f);
        Label_setTextColor(txt, (*self).textColor);
    }
    if (chev) {
        Label_setFontSize(chev, 12.0f);
        Label_setTextColor(chev, (*self).chevronColor);
        GraphicsComponent_setLocation(&(*chev).base.component, 0.0f, 0.0f);
        GraphicsComponent_setSize(&(*chev).base.component, 16.0f, (*self).rowHeight);
    }
    if (box) {
        GraphicsComponent_setLocation(&(*box).base.component, 16.0f, 0.0f);
        GraphicsComponent_setSize(&(*box).base.component, 18.0f, (*self).rowHeight);
    }
    if (txt) {
        float labelX = (*self).checklistMode ? 34.0f : 16.0f;
        GraphicsComponent_setLocation(&(*txt).base.component, labelX, 0.0f);
        GraphicsComponent_setSize(&(*txt).base.component, 120.0f, (*self).rowHeight);
    }
    return row;
}

// CORE FUNCTIONS
// ============================================================================

uint32_t ExpandableListContainer_addNode(ExpandableListContainer *self, uint32_t parentIndex, const char *label) {
    if (!self || !label)
        return EXPANDABLE_LIST_ROOT;
    if (parentIndex != EXPANDABLE_LIST_ROOT && parentIndex >= (*self).nodeCount)
        return EXPANDABLE_LIST_ROOT;
    Panel *row = buildRow(self, label);
    if (!row)
        return EXPANDABLE_LIST_ROOT;
    Panel *childPanel = Panel_0();
    if (!childPanel)
        return EXPANDABLE_LIST_ROOT;
    Panel *holder = findParentPanel(self, parentIndex);
    if (!holder)
        return EXPANDABLE_LIST_ROOT;
    if (!ensureCapacity(self))
        return EXPANDABLE_LIST_ROOT;
    // Pre-order insertion point: end of the parent's subtree so the flat
    // array stays pre-order-ordered (children always follow their parent).
    uint32_t nadd;
    if (parentIndex == EXPANDABLE_LIST_ROOT)
        nadd = (*self).nodeCount;
    else
        nadd = parentIndex + subtreeSize(self, parentIndex);
    uint32_t oldCount = (*self).nodeCount;
    for (uint32_t i = oldCount; i > nadd; i--)
        (*self).nodes[i] = (*self).nodes[i - 1];
    for (uint32_t i = nadd + 1; i <= oldCount; i++) {
        uint32_t p = (*self).nodes[i].parentIndex;
        if (p != EXPANDABLE_LIST_ROOT && p >= nadd)
            (*self).nodes[i].parentIndex = p + 1;
        uint32_t s = (*self).nodes[i].childStart;
        if (s >= nadd)
            (*self).nodes[i].childStart = s + 1;
    }
    (*self).nodes[nadd].parentIndex = parentIndex;
    (*self).nodes[nadd].childStart = 0;
    (*self).nodes[nadd].childCount = 0;
    (*self).nodes[nadd].depth = 0;
    (*self).nodes[nadd].depthLevelValue = 0.0f;
    (*self).nodes[nadd].expanded = false;
    (*self).nodes[nadd].checked = false;
    (*self).nodes[nadd].row = row;
    (*self).nodes[nadd].childPanel = childPanel;
    (*self).nodeCount++;
    GraphicsComponent_setVisible(&(*childPanel).component, false);
    Panel_addContainer(holder, row);
    Panel_addContainer(holder, childPanel);
    compHierarchy(self);
    alignChildren(self);
    ExpandableListContainer_layout(self);
    return nadd;
}

void ExpandableListContainer_clear(ExpandableListContainer *self) {
    if (!self)
        return;
    for (uint32_t i = 0; i < (*self).nodeCount; i++) {
        Panel *parentRow = (*self).nodes[i].row ? Panel_getParent((*self).nodes[i].row) : nullptr;
        if (parentRow)
            Panel_removeChild(parentRow, (*self).nodes[i].row);
        Panel *parentPanel = (*self).nodes[i].childPanel ? Panel_getParent((*self).nodes[i].childPanel) : nullptr;
        if (parentPanel)
            Panel_removeChild(parentPanel, (*self).nodes[i].childPanel);
    }
    (*self).nodeCount = 0;
    ExpandableListContainer_layout(self);
}

bool ExpandableListContainer_removeNode(ExpandableListContainer *self, uint32_t nodeIndex) {
    if (!self || nodeIndex >= (*self).nodeCount)
        return false;
    uint32_t size = subtreeSize(self, nodeIndex);
    detachSubtree(self, nodeIndex, size);
    removeRange(self, nodeIndex, size);
    ExpandableListContainer_layout(self);
    return true;
}

bool ExpandableListContainer_expand(ExpandableListContainer *self, uint32_t nodeIndex) {
    if (!self || nodeIndex >= (*self).nodeCount)
        return false;
    if ((*self).nodes[nodeIndex].expanded)
        return true;
    (*self).nodes[nodeIndex].expanded = true;
    setChevron(self, &(*self).nodes[nodeIndex]);
    ExpandableListContainer_layout(self);
    return true;
}

bool ExpandableListContainer_collapse(ExpandableListContainer *self, uint32_t nodeIndex) {
    if (!self || nodeIndex >= (*self).nodeCount)
        return false;
    if (!(*self).nodes[nodeIndex].expanded)
        return true;
    (*self).nodes[nodeIndex].expanded = false;
    setChevron(self, &(*self).nodes[nodeIndex]);
    ExpandableListContainer_layout(self);
    return true;
}

void ExpandableListContainer_toggle(ExpandableListContainer *self, uint32_t nodeIndex) {
    if (!self || nodeIndex >= (*self).nodeCount)
        return;
    if ((*self).nodes[nodeIndex].expanded)
        ExpandableListContainer_collapse(self, nodeIndex);
    else
        ExpandableListContainer_expand(self, nodeIndex);
}

void ExpandableListContainer_layout(ExpandableListContainer *self) {
    if (!self)
        return;
    Component *baseC = &(*self).base.component;
    compHierarchy(self);
    alignChildren(self);
    float yy = 0.0f;
    for (uint32_t i = 0; i < (*self).nodeCount; i++) {
        if ((*self).nodes[i].parentIndex == EXPANDABLE_LIST_ROOT)
            yy += layoutWalk(self, i, yy);
    }
    GraphicsComponent_setHeight(baseC, yy);
}

size_t ExpandableListContainer_nodeCount(const ExpandableListContainer *self) {
    return self ? (*self).nodeCount : 0;
}

// CHECKLIST
// ============================================================================

void ExpandableListContainer_setChecked(ExpandableListContainer *self, uint32_t nodeIndex, bool checked) {
    if (!self || nodeIndex >= (*self).nodeCount)
        return;
    (*self).nodes[nodeIndex].checked = checked;
    Checkbox *box = rowCheckbox((*self).nodes[nodeIndex].row, (*self).checklistMode);
    if (box)
        Checkbox_setChecked(box, checked);
}

bool ExpandableListContainer_isChecked(const ExpandableListContainer *self, uint32_t nodeIndex) {
    if (!self || nodeIndex >= (*self).nodeCount)
        return false;
    return (*self).nodes[nodeIndex].checked;
}

void ExpandableListContainer_setChecklistMode(ExpandableListContainer *self, bool mode) {
    if (!self)
        return;
    if ((*self).checklistMode == mode)
        return;
    (*self).checklistMode = mode;
    for (uint32_t i = 0; i < (*self).nodeCount; i++) {
        Panel *row = (*self).nodes[i].row;
        if (!row)
            continue;
        if (mode) {
            Checkbox *box = Checkbox_0();
            if (box) {
                Panel_addContainer(row, &(*box).base);
                Checkbox_setBox(box, (*self).boxColor);
                Checkbox_setCheck(box, (*self).checkColor);
                Checkbox_setChecked(box, (*self).nodes[i].checked);
                GraphicsComponent_setLocation(&(*box).base.component, 16.0f, 0.0f);
                GraphicsComponent_setSize(&(*box).base.component, 18.0f, (*self).rowHeight);
            }
        } else {
            // child 1 is the checkbox when checklistMode was true
            Panel *box = Panel_childCount(row) > 1 ? Panel_getChild(row, 1) : nullptr;
            if (box)
                Panel_removeChild(row, box);
        }
    }
    ExpandableListContainer_layout(self);
}

bool ExpandableListContainer_isChecklistMode(const ExpandableListContainer *self) {
    return self ? (*self).checklistMode : false;
}

// TREE INSPECTION
// ============================================================================

bool ExpandableListContainer_isExpanded(const ExpandableListContainer *self, uint32_t nodeIndex) {
    if (!self || nodeIndex >= (*self).nodeCount)
        return false;
    return (*self).nodes[nodeIndex].expanded;
}

uint32_t ExpandableListContainer_getParent(const ExpandableListContainer *self, uint32_t nodeIndex) {
    if (!self || nodeIndex >= (*self).nodeCount)
        return EXPANDABLE_LIST_ROOT;
    return (*self).nodes[nodeIndex].parentIndex;
}

uint32_t ExpandableListContainer_getDepth(const ExpandableListContainer *self, uint32_t nodeIndex) {
    if (!self || nodeIndex >= (*self).nodeCount)
        return 0;
    return (*self).nodes[nodeIndex].depth;
}

float ExpandableListContainer_getDepthLevelValue(const ExpandableListContainer *self, uint32_t nodeIndex) {
    if (!self || nodeIndex >= (*self).nodeCount)
        return 0.0f;
    return (*self).nodes[nodeIndex].depthLevelValue;
}

void ExpandableListContainer_getChildren(const ExpandableListContainer *self, uint32_t nodeIndex,
                                         uint32_t *outStart, uint32_t *outCount) {
    if (outStart)
        *outStart = 0;
    if (outCount)
        *outCount = 0;
    if (!self || nodeIndex >= (*self).nodeCount) {
        if (outStart)
            *outStart = 0;
        if (outCount)
            *outCount = 0;
        return;
    }
    if (outStart)
        *outStart = (*self).nodes[nodeIndex].childStart;
    if (outCount)
        *outCount = (*self).nodes[nodeIndex].childCount;
}

// SETTERS
// ============================================================================

void ExpandableListContainer_setIndentSpacing(ExpandableListContainer *self, float spacing) {
    if (!self)
        return;
    if (spacing < 0.0f)
        spacing = 0.0f;
    (*self).indentSpacing = spacing;
    ExpandableListContainer_layout(self);
}

void ExpandableListContainer_setRowHeight(ExpandableListContainer *self, float height) {
    if (!self)
        return;
    if (height < 1.0f)
        height = 1.0f;
    (*self).rowHeight = height;
    ExpandableListContainer_layout(self);
}

void ExpandableListContainer_setChevronGlyphCollapsed(ExpandableListContainer *self, uint32_t glyph) {
    if (!self)
        return;
    (*self).chevronCollapsed = glyph;
    for (uint32_t i = 0; i < (*self).nodeCount; i++)
        if (!(*self).nodes[i].expanded)
            setChevron(self, &(*self).nodes[i]);
}

void ExpandableListContainer_setChevronGlyphExpanded(ExpandableListContainer *self, uint32_t glyph) {
    if (!self)
        return;
    (*self).chevronExpanded = glyph;
    for (uint32_t i = 0; i < (*self).nodeCount; i++)
        if ((*self).nodes[i].expanded)
            setChevron(self, &(*self).nodes[i]);
}

void ExpandableListContainer_setChevronColor(ExpandableListContainer *self, uint32_t color) {
    if (!self)
        return;
    (*self).chevronColor = color;
    for (uint32_t i = 0; i < (*self).nodeCount; i++)
        setChevron(self, &(*self).nodes[i]);
}

void ExpandableListContainer_setTextColor(ExpandableListContainer *self, uint32_t color) {
    if (!self)
        return;
    (*self).textColor = color;
    for (uint32_t i = 0; i < (*self).nodeCount; i++) {
        Label *txt = rowLabel((*self).nodes[i].row);
        if (txt)
            Label_setTextColor(txt, color);
    }
}

void ExpandableListContainer_setBoxColor(ExpandableListContainer *self, uint32_t color) {
    if (!self)
        return;
    (*self).boxColor = color;
    for (uint32_t i = 0; i < (*self).nodeCount; i++) {
        Checkbox *box = rowCheckbox((*self).nodes[i].row, (*self).checklistMode);
        if (box)
            Checkbox_setBox(box, color);
    }
}

void ExpandableListContainer_setCheckColor(ExpandableListContainer *self, uint32_t color) {
    if (!self)
        return;
    (*self).checkColor = color;
    for (uint32_t i = 0; i < (*self).nodeCount; i++) {
        Checkbox *box = rowCheckbox((*self).nodes[i].row, (*self).checklistMode);
        if (box)
            Checkbox_setCheck(box, color);
    }
}

// GETTERS
// ============================================================================

float ExpandableListContainer_getIndentSpacing(const ExpandableListContainer *self) {
    return self ? (*self).indentSpacing : 0.0f;
}

float ExpandableListContainer_getRowHeight(const ExpandableListContainer *self) {
    return self ? (*self).rowHeight : 0.0f;
}

uint32_t ExpandableListContainer_getChevronGlyphCollapsed(const ExpandableListContainer *self) {
    return self ? (*self).chevronCollapsed : 0x003Eu;
}

uint32_t ExpandableListContainer_getChevronGlyphExpanded(const ExpandableListContainer *self) {
    return self ? (*self).chevronExpanded : 0x005Eu;
}

uint32_t ExpandableListContainer_getChevronColor(const ExpandableListContainer *self) {
    return self ? (*self).chevronColor : 0x99FFFFFFu;
}

uint32_t ExpandableListContainer_getTextColor(const ExpandableListContainer *self) {
    return self ? (*self).textColor : 0xFFEEEEEEu;
}

uint32_t ExpandableListContainer_getBoxColor(const ExpandableListContainer *self) {
    return self ? (*self).boxColor : 0xFF555555u;
}

uint32_t ExpandableListContainer_getCheckColor(const ExpandableListContainer *self) {
    return self ? (*self).checkColor : 0xFF4CD964u;
}

const ExpandableNode *ExpandableListContainer_getNode(const ExpandableListContainer *self, uint32_t nodeIndex) {
    if (!self || nodeIndex >= (*self).nodeCount)
        return nullptr;
    return &(*self).nodes[nodeIndex];
}

// PARTS
// ============================================================================

Panel *ExpandableListContainer_part_row(ExpandableListContainer *self, uint32_t nodeIndex) {
    if (!self || nodeIndex >= (*self).nodeCount)
        return nullptr;
    return (*self).nodes[nodeIndex].row;
}

Panel *ExpandableListContainer_part_childPanel(ExpandableListContainer *self, uint32_t nodeIndex) {
    if (!self || nodeIndex >= (*self).nodeCount)
        return nullptr;
    return (*self).nodes[nodeIndex].childPanel;
}

Panel *ExpandableListContainer_part_chevron(ExpandableListContainer *self, uint32_t nodeIndex) {
    if (!self || nodeIndex >= (*self).nodeCount)
        return nullptr;
    Panel *row = (*self).nodes[nodeIndex].row;
    return row ? Panel_getChild(row, 0) : nullptr;
}

Panel *ExpandableListContainer_part_label(ExpandableListContainer *self, uint32_t nodeIndex) {
    if (!self || nodeIndex >= (*self).nodeCount)
        return nullptr;
    Panel *row = (*self).nodes[nodeIndex].row;
    if (!row || Panel_childCount(row) == 0)
        return nullptr;
    return Panel_getChild(row, Panel_childCount(row) - 1);
}

Panel *ExpandableListContainer_part_checkbox(ExpandableListContainer *self, uint32_t nodeIndex) {
    if (!self || nodeIndex >= (*self).nodeCount)
        return nullptr;
    Panel *row = (*self).nodes[nodeIndex].row;
    if (!row || !(*self).checklistMode)
        return nullptr;
    return (Panel*) (void*) rowCheckbox(row, true);
}