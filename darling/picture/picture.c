#include "annotation/definition.h"
#include "annotation/overview.h"
#include "darling/picture/picture.h"
#include "nio/mem.h"
#include "oop/type.h"
#include "vulkan/vk.h"
#include "vulkan/texture/texture.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Picture
 * ============================================================================
 * Retained-mode off-heap picture node inheriting Panel -> Container: hosts an
 * Image asset descriptor or a bindless Vulkan texture, with optional UV
 * cropping, explicit pixel dimension overrides, and a scaling/fill mode (FIT,
 * ZOOM_FILL, ...). The node renders through the Stage 1 image hook (texture
 * quad, amber placeholder when unbound) and can own its GPU texture
 * (ownsTexture) or borrow a caller-bound textureId. Layout anchors
 * hierarchically through the embedded Panel/Container; Picture_load binds a
 * VFS-loaded texture and Picture_cycleMode advances the mode seamlessly.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Picture (inherits Panel -> Container)
 * LEVEL: L2 — Behavior (UI picture node behavior API)
 * ============================================================================
 * Retained-mode off-heap picture node that hosts an Image asset with optional
 * UV cropping, dimension overrides, bindless Vulkan texture rendering, and
 * hierarchical layout anchoring.
 *
 * STRUCT FIELDS (Mirroring darling/picture/picture.h):
 * ----------------------------------------------------------------------------
 *   Panel base;            // Base UI panel state (bounds, anchors, background)
 *   void *image;           // Pointer to raw Image asset descriptor
 *   float imageSizeW;      // Explicit pixel display width (-1 = auto)
 *   float imageSizeH;      // Explicit pixel display height (-1 = auto)
 *   float cropX1;          // Normalized UV top-left X crop bound
 *   float cropY1;          // Normalized UV top-left Y crop bound
 *   float cropX2;          // Normalized UV bottom-right X crop bound
 *   float cropY2;          // Normalized UV bottom-right Y crop bound
 *   bool hasImageSize;     // Explicit dimension override active flag
 *   bool hasCrop;          // Custom UV crop rect active flag
 *   int32_t textureId;     // Bindless texture ID in GPU registry (-1 = none)
 *   PictureMode mode;      // Scaling/fill anchor mode (FIT, ZOOM_FILL, etc.)
 *   bool ownsTexture;      // True if texture was loaded by this Picture and should be freed
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Picture()                               : Picture_0()
 *   - Picture(image)                          : Picture_1(image)
 *
 * Core Functions:
 *   - picturePaintImage(panel, rend, cmd, ...) : Stage 1 texture quad / amber
 *     placeholder (via Panel_setImageFn; background stays Panel default)
 *   - Picture_load(p, vfsPath)                : Loads texture via VFS and binds to node
 *   - Picture_cycleMode(p)                    : Advances to next PictureMode seamlessly
 *   - Picture_getModeName(mode)               : String label for PictureMode
 *   - Picture_free(p)                         : Frees node and owned GPU texture
 *
 * Setters:
 *   - Picture_setImage(p, image)
 *   - Picture_setImageSize(p, w, h)
 *   - Picture_setCrop(p, x1, y1, x2, y2)
 *   - Picture_setTexture(p, textureId)
 *   - Picture_setMode(p, mode)
 *
 * Getters:
 *   - Picture_getImage(p)
 *   - Picture_getImageSize(p, outW, outH)
 *   - Picture_hasImageSize(p)
 *   - Picture_getCrop(p, outX1, outY1, outX2, outY2)
 *   - Picture_hasCrop(p)
 *   - Picture_getTexture(p)
 *   - Picture_getMode(p)
 * ============================================================================
 */

// ============================================================================
// CORE FUNCTIONS (Default Renderer)
// ============================================================================

// Stage 1: texture quad via texture_quad, amber placeholder when unbound.
// Background (Panel color) is stage 0 and runs beneath this.
static bool picturePaintImage(Panel *panel, void *renderer, void *cmdBuffer,
                              float surfaceW, float surfaceH,
                              float x, float y, float w, float h) {
    (void)renderer;
    Picture *p = (Picture*) panel;
    if (!p || !cmdBuffer)
        return false;
    if (w <= 0.0f || h <= 0.0f)
        return false;
    int32_t texId = (*p).textureId;
    if (texId < 0) {
        Vk_fillRect(cmdBuffer, surfaceW, surfaceH, x, y, w, h, 1.0f, 0.8f, 0.0f, 1.0f);
        return true;
    }
    uint32_t imgW = 1, imgH = 1;
    Texture_getSize(texId, &imgW, &imgH);
    Component *c = &(*panel).component;
    float op = Component_getOpacity(c);
    if (op <= 0.0f)
        return false;
    Vk_drawTexture(cmdBuffer, surfaceW, surfaceH, x, y, w, h,
                   1.0f, 1.0f, 1.0f, op,
                   texId,
                   (*p).mode,
                   (float) imgW, (float) imgH);
    return true;
}

// ============================================================================
// CONSTRUCTORS
// ============================================================================

Picture *Picture_0(void) {
    Picture *p = (Picture*) Memory_alloc(TYPE_PICTURE_SINGLETON, sizeof(Picture));
    if (!p) return nullptr;

    Panel *basePanel = Panel_0();
    if (!basePanel) {
        Memory_free(p);
        return nullptr;
    }

    (*p).base = (*basePanel);
    Memory_free(basePanel);

    (*p).image = nullptr;
    (*p).imageSizeW = 0.0f;
    (*p).imageSizeH = 0.0f;
    (*p).cropX1 = 0.0f;
    (*p).cropY1 = 0.0f;
    (*p).cropX2 = 0.0f;
    (*p).cropY2 = 0.0f;
    (*p).hasImageSize = false;
    (*p).hasCrop = false;
    (*p).textureId = -1;
    (*p).mode = PICTURE_MODE_FIT;
    (*p).ownsTexture = false;

    Panel *pp = &(*p).base;
    Panel_setRenderHandler(pp, nullptr);
    Panel_setImageFn(pp, picturePaintImage);

    return p;
}

