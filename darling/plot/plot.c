#include "darling/plot/plot.h"

#include <string.h>

#include "annotation/incomplete.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include "nio/mem.h"
#include "oop/type.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Plot
 * ============================================================================
 * Data-first chart embedding Panel: borrowed vexspoke float buffers (xs/ys)
 * rendered in a ranged, colored, labeled frame with grid/axes/legend toggles
 * and an optional pick callback. The data buffers are strictly borrowed —
 * Plot never owns, copies, or frees them; the caller keeps them alive while
 * attached. Owned state is limited to the title/xlabel/ylabel strings (arena
 * copies) plus range, color, and visibility flags; Plot_autorange recomputes
 * ranges from the data on demand and Plot_hitPick maps a point to a sample
 * index. The chart is a leaf display node in the R4 stack, sibling to the
 * label family.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Plot (embeds Panel)
 * LEVEL: L2 — Behavior (data-first chart behavior API)
 * ============================================================================
 * Data-first chart: borrowed float buffers rendered in a ranged, colored,
 * labeled frame with grid/axes/legend toggles and a select callback slot.
 *
 * STRUCT FIELDS (Mirroring darling/plot/plot.h):
 * ----------------------------------------------------------------------------
 *   Panel base;                  // Inherited layout, bounds, hierarchy state
 *   int32_t kind;                // PlotKind (PLOT_LINE=0, BAR, SCATTER, HIST, AREA)
 *   Buffer *xs;                  // Borrowed x float buffer; never owned or freed
 *   Buffer *ys;                  // Borrowed y float buffer; never owned or freed
 *   size_t count;                // Sample count over xs/ys
 *   bool autoRange;              // Recompute ranges from data on render
 *   float minX;                  // X range lower bound
 *   float maxX;                  // X range upper bound
 *   float minY;                  // Y range lower bound
 *   float maxY;                  // Y range upper bound
 *   uint32_t lineColor;          // Series stroke color, packed 0xAARRGGBB
 *   uint32_t fillColor;          // Series fill color, packed 0xAARRGGBB
 *   uint32_t gridColor;          // Grid line color, packed 0xAARRGGBB
 *   uint32_t axisColor;          // Axis line color, packed 0xAARRGGBB
 *   char *title;                 // Owned title text; nullptr = none
 *   char *xlabel;                // Owned x-axis label; nullptr = none
 *   char *ylabel;                // Owned y-axis label; nullptr = none
 *   bool showGrid;               // Grid visibility flag
 *   bool showAxes;               // Axes visibility flag
 *   bool showLegend;             // Legend visibility flag
 *   void (*onSelect)(void *ctx); // Pick callback; nullptr means none
 *   void *ctx;                   // Callback context pointer
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Plot_0(void)
 *   - Plot_1(parent)
 *
 * Core Functions:
 *   - Plot_autorange(p)
 *   - Plot_hitPick(p, x, y)
 *
 * Setters:
 *   - Plot_setKind(p, kind)
 *   - Plot_setData(p, xs, ys, count)
 *   - Plot_setAutoRange(p, autoRange)
 *   - Plot_setMinX(p, minX)
 *   - Plot_setMaxX(p, maxX)
 *   - Plot_setMinY(p, minY)
 *   - Plot_setMaxY(p, maxY)
 *   - Plot_setXRange(p, min, max)
 *   - Plot_setYRange(p, min, max)
 *   - Plot_setLineColor(p, color)
 *   - Plot_setFillColor(p, color)
 *   - Plot_setGridColor(p, color)
 *   - Plot_setAxisColor(p, color)
 *   - Plot_setTitle(p, title)
 *   - Plot_setXLabel(p, xlabel)
 *   - Plot_setYLabel(p, ylabel)
 *   - Plot_setShowGrid(p, show)
 *   - Plot_setShowAxes(p, show)
 *   - Plot_setShowLegend(p, show)
 *   - Plot_setOnSelect(p, cb)
 *   - Plot_setCtx(p, ctx)
 *   - Plot_free(p)
 *
 * Getters:
 *   - Plot_getKind(p)
 *   - Plot_getXs(p)
 *   - Plot_getYs(p)
 *   - Plot_getCount(p)
 *   - Plot_isAutoRange(p)
 *   - Plot_getMinX(p)
 *   - Plot_getMaxX(p)
 *   - Plot_getMinY(p)
 *   - Plot_getMaxY(p)
 *   - Plot_getLineColor(p)
 *   - Plot_getFillColor(p)
 *   - Plot_getGridColor(p)
 *   - Plot_getAxisColor(p)
 *   - Plot_getTitle(p)
 *   - Plot_getXLabel(p)
 *   - Plot_getYLabel(p)
 *   - Plot_isShowGrid(p)
 *   - Plot_isShowAxes(p)
 *   - Plot_isShowLegend(p)
 *   - Plot_getOnSelect(p)
 *   - Plot_getCtx(p)
 * ============================================================================
 */

// CONSTRUCTORS

