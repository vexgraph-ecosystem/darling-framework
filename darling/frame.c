#include "darling/frame.h"

#include <stdlib.h>
#include <string.h>

#include "annotation/definition.h"
#include "annotation/overview.h"
#include "annotation/getter.h"

#include "lang/graphics.h"
#include "vulkan/vk_device.h"
#include "vulkan/vk_graphics.h"

// Dialect-internal platform hooks: objc/frame_cocoa.m on Apple, frame_stub.c
// elsewhere. frameShow builds the NSWindow -> NSVisualEffectView -> CAMetalLayer
// hierarchy and fills the frame's Surface/Device/window; frameFree tears down.
bool Frame_platformShow(Frame *frame, int width, int height, const char *title);
void Frame_platformFree(Frame *frame);

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Frame
 * ============================================================================
 * The host frame (R4): the window, its one seam, and its boards, assembled into
 * one object. It owns a VisualEffect (the blur chrome), a Device, a Surface
 * (the seam), and two Boards (scene bottom, content top); it registers as a
 * client of the demand loop so window-state/input/content/board-generation
 * changes wake exactly one present (the Present-On-Demand Law).
 *
 * Frame_present is the loop's present hook: composite the boards, draw the UI
 * through the Graphics row, present the seam. Offscreen (no window) the frame
 * renders into its Device target and the render IS the frame.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Frame (darling/frame.c)
 * ============================================================================
 * STRUCT FIELDS:
 *   VisualEffect *vfx;      // owned blur chrome (behind the seam)
 *   Device *device;         // owned, offscreen until show
 *   Surface *surface;       // owned seam (null while offscreen)
 *   Board *content;         // owned top board (nullable)
 *   Board *scene;           // owned bottom board (nullable)
 *   GraphicsLoop *loop;     // borrowed demand loop
 *   void *window;           // borrowed platform window (null while offscreen)
 *   int width, height;      // window size in logical points
 *   char title[256];        // window title
 *   GraphicsFrameFn draw; void *drawCtx;   // the UI draw callback
 *   bool valid;             // device came up
 * FUNCTION REGISTRY:
 *   Public Constructors: Frame_0
 *   Public Core: Frame_free, Frame_isValid, Frame_setSize, Frame_setTitle,
 *                Frame_show, Frame_setBoards, Frame_setDrawFn, Frame_present,
 *                Frame_markDirty, Frame_setSurface, Frame_setDevice,
 *                Frame_setPlatformWindow
 *   Public Getters: Frame_getWidth, Frame_getHeight, Frame_getContentBoard,
 *                   Frame_getSceneBoard, Frame_getSurface, Frame_getDevice,
 *                   Frame_getVisualEffect, Frame_getLoop
 * ============================================================================
 */

struct Frame {
    VisualEffect *vfx;      // owned blur chrome (behind the seam)
    Device *device;         // owned, offscreen until show
    Surface *surface;       // owned seam (null while offscreen)
    Board *content;         // owned top board (nullable)
    Board *scene;           // owned bottom board (nullable)
    GraphicsLoop *loop;     // borrowed demand loop
    void *window;           // borrowed platform window (null while offscreen)
    int width;              // window size in logical points
    int height;
    char title[256];        // window title
    GraphicsFrameFn draw;   // the UI draw callback (inside the present hook)
    void *drawCtx;
    bool valid;             // the device came up
};

// The demand loop's present hook: forward to Frame_present.
static bool framePresentFn(void *window, double dt, void *userdata) {
    (void) window;
    (void) dt;
    return Frame_present((Frame*) userdata);
}

static void frameRefreshClient(Frame *frame) {
    GraphicsClient *client = GraphicsLoop_findClient((*frame).loop, frame);
    if (client == nullptr)
        return;
    (*client).content = (*frame).content;
    (*client).scene = (*frame).scene;
    (*client).presentFn = framePresentFn;
    (*client).userdata = frame;
}

// Bind this frame's device to the process Graphics row and ensure its target
// (the seam) exists at the device's native-pixel extent, so Boards can render
// and the present hook has a seam to composite into. Cold path.
static void frameEnsureTarget(Frame *frame) {
    if (frame == nullptr || (*frame).device == nullptr)
        return;
    if (!VkGraphics_bind((*frame).device))
        return;
    (void) Graphics_registerRow(VkGraphics_getRow());
    (void) Graphics_setGraphics(LANG_BACKEND_VULKAN);
    int w = (int) Device_width((*frame).device);
    int h = (int) Device_height((*frame).device);
    if (w > 0 && h > 0)
        (void) Graphics_resize((uint32_t) w, (uint32_t) h);
}

// CONSTRUCTORS (PUBLIC & PRIVATE)

Frame *Frame_0(void) {
    Frame *frame = (Frame*) calloc(1, sizeof(Frame));
    if (frame == nullptr)
        return nullptr;
    (*frame).vfx = VisualEffect_0();
    (*frame).device = Device_new(&(DeviceDesc){ .backend = LANG_BACKEND_VULKAN });
    (*frame).loop = GraphicsLoop_default();
    (*frame).width = 800;
    (*frame).height = 600;
    const char *title = "darling frame";
    memcpy((*frame).title, title, strlen(title) + 1u);
    if ((*frame).device != nullptr)
        Device_resize((*frame).device, (uint32_t) (*frame).width, (uint32_t) (*frame).height);
    (*frame).valid = (*frame).device != nullptr;
    frameEnsureTarget(frame);
    GraphicsClient client = { .window = frame, .content = nullptr, .scene = nullptr,
                              .frameFn = nullptr, .presentFn = framePresentFn,
                              .userdata = frame };
    if ((*frame).loop != nullptr)
        GraphicsLoop_addClient((*frame).loop, &client);
    return frame;
}

