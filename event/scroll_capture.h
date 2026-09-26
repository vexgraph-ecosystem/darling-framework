#ifndef DARLING_SCROLL_CAPTURE_H
#define DARLING_SCROLL_CAPTURE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "darling/panel/scroll_panel.h"

#define SCROLL_CAPTURE_WHEEL_IDLE_MS_DEFAULT      120u
#define SCROLL_CAPTURE_MOMENTUM_GRACE_MS_DEFAULT   80u

#define SCROLL_CAPTURE_IDLE             0
#define SCROLL_CAPTURE_DIRECT           1
#define SCROLL_CAPTURE_MOMENTUM_GRACE   2
#define SCROLL_CAPTURE_NATIVE_MOMENTUM  3
#define SCROLL_CAPTURE_WHEEL_BURST      4

typedef struct ScrollCapture {
    ScrollPanel *owner;          // borrowed captured panel
    int state;                   // SCROLL_CAPTURE_* lifecycle state
    uint64_t lastEventMs;        // caller-supplied monotonic clock
    uint64_t wheelIdleMs;        // phase-less burst timeout
    uint64_t momentumGraceMs;    // direct-end native handoff grace
} ScrollCapture;

ScrollCapture *ScrollCapture_0(void);

void ScrollCapture_directBegin(ScrollCapture *self, ScrollPanel *const *candidates,
                               size_t candidateCount, float dx, float dy, uint64_t nowMs);
void ScrollCapture_directChange(ScrollCapture *self, float dx, float dy, uint64_t nowMs);
void ScrollCapture_directEnd(ScrollCapture *self, uint64_t nowMs);
void ScrollCapture_nativeMomentumBegin(ScrollCapture *self, uint64_t nowMs);
void ScrollCapture_nativeMomentumChange(ScrollCapture *self, float dx, float dy, uint64_t nowMs);
void ScrollCapture_nativeMomentumEnd(ScrollCapture *self, uint64_t nowMs);
void ScrollCapture_wheel(ScrollCapture *self, ScrollPanel *const *candidates,
                         size_t candidateCount, float dx, float dy, uint64_t nowMs);
void ScrollCapture_tick(ScrollCapture *self, uint64_t nowMs);
void ScrollCapture_cancel(ScrollCapture *self);

void ScrollCapture_setOwner(ScrollCapture *self, ScrollPanel *owner);
void ScrollCapture_setState(ScrollCapture *self, int state);
void ScrollCapture_setLastEventMs(ScrollCapture *self, uint64_t nowMs);
void ScrollCapture_setWheelIdleMs(ScrollCapture *self, uint64_t timeoutMs);
void ScrollCapture_setMomentumGraceMs(ScrollCapture *self, uint64_t graceMs);
ScrollPanel *ScrollCapture_getOwner(const ScrollCapture *self);
int ScrollCapture_getState(const ScrollCapture *self);
uint64_t ScrollCapture_getLastEventMs(const ScrollCapture *self);
uint64_t ScrollCapture_getWheelIdleMs(const ScrollCapture *self);
uint64_t ScrollCapture_getMomentumGraceMs(const ScrollCapture *self);
bool ScrollCapture_isActive(const ScrollCapture *self);

void ScrollCapture_toString(const ScrollCapture *self, char *dest, size_t cap, bool *outTruncated);
void ScrollCapture_toStringStruct(const ScrollCapture *self, char *dest, size_t cap, bool *outTruncated);

#endif
