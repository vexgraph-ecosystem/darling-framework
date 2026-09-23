#include "darling/panel/flex_container.h"

#include "annotation/definition.h"
#include "annotation/overview.h"
#include "darling/panel/panel.h"
#include "nio/mem.h"
#include "oop/type.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: FlexContainer
 * ============================================================================
 * Ordered split-panel layout — the flexbox. One {} level is one ordered
 * row/column with exactly ONE operator: '>' lays out left-to-right, 'v'
 * lays out top-to-bottom. Mixing operators at the same level is a parse
 * error; nesting expresses mixed layouts. Numbers are child ids set-wise:
 * the spec must reference exactly {1..N}, each leaf once, where N is the
 * current child count — POSITION in the string is layout order, never
 * identity. So 1>2>{4v5}>3 is four columns with a 4-over-5 column in slot
 * 3, and {1>{2v3}} is panel 1 left with 2-over-3 on the right.
 *
 * Storage is data-oriented: children stay ordinary Panels in the embedded
 * base's child list (Panel_addContainer, like ListContainer — no second
 * child list), while the split structure is a flat FlexNode pool plus
 * per-group flat kid/ratio arrays, all doubling on growth with zero
 * artificial ceilings. Parsing and validation run once on the cold path
 * (setSpec fails closed: bad specs keep the old layout); the layout pass
 * itself allocates nothing, logs nothing, and trusts the cold-validated
 * tree behind one nullptr guard. Grips are separators, not widgets: each
 * grip between slot i and i+1 owns one ratio, and dragging grip i
 * rebalances its two neighbors through FlexContainer_dragGrip (the event
 * layer calls it with pointer deltas; this class owns no gestures).
 * Per the Container-vs-Panel Law it embeds Panel as its first member.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: FlexContainer (embeds Panel)
 * LEVEL: L2 — Behavior (ordered split-panel layout behavior API)
 * ============================================================================
 * Ordered split-panel layout. One {} level = one row/column, one operator
 * ('>' row, 'v' column); numbers reference children set-wise {1..N}, each
 * once; position is layout order. Children live in the base child list;
 * structure lives in a flat node pool. Detach-only: children are never
 * freed. Bad specs fail closed. Grips rebalance neighbor ratios.
 *
 * STRUCT FIELDS (Mirroring darling/panel/flex_container.h):
 * ----------------------------------------------------------------------------
 *   Panel base;          // Inherited layout/tree/background state; children
 *                        // live in (*base).children via Panel_* tree API
 *   FlexNode *nodes;     // Flat split-structure pool; (*nodes)[root] is the
 *                        // root group (or single root leaf); nullptr = empty
 *   uint32_t nodeCount;  // Live nodes in the pool
 *   uint32_t nodeCap;    // Pool capacity, doubling growth
 *   uint32_t root;       // Root node index (valid when nodeCount > 0)
 *   float spacing;       // Gap between adjacent slots in parent units (>= 0)
 *
 * SLOT RECORD (owned by FlexContainer, zero behavior):
 * ----------------------------------------------------------------------------
 *   FlexNode isGroup;    // bool + false = leaf panel slot, true = split group
 *   FlexNode axis;       // int32_t + FLEX_ROW (>) / FLEX_COLUMN (v), groups only
 *   FlexNode panelId;    // uint32_t + 1-based child id, leaves only
 *   FlexNode kidIdx;     // uint32_t* + node indices of this group's kids
 *   FlexNode ratios;     // float* + relative shares parallel to kidIdx
 *   FlexNode kidCount;   // uint32_t + number of kids
 *   FlexNode kidCap;     // uint32_t + kid array capacity, doubling growth
 *
 * PRIVATE HELPERS (kept file-local pure-data only, each with full fields):
 * ----------------------------------------------------------------------------
 *   FlexCursor text;     // const char* + spec text being parsed
 *   FlexCursor length;   // size_t + spec length in bytes
 *   FlexCursor at;       // size_t + current parse offset
 *   FlexCursor failed;   // bool + latched parse error flag
 *   FlexEmit dest;       // char* + bounded output buffer
 *   FlexEmit cap;        // size_t + buffer capacity (room kept for NUL)
 *   FlexEmit at;         // size_t + current write offset
 *   FlexEmit truncated;  // bool + latched overflow flag
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Public Constructors: (.h)
 *   - FlexContainer()            : FlexContainer_0()
 *   - FlexContainer(spec)        : FlexContainer_1(spec)
 *
 * Private Constructors: (.c static)
 *   - (none)
 *
 * Public Core Functions: (.h)
 *   - FlexContainer_add(f, child)
 *   - FlexContainer_setSpec(f, spec)
 *   - FlexContainer_layout(f)
 *   - FlexContainer_count(f)
 *   - FlexContainer_groupCount(f)
 *   - FlexContainer_groupSize(f, group)
 *   - FlexContainer_groupAxis(f, group)
 *   - FlexContainer_setRatio(f, group, grip, ratio)
 *   - FlexContainer_getRatio(f, group, grip)
 *   - FlexContainer_dragGrip(f, group, grip, deltaPx, extentPx)
 *   - FlexContainer_getSpec(f, dest, cap, outTruncated)
 *   - FlexContainer_toString(f, dest, cap, outTruncated)
 *   - FlexContainer_toStringStruct(f, dest, cap, outTruncated)
 *
 * Private Core Functions: (.c static)
 *   - cursorSkip(c)                    : skip ASCII whitespace
 *   - cursorTake(c, want)              : consume one expected char
 *   - cursorNumber(c, outId)           : parse a 1-based decimal child id
 *   - poolGrowNodes(f)                 : double the node pool
 *   - poolFreeAll(f)                   : free node pool + all kid arrays
 *   - poolGroup(f, axis)               : append a group node, return index
 *   - poolLeaf(f, panelId)             : append a leaf node, return index
 *   - groupPush(f, group, kid)         : append a kid + unit ratio
 *   - parseItem(c, f)                  : number leaf or {group} nest
 *   - parseGroup(c, f)                 : item (op item)*, one op per level
 *   - validateIds(f, childN)           : leaves cover exactly {1..N}
 *   - flexChild(f, panelId)            : 1-based id to child Panel
 *   - layoutNode(f, node, x, y, w, h)  : recursive rect splitter (no alloc)
 *   - emitChar(e, ch)                  : bounded char writer
 *   - emitUint(e, v)                   : bounded decimal writer
 *   - emitNode(f, e, node, braced)     : canonical spec serializer
 *   - finishString(e, outTruncated)    : NUL-terminate, report truncation
 *
 * Public Setters: (.h)
 *   - FlexGraphicsComponent_setLocation(f, x, y)
 *   - FlexGraphicsComponent_setSize(f, w, h)
 *   - FlexContainer_setSpacing(f, spacing)
 *
 * Private Setters: (.c static)
 *   - (none)
 *
 * Public Getters: (.h)
 *   - FlexContainer_getSpacing(f)
 *
 * Private Getters: (.c static)
 *   - (none)
 * ============================================================================
 */

