#ifndef DARLING_FRAME_H
#define DARLING_FRAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "c23/constructor.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DARLING_FRAME_MAX_LAYERS 8

typedef struct Window Window;
typedef struct Panel Panel;
typedef struct Application Application;

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
    FrameLayer layers[DARLING_FRAME_MAX_LAYERS]; // Stacked FBOs inside CAMetalLayer
    uint32_t layerCount;

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
bool Frame_isInLiveResize(const Frame *frame);
bool Frame_isMinimized(const Frame *frame);
bool Frame_isZoomed(const Frame *frame);
void *Frame_getNativeView(const Frame *frame);

#ifdef __cplusplus
}
#endif

#endif // DARLING_FRAME_H
