#include "frame/frame.h"

#include <stdlib.h>
#include <string.h>

#include "image.h"                   // graphvex R3
#include "vulkan/surface.h"          // graphvex R3: the present seam
#include "vulkan/vulkan_backend.h"   // graphvex R3: the GPU backend

// darling R4 — frame.c
// The JFrame: a hotcwap window + owned child Panels, GPU-rendered. Frames form
// an ownership tree — closing an owner closes its children.
//
// frame.c is PURE WINDOW + LAYOUT + PAINT. It knows nothing about the mouse;
// input lives in darling/input (see the close hook below, the only seam).

struct Frame {
    Window *window;
    Color background;
    Element *root;       // the content element (the Frame's GraphicsPanel)
    Panel **panels;      // the Panel wrappers (interface), aligned with root's children
    int count, cap;
    Surface *surface;    // the present seam: capture target + host blit (fallback)
    void *surfaces[2];   // borrowed IOSurfaces (Apple zero-copy seam, double buffer)
    int targets[2];      // graphvex surface slots for those IOSurfaces
    int front;           // which surface the layer currently shows
    bool gpu;            // the zero-copy present path is live
    Image *shot;         // capture readback buffer (GPU path only, lazy)
    DisplayList *dl;     // reused paint list
    int lastW, lastH;    // the single size authority

    struct Frame *owner;
    struct Frame **children;
    int childCount, childCap;

    bool closed;
    bool dirty;          // needs a repaint

    FrameCloseFn *closeFns;
    void **closeUd;
    int closeCount, closeCap;
};

// ── the live-frame set (the runner + ownership) ─────────────────────────────
static Frame **s_live = NULL;
static int s_liveCount = 0, s_liveCap = 0;
static Frame *s_active = NULL;   // most recently created (CAPTURE)

static void live_add(Frame *f) {
    if (s_liveCount == s_liveCap) {
        s_liveCap = s_liveCap ? s_liveCap * 2 : 4;
        s_live = realloc(s_live, (size_t)s_liveCap * sizeof *s_live);
    }
    s_live[s_liveCount++] = f;
}

static void live_remove(Frame *f) {
    for (int i = 0; i < s_liveCount; i++) {
        if (s_live[i] != f) continue;
        memmove(&s_live[i], &s_live[i + 1], (size_t)(s_liveCount - i - 1) * sizeof *s_live);
        s_liveCount--;
        return;
    }
}

static void child_add(Frame *owner, Frame *child) {
    if ((*owner).childCount == (*owner).childCap) {
        (*owner).childCap = (*owner).childCap ? (*owner).childCap * 2 : 4;
        (*owner).children = realloc((*owner).children, (size_t)((*owner).childCap) * sizeof *((*owner).children));
    }
    (*owner).children[(*owner).childCount++] = child;
}

static void child_remove(Frame *owner, Frame *child) {
    for (int i = 0; i < (*owner).childCount; i++) {
        if ((*owner).children[i] != child) continue;
        memmove(&(*owner).children[i], &(*owner).children[i + 1],
                (size_t)((*owner).childCount - i - 1) * sizeof *(*owner).children);
        (*owner).childCount--;
        return;
    }
}

static void fire_close_hooks(Frame *f) {
    for (int i = 0; i < (*f).closeCount; i++) (*f).closeFns[i](f, (*f).closeUd[i]);
    (*f).closeCount = 0;
}

// ── construction ────────────────────────────────────────────────────────────
static void frame_on_resize(void *userdata) {
    Frame *f = (Frame *)userdata;
    Frame_setSize(f, Window_width((*f).window), Window_height((*f).window));
}

// The host blit (fallback path): hand the Surface's present Image to the R1
// window as an RGBA buffer. Used off-Apple or when the GPU seam is unavailable.
static bool frame_on_present(Surface *surface, void *userdata) {
    Frame *f = (Frame *)userdata;
    Image *img = Surface_presentImage(surface);
    if (!f || !img || !(*f).window) return false;
    Window_presentRGBA((*f).window, Image_pixels(img), Image_stride(img),
                       (int)Image_width(img), (int)Image_height(img));
    return true;
}

