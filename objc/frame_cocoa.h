#ifndef DARLING_OBJC_FRAME_COCOA_H
#define DARLING_OBJC_FRAME_COCOA_H

#include "darling/frame.h"

#ifdef __cplusplus
extern "C" {
#endif

// Cocoa platform glue: NSWindow -> NSVisualEffectView -> CAMetalLayer
void FrameCocoa_attach(Frame *frame);
void FrameCocoa_detach(Frame *frame);
void FrameCocoa_syncTransaction(Frame *frame);
void FrameCocoa_reassertHook(Frame *frame);

#ifdef __cplusplus
}
#endif

#endif // DARLING_OBJC_FRAME_COCOA_H
