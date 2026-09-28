#ifndef DARLING_EFFECT_VISUAL_EFFECT_H
#define DARLING_EFFECT_VISUAL_EFFECT_H

#include <stdbool.h>
#include <stdint.h>

#include "c23/constructor.h"

// darling/effect/visual_effect.h — the blur / material chrome of a Frame (R4).
//
// The frosted-glass layer that sits BEHIND the one seam (the Single-Seam Canvas
// Law: the Frame's hierarchy is NSWindow -> NSVisualEffectView -> CAMetalLayer).
// It is AppKit window chrome, so it belongs to the interface layer (R4) with the
// Frame that owns it — never to the graphics driver (R3).
//
// Decoupled from Window (the Window Decoupling Law): it attaches to a parent
// NSView and owns only its own material view; it holds no window and presents
// nothing.
//
// Materials:
//   VISUAL_EFFECT_MATERIAL_HUD        0 (HUD window / dark frosted glass)
//   VISUAL_EFFECT_MATERIAL_SIDEBAR    1 (macOS sidebar material)
//   VISUAL_EFFECT_MATERIAL_HEADER     2 (titlebar / header material)
//   VISUAL_EFFECT_MATERIAL_FULLSCREEN 3 (fullscreen sheet / overlay)

#define VISUAL_EFFECT_MATERIAL_HUD        0
#define VISUAL_EFFECT_MATERIAL_SIDEBAR    1
#define VISUAL_EFFECT_MATERIAL_HEADER     2
#define VISUAL_EFFECT_MATERIAL_FULLSCREEN 3

typedef struct VisualEffect {
    void *nativeHandle;     // OS view pointer (NSVisualEffectView on macOS)
    void *parentView;       // borrowed parent NSView / container
    float blur;             // alpha / blur strength [0.0f .. 1.0f]
    int material;           // active material preset
    bool vibrant;           // vibrancy toggle
    bool active;            // active state toggle
} VisualEffect;

// --- Constructors (the arity-overloaded chooser idiom) ---
//   VisualEffect()          -> detached
//   VisualEffect(nativeView)-> created and attached to the parent view
VisualEffect *VisualEffect_0(void);
VisualEffect *VisualEffect_1(void *nativeView);

#define VisualEffect(...) CONSTRUCTOR_DISPATCH(VisualEffect, __VA_ARGS__)

// --- Core functions ---
void VisualEffect_free(VisualEffect *vfx);                    // detaches, then frees
bool VisualEffect_attach(VisualEffect *vfx, void *nativeView); // creates the material view
void VisualEffect_detach(VisualEffect *vfx);                  // removes the material view
void *VisualEffect_nativeHandle(const VisualEffect *vfx);     // the borrowable material view

// --- Setters / Getters (the Symmetric Getter/Setter Completeness Law) ---
void  VisualEffect_setBlur(VisualEffect *vfx, float blur);
float VisualEffect_getBlur(const VisualEffect *vfx);
void  VisualEffect_setMaterial(VisualEffect *vfx, int material);
int   VisualEffect_getMaterial(const VisualEffect *vfx);
void  VisualEffect_setVibrancy(VisualEffect *vfx, bool vibrant);
bool  VisualEffect_isVibrant(const VisualEffect *vfx);
void  VisualEffect_setActive(VisualEffect *vfx, bool active);
bool  VisualEffect_isActive(const VisualEffect *vfx);

#endif // DARLING_EFFECT_VISUAL_EFFECT_H
