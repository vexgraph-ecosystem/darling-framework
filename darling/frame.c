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
    (*frame).hasVisualEffect = false;
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
    if (title != nullptr) {
        (*frame).title = strdup(title);
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

    if ((*frame).title != nullptr) {
        free((*frame).title);
        (*frame).title = nullptr;
    }

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

// WINDOW FORWARDING SETTERS
// ============================================================================

void Frame_setTitle(Frame *frame, const char *title) {
    if (frame == nullptr)
        return;
    if ((*frame).title != nullptr) {
        free((*frame).title);
        (*frame).title = nullptr;
    }
    if (title != nullptr) {
        (*frame).title = strdup(title);
    }
    if ((*frame).window != nullptr && title != nullptr) {
        Window_setTitle((*frame).window, title);
    }
}

void Frame_setSize(Frame *frame, int width, int height) {
    if (frame == nullptr)
        return;
    Frame_resize(frame, width, height);
    if ((*frame).window != nullptr)
        Window_setSize((*frame).window, width, height);
}

void Frame_setLocation(Frame *frame, int x, int y) {
    if (frame == nullptr || (*frame).window == nullptr)
        return;
    Window_setLocation((*frame).window, x, y);
}

void Frame_center(Frame *frame) {
    if (frame == nullptr || (*frame).window == nullptr)
        return;
    Window_center((*frame).window);
}

void Frame_show(Frame *frame) {
    if (frame == nullptr)
        return;
    (*frame).visible = true;
    if ((*frame).window != nullptr)
        Window_show((*frame).window);
    Frame_render(frame);
    Frame_present(frame);
}

void Frame_hide(Frame *frame) {
    if (frame == nullptr)
        return;
    (*frame).visible = false;
    if ((*frame).window != nullptr)
        Window_hide((*frame).window);
}

void Frame_setVisible(Frame *frame, bool visible) {
    if (visible)
        Frame_show(frame);
    else
        Frame_hide(frame);
}

void Frame_bringToFront(Frame *frame) {
    if (frame == nullptr || (*frame).window == nullptr)
        return;
    Window_bringToFront((*frame).window);
}

void Frame_setUndecorated(Frame *frame, int type) {
    if (frame == nullptr || (*frame).window == nullptr)
        return;
    Window_setUndecorated((*frame).window, type);
}

void Frame_setFloatingTrafficLights(Frame *frame, bool floating) {
    if (frame == nullptr || (*frame).window == nullptr)
        return;
    Window_setFloatingTrafficLights((*frame).window, floating);
}

void Frame_setBlur(Frame *frame, float blur) {
    if (frame == nullptr || (*frame).window == nullptr)
        return;
    Window_setBlur((*frame).window, blur);
}

void Frame_setOpacity(Frame *frame, float opacity) {
    if (frame == nullptr || (*frame).window == nullptr)
        return;
    Window_setOpacity((*frame).window, opacity);
}

void Frame_setTransparentBackground(Frame *frame, bool transparent) {
    if (frame == nullptr || (*frame).window == nullptr)
        return;
    Window_setTransparentBackground((*frame).window, transparent);
}

void Frame_setAlwaysOnTop(Frame *frame, bool onTop) {
    if (frame == nullptr || (*frame).window == nullptr)
        return;
    Window_setAlwaysOnTop((*frame).window, onTop);
}

void Frame_setClickThrough(Frame *frame, bool clickThrough) {
    if (frame == nullptr || (*frame).window == nullptr)
        return;
    Window_setClickThrough((*frame).window, clickThrough);
}

void Frame_setShadow(Frame *frame, bool shadow) {
    if (frame == nullptr || (*frame).window == nullptr)
        return;
    Window_setShadow((*frame).window, shadow);
}

void Frame_setMovableByBackground(Frame *frame, bool movable) {
    if (frame == nullptr || (*frame).window == nullptr)
        return;
    Window_setMovableByBackground((*frame).window, movable);
}

void Frame_minimize(Frame *frame) {
    if (frame == nullptr || (*frame).window == nullptr)
        return;
    Window_minimize((*frame).window);
}

void Frame_restore(Frame *frame) {
    if (frame == nullptr || (*frame).window == nullptr)
        return;
    Window_restore((*frame).window);
}

void Frame_setFullscreen(Frame *frame, bool fullscreen) {
    if (frame == nullptr || (*frame).window == nullptr)
        return;
    Window_setFullscreen((*frame).window, fullscreen);
}

void Frame_toggleFullscreen(Frame *frame) {
    if (frame == nullptr || (*frame).window == nullptr)
        return;
    Window_toggleFullscreen((*frame).window);
}

void Frame_setMinSize(Frame *frame, int width, int height) {
    if (frame == nullptr || (*frame).window == nullptr)
        return;
    Window_setMinSize((*frame).window, width, height);
}

void Frame_setMaxSize(Frame *frame, int width, int height) {
    if (frame == nullptr || (*frame).window == nullptr)
        return;
    Window_setMaxSize((*frame).window, width, height);
}

void Frame_setResizable(Frame *frame, bool resizable) {
    if (frame == nullptr || (*frame).window == nullptr)
        return;
    Window_setResizable((*frame).window, resizable);
}

void Frame_setClosable(Frame *frame, bool closable) {
    if (frame == nullptr || (*frame).window == nullptr)
        return;
    Window_setClosable((*frame).window, closable);
}

void Frame_setMiniaturizable(Frame *frame, bool miniaturizable) {
    if (frame == nullptr || (*frame).window == nullptr)
        return;
    Window_setMiniaturizable((*frame).window, miniaturizable);
}

void Frame_focus(Frame *frame) {
    if (frame == nullptr || (*frame).window == nullptr)
        return;
    Window_focus((*frame).window);
}

void Frame_setCursorType(Frame *frame, WindowCursorType type) {
    if (frame == nullptr || (*frame).window == nullptr)
        return;
    Window_setCursorType((*frame).window, type);
}

void Frame_setCursorLocked(Frame *frame, bool locked) {
    if (frame == nullptr || (*frame).window == nullptr)
        return;
    Window_setCursorLocked((*frame).window, locked);
}

// WINDOW FORWARDING GETTERS
// ============================================================================

const char *Frame_getTitle(const Frame *frame) {
    if (frame == nullptr || (*frame).title == nullptr)
        return "";
    return (*frame).title;
}

const char *Frame_title(const Frame *frame) {
    return Frame_getTitle(frame);
}

int Frame_width(const Frame *frame) {
    return Frame_getWidth(frame);
}

int Frame_height(const Frame *frame) {
    return Frame_getHeight(frame);
}

void Frame_getLocation(const Frame *frame, int *outX, int *outY) {
    if (frame == nullptr || (*frame).window == nullptr) {
        if (outX != nullptr) *outX = 0;
        if (outY != nullptr) *outY = 0;
        return;
    }
    Window_getLocation((*frame).window, outX, outY);
}

void Frame_getContentOrigin(const Frame *frame, int *outX, int *outY) {
    if (frame == nullptr || (*frame).window == nullptr) {
        if (outX != nullptr) *outX = 0;
        if (outY != nullptr) *outY = 0;
        return;
    }
    Window_getContentOrigin((*frame).window, outX, outY);
}

bool Frame_isVisible(const Frame *frame) {
    if (frame == nullptr)
        return false;
    return (*frame).visible;
}

bool Frame_isFullscreen(const Frame *frame) {
    if (frame == nullptr || (*frame).window == nullptr)
        return false;
    return Window_isFullscreen((Window*) (*frame).window);
}

bool Frame_isResizable(const Frame *frame) {
    if (frame == nullptr || (*frame).window == nullptr)
        return false;
    return Window_isResizable((Window*) (*frame).window);
}

bool Frame_isClosable(const Frame *frame) {
    if (frame == nullptr || (*frame).window == nullptr)
        return false;
    return Window_isClosable((Window*) (*frame).window);
}

bool Frame_isMiniaturizable(const Frame *frame) {
    if (frame == nullptr || (*frame).window == nullptr)
        return false;
    return Window_isMiniaturizable((Window*) (*frame).window);
}

bool Frame_isFocused(const Frame *frame) {
    if (frame == nullptr || (*frame).window == nullptr)
        return false;
    return Window_isFocused((Window*) (*frame).window);
}

WindowCursorType Frame_getCursorType(const Frame *frame) {
    if (frame == nullptr || (*frame).window == nullptr)
        return WINDOW_CURSOR_DEFAULT;
    return Window_getCursorType((*frame).window);
}

bool Frame_shouldClose(const Frame *frame) {
    if (frame == nullptr || (*frame).window == nullptr)
        return true;
    return Window_shouldClose((Window*) (*frame).window);
}

// EVENT ADAPTERS & LIFECYCLE
// ============================================================================

WindowEvent *Frame_getLifecycle(Frame *frame) {
    if (frame == nullptr || (*frame).window == nullptr)
        return nullptr;
    return Window_getLifecycle((*frame).window);
}

void Frame_addKeyAdapter(Frame *frame, const KeyHandler *adapter) {
    if (frame == nullptr || (*frame).window == nullptr)
        return;
    Window_addKeyAdapter((*frame).window, adapter);
}

bool Frame_removeKeyAdapter(Frame *frame, const KeyHandler *adapter) {
    if (frame == nullptr || (*frame).window == nullptr)
        return false;
    return Window_removeKeyAdapter((*frame).window, adapter);
}

void Frame_addMouseAdapter(Frame *frame, const MouseHandler *adapter) {
    if (frame == nullptr || (*frame).window == nullptr)
        return;
    Window_addMouseAdapter((*frame).window, adapter);
}

bool Frame_removeMouseAdapter(Frame *frame, const MouseHandler *adapter) {
    if (frame == nullptr || (*frame).window == nullptr)
        return false;
    return Window_removeMouseAdapter((*frame).window, adapter);
}

void Frame_addTouchAdapter(Frame *frame, const TouchHandler *adapter) {
    if (frame == nullptr || (*frame).window == nullptr)
        return;
    Window_addTouchAdapter((*frame).window, adapter);
}

bool Frame_removeTouchAdapter(Frame *frame, const TouchHandler *adapter) {
    if (frame == nullptr || (*frame).window == nullptr)
        return false;
    return Window_removeTouchAdapter((*frame).window, adapter);
}