Plot *Plot_0(void) {
    Plot *p = (Plot*) Memory_alloc(TYPE_PLOT_SINGLETON, sizeof(Plot));
    if (!p)
        return nullptr;
    Panel *base = Panel_0();
    if (!base) {
        Memory_free(p);
        return nullptr;
    }
    (*p).base = (*base);
    Memory_free(base);
    (*p).kind = PLOT_LINE;
    (*p).xs = nullptr;
    (*p).ys = nullptr;
    (*p).count = 0;
    (*p).autoRange = true;
    (*p).minX = 0.0f;
    (*p).maxX = 1.0f;
    (*p).minY = 0.0f;
    (*p).maxY = 1.0f;
    (*p).lineColor = 0xFF3A86FFu;
    (*p).fillColor = 0x663A86FFu;
    (*p).gridColor = 0xFF3A3A3Au;
    (*p).axisColor = 0xFFFFFFFFu;
    (*p).title = nullptr;
    (*p).xlabel = nullptr;
    (*p).ylabel = nullptr;
    (*p).showGrid = true;
    (*p).showAxes = true;
    (*p).showLegend = false;
    (*p).onSelect = nullptr;
    (*p).ctx = nullptr;
    return p;
}

Plot *Plot_1(Panel *parent) {
    Plot *p = Plot_0();
    if (p && parent) {
        Panel *bp = &(*p).base;
        Panel_addContainer(parent, bp);
    }
    return p;
}

// CORE FUNCTIONS

void Plot_autorange(Plot *p) {
    ;;INCOMPLETE // range computation deferred
    (void)p;
}

int32_t Plot_hitPick(const Plot *p, float x, float y) {
    ;;INCOMPLETE // point picking deferred
    (void)p;
    (void)x;
    (void)y;
    return -1;
}

// SETTERS

static void markDirty(Plot *p) {
    if (!p)
        return;
    Panel *b = &(*p).base;
    Container_markDirty(&(*b).base);
}

void Plot_setKind(Plot *p, int32_t kind) {
    if (!p)
        return;
    (*p).kind = kind;
    markDirty(p);
}

void Plot_setData(Plot *p, Buffer *xs, Buffer *ys, size_t count) {
    if (!p)
        return;
    (*p).xs = xs;
    (*p).ys = ys;
    (*p).count = count;
    markDirty(p);
}

void Plot_setAutoRange(Plot *p, bool autoRange) {
    if (!p)
        return;
    (*p).autoRange = autoRange;
    markDirty(p);
}

void Plot_setMinX(Plot *p, float minX) {
    if (!p)
        return;
    (*p).minX = minX;
    float lo = (*p).minX;
    float hi = (*p).maxX;
    if (lo > hi) {
        (*p).minX = hi;
        (*p).maxX = lo;
    }
    markDirty(p);
}

void Plot_setMaxX(Plot *p, float maxX) {
    if (!p)
        return;
    (*p).maxX = maxX;
    float lo = (*p).minX;
    float hi = (*p).maxX;
    if (lo > hi) {
        (*p).minX = hi;
        (*p).maxX = lo;
    }
    markDirty(p);
}

void Plot_setMinY(Plot *p, float minY) {
    if (!p)
        return;
    (*p).minY = minY;
    float lo = (*p).minY;
    float hi = (*p).maxY;
    if (lo > hi) {
        (*p).minY = hi;
        (*p).maxY = lo;
    }
    markDirty(p);
}

void Plot_setMaxY(Plot *p, float maxY) {
    if (!p)
        return;
    (*p).maxY = maxY;
    float lo = (*p).minY;
    float hi = (*p).maxY;
    if (lo > hi) {
        (*p).minY = hi;
        (*p).maxY = lo;
    }
    markDirty(p);
}

void Plot_setXRange(Plot *p, float min, float max) {
    if (!p)
        return;
    if (min > max) {
        float tmp = min;
        min = max;
        max = tmp;
    }
    (*p).minX = min;
    (*p).maxX = max;
    markDirty(p);
}

void Plot_setYRange(Plot *p, float min, float max) {
    if (!p)
        return;
    if (min > max) {
        float tmp = min;
        min = max;
        max = tmp;
    }
    (*p).minY = min;
    (*p).maxY = max;
    markDirty(p);
}

void Plot_setLineColor(Plot *p, uint32_t color) {
    if (!p)
        return;
    (*p).lineColor = color;
    markDirty(p);
}

void Plot_setFillColor(Plot *p, uint32_t color) {
    if (!p)
        return;
    (*p).fillColor = color;
    markDirty(p);
}

void Plot_setGridColor(Plot *p, uint32_t color) {
    if (!p)
        return;
    (*p).gridColor = color;
    markDirty(p);
}

void Plot_setAxisColor(Plot *p, uint32_t color) {
    if (!p)
        return;
    (*p).axisColor = color;
    markDirty(p);
}

