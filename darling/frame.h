#ifndef DARLING_FRAME_H
#define DARLING_FRAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "c23/constructor.h"
#include "input/key_map.h"
#include "window/window.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DARLING_FRAME_MAX_LAYERS 8
#define DARLING_FRAME_MAX_DIALOGS 16

typedef struct Frame Frame;
typedef struct Panel Panel;
typedef struct Application Application;
typedef struct Dialog Dialog;

typedef enum FrameChromeMode {
    FRAME_DECORATED = 0,               // Standard opaque title bar, title visible
    FRAME_UNDECORATED_BORDERLESS = 1,  // No title bar and no traffic lights
    FRAME_UNDECORATED_NAKED = 2,       // Transparent title bar, hidden title, traffic lights kept
} FrameChromeMode;

#define FRAME_DECORATED               0
#define FRAME_UNDECORATED_BORDERLESS  1
#define FRAME_UNDECORATED_NAKED       2
#define FRAME_UNECORATED_NAKED        2
#define FRAME_NAKED                   2
#define FRAME_BORDERLESS              1

typedef enum FrameVisualEffectMaterial {
    FRAME_MATERIAL_HUD_WINDOW = 0,
    FRAME_MATERIAL_SIDEBAR = 1,
    FRAME_MATERIAL_SELECTION = 2,
    FRAME_MATERIAL_MENU = 3,
    FRAME_MATERIAL_POPOVER = 4,
    FRAME_MATERIAL_FULLSCREEN_UI = 5
} FrameVisualEffectMaterial;

// SLOT RECORD: FrameLayer (owned by Frame)
typedef struct FrameLayer {
    uint32_t id;
    void *vkImage;             // VkImage handle
    void *vkImageView;         // VkImageView handle
    void *vkFramebuffer;       // VkFramebuffer handle
    void *metalTexture;        // CAMetalDrawable / MTLTexture
    uint32_t width;
    uint32_t height;
    float opacity;
    bool visible;
} FrameLayer;

// SLOT RECORD: FrameFunction (owned by Frame) — one composable present
// callback in the frame's grown slot table (replaces the single onRender
// hook). Slots fire in registration order on every Frame_render; dt is
// seconds since the previous render, 0.0 on the first render.
typedef struct FrameFunction {
    void (*fn)(struct Frame *frame, double dt, void *userData);
    void *userData;
} FrameFunction;

typedef struct Frame {
    Window *window;             // R1 host window pointer
    Application *application;   // R1 host application manifest pointer (nullable)
    void *graphics;             // R3 GPU graphics context (VkHotContext / Device)
    Panel *rootPanel;           // Root UI component tree
    Panel *contentPane;         // Upper board root: UI canvas (borrowed, nullable)
    Panel *scenePane;           // Bottom board root: scene/backdrop (borrowed, nullable)
    char *title;                // Owned title string (strdup on set)
    bool visible;               // Visibility state flag
    int chromeMode;             // FrameChromeMode (FRAME_DECORATED / BORDERLESS / NAKED)
    FrameLayer layers[DARLING_FRAME_MAX_LAYERS]; // Stacked FBOs inside CAMetalLayer
    uint32_t layerCount;

    Dialog *childDialogs[DARLING_FRAME_MAX_DIALOGS]; // Managed child dialogs
    uint32_t childDialogCount;
    Dialog *ownerDialog;        // Owning Dialog instance if embedded in a Dialog
    struct Frame *parentFrame;  // Parent frame if this frame is a child dialog (bidirectional tracking)

    bool (*onQuitRequested)(struct Frame *frame, void *userData);
    void *quitRequestedUserData;

    bool hasVisualEffect;       // NSVisualEffectView vibrancy enabled
    int visualEffectMaterial;   // FrameVisualEffectMaterial
    bool presentsWithTransaction; // CAMetalLayer presentsWithTransaction = YES

    int width;
    int height;
    bool inLiveResize;
    bool isMinimized;
    bool isZoomed;

    // --- Frame Function Slots (composable present callbacks, replaces onRender) ---
    FrameFunction *functions;        // Master-arena grown slot table (doubling)
    uint32_t functionCount;
    uint32_t functionCapacity;
    KeyMap *keyMap;                  // Master-arena KeyMap; lazily created on first bind
    uint64_t lastRenderNanos;        // Monotonic clock at last Frame_render (dt source)

    void *nativeView;          // Pointer to platform NSView / CAMetalLayer container
} Frame;

