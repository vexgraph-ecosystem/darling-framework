#include "darling/panel/section_container.h"

#include "darling/panel/panel.h"
#include "annotation/incomplete.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include "nio/mem.h"
#include "oop/type.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: SectionContainer
 * ============================================================================
 * Container whose children are sections with exactly one current index:
 * only the current section is live (attached with a surface), and hidden
 * sections detach — zero layers, zero surfaces (hidden = zero). Children
 * are ordinary Panels in the embedded base's child list; next/prev advance
 * the index with optional wrap-around, and an optional change callback
 * (borrowed ctx) fires on selection change. Full show/hide of section
 * children is deferred (;;INCOMPLETE) — the shell clamps/wraps the index
 * only. Per the Container-vs-Panel Law it embeds Panel as its first member
 * and inherits the layout/tree/background state; constructors cover
 * detached and parent-attached forms.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: SectionContainer (embeds Panel)
 * LEVEL: L2 — Behavior (section container)
 * ============================================================================
 * Container whose children are sections with exactly one current index.
 * Only the current section is live (attached with a surface). Hidden
 * sections detach — zero layers, zero surfaces (Rule: hidden = zero).
 *
 * Layer accounting:
 *   active section .......... visible child layers ONLY
 *   hidden sections ......... 0 layers, 0 surfaces (detached)
 *
 * STRUCT FIELDS (Mirroring darling/panel/sectioncontainer.h):
 * ----------------------------------------------------------------------------
 *   Panel base;                        // Inherited layout/tree/background state
 *   int32_t current;                   // Current section index
 *   bool wrapAround;                   // True = next/prev wraps at the ends
 *   void (*onSectionChange)(void *ctx);// Change callback; nullptr = none
 *   void *ctx;                         // Callback context
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - SectionContainer_0(void)
 *   - SectionContainer_1(parent)
 *
 * Core Functions:
 *   - SectionContainer_next(s)
 *   - SectionContainer_prev(s)
 *
 * Setters:
 *   - SectionContainer_setCurrent(s, index)
 *   - SectionContainer_setWrapAround(s, wrap)
 *
 * Getters:
 *   - SectionContainer_getCurrent(s)
 *   - SectionContainer_getCount(s)
 *   - SectionContainer_getWrapAround(s)
 * ============================================================================
 */

// CONSTRUCTORS
// ============================================================================

SectionContainer *SectionContainer_0(void) {
    SectionContainer *s = (SectionContainer*) Memory_alloc(TYPE_SECTION_CONTAINER_SINGLETON, sizeof(SectionContainer));
    if (!s)
        return nullptr;
    Panel *b = Panel_0();
    if (!b) {
        Memory_free(s);
        return nullptr;
    }
    (*s).base = (*b);
    Memory_free(b);
    (*s).current = 0;
    (*s).wrapAround = false;
    (*s).onSectionChange = nullptr;
    (*s).ctx = nullptr;
    return s;
}

SectionContainer *SectionContainer_1(Panel *parent) {
    SectionContainer *s = SectionContainer_0();
    if (s && parent) {
        Panel *b = &(*s).base;
        Panel_addContainer(parent, b);
    }
    return s;
}

// CORE FUNCTIONS
// ============================================================================

void SectionContainer_next(SectionContainer *s) {
    ;;INCOMPLETE // full show/hide of section children deferred
    if (!s)
        return;
    SectionContainer_setCurrent(s, (*s).current + 1);
}

void SectionContainer_prev(SectionContainer *s) {
    ;;INCOMPLETE // full show/hide of section children deferred
    if (!s)
        return;
    SectionContainer_setCurrent(s, (*s).current - 1);
}

// SETTERS
// ============================================================================

static void markDirty(SectionContainer *s) {
    if (!s)
        return;
    Panel *b = &(*s).base;
    Container *c = &(*b).base;
    Container_markDirty(c);
}

void SectionContainer_setCurrent(SectionContainer *s, int32_t index) {
    if (!s)
        return;
    Panel *b = &(*s).base;
    size_t n = Panel_childCount(b);
    if (n == 0) {
        (*s).current = 0;
        markDirty(s);
        return;
    }
    int32_t count = (int32_t) n;
    if ((*s).wrapAround) {
        int32_t m = index % count;
        if (m < 0)
            m += count;
        (*s).current = m;
    } else {
        if (index < 0)
            index = 0;
        if (index >= count)
            index = count - 1;
        (*s).current = index;
    }
    markDirty(s);
}

void SectionContainer_setWrapAround(SectionContainer *s, bool wrap) {
    if (!s)
        return;
    (*s).wrapAround = wrap;
    markDirty(s);
}

// GETTERS
// ============================================================================

int32_t SectionContainer_getCurrent(const SectionContainer *s) {
    return s ? (*s).current : 0;
}

int32_t SectionContainer_getCount(const SectionContainer *s) {
    if (!s)
        return 0;
    const Panel *b = &(*s).base;
    return (int32_t) Panel_childCount(b);
}

bool SectionContainer_getWrapAround(const SectionContainer *s) {
    return s ? (*s).wrapAround : false;
}