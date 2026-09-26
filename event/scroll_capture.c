#include "event/scroll_capture.h"

#include "annotation/definition.h"
#include "annotation/getter.h"
#include "annotation/overview.h"
#include "annotation/setter.h"
#include "lang/str.h"
#include "nio/mem.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: ScrollCapture
 * ============================================================================
 * R4's allocation-free scroll ownership coordinator. A caller supplies a
 * transient deepest-to-ancestor candidate span only at acquisition. The class
 * borrows exactly one ScrollPanel from acquisition through direct contact and
 * native momentum; later candidate or cursor changes cannot retarget it.
 * Phase-less wheel packets retain the same owner until a runtime-configurable
 * idle timeout. ScrollPanel remains the sole owner of offset physics.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: ScrollCapture
 * LEVEL: L2 — Behavior (borrowed scroll-gesture ownership)
 * ============================================================================
 * STRUCT FIELDS (Mirroring event/scroll_capture.h):
 *   ScrollPanel *owner;       // borrowed captured panel
 *   int state;                // SCROLL_CAPTURE_* lifecycle state
 *   uint64_t lastEventMs;     // caller monotonic clock
 *   uint64_t wheelIdleMs;     // phase-less burst timeout
 *   uint64_t momentumGraceMs; // native momentum handoff grace
 *
 * FUNCTION REGISTRY:
 * Public Constructors: ScrollCapture_0
 * Public Core: directBegin/directChange/directEnd, nativeMomentumBegin/
 *   nativeMomentumChange/nativeMomentumEnd, wheel, tick, cancel
 * Private Core: acquireOwner, finish
 * Public Setters: setOwner/setState/setLastEventMs/setWheelIdleMs/setMomentumGraceMs
 * Public Getters: getOwner/getState/getLastEventMs/getWheelIdleMs/
 *   getMomentumGraceMs/isActive
 * Public toString: toString/toStringStruct
 * ============================================================================
 */

// CONSTRUCTORS (PUBLIC & PRIVATE)

ScrollCapture *ScrollCapture_0(void) {
    ScrollCapture *self = (ScrollCapture*) Memory_alloc(TYPE_SCROLL_CAPTURE_SINGLETON,
                                                         sizeof(ScrollCapture));
    if (!self)
        return nullptr;
    (*self).owner = nullptr;
    (*self).state = SCROLL_CAPTURE_IDLE;
    (*self).lastEventMs = 0u;
    (*self).wheelIdleMs = SCROLL_CAPTURE_WHEEL_IDLE_MS_DEFAULT;
    (*self).momentumGraceMs = SCROLL_CAPTURE_MOMENTUM_GRACE_MS_DEFAULT;
    return self;
}

// CORE FUNCTIONS (PUBLIC & PRIVATE)

static ScrollPanel *acquireOwner(ScrollPanel *const *candidates, size_t candidateCount,
                                 float dx, float dy) {
    if (!candidates)
        return nullptr;
    for (size_t i = 0; i < candidateCount; i++) {
        ScrollPanel *candidate = candidates[i];
        if (ScrollPanel_canAcquire(candidate, dx, dy))
            return candidate;
    }
    return nullptr;
}

static void finish(ScrollCapture *self, bool cancel) {
    if (!self)
        return;
    ScrollPanel *owner = (*self).owner;
    if (owner) {
        if (cancel)
            ScrollPanel_cancelMotion(owner);
        else if ((*self).state == SCROLL_CAPTURE_DIRECT
                 || (*self).state == SCROLL_CAPTURE_WHEEL_BURST)
            ScrollPanel_directEnd(owner, (*self).lastEventMs);
        // MOMENTUM_GRACE already released the panel at contact end, so there is
        // nothing left to release here.
    }
    (*self).owner = nullptr;
    (*self).state = SCROLL_CAPTURE_IDLE;
}

void ScrollCapture_directBegin(ScrollCapture *self, ScrollPanel *const *candidates,
                               size_t candidateCount, float dx, float dy, uint64_t nowMs) {
    if (!self)
        return;
    ScrollCapture_cancel(self);
    (*self).owner = acquireOwner(candidates, candidateCount, dx, dy);
    (*self).lastEventMs = nowMs;
    // A gesture that found no eligible owner is still one gesture. Do not
    // re-acquire on a later packet merely because its direction changed.
    (*self).state = SCROLL_CAPTURE_DIRECT;
    if (!(*self).owner)
        return;
    ScrollPanel_directBegin((*self).owner, nowMs);
}

void ScrollCapture_directChange(ScrollCapture *self, float dx, float dy, uint64_t nowMs) {
    if (!self || (*self).state != SCROLL_CAPTURE_DIRECT || !(*self).owner)
        return;
    (*self).lastEventMs = nowMs;
    ScrollPanel_directChange((*self).owner, dx, dy, nowMs);
}

void ScrollCapture_directEnd(ScrollCapture *self, uint64_t nowMs) {
    if (!self || (*self).state != SCROLL_CAPTURE_DIRECT)
        return;
    (*self).lastEventMs = nowMs;
    if (!(*self).owner) {
        (*self).state = SCROLL_CAPTURE_IDLE;
        return;
    }
    // Fingers up: release the rubber band NOW so the spring starts this frame
    // (no dead beat), but retain the owner so a following momentum stream
    // reuses the same panel.
    ScrollPanel_directEnd((*self).owner, nowMs);
    (*self).state = SCROLL_CAPTURE_MOMENTUM_GRACE;
}

