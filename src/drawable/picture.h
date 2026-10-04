#ifndef DARLING_DRAWABLE_PICTURE_H
#define DARLING_DRAWABLE_PICTURE_H

#include "image.h"
#include "ui/element.h"

// Picture owns a standalone graphical Element and BORROWS the Image. It starts
// at the source's native size (or zero for NULL); explicit size stretches the
// whole source. Replacement does not resize. No crop/contain/cover modes yet.
// Non-NULL images require a valid RGBA8 CPU shadow in this initial backend slice.
// Attach Picture_graphics directly to another Element; this does NOT transfer
// wrapper ownership. Destroy Picture BEFORE the borrowed parent/root destroys
// its tree (e.g. in Frame_onClose). Destroy detaches its Element then frees it.
// Do not destroy Picture_graphics independently. Image backing must outlive
// Picture and every retained DisplayList that refers to it. All operations need
// external synchronization; mutations must not race painting or GPU image use.
// NULL widget queries return zero/NULL; setters/destroy are safe no-ops. Size and
// location must be finite (size nonnegative); cold rejection leaves state intact.
typedef struct Picture Picture;

Picture *Picture_0(void);
Picture *Picture_1(const Image *image);
#define PICTURE_CHOOSER(_0, _1, NAME, ...) NAME
#define Picture(...) PICTURE_CHOOSER(dummy __VA_OPT__(,) __VA_ARGS__, Picture_1, Picture_0)(__VA_ARGS__)
void Picture_destroy(Picture *picture);
Element *Picture_graphics(const Picture *picture); // borrowed; normal Element_paint
const Image *Picture_image(const Picture *picture);
void Picture_setImage(Picture *picture, const Image *image);
void Picture_setSize(Picture *picture, float width, float height);
void Picture_setLocation(Picture *picture, float x, float y);
float Picture_width(const Picture *picture);
float Picture_height(const Picture *picture);
// Top-left in a zero-origin/zero-size parent context; defaults round-trip
// setLocation. Changing anchor/pivot via graphics changes that resolved query.
Point Picture_location(const Picture *picture);
void Picture_toString(const Picture *picture, char *dest, size_t cap, bool *outTruncated);
void Picture_toStringStruct(const Picture *picture, char *dest, size_t cap, bool *outTruncated);

#endif // DARLING_DRAWABLE_PICTURE_H
