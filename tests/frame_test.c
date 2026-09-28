// tests/frame_test.c — the Frame assembly, headless (offscreen).
//
// Proves Frame_0 assembles the two-loop model: a Device + two Boards + the
// demand loop client; Frame_present composites scene -> content -> the UI draw
// fn into the seam target, in order. Skips (77) without a Vulkan loader.

#include "darling/frame.h"
#include "lang/board.h"
#include "lang/graphics.h"
#include "vulkan/vk_device.h"
#include "vulkan/vk_graphics.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

static bool hasLoader(void) {
#ifdef _WIN32
    HMODULE lib = LoadLibraryA("vulkan-1.dll");
    if (lib == nullptr)
        return false;
    FreeLibrary(lib);
    return true;
#elif defined(__APPLE__)
    const char *paths[] = { "libMoltenVK.dylib", "/opt/homebrew/lib/libMoltenVK.dylib",
        "/usr/local/lib/libMoltenVK.dylib", "libvulkan.dylib",
        "/opt/homebrew/lib/libvulkan.dylib", "/usr/local/lib/libvulkan.dylib", nullptr };
#else
    const char *paths[] = { "libvulkan.so.1", "libvulkan.so", nullptr };
#endif
    for (size_t i = 0; paths[i] != nullptr; i++) {
        void *lib = dlopen(paths[i], RTLD_NOW | RTLD_LOCAL);
        if (lib != nullptr) {
            dlclose(lib);
            return true;
        }
    }
    return false;
}

enum { W = 8, H = 8, PIXELS = W * H * 4 };

static void renderBoard(Board *board, const Rectangle *rect, uint32_t color) {
    Brush brush = { color, 1.0f };
    assert(VkGraphics_beginTarget(board));
    assert(Graphics_begin());
    assert(Graphics_clear(0x00000000u));
    assert(Graphics_fillRect(rect, &brush));
    assert(Graphics_end());
    VkGraphics_endTarget();
}

static void drawMarker(void *userdata, double dt) {
    (void) dt;
    Board *tag = (Board*) userdata;   // unused; the draw fn paints through the row
    (void) tag;
    Rectangle marker = { W - 1, 0, 1, 1 };
    Brush yellow = { 0xFFFF00FFu, 1.0f };
    assert(Graphics_fillRect(&marker, &yellow));
}

static void expect(const uint8_t *px, int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    const uint8_t *p = px + ((size_t) y * W + (size_t) x) * 4u;
    if (p[0] != r || p[1] != g || p[2] != b) {
        fprintf(stderr, "pixel(%d,%d)=%u,%u,%u expected %u,%u,%u\n", x, y, p[0], p[1], p[2], r, g, b);
        assert(0);
    }
}

int main(void) {
    if (!hasLoader()) {
        fprintf(stderr, "Vulkan loader unavailable; skip\n");
        return 77;
    }
    assert(Vulkan_registerRows());

    Frame *frame = Frame_0();
    assert(frame != nullptr && Frame_isValid(frame));
    assert(Frame_getDevice(frame) != nullptr);
    assert(Frame_getVisualEffect(frame) != nullptr);
    assert(Frame_getLoop(frame) != nullptr);
    assert(Frame_getSurface(frame) == nullptr);          // offscreen
    assert(GraphicsLoop_clientCount(Frame_getLoop(frame)) == 1);

    Frame_setTitle(frame, "darling frame test");
    Frame_setSize(frame, W, H);

    Board *scene = Board_3(LANG_BACKEND_VULKAN, W, H);
    Board *content = Board_3(LANG_BACKEND_VULKAN, W, H);
    assert(scene != nullptr && content != nullptr);
    Rectangle full = { 0, 0, W, H };
    Rectangle leftHalf = { 0, 0, W / 2, H };
    renderBoard(scene, &full, 0x00FF00FFu);          // green
    renderBoard(content, &leftHalf, 0x0000FFFFu);    // blue left, clear elsewhere
    Board_publish(scene);
    Board_publish(content);
    Frame_setBoards(frame, content, scene);
    assert(Frame_getContentBoard(frame) == content && Frame_getSceneBoard(frame) == scene);
    Frame_setDrawFn(frame, drawMarker, nullptr);

    // The present hook composites boards then the UI draw fn into the seam target.
    assert(Frame_present(frame));
    uint8_t px[PIXELS] = {0};
    assert(VkGraphics_readback(sizeof(px), px));
    expect(px, 1, 1, 0x00, 0x00, 0xFF);   // content (blue) over scene
    expect(px, 6, 6, 0x00, 0xFF, 0x00);   // scene through content's transparency
    expect(px, W - 1, 0, 0xFF, 0xFF, 0x00);   // the UI marker over both

    Frame_free(frame);   // owns the boards
    puts("frame assembly OK");
    return 0;
}
