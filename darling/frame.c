#include "darling/frame.h"

#include "annotation/overview.h"
#include "darling/panel/panel.h"
#include "kernel/application.h"
#include "window/window.h"

#include <stdlib.h>
#include <string.h>

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Frame
 * LEVEL: L2 — Behavior (Frame bridging hotcwap Window and graphvex Graphics)
 * ============================================================================
 * Host frame encapsulating an R1 Window, R3 Graphics device context, root UI
 * Panel, and a stack of VkImage / Framebuffer layers hosted on a CAMetalLayer
 * configured with presentsWithTransaction = YES for synchronized WindowServer
 * rendering during live resize, minimize, and zoom events.
 *
 * STRUCT FIELDS (Mirroring darling/frame.h):
 * ----------------------------------------------------------------------------
 *   Window *window;                               // R1 host window pointer
 *   void *graphics;                               // R3 GPU graphics context
 *   Panel *rootPanel;                             // Root UI component tree
 *   FrameLayer layers[DARLING_FRAME_MAX_LAYERS];  // Stacked FBOs inside CAMetalLayer
 *   uint32_t layerCount;                          // Active layer count
 *   bool hasVisualEffect;                         // NSVisualEffectView vibrancy
 *   int visualEffectMaterial;                     // FrameVisualEffectMaterial
 *   bool presentsWithTransaction;                 // Atomic presentation flag
 *   int width;                                    // Pixel width
 *   int height;                                   // Pixel height
 *   bool inLiveResize;                            // Drag-resize active
 *   bool isMinimized;                             // Window miniaturized
 *   bool isZoomed;                                // Window zoomed
 *   void (*onRender)(Frame *frame, void *userData); // Custom render hook
 *   void *userData;                               // Callback context
 *   void *nativeView;                             // Native platform view handle
 *
 * SLOT RECORD: FrameLayer (owned by Frame):
 * ----------------------------------------------------------------------------
 *   uint32_t id;                                  // Layer ordinal identifier
 *   void *vkImage;                                // VkImage handle
 *   void *vkImageView;                            // VkImageView handle
 *   void *vkFramebuffer;                          // VkFramebuffer handle
 *   void *metalTexture;                           // CAMetalDrawable / MTLTexture
 *   uint32_t width;                               // Layer width in pixels
 *   uint32_t height;                              // Layer height in pixels
 *   float opacity;                                // Composition alpha [0.0, 1.0]
 *   bool visible;                                 // Visibility flag
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Frame_0(void)
 *   - Frame_1(window)
 *   - Frame_2(window, graphics)
 *   - Frame_3(title, width, height)
 *   - Frame_init(window, graphics, frame)
 *   - Frame_destroy(frame)
 *   - Frame_free(frame)
 *
 * Core Functions:
 *   - Frame_render(frame)
 *   - Frame_present(frame)
 *   - Frame_resize(frame, width, height)
 *   - Frame_addLayer(frame, width, height, outLayer)
 *   - Frame_platformAttach(frame)
 *   - Frame_platformDetach(frame)
 *   - Frame_platformSyncTransaction(frame)
 *
 * Setters:
 *   - Frame_setWindow(frame, window)
 *   - Frame_setGraphics(frame, graphics)
 *   - Frame_setRootPanel(frame, panel)
 *   - Frame_setVisualEffect(frame, enable, material)
 *   - Frame_setPresentsWithTransaction(frame, presentsWithTransaction)
 *   - Frame_setOnRender(frame, onRender, userData)
 *   - Frame_setNativeView(frame, nativeView)
 *
 * Getters:
 *   - Frame_getWindow(const frame)
 *   - Frame_getGraphics(const frame)
 *   - Frame_getRootPanel(const frame)
 *   - Frame_getLayerCount(const frame)
 *   - Frame_getLayer(frame, index)
 *   - Frame_hasVisualEffect(const frame)
 *   - Frame_getVisualEffectMaterial(const frame)
 *   - Frame_isPresentsWithTransaction(const frame)
 *   - Frame_getSize(const frame, outWidth, outHeight)
 *   - Frame_getWidth(const frame)
 *   - Frame_getHeight(const frame)
 *   - Frame_isInLiveResize(const frame)
 *   - Frame_isMinimized(const frame)
 *   - Frame_isZoomed(const frame)
 *   - Frame_getNativeView(const frame)
 * ============================================================================
 */

// CONSTRUCTORS
// ============================================================================

