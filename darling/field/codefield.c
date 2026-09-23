#include "darling/field/codefield.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "annotation/definition.h"
#include "annotation/draft.h"
#include "annotation/intention.h"
#include "annotation/overview.h"
#include "annotation/getter.h"
#include "annotation/setter.h"
#include "font/font.h"
#include "lang/size.h"
#include "nio/mem.h"

;;DRAFT
;;INTENTION("Indexed CodeField Row Law: composite field with flat row slots; no child-layer ownership.")
;;DEFINITION
/**
 * CodeField owns a flat, doubling row array and arena text copies. Borrowed row
 * panels and callback contexts are detach-only. Row indices, displayed numbers,
 * and file positions are distinct; this class does not infer file mappings.
 * Numbering counts inclusion flags, while geometry sums resolved row heights.
 * All mutation is owner-thread cold work. Paint, selection and scrolling allocate
 * nothing. The bitmap text seam currently supports printable ASCII and tabs;
 * unsupported text is rejected without changing the document, never corrupted.
 * Each row is an independent visual block; explicit newlines create rows through
 * setText/insertText. Custom panels are excluded from text replacement ranges.
 * One clipped painter draws two adjacent regions through Graphics; it creates no
 * child layers or document-sized texture. The host owns event routing and focus.
 */

;;OVERVIEW
/**
 * CLASS: CodeField
 * LEVEL: L2 — Behavior
 * STRUCT FIELDS (mirrors codefield.h):
 * // --- Core ---
 *     Panel base;                 // composite painter; rows are flat slots
 *     Panel numberPanel;          // embedded number-side visual
 *     Panel textPanel;            // embedded text-side visual
 *     CodeFieldRow *rows;          // owned doubling array
 *     size_t count;               // live rows
 *     size_t capacity;            // allocated slots
 *     CodeField_ChangeFn onChange; // owner-thread notification
 *     void *ctx;                  // borrowed change context
 *     bool readOnly;              // disables interactive text mutations
 *     bool focused;               // caret visibility; host controls focus
 *     // --- Text part ---
 *     float rowHeight;            // default AUTO row minimum
 *     uint32_t textColor;         // RGBA
 *     float textScale;            // bitmap glyph scale, measured and painted together
 *     // --- Selection part ---
 *     size_t anchorRow;
 *     size_t anchorColumn;
 *     size_t activeRow;
 *     size_t activeColumn;
 *     uint32_t highlightColor;
 *     // --- Scroll part ---
 *     float scrollX;
 *     float scrollY;
 *     float contentWidth;         // derived, includes text padding
 *     float contentHeight;        // derived sum of row extents
 *     Rectangle viewport;        // resolved text viewport
 *     float barSize;              // scrollbar thickness
 *     // --- Gutter part ---
 *     float gutterWidth;          // minimum width; actual width grows with digits
 *     float measuredGutterWidth;  // derived number + button column width
 *     float buttonWidth;
 *     uint32_t gutterTextColor;
 * SLOT RECORD: CodeFieldRow (behaviorless, owned only by CodeField)
 * char *text;                  // owned arena string; never null for a live row
 *     Panel *content;              // borrowed optional visual, never freed/reparented
 *     float height;               // declared height or (float) SIZE_AUTO
 *     float top;                  // resolved content-space start
 *     float extent;               // resolved height
 *     size_t number;              // derived 1-based number, zero when excluded
 *     bool includeLineNumber;     // independent of content kind
 *     CodeField_ButtonFn button;  // optional row action
 *     void *params;               // borrowed action context
 * FUNCTION REGISTRY:
 * Public Constructors: CodeField_0 / CodeField_1 / CodeField_2 / CodeField_zero
 * Public Core Functions: free, add, insertRow, removeRow, setText, insertText,
 *   deleteBackward, deleteForward, copySelection, layout, paint, hitTest, pointer, handleKey,
 *   scrollVertical, scrollHorizontal, scrollToRow, button_invoke, toString,
 *   toStringStruct. All symbols use the CodeField_ prefix.
 * Public Setters: setRowText, setRowHeight, setRowContent, setIncludeLineNumber,
 *   selection_setRange/selectAll/setColor, button_setFunction, scroll_setOffset/
 *   setBarSize, gutter_setWidth/setButtonWidth/setBackgroundColor/setTextColor,
 *   text_setColor/setRowHeight/setScale, setReadOnly, setFocused, setOnChange, setCtx.
 * Public Getters: getRowCount/getRowText/getRowHeight/getRowContent/getLineNumber/
 *   getIncludeLineNumber/getRowAbsoluteRect, selection_getRange/getColor,
 *   button_getFunction, scroll_getOffset/getExtent/getBarSize, gutter_getWidth/
 *   getMeasuredWidth/getButtonWidth/getBackgroundColor/getTextColor,
 *   text_getColor/getRowHeight/getScale/getPanel, gutter_getPanel, isReadOnly/isFocused/getOnChange/getCtx.
 * Private Constructors: none.
 * Private Core Functions: rowAt, copyText, reserveRows, changed, orderSelection,
 *   textWidth, columnX, clamp, fill, drawText, paintPart, releaseRows, replace,
 *   positionValid, ensureCaret, writeSummary, unitX, unitY.
 * Private Setters/Getters: none.
 */

static char *copyText(const char *text, size_t length);
static CodeFieldRow *rowAt(const CodeField *self, size_t index);
static bool reserveRows(CodeField *self, size_t count);
static void releaseRows(CodeFieldRow *rows, size_t count);
static float clamp(float value, float max);
static float unitX(const CodeField *self);
static float unitY(const CodeField *self);
static float columnX(const CodeField *self, const char *text, size_t column);
static float textWidth(const CodeField *self, const char *text);
static void changed(CodeField *self);
static bool positionValid(const CodeField *self, size_t row, size_t column);
static void orderSelection(const CodeField *self, size_t *ar, size_t *ac, size_t *br, size_t *bc);
static void ensureCaret(CodeField *self);
static bool replace(CodeField *self, size_t ar, size_t ac, size_t br, size_t bc, const char *text);
static void fill(const Rectangle *rect, uint32_t color);
static void drawText(const CodeField *self, const char *text, float x, float y, uint32_t color, const Rectangle *clip);
static bool paintPart(Panel *panel, const Rectangle *rect);
static void writeSummary(const CodeField *self, bool structure, char *dest, size_t cap, bool *outTruncated);

// CONSTRUCTORS (PUBLIC & PRIVATE)

