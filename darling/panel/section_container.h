#ifndef DARLING_SECTIONCONTAINER_H
#define DARLING_SECTIONCONTAINER_H

#include <stdbool.h>
#include <stdint.h>

#include "c23/constructor.h"
#include "darling/panel/panel.h"
#include "oop/type.h"

// darling/panel/sectioncontainer.h — section container
// (a Container whose children are sections; exactly one is live.
//  Hidden sections detach — zero layers, zero surfaces.)

// Central registry owns these once landed; the guard keeps this shell
// compiling standalone until then.


typedef struct SectionContainer {
    Panel base;
    int32_t current;
    bool wrapAround;
    void (*onSectionChange)(void *ctx);
    void *ctx;
} SectionContainer;

// Constructors:
//   SectionContainer()         — detached container, section 0
//   SectionContainer(parent)   — created and attached
SectionContainer *SectionContainer_0(void);
SectionContainer *SectionContainer_1(Panel *parent);

#define SectionContainer(...) CONSTRUCTOR_DISPATCH(SectionContainer, __VA_ARGS__)

// Selection advance (shell: clamps/wraps the index; full show/hide deferred).
void SectionContainer_next(SectionContainer *s);
void SectionContainer_prev(SectionContainer *s);

int32_t SectionContainer_getCurrent(const SectionContainer *s);
void SectionContainer_setCurrent(SectionContainer *s, int32_t index);
int32_t SectionContainer_getCount(const SectionContainer *s);
bool SectionContainer_getWrapAround(const SectionContainer *s);
void SectionContainer_setWrapAround(SectionContainer *s, bool wrap);

#endif