bool Frame_init(Window *win, void *graphics, Frame *frame) {
    if (frame == nullptr)
        return false;

    memset(frame, 0, sizeof(Frame));
    (*frame).window = win;
    (*frame).graphics = graphics;
    (*frame).rootPanel = nullptr;
    (*frame).layerCount = 0;
    (*frame).hasVisualEffect = true;
    (*frame).visualEffectMaterial = FRAME_MATERIAL_HUD_WINDOW;
    (*frame).presentsWithTransaction = true;
    (*frame).width = win ? Window_width(win) : 800;
    (*frame).height = win ? Window_height(win) : 600;
    (*frame).inLiveResize = false;
    (*frame).isMinimized = false;
    (*frame).isZoomed = false;
    (*frame).onRender = nullptr;
    (*frame).userData = nullptr;
    (*frame).nativeView = nullptr;

    Frame_platformAttach(frame);
    return true;
}

Frame *Frame_0(void) {
    Frame *frame = (Frame*) calloc(1, sizeof(Frame));
    if (frame == nullptr)
        return nullptr;
    Frame_init(nullptr, nullptr, frame);
    return frame;
}

Frame *Frame_1(Window *window) {
    Frame *frame = (Frame*) calloc(1, sizeof(Frame));
    if (frame == nullptr)
        return nullptr;
    Frame_init(window, nullptr, frame);
    return frame;
}

Frame *Frame_2(Window *window, void *graphics) {
    Frame *frame = (Frame*) calloc(1, sizeof(Frame));
    if (frame == nullptr)
        return nullptr;
    Frame_init(window, graphics, frame);
    return frame;
}

Frame *Frame_3(const char *title, int width, int height) {
    Window *win = Window_create(title, width, height);
    if (win == nullptr)
        return nullptr;
    Frame *frame = Frame_2(win, nullptr);
    if (frame == nullptr) {
        Window_destroy(win);
        return nullptr;
    }
    return frame;
}

void Frame_destroy(Frame *frame) {
    if (frame == nullptr)
        return;

    if ((*frame).application != nullptr && (*frame).window != nullptr) {
        Application_removeWindow((*frame).application, (*frame).window);
        (*frame).application = nullptr;
    }

    Frame_platformDetach(frame);

    if ((*frame).rootPanel != nullptr) {
        (*frame).rootPanel = nullptr;
    }

    if ((*frame).window != nullptr) {
        Window_destroy((*frame).window);
        (*frame).window = nullptr;
    }

    (*frame).layerCount = 0;
}

void Frame_free(Frame *frame) {
    if (frame == nullptr)
        return;
    Frame_destroy(frame);
    free(frame);
}

// CORE FUNCTIONS
// ============================================================================

bool Frame_addFrameHandler(Frame *frame, Application *app) {
    if (frame == nullptr || app == nullptr)
        return false;

    Frame_setApplication(frame, app);
    return true;
}

bool Frame_removeFrameHandler(Frame *frame, Application *app) {
    if (frame == nullptr || app == nullptr)
        return false;

    if ((*frame).application == app) {
        Frame_setApplication(frame, nullptr);
        return true;
    }
    return false;
}

bool Frame_addLayer(Frame *frame, uint32_t width, uint32_t height, FrameLayer **outLayer) {
    if (frame == nullptr || (*frame).layerCount >= DARLING_FRAME_MAX_LAYERS)
        return false;

    uint32_t idx = (*frame).layerCount;
    FrameLayer *layer = &(*frame).layers[idx];
    (*layer).id = idx;
    (*layer).vkImage = nullptr;
    (*layer).vkImageView = nullptr;
    (*layer).vkFramebuffer = nullptr;
    (*layer).metalTexture = nullptr;
    (*layer).width = width;
    (*layer).height = height;
    (*layer).opacity = 1.0f;
    (*layer).visible = true;

    (*frame).layerCount = idx + 1;
    if (outLayer != nullptr)
        *outLayer = layer;

    return true;
}

void Frame_render(Frame *frame) {
    if (frame == nullptr)
        return;

    if ((*frame).onRender != nullptr)
        (*frame).onRender(frame, (*frame).userData);

    for (uint32_t i = 0; i < (*frame).layerCount; ++i) {
        FrameLayer *layer = &(*frame).layers[i];
        if (!(*layer).visible)
            continue;
    }
}

void Frame_present(Frame *frame) {
    if (frame == nullptr)
        return;

    if ((*frame).presentsWithTransaction)
        Frame_platformSyncTransaction(frame);
}

void Frame_resize(Frame *frame, int width, int height) {
    if (frame == nullptr)
        return;

    (*frame).width = width;
    (*frame).height = height;

    for (uint32_t i = 0; i < (*frame).layerCount; ++i) {
        FrameLayer *layer = &(*frame).layers[i];
        (*layer).width = (uint32_t) width;
        (*layer).height = (uint32_t) height;
    }

    Frame_render(frame);
    Frame_present(frame);
}

