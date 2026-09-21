#include "darling/frame.h"

#include "annotation/definition.h"
#include "annotation/overview.h"
#include "darling/component.h"
#include "darling/dialog/dialog.h"
#include "darling/panel/panel.h"
#include "darling/panel/list_container.h"
#include "darling/panel/grid_container.h"
#include "darling/panel/expandable_list_container.h"
#include "darling/panel/scroll_container.h"
#include "darling/compositor.h"
#include "graphvex/graphics_loop.h"
#include "event/bridge.h"
#include "input/key_map.h"
#include "kernel/application.h"
#include "nio/mem.h"
#include "window/window.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Frame
 * ============================================================================
 * Host frame encapsulating an R1 Window, R3 Graphics device context, root
 * UI Panel, and a stack of VkImage / Framebuffer layers hosted on a
 * CAMetalLayer configured with presentsWithTransaction = YES for
 * synchronized WindowServer rendering during live resize, minimize, and
 * zoom events — the bridge between hotcwap's Window and graphvex's
 * Graphics. Board roots (contentPane upper, scenePane lower) are borrowed
 * and nullable; the Window Board Root Lock Law enforces lockedRoot +
 * top-left geometry on set, and the compositor resolves both roots from
 * the Frame alone (the Window Decoupling Law — the Window holds zero
 * Panels). Layer slots are a fixed array (DARLING_FRAME_MAX_LAYERS);
 * FrameFunction present callbacks live in a master-arena grown slot table
 * (doubling) that fires in registration order on every Frame_render; the
 * KeyMap is lazily created in the master arena on first bind. The
 * inSyncResize flag is a re-entrancy guard (one render+present per
 * geometry event), presentedFrames drives the compositor infancy gate,
 * emptyPresents caps consecutive empty seam presents, and lastPublishGen
 * re-arms present demand on fresh VkLayer publishes. lastComponentGen
 * latches the Component generation counter (Component_gen) so the
 * component seam re-arms demand when a component tree mutates without a
 * board/panel paint.
 * ============================================================================
 */

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
 *   Panel *contentPane;                           // Upper board root (borrowed, nullable)
 *   Panel *scenePane;                             // Bottom board root (borrowed, nullable)
 *   FrameLayer layers[DARLING_FRAME_MAX_LAYERS];  // Stacked FBOs inside CAMetalLayer
 *   uint32_t layerCount;                          // Active layer count
 *   Dialog *childDialogs[16];                     // Managed child dialogs (max 16)
 *   uint32_t childDialogCount;                    // Active child dialog count
 *   Dialog *ownerDialog;                          // Owning Dialog if embedded in a Dialog
 *   Frame *parentFrame;                           // Parent frame if this is a child dialog
  *   bool presentsWithTransaction;                 // Atomic presentation flag
 *   uint32_t presentedFrames;                       // Confirmed seam presents (infancy gate)
 *   uint32_t emptyPresents;                         // Consecutive empty seam presents (empty-cap guard)
 *   uint64_t lastPublishGen;                        // Last observed VkLayer publish generation (probe re-arm)
 *   uint64_t lastComponentGen;                      // Last observed Component generation (component seam demand latch)
 *   int width;                                    // Pixel width
 *   int height;                                   // Pixel height
 *   bool inLiveResize;                            // Drag-resize active
 *   bool isMinimized;                             // Window miniaturized
 *   bool isZoomed;                                // Window zoomed
 *   bool inSyncResize;                            // Re-entrancy guard (one render+present per geometry event)
 *   int drawableWidth;                            // Authoritative native-px footprint of the live
 *   int drawableHeight;                           // content rect (resolved via convertRectToBacking
 *                                                 // at geometry time; seam + present path agree)
 *   FrameFunction *functions;                     // Master-arena grown slot table
 *   uint32_t functionCount;                       // Live present callbacks
 *   uint32_t functionCapacity;                    // Doubling capacity
 *   KeyMap *keyMap;                               // Master-arena KeyMap (lazy)
 *   uint64_t lastRenderNanos;                     // Monotonic dt source
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
 * SLOT RECORD: FrameFunction (owned by Frame):
 * ----------------------------------------------------------------------------
 *   void (*fn)(Frame *frame, double dt, void *userData); // Present callback
 *   void *userData;                                       // Callback context
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
 *   - Frame_addFrameHandler(frame, app)
 *   - Frame_removeFrameHandler(frame, app)
 *   - Frame_addFrameFunction(frame, fn, userData)   : slot index
 *   - Frame_removeFrameFunction(frame, index)       : swap-remove
 *   - Frame_getFrameFunctionCount(frame)
 *   - Frame_addKeyFunction(frame, combo, fn, userData)
 *   - Frame_addMouseFunction(frame, combo, fn, userData)
 *   - Frame_removeFunction(frame, combo, fn)
 *   - Frame_getKeyMap(frame)
 *   - Frame_platformAttach(frame)
 *   - Frame_platformDetach(frame)
 *   - Frame_platformSyncTransaction(frame)
 *
 * Setters:
 *   - Frame_setWindow(frame, window)
 *   - Frame_setApplication(frame, app)
 *   - Frame_setGraphics(frame, graphics)
 *   - Frame_setRootPanel(frame, panel)
  *   - Frame_setContentPane(frame, panel)
 *   - Frame_setScenePane(frame, panel)
 *   - Frame_setPresentsWithTransaction(frame, presentsWithTransaction)
 *   - Frame_setNativeView(frame, nativeView)
 *   - Frame_setOnQuitRequested(frame, onQuitRequested, userData)
 *
 * Window Forwarding Setters (thin pass-through to the R1 host window):
 *   - Frame_setTitle(frame, title)
 *   - Frame_setSize(frame, width, height)
 *   - Frame_setLocation(frame, x, y)
 *   - Frame_center(frame)
 *   - Frame_show(frame)
 *   - Frame_hide(frame)
 *   - Frame_setVisible(frame, visible)
 *   - Frame_bringToFront(frame)
 *   - Frame_setUndecorated(frame, type)
 *   - Frame_setDecorated(frame, flag)
 *   - Frame_setNaked(frame, naked)
 *   - Frame_setBorderless(frame, borderless)
 *   - Frame_setFloatingTrafficLights(frame, floating)
  *   - Frame_macos_setTrafficLightVisible(frame, visible)  : macOS cluster toggle
 *   - Frame_setOpacity(frame, opacity)
 *   - Frame_setTransparent(frame, transparent)
 *   - Frame_setTransparentBackground(frame, transparent)
 *   - Frame_setAlwaysOnTop(frame, onTop)
 *   - Frame_setClickThrough(frame, clickThrough)
 *   - Frame_setShadow(frame, shadow)
 *   - Frame_setMovableByBackground(frame, movable)
 *   - Frame_minimize(frame)
 *   - Frame_restore(frame)
 *   - Frame_setFullscreen(frame, fullscreen)
 *   - Frame_toggleFullscreen(frame)
 *   - Frame_setMinSize(frame, width, height)
 *   - Frame_setMaxSize(frame, width, height)
 *   - Frame_setResizable(frame, resizable)
 *   - Frame_setClosable(frame, closable)
 *   - Frame_setMiniaturizable(frame, miniaturizable)
 *   - Frame_focus(frame)
 *   - Frame_setCursorType(frame, type)
 *   - Frame_setCursorLocked(frame, locked)
 *
 * Getters:
 *   - Frame_getWindow(const frame) / Frame_window(const frame)
 *   - Frame_getApplication(const frame) / Frame_application(const frame)
 *   - Frame_getGraphics(const frame)
 *   - Frame_getRootPanel(const frame)
 *   - Frame_getContentPane(const frame)
 *   - Frame_getScenePane(const frame)
 *   - Frame_getTitle(const frame) / Frame_title(const frame)
 *   - Frame_getLayerCount(const frame)
  *   - Frame_getLayer(frame, index)
 *   - Frame_isPresentsWithTransaction(const frame)
 *   - Frame_getSize(const frame, outWidth, outHeight)
 *   - Frame_getWidth(const frame) / Frame_width(const frame)
 *   - Frame_getHeight(const frame) / Frame_height(const frame)
 *   - Frame_isInLiveResize(const frame)
 *   - Frame_isMinimized(const frame)
 *   - Frame_isZoomed(const frame)
 *   - Frame_getNativeView(const frame)
 *
 * Window Forwarding Getters (thin pass-through to the R1 host window):
 *   - Frame_getLocation(const frame, outX, outY)
 *   - Frame_getContentOrigin(const frame, outX, outY)
 *   - Frame_isVisible(const frame)
 *   - Frame_isFullscreen(const frame)
 *   - Frame_isResizable(const frame)
 *   - Frame_isClosable(const frame)
 *   - Frame_isMiniaturizable(const frame)
 *   - Frame_isFocused(const frame)
 *   - Frame_isTransparent(const frame)
 *   - Frame_getDecorated(const frame)
 *   - Frame_isDecorated(const frame)
 *   - Frame_isNaked(const frame)
 *   - Frame_isBorderless(const frame)
 *   - Frame_getCursorType(const frame)
 *   - Frame_shouldClose(const frame)
 *
 * Dialog Hierarchy & Closing Policy:
 *   - Frame_addChildDialog(frame, dialog)
 *   - Frame_removeChildDialog(frame, dialog)
 *   - Frame_getChildDialogCount(const frame)
 *   - Frame_getChildDialog(const frame, index)
 *   - Frame_getActiveClingingDialog(const frame)
 *   - Frame_canClose(const frame)
 *   - Frame_closeChildDialogs(frame)
 *   - Frame_close(frame)
 *
 * Event Adapters & Lifecycle:
 *   - Frame_getLifecycle(frame)
 *   - Frame_addKeyAdapter(frame, adapter)
 *   - Frame_removeKeyAdapter(frame, adapter)
 *   - Frame_addMouseAdapter(frame, adapter)
 *   - Frame_removeMouseAdapter(frame, adapter)
 *   - Frame_addTouchAdapter(frame, adapter)
 *   - Frame_removeTouchAdapter(frame, adapter)
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
    (*frame).childDialogCount = 0;
    (*frame).ownerDialog = nullptr;
    (*frame).parentFrame = nullptr;
    (*frame).contentPane = nullptr;
    (*frame).scenePane = nullptr;
    (*frame).onQuitRequested = nullptr;
        (*frame).quitRequestedUserData = nullptr;
    (*frame).presentsWithTransaction = true;
    (*frame).presentedFrames = 0;
    (*frame).emptyPresents = 0;
    (*frame).lastPublishGen = 0;
    (*frame).lastComponentGen = 0;
    (*frame).chromeMode = FRAME_DECORATED;
    (*frame).width = win ? Window_width(win) : 800;
    (*frame).height = win ? Window_height(win) : 600;
    (*frame).inLiveResize = false;
    (*frame).isMinimized = false;
    (*frame).isZoomed = false;
    (*frame).functions = nullptr;
    (*frame).functionCount = 0;
    (*frame).functionCapacity = 0;
    (*frame).keyMap = nullptr;
    (*frame).lastRenderNanos = 0;
    (*frame).nativeView = nullptr;
    (*frame).visualEffect = VisualEffect_0();
    (*frame).surface = Surface_0();

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

    Frame_closeChildDialogs(frame);
    for (uint32_t i = 0; i < (*frame).childDialogCount; i++) {
        Dialog *d = (*frame).childDialogs[i];
        if (d != nullptr) {
            (*d).handler = nullptr;
            (*d).frame.parentFrame = nullptr;
        }
    }
    (*frame).childDialogCount = 0;

    if ((*frame).application != nullptr && (*frame).window != nullptr) {
        Application_removeWindow((*frame).application, (*frame).window);
        (*frame).application = nullptr;
    }

    Frame_platformDetach(frame);

    if ((*frame).visualEffect != nullptr) {
        VisualEffect_free((*frame).visualEffect);
        (*frame).visualEffect = nullptr;
    }
    if ((*frame).surface != nullptr) {
        Surface_free((*frame).surface);
        (*frame).surface = nullptr;
    }

    // Release arena-backed composables before the window resources die.
    // The master arena owns their slabs — reclaimed at Memory_freeAll (the
    // Teardown Order Law); we drop references for lifetime clarity.
    if ((*frame).keyMap != nullptr)
        KeyMap_destroy((*frame).keyMap);
    (*frame).keyMap = nullptr;
    (*frame).functions = nullptr;
    (*frame).functionCount = 0;
    (*frame).functionCapacity = 0;

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