CodeField *CodeField_0(void) {
    CodeField *self = (CodeField*) Memory_alloc(TYPE_CODEFIELD_SINGLETON, sizeof(CodeField));
    if (!self)
        return nullptr;
    Panel *base = Panel_0();
    if (!base) {
        Memory_free(self);
        return nullptr;
    }
    memset(self, 0, sizeof(*self));
    (*self).base = *base;
    Memory_free(base);
    Panel *numberPanel = Panel_0();
    Panel *textPanel = Panel_0();
    if (!numberPanel || !textPanel) {
        Memory_free(numberPanel);
        Memory_free(textPanel);
        Memory_free(self);
        return nullptr;
    }
    (*self).numberPanel = *numberPanel;
    (*self).textPanel = *textPanel;
    Memory_free(numberPanel);
    Memory_free(textPanel);
    Panel_setBackgroundColor(&(*self).textPanel, 0x111B29FFu);
    (*self).textScale = 1.0f;
    (*self).rowHeight = 24.0f;
    (*self).barSize = 10.0f;
    (*self).gutterWidth = 48.0f;
    (*self).buttonWidth = 20.0f;
    (*self).textColor = 0xDEE6F0FFu;
    (*self).highlightColor = 0x305479FFu;
    Panel_setBackgroundColor(&(*self).numberPanel, 0x151D29FFu);
    (*self).gutterTextColor = 0x8391A6FFu;
    Panel_setBackgroundColor(&(*self).base, 0x101722FFu);
    Panel_setBackgroundFn(&(*self).base, paintPart);
    return self;
}

CodeField *CodeField_1(Panel *parent) {
    CodeField *self = CodeField_0();
    if (self && parent)
        Panel_addContainer(parent, &(*self).base);
    return self;
}

CodeField *CodeField_2(Panel *parent, const char *text) {
    CodeField *self = CodeField_0();
    if (self && !CodeField_setText(self, text)) {
        CodeField_free(self);
        return nullptr;
    }
    if (self && parent)
        Panel_addContainer(parent, &(*self).base);
    return self;
}

CodeField *CodeField_zero(void) { return CodeField_0(); }

// CORE FUNCTIONS (PUBLIC & PRIVATE)

static char *copyText(const char *text, size_t length) {
    for (size_t i = 0; i < length; i++) {
        unsigned char c = (unsigned char) text[i];
        if ((c < 32 && c != '\t') || c > 126)
            return nullptr;
    }
    if (length == SIZE_MAX)
        return nullptr;
    char *copy = (char*) Memory_alloc(TYPE_BYTE_ARRAY, length + 1);
    if (copy) {
        memcpy(copy, text, length);
        copy[length] = '\0';
    }
    return copy;
}

static CodeFieldRow *rowAt(const CodeField *self, size_t index) {
    return self && index < (*self).count ? &(*self).rows[index] : nullptr;
}

static bool reserveRows(CodeField *self, size_t count) {
    if (count <= (*self).capacity)
        return true;
    size_t capacity = (*self).capacity ? (*self).capacity : 1;
    while (capacity < count) {
        if (capacity > SIZE_MAX / 2)
            return false;
        capacity *= 2;
    }
    if (capacity > SIZE_MAX / sizeof(CodeFieldRow))
        return false;
    CodeFieldRow *rows = (CodeFieldRow*) Memory_alloc(TYPE_BYTE_ARRAY, capacity * sizeof(CodeFieldRow));
    if (!rows)
        return false;
    if ((*self).count)
        memcpy(rows, (*self).rows, (*self).count * sizeof(CodeFieldRow));
    Memory_free((*self).rows);
    (*self).rows = rows;
    (*self).capacity = capacity;
    return true;
}

static void releaseRows(CodeFieldRow *rows, size_t count) {
    for (size_t i = 0; i < count; i++) {
        CodeFieldRow *row = &rows[i];
        Memory_free((*row).text);
    }
    Memory_free(rows);
}

void CodeField_free(CodeField *self) {
    if (!self)
        return;
    Panel *base = &(*self).base;
    Panel *parent = Panel_getParent(base);
    if (parent)
        Panel_removeChild(parent, base);
    releaseRows((*self).rows, (*self).count);
    Memory_free(self);
}

static float clamp(float value, float max) {
    return fminf(fmaxf(value, 0.0f), fmaxf(max, 0.0f));
}

static float unitX(const CodeField *self) {
    const Panel *base = &(*self).base;
    return fabsf(GraphicsComponent_getScaleX(&(*base).component));
}

static float unitY(const CodeField *self) {
    const Panel *base = &(*self).base;
    return fabsf(GraphicsComponent_getScaleY(&(*base).component));
}

static float columnX(const CodeField *self, const char *text, size_t column) {
    float x = 0.0f;
    float tab = (float) Font_advance(' ') * 4.0f * (*self).textScale * unitX(self);
    for (size_t i = 0; i < column && text[i]; i++)
        x = text[i] == '\t' ? (floorf(x / tab) + 1.0f) * tab : x + (float) Font_advance(text[i]) * (*self).textScale * unitX(self);
    return x;
}

static float textWidth(const CodeField *self, const char *text) { return columnX(self, text, strlen(text)); }