void Plot_setTitle(Plot *p, const char *title) {
    if (!p)
        return;
    char *old = (*p).title;
    if (old) {
        Memory_free(old);
        (*p).title = nullptr;
    }
    if (title) {
        size_t len = strlen(title) + 1;
        char *buf = (char*) Memory_alloc(TYPE_ARRAY, len);
        if (buf)
            memcpy(buf, title, len);
        (*p).title = buf;
    }
    markDirty(p);
}

void Plot_setXLabel(Plot *p, const char *xlabel) {
    if (!p)
        return;
    char *old = (*p).xlabel;
    if (old) {
        Memory_free(old);
        (*p).xlabel = nullptr;
    }
    if (xlabel) {
        size_t len = strlen(xlabel) + 1;
        char *buf = (char*) Memory_alloc(TYPE_ARRAY, len);
        if (buf)
            memcpy(buf, xlabel, len);
        (*p).xlabel = buf;
    }
    markDirty(p);
}

void Plot_setYLabel(Plot *p, const char *ylabel) {
    if (!p)
        return;
    char *old = (*p).ylabel;
    if (old) {
        Memory_free(old);
        (*p).ylabel = nullptr;
    }
    if (ylabel) {
        size_t len = strlen(ylabel) + 1;
        char *buf = (char*) Memory_alloc(TYPE_ARRAY, len);
        if (buf)
            memcpy(buf, ylabel, len);
        (*p).ylabel = buf;
    }
    markDirty(p);
}

void Plot_setShowGrid(Plot *p, bool show) {
    if (!p)
        return;
    (*p).showGrid = show;
    markDirty(p);
}

void Plot_setShowAxes(Plot *p, bool show) {
    if (!p)
        return;
    (*p).showAxes = show;
    markDirty(p);
}

void Plot_setShowLegend(Plot *p, bool show) {
    if (!p)
        return;
    (*p).showLegend = show;
    markDirty(p);
}

void Plot_setOnSelect(Plot *p, PlotSelectFn cb) {
    if (!p)
        return;
    (*p).onSelect = cb;
}

void Plot_setCtx(Plot *p, void *ctx) {
    if (!p)
        return;
    (*p).ctx = ctx;
}

void Plot_free(Plot *p) {
    if (!p)
        return;
    char *title = (*p).title;
    if (title)
        Memory_free(title);
    char *xl = (*p).xlabel;
    if (xl)
        Memory_free(xl);
    char *yl = (*p).ylabel;
    if (yl)
        Memory_free(yl);
    (*p).title = nullptr;
    (*p).xlabel = nullptr;
    (*p).ylabel = nullptr;
    (*p).xs = nullptr;
    (*p).ys = nullptr;
    (*p).onSelect = nullptr;
    (*p).ctx = nullptr;
    Memory_free(p);
}

// GETTERS

int32_t Plot_getKind(const Plot *p) {
    return p ? (*p).kind : PLOT_LINE;
}

Buffer *Plot_getXs(const Plot *p) {
    return p ? (*p).xs : nullptr;
}

Buffer *Plot_getYs(const Plot *p) {
    return p ? (*p).ys : nullptr;
}

size_t Plot_getCount(const Plot *p) {
    return p ? (*p).count : 0;
}

bool Plot_isAutoRange(const Plot *p) {
    return p ? (*p).autoRange : false;
}

float Plot_getMinX(const Plot *p) {
    return p ? (*p).minX : 0.0f;
}

float Plot_getMaxX(const Plot *p) {
    return p ? (*p).maxX : 0.0f;
}

float Plot_getMinY(const Plot *p) {
    return p ? (*p).minY : 0.0f;
}

float Plot_getMaxY(const Plot *p) {
    return p ? (*p).maxY : 0.0f;
}

uint32_t Plot_getLineColor(const Plot *p) {
    return p ? (*p).lineColor : 0;
}

uint32_t Plot_getFillColor(const Plot *p) {
    return p ? (*p).fillColor : 0;
}

uint32_t Plot_getGridColor(const Plot *p) {
    return p ? (*p).gridColor : 0;
}

uint32_t Plot_getAxisColor(const Plot *p) {
    return p ? (*p).axisColor : 0;
}

const char *Plot_getTitle(const Plot *p) {
    return p ? (*p).title : nullptr;
}

const char *Plot_getXLabel(const Plot *p) {
    return p ? (*p).xlabel : nullptr;
}

const char *Plot_getYLabel(const Plot *p) {
    return p ? (*p).ylabel : nullptr;
}

bool Plot_isShowGrid(const Plot *p) {
    return p ? (*p).showGrid : false;
}

bool Plot_isShowAxes(const Plot *p) {
    return p ? (*p).showAxes : false;
}

bool Plot_isShowLegend(const Plot *p) {
    return p ? (*p).showLegend : false;
}

PlotSelectFn Plot_getOnSelect(const Plot *p) {
    return p ? (*p).onSelect : nullptr;
}

void *Plot_getCtx(const Plot *p) {
    return p ? (*p).ctx : nullptr;
}
