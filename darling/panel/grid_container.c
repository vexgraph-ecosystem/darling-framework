#include "darling/panel/grid_container.h"

#include "annotation/definition.h"
#include "annotation/overview.h"
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
 * DEFINITION: GridContainer
 * ============================================================================
 * Excel-core grid layout: fixed or auto rows x cols of Panel* cell slots
 * with uniform gaps, optional frozen header counts, and per-row height
 * overrides (-1 = auto). Cells live ONLY in the row-major slot array —
 * never in the embedded base's child list — so an empty cell is nullptr
 * and skipped in layout; the grid auto-grows on setCell past the edge via
 * an exact-size row-major remap (Memory_alloc, zero steady-state
 * allocation in the layout pass). Detach-only: overwriting or clearing a
 * slot drops the old pointer without freeing it. The layout pass sizes
 * each column to its widest child and each row to its tallest child unless
 * a row override is set, then stacks cells top-left into their cell rects
 * (no stretch in v1) and wraps the grid's own size around the total.
 * Selection/editing lives above (a GridView controller later), not here.
 * Per the Container-vs-Panel Law it embeds Panel as its first member;
 * frozen header rows/cols are honored by a wrapping ScrollContainer.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: GridContainer (embeds Panel)
 * LEVEL: L2 — Behavior (excel-core grid layout behavior API)
 * ============================================================================
 * Fixed or auto rows x cols of Panel* cell slots with uniform gaps,
 * optional frozen header counts, and per-row height overrides (-1 = auto).
 * Cells live ONLY in the row-major slot array (never in the base child
 * list); empty cell = nullptr, skipped in layout. The layout pass sizes
 * each column to its widest child and each row to its tallest child
 * unless a row override is set, then stacks cells top-left into their
 * cell rects (no stretch in v1) and wraps the grid's own size around
 * the total. Selection/editing lives above, not here.
 *
 * STRUCT FIELDS (Mirroring darling/panel/grid_container.h):
 * ----------------------------------------------------------------------------
 *   Panel base;          // Inherited layout/tree/background state (cells are
 *                        // NOT base children; they live in cells[] below)
 *   Panel **cells;       // Row-major rows*cols slots; nullptr = empty cell;
 *                        // nullptr whole-array = 0x0 grid
 *   float *rowHeights;   // Per-row height override; -1.0f = auto; nullptr
 *                        // whole-array = 0x0 grid
 *   int32_t rows;        // Logical row extent (>= 0; grows on setCell)
 *   int32_t cols;        // Logical column extent (>= 0; grows on setCell)
 *   float gapX;          // Uniform horizontal gap between columns (>= 0)
 *   float gapY;          // Uniform vertical gap between rows (>= 0)
 *   int32_t headerRows;  // Frozen header row count (ScrollContainer honors; >= 0)
 *   int32_t headerCols;  // Frozen header column count (ScrollContainer honors)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - GridContainer_0(void)
 *   - GridContainer_2(rows, cols)
 *
 * Core Functions:
 *   - GridContainer_setCell(g, row, col, cell)
 *   - GridContainer_getCell(g, row, col)
 *   - GridContainer_layout(g)
 *
 * Setters:
 *   - GridComponent_setLocation(g, x, y)
 *   - GridComponent_setSize(g, w, h)
 *   - GridContainer_setGap(g, gx, gy)
 *   - GridContainer_setHeaderRows(g, count)
 *   - GridContainer_setHeaderCols(g, count)
 *   - GridContainer_setRowHeight(g, row, h)
 *
 * Getters:
 *   - GridContainer_getGap(g, gx, gy)
 *   - GridContainer_getHeaderRows(g)
 *   - GridContainer_getHeaderCols(g)
 *   - GridContainer_getRowHeight(g, row)
 *   - GridContainer_rowCount(g)
 *   - GridContainer_colCount(g)
 * ============================================================================
 */

// CONSTRUCTORS
// ============================================================================