// SETTERS
// ============================================================================

void Frame_setWindow(Frame *frame, Window *window) {
    if (frame == nullptr)
        return;
    if ((*frame).application != nullptr && (*frame).window != nullptr) {
        Application_removeWindow((*frame).application, (*frame).window);
    }
    (*frame).window = window;
    if ((*frame).application != nullptr && (*frame).window != nullptr) {
        Application_addWindow((*frame).application, (*frame).window);
    }
}

void Frame_setApplication(Frame *frame, Application *app) {
    if (frame == nullptr)
        return;
    if ((*frame).application != nullptr && (*frame).window != nullptr) {
        Application_removeWindow((*frame).application, (*frame).window);
    }
    (*frame).application = app;
    if ((*frame).application != nullptr && (*frame).window != nullptr) {
        Application_addWindow((*frame).application, (*frame).window);
    }
}

void Frame_setGraphics(Frame *frame, void *graphics) {
    if (frame == nullptr)
        return;
    (*frame).graphics = graphics;
}

void Frame_setRootPanel(Frame *frame, Panel *panel) {
    if (frame == nullptr)
        return;
    (*frame).rootPanel = panel;
}

void Frame_setVisualEffect(Frame *frame, bool enable, int material) {
    if (frame == nullptr)
        return;
    (*frame).hasVisualEffect = enable;
    (*frame).visualEffectMaterial = material;
}

void Frame_setPresentsWithTransaction(Frame *frame, bool presentsWithTransaction) {
    if (frame == nullptr)
        return;
    (*frame).presentsWithTransaction = presentsWithTransaction;
}

void Frame_setOnRender(Frame *frame, void (*onRender)(Frame *frame, void *userData), void *userData) {
    if (frame == nullptr)
        return;
    (*frame).onRender = onRender;
    (*frame).userData = userData;
}

void Frame_setNativeView(Frame *frame, void *nativeView) {
    if (frame == nullptr)
        return;
    (*frame).nativeView = nativeView;
}

// GETTERS
// ============================================================================

Window *Frame_getWindow(const Frame *frame) {
    if (frame == nullptr)
        return nullptr;
    return (*frame).window;
}

Window *Frame_window(const Frame *frame) {
    return Frame_getWindow(frame);
}

Application *Frame_getApplication(const Frame *frame) {
    if (frame == nullptr)
        return nullptr;
    return (*frame).application;
}

Application *Frame_application(const Frame *frame) {
    return Frame_getApplication(frame);
}

void *Frame_getGraphics(const Frame *frame) {
    if (frame == nullptr)
        return nullptr;
    return (*frame).graphics;
}

Panel *Frame_getRootPanel(const Frame *frame) {
    if (frame == nullptr)
        return nullptr;
    return (*frame).rootPanel;
}

uint32_t Frame_getLayerCount(const Frame *frame) {
    if (frame == nullptr)
        return 0;
    return (*frame).layerCount;
}

FrameLayer *Frame_getLayer(Frame *frame, uint32_t index) {
    if (frame == nullptr || index >= (*frame).layerCount)
        return nullptr;
    return &(*frame).layers[index];
}

bool Frame_hasVisualEffect(const Frame *frame) {
    if (frame == nullptr)
        return false;
    return (*frame).hasVisualEffect;
}

int Frame_getVisualEffectMaterial(const Frame *frame) {
    if (frame == nullptr)
        return 0;
    return (*frame).visualEffectMaterial;
}

bool Frame_isPresentsWithTransaction(const Frame *frame) {
    if (frame == nullptr)
        return false;
    return (*frame).presentsWithTransaction;
}

void Frame_getSize(const Frame *frame, int *outWidth, int *outHeight) {
    if (outWidth != nullptr)
        *outWidth = frame ? (*frame).width : 0;
    if (outHeight != nullptr)
        *outHeight = frame ? (*frame).height : 0;
}

int Frame_getWidth(const Frame *frame) {
    if (frame == nullptr)
        return 0;
    return (*frame).width;
}

int Frame_getHeight(const Frame *frame) {
    if (frame == nullptr)
        return 0;
    return (*frame).height;
}

bool Frame_isInLiveResize(const Frame *frame) {
    if (frame == nullptr)
        return false;
    return (*frame).inLiveResize;
}

bool Frame_isMinimized(const Frame *frame) {
    if (frame == nullptr)
        return false;
    return (*frame).isMinimized;
}

bool Frame_isZoomed(const Frame *frame) {
    if (frame == nullptr)
        return false;
    return (*frame).isZoomed;
}

void *Frame_getNativeView(const Frame *frame) {
    if (frame == nullptr)
        return nullptr;
    return (*frame).nativeView;
}