// Constructors:
//   Frame()
//   Frame(window)
//   Frame(window, graphics)
//   Frame(title, width, height)
Frame *Frame_0(void);
Frame *Frame_1(Window *window);
Frame *Frame_2(Window *window, void *graphics);
Frame *Frame_3(const char *title, int width, int height);

#define Frame(...) CONSTRUCTOR_DISPATCH(Frame, __VA_ARGS__)

bool Frame_init(Window *win, void *graphics, Frame *frame);
void Frame_destroy(Frame *frame);
void Frame_free(Frame *frame);

// Core Functions:
void Frame_render(Frame *frame);
void Frame_present(Frame *frame);
void Frame_resize(Frame *frame, int width, int height);
bool Frame_addLayer(Frame *frame, uint32_t width, uint32_t height, FrameLayer **outLayer);
bool Frame_addFrameHandler(Frame *frame, Application *app);
bool Frame_removeFrameHandler(Frame *frame, Application *app);

// Platform hooks for AppKit / window_cocoa.m integration
void Frame_platformAttach(Frame *frame);
void Frame_platformDetach(Frame *frame);
void Frame_platformSyncTransaction(Frame *frame);

// Setters:
void Frame_setWindow(Frame *frame, Window *window);
void Frame_setApplication(Frame *frame, Application *app);
void Frame_setGraphics(Frame *frame, void *graphics);
void Frame_setRootPanel(Frame *frame, Panel *panel);
// Board roots (non-strict: any Panel subtree in either; borrowed, nullable).
// Content = upper board (UI canvas); scene = bottom board (backdrop/world).
// A null scene pane clears the bottom layer transparent. The Window holds
// zero Panels (the Window Decoupling Law) — the compositor resolves both
// roots from the Frame alone.
void Frame_setContentPane(Frame *frame, Panel *panel);
void Frame_setScenePane(Frame *frame, Panel *panel);
void Frame_setVisualEffect(Frame *frame, bool enable, int material);
void Frame_setPresentsWithTransaction(Frame *frame, bool presentsWithTransaction);
void Frame_setNativeView(Frame *frame, void *nativeView);

// Frame Function slots (composable present callbacks — replaces the single
// onRender hook). Frame_addFrameFunction returns the slot index, or
// UINT32_MAX on failure (null fn / master arena unavailable);
// Frame_removeFrameFunction swap-removes (the last slot takes the hole).
uint32_t Frame_addFrameFunction(Frame *frame, void (*fn)(Frame *frame, double dt, void *userData), void *userData);
bool Frame_removeFrameFunction(Frame *frame, uint32_t index);
uint32_t Frame_getFrameFunctionCount(const Frame *frame);

// Input bindings (KeyMap-backed). Combos are int64 compositions of
// KMOD_*/KMODE_*/KEY_* (or MOUSE_*) constants — see input/key_map.h for
// the KeyMap_buildCombo* builders. The frame resolves its KeyMap once per
// Frame_render and fires at most one binding per present (the
// Present-On-Demand Law), consuming the source tap before the callback.
bool Frame_addKeyFunction(Frame *frame, int64_t combo, KeyBindingFn fn, void *userData);
bool Frame_addMouseFunction(Frame *frame, int64_t combo, KeyBindingFn fn, void *userData);
bool Frame_removeFunction(Frame *frame, int64_t combo, KeyBindingFn fn);
KeyMap *Frame_getKeyMap(const Frame *frame);

// Window Forwarding Setters:
void Frame_setTitle(Frame *frame, const char *title);
void Frame_setSize(Frame *frame, int width, int height);
void Frame_setLocation(Frame *frame, int x, int y);
void Frame_center(Frame *frame);
void Frame_show(Frame *frame);
void Frame_hide(Frame *frame);
void Frame_setVisible(Frame *frame, bool visible);
void Frame_bringToFront(Frame *frame);
void Frame_setUndecorated(Frame *frame, int type);
void Frame_setDecorated(Frame *frame, int flag);
void Frame_setNaked(Frame *frame, bool naked);
void Frame_setBorderless(Frame *frame, bool borderless);
void Frame_setFloatingTrafficLights(Frame *frame, bool floating);
// macOS-only convenience: show/hide all three traffic lights (close, minimize,
// zoom) at once — forwards to Window_macOS_setTrafficLightButtonVisible.
void Frame_macos_setTrafficLightVisible(Frame *frame, bool visible);
void Frame_setBlur(Frame *frame, float blur);
void Frame_setOpacity(Frame *frame, float opacity);
void Frame_setTransparent(Frame *frame, bool transparent);
void Frame_setTransparentBackground(Frame *frame, bool transparent);
void Frame_setAlwaysOnTop(Frame *frame, bool onTop);
void Frame_setClickThrough(Frame *frame, bool clickThrough);
void Frame_setShadow(Frame *frame, bool shadow);
void Frame_setMovableByBackground(Frame *frame, bool movable);
void Frame_minimize(Frame *frame);
void Frame_restore(Frame *frame);
void Frame_setFullscreen(Frame *frame, bool fullscreen);
void Frame_toggleFullscreen(Frame *frame);
void Frame_setMinSize(Frame *frame, int width, int height);
void Frame_setMaxSize(Frame *frame, int width, int height);
void Frame_setResizable(Frame *frame, bool resizable);
void Frame_setClosable(Frame *frame, bool closable);
void Frame_setMiniaturizable(Frame *frame, bool miniaturizable);
void Frame_focus(Frame *frame);
void Frame_setCursorType(Frame *frame, WindowCursorType type);
void Frame_setCursorLocked(Frame *frame, bool locked);

