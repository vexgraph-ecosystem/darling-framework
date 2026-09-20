#include "darling/field/slider.h"

#include <math.h>

#include "annotation/definition.h"
#include "annotation/overview.h"
#include "event/pointer.h"
#include "nio/mem.h"
#include "oop/type.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Slider
 * ============================================================================
 * Linear value track with an optional second range thumb and change callback.
 * Embeds a Panel for layout and hierarchy; the struct is arena-allocated with
 * zero owned heap. Values clamp to [min,max], step drives keyboard nudges,
 * vertical/showValue/range flip presentation, and fill/knob carry packed
 * colors. Pointer handling maps local coordinates to the track fraction; the
 * borrowed onChange(ctx) callback fires on value changes. A leaf R4 field
 * widget with symmetric getters/setters.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Slider (embeds Panel)
 * LEVEL: L2 — Behavior (linear value track behavior API)
 * ============================================================================
 * Linear value track with an optional second range thumb and change callback.
 *
 * STRUCT FIELDS (Mirroring darling/field/slider.h):
 * ----------------------------------------------------------------------------
 *   Panel base;                  // Inherited layout, bounds, hierarchy state
 *   float min;                   // Lower track bound
 *   float max;                   // Upper track bound
 *   float value;                 // Primary thumb value, clamped to min/max
 *   float step;                  // Keyboard nudge increment
 *   float value2;                // Secondary range thumb value
 *   bool vertical;               // Vertical track flag
 *   bool showValue;              // Value readout flag
 *   bool range;                  // Dual-thumb range flag
 *   uint32_t fill;               // Track fill color, packed 0xAARRGGBB
 *   uint32_t knob;               // Thumb color, packed 0xAARRGGBB
 *   void (*onChange)(void *ctx); // Change callback; nullptr means none
 *   void *ctx;                   // Callback context pointer
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Slider_0(void)
 *   - Slider_1(parent)
 *
 * Core Functions:
 *   - Slider_setRange(s, min, max)
 *   - Slider_handlePointer(s, kind, localX, localY)
 *
 * Setters:
 *   - Slider_setMin(s, min)
 *   - Slider_setMax(s, max)
 *   - Slider_setValue(s, value)
 *   - Slider_setStep(s, step)
 *   - Slider_setValue2(s, value2)
 *   - Slider_setVertical(s, vertical)
 *   - Slider_setShowValue(s, show)
 *   - Slider_setRangeEnabled(s, enabled)
 *   - Slider_setFill(s, color)
 *   - Slider_setKnob(s, color)
 *   - Slider_setOnChange(s, cb)
 *   - Slider_setCtx(s, ctx)
 *
 * Getters:
 *   - Slider_getMin(s)
 *   - Slider_getMax(s)
 *   - Slider_getValue(s)
 *   - Slider_getStep(s)
 *   - Slider_getValue2(s)
 *   - Slider_isVertical(s)
 *   - Slider_isShowValue(s)
 *   - Slider_isRange(s)
 *   - Slider_getFill(s)
 *   - Slider_getKnob(s)
 *   - Slider_getOnChange(s)
 *   - Slider_getCtx(s)
 * ============================================================================
 */

// CONSTRUCTORS

Slider *Slider_0(void) {
    Slider *s = (Slider*) Memory_alloc(TYPE_SLIDER_SINGLETON, sizeof(Slider));
    if (!s)
        return nullptr;
    Panel *base = Panel_0();
    if (!base) {
        Memory_free(s);
        return nullptr;
    }
    (*s).base = (*base);
    Memory_free(base);
    (*s).min = 0.0f;
    (*s).max = 1.0f;
    (*s).value = 0.0f;
    (*s).step = 0.01f;
    (*s).value2 = 1.0f;
    (*s).vertical = false;
    (*s).showValue = true;
    (*s).range = false;
    (*s).fill = 0xFF3A86FFu;
    (*s).knob = 0xFFFFFFFFu;
    (*s).onChange = nullptr;
    (*s).ctx = nullptr;
    return s;
}

Slider *Slider_1(Panel *parent) {
    Slider *s = Slider_0();
    if (s && parent)
        Panel_addContainer(parent, &(*s).base);
    return s;
}

// CORE FUNCTIONS

static void markDirty(Slider *s) {
    if (!s) return;
    Panel *b = &(*s).base;
    Container_markDirty(&(*b).base);
}

