#include "darling/anim/anim.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "annotation/definition.h"
#include "annotation/overview.h"
#include "../../c23/darling-type.h"
#include "darling/button/button.h"
#include "darling/label/label.h"
#include "darling/panel/panel.h"
#include "nio/mem.h"
#include "oop/type.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Anim
 * ============================================================================
 * Keyframed 2D animation data plus a Thread-0 player: five per-section key
 * lists (location, size, scale, font size, alpha), each key stamped with a
 * timestamp and one flat easing token (ANIM_EASE_OUT, ANIM_BOUNCE_OUT, ...).
 * An Anim is data, not a node — playback binds a BORROWED Anim to a Container
 * via Container_animate and advances on Thread 0 through Anim_tick(dt) next
 * to layout, so one pop preset animates fifty toasts and Anim_free is called
 * only when the preset is retired. Easing follows the sketch-as-spec law: IN
 * launches, OUT lands, IN_OUT is the merged continuous hill, EXPO applies
 * pow(x,2) twice. Key lists grow on the heap at edit time; tick playback
 * advances elapsed with zero steady-state allocation.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Anim
 * LEVEL: L2 — Behavior (keyframed 2D animation data + Thread-0 player)
 * ============================================================================
 * An Anim is data, not a node: five per-section key lists (location, size,
 * scale, font size, alpha), each key stamped with a timestamp and one flat
 * easing token (ANIM_EASE_OUT, ANIM_BOUNCE_OUT, ...). Playback binds a
 * BORROWED Anim to a Container via ContainerClass_animate and advances on
 * Thread 0 through Anim_tick(dt), next to layout.
 *
 * Easing law (the sketch is the spec): IN launches, OUT lands, IN_OUT is
 * the merged hill (IN below the midpoint, OUT above, continuous). EXPO is
 * EASE squared — it literally applies pow(x,2) twice — hence steeper.
 *
 * STRUCT FIELDS (Mirroring darling/anim/anim.h):
 * ----------------------------------------------------------------------------
 *   AnimLocKey   *loc;    size_t locCount, locCap;    // x/y targets
 *   AnimSizeKey  *size;   size_t sizeCount, sizeCap;  // w/h targets
 *   AnimScaleKey *scale;  size_t scaleCount, scaleCap;// sx/sy targets
 *   AnimFontKey  *font;   size_t fontCount, fontCap;  // point-size targets
 *   AnimAlphaKey *alpha;  size_t alphaCount, alphaCap;// bg-channel 0..1
 *   bool loop;            // true wraps elapsed past duration
 *   Anim_DoneFn onDone;   // fired once when a non-loop binding finishes
 *   void *ctx;            // completion context
 *
 * Player binding (private): { target, borrowed anim, kind, elapsed,
 * captured from-values }. The first key of each section interpolates FROM
 * the value captured at play time, so one preset is reusable.
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Anim_0(void)
 *
 * Core Functions:
 *   - Anim_free(a)
 *   - Anim_addLocation/Size/Scale/FontSize/Alpha(a, t, ..., ease)
 *   - Anim_eval(ease, u)
 *   - Anim_duration(a)
 *   - Anim_keyCount(a, section)
 *   - Anim_play(c, a, kind)
 *   - Anim_tick(dt)
 *   - Anim_cancel(c)
 *   - Anim_cancelAll()
 *   - Anim_isPlaying(c)
 *   - Anim_liveCount()
 *   - Container_animate + Panel/Label/Button/..._animate facades
 *
 * Setters:
 *   - Anim_setLoop(a, loop)
 *   - Anim_setOnDone(a, fn, ctx)
 *
 * Getters:
 *   - Anim_isLoop(a)
 *   - Anim_getOnDone(a)
 *   - Anim_getDoneContext(a)
 * ============================================================================
 */

#ifndef ANIM_PI
#define ANIM_PI 3.14159265358979323846f
#endif

// --- easing evaluator -------------------------------------------------------

static float animEaseOutElastic(float u) {
    if (u <= 0.0f)
        return 0.0f;
    if (u >= 1.0f)
        return 1.0f;
    const float c4 = (2.0f * ANIM_PI) / 3.0f;
    return powf(2.0f, -10.0f * u) * sinf((u * 10.0f - 0.75f) * c4) + 1.0f;
}

