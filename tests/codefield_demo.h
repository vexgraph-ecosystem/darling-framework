#ifndef CODEFIELD_DEMO_H
#define CODEFIELD_DEMO_H
#include <stdbool.h>
// C-only bridge keeps Darling's legacy Component alias out of Apple's headers.
enum {
    DEMO_KEY_LEFT, DEMO_KEY_RIGHT, DEMO_KEY_UP, DEMO_KEY_DOWN,
    DEMO_KEY_HOME, DEMO_KEY_END, DEMO_KEY_BACKSPACE,
    DEMO_KEY_DELETE, DEMO_KEY_ENTER, DEMO_KEY_TAB
};
void CodeFieldDemo_setup(void);
void CodeFieldDemo_render(unsigned width, unsigned height);
const unsigned char *CodeFieldDemo_pixels(void);
void CodeFieldDemo_pointer(float x, float y, bool extend);
void CodeFieldDemo_scroll(float x, float y);
void CodeFieldDemo_text(const char *text);
void CodeFieldDemo_selectAll(void);
void CodeFieldDemo_key(int key, bool extend);
void CodeFieldDemo_close(void);
#endif
