#include "annotation/overview.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "darling/frame.h"
#include "input/key.h"
#include "input/mouse.h"
#include "kernel/application.h"

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: FrameTest (_tests/darling/frame_test.c)
 * LEVEL: L3 — Module Code (headless verification harness)
 * ============================================================================
 * Verification suite for Frame: layer stacking, presentsWithTransaction,
 * visual effect configuration, layer dimensions, composable frame-function
 * slots (replacing onRender), KeyMap-backed input bindings, and teardown.
 *
 * STRUCT FIELDS: none — procedural test harness.
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - main(void)
 * ============================================================================
 */

static double g_lastDt = -1.0;

static void onFrameRender(Frame *frame, double dt, void *userData) {
    (void) frame;
    (void) dt;
    (*(int*) userData) += 1;
}

static void onFrameDt(Frame *frame, double dt, void *userData) {
    (void) frame;
    g_lastDt = dt;
    (*(int*) userData) += 1;
}

static void onInputFire(void *userData, int64_t combo) {
    (void) combo;
    (*(int*) userData) += 1;
}

// Taps settle when the per-key pending window closes. The platform drivers
// use 250ms windows; tests drive a 30ms window and wait 40ms so settlement
// lands with ~250ms-of-test-latency saved. Settlement is what fires
// KeyMap bindings, so every tap below must wait it out.
#define TAP_WIN_NS 30000000ULL  // 30 ms tap window
#define SETTLE_SLEEP_US 40000   // 40 ms > window, so settlement always lands

static void tapKeyWin(int key, uint64_t winNanos) {
    Key_pushEvent(0, key, KEY_ACTION_DOWN, winNanos);
    Key_dispatchEvents();
    Key_pushEvent(0, key, KEY_ACTION_UP, winNanos);
    Key_dispatchEvents();
}

static void settle(void) {
    usleep(SETTLE_SLEEP_US);
}

static void holdSuper(bool down) {
    Key_pushEvent(0, KEY_LEFT_SUPER, down ? KEY_ACTION_DOWN : KEY_ACTION_UP, 250000000ULL);
    Key_dispatchEvents();
}

