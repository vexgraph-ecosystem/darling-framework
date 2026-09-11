#ifndef DARLING_PLOT_H
#define DARLING_PLOT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "buffer/buffer.h"
#include "c23/constructor.h"
#include "darling/panel/panel.h"
#include "oop/type.h"


// darling/plot/plot.h — data-first chart: borrowed float buffers + axis
// ranges + colors + owned labels (ggplot-inspired, single class).
//
// xs/ys are BORROWED refs to vexspoke float buffers: Plot never owns, copies,
// or frees them — the caller keeps them alive while attached.

typedef void (*PlotSelectFn)(void *ctx);

typedef enum PlotKind {
    PLOT_LINE = 0,
    PLOT_BAR,
    PLOT_SCATTER,
    PLOT_HIST,
    PLOT_AREA
} PlotKind;

typedef struct Plot {
    Panel base;
    int32_t kind;
    Buffer *xs;
    Buffer *ys;
    size_t count;
    bool autoRange;
    float minX;
    float maxX;
    float minY;
    float maxY;
    uint32_t lineColor;
    uint32_t fillColor;
    uint32_t gridColor;
    uint32_t axisColor;
    char *title;
    char *xlabel;
    char *ylabel;
    bool showGrid;
    bool showAxes;
    bool showLegend;
    void (*onSelect)(void *ctx);
    void *ctx;
} Plot;

// Constructors:
//   Plot()        — detached line plot on [0, 1] x [0, 1]
//   Plot(parent)  — created and attached
Plot *Plot_0(void);
Plot *Plot_1(Panel *parent);

#define Plot(...) CONSTRUCTOR_DISPATCH(Plot, __VA_ARGS__)

// Core (shell stubs; range computation + picking land with the render pass).
void Plot_autorange(Plot *p);
int32_t Plot_hitPick(const Plot *p, float x, float y);

void Plot_free(Plot *p);

// Setters.
void Plot_setKind(Plot *p, int32_t kind);
void Plot_setData(Plot *p, Buffer *xs, Buffer *ys, size_t count);
void Plot_setAutoRange(Plot *p, bool autoRange);
void Plot_setMinX(Plot *p, float minX);
void Plot_setMaxX(Plot *p, float maxX);
void Plot_setMinY(Plot *p, float minY);
void Plot_setMaxY(Plot *p, float maxY);
void Plot_setXRange(Plot *p, float min, float max);
void Plot_setYRange(Plot *p, float min, float max);
void Plot_setLineColor(Plot *p, uint32_t color);
void Plot_setFillColor(Plot *p, uint32_t color);
void Plot_setGridColor(Plot *p, uint32_t color);
void Plot_setAxisColor(Plot *p, uint32_t color);
void Plot_setTitle(Plot *p, const char *title);
void Plot_setXLabel(Plot *p, const char *xlabel);
void Plot_setYLabel(Plot *p, const char *ylabel);
void Plot_setShowGrid(Plot *p, bool show);
void Plot_setShowAxes(Plot *p, bool show);
void Plot_setShowLegend(Plot *p, bool show);
void Plot_setOnSelect(Plot *p, PlotSelectFn cb);
void Plot_setCtx(Plot *p, void *ctx);

// Getters.
int32_t Plot_getKind(const Plot *p);
Buffer *Plot_getXs(const Plot *p);
Buffer *Plot_getYs(const Plot *p);
size_t Plot_getCount(const Plot *p);
bool Plot_isAutoRange(const Plot *p);
float Plot_getMinX(const Plot *p);
float Plot_getMaxX(const Plot *p);
float Plot_getMinY(const Plot *p);
float Plot_getMaxY(const Plot *p);
uint32_t Plot_getLineColor(const Plot *p);
uint32_t Plot_getFillColor(const Plot *p);
uint32_t Plot_getGridColor(const Plot *p);
uint32_t Plot_getAxisColor(const Plot *p);
const char *Plot_getTitle(const Plot *p);
const char *Plot_getXLabel(const Plot *p);
const char *Plot_getYLabel(const Plot *p);
bool Plot_isShowGrid(const Plot *p);
bool Plot_isShowAxes(const Plot *p);
bool Plot_isShowLegend(const Plot *p);
PlotSelectFn Plot_getOnSelect(const Plot *p);
void *Plot_getCtx(const Plot *p);

#endif