// The zero-copy seam: two host IOSurfaces imported as GPU targets. The back one
// is rendered into, published to the layer, then swapped. Fails (returns false)
// off-Apple, when VK_EXT_metal_objects is absent, or on OOM — the Frame then
// falls back to the RGBA path.
static bool frame_gpu_open(Frame *f, int wpx, int hpx) {
    for (int i = 0; i < 2; i++)
        (*f).surfaces[i] = Window_createPresentSurface((*f).window, wpx, hpx);
    if (!(*f).surfaces[0] || !(*f).surfaces[1]) return false;
    for (int i = 0; i < 2; i++) {
        (*f).targets[i] = VulkanBackend_addSurface((*f).surfaces[i], (uint32_t)wpx, (uint32_t)hpx);
        if ((*f).targets[i] < 0) return false;
    }
    (*f).front = 0;
    (*f).gpu = true;
    return true;
}

// Tear the GPU seam down (idempotent; safe to call after a partial open).
static void frame_gpu_close(Frame *f) {
    for (int i = 0; i < 2; i++) {
        if ((*f).targets[i] >= 0) VulkanBackend_removeSurface((*f).targets[i]);
        (*f).targets[i] = -1;
        if ((*f).surfaces[i]) Window_destroyPresentSurface((*f).window, (*f).surfaces[i]);
        (*f).surfaces[i] = NULL;
    }
    (*f).gpu = false;
}

// Resize = an IOSurface cannot grow, so rebuild both at the new extent.
static void frame_gpu_reopen(Frame *f, int wpx, int hpx) {
    frame_gpu_close(f);
    if (!frame_gpu_open(f, wpx, hpx))
        frame_gpu_close(f);   // leave gpu false; the RGBA fallback carries on
}

Frame *Frame_0(void) { return Frame_3("darling", 800, 600); }
Frame *Frame_1(const char *title) { return Frame_3(title, 800, 600); }

Frame *Frame_3(const char *title, int widthPx, int heightPx) {
    Graphics_register(VulkanBackend_row());
    Graphics_use(BACKEND_VULKAN);

    int wpx = widthPx > 0 ? widthPx : 800;
    int hpx = heightPx > 0 ? heightPx : 600;
    Frame *f = calloc(1, sizeof *f);
    if (!f) return NULL;
    (*f).targets[0] = (*f).targets[1] = -1;
    (*f).window = Window_create(title ? title : "darling", wpx, hpx);
    if (!(*f).window) { free(f); return NULL; }
    // The present seam: its retained Image is the capture target, and its host
    // blit publishes to the window. The borrowed native is the content view.
    (*f).surface = Surface_2(Window_contentView((*f).window), (uint32_t)wpx, (uint32_t)hpx);
    if (!(*f).surface) { Window_destroy((*f).window); free(f); return NULL; }
    Surface_onPresent((*f).surface, frame_on_present, f);
    // Prefer the zero-copy GPU seam; it falls back silently when unavailable.
    if (!frame_gpu_open(f, wpx, hpx)) frame_gpu_close(f);
    // OPAQUE by default. Background alpha controls paint only; OS window
    // transparency is independent and requires Frame_setTransparent(true).
    (*f).background = COLOR_RGBA(18, 20, 28, 255);
    (*f).root = Element();   // the window's content element (transparent)
    Element_setBackground((*f).root, COLOR_CLEAR);
    Element_setSize((*f).root, (float)Window_width((*f).window), (float)Window_height((*f).window));
    (*f).dl = DisplayList_0();
    Window_setResizeRenderHook((*f).window, frame_on_resize, f);
    live_add(f);
    s_active = f;
    return f;
}