void CodeField_layout(CodeField *self) {
    if (!self)
        return;
    Panel *base = &(*self).base;
    GraphicsComponent *gc = &(*base).component;
    float top = 0.0f, width = 0.0f;
    size_t number = 0;

    for (size_t i = 0; i < (*self).count; i++) {
        CodeFieldRow *row = rowAt(self, i);
        float height = (*row).height;
        float rowWidth = textWidth(self, (*row).text) + 16.0f * unitX(self);
        if ((*row).content) {
            Panel *content = (*row).content;
            GraphicsComponent *geometry = &(*content).component;
            if (Size_isAutoF(height))
                height = GraphicsComponent_getAbsH(geometry);
            rowWidth = fmaxf(rowWidth, GraphicsComponent_getAbsW(geometry) * unitX(self));
        }
        if (Size_isAutoF(height))
            height = (*self).rowHeight;
        (*row).extent = fmaxf(height, (float) Font_lineHeight() * (*self).textScale + 16.0f) * unitY(self);
        (*row).top = top;
        top += (*row).extent;
        (*row).number = (*row).includeLineNumber ? ++number : 0;
        width = fmaxf(width, rowWidth);

    }
    size_t digits = 1;
    for (size_t n = number; n >= 10; n /= 10)
        digits++;
    float gutter = fmaxf((*self).gutterWidth, (float) digits * (float) Font_advance('0') * (*self).textScale + 16.0f) * unitX(self);
    gutter += (*self).buttonWidth * unitX(self);
    float sx = unitX(self), sy = unitY(self);
    if (sx > 0.0f && sy > 0.0f)
        GraphicsComponent_setMeasuredSize(gc, (width + gutter) / sx + (*self).barSize,
                                          top / sy + (*self).barSize);
    float w = fmaxf(0.0f, GraphicsComponent_getAbsW(gc));
    float h = fmaxf(0.0f, GraphicsComponent_getAbsH(gc));
    gutter = fminf(gutter, w);
    (*self).measuredGutterWidth = gutter;
    (*self).contentWidth = width;
    (*self).contentHeight = top;
    (*self).viewport = (Rectangle) { GraphicsComponent_getAbsX(gc) + gutter,
        GraphicsComponent_getAbsY(gc), fmaxf(0.0f, w - gutter - (*self).barSize * unitX(self)),
        fmaxf(0.0f, h - (*self).barSize * unitY(self)) };
    Rectangle *view = &(*self).viewport;
    Panel_setLocation(&(*self).textPanel, (*view).x, (*view).y);
    Panel_setSize(&(*self).textPanel, (*view).width, (*view).height);
    Panel_setLocation(&(*self).numberPanel, (*view).x - gutter, (*view).y);
    Panel_setSize(&(*self).numberPanel, gutter, (*view).height);
    (*self).scrollX = clamp((*self).scrollX, width - (*view).width);
    (*self).scrollY = clamp((*self).scrollY, top - (*view).height);
}

static void changed(CodeField *self) {
    CodeField_layout(self);
    Panel *base = &(*self).base;
    Panel_setBackgroundColor(base, Panel_getBackgroundColor(base));
    if ((*self).onChange)
        (*self).onChange((*self).ctx);
}

bool CodeField_insertRow(CodeField *self, size_t index, const char *text) {
    if (!self || !text || index > (*self).count || (*self).count == SIZE_MAX)
        return false;
    char *copy = copyText(text, strlen(text));
    if (!copy)
        return false;
    if (!reserveRows(self, (*self).count + 1)) {
        Memory_free(copy);
        return false;
    }
    memmove(&(*self).rows[index + 1], &(*self).rows[index], ((*self).count - index) * sizeof(CodeFieldRow));
    CodeFieldRow *row = &(*self).rows[index];
    *row = (CodeFieldRow) { .text = copy, .height = (float) SIZE_AUTO, .includeLineNumber = true };
    (*self).count++;
    (*self).anchorRow = (*self).activeRow = index;
    (*self).anchorColumn = (*self).activeColumn = 0;
    changed(self);
    return true;
}

bool CodeField_add(CodeField *self, const char *text) {
    return self && CodeField_insertRow(self, (*self).count, text);
}

bool CodeField_removeRow(CodeField *self, size_t index) {
    CodeFieldRow *row = rowAt(self, index);
    if (!row)
        return false;
    Memory_free((*row).text);
    memmove(row, row + 1, ((*self).count - index - 1) * sizeof(CodeFieldRow));
    (*self).count--;
    (*self).anchorRow = (*self).activeRow = (*self).count ? ((*self).count - 1) : 0;
    (*self).anchorColumn = (*self).activeColumn = 0;
    changed(self);
    return true;
}

static bool positionValid(const CodeField *self, size_t row, size_t column) {
    CodeFieldRow *slot = rowAt(self, row);
    return slot && column <= strlen((*slot).text);
}

static void orderSelection(const CodeField *self, size_t *ar, size_t *ac, size_t *br, size_t *bc) {
    bool reverse = (*self).anchorRow > (*self).activeRow ||
        ((*self).anchorRow == (*self).activeRow && (*self).anchorColumn > (*self).activeColumn);
    *ar = reverse ? (*self).activeRow : (*self).anchorRow;
    *ac = reverse ? (*self).activeColumn : (*self).anchorColumn;
    *br = reverse ? (*self).anchorRow : (*self).activeRow;
    *bc = reverse ? (*self).anchorColumn : (*self).activeColumn;
}

static void ensureCaret(CodeField *self) {
    CodeField_scrollToRow(self, (*self).activeRow);
    CodeFieldRow *row = rowAt(self, (*self).activeRow);
    if (!row)
        return;
    Rectangle *view = &(*self).viewport;
    float x = columnX(self, (*row).text, (*self).activeColumn) + 8.0f * unitX(self);
    if (x < (*self).scrollX)
        (*self).scrollX = x;
    if (x + 2.0f > (*self).scrollX + (*view).width)
        (*self).scrollX = x + 2.0f - (*view).width;
    CodeField_layout(self);
}