// PRIVATE HELPERS (pure-data parse/emit cursors)
// ============================================================================

typedef struct FlexCursor {
    const char *text;
    size_t length;
    size_t at;
    bool failed;
} FlexCursor;

typedef struct FlexEmit {
    char *dest;
    size_t cap;
    size_t at;
    bool truncated;
} FlexEmit;

// CONSTRUCTORS (PUBLIC & PRIVATE)
// ============================================================================

FlexContainer *FlexContainer_0(void) {
    FlexContainer *f = (FlexContainer*) Memory_alloc(TYPE_FLEX_CONTAINER_SINGLETON, sizeof(FlexContainer));
    if (!f)
        return nullptr;
    Panel *b = Panel_0();
    if (!b) {
        Memory_free(f);
        return nullptr;
    }
    (*f).base = (*b);
    Memory_free(b);
    (*f).nodes = nullptr;
    (*f).nodeCount = 0u;
    (*f).nodeCap = 0u;
    (*f).root = 0u;
    (*f).spacing = 0.0f;
    return f;
}

FlexContainer *FlexContainer_1(const char *spec) {
    FlexContainer *f = FlexContainer_0();
    if (f && spec)
        FlexContainer_setSpec(f, spec);
    return f;
}

// CORE FUNCTIONS (PUBLIC & PRIVATE)
// ============================================================================

