#ifndef DARLING_FIELD_INPUTOTP_H
#define DARLING_FIELD_INPUTOTP_H

#include <stdbool.h>
#include <stdint.h>

#include "c23/constructor.h"
#include "darling/panel/panel.h"
#include "event/keyevent.h"

#define INPUTOTP_MAX_BOXES 16

// Fixed-length one-time-code input: Panel layout plus an owned digit buffer.
typedef void (*InputOTP_CompleteFn)(void *ctx);

typedef struct InputOTP {
    Panel base;
    char *digits;
    int32_t length;
    float boxSize;
    float gap;
    int32_t cursor;
    bool focused;
    bool password;
    float fontSize;
    uint32_t textColor;
    uint32_t boxBackground;
    uint32_t boxBorderColor;
    uint32_t boxActiveBorder;
    InputOTP_CompleteFn onComplete;
    void *ctx;

    // Raster cache per box
    int32_t boxTex[INPUTOTP_MAX_BOXES];
    int32_t boxW[INPUTOTP_MAX_BOXES];
    int32_t boxH[INPUTOTP_MAX_BOXES];
    char boxChar[INPUTOTP_MAX_BOXES];
} InputOTP;

InputOTP *InputOTP_1(int32_t length);
InputOTP *InputOTP_2(Panel *parent, int32_t length);
InputOTP *InputOTP_1_parent(Panel *parent);

#define InputOTP(...) CONSTRUCTOR_DISPATCH(InputOTP, __VA_ARGS__)

// Core entries
void InputOTP_pushDigit(InputOTP *otp, char digit);
void InputOTP_handlePointer(InputOTP *self, int kind, float localX, float localY);
void InputOTP_handleKey(InputOTP *self, const UIKeyEvent *ev);

void InputOTP_free(InputOTP *otp);

void InputOTP_setDigits(InputOTP *otp, const char *digits);
void InputOTP_setBoxSize(InputOTP *otp, float size);
void InputOTP_setGap(InputOTP *otp, float gap);
void InputOTP_setCursor(InputOTP *otp, int32_t cursor);
void InputOTP_setPassword(InputOTP *otp, bool password);
void InputOTP_setFontSize(InputOTP *otp, float size);
void InputOTP_setTextColor(InputOTP *otp, uint32_t color);
void InputOTP_setBoxBackground(InputOTP *otp, uint32_t color);
void InputOTP_setBoxBorderColor(InputOTP *otp, uint32_t color);
void InputOTP_setBoxActiveBorder(InputOTP *otp, uint32_t color);
void InputOTP_setOnComplete(InputOTP *otp, InputOTP_CompleteFn fn);
void InputOTP_setCtx(InputOTP *otp, void *ctx);

const char *InputOTP_getDigits(const InputOTP *otp);
int32_t InputOTP_getLength(const InputOTP *otp);
float InputOTP_getBoxSize(const InputOTP *otp);
float InputOTP_getGap(const InputOTP *otp);
int32_t InputOTP_getCursor(const InputOTP *otp);
bool InputOTP_isFocused(const InputOTP *otp);
bool InputOTP_isPassword(const InputOTP *otp);
float InputOTP_getFontSize(const InputOTP *otp);
uint32_t InputOTP_getTextColor(const InputOTP *otp);
uint32_t InputOTP_getBoxBackground(const InputOTP *otp);
uint32_t InputOTP_getBoxBorderColor(const InputOTP *otp);
uint32_t InputOTP_getBoxActiveBorder(const InputOTP *otp);
InputOTP_CompleteFn InputOTP_getOnComplete(const InputOTP *otp);
void *InputOTP_getCtx(const InputOTP *otp);

#endif