static bool replace(CodeField *self, size_t ar, size_t ac, size_t br, size_t bc, const char *text) {
    if (!self || !text || !positionValid(self, ar, ac) || !positionValid(self, br, bc) || ar > br)
        return false;
    for (size_t i = ar; i <= br; i++)
        if (CodeField_getRowContent(self, i))
            return false;
    CodeFieldRow *first = rowAt(self, ar);
    CodeFieldRow *last = rowAt(self, br);
    size_t length = strlen(text), suffix = strlen((*last).text + bc);
    if (length > SIZE_MAX - ac - 1 || suffix > SIZE_MAX - ac - length - 1)
        return false;
    size_t total = ac + length + suffix;
    char *joined = (char*) Memory_alloc(TYPE_BYTE_ARRAY, total + 1);
    if (!joined)
        return false;
    memcpy(joined, (*first).text, ac);
    memcpy(joined + ac, text, length);
    memcpy(joined + ac + length, (*last).text + bc, suffix + 1);
    size_t count = 1;
    for (size_t i = 0; i < total; i++)
        count += joined[i] == '\n';
    if (count > SIZE_MAX / sizeof(CodeFieldRow)) {
        Memory_free(joined);
        return false;
    }
    CodeFieldRow *fresh = (CodeFieldRow*) Memory_alloc(TYPE_BYTE_ARRAY, count * sizeof(CodeFieldRow));
    if (!fresh) {
        Memory_free(joined);
        return false;
    }
    memset(fresh, 0, count * sizeof(CodeFieldRow));
    size_t start = 0, index = 0, caretRow = ar, caretColumn = ac;
    bool ok = true;
    for (size_t i = 0; i <= total; i++) {
        if (joined[i] != '\n' && joined[i] != '\0')
            continue;
        CodeFieldRow *row = &fresh[index++];
        (*row).text = copyText(joined + start, i - start);
        (*row).height = (float) SIZE_AUTO;
        (*row).includeLineNumber = true;
        if (!(*row).text) {
            ok = false;
            break;
        }
        start = i + 1;
    }
    for (size_t i = 0; i < length; i++) {
        if (text[i] == '\n') {
            caretRow++;
            caretColumn = 0;
        } else {
            caretColumn++;
        }
    }
    Memory_free(joined);
    size_t remaining = (*self).count - (br - ar + 1);
    if (!ok || count > SIZE_MAX - remaining || !reserveRows(self, remaining + count)) {
        releaseRows(fresh, count);
        return false;
    }
    first = rowAt(self, ar);
    CodeFieldRow *head = &fresh[0];
    (*head).includeLineNumber = (*first).includeLineNumber;
    (*head).height = (*first).height;
    (*head).button = (*first).button;
    (*head).params = (*first).params;
    for (size_t i = ar; i <= br; i++) {
        CodeFieldRow *row = rowAt(self, i);
        Memory_free((*row).text);
    }
    memmove(&(*self).rows[ar + count], &(*self).rows[br + 1], ((*self).count - br - 1) * sizeof(CodeFieldRow));
    memcpy(&(*self).rows[ar], fresh, count * sizeof(CodeFieldRow));
    Memory_free(fresh);
    (*self).count = remaining + count;
    (*self).anchorRow = (*self).activeRow = caretRow;
    (*self).anchorColumn = (*self).activeColumn = caretColumn;
    changed(self);
    ensureCaret(self);
    return true;
}

bool CodeField_insertText(CodeField *self, const char *text) {
    if (!self || (*self).readOnly || !text)
        return false;
    if (!(*self).count) {
        if (!CodeField_setText(self, text))
            return false;
        size_t last = (*self).count - 1;
        size_t column = strlen(CodeField_getRowText(self, last));
        CodeField_selection_setRange(self, last, column, last, column);
        ensureCaret(self);
        return true;
    }
    size_t ar, ac, br, bc;
    orderSelection(self, &ar, &ac, &br, &bc);
    return replace(self, ar, ac, br, bc, text);
}

bool CodeField_deleteBackward(CodeField *self) {
    if (!self || (*self).readOnly || !(*self).count)
        return false;
    size_t ar, ac, br, bc;
    orderSelection(self, &ar, &ac, &br, &bc);
    if (ar == br && ac == bc) {
        if (ac)
            ac--;
        else if (ar) {
            ar--;
            ac = strlen(CodeField_getRowText(self, ar));
        } else
            return false;
    }
    return replace(self, ar, ac, br, bc, "");
}

bool CodeField_deleteForward(CodeField *self) {
    if (!self || (*self).readOnly || !(*self).count)
        return false;
    size_t ar, ac, br, bc;
    orderSelection(self, &ar, &ac, &br, &bc);
    if (ar == br && ac == bc) {
        if (bc < strlen(CodeField_getRowText(self, br)))
            bc++;
        else if (br + 1 < (*self).count) {
            br++;
            bc = 0;
        } else
            return false;
    }
    return replace(self, ar, ac, br, bc, "");
}

bool CodeField_copySelection(const CodeField *self, char *dest, size_t cap, bool *outTruncated) {
    if (outTruncated)
        *outTruncated = false;
    if (!self || !dest || !cap) {
        if (outTruncated)
            *outTruncated = true;
        return false;
    }
    size_t used = 0, ar, ac, br, bc;
    orderSelection(self, &ar, &ac, &br, &bc);
    bool truncated = false;
    for (size_t i = ar; i <= br && i < (*self).count; i++) {
        const char *text = CodeField_getRowText(self, i);
        size_t begin = i == ar ? ac : 0;
        size_t end = i == br ? bc : strlen(text);
        for (size_t j = begin; j < end; j++) {
            if (used < cap - 1)
                dest[used++] = text[j];
            else
                truncated = true;
        }
        if (i < br) {
            if (used < cap - 1)
                dest[used++] = '\n';
            else
                truncated = true;
        }
    }
    dest[used] = '\0';
    if (outTruncated)
        *outTruncated = truncated;
    return !truncated;
}

static void fill(const Rectangle *rect, uint32_t color) {
    Brush brush = { color, 1.0f };
    Graphics_fillRect(rect, &brush);
}

static void drawText(const CodeField *self, const char *text, float x, float y, uint32_t color, const Rectangle *clip) {
    Brush brush = { color, 1.0f };
    float advance = 0.0f;
    float tab = (float) Font_advance(' ') * 4.0f * (*self).textScale * unitX(self);
    for (const char *p = text; *p; p++) {
        if (*p == '\t') {
            advance = (floorf(advance / tab) + 1.0f) * tab;
            continue;
        }
        float width = (float) Font_advance(*p) * (*self).textScale * unitX(self);
        Rectangle glyph = { x + advance, y, width, (float) Font_lineHeight() * (*self).textScale * unitY(self) };
        if (Rectangle_intersects(&glyph, clip)) {
            char single[] = { *p, '\0' };
            if ((*self).textScale * unitX(self) == 1.0f && (*self).textScale * unitY(self) == 1.0f)
                Graphics_drawText(&glyph, single, &brush);
            else {
                const uint8_t *bits = Font_glyph(*p);
                if (bits) {
                    // Fixed bitmap format, scaled through backend-neutral rectangles.
                    for (int row = 0; row < 8; row++) {
                        for (int col = 0; col < 8; col++) {
                            if (!(bits[row] & (0x80u >> col)))
                                continue;
                            Rectangle pixel = { glyph.x + (float) col * (*self).textScale * unitX(self),
                                glyph.y + (float) row * (*self).textScale * unitY(self), (*self).textScale * unitX(self), (*self).textScale * unitY(self) };
                            Graphics_fillRect(&pixel, &brush);
                        }
                    }
                }
            }
        }
        advance += width;
        if (x + advance >= (*clip).x + (*clip).width)
            break;
    }
}

