#ifndef DARLING_SCROLLBAR_H
#define DARLING_SCROLLBAR_H

#include <stdbool.h>
#include <stdint.h>

#include "c23/constructor.h"
#include "darling/panel/panel.h"
#include "event/pointer.h"
#include "oop/type.h"



#define SCROLL_BAR_GESTURE  0
#define SCROLL_BAR_POINT    1

// darling/field/scrollbar.h — track+thumb scroller with two thumb laws.

typedef struct ScrollBar {
    Panel base;
    int mode;
    float min;
    float max;
    float value;
    float thumbMin;
} ScrollBar;

// Constructors:
//   ScrollBar()       — detached gesture-mode bar on [0, 1]
//   ScrollBar(mode)   — detached bar in the given mode
ScrollBar *ScrollBar_0(void);
ScrollBar *ScrollBar_1(int mode);

#define ScrollBar(...) CONSTRUCTOR_DISPATCH(ScrollBar, __VA_ARGS__)

// Core (pure value mapping; the input pump calls these later).
void ScrollBar_dragBy(ScrollBar *s, float deltaPx, float trackLen);
void ScrollBar_clickAt(ScrollBar *s, float fraction);
void ScrollBar_setRange(ScrollBar *s, float min, float max);
void ScrollBar_handlePointer(ScrollBar *s, int kind, float localX, float localY);

// Setters.
void ScrollBar_setMode(ScrollBar *s, int mode);
void ScrollBar_setValue(ScrollBar *s, float value);
void ScrollBar_setThumbMin(ScrollBar *s, float px);

// Getters.
int ScrollBar_getMode(const ScrollBar *s);
float ScrollBar_getValue(const ScrollBar *s);
float ScrollBar_getThumbMin(const ScrollBar *s);
void ScrollBar_getRange(const ScrollBar *s, float *outMin, float *outMax);

#endif