static float animEaseInElastic(float u) {
    return 1.0f - animEaseOutElastic(1.0f - u);
}

static float animEaseInOutElastic(float u) {
    if (u <= 0.0f)
        return 0.0f;
    if (u >= 1.0f)
        return 1.0f;
    const float c5 = (2.0f * ANIM_PI) / 4.5f;
    if (u < 0.5f)
        return -(powf(2.0f, 20.0f * u - 10.0f) * sinf((20.0f * u - 11.125f) * c5)) / 2.0f;
    return (powf(2.0f, -20.0f * u + 10.0f) * sinf((20.0f * u - 11.125f) * c5)) / 2.0f + 1.0f;
}

static float animEaseOutBounce(float u) {
    const float n1 = 7.5625f;
    const float d1 = 2.75f;
    if (u < 1.0f / d1)
        return n1 * u * u;
    if (u < 2.0f / d1) {
        float v = u - 1.5f / d1;
        return n1 * v * v + 0.75f;
    }
    if (u < 2.5f / d1) {
        float v = u - 2.25f / d1;
        return n1 * v * v + 0.9375f;
    }
    float v = u - 2.625f / d1;
    return n1 * v * v + 0.984375f;
}

static float animEaseInBounce(float u) {
    return 1.0f - animEaseOutBounce(1.0f - u);
}

static float animEaseInOutBounce(float u) {
    if (u < 0.5f)
        return (1.0f - animEaseOutBounce(1.0f - 2.0f * u)) / 2.0f;
    return (1.0f + animEaseOutBounce(2.0f * u - 1.0f)) / 2.0f;
}

float Anim_eval(AnimEase ease, float u) {
    if (u <= 0.0f)
        return 0.0f;
    if (u >= 1.0f)
        return 1.0f;
    float v = 1.0f - u;
    switch (ease) {
        case ANIM_NORMAL:
            return u;
        case ANIM_EASE_IN:
            return u * u;
        case ANIM_EASE_OUT:
            return 1.0f - v * v;
        case ANIM_EASE_IN_OUT:
            return u < 0.5f ? 2.0f * u * u : 1.0f - 2.0f * v * v;
        case ANIM_SINE_IN:
            return 1.0f - cosf(u * ANIM_PI / 2.0f);
        case ANIM_SINE_OUT:
            return sinf(u * ANIM_PI / 2.0f);
        case ANIM_SINE_IN_OUT:
            return (1.0f - cosf(u * ANIM_PI)) / 2.0f;
        case ANIM_EXPONENTIAL_IN: {
            float q = u * u; // pow(x,2) applied twice: steeper than EASE
            return q * q;
        }
        case ANIM_EXPONENTIAL_OUT: {
            float q = v * v;
            return 1.0f - q * q;
        }
        case ANIM_EXPONENTIAL_IN_OUT:
            if (u < 0.5f) {
                float q = 2.0f * u;
                q = q * q;
                return (q * q) / 2.0f;
            } {
                float q = 2.0f * v;
                q = q * q;
                return 1.0f - (q * q) / 2.0f;
            }
        case ANIM_ELASTIC_IN:
            return animEaseInElastic(u);
        case ANIM_ELASTIC_OUT:
            return animEaseOutElastic(u);
        case ANIM_ELASTIC_IN_OUT:
            return animEaseInOutElastic(u);
        case ANIM_BOUNCE_IN:
            return animEaseInBounce(u);
        case ANIM_BOUNCE_OUT:
            return animEaseOutBounce(u);
        case ANIM_BOUNCE_IN_OUT:
            return animEaseInOutBounce(u);
        default:
            return u;
    }
}

// --- constructors -----------------------------------------------------------

Anim *Anim_0(void) {
    Anim *a = (Anim *)Memory_alloc(TYPE_ANIM_SINGLETON, sizeof(Anim));
    if (!a)
        return nullptr;
    memset(a, 0, sizeof(Anim));
    return a;
}

void Anim_free(Anim *a) {
    if (!a)
        return;
    free((*a).loc);
    free((*a).size);
    free((*a).scale);
    free((*a).font);
    free((*a).alpha);
    Memory_free(a);
}

// --- key insertion (sorted by t) --------------------------------------------