bool CodeField_paint(CodeField *self) {
    if (!self)
        return false;
    CodeField_layout(self);
    Panel *base = &(*self).base;
    GraphicsComponent *gc = &(*base).component;
    if (!Panel_isVisible(base))
        return false;
    Rectangle bounds = { GraphicsComponent_getAbsX(gc), GraphicsComponent_getAbsY(gc),
                         GraphicsComponent_getAbsW(gc), GraphicsComponent_getAbsH(gc) };
    Rectangle previous, clip = bounds;
    bool hadClip = Graphics_getClip(&previous);
    if (hadClip)
        Rectangle_intersection(&bounds, &previous, &clip);
    Graphics_clip(&clip);
    fill(&bounds, Panel_getBackgroundColor(base));
    Rectangle *view = &(*self).viewport;
    Rectangle textClip;
    Rectangle_intersection(view, &clip, &textClip);
    Graphics_clip(&textClip);
    fill(view, Panel_getBackgroundColor(&(*self).textPanel));
    size_t ar, ac, br, bc;
    orderSelection(self, &ar, &ac, &br, &bc);
    for (size_t i = 0; i < (*self).count; i++) {
        CodeFieldRow *row = rowAt(self, i);
        Rectangle rect = { (*view).x, (*view).y + (*row).top - (*self).scrollY, (*view).width, (*row).extent };
        if (!Rectangle_intersects(&rect, &textClip))
            continue;
        float x = rect.x + 8.0f * unitX(self) - (*self).scrollX;
        float y = rect.y + 8.0f * unitY(self);
        Rectangle rowClip;
        Rectangle_intersection(&rect, &textClip, &rowClip);
        Graphics_clip(&rowClip);
        if ((*row).content) {
            Panel *content = (*row).content;
            GraphicsComponent *geometry = &(*content).component;
            Rectangle custom = { x, rect.y, GraphicsComponent_getAbsW(geometry), (*row).extent };
            Panel_paintParts(content, &custom);
            Graphics_clip(&rowClip);
        }
        if (i >= ar && i <= br && (ar != br || ac != bc)) {
            size_t start = i == ar ? ac : 0;
            size_t end = i == br ? bc : strlen((*row).text);
            float left = columnX(self, (*row).text, start);
            float right = columnX(self, (*row).text, end);
            if (i < br)
                right += (float) Font_advance(' ') * (*self).textScale * unitX(self);
            Rectangle selection = { x + left, y - 3.0f, right - left, ((float) Font_lineHeight() * (*self).textScale + 6.0f) * unitY(self) };
            fill(&selection, (*self).highlightColor);
        }
        drawText(self, (*row).text, x, y, (*self).textColor, &rowClip);
        if ((*self).focused && i == (*self).activeRow && !(*row).content) {
            Rectangle caret = { x + columnX(self, (*row).text, (*self).activeColumn), y - 2.0f,
                                1.0f, ((float) Font_lineHeight() * (*self).textScale + 4.0f) * unitY(self) };
            fill(&caret, (*self).textColor);
        }
    }
    Graphics_clip(&clip);
    float bar = (*self).barSize;
    Rectangle trackH = { (*view).x, (*view).y + (*view).height, (*view).width, bar * unitY(self) };
    Rectangle trackV = { (*view).x + (*view).width, (*view).y, bar * unitX(self), (*view).height };
    fill(&trackH, 0x1B2636FFu);
    fill(&trackV, 0x1B2636FFu);
    if ((*self).contentWidth > (*view).width && (*view).width > 0.0f) {
        float thumb = (*view).width * (*view).width / (*self).contentWidth;
        Rectangle h = { trackH.x + (*self).scrollX / ((*self).contentWidth - (*view).width) * (trackH.width - thumb),
                        trackH.y + 2.0f, thumb, fmaxf(0.0f, trackH.height - 4.0f) };
        fill(&h, 0x60758FFFu);
    }
    if ((*self).contentHeight > (*view).height && (*view).height > 0.0f) {
        float thumb = (*view).height * (*view).height / (*self).contentHeight;
        Rectangle v = { trackV.x + 2.0f, trackV.y + (*self).scrollY / ((*self).contentHeight - (*view).height) * (trackV.height - thumb),
                        fmaxf(0.0f, trackV.width - 4.0f), thumb };
        fill(&v, 0x60758FFFu);
    }
    Rectangle gutter = { bounds.x, bounds.y, (*self).measuredGutterWidth, bounds.height };
    Rectangle gutterClip;
    Rectangle_intersection(&gutter, &clip, &gutterClip);
    gutterClip.height = fminf(gutterClip.height, fmaxf(0.0f, (*view).y + (*view).height - gutterClip.y));
    fill(&gutter, Panel_getBackgroundColor(&(*self).numberPanel));
    Graphics_clip(&gutterClip);
    for (size_t i = 0; i < (*self).count; i++) {
        CodeFieldRow *row = rowAt(self, i);
        float y = (*view).y + (*row).top - (*self).scrollY;
        Rectangle rowRect = { gutter.x, y, gutter.width, (*row).extent };
        if (!Rectangle_intersects(&rowRect, &gutterClip))
            continue;
        char number[3 * sizeof(size_t) + 1];
        number[0] = '\0';
        if ((*row).number)
            snprintf(number, sizeof(number), "%zu", (*row).number);
        float numberRight = (*view).x - ((*self).buttonWidth + 8.0f) * unitX(self);
        drawText(self, number, numberRight - textWidth(self, number), y + 8.0f * unitY(self), (*self).gutterTextColor, &gutterClip);
        if ((*row).button) {
            Rectangle button = { (*view).x - (*self).buttonWidth * unitX(self), y + 3.0f, (*self).buttonWidth * unitX(self) - 3.0f,
                                 fminf((*row).extent - 6.0f, (*self).rowHeight * unitY(self) - 6.0f) };
            fill(&button, 0x293C53FFu);
            drawText(self, "+", button.x + 4.0f, button.y + 4.0f, 0x8EC9FFFFu, &gutterClip);
        }
    }
    Graphics_clip(hadClip ? &previous : nullptr);
    return true;
}

static bool paintPart(Panel *panel, const Rectangle *rect) {
    (void) rect; // geometry authority is the resolved GraphicsComponent
    return CodeField_paint((CodeField*) panel);
}

void CodeField_scrollVertical(CodeField *self, float delta) {
    if (self && isfinite(delta))
        CodeField_scroll_setOffset(self, (*self).scrollX, (*self).scrollY + delta);
}

void CodeField_scrollHorizontal(CodeField *self, float delta) {
    if (self && isfinite(delta))
        CodeField_scroll_setOffset(self, (*self).scrollX + delta, (*self).scrollY);
}

