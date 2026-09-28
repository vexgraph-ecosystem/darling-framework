#include "effect/visual_effect.h"

#include <stdlib.h>

#include "annotation/definition.h"
#include "annotation/overview.h"
#include "annotation/getter.h"
#include "annotation/setter.h"

#if defined(__APPLE__)
extern void *VisualEffect_cocoaAttach(void *parentView, int material, float blur, bool vibrant);
extern void  VisualEffect_cocoaDetach(void *nativeHandle);
extern void  VisualEffect_cocoaSetBlur(void *nativeHandle, float blur);
extern void  VisualEffect_cocoaSetMaterial(void *nativeHandle, int material);
extern void  VisualEffect_cocoaSetVibrancy(void *nativeHandle, bool vibrant);
extern void  VisualEffect_cocoaSetActive(void *nativeHandle, bool active);
#endif

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: VisualEffect
 * ============================================================================
 * Hardware-accelerated background blur, frosted glass, and vibrancy — the
 * chrome behind the Frame's one seam. Owns the platform material view
 * (NSVisualEffectView on macOS) and its lifecycle, blur alpha and material.
 * Decoupled from Window per the Window Decoupling Law; it never presents.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: VisualEffect (effect/visual_effect.c)
 * ============================================================================
 * SUMMARY:
 *   Manages the frosted-glass material layer independently of the window. On
 *   macOS it bridges to NSVisualEffectView; elsewhere it degrades to a
 *   transparent placeholder (attach succeeds, no native view).
 *
 * STRUCT FIELDS (mirroring effect/visual_effect.h):
 *   void *nativeHandle;  void *parentView;  float blur;  int material;
 *   bool vibrant;  bool active;
 *
 * FUNCTION REGISTRY:
 *   Public Constructors: VisualEffect_0, VisualEffect_1
 *   Public Core: VisualEffect_free, VisualEffect_attach, VisualEffect_detach,
 *                VisualEffect_nativeHandle
 *   Public Setters: VisualEffect_setBlur, VisualEffect_setMaterial,
 *                   VisualEffect_setVibrancy, VisualEffect_setActive
 *   Public Getters: VisualEffect_getBlur, VisualEffect_getMaterial,
 *                   VisualEffect_isVibrant, VisualEffect_isActive
 * ============================================================================
 */

VisualEffect *VisualEffect_0(void) {
    VisualEffect *vfx = (VisualEffect*) calloc(1, sizeof(VisualEffect));
    if (vfx == nullptr)
        return nullptr;
    (*vfx).blur = 1.0f;
    (*vfx).material = VISUAL_EFFECT_MATERIAL_HUD;
    (*vfx).vibrant = false;
    (*vfx).active = true;
    return vfx;
}

VisualEffect *VisualEffect_1(void *nativeView) {
    VisualEffect *vfx = VisualEffect_0();
    if (vfx == nullptr)
        return nullptr;
    if (nativeView != nullptr)
        VisualEffect_attach(vfx, nativeView);
    return vfx;
}

void VisualEffect_free(VisualEffect *vfx) {
    if (vfx == nullptr)
        return;
    VisualEffect_detach(vfx);
    free(vfx);
}

bool VisualEffect_attach(VisualEffect *vfx, void *nativeView) {
    if (vfx == nullptr || nativeView == nullptr)
        return false;
    if ((*vfx).nativeHandle != nullptr)
        VisualEffect_detach(vfx);
#if defined(__APPLE__)
    (*vfx).parentView = nativeView;
    (*vfx).nativeHandle = VisualEffect_cocoaAttach(nativeView, (*vfx).material, (*vfx).blur, (*vfx).vibrant);
    return (*vfx).nativeHandle != nullptr;
#else
    (*vfx).parentView = nativeView;
    return true;
#endif
}

void VisualEffect_detach(VisualEffect *vfx) {
    if (vfx == nullptr || (*vfx).nativeHandle == nullptr)
        return;
#if defined(__APPLE__)
    VisualEffect_cocoaDetach((*vfx).nativeHandle);
#endif
    (*vfx).nativeHandle = nullptr;
    (*vfx).parentView = nullptr;
}

;;SETTER
void VisualEffect_setBlur(VisualEffect *vfx, float blur) {
    if (vfx == nullptr)
        return;
    (*vfx).blur = blur;
#if defined(__APPLE__)
    if ((*vfx).nativeHandle != nullptr)
        VisualEffect_cocoaSetBlur((*vfx).nativeHandle, blur);
#endif
}

;;SETTER
void VisualEffect_setMaterial(VisualEffect *vfx, int material) {
    if (vfx == nullptr)
        return;
    (*vfx).material = material;
#if defined(__APPLE__)
    if ((*vfx).nativeHandle != nullptr)
        VisualEffect_cocoaSetMaterial((*vfx).nativeHandle, material);
#endif
}

;;SETTER
void VisualEffect_setVibrancy(VisualEffect *vfx, bool vibrant) {
    if (vfx == nullptr)
        return;
    (*vfx).vibrant = vibrant;
#if defined(__APPLE__)
    if ((*vfx).nativeHandle != nullptr)
        VisualEffect_cocoaSetVibrancy((*vfx).nativeHandle, vibrant);
#endif
}

;;SETTER
void VisualEffect_setActive(VisualEffect *vfx, bool active) {
    if (vfx == nullptr)
        return;
    (*vfx).active = active;
#if defined(__APPLE__)
    if ((*vfx).nativeHandle != nullptr)
        VisualEffect_cocoaSetActive((*vfx).nativeHandle, active);
#endif
}

;;GETTER
float VisualEffect_getBlur(const VisualEffect *vfx) {
    return vfx ? (*vfx).blur : 0.0f;
}

;;GETTER
int VisualEffect_getMaterial(const VisualEffect *vfx) {
    return vfx ? (*vfx).material : VISUAL_EFFECT_MATERIAL_HUD;
}

;;GETTER
bool VisualEffect_isVibrant(const VisualEffect *vfx) {
    return vfx ? (*vfx).vibrant : false;
}

;;GETTER
bool VisualEffect_isActive(const VisualEffect *vfx) {
    return vfx ? (*vfx).active : false;
}

;;GETTER
void *VisualEffect_nativeHandle(const VisualEffect *vfx) {
    return vfx ? (*vfx).nativeHandle : nullptr;
}