void ScrollCapture_nativeMomentumBegin(ScrollCapture *self, uint64_t nowMs) {
    if (!self || !(*self).owner)
        return;
    if ((*self).state != SCROLL_CAPTURE_DIRECT && (*self).state != SCROLL_CAPTURE_MOMENTUM_GRACE)
        return;
    (*self).lastEventMs = nowMs;
    (*self).state = SCROLL_CAPTURE_NATIVE_MOMENTUM;
    ScrollPanel_nativeMomentumBegin((*self).owner, nowMs);
}

void ScrollCapture_nativeMomentumChange(ScrollCapture *self, float dx, float dy, uint64_t nowMs) {
    if (!self || (*self).state != SCROLL_CAPTURE_NATIVE_MOMENTUM || !(*self).owner)
        return;
    (*self).lastEventMs = nowMs;
    ScrollPanel_nativeMomentumChange((*self).owner, dx, dy, nowMs);
}

void ScrollCapture_nativeMomentumEnd(ScrollCapture *self, uint64_t nowMs) {
    if (!self || (*self).state != SCROLL_CAPTURE_NATIVE_MOMENTUM || !(*self).owner)
        return;
    (*self).lastEventMs = nowMs;
    ScrollPanel_nativeMomentumEnd((*self).owner, nowMs);
    (*self).owner = nullptr;
    (*self).state = SCROLL_CAPTURE_IDLE;
}

void ScrollCapture_wheel(ScrollCapture *self, ScrollPanel *const *candidates,
                          size_t candidateCount, float dx, float dy, uint64_t nowMs) {
    if (!self)
        return;
    // A fresh wheel packet can arrive before the next display tick. Expire
    // the previous burst first so edge selection runs for the new gesture.
    ScrollCapture_tick(self, nowMs);
    if ((*self).state != SCROLL_CAPTURE_WHEEL_BURST) {
        ScrollCapture_cancel(self);
        (*self).owner = acquireOwner(candidates, candidateCount, dx, dy);
        if ((*self).owner)
            ScrollPanel_directBegin((*self).owner, nowMs);
    }
    (*self).lastEventMs = nowMs;
    (*self).state = SCROLL_CAPTURE_WHEEL_BURST;
    if (!(*self).owner)
        return;
    ScrollPanel_directChange((*self).owner, dx, dy, nowMs);
}

void ScrollCapture_tick(ScrollCapture *self, uint64_t nowMs) {
    if (!self || nowMs < (*self).lastEventMs)
        return;
    uint64_t elapsed = nowMs - (*self).lastEventMs;
    if ((*self).state == SCROLL_CAPTURE_WHEEL_BURST && elapsed >= (*self).wheelIdleMs) {
        (*self).lastEventMs = nowMs;
        finish(self, false);
    } else if ((*self).state == SCROLL_CAPTURE_MOMENTUM_GRACE
               && elapsed >= (*self).momentumGraceMs) {
        (*self).lastEventMs = nowMs;
        finish(self, false);
    }
}

void ScrollCapture_cancel(ScrollCapture *self) {
    finish(self, true);
}

// SETTERS (PUBLIC & PRIVATE)

;;SETTER
void ScrollCapture_setOwner(ScrollCapture *self, ScrollPanel *owner) {
    if (self)
        (*self).owner = owner;
}

;;SETTER
void ScrollCapture_setState(ScrollCapture *self, int state) {
    if (self)
        (*self).state = state;
}

;;SETTER
void ScrollCapture_setLastEventMs(ScrollCapture *self, uint64_t nowMs) {
    if (self)
        (*self).lastEventMs = nowMs;
}

;;SETTER
void ScrollCapture_setWheelIdleMs(ScrollCapture *self, uint64_t timeoutMs) {
    if (self)
        (*self).wheelIdleMs = timeoutMs;
}

;;SETTER
void ScrollCapture_setMomentumGraceMs(ScrollCapture *self, uint64_t graceMs) {
    if (self)
        (*self).momentumGraceMs = graceMs;
}

// GETTERS (PUBLIC & PRIVATE)

;;GETTER
ScrollPanel *ScrollCapture_getOwner(const ScrollCapture *self) {
    return self ? (*self).owner : nullptr;
}

;;GETTER
int ScrollCapture_getState(const ScrollCapture *self) {
    return self ? (*self).state : SCROLL_CAPTURE_IDLE;
}

;;GETTER
uint64_t ScrollCapture_getLastEventMs(const ScrollCapture *self) {
    return self ? (*self).lastEventMs : 0u;
}

;;GETTER
uint64_t ScrollCapture_getWheelIdleMs(const ScrollCapture *self) {
    return self ? (*self).wheelIdleMs : 0u;
}

;;GETTER
uint64_t ScrollCapture_getMomentumGraceMs(const ScrollCapture *self) {
    return self ? (*self).momentumGraceMs : 0u;
}

;;GETTER
bool ScrollCapture_isActive(const ScrollCapture *self) {
    return self ? (*self).owner != nullptr : false;
}

void ScrollCapture_toString(const ScrollCapture *self, char *dest, size_t cap, bool *outTruncated) {
    Str s;
    Str_init(&s, dest, cap);
    if (!self) {
        Str_put(&s, "nullptr");
    } else {
        Str_printf(&s, "ScrollCapture[state=%d active=%s wheelIdleMs=%llu graceMs=%llu]",
            (*self).state, (*self).owner ? "true" : "false",
            (unsigned long long) (*self).wheelIdleMs,
            (unsigned long long) (*self).momentumGraceMs);
    }
    if (outTruncated)
        *outTruncated = Str_isTruncated(&s);
}

void ScrollCapture_toStringStruct(const ScrollCapture *self, char *dest, size_t cap,
                                  bool *outTruncated) {
    ScrollCapture_toString(self, dest, cap, outTruncated);
}