#define ANIM_GROW(keys, count, cap, KeyT)                                            \
    do {                                                                             \
        if ((count) + 1 > (cap)) {                                                   \
            size_t ncap = (cap) == 0 ? 4 : (cap) * 2;                                \
            KeyT *n = (KeyT *)realloc((keys), ncap * sizeof(KeyT));                   \
            if (!n)                                                                  \
                return;                                                              \
            (keys) = n;                                                              \
            (cap) = ncap;                                                            \
        }                                                                            \
    } while (0)

void Anim_addLocation(Anim *a, float t, float x, float y, AnimEase ease) {
    if (!a)
        return;
    ANIM_GROW((*a).loc, (*a).locCount, (*a).locCap, AnimLocKey);
    size_t i = (*a).locCount;
    while (i > 0 && (*a).loc[i - 1].t > t) {
        (*a).loc[i] = (*a).loc[i - 1];
        i--;
    }
    (*a).loc[i].t = t;
    (*a).loc[i].x = x;
    (*a).loc[i].y = y;
    (*a).loc[i].ease = ease;
    (*a).locCount++;
}

void Anim_addSize(Anim *a, float t, float w, float h, AnimEase ease) {
    if (!a)
        return;
    ANIM_GROW((*a).size, (*a).sizeCount, (*a).sizeCap, AnimSizeKey);
    size_t i = (*a).sizeCount;
    while (i > 0 && (*a).size[i - 1].t > t) {
        (*a).size[i] = (*a).size[i - 1];
        i--;
    }
    (*a).size[i].t = t;
    (*a).size[i].w = w;
    (*a).size[i].h = h;
    (*a).size[i].ease = ease;
    (*a).sizeCount++;
}

void Anim_addScale(Anim *a, float t, float sx, float sy, AnimEase ease) {
    if (!a)
        return;
    ANIM_GROW((*a).scale, (*a).scaleCount, (*a).scaleCap, AnimScaleKey);
    size_t i = (*a).scaleCount;
    while (i > 0 && (*a).scale[i - 1].t > t) {
        (*a).scale[i] = (*a).scale[i - 1];
        i--;
    }
    (*a).scale[i].t = t;
    (*a).scale[i].sx = sx;
    (*a).scale[i].sy = sy;
    (*a).scale[i].ease = ease;
    (*a).scaleCount++;
}

void Anim_addFontSize(Anim *a, float t, float size, AnimEase ease) {
    if (!a)
        return;
    ANIM_GROW((*a).font, (*a).fontCount, (*a).fontCap, AnimFontKey);
    size_t i = (*a).fontCount;
    while (i > 0 && (*a).font[i - 1].t > t) {
        (*a).font[i] = (*a).font[i - 1];
        i--;
    }
    (*a).font[i].t = t;
    (*a).font[i].size = size;
    (*a).font[i].ease = ease;
    (*a).fontCount++;
}

void Anim_addAlpha(Anim *a, float t, float alpha, AnimEase ease) {
    if (!a)
        return;
    if (alpha < 0.0f)
        alpha = 0.0f;
    if (alpha > 1.0f)
        alpha = 1.0f;
    ANIM_GROW((*a).alpha, (*a).alphaCount, (*a).alphaCap, AnimAlphaKey);
    size_t i = (*a).alphaCount;
    while (i > 0 && (*a).alpha[i - 1].t > t) {
        (*a).alpha[i] = (*a).alpha[i - 1];
        i--;
    }
    (*a).alpha[i].t = t;
    (*a).alpha[i].alpha = alpha;
    (*a).alpha[i].ease = ease;
    (*a).alphaCount++;
}

// --- queries ----------------------------------------------------------------

float Anim_duration(const Anim *a) {
    if (!a)
        return 0.0f;
    float dur = 0.0f;
    if ((*a).locCount > 0 && (*a).loc[(*a).locCount - 1].t > dur)
        dur = (*a).loc[(*a).locCount - 1].t;
    if ((*a).sizeCount > 0 && (*a).size[(*a).sizeCount - 1].t > dur)
        dur = (*a).size[(*a).sizeCount - 1].t;
    if ((*a).scaleCount > 0 && (*a).scale[(*a).scaleCount - 1].t > dur)
        dur = (*a).scale[(*a).scaleCount - 1].t;
    if ((*a).fontCount > 0 && (*a).font[(*a).fontCount - 1].t > dur)
        dur = (*a).font[(*a).fontCount - 1].t;
    if ((*a).alphaCount > 0 && (*a).alpha[(*a).alphaCount - 1].t > dur)
        dur = (*a).alpha[(*a).alphaCount - 1].t;
    return dur;
}

