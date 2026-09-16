#ifndef DARLING_FRAME_H
#define DARLING_FRAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "c23/constructor.h"
#include "window/window.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DARLING_FRAME_MAX_LAYERS 8
#define DARLING_FRAME_MAX_DIALOGS 16

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

typedef struct Frame {
    Window *window;             // R1 host window pointer
    Application *application;   // R1 host application manifest pointer (nullable)
    void *graphics;             // R3 GPU graphics context (VkHotContext / Device)
    Panel *rootPanel;           // Root UI component tree
    char *title;                // Owned title string (strdup on set)
    bool visible;               // Visibility state flag
    int chromeMode;             // FrameChromeMode (FRAME_DECORATED / BORDERLESS / NAKED)
    FrameLayer layers[DARLING_FRAME_MAX_LAYERS]; // Stacked FBOs inside CAMetalLayer
    uint32_t layerCount;

    Dialog *childDialogs[DARLING_FRAME_MAX_DIALOGS]; // Managed child dialogs
    uint32_t childDialogCount;
    Dialog *ownerDialog;        // Owning Dialog instance if embedded in a Dialog

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

    void (*onRender)(struct Frame *frame, void *userData);
    void *userData;

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
void Frame_setVisualEffect(Frame *frame, bool enable, int material);
void Frame_setPresentsWithTransaction(Frame *frame, bool presentsWithTransaction);
void Frame_setOnRender(Frame *frame, void (*onRender)(Frame *frame, void *userData), void *userData);
void Frame_setNativeView(Frame *frame, void *nativeView);

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