bool Darling_bridge(Frame *frame, Application *app) {
    if (frame == nullptr || app == nullptr)
        return false;

    Window *window = Frame_getWindow(frame);
    if (!window)
        return false;

    // 1. Initialize Vulkan/Metal compositor and board/layer flight
    Darling_initCompositor(frame);

    // 2. Attach UI event bridge to the root panel or content pane
    Panel *target = (*frame).contentPane ? (*frame).contentPane : (*frame).rootPanel;
    if (target)
        Darling_bridgeAttach(target);
    Darling_bridgeSetWindow(window);

    // 3. Connect frame to the Application manifest and show window
    Frame_addFrameHandler(frame, app);
    Application_addWindow(app, window);
    Frame_show(frame);

    return true;
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

    // Input first: fold live gesture state into the KeyMap once per present.
    // At most one binding fires; the source tap is consumed before the
    // callback so a re-entrant render can never re-fire (the Present-On-
    // Demand Law).
    if ((*frame).keyMap != nullptr) {
        int64_t firedCombo = 0;
        KeyMap_resolve((*frame).keyMap, &firedCombo);
    }

    // Frame functions receive dt: seconds since the previous render,
    // measured on the CLOCK_MONOTONIC clock (0.0 on the first render).
    double dt = 0.0;
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        uint64_t nowNanos = (uint64_t) ts.tv_sec * 1000000000ull + (uint64_t) ts.tv_nsec;
        if ((*frame).lastRenderNanos != 0 && nowNanos >= (*frame).lastRenderNanos)
            dt = (double) (nowNanos - (*frame).lastRenderNanos) / 1000000000.0;
        (*frame).lastRenderNanos = nowNanos;
    }

    for (uint32_t i = 0; i < (*frame).functionCount; i++) {
        FrameFunction *fn = &(*frame).functions[i];
        if ((*fn).fn != nullptr)
            (*fn).fn(frame, dt, (*fn).userData);
    }

    for (uint32_t i = 0; i < (*frame).layerCount; ++i) {
        FrameLayer *layer = &(*frame).layers[i];
        if (!(*layer).visible)
            continue;
    }
}

