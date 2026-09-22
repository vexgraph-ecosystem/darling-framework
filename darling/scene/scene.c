#include "darling/scene/scene.h"

#include "nio/mem.h"
#include "oop/type.h"
#include "annotation/definition.h"
#include "annotation/overview.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Scene
 * ============================================================================
 * The scene root: Panel hierarchy state plus a virtual-size mapping mode,
 * embedding Panel so the scene's virtual size IS its Container w/h — the
 * present pass scales it into whatever the window occupies, so the scene
 * never re-renders on resize. Scene2D and Scene3D are dispatch tags with no
 * extra payload, embedding Scene as their first member. The present mode
 * selects the destination: COMPOSITED keeps a retained offscreen target
 * sampled by the canvas (the Single-Seam Canvas Law — one on-screen
 * CAMetalLayer total), and INLINE draws directly into the parent board render
 * pass. Scene hosts the 2D/3D scene-graph content of the R4 stack.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Scene (embeds Panel; Scene2D/Scene3D embed Scene)
 * LEVEL: L2 — Behavior (UI scene root behavior API)
 * ============================================================================
 * The scene root: Panel hierarchy state plus a virtual-size mapping mode.
 * The scene's virtual size IS its Container w/h — the present pass scales
 * it into whatever the window occupies. Scene2D/Scene3D are dispatch tags.
 *
 * STRUCT FIELDS (Mirroring darling/scene/scene.h):
 * ----------------------------------------------------------------------------
 *   Scene:                 // The scene root (Panel + mapping + present mode)
 *     Panel base;          // Inherited layout/bounds/tree state (see panel.h)
 *     int32_t mode;        // SCENE_MODE_STRETCH/FIT/PIXEL mapping mode
 *     int32_t presentMode; // SCENE_PRESENT_COMPOSITED/INLINE destination
 *   Scene2D:               // 2D dispatch tag, no extra payload
 *     Scene base;          // Embedded scene root
 *   Scene3D:               // 3D dispatch tag, no extra payload
 *     Scene base;          // Embedded scene root
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Scene_0(void)
 *   - Scene_2(width, height)
 *   - Scene_3(width, height, mode)
 *   - Scene2D_0(void)
 *   - Scene3D_0(void)
 *
 * Setters:
 *   - Scene_setMode(s, mode)
 *   - Scene_setPresentMode(s, presentMode)
 *   - Scene_setLocation(s, x, y)
 *   - Scene_setSize(s, w, h)
 *   - Scene_setAnchor(s, anchor)
 *   - Scene_setPivot(s, pivot)
 *   - Scene_setBackgroundColor(s, color)
 *   - Scene2D_setLocation(s, x, y)
 *   - Scene2D_setSize(s, w, h)
 *   - Scene2D_setAnchor(s, anchor)
 *   - Scene2D_setPivot(s, pivot)
 *   - Scene2D_setBackgroundColor(s, color)
 *   - Scene3D_setLocation(s, x, y)
 *   - Scene3D_setSize(s, w, h)
 *   - Scene3D_setAnchor(s, anchor)
 *   - Scene3D_setPivot(s, pivot)
 *   - Scene3D_setBackgroundColor(s, color)
 *
 * Getters:
 *   - Scene_getMode(s)
 *   - Scene_getPresentMode(s)
 *   - Scene_getVirtualWidth(s)
 *   - Scene_getVirtualHeight(s)
 * ============================================================================
 */


// darling/scene.c — scene root (Legacy: darling/Scene.java).

static Scene *allocScene(uint64_t typeId) {
    Scene *s = (Scene*) Memory_alloc(typeId, sizeof(Scene));
    if (!s)
        return nullptr;

    Panel *p = Panel_0();
    if (!p) {
        Memory_free(s);
        return nullptr;
    }
    (*s).base = (*p);
    Memory_free(p);

    (*s).mode = SCENE_MODE_PIXEL; // legacy default: resize reveals more canvas
    (*s).presentMode = SCENE_PRESENT_COMPOSITED; // Rule 14: retained target
    return s;
}

// The scene's Container sits two prefixes deep; rule 10 wants it hoisted.
static Component *sceneLayout(const Scene *s) {
    Panel *p = s ? (Panel*) &(*s).base : nullptr;
    return p ? &(*p).component : nullptr;
}

Scene *Scene_0(void) {
    return allocScene(TYPE_SCENE_SINGLETON);
}

Scene *Scene_2(float width, float height) {
    Scene *s = Scene_0();
    if (s)
        GraphicsComponent_setSize(sceneLayout(s), width, height);
    return s;
}

Scene *Scene_3(float width, float height, int mode) {
    Scene *s = Scene_2(width, height);
    if (s && mode >= SCENE_MODE_STRETCH && mode <= SCENE_MODE_PIXEL)
        (*s).mode = mode;
    return s;
}

Scene2D *Scene2D_0(void) {
    return (Scene2D*) allocScene(TYPE_SCENE2D_SINGLETON);
}

Scene3D *Scene3D_0(void) {
    return (Scene3D*) allocScene(TYPE_SCENE3D_SINGLETON);
}

int Scene_getMode(const Scene *s) {
    return s ? (*s).mode : SCENE_MODE_PIXEL;
}

void Scene_setMode(Scene *s, int mode) {
    if (!s || mode < SCENE_MODE_STRETCH || mode > SCENE_MODE_PIXEL)
        return;
    (*s).mode = mode;
}

int Scene_getPresentMode(const Scene *s) {
    return s ? (*s).presentMode : SCENE_PRESENT_COMPOSITED;
}

void Scene_setPresentMode(Scene *s, int presentMode) {
    if (!s || (presentMode != SCENE_PRESENT_COMPOSITED && presentMode != SCENE_PRESENT_INLINE))
        return;
    (*s).presentMode = presentMode;
}

float Scene_getVirtualWidth(const Scene *s) {
    return Component_getWidth(sceneLayout(s));
}

float Scene_getVirtualHeight(const Scene *s) {
    return Component_getHeight(sceneLayout(s));
}