// Grow the slot arrays to cover (row, col); exact-size policy, row-major
// remap. Updates the logical extent on success. True unless OOM.
static bool ensureCapacity(GridContainer *g, int32_t row, int32_t col) {
    int32_t rows = (*g).rows;
    int32_t cols = (*g).cols;
    int32_t wantR = row + 1;
    int32_t wantC = col + 1;
    int32_t needR = wantR > rows ? wantR : rows;
    int32_t needC = wantC > cols ? wantC : cols;
    if (needR == rows && needC == cols)
        return (*g).cells != nullptr;
    size_t count = (size_t) needR * (size_t) needC;
    Panel **next = (Panel**) Memory_alloc(TYPE_GRID_PANEL_SINGLETON, count * sizeof(Panel *));
    if (!next)
        return false;
    memset(next, 0, count * sizeof(Panel *));
    Panel **old = (*g).cells;
    if (old && rows > 0 && cols > 0) {
        for (int32_t r = 0; r < rows; r++) {
            for (int32_t q = 0; q < cols; q++)
                next[(size_t) r * (size_t) needC + (size_t) q]
                    = old[(size_t) r * (size_t) cols + (size_t) q];
        }
        Memory_free(old);
    }
    float *nextH = (float*) Memory_alloc(TYPE_GRID_PANEL_SINGLETON, (size_t) needR * sizeof(float));
    if (!nextH) {
        Memory_free(next);
        return false;
    }
    for (int32_t r = 0; r < needR; r++)
        nextH[r] = -1.0f;
    float *oldH = (*g).rowHeights;
    if (oldH && rows > 0) {
        for (int32_t r = 0; r < rows; r++)
            nextH[r] = oldH[r];
        Memory_free(oldH);
    }
    (*g).cells = next;
    (*g).rowHeights = nextH;
    (*g).rows = needR;
    (*g).cols = needC;
    return true;
}

GridContainer *GridContainer_0(void) {
    GridContainer *g = (GridContainer*) Memory_alloc(TYPE_GRID_PANEL_SINGLETON, sizeof(GridContainer));
    if (!g)
        return nullptr;
    Panel *b = Panel_0();
    if (!b) {
        Memory_free(g);
        return nullptr;
    }
    (*g).base = (*b);
    Memory_free(b);
    (*g).cells = nullptr;
    (*g).rowHeights = nullptr;
    (*g).rows = 0;
    (*g).cols = 0;
    (*g).gapX = 0.0f;
    (*g).gapY = 0.0f;
    (*g).headerRows = 0;
    (*g).headerCols = 0;
    return g;
}

GridContainer *GridContainer_2(int32_t rows, int32_t cols) {
    GridContainer *g = GridContainer_0();
    if (!g)
        return nullptr;
    if (rows < 0)
        rows = 0;
    if (cols < 0)
        cols = 0;
    if (rows == 0 || cols == 0)
        return g;
    if (!ensureCapacity(g, rows - 1, cols - 1)) {
        Memory_free(g);
        return nullptr;
    }
    return g;
}

// CORE FUNCTIONS
// ============================================================================

static void markDirty(GridContainer *g) {
    (void) g;
    (void) 0;
}

static float colWidth(const GridContainer *g, int32_t col) {
    float w = 0.0f;
    int32_t rows = (*g).rows;
    int32_t cols = (*g).cols;
    Panel **cells = (*g).cells;
    for (int32_t r = 0; r < rows; r++) {
        Panel *cell = cells[(size_t) r * (size_t) cols + (size_t) col];
        if (!cell)
            continue;
        const Panel *cp = cell;
        const Component *cb = &(*cp).component;
        float cw = Component_getWidth(cb);
        if (cw > w)
            w = cw;
    }
    return w;
}

static float rowHeightOf(const GridContainer *g, int32_t row) {
    float *heights = (*g).rowHeights;
    float fixed = heights[row];
    if (fixed >= 0.0f)
        return fixed;
    float h = 0.0f;
    int32_t cols = (*g).cols;
    Panel **cells = (*g).cells;
    for (int32_t q = 0; q < cols; q++) {
        Panel *cell = cells[(size_t) row * (size_t) cols + (size_t) q];
        if (!cell)
            continue;
        const Panel *cp = cell;
        const Component *cb = &(*cp).component;
        float ch = Component_getHeight(cb);
        if (ch > h)
            h = ch;
    }
    return h;
}