void Slider_setRange(Slider *s, float min, float max) {
    if (!s) return;
    (*s).min = min;
    (*s).max = max;
    if ((*s).value < min) (*s).value = min;
    if ((*s).value > max) (*s).value = max;
    markDirty(s);
}

void Slider_handlePointer(Slider *s, int kind, float localX, float localY) {
    if (!s) return;
    Panel *p = &(*s).base;
    Container *cnt = &(*p).base;
    float w = (*cnt).w;
    float h = (*cnt).h;
    if (w <= 0.0f) w = 100.0f;
    if (h <= 0.0f) h = 20.0f;
    if (kind != PTR_DOWN && kind != PTR_DRAG) return;
    float ratio = (*s).vertical ? (localY / h) : (localX / w);
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    float val = (*s).min + ratio * ((*s).max - (*s).min);
    if ((*s).step > 0.0f)
        val = (*s).min + roundf((val - (*s).min) / (*s).step) * (*s).step;
    if (val < (*s).min) val = (*s).min;
    if (val > (*s).max) val = (*s).max;
    if (val != (*s).value) {
        (*s).value = val;
        markDirty(s);
        void (*fn)(void *ctx) = (*s).onChange;
        void *ctx = (*s).ctx;
        if (fn) fn(ctx);
    }
}

// SETTERS

static float clampValue(const Slider *s, float value) {
    float lo = (*s).min;
    float hi = (*s).max;
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

void Slider_setMin(Slider *s, float min) {
    if (!s)
        return;
    (*s).min = min;
    markDirty(s);
}

void Slider_setMax(Slider *s, float max) {
    if (!s)
        return;
    (*s).max = max;
    markDirty(s);
}

void Slider_setValue(Slider *s, float value) {
    if (!s)
        return;
    (*s).value = clampValue(s, value);
    markDirty(s);
}

void Slider_setStep(Slider *s, float step) {
    if (!s)
        return;
    (*s).step = step;
    markDirty(s);
}

void Slider_setValue2(Slider *s, float value2) {
    if (!s)
        return;
    (*s).value2 = clampValue(s, value2);
    markDirty(s);
}

void Slider_setVertical(Slider *s, bool vertical) {
    if (!s)
        return;
    (*s).vertical = vertical;
    markDirty(s);
}

void Slider_setShowValue(Slider *s, bool show) {
    if (!s)
        return;
    (*s).showValue = show;
    markDirty(s);
}

void Slider_setRangeEnabled(Slider *s, bool enabled) {
    if (!s)
        return;
    (*s).range = enabled;
    markDirty(s);
}

void Slider_setFill(Slider *s, uint32_t color) {
    if (!s)
        return;
    (*s).fill = color;
    markDirty(s);
}

void Slider_setKnob(Slider *s, uint32_t color) {
    if (!s)
        return;
    (*s).knob = color;
    markDirty(s);
}

void Slider_setOnChange(Slider *s, SliderChangeFn cb) {
    if (!s)
        return;
    (*s).onChange = cb;
}

void Slider_setCtx(Slider *s, void *ctx) {
    if (!s)
        return;
    (*s).ctx = ctx;
}

// GETTERS

float Slider_getMin(const Slider *s) {
    return s ? (*s).min : 0.0f;
}

float Slider_getMax(const Slider *s) {
    return s ? (*s).max : 0.0f;
}

float Slider_getValue(const Slider *s) {
    return s ? (*s).value : 0.0f;
}

float Slider_getStep(const Slider *s) {
    return s ? (*s).step : 0.0f;
}

float Slider_getValue2(const Slider *s) {
    return s ? (*s).value2 : 0.0f;
}

bool Slider_isVertical(const Slider *s) {
    return s ? (*s).vertical : false;
}

bool Slider_isShowValue(const Slider *s) {
    return s ? (*s).showValue : false;
}

bool Slider_isRange(const Slider *s) {
    return s ? (*s).range : false;
}

uint32_t Slider_getFill(const Slider *s) {
    return s ? (*s).fill : 0;
}

uint32_t Slider_getKnob(const Slider *s) {
    return s ? (*s).knob : 0;
}

SliderChangeFn Slider_getOnChange(const Slider *s) {
    return s ? (*s).onChange : nullptr;
}

void *Slider_getCtx(const Slider *s) {
    return s ? (*s).ctx : nullptr;
}