int main(void) {
    printf("=== Running Frame Test Suite ===\n");

    // 1. Null safety
    assert(Frame_getWindow(nullptr) == nullptr);
    assert(Frame_getGraphics(nullptr) == nullptr);
    assert(Frame_getRootPanel(nullptr) == nullptr);
    assert(Frame_getLayerCount(nullptr) == 0);
    assert(Frame_getLayer(nullptr, 0) == nullptr);
    assert(Frame_hasVisualEffect(nullptr) == false);
    assert(Frame_isPresentsWithTransaction(nullptr) == false);
    assert(Frame_getWidth(nullptr) == 0);
    assert(Frame_getHeight(nullptr) == 0);
    assert(Frame_getFrameFunctionCount(nullptr) == 0);
    assert(Frame_getKeyMap(nullptr) == nullptr);
    assert(Frame_addFrameFunction(nullptr, onFrameRender, nullptr) == UINT32_MAX);
    assert(Frame_removeFrameFunction(nullptr, 0) == false);
    assert(Frame_addKeyFunction(nullptr, 0, onInputFire, nullptr) == false);
    assert(Frame_addMouseFunction(nullptr, 0, onInputFire, nullptr) == false);
    assert(Frame_removeFunction(nullptr, 0, onInputFire) == false);
    Frame_render(nullptr);
    Frame_present(nullptr);
    Frame_resize(nullptr, 100, 100);
    Frame_destroy(nullptr);
    Frame_free(nullptr);

    // 2. Lifecycle & Stacked FBO Layers
    Frame frame;
    bool ok = Frame_init(nullptr, nullptr, &frame);
    assert(ok == true);
    assert(Frame_isPresentsWithTransaction(&frame) == true);
    assert(Frame_hasVisualEffect(&frame) == false);
    Frame_setVisualEffect(&frame, true, FRAME_MATERIAL_HUD_WINDOW);
    assert(Frame_hasVisualEffect(&frame) == true);
    Frame_setVisualEffect(&frame, false, FRAME_MATERIAL_HUD_WINDOW);
    assert(Frame_hasVisualEffect(&frame) == false);

    // Test window forwarding methods
    Frame_setTitle(&frame, "TestTitle");
    assert(strcmp(Frame_getTitle(&frame), "TestTitle") == 0);
    assert(strcmp(Frame_title(&frame), "TestTitle") == 0);
    Frame_setSize(&frame, 640, 480);
    assert(Frame_width(&frame) == 640);
    assert(Frame_height(&frame) == 480);
    assert(Frame_isVisible(&frame) == false);
    Frame_setVisible(&frame, true);
    assert(Frame_isVisible(&frame) == true);
    Frame_setVisible(&frame, false);
    assert(Frame_isVisible(&frame) == false);

    // Test chrome modes & transparency
    assert(Frame_isDecorated(&frame) == true);
    assert(Frame_isNaked(&frame) == false);
    assert(Frame_isBorderless(&frame) == false);
    Frame_setDecorated(&frame, FRAME_UNDECORATED_NAKED);
    assert(Frame_getDecorated(&frame) == FRAME_UNDECORATED_NAKED);
    assert(Frame_isNaked(&frame) == true);
    assert(Frame_isDecorated(&frame) == false);
    Frame_setBorderless(&frame, true);
    assert(Frame_isBorderless(&frame) == true);
    Frame_setNaked(&frame, false);
    assert(Frame_isDecorated(&frame) == true);
    Frame_setDecorated(&frame, FRAME_UNECORATED_NAKED);
    assert(Frame_isNaked(&frame) == true);
    Frame_setDecorated(&frame, FRAME_DECORATED);
    assert(Frame_isDecorated(&frame) == true);
    Frame_setTransparent(&frame, true);
    Frame_setTransparentBackground(&frame, false);

    // Add stacked FBO layers (e.g. Layer 0: Scene, Layer 1: Content/UI, Layer 2: Modal)
    FrameLayer *layer0 = nullptr;
    FrameLayer *layer1 = nullptr;
    FrameLayer *layer2 = nullptr;

    assert(Frame_addLayer(&frame, 800, 600, &layer0) == true);
    assert(layer0 != nullptr);
    assert((*layer0).id == 0);
    assert((*layer0).width == 800);
    assert((*layer0).height == 600);

    assert(Frame_addLayer(&frame, 800, 600, &layer1) == true);
    assert(layer1 != nullptr);
    assert((*layer1).id == 1);

    assert(Frame_addLayer(&frame, 800, 600, &layer2) == true);
    assert(layer2 != nullptr);
    assert((*layer2).id == 2);

    assert(Frame_getLayerCount(&frame) == 3);
    assert(Frame_getLayer(&frame, 0) == layer0);
    assert(Frame_getLayer(&frame, 1) == layer1);
    assert(Frame_getLayer(&frame, 2) == layer2);
    assert(Frame_getLayer(&frame, 3) == nullptr);

    // 3. Composable frame functions (replaces the single onRender hook)
    int renderCount = 0;
    uint32_t f0 = Frame_addFrameFunction(&frame, onFrameRender, &renderCount);
    assert(f0 == 0);
    assert(Frame_getFrameFunctionCount(&frame) == 1);
    Frame_render(&frame);
    assert(renderCount == 1);

    Frame_present(&frame);

    // 4. Resize propagation across stacked layers
    Frame_resize(&frame, 1024, 768);
    assert(Frame_getWidth(&frame) == 1024);
    assert(Frame_getHeight(&frame) == 768);
    assert((*layer0).width == 1024);
    assert((*layer0).height == 768);
    assert((*layer1).width == 1024);
    assert((*layer2).width == 1024);
    assert(renderCount == 2); // Frame_resize triggers render

    // 4b. Multiple slots, swap-remove semantics
    int dtCount = 0;
    g_lastDt = -1.0;
    uint32_t f1 = Frame_addFrameFunction(&frame, onFrameDt, &dtCount);
    assert(f1 == 1);
    assert(Frame_getFrameFunctionCount(&frame) == 2);

    Frame_render(&frame);
    assert(dtCount == 1);
    assert(renderCount == 3);

    Frame_render(&frame);
    assert(dtCount == 2);
    assert(renderCount == 4);

    assert(Frame_removeFrameFunction(&frame, 0) == true); // swap-remove
    assert(Frame_getFrameFunctionCount(&frame) == 1);
    int dtBefore = dtCount;
    Frame_render(&frame);
    assert(dtCount == dtBefore + 1); // dt slot swapped into hole 0, still fires
    assert(renderCount == 4);        // onFrameRender slot gone — no longer fires

    // 5. KeyMap-backed input bindings (resolved once per Frame_render; taps
    // settle when the per-key pending window closes — short test window)
    Key_init();
    Mouse_init();

    int inputFires = 0;
    int mouseFires = 0;
    int64_t cmdQ = KMOD_CMD | KMODE_TAP | KEY_Q;
    int64_t dblRight = KMODE_DOUBLE_TAP | MOUSE_RIGHT;

    assert(Frame_addKeyFunction(&frame, cmdQ, onInputFire, &inputFires) == true);
    assert(Frame_getKeyMap(&frame) != nullptr);
    assert(KeyMap_count(Frame_getKeyMap(&frame)) == 1);

    // exact modifier gate: plain A and modifier-less Q must not fire Cmd+Q
    tapKeyWin(KEY_A, TAP_WIN_NS);
    settle();
    Frame_render(&frame);
    assert(inputFires == 0);
    tapKeyWin(KEY_Q, TAP_WIN_NS);
    settle();
    Frame_render(&frame);
    assert(inputFires == 0);
    holdSuper(true);
    tapKeyWin(KEY_A, TAP_WIN_NS); // A under Cmd — code mismatch
    settle();
    Frame_render(&frame);
    assert(inputFires == 0);
    tapKeyWin(KEY_Q, TAP_WIN_NS); // Cmd+Q — fires at settlement
    settle();
    Frame_render(&frame);
    assert(inputFires == 1);
    Frame_render(&frame);
    assert(inputFires == 1); // consumed — no re-fire

    // mouse double-click binding (must not be shadowed by live Cmd)
    assert(Frame_addMouseFunction(&frame, dblRight, onInputFire, &mouseFires) == true);
    assert(KeyMap_count(Frame_getKeyMap(&frame)) == 2);
    holdSuper(false);
    Mouse_pushButtonEvent(0, MOUSE_RIGHT, KEY_ACTION_DOWN, TAP_WIN_NS);
    Mouse_pushButtonEvent(0, MOUSE_RIGHT, KEY_ACTION_UP, TAP_WIN_NS);
    Mouse_pushButtonEvent(0, MOUSE_RIGHT, KEY_ACTION_DOWN, TAP_WIN_NS);
    Mouse_pushButtonEvent(0, MOUSE_RIGHT, KEY_ACTION_UP, TAP_WIN_NS);
    settle();
    Frame_render(&frame);
    assert(mouseFires == 1);
    assert(inputFires == 1); // at most one binding fires per present
    Frame_render(&frame);
    assert(mouseFires == 1); // consumed

    // removal
    assert(Frame_removeFunction(&frame, cmdQ, onInputFire) == true);
    assert(Frame_removeFunction(&frame, dblRight, onInputFire) == true);
    assert(KeyMap_count(Frame_getKeyMap(&frame)) == 0);

    Key_shutdown();
    Mouse_shutdown();

    // 6. Application Frame Handler Bridge
    Application *testApp = Application_1("FrameBridgeApp");
    assert(testApp != nullptr);
    assert(Frame_application(&frame) == nullptr);
    assert(Frame_window(&frame) == nullptr);

    assert(Frame_addFrameHandler(&frame, testApp) == true);
    assert(Frame_application(&frame) == testApp);

    assert(Frame_removeFrameHandler(&frame, testApp) == true);
    assert(Frame_application(&frame) == nullptr);

    Application_free(testApp);

    // 7. Heap constructor & teardown; fresh-frame dt semantics
    Frame *heapFrame = Frame_0();
    assert(heapFrame != nullptr);
    assert(Frame_isPresentsWithTransaction(heapFrame) == true);

    // A fresh frame's first render delivers dt == 0.0 by construction;
    // the second carries real elapsed time (> 0).
    int heapDtCount = 0;
    g_lastDt = -1.0;
    uint32_t h0 = Frame_addFrameFunction(heapFrame, onFrameDt, &heapDtCount);
    assert(h0 == 0);
    Frame_render(heapFrame);
    assert(heapDtCount == 1);
    assert(g_lastDt == 0.0);
    usleep(2000); // let the monotonic clock advance so dt > 0.0 is real, not a flake
    Frame_render(heapFrame);
    assert(heapDtCount == 2);
    assert(g_lastDt > 0.0);

    Frame_free(heapFrame);

    // 8. Board panes: content upper + scene bottom, borrowed and nullable
    assert(Frame_getContentPane(nullptr) == nullptr);
    assert(Frame_getScenePane(nullptr) == nullptr);
    assert(Frame_getContentPane(&frame) == nullptr);
    assert(Frame_getScenePane(&frame) == nullptr);
    Frame_setContentPane(nullptr, nullptr);
    Frame_setScenePane(nullptr, nullptr);
    Frame_setContentPane(&frame, nullptr);
    assert(Frame_getContentPane(&frame) == nullptr);
    Frame_setScenePane(&frame, nullptr);
    assert(Frame_getScenePane(&frame) == nullptr);

    // 9. Traffic-light cluster toggle: null-safe no-op without a window
    Frame_macos_setTrafficLightVisible(nullptr, false);
    Frame_macos_setTrafficLightVisible(&frame, true);

    Frame_destroy(&frame);
    printf("=== Frame Test Suite Passed! ===\n");
    return 0;
}