// the single teardown: closes children (deepest-first), drops registrations,
// frees the element tree, the window and the frame.
void Frame_destroy(Frame *frame) {
    if (!frame) return;
    (*frame).closed = true;
    while ((*frame).childCount > 0) Frame_close((*frame).children[(*frame).childCount - 1]);
    if ((*frame).owner) child_remove((*frame).owner, frame);
    live_remove(frame);
    if (s_active == frame) s_active = NULL;
    fire_close_hooks(frame);   // let input drop its registrations before we free
    Frame_removePanels(frame);
    free((*frame).panels);
    Element_destroy((*frame).root);
    free((*frame).children);
    free((*frame).closeFns);
    free((*frame).closeUd);
    frame_gpu_close(frame);
    if ((*frame).shot) Image_destroy((*frame).shot);
    Surface_destroy((*frame).surface);
    DisplayList_free((*frame).dl);
    if ((*frame).window) Window_destroy((*frame).window);
    free(frame);
}

// close this frame and every child (deepest-first), then free it
void Frame_close(Frame *f) { Frame_destroy(f); }

// ── basics ──────────────────────────────────────────────────────────────────
Window *Frame_window(const Frame *frame) { return frame ? (*frame).window : NULL; }
Surface *Frame_surface(const Frame *frame) { return frame ? (*frame).surface : NULL; }
void Frame_setTitle(Frame *frame, const char *title) {
    if (frame && (*frame).window) Window_setTitle((*frame).window, title);
}
void  Frame_setBackground(Frame *frame, Color color) {
    if (!frame) return;
    (*frame).background = color;
    // Paint alpha must not change the explicit OS window transparency policy.
}
void  Frame_setBackgroundColor(Frame *frame, Color color) { Frame_setBackground(frame, color); }
Color Frame_background(const Frame *frame) { return frame ? (*frame).background : COLOR_CLEAR; }
void  Frame_setTransparent(Frame *frame, bool transparent) {   // true = see-through
    if (frame && (*frame).window) Window_setTransparentBackground((*frame).window, transparent);
}
void  Frame_setBlur(Frame *frame, float radius) {   // frost the WINDOW backdrop
    if (frame && (*frame).window) Window_setBackdropBlur((*frame).window, radius);
}

// ── Liquid Glass (macOS-exclusive, capability-gated) ─────────────────────────
// The Frame forwards to the borrowed R1 window; the AppKit chrome lives there.
bool Frame_macOS_hasLiquidGlass(void) { return Window_macOS_hasLiquidGlass(); }

void Frame_macOS_setLiquidGlass(Frame *frame, const FrameLiquidGlassDesc *desc) {
    if (frame && (*frame).window) Window_macOS_setLiquidGlass((*frame).window, desc);
}

bool Frame_macOS_getLiquidGlass(const Frame *frame, FrameLiquidGlassDesc *out) {
    return (frame && (*frame).window) ? Window_macOS_getLiquidGlass((*frame).window, out) : false;
}

void Frame_onClose(Frame *frame, FrameCloseFn fn, void *userdata) {
    if (!frame || !fn) return;
    if ((*frame).closeCount == (*frame).closeCap) {
        int ncap = (*frame).closeCap ? (*frame).closeCap * 2 : 4;
        FrameCloseFn *gf = realloc((*frame).closeFns, (size_t)ncap * sizeof *gf);
        void **gu = realloc((*frame).closeUd, (size_t)ncap * sizeof *gu);
        if (!gf || !gu) return;
        (*frame).closeFns = gf;
        (*frame).closeUd = gu;
        (*frame).closeCap = ncap;
    }
    (*frame).closeFns[(*frame).closeCount] = fn;
    (*frame).closeUd[(*frame).closeCount] = userdata;
    (*frame).closeCount++;
}

// ── windows ─────────────────────────────────────────────────────────────────
void Frame_show(Frame *f) {
    if (!f || (*f).closed) return;
    Window_show((*f).window);
    (*f).dirty = true;
    Frame_render(f);
    (*f).dirty = false;
}

