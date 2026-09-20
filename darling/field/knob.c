#include "darling/field/knob.h"

#include <math.h>

#include "annotation/definition.h"
#include "annotation/overview.h"
#include "event/pointer.h"
#include "nio/mem.h"
#include "oop/type.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Knob
 * ============================================================================
 * Rotary value dial mapping a clamped value over an angular sweep. Embeds a
 * Panel for layout and hierarchy; the struct is arena-allocated with zero
 * owned heap. Pointer DOWN/DRAG maps the local angle to a normalized t,
 * clamps to [0,1], scales into [min,max], and fires the borrowed
 * onChange(ctx) callback only when the value actually changes. A leaf R4
 * field widget with symmetric getters/setters and zero steady-state
 * allocation.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Knob (embeds Panel)
 * LEVEL: L2 — Behavior (rotary dial behavior API)
 * ============================================================================
 * Rotary value dial mapping a clamped value over an angular sweep.
 *
 * STRUCT FIELDS (Mirroring darling/field/knob.h):
 * ----------------------------------------------------------------------------
 *   Panel base;                  // Inherited layout, bounds, hierarchy state
 *   float min;                   // Lower value bound
 *   float max;                   // Upper value bound
 *   float value;                 // Dial value, clamped to min/max
 *   float startAngle;            // Sweep start in degrees
 *   float sweep;                 // Sweep extent in degrees
 *   float diameter;              // Dial diameter in parent units
 *   void (*onChange)(void *ctx); // Change callback; nullptr means none
 *   void *ctx;                   // Callback context pointer
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Knob_0(void)
 *   - Knob_1(parent)
 *
 * Core Functions:
 *   - Knob_setNormalized(k, t)
 *   - Knob_handlePointer(k, kind, localX, localY)
 *
 * Setters:
 *   - Knob_setMin(k, min)
 *   - Knob_setMax(k, max)
 *   - Knob_setValue(k, value)
 *   - Knob_setStartAngle(k, angle)
 *   - Knob_setSweep(k, sweep)
 *   - Knob_setDiameter(k, diameter)
 *   - Knob_setOnChange(k, cb)
 *   - Knob_setCtx(k, ctx)
 *
 * Getters:
 *   - Knob_getMin(k)
 *   - Knob_getMax(k)
 *   - Knob_getValue(k)
 *   - Knob_getStartAngle(k)
 *   - Knob_getSweep(k)
 *   - Knob_getDiameter(k)
 *   - Knob_getOnChange(k)
 *   - Knob_getCtx(k)
 * ============================================================================
 */

// CONSTRUCTORS

Knob *Knob_0(void) {
    Knob *k = (Knob*) Memory_alloc(TYPE_KNOB_SINGLETON, sizeof(Knob));
    if (!k)
        return nullptr;
    Panel *base = Panel_0();
    if (!base) {
        Memory_free(k);
        return nullptr;
    }
    (*k).base = (*base);
    Memory_free(base);
    (*k).min = 0.0f;
    (*k).max = 1.0f;
    (*k).value = 0.0f;
    (*k).startAngle = 135.0f;
    (*k).sweep = 270.0f;
    (*k).diameter = 48.0f;
    (*k).onChange = nullptr;
    (*k).ctx = nullptr;
    return k;
}

Knob *Knob_1(Panel *parent) {
    Knob *k = Knob_0();
    if (k && parent)
        Panel_addContainer(parent, &(*k).base);
    return k;
}

// CORE FUNCTIONS

static void markDirty(Knob *k) {
    if (!k) return;
    Panel *b = &(*k).base;
    Container_markDirty(&(*b).base);
}

void Knob_setNormalized(Knob *k, float t) {
    if (!k) return;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    float val = (*k).min + t * ((*k).max - (*k).min);
    if (val != (*k).value) {
        (*k).value = val;
        markDirty(k);
        void (*fn)(void *ctx) = (*k).onChange;
        void *ctx = (*k).ctx;
        if (fn) fn(ctx);
    }
}

void Knob_handlePointer(Knob *k, int kind, float localX, float localY) {
    if (!k) return;
    if (kind != PTR_DOWN && kind != PTR_DRAG) return;
    Panel *p = &(*k).base;
    Container *cnt = &(*p).base;
    float d = (*k).diameter;
    if (d <= 0.0f) d = (*cnt).w > 0.0f ? (*cnt).w : 40.0f;
    float cx = d * 0.5f;
    float cy = d * 0.5f;
    float dx = localX - cx;
    float dy = localY - cy;
    float angle = atan2f(dy, dx); /* [-PI, PI] */
    float norm = (angle + 3.14159265f) / (2.0f * 3.14159265f);
    Knob_setNormalized(k, norm);
}

// SETTERS

static float clampValue(const Knob *k, float value) {
    float lo = (*k).min;
    float hi = (*k).max;
    if (lo > hi) {
        float tmp = lo;
        lo = hi;
        hi = tmp;
    }
    if (value < lo)
        return lo;
    if (value > hi)
        return hi;
    return value;
}

void Knob_setMin(Knob *k, float min) {
    if (!k)
        return;
    (*k).min = min;
    markDirty(k);
}

void Knob_setMax(Knob *k, float max) {
    if (!k)
        return;
    (*k).max = max;
    markDirty(k);
}

void Knob_setValue(Knob *k, float value) {
    if (!k)
        return;
    (*k).value = clampValue(k, value);
    markDirty(k);
}

void Knob_setStartAngle(Knob *k, float angle) {
    if (!k)
        return;
    (*k).startAngle = angle;
    markDirty(k);
}

void Knob_setSweep(Knob *k, float sweep) {
    if (!k)
        return;
    (*k).sweep = sweep;
    markDirty(k);
}

void Knob_setDiameter(Knob *k, float diameter) {
    if (!k)
        return;
    (*k).diameter = diameter;
    markDirty(k);
}

void Knob_setOnChange(Knob *k, KnobChangeFn cb) {
    if (!k)
        return;
    (*k).onChange = cb;
}

void Knob_setCtx(Knob *k, void *ctx) {
    if (!k)
        return;
    (*k).ctx = ctx;
}

// GETTERS

float Knob_getMin(const Knob *k) {
    return k ? (*k).min : 0.0f;
}

float Knob_getMax(const Knob *k) {
    return k ? (*k).max : 0.0f;
}

float Knob_getValue(const Knob *k) {
    return k ? (*k).value : 0.0f;
}

float Knob_getStartAngle(const Knob *k) {
    return k ? (*k).startAngle : 0.0f;
}

float Knob_getSweep(const Knob *k) {
    return k ? (*k).sweep : 0.0f;
}

float Knob_getDiameter(const Knob *k) {
    return k ? (*k).diameter : 0.0f;
}

KnobChangeFn Knob_getOnChange(const Knob *k) {
    return k ? (*k).onChange : nullptr;
}

void *Knob_getCtx(const Knob *k) {
    return k ? (*k).ctx : nullptr;
}
