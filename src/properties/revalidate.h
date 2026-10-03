#ifndef DARLING_PROPERTIES_REVALIDATE_H
#define DARLING_PROPERTIES_REVALIDATE_H

#include "c23/overload.h"

// darling R4 — properties/revalidate.h
//
// The REVALIDATE operation: the top-down render cascade. `Frame_revalidate` is
// the one call that makes a frame reflect its tree:
//
//   Frame_revalidate(frame)
//     -> Frame_prepareRevalidate (size the window once)
//     -> Surface_revalidate     (revalidate every attached Board, then present)
//          -> Board_revalidate  (content board: revalidate the Element tree,
//                                paint it, publish the generation)
//
// Grouped here so the cascade entry point is not buried in the widget file.

typedef struct Frame Frame;

void Frame_revalidate_1(Frame *frame);

#define Frame_revalidate(...) OVERLOAD_DISPATCH(Frame_revalidate, __VA_ARGS__)

#endif // DARLING_PROPERTIES_REVALIDATE_H