void Frame_hide(Frame *f) {
    if (f && (*f).window) Window_hide((*f).window);
}

bool Frame_isClosed(const Frame *f) { return !f || (*f).closed; }
Frame *Frame_owner(const Frame *f) { return f ? (*f).owner : NULL; }

void Frame_setOwner(Frame *f, Frame *owner) {
    if (!f || f == owner) return;
    if ((*f).owner) child_remove((*f).owner, f);
    (*f).owner = owner;
    if (owner) child_add(owner, f);
}

// close this frame and every child (deepest-first), then free it

// ── children (panels) ───────────────────────────────────────────────────────
Element *Frame_element(const Frame *frame) { return frame ? (*frame).root : NULL; }

Panel *Frame_add(Frame *frame, Panel *panel) {
    if (!frame || !(*frame).root || !panel) return NULL;
    Panel **grown = realloc((*frame).panels, (size_t)((*frame).count + 1) * sizeof *grown);
    if (!grown) { Panel_destroy(panel); return NULL; }
    (*frame).panels = grown;
    (*frame).panels[(*frame).count++] = panel;
    Element_add((*frame).root, Panel_graphics(panel));
    return panel;
}

Panel *Frame_addPanel(Frame *frame, const ElementDesc *desc) {
    return Frame_add(frame, Panel(desc));
}

int Frame_count(const Frame *frame) { return frame ? (*frame).count : 0; }

Panel *Frame_panel(const Frame *frame, int index) {
    if (!frame || index < 0 || index >= (*frame).count) return NULL;
    return (*frame).panels[index];
}

void Frame_removePanels(Frame *frame) {
    if (!frame) return;
    for (int i = 0; i < (*frame).count; i++) {
        Element *g = Panel_graphics((*frame).panels[i]);
        if (g) Element_remove(g);        // unlink from the tree before freeing it
        Panel_destroy((*frame).panels[i]);
    }
    (*frame).count = 0;
}

Rect Frame_root(const Frame *frame) {
    if (!frame) return (Rect){0, 0, 0, 0};
    // the layout root IS the window (native px); before the first render, fall
    // back to the live window size so layout/hit-testing works immediately.
    int w = (*frame).lastW, h = (*frame).lastH;
    if ((w <= 0 || h <= 0) && (*frame).window) {
        w = Window_width((*frame).window);
        h = Window_height((*frame).window);
    }
    return (Rect){0, 0, (float)w, (float)h};
}

void Frame_paint(const Frame *frame, DisplayList *dl) {
    if (!frame || !dl || !(*frame).root) return;
    Element_paint((*frame).root, Frame_root(frame), dl);
}

// ── the one resize surface ──────────────────────────────────────────────────
void Frame_setSize(Frame *frame, int widthPx, int heightPx) {
    if (!frame || widthPx <= 0 || heightPx <= 0) return;
    if (widthPx == (*frame).lastW && heightPx == (*frame).lastH) return;
    (*frame).lastW = widthPx;
    (*frame).lastH = heightPx;
    if ((*frame).root) Element_setSize((*frame).root, (float)widthPx, (float)heightPx);
    Graphics_resize((uint32_t)widthPx, (uint32_t)heightPx);
    // revalidate the present seam: an IOSurface cannot grow, so rebuild the GPU
    // double buffer; the fallback capture Image is resized in place.
    if ((*frame).gpu) frame_gpu_reopen(frame, widthPx, heightPx);
    Surface_resize((*frame).surface, (uint32_t)widthPx, (uint32_t)heightPx);
    Frame_render(frame);
}