static void cursorSkip(FlexCursor *c) {
    while ((*c).at < (*c).length) {
        char ch = (*c).text[(*c).at];
        if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r')
            return;
        (*c).at++;
    }
}

static bool cursorTake(FlexCursor *c, char want) {
    cursorSkip(c);
    if ((*c).at >= (*c).length)
        return false;
    if ((*c).text[(*c).at] != want)
        return false;
    (*c).at++;
    return true;
}

static bool cursorNumber(FlexCursor *c, uint32_t *outId) {
    cursorSkip(c);
    uint32_t id = 0u;
    bool any = false;
    while ((*c).at < (*c).length) {
        char ch = (*c).text[(*c).at];
        if (ch < '0' || ch > '9')
            break;
        if (id > 100000000u) {
            (*c).failed = true;
            return false;
        }
        id = id * 10u + (uint32_t) (ch - '0');
        (*c).at++;
        any = true;
    }
    if (!any)
        return false;
    *outId = id;
    return true;
}

static bool poolGrowNodes(FlexContainer *f) {
    uint32_t cap = (*f).nodeCap == 0u ? 4u : (*f).nodeCap * 2u;
    FlexNode *next = (FlexNode*) Memory_alloc(TYPE_FLEX_CONTAINER_SINGLETON, (size_t) cap * sizeof(FlexNode));
    if (!next)
        return false;
    FlexNode *old = (*f).nodes;
    for (uint32_t i = 0u; i < (*f).nodeCount; i++)
        next[i] = old[i];
    if (old)
        Memory_free(old);
    (*f).nodes = next;
    (*f).nodeCap = cap;
    return true;
}

static void poolFreeAll(FlexContainer *f) {
    FlexNode *pool = (*f).nodes;
    if (pool) {
        for (uint32_t i = 0u; i < (*f).nodeCount; i++) {
            FlexNode *n = &pool[i];
            if ((*n).isGroup) {
                if ((*n).kidIdx)
                    Memory_free((*n).kidIdx);
                if ((*n).ratios)
                    Memory_free((*n).ratios);
            }
        }
        Memory_free(pool);
    }
    (*f).nodes = nullptr;
    (*f).nodeCount = 0u;
    (*f).nodeCap = 0u;
    (*f).root = 0u;
}

static int32_t poolGroup(FlexContainer *f, int32_t axis) {
    if ((*f).nodeCount >= (*f).nodeCap && !poolGrowNodes(f))
        return -1;
    FlexNode *pool = (*f).nodes;
    FlexNode *n = &pool[(*f).nodeCount];
    (*n).isGroup = true;
    (*n).axis = axis;
    (*n).panelId = 0u;
    (*n).kidIdx = nullptr;
    (*n).ratios = nullptr;
    (*n).kidCount = 0u;
    (*n).kidCap = 0u;
    int32_t idx = (int32_t) (*f).nodeCount;
    (*f).nodeCount++;
    return idx;
}

static int32_t poolLeaf(FlexContainer *f, uint32_t panelId) {
    if ((*f).nodeCount >= (*f).nodeCap && !poolGrowNodes(f))
        return -1;
    FlexNode *pool = (*f).nodes;
    FlexNode *n = &pool[(*f).nodeCount];
    (*n).isGroup = false;
    (*n).axis = FLEX_ROW;
    (*n).panelId = panelId;
    (*n).kidIdx = nullptr;
    (*n).ratios = nullptr;
    (*n).kidCount = 0u;
    (*n).kidCap = 0u;
    int32_t idx = (int32_t) (*f).nodeCount;
    (*f).nodeCount++;
    return idx;
}

