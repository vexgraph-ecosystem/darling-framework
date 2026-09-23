#import <Cocoa/Cocoa.h>
#include "codefield_demo.h"

@interface CodeFieldDemoView : NSView
@end
@implementation CodeFieldDemoView
- (BOOL)isFlipped { return YES; }
- (BOOL)acceptsFirstResponder { return YES; }
- (void)drawRect:(NSRect)dirtyRect {
    (void) dirtyRect;
    NSRect pixels = [self convertRectToBacking:[self bounds]];
    CodeFieldDemo_render((unsigned) pixels.size.width, (unsigned) pixels.size.height);
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    CGContextRef bitmap = CGBitmapContextCreate((void*) CodeFieldDemo_pixels(), (unsigned) pixels.size.width, (unsigned) pixels.size.height,
        8, (unsigned) pixels.size.width * 4, space, kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
    CGImageRef image = CGBitmapContextCreateImage(bitmap);
    CGContextRef context = [[NSGraphicsContext currentContext] CGContext];
    CGContextSaveGState(context);
    CGContextTranslateCTM(context, 0, [self bounds].size.height);
    CGContextScaleCTM(context, 1, -1);
    CGContextDrawImage(context, NSRectToCGRect([self bounds]), image);
    CGContextRestoreGState(context);
    CGImageRelease(image);
    CGContextRelease(bitmap);
    CGColorSpaceRelease(space);
}
- (void)mouseDown:(NSEvent *)event {
    NSPoint point = [self convertPointToBacking:[self convertPoint:[event locationInWindow] fromView:nil]];
    CodeFieldDemo_pointer( point.x, point.y, ([event modifierFlags] & NSEventModifierFlagShift) != 0);
    [self setNeedsDisplay:YES];
}
- (void)mouseDragged:(NSEvent *)event {
    NSPoint point = [self convertPointToBacking:[self convertPoint:[event locationInWindow] fromView:nil]];
    CodeFieldDemo_pointer( point.x, point.y, true);
    [self setNeedsDisplay:YES];
}
- (void)scrollWheel:(NSEvent *)event {
    CGFloat scale = [[self window] backingScaleFactor];
    CodeFieldDemo_scroll(-[event scrollingDeltaX] * scale, -[event scrollingDeltaY] * scale);
    [self setNeedsDisplay:YES];
}
- (void)keyDown:(NSEvent *)event {
    bool extend = ([event modifierFlags] & NSEventModifierFlagShift) != 0;
    NSString *characters = [event charactersIgnoringModifiers];
    if (![characters length])
        return;
    unichar c = [characters characterAtIndex:0];
    if ([event modifierFlags] & NSEventModifierFlagCommand) {
        if (c == 'a')
            CodeFieldDemo_selectAll();
    } else {
        switch (c) {
            case NSLeftArrowFunctionKey: CodeFieldDemo_key( DEMO_KEY_LEFT, extend); break;
            case NSRightArrowFunctionKey: CodeFieldDemo_key( DEMO_KEY_RIGHT, extend); break;
            case NSUpArrowFunctionKey: CodeFieldDemo_key( DEMO_KEY_UP, extend); break;
            case NSDownArrowFunctionKey: CodeFieldDemo_key( DEMO_KEY_DOWN, extend); break;
            case NSHomeFunctionKey: CodeFieldDemo_key( DEMO_KEY_HOME, extend); break;
            case NSEndFunctionKey: CodeFieldDemo_key( DEMO_KEY_END, extend); break;
            case NSDeleteFunctionKey: CodeFieldDemo_key( DEMO_KEY_DELETE, extend); break;
            case 127: CodeFieldDemo_key( DEMO_KEY_BACKSPACE, extend); break;
            case '\r': CodeFieldDemo_key( DEMO_KEY_ENTER, extend); break;
            case '\t': CodeFieldDemo_key( DEMO_KEY_TAB, extend); break;
            default: CodeFieldDemo_text( [[event characters] UTF8String]); break;
        }
    }
    [self setNeedsDisplay:YES];
}
@end

int main(int argc, const char **argv) {
    @autoreleasepool {
        CodeFieldDemo_setup();
        if (argc > 1) {
            NSRect pixels = NSMakeRect(0, 0, 1100, 700);
            CodeFieldDemo_render(1100, 700);
                    NSBitmapImageRep *bitmap = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
                pixelsWide:(unsigned) pixels.size.width pixelsHigh:(unsigned) pixels.size.height bitsPerSample:8 samplesPerPixel:4
                hasAlpha:YES isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace bytesPerRow:(unsigned) pixels.size.width * 4 bitsPerPixel:32];
            memcpy([bitmap bitmapData], CodeFieldDemo_pixels(), (size_t) (unsigned) pixels.size.width * (unsigned) pixels.size.height * 4);
            NSData *png = [bitmap representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
            if (![png writeToFile:[NSString stringWithUTF8String:argv[1]] atomically:YES])
                return 1;
        } else {
            NSApplication *app = [NSApplication sharedApplication];
            [app setActivationPolicy:NSApplicationActivationPolicyRegular];
            NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 900, 620)
                styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable
                backing:NSBackingStoreBuffered defer:NO];
            [window setReleasedWhenClosed:NO];
            [window setTitle:@"Darling / indexed CodeField"];
            CodeFieldDemoView *view = [[CodeFieldDemoView alloc] initWithFrame:NSMakeRect(0, 0, 900, 620)];
            [view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
            [window setContentView:view];
            [window makeFirstResponder:view];
            [window center];
            [window makeKeyAndOrderFront:nil];
            [app activateIgnoringOtherApps:YES];
            id token = [[NSNotificationCenter defaultCenter] addObserverForName:NSWindowWillCloseNotification
                object:window queue:nil usingBlock:^(NSNotification *notification) {
                    (void) notification;
                    [app stop:nil];
                    NSEvent *wake = [NSEvent otherEventWithType:NSEventTypeApplicationDefined location:NSZeroPoint
                        modifierFlags:0 timestamp:0 windowNumber:0 context:nil subtype:0 data1:0 data2:0];
                    [app postEvent:wake atStart:NO];
                }];
            [app run];
            [[NSNotificationCenter defaultCenter] removeObserver:token];
        }
        CodeFieldDemo_close();
    }
    return 0;
}
