#ifndef DARLING_FLEX_CONTAINER_H
#define DARLING_FLEX_CONTAINER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "c23/constructor.h"
#include "darling/panel/panel.h"
#include "oop/type.h"

// darling/panel/flex_container.h — ordered split-panel layout (the flexbox).
//
// One {} level = one ordered row/column with exactly ONE operator: '>'
// splits left-to-right (FLEX_ROW), 'v' splits top-to-bottom (FLEX_COLUMN).
// Mixing operators at the same level is a parse error — nest instead.
// Numbers are child ids set-wise: the spec must reference exactly {1..N}
// (each leaf once, N = current child count); POSITION in the string is
// layout order, not identity. So 1>2>{4v5}>3 is four columns where slot 3
// holds a 4-over-5 column. Children stay ordinary Panels in the embedded
// base's child list (Panel_addContainer) — no second child list.
// Detach-only: the flex never frees children. Every grip between slot i
// and i+1 owns one ratio; dragging grip i rebalances its two neighbors.
// A bad spec fails closed (returns false, keeps the old layout).

#define FLEX_ROW     0
#define FLEX_COLUMN  1

#define FLEX_RATIO_MIN 0.05f

// SLOT RECORD (owned by FlexContainer, zero behavior — all verbs hang off
// FlexContainer_* below).
typedef struct FlexNode {
    bool isGroup;        // false = leaf panel slot, true = split group
    int32_t axis;        // FLEX_ROW / FLEX_COLUMN (groups only)
    uint32_t panelId;    // 1-based child id (leaves only)
    uint32_t *kidIdx;    // node indices of this group's kids (groups only)
    float *ratios;       // parallel to kidIdx; relative shares (groups only)
    uint32_t kidCount;   // number of kids (groups only)
    uint32_t kidCap;     // kid array capacity, doubling growth (groups only)
} FlexNode;

typedef struct FlexContainer {
    Panel base;
    FlexNode *nodes;
    uint32_t nodeCount;
    uint32_t nodeCap;
    uint32_t root;
    float spacing;
} FlexContainer;

// Constructors:
//   FlexContainer()        — empty flex, no spec, zero spacing
//   FlexContainer(spec)    — empty flex + setSpec attempt (fails gracefully
//                            with zero children; add children then setSpec)
FlexContainer *FlexContainer_0(void);
FlexContainer *FlexContainer_1(const char *spec);

#define FlexContainer(...) CONSTRUCTOR_DISPATCH(FlexContainer, __VA_ARGS__)

// Layout facade (same pattern as Panel_* shims: forward over the prefix).
static inline void FlexGraphicsComponent_setLocation(FlexContainer *f, float x, float y)
    { if (f) Panel_setLocation(&(*f).base, x, y); }
static inline void FlexGraphicsComponent_setSize(FlexContainer *f, float w, float h)
    { if (f) Panel_setSize(&(*f).base, w, h); }

// Core (detach-only children; spec fails closed; every mutation re-runs
// the layout pass; layout allocates nothing; group 0 is always the root
// group, nested groups follow in parse order).
void FlexContainer_add(FlexContainer *f, Panel *child);
bool FlexContainer_setSpec(FlexContainer *f, const char *spec);
void FlexContainer_layout(FlexContainer *f);
size_t FlexContainer_count(const FlexContainer *f);
uint32_t FlexContainer_groupCount(const FlexContainer *f);
uint32_t FlexContainer_groupSize(const FlexContainer *f, uint32_t group);
int32_t FlexContainer_groupAxis(const FlexContainer *f, uint32_t group);
bool FlexContainer_setRatio(FlexContainer *f, uint32_t group, uint32_t grip, float ratio);
float FlexContainer_getRatio(const FlexContainer *f, uint32_t group, uint32_t grip);
bool FlexContainer_dragGrip(FlexContainer *f, uint32_t group, uint32_t grip, float deltaPx, float extentPx);
bool FlexContainer_getSpec(const FlexContainer *f, char *dest, size_t cap, bool *outTruncated);
bool FlexContainer_toString(const FlexContainer *f, char *dest, size_t cap, bool *outTruncated);
bool FlexContainer_toStringStruct(const FlexContainer *f, char *dest, size_t cap, bool *outTruncated);

// Setters.
void FlexContainer_setSpacing(FlexContainer *f, float spacing);

// Getters.
float FlexContainer_getSpacing(const FlexContainer *f);

#endif