static bool groupPush(FlexContainer *f, uint32_t group, uint32_t kid) {
    FlexNode *pool = (*f).nodes;
    if (group >= (*f).nodeCount)
        return false;
    FlexNode *n = &pool[group];
    if (!(*n).isGroup)
        return false;
    if ((*n).kidCount >= (*n).kidCap) {
        uint32_t cap = (*n).kidCap == 0u ? 2u : (*n).kidCap * 2u;
        uint32_t *nextK = (uint32_t*) Memory_alloc(TYPE_FLEX_CONTAINER_SINGLETON, (size_t) cap * sizeof(uint32_t));
        if (!nextK)
            return false;
        float *nextR = (float*) Memory_alloc(TYPE_FLEX_CONTAINER_SINGLETON, (size_t) cap * sizeof(float));
        if (!nextR) {
            Memory_free(nextK);
            return false;
        }
        for (uint32_t i = 0u; i < (*n).kidCount; i++) {
            nextK[i] = (*n).kidIdx[i];
            nextR[i] = (*n).ratios[i];
        }
        if ((*n).kidIdx)
            Memory_free((*n).kidIdx);
        if ((*n).ratios)
            Memory_free((*n).ratios);
        (*n).kidIdx = nextK;
        (*n).ratios = nextR;
        (*n).kidCap = cap;
    }
    (*n).kidIdx[(*n).kidCount] = kid;
    (*n).ratios[(*n).kidCount] = 1.0f;
    (*n).kidCount++;
    return true;
}

static int32_t parseItem(FlexCursor *c, FlexContainer *f);

static int32_t parseGroup(FlexCursor *c, FlexContainer *f) {
    int32_t first = parseItem(c, f);
    if (first < 0)
        return -1;
    cursorSkip(c);
    if ((*c).at >= (*c).length)
        return first;
    char peek = (*c).text[(*c).at];
    if (peek != '>' && peek != 'v')
        return first;
    int32_t axis = peek == '>' ? FLEX_ROW : FLEX_COLUMN;
    (*c).at++;
    int32_t group = poolGroup(f, axis);
    if (group < 0) {
        (*c).failed = true;
        return -1;
    }
    if (!groupPush(f, (uint32_t) group, (uint32_t) first)) {
        (*c).failed = true;
        return -1;
    }
    for (;;) {
        int32_t item = parseItem(c, f);
        if (item < 0)
            return -1;
        if (!groupPush(f, (uint32_t) group, (uint32_t) item)) {
            (*c).failed = true;
            return -1;
        }
        cursorSkip(c);
        if ((*c).at >= (*c).length)
            break;
        char next = (*c).text[(*c).at];
        if (next != peek)
            break;
        (*c).at++;
    }
    return group;
}

static int32_t parseItem(FlexCursor *c, FlexContainer *f) {
    cursorSkip(c);
    if ((*c).at >= (*c).length) {
        (*c).failed = true;
        return -1;
    }
    char ch = (*c).text[(*c).at];
    if (ch == '{') {
        (*c).at++;
        int32_t inner = parseGroup(c, f);
        if (inner < 0)
            return -1;
        if (!cursorTake(c, '}')) {
            (*c).failed = true;
            return -1;
        }
        return inner;
    }
    uint32_t id = 0u;
    if (!cursorNumber(c, &id) || id < 1u) {
        (*c).failed = true;
        return -1;
    }
    return poolLeaf(f, id);
}

static bool validateIds(FlexContainer *f, uint32_t childN) {
    if ((*f).nodeCount == 0u)
        return childN == 0u;
    if (childN == 0u)
        return false;
    bool *seen = (bool*) Memory_alloc(TYPE_FLEX_CONTAINER_SINGLETON, (size_t) childN + 1u);
    if (!seen)
        return false;
    memset(seen, 0, (size_t) childN + 1u);
    bool ok = true;
    FlexNode *pool = (*f).nodes;
    for (uint32_t i = 0u; i < (*f).nodeCount; i++) {
        FlexNode *n = &pool[i];
        if ((*n).isGroup)
            continue;
        uint32_t id = (*n).panelId;
        if (id < 1u || id > childN || seen[id]) {
            ok = false;
            break;
        }
        seen[id] = true;
    }
    if (ok) {
        for (uint32_t id = 1u; id <= childN; id++) {
            if (!seen[id]) {
                ok = false;
                break;
            }
        }
    }
    Memory_free(seen);
    return ok;
}

static Panel *flexChild(FlexContainer *f, uint32_t panelId) {
    Panel *b = &(*f).base;
    size_t n = Panel_childCount(b);
    if (panelId < 1u || (size_t) panelId > n)
        return nullptr;
    return Panel_getChild(b, (size_t) panelId - 1u);
}