void CodeField_scrollToRow(CodeField *self, size_t index) {
    CodeFieldRow *row = rowAt(self, index);
    if (!row)
        return;
    CodeField_layout(self);
    Rectangle *view = &(*self).viewport;
    if ((*row).top < (*self).scrollY || (*row).extent > (*view).height)
        (*self).scrollY = (*row).top;
    else if ((*row).top + (*row).extent > (*self).scrollY + (*view).height)
        (*self).scrollY = (*row).top + (*row).extent - (*view).height;
    CodeField_scroll_setOffset(self, (*self).scrollX, (*self).scrollY);
}

bool CodeField_hitTest(CodeField *self, float x, float y, size_t *destRow, size_t *destColumn) {
    if (!self || !isfinite(x) || !isfinite(y) || !Panel_isVisible(&(*self).base))
        return false;
    CodeField_layout(self);
    Rectangle *view = &(*self).viewport;
    if (!Rectangle_containsPoint(view, x, y))
        return false;
    float contentY = y - (*view).y + (*self).scrollY;
    float contentX = x - (*view).x + (*self).scrollX - 8.0f * unitX(self);
    for (size_t i = 0; i < (*self).count; i++) {
        CodeFieldRow *row = rowAt(self, i);
        if (contentY < (*row).top || contentY >= (*row).top + (*row).extent)
            continue;
        size_t column = 0, length = strlen((*row).text);
        while (column < length) {
            float left = columnX(self, (*row).text, column);
            float right = columnX(self, (*row).text, column + 1);
            if (contentX < (left + right) * 0.5f)
                break;
            column++;
        }
        if (destRow)
            *destRow = i;
        if (destColumn)
            *destColumn = column;
        return true;
    }
    return false;
}

bool CodeField_pointer(CodeField *self, float x, float y, bool extendSelection) {
    if (!self || !isfinite(x) || !isfinite(y) || !Panel_isVisible(&(*self).base))
        return false;
    CodeField_layout(self);
    Rectangle *view = &(*self).viewport;
    float right = (*view).x + (*view).width, bottom = (*view).y + (*view).height;
    if (x >= right && x < right + (*self).barSize * unitX(self) && y >= (*view).y && y < bottom && (*view).height > 0.0f) {
        CodeField_scroll_setOffset(self, (*self).scrollX, (y - (*view).y) / (*view).height * (*self).contentHeight - (*view).height * 0.5f);
        return true;
    }
    if (y >= bottom && y < bottom + (*self).barSize * unitY(self) && x >= (*view).x && x < right && (*view).width > 0.0f) {
        CodeField_scroll_setOffset(self, (x - (*view).x) / (*view).width * (*self).contentWidth - (*view).width * 0.5f, (*self).scrollY);
        return true;
    }
    if (!extendSelection && x >= (*view).x - (*self).buttonWidth * unitX(self) && x < (*view).x && y >= (*view).y && y < bottom) {
        float contentY = y - (*view).y + (*self).scrollY;
        for (size_t i = 0; i < (*self).count; i++) {
            CodeFieldRow *row = rowAt(self, i);
            if (contentY >= (*row).top && contentY < (*row).top + (*row).extent)
                return CodeField_button_invoke(self, i);
        }
    }
    size_t row, column;
    if (!CodeField_hitTest(self, x, y, &row, &column) || CodeField_getRowContent(self, row))
        return false;
    CodeField_setFocused(self, true);
    return CodeField_selection_setRange(self, extendSelection ? (*self).anchorRow : row,
                                        extendSelection ? (*self).anchorColumn : column, row, column);
}

bool CodeField_button_invoke(CodeField *self, size_t index) {
    CodeFieldRow *row = rowAt(self, index);
    if (!row || !(*row).button)
        return false;
    (*row).button((*row).params, index, (*row).number);
    return true;
}

static void writeSummary(const CodeField *self, bool structure, char *dest, size_t cap, bool *outTruncated) {
    int n = 0;
    if (!dest || !cap) {
        if (outTruncated)
            *outTruncated = true;
        return;
    }
    if (!self)
        n = snprintf(dest, cap, "nullptr");
    else if (!structure)
        n = snprintf(dest, cap, "CodeField(rows=%zu, scroll=(%.1f,%.1f))", (*self).count, (*self).scrollX, (*self).scrollY);
    else
        n = snprintf(dest, cap,
            "CodeField{base=Panel,numberPanel=Panel,textPanel=Panel,rows=%p,count=%zu,capacity=%zu,onChange=%s,ctx=%p,readOnly=%d,focused=%d,"
            "rowHeight=%g,textColor=%08x,textScale=%g,anchorRow=%zu,anchorColumn=%zu,activeRow=%zu,activeColumn=%zu,"
            "highlightColor=%08x,scrollX=%g,scrollY=%g,contentWidth=%g,contentHeight=%g,viewport=Rectangle(%g,%g,%g,%g),"
            "barSize=%g,gutterWidth=%g,measuredGutterWidth=%g,buttonWidth=%g,gutterTextColor=%08x}",
            (void*) (*self).rows, (*self).count, (*self).capacity, (*self).onChange ? "set" : "null", (*self).ctx,
            (*self).readOnly, (*self).focused, (*self).rowHeight, (*self).textColor, (*self).textScale,
            (*self).anchorRow, (*self).anchorColumn, (*self).activeRow, (*self).activeColumn,
            (*self).highlightColor, (*self).scrollX, (*self).scrollY, (*self).contentWidth, (*self).contentHeight,
            Rectangle_getX(&(*self).viewport), Rectangle_getY(&(*self).viewport),
            Rectangle_getWidth(&(*self).viewport), Rectangle_getHeight(&(*self).viewport),
            (*self).barSize, (*self).gutterWidth, (*self).measuredGutterWidth, (*self).buttonWidth,
            (*self).gutterTextColor);
    if (outTruncated)
        *outTruncated = n < 0 || (size_t) n >= cap;
}

void CodeField_toString(const CodeField *self, char *dest, size_t cap, bool *outTruncated) {
    writeSummary(self, false, dest, cap, outTruncated);
}

void CodeField_toStringStruct(const CodeField *self, char *dest, size_t cap, bool *outTruncated) {
    writeSummary(self, true, dest, cap, outTruncated);
}