uint32_t Frame_addFrameFunction(Frame *frame, void (*fn)(Frame *frame, double dt, void *userData), void *userData) {
    if (frame == nullptr || fn == nullptr)
        return UINT32_MAX;

    if ((*frame).functionCount >= (*frame).functionCapacity) {
        uint32_t newCap = (*frame).functionCapacity == 0 ? 4 : (*frame).functionCapacity * 2;
        FrameFunction *grown = (FrameFunction*) MemoryArena_alloc(
            Memory_defaultArena(), TYPE_FRAME_FUNCTION_ARRAY, sizeof(FrameFunction) * newCap
        );
        if (grown == nullptr)
            return UINT32_MAX;
        if ((*frame).functions != nullptr)
            memcpy(grown, (*frame).functions, sizeof(FrameFunction) * (*frame).functionCount);
        (*frame).functions = grown;
        (*frame).functionCapacity = newCap;
    }

    uint32_t slot = (*frame).functionCount;
    (*frame).functions[slot].fn = fn;
    (*frame).functions[slot].userData = userData;
    (*frame).functionCount = slot + 1;
    return slot;
}

bool Frame_removeFrameFunction(Frame *frame, uint32_t index) {
    if (frame == nullptr || index >= (*frame).functionCount)
        return false;
    uint32_t last = (*frame).functionCount - 1;
    if (index != last)
        (*frame).functions[index] = (*frame).functions[last];
    (*frame).functionCount = last;
    return true;
}