static void layoutNode(FlexContainer *f, uint32_t node, float x, float y, float w, float h) {
    FlexNode *pool = (*f).nodes;
    FlexNode *n = &pool[node];
    if (!(*n).isGroup) {
        Panel *kid = flexChild(f, (*n).panelId);
        if (!kid)
            return;
        Component *kb = &(*kid).component;
        GraphicsComponent_setLocation(kb, x, y);
        GraphicsComponent_setSize(kb, w, h);
        return;
    }
    uint32_t count = (*n).kidCount;
    if (count == 0u)
        return;
    uint32_t *kids = (*n).kidIdx;
    float *ratios = (*n).ratios;
    float total = 0.0f;
    for (uint32_t i = 0u; i < count; i++)
        total += ratios[i];
    if (total <= 0.0f)
        return;
    float spacing = (*f).spacing;
    bool row = (*n).axis == FLEX_ROW;
    float extent = row ? w : h;
    float avail = extent - spacing * (float) (count - 1u);
    if (avail < 0.0f)
        avail = 0.0f;
    float cursor = 0.0f;
    for (uint32_t i = 0u; i < count; i++) {
        float share = avail * ratios[i] / total;
        if (row)
            layoutNode(f, kids[i], x + cursor, y, share, h);
        else
            layoutNode(f, kids[i], x, y + cursor, w, share);
        cursor += share + spacing;
    }
}

static void emitChar(FlexEmit *e, char ch) {
    if ((*e).at + 1u >= (*e).cap) {
        (*e).truncated = true;
        return;
    }
    (*e).dest[(*e).at] = ch;
    (*e).at++;
}

static void emitUint(FlexEmit *e, uint32_t v) {
    char buf[11];
    int len = 0;
    if (v == 0u)
        buf[len++] = '0';
    else {
        char rev[11];
        int rlen = 0;
        while (v > 0u) {
            rev[rlen++] = (char) ('0' + (v % 10u));
            v /= 10u;
        }
        while (rlen > 0) {
            rlen--;
            buf[len++] = rev[rlen];
        }
    }
    for (int i = 0; i < len; i++)
        emitChar(e, buf[i]);
}

static void emitNode(const FlexContainer *f, FlexEmit *e, uint32_t node, bool braced) {
    FlexNode *pool = (*f).nodes;
    FlexNode *n = &pool[node];
    if (!(*n).isGroup) {
        emitUint(e, (*n).panelId);
        return;
    }
    if (braced)
        emitChar(e, '{');
    char op = (*n).axis == FLEX_ROW ? '>' : 'v';
    for (uint32_t i = 0u; i < (*n).kidCount; i++) {
        emitNode(f, e, (*n).kidIdx[i], true);
        if (i + 1u < (*n).kidCount)
            emitChar(e, op);
    }
    if (braced)
        emitChar(e, '}');
}

static bool finishString(FlexEmit *e, bool *outTruncated) {
    if ((*e).cap == 0u) {
        if (outTruncated)
            (*outTruncated) = true;
        return false;
    }
    if ((*e).at >= (*e).cap) {
        (*e).dest[(*e).cap - 1u] = '\0';
        if (outTruncated)
            (*outTruncated) = true;
        return false;
    }
    (*e).dest[(*e).at] = '\0';
    if (outTruncated)
        (*outTruncated) = (*e).truncated;
    return !(*e).truncated;
}

void FlexContainer_add(FlexContainer *f, Panel *child) {
    if (!f || !child)
        return;
    Panel *b = &(*f).base;
    if (child == b)
        return;
    Panel_addContainer(b, child);
    FlexContainer_layout(f);
}