bool CodeField_handleKey(CodeField *self, CodeFieldKey key, bool extendSelection) {
    if (!self || !(*self).focused)
        return false;
    if (key == CODEFIELD_KEY_ENTER)
        return CodeField_insertText(self, "\n");
    if (key == CODEFIELD_KEY_TAB)
        return CodeField_insertText(self, "\t");
    if (key == CODEFIELD_KEY_BACKSPACE)
        return CodeField_deleteBackward(self);
    if (key == CODEFIELD_KEY_DELETE)
        return CodeField_deleteForward(self);
    if (!(*self).count)
        return false;
    size_t row = (*self).activeRow, column = (*self).activeColumn;
    size_t length = strlen(CodeField_getRowText(self, row));
    switch (key) {
        case CODEFIELD_KEY_LEFT:
            if (column)
                column--;
            else if (row) {
                row--;
                column = strlen(CodeField_getRowText(self, row));
            }
            break;
        case CODEFIELD_KEY_RIGHT:
            if (column < length)
                column++;
            else if (row + 1 < (*self).count) {
                row++;
                column = 0;
            }
            break;
        case CODEFIELD_KEY_UP:
            if (row)
                row--;
            break;
        case CODEFIELD_KEY_DOWN:
            if (row + 1 < (*self).count)
                row++;
            break;
        case CODEFIELD_KEY_HOME: column = 0; break;
        case CODEFIELD_KEY_END: column = length; break;
        default: return false;
    }
    size_t targetLength = strlen(CodeField_getRowText(self, row));
    if (column > targetLength)
        column = targetLength;
    CodeField_selection_setRange(self, extendSelection ? (*self).anchorRow : row,
                                  extendSelection ? (*self).anchorColumn : column, row, column);
    ensureCaret(self);
    return true;
}

// SETTERS (PUBLIC & PRIVATE)

;;SETTER
bool CodeField_setText(CodeField *self, const char *text) {
    if (!self || !text)
        return false;
    CodeField *temp = CodeField_0();
    if (!temp)
        return false;
    bool ok = CodeField_add(temp, "") && replace(temp, 0, 0, 0, 0, text);
    if (ok) {
        releaseRows((*self).rows, (*self).count);
        (*self).rows = (*temp).rows;
        (*self).count = (*temp).count;
        (*self).capacity = (*temp).capacity;
        (*temp).rows = nullptr;
        (*temp).count = 0;
        (*self).anchorRow = (*self).activeRow = 0;
        (*self).anchorColumn = (*self).activeColumn = 0;
        (*self).scrollX = (*self).scrollY = 0.0f;
    }
    CodeField_free(temp);
    if (ok)
        changed(self);
    return ok;
}

;;SETTER
bool CodeField_setRowText(CodeField *self, size_t index, const char *text) {
    CodeFieldRow *row = rowAt(self, index);
    if (!row || !text)
        return false;
    char *copy = copyText(text, strlen(text));
    if (!copy)
        return false;
    Memory_free((*row).text);
    (*row).text = copy;
    (*self).anchorRow = (*self).activeRow = index;
    (*self).anchorColumn = (*self).activeColumn = 0;
    changed(self);
    return true;
}

;;SETTER
bool CodeField_setIncludeLineNumber(CodeField *self, size_t index, bool flag) {
    CodeFieldRow *row = rowAt(self, index);
    if (!row)
        return false;
    (*row).includeLineNumber = flag;
    changed(self);
    return true;
}

;;SETTER
bool CodeField_setRowHeight(CodeField *self, size_t index, float height) {
    CodeFieldRow *row = rowAt(self, index);
    if (!row || !isfinite(height) || (!Size_isAutoF(height) && height < (float) Font_lineHeight()))
        return false;
    (*row).height = height;
    changed(self);
    return true;
}

;;SETTER
bool CodeField_setRowContent(CodeField *self, size_t index, Panel *content) {
    CodeFieldRow *row = rowAt(self, index);
    if (!row || content == &(*self).base || (content && Panel_getBackgroundFn(content) == paintPart))
        return false;
    (*row).content = content;
    changed(self);
    return true;
}

;;SETTER
bool CodeField_selection_setRange(CodeField *self, size_t ar, size_t ac, size_t br, size_t bc) {
    if (!positionValid(self, ar, ac) || !positionValid(self, br, bc))
        return false;
    (*self).anchorRow = ar;
    (*self).anchorColumn = ac;
    (*self).activeRow = br;
    (*self).activeColumn = bc;
    Panel *base = &(*self).base;
    Panel_setBackgroundColor(base, Panel_getBackgroundColor(base));
    return true;
}

;;SETTER
void CodeField_selection_selectAll(CodeField *self) {
    if (self && (*self).count) {
        size_t last = (*self).count - 1;
        CodeField_selection_setRange(self, 0, 0, last, strlen(CodeField_getRowText(self, last)));
    }
}

;;SETTER
bool CodeField_button_setFunction(CodeField *self, size_t index, CodeField_ButtonFn fn, void *params) {
    CodeFieldRow *row = rowAt(self, index);
    if (!row)
        return false;
    (*row).button = fn;
    (*row).params = params;
    changed(self);
    return true;
}

;;SETTER
void CodeField_scroll_setOffset(CodeField *self, float x, float y) {
    if (!self || !isfinite(x) || !isfinite(y))
        return;
    (*self).scrollX = x;
    (*self).scrollY = y;
    CodeField_layout(self);
    Panel *base = &(*self).base;
    Panel_setBackgroundColor(base, Panel_getBackgroundColor(base));
}

;;SETTER
void CodeField_selection_setColor(CodeField *self, uint32_t value) {
    if (!self)
        return;
    (*self).highlightColor = value;
    changed(self);
}

;;SETTER
void CodeField_scroll_setBarSize(CodeField *self, float value) {
    if (!self || !isfinite(value) || value < 0.0f)
        return;
    (*self).barSize = value;
    changed(self);
}

;;SETTER
void CodeField_gutter_setWidth(CodeField *self, float value) {
    if (!self || !isfinite(value) || value < 0.0f)
        return;
    (*self).gutterWidth = value;
    changed(self);
}

;;SETTER
void CodeField_gutter_setButtonWidth(CodeField *self, float value) {
    if (!self || !isfinite(value) || value < 0.0f)
        return;
    (*self).buttonWidth = value;
    changed(self);
}

;;SETTER
void CodeField_gutter_setBackgroundColor(CodeField *self, uint32_t value) {
    if (!self)
        return;
    Panel_setBackgroundColor(&(*self).numberPanel, value);
    changed(self);
}