Picture *Picture_1(void *image) {
    Picture *p = Picture_0();
    if (p) {
        (*p).image = image;
    }
    return p;
}

// ============================================================================
// LIFECYCLE & ASSET BINDING
// ============================================================================

bool Picture_load(Picture *p, const char *vfsPath) {
    if (!p || !vfsPath) return false;

    int32_t newId = Texture_load(vfsPath);
    if (newId < 0) return false;

    if ((*p).ownsTexture && (*p).textureId >= 0 && (*p).textureId != newId)
        Texture_free((*p).textureId);

    (*p).textureId = newId;
    (*p).ownsTexture = true;

    uint32_t w = 0, h = 0;
    if (Texture_getSize(newId, &w, &h)) {
        (*p).imageSizeW = (float) w;
        (*p).imageSizeH = (float) h;
        (*p).hasImageSize = true;
    }

    Panel *basePanel = &(*p).base;
    (void) basePanel;
    return true;
}

void Picture_free(Picture *p) {
    if (!p) return;
    if ((*p).ownsTexture && (*p).textureId >= 0) {
        Texture_free((*p).textureId);
        (*p).textureId = -1;
    }
    Memory_free(p);
}

// ============================================================================
// SETTERS
// ============================================================================

void Picture_setImage(Picture *p, void *image) {
    if (p) {
        (*p).image = image;
        Panel *basePanel = &(*p).base;
        (void) basePanel;
    }
}

void Picture_setTexture(Picture *p, int32_t textureId) {
    if (p) {
        if ((*p).ownsTexture && (*p).textureId >= 0 && (*p).textureId != textureId)
            Texture_free((*p).textureId);
        (*p).textureId = textureId;
        (*p).ownsTexture = false;
        Panel *basePanel = &(*p).base;
        (void) basePanel;
    }
}

void Picture_setMode(Picture *p, PictureMode mode) {
    if (p) {
        (*p).mode = mode;
        Panel *basePanel = &(*p).base;
        (void) basePanel;
    }
}

void Picture_cycleMode(Picture *p) {
    if (p) {
        (*p).mode = (PictureMode)(((*p).mode + 1) % 8);
        Panel *basePanel = &(*p).base;
        (void) basePanel;
    }
}

const char *Picture_getModeName(PictureMode mode) {
    switch (mode) {
        case PICTURE_MODE_FIT:               return "FIT";
        case PICTURE_MODE_ZOOM_FILL:         return "ZOOM_FILL";
        case PICTURE_MODE_ZOOM_FIT:          return "ZOOM_FIT";
        case PICTURE_MODE_FILL_CENTER:       return "FILL_CENTER";
        case PICTURE_MODE_FILL_TOP_LEFT:     return "FILL_TOP_LEFT";
        case PICTURE_MODE_FILL_TOP_RIGHT:    return "FILL_TOP_RIGHT";
        case PICTURE_MODE_FILL_BOTTOM_LEFT:  return "FILL_BOTTOM_LEFT";
        case PICTURE_MODE_FILL_BOTTOM_RIGHT: return "FILL_BOTTOM_RIGHT";
        default:                             return "UNKNOWN";
    }
}

void Picture_setImageSize(Picture *p, float w, float h) {
    if (p) {
        (*p).imageSizeW = w;
        (*p).imageSizeH = h;
        (*p).hasImageSize = true;
        Panel *basePanel = &(*p).base;
        (void) basePanel;
    }
}

void Picture_setCrop(Picture *p, float x1, float y1, float x2, float y2) {
    if (p) {
        (*p).cropX1 = x1;
        (*p).cropY1 = y1;
        (*p).cropX2 = x2;
        (*p).cropY2 = y2;
        (*p).hasCrop = true;
        Panel *basePanel = &(*p).base;
        (void) basePanel;
    }
}

// ============================================================================
// GETTERS
// ============================================================================

void *Picture_getImage(const Picture *p) {
    return p ? (*p).image : nullptr;
}

int32_t Picture_getTexture(const Picture *p) {
    return p ? (*p).textureId : -1;
}

PictureMode Picture_getMode(const Picture *p) {
    return p ? (*p).mode : PICTURE_MODE_FIT;
}

void Picture_getImageSize(const Picture *p, float *outW, float *outH) {
    if (outW) (*outW) = p ? (*p).imageSizeW : 0.0f;
    if (outH) (*outH) = p ? (*p).imageSizeH : 0.0f;
}

bool Picture_hasImageSize(const Picture *p) {
    return p ? (*p).hasImageSize : false;
}

void Picture_getCrop(const Picture *p, float *outX1, float *outY1, float *outX2, float *outY2) {
    if (outX1) *outX1 = p ? (*p).cropX1 : 0.0f;
    if (outY1) *outY1 = p ? (*p).cropY1 : 0.0f;
    if (outX2) *outX2 = p ? (*p).cropX2 : 0.0f;
    if (outY2) *outY2 = p ? (*p).cropY2 : 0.0f;
}

bool Picture_hasCrop(const Picture *p) {
    return p ? (*p).hasCrop : false;
}