bool FlexContainer_setSpec(FlexContainer *f, const char *spec) {
    if (!f || !spec)
        return false;
    FlexContainer tmp;
    memset(&tmp, 0, sizeof(FlexContainer));
    FlexCursor c;
    c.text = spec;
    c.length = strlen(spec);
    c.at = 0u;
    c.failed = false;
    cursorSkip(&c);
    int32_t root = -1;
    if (c.at >= c.length) {
        root = -2;
    } else if (c.text[c.at] == '{') {
        c.at++;
        root = parseGroup(&c, &tmp);
        if (root >= 0 && !c.failed) {
            if (!cursorTake(&c, '}'))
                c.failed = true;
            else {
                cursorSkip(&c);
                if (c.at < c.length)
                    c.failed = true;
            }
        }
    } else {
        root = parseGroup(&c, &tmp);
        if (root >= 0 && !c.failed) {
            cursorSkip(&c);
            if (c.at < c.length)
                c.failed = true;
        }
    }
    // NOTE: FlexCursor/FlexContainer field reads below ride on locals, so
    // each stays within the two-layer cap.
    bool failed = c.failed;
    Panel *b = &(*f).base;
    size_t n = Panel_childCount(b);
    bool ok = !failed && root != -1;
    if (ok && root == -2)
        ok = n == 0u;
    if (ok && root >= 0)
        ok = validateIds(&tmp, (uint32_t) n);
    if (!ok) {
        poolFreeAll(&tmp);
        return false;
    }
    if (ok && root > 0) {
        FlexNode *pool = tmp.nodes;
        for (uint32_t i = 0u; i < tmp.nodeCount; i++) {
            FlexNode *n = &pool[i];
            if (!(*n).isGroup)
                continue;
            for (uint32_t k = 0u; k < (*n).kidCount; k++) {
                if ((*n).kidIdx[k] == 0u)
                    (*n).kidIdx[k] = (uint32_t) root;
                else if ((*n).kidIdx[k] == (uint32_t) root)
                    (*n).kidIdx[k] = 0u;
            }
        }
        FlexNode swap = pool[0u];
        pool[0u] = pool[(uint32_t) root];
        pool[(uint32_t) root] = swap;
        root = 0;
    }
    poolFreeAll(f);
    (*f).nodes = tmp.nodes;
    (*f).nodeCount = tmp.nodeCount;
    (*f).nodeCap = tmp.nodeCap;
    (*f).root = root >= 0 ? (uint32_t) root : 0u;
    FlexContainer_layout(f);
    return true;
}

void FlexContainer_layout(FlexContainer *f) {
    if (!f)
        return;
    if ((*f).nodeCount == 0u)
        return;
    if ((*f).root >= (*f).nodeCount)
        return;
    Panel *b = &(*f).base;
    Component *c = &(*b).component;
    float w = Component_getWidth(c);
    float h = Component_getHeight(c);
    layoutNode(f, (*f).root, 0.0f, 0.0f, w, h);
}

size_t FlexContainer_count(const FlexContainer *f) {
    if (!f)
        return 0u;
    const Panel *b = &(*f).base;
    return Panel_childCount(b);
}

uint32_t FlexContainer_groupCount(const FlexContainer *f) {
    if (!f)
        return 0u;
    uint32_t groups = 0u;
    FlexNode *pool = (*f).nodes;
    for (uint32_t i = 0u; i < (*f).nodeCount; i++) {
        FlexNode *n = &pool[i];
        if ((*n).isGroup)
            groups++;
    }
    return groups;
}

uint32_t FlexContainer_groupSize(const FlexContainer *f, uint32_t group) {
    if (!f || group >= (*f).nodeCount)
        return 0u;
    FlexNode *pool = (*f).nodes;
    FlexNode *n = &pool[group];
    if (!(*n).isGroup)
        return 0u;
    return (*n).kidCount;
}

int32_t FlexContainer_groupAxis(const FlexContainer *f, uint32_t group) {
    if (!f || group >= (*f).nodeCount)
        return FLEX_ROW;
    FlexNode *pool = (*f).nodes;
    FlexNode *n = &pool[group];
    if (!(*n).isGroup)
        return FLEX_ROW;
    return (*n).axis;
}

bool FlexContainer_setRatio(FlexContainer *f, uint32_t group, uint32_t grip, float ratio) {
    if (!f || group >= (*f).nodeCount)
        return false;
    FlexNode *pool = (*f).nodes;
    FlexNode *n = &pool[group];
    if (!(*n).isGroup)
        return false;
    if (grip + 1u >= (*n).kidCount)
        return false;
    if (ratio < FLEX_RATIO_MIN)
        ratio = FLEX_RATIO_MIN;
    (*n).ratios[grip] = ratio;
    FlexContainer_layout(f);
    return true;
}

float FlexContainer_getRatio(const FlexContainer *f, uint32_t group, uint32_t grip) {
    if (!f || group >= (*f).nodeCount)
        return 0.0f;
    FlexNode *pool = (*f).nodes;
    FlexNode *n = &pool[group];
    if (!(*n).isGroup)
        return 0.0f;
    if (grip + 1u >= (*n).kidCount)
        return 0.0f;
    return (*n).ratios[grip];
}