uint32_t Frame_getFrameFunctionCount(const Frame *frame) {
    if (frame == nullptr)
        return 0;
    return (*frame).functionCount;
}

bool Frame_addKeyFunction(Frame *frame, int64_t combo, KeyBindingFn fn, void *userData) {
    if (frame == nullptr)
        return false;
    if ((*frame).keyMap == nullptr)
        (*frame).keyMap = KeyMap_create(Memory_defaultArena());
    if ((*frame).keyMap == nullptr)
        return false;
    return KeyMap_bind((*frame).keyMap, combo, fn, userData);
}

bool Frame_addMouseFunction(Frame *frame, int64_t combo, KeyBindingFn fn, void *userData) {
    // KeyMap is gesture-generic and does not distinguish bind time — the
    // split API exists so intent reads at the call site. Mouse combos use
    // MOUSE_* button ids (0..31) as the key-code half of the combo.
    return Frame_addKeyFunction(frame, combo, fn, userData);
}

bool Frame_removeFunction(Frame *frame, int64_t combo, KeyBindingFn fn) {
    if (frame == nullptr)
        return false;
    if ((*frame).keyMap == nullptr)
        return false;
    return KeyMap_unbind((*frame).keyMap, combo, fn);
}

KeyMap *Frame_getKeyMap(const Frame *frame) {
    if (frame == nullptr)
        return nullptr;
    return (*frame).keyMap;
}

void Frame_present(Frame *frame) {
    if (frame == nullptr)
        return;

    if ((*frame).presentsWithTransaction)
        Frame_platformSyncTransaction(frame);
}

static void relayoutSubtree(Panel *parent, float parentX, float parentY, float parentW, float parentH) {
    if (!parent || parentW <= 0.0f || parentH <= 0.0f)
        return;

    uint64_t ptype = Memory_type(parent);
    if (ptype == TYPE_LIST_PANEL_SINGLETON) {
        ListContainer_layout((ListContainer*) parent);
    } else if (ptype == TYPE_GRID_PANEL_SINGLETON) {
        GridContainer_layout((GridContainer*) parent);
    } else if (ptype == TYPE_EXPANDABLE_LIST_CONTAINER_SINGLETON) {
        ExpandableListContainer_layout((ExpandableListContainer*) parent);
    } else if (ptype == TYPE_SCROLL_PANEL_SINGLETON) {
        ScrollContainer_setViewportSize((ScrollContainer*) parent, parentW, parentH);
    }

    size_t childCount = Panel_childCount(parent);
    for (size_t i = 0; i < childCount; i++) {
        Panel *child = Panel_getChild(parent, i);
        if (!child)
            continue;

        Vec4 rect;
        Container *base = &(*child).base;
        Container_resolve(base, parentX, parentY, parentW, parentH, &rect);
        relayoutSubtree(child, rect.x, rect.y, rect.z, rect.w);
    }
}