// Getters:
Window *Frame_getWindow(const Frame *frame);
Window *Frame_window(const Frame *frame);
Application *Frame_getApplication(const Frame *frame);
Application *Frame_application(const Frame *frame);
void *Frame_getGraphics(const Frame *frame);
Panel *Frame_getRootPanel(const Frame *frame);
Panel *Frame_getContentPane(const Frame *frame);
Panel *Frame_getScenePane(const Frame *frame);
uint32_t Frame_getLayerCount(const Frame *frame);
FrameLayer *Frame_getLayer(Frame *frame, uint32_t index);
bool Frame_hasVisualEffect(const Frame *frame);
int Frame_getVisualEffectMaterial(const Frame *frame);
bool Frame_isPresentsWithTransaction(const Frame *frame);
void Frame_getSize(const Frame *frame, int *outWidth, int *outHeight);
int Frame_getWidth(const Frame *frame);
int Frame_getHeight(const Frame *frame);
int Frame_width(const Frame *frame);
int Frame_height(const Frame *frame);
bool Frame_isInLiveResize(const Frame *frame);
bool Frame_isMinimized(const Frame *frame);
bool Frame_isZoomed(const Frame *frame);
void *Frame_getNativeView(const Frame *frame);

// Window Forwarding Getters:
const char *Frame_getTitle(const Frame *frame);
const char *Frame_title(const Frame *frame);
void Frame_getLocation(const Frame *frame, int *outX, int *outY);
void Frame_getContentOrigin(const Frame *frame, int *outX, int *outY);
bool Frame_isVisible(const Frame *frame);
bool Frame_isFullscreen(const Frame *frame);
bool Frame_isResizable(const Frame *frame);
bool Frame_isClosable(const Frame *frame);
bool Frame_isMiniaturizable(const Frame *frame);
bool Frame_isFocused(const Frame *frame);
bool Frame_isTransparent(const Frame *frame);
int  Frame_getDecorated(const Frame *frame);
bool Frame_isDecorated(const Frame *frame);
bool Frame_isNaked(const Frame *frame);
bool Frame_isBorderless(const Frame *frame);
WindowCursorType Frame_getCursorType(const Frame *frame);
bool Frame_shouldClose(const Frame *frame);

// Dialog Hierarchy & Closing Policy:
bool Frame_addChildDialog(Frame *frame, Dialog *dialog);
bool Frame_removeChildDialog(Frame *frame, Dialog *dialog);
uint32_t Frame_getChildDialogCount(const Frame *frame);
Dialog *Frame_getChildDialog(const Frame *frame, uint32_t index);
Dialog *Frame_getActiveClingingDialog(const Frame *frame);
bool Frame_canClose(const Frame *frame);
void Frame_closeChildDialogs(Frame *frame);
void Frame_close(Frame *frame);
void Frame_setOnQuitRequested(Frame *frame, bool (*onQuitRequested)(Frame *frame, void *userData), void *userData);

// Event Adapters & Lifecycle:
WindowEvent *Frame_getLifecycle(Frame *frame);
void Frame_addKeyAdapter(Frame *frame, const KeyHandler *adapter);
bool Frame_removeKeyAdapter(Frame *frame, const KeyHandler *adapter);
void Frame_addMouseAdapter(Frame *frame, const MouseHandler *adapter);
bool Frame_removeMouseAdapter(Frame *frame, const MouseHandler *adapter);
void Frame_addTouchAdapter(Frame *frame, const TouchHandler *adapter);
bool Frame_removeTouchAdapter(Frame *frame, const TouchHandler *adapter);

#ifdef __cplusplus
}
#endif

#endif // DARLING_FRAME_H
