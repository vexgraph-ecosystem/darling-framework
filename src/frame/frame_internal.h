#ifndef DARLING_FRAME_INTERNAL_H
#define DARLING_FRAME_INTERNAL_H

#include "frame/frame.h"

// darling R4 — frame/frame_internal.h
//
// The seam between the Frame widget (frame.c) and the properties layer
// (properties/add.c, remove.c, revalidate.c, set_size.c). Frame keeps its panels
// array and native render targets private; properties use the seams below.

// Link `panel` into the frame's content Element and take the wrapper into the
// frame's owned set. index < 0 appends; index >= 0 inserts there. False on OOM.
bool Frame_ownPanel(Frame *frame, Panel *panel, int index);

// Unlink and destroy every panel the frame owns (the wrappers + their Elements).
void Frame_clearPanels(Frame *frame);

// The revalidate preamble: true when the frame is ready to revalidate. When the
// frame has no size yet, this sizes it from the window (which repaints) and
// returns false, so the caller stops — no recursive revalidate.
bool Frame_prepareRevalidate(Frame *frame);

// Resize the private layout/render targets without presenting. Returns true
// only for a changed positive extent; invalid and same-size calls are no-ops.
bool Frame_resizeTargets(Frame *frame, int widthPx, int heightPx);

#endif // DARLING_FRAME_INTERNAL_H