void Frame_relayoutChildren(Frame *frame) {
    if (!frame)
        return;
    // Live fractional bounds when the platform seam has resolved them (the
    // Single Rounding Currency Law): the rounded int cache lags the true
    // window by up to half a point, and every parent-derived edge (center,
    // percent, right/bottom anchors) would sit off the device grid by a
    // wobbling sub-pixel amount. Edges pinned AT 0 or AT the parent edge
    // stay exact either way — the middle drifts, which is why a live resize
    // feels stable at the dragged edge and slinky elsewhere.
    float w = (*frame).liveWidth > 0.0f ? (*frame).liveWidth : (float) (*frame).width;
    float h = (*frame).liveHeight > 0.0f ? (*frame).liveHeight : (float) (*frame).height;
    if (w <= 0.0f || h <= 0.0f)
        return;

    if ((*frame).contentPane != nullptr)
        relayoutSubtree((*frame).contentPane, 0.0f, 0.0f, w, h);
    if ((*frame).scenePane != nullptr)
        relayoutSubtree((*frame).scenePane, 0.0f, 0.0f, w, h);
    if ((*frame).rootPanel != nullptr && (*frame).rootPanel != (*frame).contentPane)
        relayoutSubtree((*frame).rootPanel, 0.0f, 0.0f, w, h);
}

bool Frame_syncResize(Frame *frame, int width, int height) {
    if (frame == nullptr || width <= 0 || height <= 0)
        return false;

    // Re-entrancy guard (one render+present per geometry event): a
    // programmatic resize lands here, calls Window_setSize, and AppKit
    // reflects it synchronously through setFrameSize: -> windowRefreshSize ->
    // the resize hook -> back into THIS body. Without the guard the nested
    // call plus both outer passes render+present up to three times for one
    // resize. The outer pass finishes the job; nested calls stand down.
    if ((*frame).inSyncResize)
        return false;
    (*frame).inSyncResize = true;

    (*frame).width = width;
    (*frame).height = height;

    if ((*frame).window != nullptr) {
        if (Window_width((*frame).window) != width || Window_height((*frame).window) != height) {
            Window_setSize((*frame).window, width, height);
        }
    }

    for (uint32_t i = 0; i < (*frame).layerCount; ++i) {
        FrameLayer *layer = &(*frame).layers[i];
        (*layer).width = (uint32_t) width;
        (*layer).height = (uint32_t) height;
    }

    // Force locked roots to track new window dimensions (live fractional
    // bounds per the Single Rounding Currency Law — see relayoutChildren).
    float liveW = (*frame).liveWidth > 0.0f ? (*frame).liveWidth : (float) width;
    float liveH = (*frame).liveHeight > 0.0f ? (*frame).liveHeight : (float) height;
    if ((*frame).rootComponent != nullptr) {
        Component_setSize((*frame).rootComponent, liveW, liveH);
    }
    if ((*frame).contentPane != nullptr)
        Container_forceSize(&(*(*frame).contentPane).base, liveW, liveH);
    if ((*frame).scenePane != nullptr)
        Container_forceSize(&(*(*frame).scenePane).base, liveW, liveH);
    if ((*frame).rootPanel != nullptr && (*frame).rootPanel != (*frame).contentPane)
        Container_forceSize(&(*(*frame).rootPanel).base, liveW, liveH);
    // Panel Override Law: the embedded Component metadata tracks the same
    // forced size (plain setters ARE the force path — Component carries no
    // lock flag; the Panel facades enforce it).
    if ((*frame).contentPane != nullptr) {
        Panel *board = (*frame).contentPane;
        Component_setSize(&(*board).component, liveW, liveH);
    }
    if ((*frame).scenePane != nullptr) {
        Panel *board = (*frame).scenePane;
        Component_setSize(&(*board).component, liveW, liveH);
    }
    if ((*frame).rootPanel != nullptr && (*frame).rootPanel != (*frame).contentPane) {
        Panel *board = (*frame).rootPanel;
        Component_setSize(&(*board).component, liveW, liveH);
    }

    // Edit layouts of the children & resolve anchors/locations
    Frame_relayoutChildren(frame);

    // Synchronize platform layer (CAMetalLayer) bounds and scale
    Frame_platformSyncLayer(frame, width, height);

    // Render frame callbacks (input resolution, FrameFunction callbacks)
    Frame_render(frame);

    // Synchronously present to WindowServer
    bool presented = Darling_syncPresent(frame);
    (*frame).inSyncResize = false;
    return presented;
}