;;SETTER
void CodeField_gutter_setTextColor(CodeField *self, uint32_t value) {
    if (!self)
        return;
    (*self).gutterTextColor = value;
    changed(self);
}

;;SETTER
void CodeField_text_setColor(CodeField *self, uint32_t value) {
    if (!self)
        return;
    (*self).textColor = value;
    changed(self);
}

;;SETTER
void CodeField_text_setRowHeight(CodeField *self, float value) {
    if (!self || !isfinite(value) || value < Font_lineHeight())
        return;
    (*self).rowHeight = value;
    changed(self);
}

;;SETTER
void CodeField_setReadOnly(CodeField *self, bool value) {
    if (!self)
        return;
    (*self).readOnly = value;
}

;;SETTER
void CodeField_setFocused(CodeField *self, bool value) {
    if (!self)
        return;
    (*self).focused = value;
    Panel *base = &(*self).base;
    Panel_setBackgroundColor(base, Panel_getBackgroundColor(base));
}

;;SETTER
void CodeField_setOnChange(CodeField *self, CodeField_ChangeFn value) {
    if (!self)
        return;
    (*self).onChange = value;
}

;;SETTER
void CodeField_setCtx(CodeField *self, void * value) {
    if (!self)
        return;
    (*self).ctx = value;
}

;;SETTER
void CodeField_text_setScale(CodeField *self, float scale) {
    if (!self || !isfinite(scale) || scale <= 0.0f)
        return;
    (*self).textScale = scale;
    changed(self);
}

// GETTERS (PUBLIC & PRIVATE)

;;GETTER
size_t CodeField_getRowCount(const CodeField *self) { return self ? (*self).count : 0; }

;;GETTER
const char *CodeField_getRowText(const CodeField *self, size_t index) {
    CodeFieldRow *row = rowAt(self, index);
    return row ? (*row).text : nullptr;
}

;;GETTER
bool CodeField_getIncludeLineNumber(const CodeField *self, size_t index) {
    CodeFieldRow *row = rowAt(self, index);
    return row && (*row).includeLineNumber;
}

;;GETTER
size_t CodeField_getLineNumber(const CodeField *self, size_t index) {
    CodeFieldRow *row = rowAt(self, index);
    return row ? (*row).number : 0;
}

;;GETTER
float CodeField_getRowHeight(const CodeField *self, size_t index) {
    CodeFieldRow *row = rowAt(self, index);
    return row ? (*row).height : 0.0f;
}

;;GETTER
Panel *CodeField_getRowContent(const CodeField *self, size_t index) {
    CodeFieldRow *row = rowAt(self, index);
    return row ? (*row).content : nullptr;
}

;;GETTER
bool CodeField_getRowAbsoluteRect(CodeField *self, size_t index, Rectangle *dest) {
    CodeFieldRow *row = rowAt(self, index);
    if (!row || !dest)
        return false;
    CodeField_layout(self);
    Rectangle *view = &(*self).viewport;
    *dest = (Rectangle) { (*view).x - (*self).scrollX, (*view).y + (*row).top - (*self).scrollY,
                         (*self).contentWidth, (*row).extent };
    return true;
}

;;GETTER
CodeField_ButtonFn CodeField_button_getFunction(const CodeField *self, size_t index, void **destParams) {
    CodeFieldRow *row = rowAt(self, index);
    if (destParams)
        *destParams = row ? (*row).params : nullptr;
    return row ? (*row).button : nullptr;
}

;;GETTER
void CodeField_selection_getRange(const CodeField *self, size_t *ar, size_t *ac, size_t *br, size_t *bc) {
    if (ar)
        *ar = self ? (*self).anchorRow : 0;
    if (ac)
        *ac = self ? (*self).anchorColumn : 0;
    if (br)
        *br = self ? (*self).activeRow : 0;
    if (bc)
        *bc = self ? (*self).activeColumn : 0;
}

;;GETTER
void CodeField_scroll_getOffset(const CodeField *self, float *x, float *y) {
    if (x)
        *x = self ? (*self).scrollX : 0.0f;
    if (y)
        *y = self ? (*self).scrollY : 0.0f;
}

;;GETTER
void CodeField_scroll_getExtent(const CodeField *self, float *width, float *height) {
    if (width)
        *width = self ? (*self).contentWidth : 0.0f;
    if (height)
        *height = self ? (*self).contentHeight : 0.0f;
}

;;GETTER
uint32_t CodeField_selection_getColor(const CodeField *self) { return self ? (*self).highlightColor : 0; }

;;GETTER
float CodeField_scroll_getBarSize(const CodeField *self) { return self ? (*self).barSize : 0; }

;;GETTER
float CodeField_gutter_getWidth(const CodeField *self) { return self ? (*self).gutterWidth : 0; }

;;GETTER
float CodeField_gutter_getButtonWidth(const CodeField *self) { return self ? (*self).buttonWidth : 0; }

;;GETTER
uint32_t CodeField_gutter_getBackgroundColor(const CodeField *self) { return self ? Panel_getBackgroundColor(&(*self).numberPanel) : 0; }

;;GETTER
uint32_t CodeField_gutter_getTextColor(const CodeField *self) { return self ? (*self).gutterTextColor : 0; }

;;GETTER
uint32_t CodeField_text_getColor(const CodeField *self) { return self ? (*self).textColor : 0; }

;;GETTER
float CodeField_text_getRowHeight(const CodeField *self) { return self ? (*self).rowHeight : 0; }

;;GETTER
bool CodeField_isReadOnly(const CodeField *self) { return self ? (*self).readOnly : 0; }

;;GETTER
bool CodeField_isFocused(const CodeField *self) { return self ? (*self).focused : 0; }

;;GETTER
CodeField_ChangeFn CodeField_getOnChange(const CodeField *self) { return self ? (*self).onChange : 0; }

;;GETTER
void * CodeField_getCtx(const CodeField *self) { return self ? (*self).ctx : 0; }

;;GETTER
float CodeField_gutter_getMeasuredWidth(const CodeField *self) { return self ? (*self).measuredGutterWidth : 0.0f; }

;;GETTER
float CodeField_text_getScale(const CodeField *self) { return self ? (*self).textScale : 0.0f; }

;;GETTER
Panel *CodeField_text_getPanel(CodeField *self) { return self ? &(*self).textPanel : nullptr; }

;;GETTER
Panel *CodeField_gutter_getPanel(CodeField *self) { return self ? &(*self).numberPanel : nullptr; }