void GridContainer_layout(GridContainer *g) {
    if (!g)
        return;
    Panel *b = &(*g).base;
    Component *c = &(*b).component;
    int32_t rows = (*g).rows;
    int32_t cols = (*g).cols;
    Panel **cells = (*g).cells;
    if (rows <= 0 || cols <= 0 || !cells)
        return;
    float gx = (*g).gapX;
    float gy = (*g).gapY;
    float y = 0.0f;
    float totalW = 0.0f;
    for (int32_t r = 0; r < rows; r++) {
        float rh = rowHeightOf(g, r);
        float x = 0.0f;
        for (int32_t q = 0; q < cols; q++) {
            float cw = colWidth(g, q);
            Panel *cell = cells[(size_t) r * (size_t) cols + (size_t) q];
            if (cell) {
                Component *cb = &(*cell).component;
                Component_setLocation(cb, x, y);
            }
            x += cw + gx;
        }
        totalW = x - gx;
        y += rh + gy;
    }
    Component_setWidth(c, totalW);
    Component_setHeight(c, y - gy);
}

void GridContainer_setCell(GridContainer *g, int32_t row, int32_t col, Panel *cell) {
    if (!g)
        return;
    if (row < 0 || col < 0)
        return;
    if (!ensureCapacity(g, row, col))
        return;
    Panel **cells = (*g).cells;
    int32_t cols = (*g).cols;
    if (!cells)
        return;
    cells[(size_t) row * (size_t) cols + (size_t) col] = cell;
    GridContainer_layout(g);
}

Panel *GridContainer_getCell(const GridContainer *g, int32_t row, int32_t col) {
    if (!g)
        return nullptr;
    if (row < 0 || col < 0)
        return nullptr;
    int32_t rows = (*g).rows;
    int32_t cols = (*g).cols;
    if (row >= rows || col >= cols)
        return nullptr;
    Panel **cells = (*g).cells;
    if (!cells)
        return nullptr;
    return cells[(size_t) row * (size_t) cols + (size_t) col];
}

// SETTERS
// ============================================================================

void GridContainer_setGap(GridContainer *g, float gx, float gy) {
    if (!g)
        return;
    if (gx < 0.0f)
        gx = 0.0f;
    if (gy < 0.0f)
        gy = 0.0f;
    (*g).gapX = gx;
    (*g).gapY = gy;
    GridContainer_layout(g);
    markDirty(g);
}

void GridContainer_setHeaderRows(GridContainer *g, int32_t count) {
    if (!g)
        return;
    if (count < 0)
        count = 0;
    (*g).headerRows = count;
    markDirty(g);
}

void GridContainer_setHeaderCols(GridContainer *g, int32_t count) {
    if (!g)
        return;
    if (count < 0)
        count = 0;
    (*g).headerCols = count;
    markDirty(g);
}

void GridContainer_setRowHeight(GridContainer *g, int32_t row, float h) {
    if (!g)
        return;
    if (row < 0 || row >= (*g).rows)
        return;
    float *heights = (*g).rowHeights;
    if (!heights)
        return;
    heights[row] = h < 0.0f ? -1.0f : h;
    GridContainer_layout(g);
    markDirty(g);
}

// GETTERS
// ============================================================================

void GridContainer_getGap(const GridContainer *g, float *gx, float *gy) {
    float ox = g ? (*g).gapX : 0.0f;
    float oy = g ? (*g).gapY : 0.0f;
    if (gx)
        *gx = ox;
    if (gy)
        *gy = oy;
}

int32_t GridContainer_getHeaderRows(const GridContainer *g) {
    return g ? (*g).headerRows : 0;
}

int32_t GridContainer_getHeaderCols(const GridContainer *g) {
    return g ? (*g).headerCols : 0;
}

float GridContainer_getRowHeight(const GridContainer *g, int32_t row) {
    if (!g)
        return 0.0f;
    if (row < 0 || row >= (*g).rows)
        return 0.0f;
    float *heights = (*g).rowHeights;
    if (!heights)
        return 0.0f;
    return heights[row];
}

int32_t GridContainer_rowCount(const GridContainer *g) {
    return g ? (*g).rows : 0;
}

int32_t GridContainer_colCount(const GridContainer *g) {
    return g ? (*g).cols : 0;
}
