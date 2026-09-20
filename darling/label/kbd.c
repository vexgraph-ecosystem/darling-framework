#include "darling/label/kbd.h"

#include <string.h>

#include "annotation/definition.h"
#include "annotation/overview.h"
#include "nio/mem.h"
#include "oop/type.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Kbd
 * ============================================================================
 * Keyboard-shortcut chip: a pure display leaf embedding Panel, carrying an
 * owned key string and packed 0xAARRGGBB chip colors. The node is
 * arena-allocated (TYPE_KBD_SINGLETON) and the key string is copied into the
 * arena on set (TYPE_ARRAY), so the chip owns its payload and never borrows
 * caller text. Every setter marks the embedded Container dirty so the next
 * layout pass repaints the chip; there is no core logic — Kbd is a
 * display-only node in the label family, sibling to Typography, that renders
 * whatever the Panel pipeline paints.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Kbd (embeds Panel)
 * LEVEL: L2 — Behavior (shortcut chip behavior API)
 * ============================================================================
 * Keyboard-shortcut chip with an owned key string and chip colors.
 *
 * STRUCT FIELDS (Mirroring darling/label/kbd.h):
 * ----------------------------------------------------------------------------
 *   Panel base;     // Inherited layout, bounds, and hierarchy state
 *   char *keys;     // Owned key string payload
 *   uint32_t bg;    // Chip background, packed 0xAARRGGBB
 *   uint32_t fg;    // Chip foreground, packed 0xAARRGGBB
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Kbd_0(void)
 *   - Kbd_1(keys)
 *   - Kbd_2(parent, keys)
 *
 * Core Functions:
 *   - (none: pure display node)
 *
 * Setters:
 *   - Kbd_setKeys(k, keys)
 *   - Kbd_setBg(k, color)
 *   - Kbd_setFg(k, color)
 *   - Kbd_free(k)
 *
 * Getters:
 *   - Kbd_getKeys(k)
 *   - Kbd_getBg(k)
 *   - Kbd_getFg(k)
 * ============================================================================
 */

// CONSTRUCTORS

Kbd *Kbd_0(void) {
    Kbd *k = (Kbd*) Memory_alloc(TYPE_KBD_SINGLETON, sizeof(Kbd));
    if (!k)
        return nullptr;
    Panel *base = Panel_0();
    if (!base) {
        Memory_free(k);
        return nullptr;
    }
    (*k).base = (*base);
    Memory_free(base);
    (*k).keys = nullptr;
    (*k).bg = 0xFFF2F2F2u;
    (*k).fg = 0xFF1A1A1Au;
    return k;
}

Kbd *Kbd_1(const char *keys) {
    Kbd *k = Kbd_0();
    if (k)
        Kbd_setKeys(k, keys);
    return k;
}

Kbd *Kbd_2(Panel *parent, const char *keys) {
    Kbd *k = Kbd_1(keys);
    if (k && parent)
        Panel_addContainer(parent, &(*k).base);
    return k;
}

// CORE FUNCTIONS

// (none: pure display node)

// SETTERS

static void markDirty(Kbd *k) {
    if (!k)
        return;
    Panel *b = &(*k).base;
    Container_markDirty(&(*b).base);
}

void Kbd_setKeys(Kbd *k, const char *keys) {
    if (!k)
        return;
    if ((*k).keys)
        Memory_free((*k).keys);
    (*k).keys = nullptr;
    if (keys) {
        size_t len = strlen(keys) + 1;
        (*k).keys = (char*) Memory_alloc(TYPE_ARRAY, len);
        if ((*k).keys)
            strcpy((*k).keys, keys);
    }
    markDirty(k);
}

void Kbd_setBg(Kbd *k, uint32_t color) {
    if (!k)
        return;
    (*k).bg = color;
    markDirty(k);
}

void Kbd_setFg(Kbd *k, uint32_t color) {
    if (!k)
        return;
    (*k).fg = color;
    markDirty(k);
}

void Kbd_free(Kbd *k) {
    if (!k)
        return;
    if ((*k).keys)
        Memory_free((*k).keys);
    (*k).keys = nullptr;
    Memory_free(k);
}

// GETTERS

const char *Kbd_getKeys(const Kbd *k) {
    return k ? (*k).keys : nullptr;
}

uint32_t Kbd_getBg(const Kbd *k) {
    return k ? (*k).bg : 0;
}

uint32_t Kbd_getFg(const Kbd *k) {
    return k ? (*k).fg : 0;
}
