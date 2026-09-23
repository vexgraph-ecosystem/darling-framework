#ifndef DARLING_FIELD_CODEFIELD_H
#define DARLING_FIELD_CODEFIELD_H

#include "darling/panel/panel.h"

// Indexed rows are independent of source-file lines. Positions are ASCII byte
// offsets for the current built-in bitmap font. Mutations are owner-thread cold
// operations; layout, paint, scroll, hit-test, and selection allocate nothing.
typedef void (*CodeField_ButtonFn)(void *params, size_t index, size_t lineNumber);
typedef void (*CodeField_ChangeFn)(void *ctx);
typedef enum CodeFieldKey {
    CODEFIELD_KEY_LEFT, CODEFIELD_KEY_RIGHT, CODEFIELD_KEY_UP, CODEFIELD_KEY_DOWN,
    CODEFIELD_KEY_HOME, CODEFIELD_KEY_END, CODEFIELD_KEY_BACKSPACE,
    CODEFIELD_KEY_DELETE, CODEFIELD_KEY_ENTER, CODEFIELD_KEY_TAB
} CodeFieldKey;


// SLOT RECORD: behaviorless data owned exclusively by CodeField.
typedef struct CodeFieldRow {
    char *text;                  // owned arena string; never null for a live row
    Panel *content;              // borrowed optional visual, never freed/reparented
    float height;               // declared height or SIZE_AUTO
    float top;                  // resolved content-space start
    float extent;               // resolved height
    size_t number;              // derived 1-based number, zero when excluded
    bool includeLineNumber;     // independent of content kind
    CodeField_ButtonFn button;  // optional row action
    void *params;               // borrowed action context
} CodeFieldRow;

typedef struct CodeField {
    // --- Core ---
    Panel base;                 // composite painter; rows are flat slots
    Panel numberPanel;          // embedded number-side visual
    Panel textPanel;            // embedded text-side visual
    CodeFieldRow *rows;          // owned doubling array
    size_t count;               // live rows
    size_t capacity;            // allocated slots
    CodeField_ChangeFn onChange; // owner-thread notification
    void *ctx;                  // borrowed change context
    bool readOnly;              // disables interactive text mutations
    bool focused;               // caret visibility; host controls focus
    // --- Text part ---
    float rowHeight;            // default AUTO row minimum
    uint32_t textColor;         // RGBA
    float textScale;            // bitmap glyph scale, measured and painted together
    // --- Selection part ---
    size_t anchorRow;
    size_t anchorColumn;
    size_t activeRow;
    size_t activeColumn;
    uint32_t highlightColor;
    // --- Scroll part ---
    float scrollX;
    float scrollY;
    float contentWidth;         // derived, includes text padding
    float contentHeight;        // derived sum of row extents
    Rectangle viewport;        // resolved text viewport
    float barSize;              // scrollbar thickness
    // --- Gutter part ---
    float gutterWidth;          // minimum width; actual width grows with digits
    float measuredGutterWidth;  // derived number + button column width
    float buttonWidth;
    uint32_t gutterTextColor;
} CodeField;

CodeField *CodeField_0(void);
CodeField *CodeField_1(Panel *parent);
CodeField *CodeField_2(Panel *parent, const char *text);
#define CodeField(...) CONSTRUCTOR_DISPATCH(CodeField, __VA_ARGS__)
CodeField *CodeField_zero(void);
void CodeField_free(CodeField *self);

// Cold, failure-atomic structural/text mutations. False leaves old text intact.
bool CodeField_add(CodeField *self, const char *text);
bool CodeField_insertRow(CodeField *self, size_t index, const char *text);
bool CodeField_removeRow(CodeField *self, size_t index);
bool CodeField_setText(CodeField *self, const char *text);
bool CodeField_setRowText(CodeField *self, size_t index, const char *text);
bool CodeField_insertText(CodeField *self, const char *text);
bool CodeField_deleteBackward(CodeField *self);
bool CodeField_deleteForward(CodeField *self);
bool CodeField_copySelection(const CodeField *self, char *dest, size_t cap, bool *outTruncated);
bool CodeField_handleKey(CodeField *self, CodeFieldKey key, bool extendSelection);
void CodeField_layout(CodeField *self);
bool CodeField_paint(CodeField *self);
void CodeField_scrollVertical(CodeField *self, float delta);
void CodeField_scrollHorizontal(CodeField *self, float delta);
void CodeField_scrollToRow(CodeField *self, size_t index);
bool CodeField_hitTest(CodeField *self, float x, float y, size_t *destRow, size_t *destColumn);
// Screen/native-pixel coordinates. Drag extends text selection; buttons invoke
// only on press. Scrollbar track presses reposition the corresponding offset.
bool CodeField_pointer(CodeField *self, float x, float y, bool extendSelection);