size_t Anim_keyCount(const Anim *a, int section) {
    if (!a)
        return 0;
    switch (section) {
        case ANIM_SECTION_LOCATION: return (*a).locCount;
        case ANIM_SECTION_SIZE: return (*a).sizeCount;
        case ANIM_SECTION_SCALE: return (*a).scaleCount;
        case ANIM_SECTION_FONT_SIZE: return (*a).fontCount;
        case ANIM_SECTION_ALPHA: return (*a).alphaCount;
        default: return 0;
    }
}

void Anim_setLoop(Anim *a, bool loop) {
    if (a)
        (*a).loop = loop;
}

bool Anim_isLoop(const Anim *a) {
    return a && (*a).loop;
}

void Anim_setOnDone(Anim *a, Anim_DoneFn fn, void *ctx) {
    if (!a)
        return;
    (*a).onDone = fn;
    (*a).ctx = ctx;
}

Anim_DoneFn Anim_getOnDone(const Anim *a) {
    return a ? (*a).onDone : nullptr;
}

void *Anim_getDoneContext(const Anim *a) {
    return a ? (*a).ctx : nullptr;
}

// --- player -----------------------------------------------------------------

typedef struct AnimBinding {
    Container *c;
    Anim *a;
    int kind;
    double elapsed;
    float fromX, fromY;
    float fromW, fromH;
    float fromSX, fromSY;
    float fromFont;
    float fromAlpha;
} AnimBinding;

static AnimBinding *s_bindings = nullptr;
static size_t s_bindingCount = 0;
static size_t s_bindingCap = 0;

static float clamp01(float v) {
    if (v < 0.0f)
        return 0.0f;
    if (v > 1.0f)
        return 1.0f;
    return v;
}

static float segU(float t, float t0, float t1) {
    if (t <= t0)
        return 0.0f;
    if (t >= t1)
        return 1.0f;
    float span = t1 - t0;
    if (span <= 0.0f)
        return 1.0f;
    return (t - t0) / span;
}

// Each sampler: from-value captured at play, absolute key targets after.
static float sampleLocX(const Anim *a, float from, float t) {
    if ((*a).locCount == 0)
        return from;
    if (t <= (*a).loc[0].t)
        return from + ((*a).loc[0].x - from) * Anim_eval((*a).loc[0].ease, segU(t, 0.0f, (*a).loc[0].t));
    for (size_t i = 0; i + 1 < (*a).locCount; i++) {
        if (t <= (*a).loc[i + 1].t)
            return (*a).loc[i].x
                + ((*a).loc[i + 1].x - (*a).loc[i].x)
                    * Anim_eval((*a).loc[i + 1].ease, segU(t, (*a).loc[i].t, (*a).loc[i + 1].t));
    }
    return (*a).loc[(*a).locCount - 1].x;
}

static float sampleLocY(const Anim *a, float from, float t) {
    if ((*a).locCount == 0)
        return from;
    if (t <= (*a).loc[0].t)
        return from + ((*a).loc[0].y - from) * Anim_eval((*a).loc[0].ease, segU(t, 0.0f, (*a).loc[0].t));
    for (size_t i = 0; i + 1 < (*a).locCount; i++) {
        if (t <= (*a).loc[i + 1].t)
            return (*a).loc[i].y
                + ((*a).loc[i + 1].y - (*a).loc[i].y)
                    * Anim_eval((*a).loc[i + 1].ease, segU(t, (*a).loc[i].t, (*a).loc[i + 1].t));
    }
    return (*a).loc[(*a).locCount - 1].y;
}

static float sampleSizeW(const Anim *a, float from, float t) {
    if ((*a).sizeCount == 0)
        return from;
    if (t <= (*a).size[0].t)
        return from + ((*a).size[0].w - from) * Anim_eval((*a).size[0].ease, segU(t, 0.0f, (*a).size[0].t));
    for (size_t i = 0; i + 1 < (*a).sizeCount; i++) {
        if (t <= (*a).size[i + 1].t)
            return (*a).size[i].w
                + ((*a).size[i + 1].w - (*a).size[i].w)
                    * Anim_eval((*a).size[i + 1].ease, segU(t, (*a).size[i].t, (*a).size[i + 1].t));
    }
    return (*a).size[(*a).sizeCount - 1].w;
}

