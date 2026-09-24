// demo/scroll_demo.c — visual proof for nested ScrollPanels (headless).
//
// DEMO entry over the shared ScrollScene: scripts five scroll frames —
// rest, outer scroll, chained inner-then-outer bubble, idle (overlay bars
// hide), both-at-end — and dumps one BMP per frame into argv[1]. Pure
// raster row, no window, no GPU: the same pixels a window would blit.
//
//   scroll_demo /tmp/scroll_frames
//
// The live-window twin lives in the umbrella probe (main/, SCROLL_PROBE=1).

#include "demo/scroll_scene.h"

#include "lang/graphics.h"
#include "lang/image.h"
#include "raster/raster_graphics.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define DEMO_FB_W 480
#define DEMO_FB_H 360

static bool dumpBMP(const char *path, const uint8_t *rgba, int width, int height) {
    if (!path || !rgba || width <= 0 || height <= 0)
        return false;
    FILE *f = fopen(path, "wb");
    if (!f)
        return false;
    uint32_t stride = ((uint32_t) width * 3u + 3u) & ~3u;
    uint32_t imageSize = stride * (uint32_t) height;
    uint32_t fileSize = 54u + imageSize;
    uint8_t hdr[54] = { 0 };
    hdr[0] = 'B';
    hdr[1] = 'M';
    hdr[2] = (uint8_t) fileSize;
    hdr[3] = (uint8_t) (fileSize >> 8);
    hdr[4] = (uint8_t) (fileSize >> 16);
    hdr[5] = (uint8_t) (fileSize >> 24);
    hdr[10] = 54;
    hdr[14] = 40;
    hdr[18] = (uint8_t) width;
    hdr[19] = (uint8_t) (width >> 8);
    hdr[20] = (uint8_t) (width >> 16);
    hdr[21] = (uint8_t) (width >> 24);
    hdr[22] = (uint8_t) height;
    hdr[23] = (uint8_t) (height >> 8);
    hdr[24] = (uint8_t) (height >> 16);
    hdr[25] = (uint8_t) (height >> 24);
    hdr[26] = 1;
    hdr[28] = 24;
    hdr[34] = (uint8_t) imageSize;
    hdr[35] = (uint8_t) (imageSize >> 8);
    hdr[36] = (uint8_t) (imageSize >> 16);
    hdr[37] = (uint8_t) (imageSize >> 24);
    bool ok = fwrite(hdr, 1, sizeof(hdr), f) == sizeof(hdr);
    uint8_t pad[3] = { 0, 0, 0 };
    for (int y = height - 1; ok && y >= 0; y--) {
        for (int x = 0; ok && x < width; x++) {
            const uint8_t *p = rgba + ((size_t) y * (size_t) width + (size_t) x) * 4u;
            uint8_t bgr[3] = { p[2], p[1], p[0] };
            ok = fwrite(bgr, 1, 3, f) == 3;
        }
        uint32_t rem = stride - (uint32_t) width * 3u;
        if (ok && rem > 0)
            ok = fwrite(pad, 1, rem, f) == rem;
    }
    fclose(f);
    return ok;
}

static void dumpFrame(const char *outdir, const char *name) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s.bmp", outdir, name);
    Image *fb = RasterGraphics_getFramebuffer();
    if (fb)
        dumpBMP(path, Image_pixels(fb), DEMO_FB_W, DEMO_FB_H);
    printf("frame %s\n", path);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: scroll_demo <outdir>\n");
        return 1;
    }
    const char *outdir = argv[1];
    if (!Graphics_registerRow(RasterGraphics_getRow()))
        return 1;
    Graphics_setGraphics(LANG_BACKEND_RASTER);

    ScrollScene_build();
    // f0: rest, classic bars.
    ScrollScene_paint(DEMO_FB_W, DEMO_FB_H);
    dumpFrame(outdir, "f0_rest");
    // f1: outer scrolled +180.
    ScrollScene_scrollOuter(180.0f, 1000u);
    ScrollScene_paint(DEMO_FB_W, DEMO_FB_H);
    dumpFrame(outdir, "f1_outer180");
    // f2: overlay on, chained inner scroll bubbles to the outer.
    ScrollScene_setOverlay(true);
    ScrollScene_scrollChained(300.0f, 2000u);
    ScrollScene_paint(DEMO_FB_W, DEMO_FB_H);
    dumpFrame(outdir, "f2_chained");
    // f3: idle — overlay bars hide, content stays.
    ScrollScene_tick(4000u);
    ScrollScene_paint(DEMO_FB_W, DEMO_FB_H);
    dumpFrame(outdir, "f3_idle");
    // f4: both at end.
    ScrollScene_toEnd(5000u);
    ScrollScene_paint(DEMO_FB_W, DEMO_FB_H);
    dumpFrame(outdir, "f4_end");
    // f5/f6: smooth momentum — an input of 120px lands, then glides past it
    // (friction 2 = a longer stop) as the clock advances. Step mode would
    // sit still after the input.
    ScrollScene_setOffsets(0.0f, 0.0f, 6000u);
    ScrollScene_setSmooth(true, 2.0f);
    ScrollScene_scrollInput(120.0f, 6100u);
    ScrollScene_paint(DEMO_FB_W, DEMO_FB_H);
    dumpFrame(outdir, "f5_momentum_input");
    ScrollScene_tick(6200u);
    ScrollScene_tick(6350u);
    ScrollScene_paint(DEMO_FB_W, DEMO_FB_H);
    dumpFrame(outdir, "f6_momentum_glide");

    ScrollScene_free();
    RasterGraphics_shutdown();
    return 0;
}
