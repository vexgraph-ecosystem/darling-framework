#ifndef DARLING_GRID_PANEL_H
#define DARLING_GRID_PANEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "c23/constructor.h"
#include "darling/panel/panel.h"
#include "oop/type.h"

// darling/panel/grid_container.h — excel core (fixed or auto rows x cols of
// Panel* cell slots; empty cell = nullptr, skipped in layout).
// Cells live ONLY in the row-major slot array below, never in the embedded
// base's child list. Detach-only: overwriting or clearing a slot drops the
// old pointer without freeing it. The grid auto-grows on setCell past the
// edge from either constructor; GridContainer_2 presets the initial extent.
// Selection/editing lives above (a GridView controller later), not here.

// Central registry owns these once landed; the guard keeps this shell
// compiling standalone until then.


typedef struct GridContainer {
    Panel base;
    Panel **cells;
    float *rowHeights;
    int32_t rows;
    int32_t cols;
    float gapX;
    float gapY;
    int32_t headerRows;
    int32_t headerCols;
} GridContainer;

// Constructors:
//   GridContainer()            — empty auto-grow grid (0 x 0)
//   GridContainer(rows, cols)  — preset extent, all cells empty, heights auto
GridContainer *GridContainer_0(void);
GridContainer *GridContainer_2(int32_t rows, int32_t cols);

#define GridContainer(...) CONSTRUCTOR_DISPATCH(GridContainer, __VA_ARGS__)

// Layout facade (same pattern as Panel_* shims: forward over the prefix).
static inline void GridGraphicsComponent_setLocation(GridContainer *g, float x, float y)
    { if (g) Panel_setLocation(&(*g).base, x, y); }
static inline void GridGraphicsComponent_setSize(GridContainer *g, float w, float h)
    { if (g) Panel_setSize(&(*g).base, w, h); }

// Core (cells are slots: nullptr clears; overwrite drops without freeing;
// every mutation re-runs the layout pass).
void GridContainer_setCell(GridContainer *g, int32_t row, int32_t col, Panel *cell);
Panel *GridContainer_getCell(const GridContainer *g, int32_t row, int32_t col);
void GridContainer_layout(GridContainer *g);

// Setters.
void GridContainer_setGap(GridContainer *g, float gx, float gy);
void GridContainer_setHeaderRows(GridContainer *g, int32_t count);
void GridContainer_setHeaderCols(GridContainer *g, int32_t count);
void GridContainer_setRowHeight(GridContainer *g, int32_t row, float h);

// Getters.
void GridContainer_getGap(const GridContainer *g, float *gx, float *gy);
int32_t GridContainer_getHeaderRows(const GridContainer *g);
int32_t GridContainer_getHeaderCols(const GridContainer *g);
float GridContainer_getRowHeight(const GridContainer *g, int32_t row);
int32_t GridContainer_rowCount(const GridContainer *g);
int32_t GridContainer_colCount(const GridContainer *g);

#endif