static float sampleSizeH(const Anim *a, float from, float t) {
    if ((*a).sizeCount == 0)
        return from;
    if (t <= (*a).size[0].t)
        return from + ((*a).size[0].h - from) * Anim_eval((*a).size[0].ease, segU(t, 0.0f, (*a).size[0].t));
    for (size_t i = 0; i + 1 < (*a).sizeCount; i++) {
        if (t <= (*a).size[i + 1].t)
            return (*a).size[i].h
                + ((*a).size[i + 1].h - (*a).size[i].h)
                    * Anim_eval((*a).size[i + 1].ease, segU(t, (*a).size[i].t, (*a).size[i + 1].t));
    }
    return (*a).size[(*a).sizeCount - 1].h;
}

static float sampleScaleX(const Anim *a, float from, float t) {
    if ((*a).scaleCount == 0)
        return from;
    if (t <= (*a).scale[0].t)
        return from + ((*a).scale[0].sx - from) * Anim_eval((*a).scale[0].ease, segU(t, 0.0f, (*a).scale[0].t));
    for (size_t i = 0; i + 1 < (*a).scaleCount; i++) {
        if (t <= (*a).scale[i + 1].t)
            return (*a).scale[i].sx
                + ((*a).scale[i + 1].sx - (*a).scale[i].sx)
                    * Anim_eval((*a).scale[i + 1].ease, segU(t, (*a).scale[i].t, (*a).scale[i + 1].t));
    }
    return (*a).scale[(*a).scaleCount - 1].sx;
}

static float sampleScaleY(const Anim *a, float from, float t) {
    if ((*a).scaleCount == 0)
        return from;
    if (t <= (*a).scale[0].t)
        return from + ((*a).scale[0].sy - from) * Anim_eval((*a).scale[0].ease, segU(t, 0.0f, (*a).scale[0].t));
    for (size_t i = 0; i + 1 < (*a).scaleCount; i++) {
        if (t <= (*a).scale[i + 1].t)
            return (*a).scale[i].sy
                + ((*a).scale[i + 1].sy - (*a).scale[i].sy)
                    * Anim_eval((*a).scale[i + 1].ease, segU(t, (*a).scale[i].t, (*a).scale[i + 1].t));
    }
    return (*a).scale[(*a).scaleCount - 1].sy;
}

static float sampleFont(const Anim *a, float from, float t) {
    if ((*a).fontCount == 0)
        return from;
    float v;
    if (t <= (*a).font[0].t)
        v = from + ((*a).font[0].size - from) * Anim_eval((*a).font[0].ease, segU(t, 0.0f, (*a).font[0].t));
    else {
        v = (*a).font[(*a).fontCount - 1].size;
        for (size_t i = 0; i + 1 < (*a).fontCount; i++) {
            if (t <= (*a).font[i + 1].t) {
                v = (*a).font[i].size
                    + ((*a).font[i + 1].size - (*a).font[i].size)
                        * Anim_eval((*a).font[i + 1].ease, segU(t, (*a).font[i].t, (*a).font[i + 1].t));
                break;
            }
        }
    }
    return v < 0.5f ? 0.5f : v;
}

static float sampleAlpha(const Anim *a, float from, float t) {
    float v;
    if ((*a).alphaCount == 0)
        return from;
    if (t <= (*a).alpha[0].t)
        v = from + ((*a).alpha[0].alpha - from) * Anim_eval((*a).alpha[0].ease, segU(t, 0.0f, (*a).alpha[0].t));
    else {
        v = (*a).alpha[(*a).alphaCount - 1].alpha;
        for (size_t i = 0; i + 1 < (*a).alphaCount; i++) {
            if (t <= (*a).alpha[i + 1].t) {
                v = (*a).alpha[i].alpha
                    + ((*a).alpha[i + 1].alpha - (*a).alpha[i].alpha)
                        * Anim_eval((*a).alpha[i + 1].ease, segU(t, (*a).alpha[i].t, (*a).alpha[i + 1].t));
                break;
            }
        }
    }
    return clamp01(v);
}