void Frame_render(Frame *frame) {
    if (!frame || !(*frame).window || (*frame).closed) return;
    if ((*frame).lastW <= 0 || (*frame).lastH <= 0) {
        Frame_setSize(frame, Window_width((*frame).window), Window_height((*frame).window));
        return;
    }
    if ((*frame).gpu) {
        // zero-copy: render into the back IOSurface, publish it, swap front/back
        int back = 1 - (*frame).front;
        VulkanBackend_useSurface((*frame).targets[back]);
        if (!Graphics_begin()) return;
        Graphics_clear((*frame).background);
        DisplayList_clear((*frame).dl);
        Frame_paint(frame, (*frame).dl);
        Graphics_submit((*frame).dl);
        Graphics_end();
        if (Graphics_present()) {
            Window_presentSurface((*frame).window, (*frame).surfaces[back]);
            (*frame).front = back;
        }
        return;
    }
    if (!(*frame).surface) return;
    if (!Graphics_begin()) return;
    Graphics_clear((*frame).background);
    DisplayList_clear((*frame).dl);
    Frame_paint(frame, (*frame).dl);
    Graphics_submit((*frame).dl);
    Graphics_end();
    // fallback: render into the Surface's retained Image, publish via the blit
    if (Graphics_capture(Surface_presentImage((*frame).surface)))
        Surface_present((*frame).surface);
}

void Frame_invalidate(Frame *frame) { if (frame) (*frame).dirty = true; }

// the input layer works in ELEMENTS; this is how it reaches the owning Frame
void Frame_invalidateElement(Element *root) {
    if (!root) return;
    for (int i = 0; i < s_liveCount; i++)
        if ((*s_live[i]).root == root) { (*s_live[i]).dirty = true; return; }
}

// ── the runner ──────────────────────────────────────────────────────────────
void Frame_runAll(Frame *root) {
    if (!root) return;
    Frame_show(root);
    bool rootClosed = false;
    while (!rootClosed && s_liveCount > 0) {
        Window_pollEvents();   // hotcwap also dispatches input here
        for (int i = 0; i < s_liveCount; ) {
            Frame *f = s_live[i];
            if (Window_shouldClose((*f).window)) {
                if (f == root) { rootClosed = true; i++; continue; }  // caller owns root
                Frame_close(f);   // a child: close it now (freed)
                continue;
            }
            if ((*f).dirty) { Frame_render(f); (*f).dirty = false; }
            i++;
        }
        if (rootClosed) break;
        Window_waitEvents(NULL, 0);
    }
    // tear down everything root OWNS; the ROOT is the caller's to destroy
    while ((*root).childCount > 0) Frame_close((*root).children[(*root).childCount - 1]);
    for (int i = 0; i < s_liveCount; ) {
        if (s_live[i] == root) { i++; continue; }
        Frame_close(s_live[i]);
    }
}

void Frame_run(Frame *frame) { Frame_runAll(frame); }

// ── screenshots ─────────────────────────────────────────────────────────────
Frame *Frame_active(void) { return s_active; }

Image *Frame_capture(Frame *frame) {
    if (!frame) return NULL;
    Frame_render(frame);
    if (!(*frame).gpu) return Surface_presentImage((*frame).surface);
    // GPU seam: read the FRONT IOSurface back into a CPU Image (on demand only)
    if ((*frame).lastW <= 0 || (*frame).lastH <= 0) return NULL;
    if (!(*frame).shot) (*frame).shot = Image_0();
    if (!(*frame).shot) return NULL;
    if (!Image_ensureShadow((*frame).shot, (uint32_t)(*frame).lastW, (uint32_t)(*frame).lastH))
        return NULL;
    Window_readPresentSurface((*frame).window, (*frame).surfaces[(*frame).front],
                              Image_pixels((*frame).shot), Image_stride((*frame).shot));
    return (*frame).shot;
}

bool Frame_savePNG(Frame *frame, const char *path) {
    Image *img = Frame_capture(frame);
    if (!img || !path) return false;
    return Window_writePNG(Image_pixels(img), Image_stride(img),
                           (int)Image_width(img), (int)Image_height(img), path);
}