void Frame_resize(Frame *frame, int width, int height) {
    Frame_syncResize(frame, width, height);
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

void Frame_setRootComponent(Frame *frame, Component *component) {
    if (frame == nullptr)
        return;
    (*frame).rootComponent = component;
    if (component != nullptr) {
        Component_setSize(component, (float)(*frame).width, (float)(*frame).height);
    }
}

void Frame_setContentPane(Frame *frame, Panel *panel) {
    if (frame == nullptr)
        return;
    (*frame).contentPane = panel;
    if (panel != nullptr) {
        // Window Board Root Lock Law (law 49): lock the board root to the window dimensions.
        Container *base = &(*panel).base;
        (*base).lockedRoot = 1;
        (*base).anchor = CONTAINER_ANCHOR_TOP_LEFT;
        (*base).pivot = CONTAINER_PIVOT_TOP_LEFT;
        (*base).x = 0.0f;
        (*base).y = 0.0f;
        (*base).w = (float)(*frame).width;
        (*base).h = (float)(*frame).height;
        (*base).dirty = 1;
        // Panel Override Law: mirror the override into the embedded
        // Component metadata (anchor/pivot values mirror CONTAINER_*).
        Component *meta = &(*panel).component;
        Component_setAnchor(meta, COMPONENT_ANCHOR_TOP_LEFT);
        Component_setPivot(meta, COMPONENT_PIVOT_TOP_LEFT);
        Component_setLocation(meta, 0.0f, 0.0f);
        Component_setSize(meta, (float)(*frame).width, (float)(*frame).height);
    }
}

void Frame_setScenePane(Frame *frame, Panel *panel) {
    if (frame == nullptr)
        return;
    (*frame).scenePane = panel;
    if (panel != nullptr) {
        // Window Board Root Lock Law (law 49): lock the board root to the window dimensions.
        Container *base = &(*panel).base;
        (*base).lockedRoot = 1;
        (*base).anchor = CONTAINER_ANCHOR_TOP_LEFT;
        (*base).pivot = CONTAINER_PIVOT_TOP_LEFT;
        (*base).x = 0.0f;
        (*base).y = 0.0f;
        (*base).w = (float)(*frame).width;
        (*base).h = (float)(*frame).height;
        (*base).dirty = 1;
        // Panel Override Law: mirror the override into the embedded
        // Component metadata (anchor/pivot values mirror CONTAINER_*).
        Component *meta = &(*panel).component;
        Component_setAnchor(meta, COMPONENT_ANCHOR_TOP_LEFT);
        Component_setPivot(meta, COMPONENT_PIVOT_TOP_LEFT);
        Component_setLocation(meta, 0.0f, 0.0f);
        Component_setSize(meta, (float)(*frame).width, (float)(*frame).height);
    }
}

void Frame_setPresentsWithTransaction(Frame *frame, bool presentsWithTransaction) {
    if (frame == nullptr)
        return;
    (*frame).presentsWithTransaction = presentsWithTransaction;
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

Component *Frame_getRootComponent(const Frame *frame) {
    if (frame == nullptr)
        return nullptr;
    return (*frame).rootComponent;
}

Panel *Frame_getContentPane(const Frame *frame) {
    if (frame == nullptr)
        return nullptr;
    return (*frame).contentPane;
}

Panel *Frame_getScenePane(const Frame *frame) {
    if (frame == nullptr)
        return nullptr;
    return (*frame).scenePane;
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

float Frame_getLiveWidth(const Frame *frame) {
    if (frame == nullptr)
        return 0.0f;
    return (*frame).liveWidth;
}

float Frame_getLiveHeight(const Frame *frame) {
    if (frame == nullptr)
        return 0.0f;
    return (*frame).liveHeight;
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
    if ((*frame).window != nullptr) {
        Window_show((*frame).window);
        GraphicsLoop_markDirty(GraphicsLoop_default(), (*frame).window);
    }
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
    if (frame == nullptr)
        return;
    (*frame).chromeMode = type;
    if ((*frame).window != nullptr)
        Window_setUndecorated((*frame).window, type);
}

void Frame_setDecorated(Frame *frame, int flag) {
    if (frame == nullptr)
        return;
    (*frame).chromeMode = flag;
    if ((*frame).window != nullptr)
        Window_setUndecorated((*frame).window, flag);
}

void Frame_setNaked(Frame *frame, bool naked) {
    Frame_setDecorated(frame, naked ? FRAME_UNDECORATED_NAKED : FRAME_DECORATED);
}

void Frame_setBorderless(Frame *frame, bool borderless) {
    Frame_setDecorated(frame, borderless ? FRAME_UNDECORATED_BORDERLESS : FRAME_DECORATED);
}

void Frame_setFloatingTrafficLights(Frame *frame, bool floating) {
    if (frame == nullptr || (*frame).window == nullptr)
        return;
    Window_setFloatingTrafficLights((*frame).window, floating);
}

void Frame_macos_setTrafficLightVisible(Frame *frame, bool visible) {
    if (frame == nullptr || (*frame).window == nullptr)
        return;
    Window_macOS_setTrafficLightButtonVisible((*frame).window, WINDOW_TRAFFIC_LIGHT_CLOSE, visible);
    Window_macOS_setTrafficLightButtonVisible((*frame).window, WINDOW_TRAFFIC_LIGHT_MINIMIZE, visible);
    Window_macOS_setTrafficLightButtonVisible((*frame).window, WINDOW_TRAFFIC_LIGHT_ZOOM, visible);
}

void Frame_setOpacity(Frame *frame, float opacity) {
    if (frame == nullptr || (*frame).window == nullptr)
        return;
    Window_setOpacity((*frame).window, opacity);
}

void Frame_setTransparent(Frame *frame, bool transparent) {
    if (frame == nullptr || (*frame).window == nullptr)
        return;
    Window_setTransparent((*frame).window, transparent);
    Window_setTransparentBackground((*frame).window, transparent);
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
    if (frame == nullptr)
        return;
    Dialog *clinging = Frame_getActiveClingingDialog(frame);
    if (clinging != nullptr) {
        Dialog_focus(clinging);
        Dialog_bringToFront(clinging);
        return;
    }
    if ((*frame).window != nullptr)
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

bool Frame_isTransparent(const Frame *frame) {
    if (frame == nullptr || (*frame).window == nullptr)
        return false;
    return Window_isTransparent((*frame).window);
}

int Frame_getDecorated(const Frame *frame) {
    if (frame == nullptr)
        return FRAME_DECORATED;
    return (*frame).chromeMode;
}

bool Frame_isDecorated(const Frame *frame) {
    if (frame == nullptr)
        return true;
    return (*frame).chromeMode == FRAME_DECORATED;
}

bool Frame_isNaked(const Frame *frame) {
    if (frame == nullptr)
        return false;
    return (*frame).chromeMode == FRAME_UNDECORATED_NAKED;
}

bool Frame_isBorderless(const Frame *frame) {
    if (frame == nullptr)
        return false;
    return (*frame).chromeMode == FRAME_UNDECORATED_BORDERLESS;
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

// DIALOG HIERARCHY & CLOSING POLICY
// ============================================================================

bool Frame_addChildDialog(Frame *frame, Dialog *dialog) {
    if (frame == nullptr || dialog == nullptr)
        return false;

    for (uint32_t i = 0; i < (*frame).childDialogCount; i++) {
        if ((*frame).childDialogs[i] == dialog)
            return true;
    }

    if ((*frame).childDialogCount >= DARLING_FRAME_MAX_DIALOGS)
        return false;

    (*frame).childDialogs[(*frame).childDialogCount++] = dialog;
    return true;
}

bool Frame_removeChildDialog(Frame *frame, Dialog *dialog) {
    if (frame == nullptr || dialog == nullptr)
        return false;

    for (uint32_t i = 0; i < (*frame).childDialogCount; i++) {
        if ((*frame).childDialogs[i] == dialog) {
            for (uint32_t j = i; j + 1 < (*frame).childDialogCount; j++) {
                (*frame).childDialogs[j] = (*frame).childDialogs[j + 1];
            }
            (*frame).childDialogs[--(*frame).childDialogCount] = nullptr;
            (*dialog).handler = nullptr;
            (*dialog).frame.parentFrame = nullptr;
            return true;
        }
    }
    return false;
}

uint32_t Frame_getChildDialogCount(const Frame *frame) {
    if (frame == nullptr)
        return 0;
    return (*frame).childDialogCount;
}

Dialog *Frame_getChildDialog(const Frame *frame, uint32_t index) {
    if (frame == nullptr || index >= (*frame).childDialogCount)
        return nullptr;
    return (*frame).childDialogs[index];
}

// Deepest open focus-holding dialog below f, skipping the excluded subtree
// (an ancestor re-search must never re-descend into the frame it came from).
static Dialog *frameDeepestHolding(const Frame *f, const Frame *exclude) {
    if (f == nullptr)
        return nullptr;
    for (uint32_t i = 0; i < (*f).childDialogCount; i++) {
        Dialog *d = (*f).childDialogs[i];
        if (d == nullptr || &(*d).frame == exclude)
            continue;
        if ((Dialog_isClinging(d) || Dialog_isModal(d)) && Dialog_isOpen(d)) {
            Dialog *deeper = frameDeepestHolding(&(*d).frame, nullptr);
            if (deeper != nullptr)
                return deeper;
            return d;
        }
    }
    return nullptr;
}

Dialog *Frame_getActiveClingingDialog(const Frame *frame) {
    if (frame == nullptr)
        return nullptr;

    // Search downward: deepest open focus-holding child of this frame.
    // Modal implies focus capture exactly like clinging: a modal dialog
    // holds focus until closed even when clinging was never set.
    Dialog *down = frameDeepestHolding(frame, nullptr);
    if (down != nullptr)
        return down;

    // Search upward: if this frame holds nothing and is itself a child
    // dialog, the holder governing this frame is the holder governing the
    // parent — excluding each subtree already searched on the way up.
    const Frame *child = frame;
    const Frame *parent = (*frame).parentFrame;
    while (parent != nullptr) {
        Dialog *up = frameDeepestHolding(parent, child);
        if (up != nullptr)
            return up;
        child = parent;
        parent = (*parent).parentFrame;
    }

    return nullptr;
}

bool Frame_canClose(const Frame *frame) {
    if (frame == nullptr)
        return true;

    if (Frame_getActiveClingingDialog(frame) != nullptr)
        return false;

    for (uint32_t i = 0; i < (*frame).childDialogCount; i++) {
        Dialog *d = (*frame).childDialogs[i];
        if (d != nullptr && Dialog_isOpen(d)) {
            if (!Frame_canClose(&(*d).frame))
                return false;
        }
    }

    if ((*frame).onQuitRequested != nullptr) {
        return (*frame).onQuitRequested((Frame*) frame, (*frame).quitRequestedUserData);
    }
    return true;
}

void Frame_closeChildDialogs(Frame *frame) {
    if (frame == nullptr)
        return;
    for (uint32_t i = 0; i < (*frame).childDialogCount; i++) {
        Dialog *d = (*frame).childDialogs[i];
        if (d != nullptr && Dialog_isOpen(d)) {
            Dialog_close(d);
        }
    }
}

void Frame_close(Frame *frame) {
    if (frame == nullptr)
        return;
    if (!Frame_canClose(frame))
        return;

    if ((*frame).ownerDialog != nullptr) {
        Dialog_close((*frame).ownerDialog);
        return;
    }

    Frame_closeChildDialogs(frame);
    Frame_hide(frame);
    if ((*frame).window != nullptr) {
        Window_setShouldClose((*frame).window, true);
    }
}

void Frame_setOnQuitRequested(Frame *frame, bool (*onQuitRequested)(Frame *frame, void *userData), void *userData) {
    if (frame == nullptr)
        return;
    (*frame).onQuitRequested = onQuitRequested;
    (*frame).quitRequestedUserData = userData;
}

// VISUAL EFFECT & SURFACE INTEGRATION
// ============================================================================

void Frame_setBlur(Frame *frame, float blur) {
    if (frame == nullptr)
        return;
    if ((*frame).visualEffect != nullptr)
        VisualEffect_setBlur((*frame).visualEffect, blur);
}

float Frame_getBlur(const Frame *frame) {
    if (frame == nullptr || (*frame).visualEffect == nullptr)
        return 0.0f;
    return VisualEffect_getBlur((*frame).visualEffect);
}

void Frame_setMaterial(Frame *frame, int material) {
    if (frame == nullptr)
        return;
    if ((*frame).visualEffect != nullptr)
        VisualEffect_setMaterial((*frame).visualEffect, material);
}

int Frame_getMaterial(const Frame *frame) {
    if (frame == nullptr || (*frame).visualEffect == nullptr)
        return 0;
    return VisualEffect_getMaterial((*frame).visualEffect);
}

void Frame_setVibrancy(Frame *frame, bool vibrant) {
    if (frame == nullptr)
        return;
    if ((*frame).visualEffect != nullptr)
        VisualEffect_setVibrancy((*frame).visualEffect, vibrant);
}

bool Frame_isVibrant(const Frame *frame) {
    if (frame == nullptr || (*frame).visualEffect == nullptr)
        return false;
    return VisualEffect_isVibrant((*frame).visualEffect);
}

VisualEffect *Frame_getVisualEffect(const Frame *frame) {
    return frame ? (*frame).visualEffect : nullptr;
}

Surface *Frame_getSurface(const Frame *frame) {
    return frame ? (*frame).surface : nullptr;
}

void Frame_setBackgroundColor(Frame *frame, const Color *color) {
    if (frame == nullptr)
        return;
    if (color != nullptr) {
        (*frame).backgroundColor = *color;
        if ((*frame).window != nullptr)
            Window_setTransparentBackground((*frame).window, (*color).a < 1.0f);
    }
}

const Color *Frame_getBackgroundColor(const Frame *frame) {
    if (frame == nullptr)
        return nullptr;
    return &(*frame).backgroundColor;
}

void Frame_setBackground(Frame *frame, float r, float g, float b, float a) {
    if (frame == nullptr)
        return;
    Color c;
    Color_init(r, g, b, a, &c);
    Frame_setBackgroundColor(frame, &c);
}

void Frame_getBackground(const Frame *frame, float *outR, float *outG, float *outB, float *outA) {
    if (frame == nullptr) {
        if (outR) *outR = 0.0f;
        if (outG) *outG = 0.0f;
        if (outB) *outB = 0.0f;
        if (outA) *outA = 0.0f;
        return;
    }
    if (outR) *outR = (*frame).backgroundColor.r;
    if (outG) *outG = (*frame).backgroundColor.g;
    if (outB) *outB = (*frame).backgroundColor.b;
    if (outA) *outA = (*frame).backgroundColor.a;
}