// State-safe apply: direct field writes + dirty=1. Public setters are NOT
// used here — they invalidateBase (recapture the resize reference), which
// would make anchored panels jump mid-animation (the subtle law).
static void animApply(AnimBinding *b, float t) {
    Container *c = (*b).c;
    Anim *a = (*b).a;
    (*c).x = sampleLocX(a, (*b).fromX, t);
    (*c).y = sampleLocY(a, (*b).fromY, t);
    (*c).w = sampleSizeW(a, (*b).fromW, t);
    (*c).h = sampleSizeH(a, (*b).fromH, t);
    (*c).scaleX = sampleScaleX(a, (*b).fromSX, t);
    (*c).scaleY = sampleScaleY(a, (*b).fromSY, t);
    (*c).dirty = 1;
    if ((*b).kind == ANIM_KIND_PANEL || (*b).kind == ANIM_KIND_LABEL || (*b).kind == ANIM_KIND_BUTTON) {
        Panel *p = (Panel *)c;
        uint32_t col = (*p).color;
        // darling colors are 0xAARRGGBB: alpha lives in the HIGH byte.
        uint8_t na = (uint8_t)(sampleAlpha(a, (*b).fromAlpha, t) * 255.0f + 0.5f);
        (*p).color = (col & 0x00FFFFFFu) | ((uint32_t)na << 24);
    }
    if ((*b).kind == ANIM_KIND_LABEL) {
        Label *l = (Label *)c;
        (*l).fontSize = sampleFont(a, (*b).fromFont, t);
        (*l).rasterDirty = true;
    } else if ((*b).kind == ANIM_KIND_BUTTON) {
        Button *btn = (Button *)c;
        float s = sampleFont(a, (*b).fromFont, t);
        (*btn).fontSize = s < 0.5f ? 0.5f : s;
    }
}

static int animFind(const Container *c) {
    for (size_t i = 0; i < s_bindingCount; i++) {
        if (s_bindings[i].c == c)
            return (int)i;
    }
    return -1;
}

void Anim_play(Container *c, Anim *a, int kind) {
    if (!c)
        return;
    if (!a) {
        Anim_cancel(c);
        return;
    }
    int idx = animFind(c);
    if (idx < 0) {
        if (s_bindingCount + 1 > s_bindingCap) {
            size_t ncap = s_bindingCap == 0 ? 16 : s_bindingCap * 2;
            AnimBinding *n = (AnimBinding *)realloc(s_bindings, ncap * sizeof(AnimBinding));
            if (!n)
                return;
            s_bindings = n;
            s_bindingCap = ncap;
        }
        idx = (int)s_bindingCount++;
    }
    AnimBinding *b = &s_bindings[idx];
    (*b).c = c;
    (*b).a = a;
    (*b).kind = kind;
    (*b).elapsed = 0.0;
    (*b).fromX = (*c).x;
    (*b).fromY = (*c).y;
    (*b).fromW = (*c).w;
    (*b).fromH = (*c).h;
    (*b).fromSX = (*c).scaleX;
    (*b).fromSY = (*c).scaleY;
    (*b).fromFont = 12.0f;
    (*b).fromAlpha = 1.0f;
    if (kind == ANIM_KIND_LABEL)
        (*b).fromFont = ((const Label *)c)->fontSize;
    else if (kind == ANIM_KIND_BUTTON)
        (*b).fromFont = ((const Button *)c)->fontSize;
    if (kind == ANIM_KIND_PANEL || kind == ANIM_KIND_LABEL || kind == ANIM_KIND_BUTTON)
        (*b).fromAlpha = (float)((((const Panel *)c)->color >> 24) & 0xFFu) / 255.0f;
    float dur = Anim_duration(a);
    if (dur <= 0.0f) {
        animApply(b, 0.0f);
        Anim_DoneFn fn = (*a).onDone;
        void *ctx = (*a).ctx;
        s_bindings[idx] = s_bindings[--s_bindingCount];
        if (fn)
            fn(ctx);
    }
}