bool FlexContainer_dragGrip(FlexContainer *f, uint32_t group, uint32_t grip, float deltaPx, float extentPx) {
    if (!f || group >= (*f).nodeCount)
        return false;
    if (extentPx == 0.0f)
        return false;
    FlexNode *pool = (*f).nodes;
    FlexNode *n = &pool[group];
    if (!(*n).isGroup)
        return false;
    uint32_t count = (*n).kidCount;
    if (count < 2u || grip + 1u >= count)
        return false;
    float total = 0.0f;
    for (uint32_t i = 0u; i < count; i++)
        total += (*n).ratios[i];
    float cap = total - FLEX_RATIO_MIN * (float) (count - 1u);
    if (cap < FLEX_RATIO_MIN)
        cap = FLEX_RATIO_MIN;
    float next = (*n).ratios[grip] + deltaPx / extentPx * total;
    if (next < FLEX_RATIO_MIN)
        next = FLEX_RATIO_MIN;
    if (next > cap)
        next = cap;
    (*n).ratios[grip] = next;
    FlexContainer_layout(f);
    return true;
}

bool FlexContainer_getSpec(const FlexContainer *f, char *dest, size_t cap, bool *outTruncated) {
    if (!f || !dest || cap == 0u) {
        if (outTruncated)
            (*outTruncated) = true;
        return false;
    }
    FlexEmit e;
    e.dest = dest;
    e.cap = cap;
    e.at = 0u;
    e.truncated = false;
    if ((*f).nodeCount > 0u)
        emitNode(f, &e, (*f).root, false);
    return finishString(&e, outTruncated);
}

bool FlexContainer_toString(const FlexContainer *f, char *dest, size_t cap, bool *outTruncated) {
    if (!f) {
        if (!dest || cap == 0u) {
            if (outTruncated)
                (*outTruncated) = true;
            return false;
        }
        snprintf(dest, cap, "nullptr");
        if (outTruncated)
            (*outTruncated) = false;
        return true;
    }
    if (!dest || cap == 0u) {
        if (outTruncated)
            (*outTruncated) = true;
        return false;
    }
    const Panel *b = &(*f).base;
    int need = snprintf(dest, cap, "FlexContainer(children=%zu, groups=%u, spacing=%.2f)",
        Panel_childCount(b), FlexContainer_groupCount(f), (double) (*f).spacing);
    if (need < 0) {
        if (outTruncated)
            (*outTruncated) = true;
        return false;
    }
    bool trunc = (size_t) need >= cap;
    if (outTruncated)
        (*outTruncated) = trunc;
    return !trunc;
}

bool FlexContainer_toStringStruct(const FlexContainer *f, char *dest, size_t cap, bool *outTruncated) {
    if (!f) {
        if (!dest || cap == 0u) {
            if (outTruncated)
                (*outTruncated) = true;
            return false;
        }
        snprintf(dest, cap, "nullptr");
        if (outTruncated)
            (*outTruncated) = false;
        return true;
    }
    if (!dest || cap == 0u) {
        if (outTruncated)
            (*outTruncated) = true;
        return false;
    }
    const Panel *b = &(*f).base;
    int need = snprintf(dest, cap, "FlexContainer{children=%zu, nodes=%u/%u, root=%u, spacing=%.2f}",
        Panel_childCount(b), (*f).nodeCount, (*f).nodeCap, (*f).root, (double) (*f).spacing);
    if (need < 0) {
        if (outTruncated)
            (*outTruncated) = true;
        return false;
    }
    bool trunc = (size_t) need >= cap;
    if (outTruncated)
        (*outTruncated) = trunc;
    return !trunc;
}

// SETTERS (PUBLIC & PRIVATE)
// ============================================================================

void FlexContainer_setSpacing(FlexContainer *f, float spacing) {
    if (!f)
        return;
    if (spacing < 0.0f)
        spacing = 0.0f;
    (*f).spacing = spacing;
    FlexContainer_layout(f);
}

// GETTERS (PUBLIC & PRIVATE)
// ============================================================================

float FlexContainer_getSpacing(const FlexContainer *f) {
    return f ? (*f).spacing : 0.0f;
}
