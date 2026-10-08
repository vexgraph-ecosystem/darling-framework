#include "drawable/picture.h"

#include "annotation/definition.h"
#include "annotation/overview.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "exception/throw.h"

;;DEFINITION
/**
 * Picture is a minimal borrowed-image widget owning one standalone Element.
 * A drawable Image may carry a CPU shadow or a completed GPU texture; Picture
 * never demands a readback just to display a filter result.
 * Native initial size, explicit stretching and ordinary Element paint are the
 * implemented scope, not the legacy crop/fit API. Parent attachment borrows the
 * wrapper's Element: teardown must destroy this wrapper before the parent's
 * tree. Destroy detaches first so the parent cannot double-free the Element.
 * Image ownership never transfers. Retained lists may outlive the wrapper but
 * must not outlive the borrowed image. R3 still owns all rendering semantics.
 */

;;OVERVIEW
/**
 * CLASS: Picture (drawable/picture.c)
 * Fields: Element *graphics (owned standalone node with borrowed image).
 * Public: _0/_1 + chooser; destroy, graphics, image/setImage, setSize,
 * setLocation, width/height/location, toString/toStringStruct.
 * Private: format writes bounded cold strings. CPU/GPU drawable admission
 * rejects metadata-only Images. Setters validate finite geometry
 * then forward to Element. No input/focus or automatic filter capability.
 */

struct Picture {
    Element *graphics;
};

Picture *Picture_1(const Image *image) {
    if (image && (!Image_isValid(image) || Image_format(image) != IMAGE_FORMAT_RGBA8 ||
                  !Image_isDrawable(image))) {
        THROW("Picture requires a valid borrowed drawable RGBA8 Image");
        return nullptr;
    }
    Picture *picture = calloc(1, sizeof *picture);
    if (!picture) {
        THROW("Picture allocation failed");
        return nullptr;
    }
    ElementDesc desc = {.width = (float) Image_width(image),
                        .height = (float) Image_height(image)};
    Element *graphics = Element(&desc);
    if (!graphics) {
        free(picture);
        THROW("Picture Element allocation failed");
        return nullptr;
    }
    (*picture).graphics = graphics;
    Element_setImage(graphics, image);
    return picture;
}

Picture *Picture_0(void) {
    return Picture_1(nullptr);
}

void Picture_destroy(Picture *picture) {
    if (!picture)
        return;
    Element *graphics = (*picture).graphics;
    Element_remove(graphics);
    Element_destroy(graphics);
    free(picture);
}

Element *Picture_graphics(const Picture *picture) {
    return picture ? (*picture).graphics : nullptr;
}

const Image *Picture_image(const Picture *picture) {
    return Element_image(Picture_graphics(picture));
}

void Picture_setImage(Picture *picture, const Image *image) {
    if (!picture)
        return;
    if (image && (!Image_isValid(image) || Image_format(image) != IMAGE_FORMAT_RGBA8 ||
                  !Image_isDrawable(image))) {
        THROW("Picture requires a valid borrowed drawable RGBA8 Image");
        return;
    }
    Element_setImage((*picture).graphics, image);
}

void Picture_setSize(Picture *picture, float width, float height) {
    if (!picture)
        return;
    if (!isfinite(width) || !isfinite(height) || width < 0 || height < 0) {
        THROW("Picture size must be finite and nonnegative");
        return;
    }
    Element_setSize((*picture).graphics, width, height);
}

void Picture_setLocation(Picture *picture, float x, float y) {
    if (!picture)
        return;
    if (!isfinite(x) || !isfinite(y)) {
        THROW("Picture location must be finite");
        return;
    }
    Element_setOffset((*picture).graphics, x, y);
}

float Picture_width(const Picture *picture) {
    return Element_width(Picture_graphics(picture));
}
float Picture_height(const Picture *picture) {
    return Element_height(Picture_graphics(picture));
}
Point Picture_location(const Picture *picture) {
    Rect rect = Element_eventBound(Picture_graphics(picture), (Rect){0});
    return (Point){rect.x, rect.y};
}

static void format(const Picture *picture, bool structure, char *dest, size_t cap,
                   bool *outTruncated) {
    if (!dest && cap) {
        if (outTruncated)
            *outTruncated = true;
        THROW("Picture string destination is NULL");
        return;
    }
    int length;
    if (!picture)
        length = snprintf(dest, cap, "nullptr");
    else if (structure)
        length = snprintf(dest, cap, "Picture{graphics=Element(width=%g,height=%g)}",
                          (double) Picture_width(picture), (double) Picture_height(picture));
    else {
        const Image *image = Picture_image(picture);
        length = snprintf(dest, cap, "Picture(%gx%g,image=%ux%u)",
                          (double) Picture_width(picture), (double) Picture_height(picture),
                          Image_width(image), Image_height(image));
    }
    if (outTruncated)
        *outTruncated = length < 0 || (size_t) length >= cap;
}

void Picture_toString(const Picture *picture, char *dest, size_t cap, bool *outTruncated) {
    format(picture, false, dest, cap, outTruncated);
}
void Picture_toStringStruct(const Picture *picture, char *dest, size_t cap, bool *outTruncated) {
    format(picture, true, dest, cap, outTruncated);
}