void Anim_tick(double dt) {
    if (dt < 0.0)
        dt = 0.0;
    size_t i = 0;
    while (i < s_bindingCount) {
        AnimBinding *b = &s_bindings[i];
        (*b).elapsed += dt;
        float dur = Anim_duration((*b).a);
        if (dur > 0.0f && (*b).elapsed < (double)dur) {
            animApply(b, (float)(*b).elapsed);
            i++;
            continue;
        }
        if ((*b).a->loop && dur > 0.0f) {
            (*b).elapsed = fmod((*b).elapsed, (double)dur);
            animApply(b, (float)(*b).elapsed);
            i++;
            continue;
        }
        animApply(b, dur);
        Anim_DoneFn fn = (*b).a->onDone;
        void *ctx = (*b).a->ctx;
        s_bindings[i] = s_bindings[--s_bindingCount];
        if (fn)
            fn(ctx);
    }
}

void Anim_cancel(Container *c) {
    if (!c)
        return;
    int idx = animFind(c);
    if (idx >= 0)
        s_bindings[idx] = s_bindings[--s_bindingCount];
}

void Anim_cancelAll(void) {
    s_bindingCount = 0;
}

bool Anim_isPlaying(const Container *c) {
    return c && animFind(c) >= 0;
}

size_t Anim_liveCount(void) {
    return s_bindingCount;
}

// --- per-class facades ------------------------------------------------------

void Container_animate(Container *c, Anim *a) {
    if (c)
        Anim_play(c, a, ANIM_KIND_CONTAINER);
}

void Panel_animate(struct Panel *p, Anim *a) {
    if (p)
        Anim_play((Container *)p, a, ANIM_KIND_PANEL);
}

void Label_animate(struct Label *l, Anim *a) {
    if (l)
        Anim_play((Container *)l, a, ANIM_KIND_LABEL);
}

void Button_animate(struct Button *b, Anim *a) {
    if (b)
        Anim_play((Container *)b, a, ANIM_KIND_BUTTON);
}

#define ANIM_PANEL_FACADE(Name, Type)                       \
    void Name##_animate(struct Type *p, Anim *a) {           \
        if (p)                                              \
            Anim_play((Container *)p, a, ANIM_KIND_PANEL);   \
    }

ANIM_PANEL_FACADE(ListContainer, ListContainer)
ANIM_PANEL_FACADE(GridContainer, GridContainer)
ANIM_PANEL_FACADE(ScrollContainer, ScrollContainer)
ANIM_PANEL_FACADE(SectionContainer, SectionContainer)
ANIM_PANEL_FACADE(LayeredContainer, LayeredContainer)
ANIM_PANEL_FACADE(MarkdownPanel, MarkdownPanel)
ANIM_PANEL_FACADE(RichTextPanel, RichTextPanel)
ANIM_PANEL_FACADE(Switch, Switch)
ANIM_PANEL_FACADE(Checkbox, Checkbox)
ANIM_PANEL_FACADE(RadioGroup, RadioGroup)
ANIM_PANEL_FACADE(Slider, Slider)
ANIM_PANEL_FACADE(Knob, Knob)
ANIM_PANEL_FACADE(Input, Input)
ANIM_PANEL_FACADE(Textarea, Textarea)
ANIM_PANEL_FACADE(InputOTP, InputOTP)
ANIM_PANEL_FACADE(Select, Select)
ANIM_PANEL_FACADE(ScrollBar, ScrollBar)
ANIM_PANEL_FACADE(DatePicker, DatePicker)
ANIM_PANEL_FACADE(ColorPicker, ColorPicker)
ANIM_PANEL_FACADE(ColorSwatch, ColorSwatch)
ANIM_PANEL_FACADE(Dialog, Dialog)
ANIM_PANEL_FACADE(AlertDialog, AlertDialog)
ANIM_PANEL_FACADE(ColorDialog, ColorDialog)
ANIM_PANEL_FACADE(FileDialog, FileDialog)
ANIM_PANEL_FACADE(Picture, Picture)
ANIM_PANEL_FACADE(Plot, Plot)
ANIM_PANEL_FACADE(Kbd, Kbd)
ANIM_PANEL_FACADE(RichLabel, RichLabel)
ANIM_PANEL_FACADE(Typography, Typography)
ANIM_PANEL_FACADE(Scene, Scene)
ANIM_PANEL_FACADE(Canvas, Canvas)