bool CodeField_setIncludeLineNumber(CodeField *self, size_t index, bool flag);
bool CodeField_getIncludeLineNumber(const CodeField *self, size_t index);
size_t CodeField_getLineNumber(const CodeField *self, size_t index);
size_t CodeField_getRowCount(const CodeField *self);
const char *CodeField_getRowText(const CodeField *self, size_t index);
bool CodeField_setRowHeight(CodeField *self, size_t index, float height);
float CodeField_getRowHeight(const CodeField *self, size_t index);
bool CodeField_setRowContent(CodeField *self, size_t index, Panel *content);
Panel *CodeField_getRowContent(const CodeField *self, size_t index);
bool CodeField_getRowAbsoluteRect(CodeField *self, size_t index, Rectangle *dest);

bool CodeField_selection_setRange(CodeField *self, size_t anchorRow, size_t anchorColumn,
                                  size_t activeRow, size_t activeColumn);
void CodeField_selection_getRange(const CodeField *self, size_t *anchorRow, size_t *anchorColumn,
                                  size_t *activeRow, size_t *activeColumn);
void CodeField_selection_selectAll(CodeField *self);
void CodeField_selection_setColor(CodeField *self, uint32_t color);
uint32_t CodeField_selection_getColor(const CodeField *self);

bool CodeField_button_setFunction(CodeField *self, size_t index, CodeField_ButtonFn fn, void *params);
CodeField_ButtonFn CodeField_button_getFunction(const CodeField *self, size_t index, void **destParams);
bool CodeField_button_invoke(CodeField *self, size_t index);
void CodeField_scroll_setOffset(CodeField *self, float x, float y);
void CodeField_scroll_getOffset(const CodeField *self, float *x, float *y);
void CodeField_scroll_getExtent(const CodeField *self, float *width, float *height);
void CodeField_scroll_setBarSize(CodeField *self, float size);
float CodeField_scroll_getBarSize(const CodeField *self);
void CodeField_gutter_setWidth(CodeField *self, float width);
float CodeField_gutter_getWidth(const CodeField *self);
float CodeField_gutter_getMeasuredWidth(const CodeField *self);
void CodeField_gutter_setButtonWidth(CodeField *self, float width);
float CodeField_gutter_getButtonWidth(const CodeField *self);
void CodeField_gutter_setBackgroundColor(CodeField *self, uint32_t color);
uint32_t CodeField_gutter_getBackgroundColor(const CodeField *self);
void CodeField_gutter_setTextColor(CodeField *self, uint32_t color);
uint32_t CodeField_gutter_getTextColor(const CodeField *self);
void CodeField_text_setColor(CodeField *self, uint32_t color);
uint32_t CodeField_text_getColor(const CodeField *self);
void CodeField_text_setRowHeight(CodeField *self, float height);
float CodeField_text_getRowHeight(const CodeField *self);
void CodeField_text_setScale(CodeField *self, float scale);
float CodeField_text_getScale(const CodeField *self);
Panel *CodeField_text_getPanel(CodeField *self);
Panel *CodeField_gutter_getPanel(CodeField *self);
void CodeField_setReadOnly(CodeField *self, bool flag);
bool CodeField_isReadOnly(const CodeField *self);
void CodeField_setFocused(CodeField *self, bool flag);
bool CodeField_isFocused(const CodeField *self);
void CodeField_setOnChange(CodeField *self, CodeField_ChangeFn fn);
CodeField_ChangeFn CodeField_getOnChange(const CodeField *self);
void CodeField_setCtx(CodeField *self, void *ctx);
void *CodeField_getCtx(const CodeField *self);
void CodeField_toString(const CodeField *self, char *dest, size_t cap, bool *outTruncated);
void CodeField_toStringStruct(const CodeField *self, char *dest, size_t cap, bool *outTruncated);
#endif
