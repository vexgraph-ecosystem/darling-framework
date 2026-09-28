// tests/visual_effect_test.m — the Frame's blur chrome, on real AppKit.
//
// Creates the material view, attaches it to a parent NSView, exercises the
// material/blur/vibrancy/active setters, and detaches cleanly. No window.

#import <Cocoa/Cocoa.h>

#include "effect/visual_effect.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

int main(void) {
    @autoreleasepool {
        NSView *parent = [[NSView alloc] initWithFrame:NSMakeRect(0.0, 0.0, 100.0, 100.0)];
        assert(parent != nil);

        VisualEffect *vfx = VisualEffect_0();
        assert(vfx != nullptr);
        assert(VisualEffect_getBlur(vfx) == 1.0f);
        assert(VisualEffect_getMaterial(vfx) == VISUAL_EFFECT_MATERIAL_HUD);
        assert(VisualEffect_isActive(vfx));
        assert(!VisualEffect_isVibrant(vfx));
        assert(VisualEffect_nativeHandle(vfx) == nullptr);   // detached

        assert(VisualEffect_attach(vfx, (__bridge void*) parent));
        assert(VisualEffect_nativeHandle(vfx) != nullptr);    // the NSVisualEffectView
        assert([parent.subviews count] == 1);                 // parented behind the seam

        VisualEffect_setBlur(vfx, 0.5f);
        VisualEffect_setMaterial(vfx, VISUAL_EFFECT_MATERIAL_SIDEBAR);
        VisualEffect_setVibrancy(vfx, true);
        VisualEffect_setActive(vfx, false);
        assert(VisualEffect_getBlur(vfx) == 0.5f);
        assert(VisualEffect_getMaterial(vfx) == VISUAL_EFFECT_MATERIAL_SIDEBAR);
        assert(VisualEffect_isVibrant(vfx));
        assert(!VisualEffect_isActive(vfx));

        VisualEffect_detach(vfx);
        assert(VisualEffect_nativeHandle(vfx) == nullptr);
        assert([parent.subviews count] == 0);                 // removed from the hierarchy
        VisualEffect_free(vfx);
    }
    puts("visual effect OK");
    return 0;
}
