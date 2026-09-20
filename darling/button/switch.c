#include "darling/button/switch.h"

#include "darling/panel/panel.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include "event/pointer.h"
#include "nio/mem.h"
#include "oop/type.h"

#include <stdbool.h>
#include <stdint.h>

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Switch
 * ============================================================================
 * Panel shell for an on/off toggle switch: a boolean state, track and knob
 * colors, and a change callback that fires whenever the state flips. Toggle
 * routes through setOn so the callback always fires on a real flip; pointer
 * handling flips state on a PTR_UP inside the track bounds. A leaf R4 widget
 * with no owned resources beyond the embedded Panel base — heap-allocated once
 * at construction, zero steady-state allocation.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Switch (embeds Panel)
 * LEVEL: L2 — Behavior (on/off toggle switch shell)
 * ============================================================================
 * Panel shell for an on/off toggle switch with track and knob colors plus a
 * change callback that fires whenever the state flips.
 *
 * STRUCT FIELDS (Mirroring darling/button/switch.h):
 * ----------------------------------------------------------------------------
 *   Panel base;                        // Inherited layout/tree/background state
 *   bool on;                           // True = on position
 *   uint32_t trackOn;                  // Packed 0xAARRGGBB on-state track
 *   uint32_t trackOff;                 // Packed 0xAARRGGBB off-state track
 *   uint32_t knob;                     // Packed 0xAARRGGBB knob fill
 *   void (*onChange)(void *ctx);       // Change callback; nullptr = none
 *   void *ctx;                         // Callback context
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Switch_0(void)
 *   - Switch_1(parent)
 *
 * Core Functions:
 *   - Switch_toggle(s)
 *   - Switch_handlePointer(s, kind, localX, localY)
 *
 * Setters:
 *   - Switch_setOn(s, on)
 *   - Switch_setTrackOn(s, color)
 *   - Switch_setTrackOff(s, color)
 *   - Switch_setKnob(s, color)
 *   - Switch_setOnChange(s, fn, ctx)
 *
 * Getters:
 *   - Switch_isOn(s)
 *   - Switch_getTrackOn(s)
 *   - Switch_getTrackOff(s)
 *   - Switch_getKnob(s)
 *   - Switch_getOnChange(s)
 *   - Switch_getChangeContext(s)
 * ============================================================================
 */

// CONSTRUCTORS
// ============================================================================

Switch *Switch_0(void) {
    Switch *s = (Switch*) Memory_alloc(TYPE_SWITCH_SINGLETON, sizeof(Switch));
    if (!s)
        return nullptr;
    Panel *p = Panel_0();
    if (!p) {
        Memory_free(s);
        return nullptr;
    }
    (*s).base = (*p);
    Memory_free(p);
    (*s).on = false;
    (*s).trackOn = 0xFF4CAF50u;
    (*s).trackOff = 0xFF777777u;
    (*s).knob = 0xFFFFFFFFu;
    (*s).onChange = nullptr;
    (*s).ctx = nullptr;
    return s;
}

Switch *Switch_1(Panel *parent) {
    Switch *s = Switch_0();
    if (s && parent) {
        Panel *p = &(*s).base;
        Panel_addContainer(parent, p);
    }
    return s;
}

// CORE FUNCTIONS
// ============================================================================

void Switch_toggle(Switch *s) {
    if (!s)
        return;
    Switch_setOn(s, !(*s).on);
}

void Switch_handlePointer(Switch *s, int kind, float localX, float localY) {
    if (!s)
        return;
    Panel *p = &(*s).base;
    Container *cnt = &(*p).base;
    float w = (*cnt).w > 0.0f ? (*cnt).w : 44.0f;
    float h = (*cnt).h > 0.0f ? (*cnt).h : 24.0f;
    bool inside = (localX >= 0.0f && localX <= w && localY >= 0.0f && localY <= h);
    if (kind == PTR_UP && inside)
        Switch_setOn(s, !(*s).on);
}

// SETTERS
// ============================================================================

static void markDirty(Switch *s) {
    if (!s)
        return;
    Panel *p = &(*s).base;
    Container *c = &(*p).base;
    Container_markDirty(c);
}

void Switch_setOn(Switch *s, bool on) {
    if (!s)
        return;
    if ((*s).on == on)
        return;
    (*s).on = on;
    markDirty(s);
    void (*fn)(void *ctx) = (*s).onChange;
    void *ctx = (*s).ctx;
    if (fn)
        fn(ctx);
}

void Switch_setTrackOn(Switch *s, uint32_t color) {
    if (!s)
        return;
    (*s).trackOn = color;
    markDirty(s);
}

void Switch_setTrackOff(Switch *s, uint32_t color) {
    if (!s)
        return;
    (*s).trackOff = color;
    markDirty(s);
}

void Switch_setKnob(Switch *s, uint32_t color) {
    if (!s)
        return;
    (*s).knob = color;
    markDirty(s);
}

void Switch_setOnChange(Switch *s, void (*fn)(void *ctx), void *ctx) {
    if (!s)
        return;
    (*s).onChange = fn;
    (*s).ctx = ctx;
}

// GETTERS
// ============================================================================

bool Switch_isOn(const Switch *s) {
    return s ? (*s).on : false;
}

uint32_t Switch_getTrackOn(const Switch *s) {
    return s ? (*s).trackOn : 0u;
}

uint32_t Switch_getTrackOff(const Switch *s) {
    return s ? (*s).trackOff : 0u;
}

uint32_t Switch_getKnob(const Switch *s) {
    return s ? (*s).knob : 0u;
}

void (*Switch_getOnChange(const Switch *s))(void *ctx) {
    return s ? (*s).onChange : nullptr;
}

void *Switch_getChangeContext(const Switch *s) {
    return s ? (*s).ctx : nullptr;
}