void Frame_free(Frame *frame) {
    if (frame == nullptr)
        return;
    if ((*frame).loop != nullptr)
        GraphicsLoop_removeClient((*frame).loop, frame);
    Frame_platformFree(frame);
    if ((*frame).content != nullptr)
        Board_destroy((*frame).content);
    if ((*frame).scene != nullptr)
        Board_destroy((*frame).scene);
    if ((*frame).surface != nullptr)
        Surface_destroy((*frame).surface);
    if ((*frame).device != nullptr)
        Device_destroy((*frame).device);
    VisualEffect_free((*frame).vfx);
    free(frame);
}

;;GETTER
bool Frame_isValid(const Frame *frame) {
    return frame != nullptr && (*frame).valid;
}

// WINDOW (PUBLIC & PRIVATE)

void Frame_setSize(Frame *frame, int width, int height) {
    if (frame == nullptr || width <= 0 || height <= 0)
        return;
    (*frame).width = width;
    (*frame).height = height;
    // Offscreen the points ARE the pixels; a shown frame's platform layer
    // overrides this with the native-pixel extent on resize.
    if ((*frame).device != nullptr && (*frame).window == nullptr) {
        Device_resize((*frame).device, (uint32_t) width, (uint32_t) height);
        frameEnsureTarget(frame);
    }
    Frame_markDirty(frame);
}

void Frame_setTitle(Frame *frame, const char *title) {
    if (frame == nullptr || title == nullptr)
        return;
    size_t n = strlen(title);
    if (n >= sizeof((*frame).title))
        n = sizeof((*frame).title) - 1u;
    memcpy((*frame).title, title, n);
    (*frame).title[n] = '\0';
}

;;GETTER
int Frame_getWidth(const Frame *frame) {
    return frame ? (*frame).width : 0;
}

;;GETTER
int Frame_getHeight(const Frame *frame) {
    return frame ? (*frame).height : 0;
}

bool Frame_show(Frame *frame) {
    if (frame == nullptr || (*frame).device == nullptr || (*frame).window != nullptr)
        return false;
    bool shown = Frame_platformShow(frame, (*frame).width, (*frame).height, (*frame).title);
    if (shown)
        Frame_markDirty(frame);
    return shown;
}

// BOARDS (PUBLIC & PRIVATE)

void Frame_setBoards(Frame *frame, Board *content, Board *scene) {
    if (frame == nullptr)
        return;
    (*frame).content = content;
    (*frame).scene = scene;
    frameRefreshClient(frame);
    Frame_markDirty(frame);
}

;;GETTER
Board *Frame_getContentBoard(const Frame *frame) {
    return frame ? (*frame).content : nullptr;
}

;;GETTER
Board *Frame_getSceneBoard(const Frame *frame) {
    return frame ? (*frame).scene : nullptr;
}

// DRAW CALLBACK (PUBLIC)

void Frame_setDrawFn(Frame *frame, GraphicsFrameFn draw, void *userdata) {
    if (frame == nullptr)
        return;
    (*frame).draw = draw;
    (*frame).drawCtx = userdata;
}

// THE PRESENT HOOK (PUBLIC)

bool Frame_present(Frame *frame) {
    if (frame == nullptr || (*frame).device == nullptr)
        return false;
    frameEnsureTarget(frame);
    int w = (int) Device_width((*frame).device);
    int h = (int) Device_height((*frame).device);
    if (w <= 0 || h <= 0)
        return false;
    if (!Graphics_begin())
        return false;
    if (!Graphics_clear(0x1E1E24FFu)) {
        Graphics_end();
        return false;
    }
    Rectangle full = { 0.0f, 0.0f, (float) w, (float) h };
    if ((*frame).scene != nullptr)
        VkGraphics_drawBoard((*frame).scene, &full);
    if ((*frame).content != nullptr)
        VkGraphics_drawBoard((*frame).content, &full);
    if ((*frame).draw != nullptr)
        (*frame).draw((*frame).drawCtx, 0.0);
    if (!Graphics_end())
        return false;
    bool presented = Graphics_present();
    // Offscreen there is no seam to present: the completed render IS the frame.
    return presented || (*frame).surface == nullptr;
}

void Frame_markDirty(Frame *frame) {
    if (frame != nullptr && (*frame).loop != nullptr)
        GraphicsLoop_markDirty((*frame).loop, frame);
}

// DIALECT-INTERNAL PLATFORM SETTERS (frame_cocoa.m / frame_stub.c use these)

void Frame_setSurface(Frame *frame, Surface *surface) {
    if (frame != nullptr)
        (*frame).surface = surface;
}

void Frame_setDevice(Frame *frame, Device *device) {
    if (frame == nullptr || device == nullptr)
        return;
    // A windowed device replaces the offscreen one (the platform owns the swap).
    (*frame).device = device;
    (*frame).valid = true;
}

void Frame_setPlatformWindow(Frame *frame, void *window) {
    if (frame != nullptr)
        (*frame).window = window;
}

// GETTERS (PUBLIC & PRIVATE)

;;GETTER
Surface *Frame_getSurface(const Frame *frame) {
    return frame ? (*frame).surface : nullptr;
}

;;GETTER
Device *Frame_getDevice(const Frame *frame) {
    return frame ? (*frame).device : nullptr;
}

;;GETTER
VisualEffect *Frame_getVisualEffect(const Frame *frame) {
    return frame ? (*frame).vfx : nullptr;
}

;;GETTER
GraphicsLoop *Frame_getLoop(const Frame *frame) {
    return frame ? (*frame).loop : nullptr;
}
