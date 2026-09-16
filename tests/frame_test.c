#include "annotation/overview.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "darling/frame.h"
#include "kernel/application.h"

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: FrameTest (_tests/darling/frame_test.c)
 * LEVEL: L3 — Module Code (headless verification harness)
 * ============================================================================
 * Verification suite for Frame: layer stacking, presentsWithTransaction,
 * visual effect configuration, layer dimensions, render callbacks, and teardown.
 *
 * STRUCT FIELDS: none — procedural test harness.
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - main(void)
 * ============================================================================
 */

static void onFrameRender(Frame *frame, void *userData) {
    (void) frame;
    (*(int*) userData) += 1;
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

    // 3. Render callback & continuous presentation
    int renderCount = 0;
    Frame_setOnRender(&frame, onFrameRender, &renderCount);
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

    // 5. Application Frame Handler Bridge
    Application *testApp = Application_1("FrameBridgeApp");
    assert(testApp != nullptr);
    assert(Frame_application(&frame) == nullptr);
    assert(Frame_window(&frame) == nullptr);

    assert(Frame_addFrameHandler(&frame, testApp) == true);
    assert(Frame_application(&frame) == testApp);

    assert(Frame_removeFrameHandler(&frame, testApp) == true);
    assert(Frame_application(&frame) == nullptr);

    Application_free(testApp);

    // 6. Heap constructor & teardown
    Frame *heapFrame = Frame_0();
    assert(heapFrame != nullptr);
    assert(Frame_isPresentsWithTransaction(heapFrame) == true);
    Frame_free(heapFrame);

    Frame_destroy(&frame);
    printf("=== Frame Test Suite Passed! ===\n");
    return 0;
}
