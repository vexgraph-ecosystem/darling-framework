#include "render/surface.h"

#include <stdatomic.h>
#include <stdlib.h>

#include "nio/mem.h"
#include "oop/type.h"
#include "annotation/definition.h"
#include "annotation/overview.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Surface
 * ============================================================================
 * A scissored, double-buffered stamp of the master canvas — a mini-swapchain
 * per panel. Producers paint the back buffer and flip one atomic word while
 * the compositor stamps only the front, so a mid-paint stamp can never tear.
 * Because each panel owns its resolution, UI and scene stamp at different
 * scales onto the same master — seamless scaling by construction (v1 stamps
 * 1:1; scaled stamps arrive via Buffer_sample). The struct is arena-allocated
 * (Memory_alloc) with a ColorBuffer pair; the producer side (back/flip) runs
 * on draw threads while the compositor side (front/composite) runs on thread
 * 0, with the atomic front word as the only shared state. All accessors
 * null-guard and return safe defaults per the Cold-Strict, Hot-Minimal
 * Validation Law.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Surface (double-buffered panel swapchain)
 * LEVEL: L2 — Behavior (double-buffered surface behavior API)
 * ============================================================================
 * A scissored, double-buffered stamp of the master canvas: producers paint
 * the back buffer and flip one atomic word while the compositor stamps only
 * the front, so a mid-paint stamp can never tear.
 *
 * STRUCT FIELDS (local to this file):
 * ----------------------------------------------------------------------------
 *   Surface {              // Mini-swapchain per panel (opaque, see surface.h)
 *     Buffer *canvas[2];   // Back/front paint buffers (ColorBuffer pair)
 *     _Atomic int front;   // 0 or 1: which canvas the compositor may read
 *     int x;               // Scissor origin X on the master canvas
 *     int y;               // Scissor origin Y on the master canvas
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Surface_new(width, height, x, y)
 *
 * Core Functions:
 *   - Surface_back(panel)
 *   - Surface_flip(panel)
 *   - Surface_front(panel)
 *   - Surface_x(panel)
 *   - Surface_y(panel)
 *   - Surface_composite(panel, master)
 *   - Surface_free(panel)
 *
 * Setters:
 *   - Surface_setScissor(panel, x, y)
 * ============================================================================
 */


// render/panel.c — mini-swapchain per panel. The producer paints the back
// canvas and flips one atomic word; the compositor reads only the front, so
// a stamp mid-paint can never tear.

typedef struct Surface {
    Buffer *canvas[2];
    _Atomic int front;      // 0 or 1: which canvas the compositor may read
    int x;
    int y;
} Surface;

Surface *Surface_new(size_t width, size_t height, int x, int y) {
    Surface *p = (Surface*) Memory_alloc(FORM_STRUCT_SINGLETON | ID_CUSTOM_STRUCT, sizeof(Surface));
    if (!p)
        return nullptr;
    (*p).canvas[0] = ColorBuffer(width, height);
    (*p).canvas[1] = ColorBuffer(width, height);
    if (!(*p).canvas[0] || !(*p).canvas[1]) {
        if ((*p).canvas[0])
            Buffer_free((*p).canvas[0]);
        if ((*p).canvas[1])
            Buffer_free((*p).canvas[1]);
        Memory_free(p);
        return nullptr;
    }
    atomic_init(&(*p).front, 0);
    (*p).x = x;
    (*p).y = y;
    return p;
}

Buffer *Surface_back(Surface *panel) {
    if (!panel)
        return nullptr;
    int front = atomic_load_explicit(&(*panel).front, memory_order_relaxed);
    return (*panel).canvas[front ^ 1];
}

void Surface_flip(Surface *panel) {
    if (!panel)
        return;
    atomic_fetch_xor_explicit(&(*panel).front, 1, memory_order_acq_rel);
}

const Buffer *Surface_front(Surface *panel) {
    if (!panel)
        return nullptr;
    int front = atomic_load_explicit(&(*panel).front, memory_order_acquire);
    return (*panel).canvas[front];
}

void Surface_setScissor(Surface *panel, int x, int y) {
    if (!panel)
        return;
    (*panel).x = x;
    (*panel).y = y;
}

int Surface_x(Surface *panel) {
    return panel ? (*panel).x : 0;
}

int Surface_y(Surface *panel) {
    return panel ? (*panel).y : 0;
}

// Stamp front onto master at the scissor, clipped to both rectangles.
void Surface_composite(Surface *panel, Buffer *master) {
    if (!panel || !master)
        return;
    const Buffer *src = Surface_front(panel);
    size_t sw = Buffer_width(src);
    size_t sh = Buffer_height(src);

    int sx = (*panel).x;
    int sy = (*panel).y;

    long clipW = (long)Buffer_width(master) - sx;
    long clipH = (long)Buffer_height(master) - sy;
    if (clipW > (long)sw)
        clipW = (long)sw;
    if (clipH > (long)sh)
        clipH = (long)sh;
    if (sx < 0) {
        // negative scissor: skip the off-canvas left/top band
        long skip = -sx;
        sx += skip;
        clipW -= skip > 0 ? skip : 0;
    }
    if (sy < 0) {
        long skip = -sy;
        sy += skip;
        clipH -= skip > 0 ? skip : 0;
    }
    if (clipW <= 0 || clipH <= 0 || sx < 0 || sy < 0)
        return;

    Buffer_blit(src, master, 0, 0, (size_t)sx, (size_t)sy,
                (size_t)clipW, (size_t)clipH);
}

void Surface_free(Surface *panel) {
    if (!panel)
        return;
    Buffer_free((*panel).canvas[0]);
    Buffer_free((*panel).canvas[1]);
    Memory_free(panel);
}
