#include "darling/panel/markdown_panel.h"

#include "darling/cursor/cursor.h"
#include "darling/label/label.h"
#include "darling/label/rich_label.h"
#include "darling/panel/panel.h"
#include "event/pointer.h"
#include "annotation/overview.h"
#include "input/key.h"
#include "nio/mem.h"
#include "oop/type.h"
#include "primitive/string.h"
#include "text/rich_text.h"
#include "text/text_core.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: MarkdownPanel (embeds Panel)
 * LEVEL: L2 — Behavior (markdown-fed document panel behavior API)
 * ============================================================================
 * Takes a markdown string, scans it with a zero-alloc line walker over the
 * vexspoke primitive/string block (index arithmetic only — no malloc in the
 * scan path), and builds an owned row list of Label/RichLabel children
 * inside an inner Panel container. v1 syntax: hash headings, dash/star
 * bullets, backtick inline code, fenced blocks, bold and italic spans via
 * RichText styles. Rebuild is detach-all + re-layout (cold path); row payloads use
 * Memory_alloc like the rest of the tree.
 *
 * Inline markup becomes RichLabel rows only when a Font is set (RichText
 * layout needs a font for quads); without one those lines fall back to
 * plain Labels with markers stripped. RichText span tags use two-digit
 * style ids ([00]/[01]/...) because the RichText parser reserves
 * single-char tokens for n/l/c/r/j. Rows live in a vertical ListContainer,
 * which owns Y positions; stackRow sizes rows and advances the height
 * cursor for the panel's own height accounting.
 *
 * STRUCT FIELDS (Mirroring darling/panel/markdown_panel.h):
 * ----------------------------------------------------------------------------
 *   Panel base;                        // Inherited layout/tree/background state
 *   uint8_t *textBlock;                // Owned vexspoke string block (source)
 *   Font *font;                        // Aliased font for RichText rows; nullable
 *   uint32_t codeBackground;           // Fill color behind fenced code rows
 *   ListContainer *rows;                   // Owned vertical ListContainer of rows
 *   float rowSpacing;                  // Vertical gap between stacked rows
 *   struct MarkdownRowSlot *slots;     // Owned row records (see SLOT RECORD)
 *   size_t rowCount;                   // Active row record count
 *   size_t rowCapacity;                // Row record capacity
 *   bool highlightable;                // Enables document drag selection (no caret)
 *   TextSelect select;                 // Shared selection part (doc-byte space + hover)
 *   uint32_t highlightColor;           // Packed 0xAARRGGBB selection fill (0x662563EB)
 *
 * SLOT RECORD (file-local, behaviorless; all behavior hangs off MarkdownPanel):
 * ----------------------------------------------------------------------------
 *   struct MarkdownRowSlot {
 *     Panel *panel;                    // Row base (Label* or RichLabel* payload)
 *     RichText *model;                 // Owned RichText model, or nullptr for Labels
 *     uint8_t isRich;                  // 1 = RichLabel row, 0 = Label row
 *     float height;                    // Final row height (stacking + hit-testing)
 *     uint32_t cellStart;              // Row start offset in doc selection space
 *     uint32_t textLen;                // Row text length (visible/model bytes)
 *   }
 *
 * SELECTION SIMPLIFICATION (Rule 33 managed exception, Tier 1 preserved):
 *   Document selection aggregates each row's own text length into one
 *   contiguous byte space (slot k starts at the sum of textLen of rows 0..k-1)
 *   on the rendered text, NOT source-byte offsets. Both row types expose their
 *   charIndexAt/setSelection in exactly this per-row space (Label visible
 *   bytes, RichLabel model raw-string bytes), so down→charIndex, clamp across
 *   rows, and mirror per-row sel become O(1) translations — no tag/marker
 *   reverse-mapping, no allocation on the pointer path. getSelection returns
 *   positions in this rendered-text space; a selection that spans rows simply
 *   covers the whole text of the rows it lands on.
 *
 * SELECTION COPY (display-accurate, cold path):
 *   getSelectedText walks the committed span one row at a time and emits the
 *   rendered bytes the user sees: Label rows slice their stored display string
 *   (markers already stripped at row build; the "• " bullet literal lives at
 *   [0,4) of the row string and is included when the selection touches it),
 *   RichLabel rows strip [nn]/style tags and unescape \[ back to '[' from the
 *   tagged model string. This matches the SELECTION SIMPLIFICATION space 1:1,
 *   so the copy never re-runs markdown parsing; source-offset slot fields
 *   (srcStart/srcSkip/srcLen) were considered and rejected — they would be
 *   dead structure since the stored row strings are already display strings.
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - MarkdownPanel_0(void)
 *   - MarkdownPanel_1(text)
 *
 * Core Functions:
 *   - MarkdownPanel_free(s)
 *   - MarkdownPanel_handlePointer(s, kind, localX, localY, window) : document selection
 *   - MarkdownPanel_handleKey(s, ev)                               : key event (Cmd+C copy / Cmd+V paste)
 *   - MarkdownPanel_getSelectedText(s)                             : display-accurate copy
 *
 * Setters:
 *   - MarkdownPanel_setText(s, text)
 *   - MarkdownPanel_setFont(s, font)
 *   - MarkdownPanel_setCodeBackground(s, color)
 *   - MarkdownPanel_setRowSpacing(s, spacing)
 *   - MarkdownPanel_setLocation(s, x, y)
 *   - MarkdownPanel_setSize(s, w, h)
 *   - MarkdownPanel_setBackgroundColor(s, color)
 *   - MarkdownPanel_setHighlightable(s, flag)
 *   - MarkdownPanel_setSelection(s, start, end)
 *   - MarkdownPanel_setHighlightColor(s, color)
 *   - MarkdownPanel_setHighlightColorRGBA(s, r, g, b, a)
 *
 * Getters:
 *   - MarkdownPanel_getText(s)
 *   - MarkdownPanel_getFont(s)
 *   - MarkdownPanel_getCodeBackground(s)
 *   - MarkdownPanel_getRows(s)
 *   - MarkdownPanel_getRowSpacing(s)
 *   - MarkdownPanel_getRowCount(s)
 *   - MarkdownPanel_getRow(s, index)
 *   - MarkdownPanel_isHighlightable(s)
 *   - MarkdownPanel_getSelection(s, outStart, outEnd)   : always ordered, null-safe
 *   - MarkdownPanel_getHighlightColor(s)
 *   - MarkdownPanel_getHighlightColorRGBA(s, outR, outG, outB, outA)
 * ============================================================================
 */

struct MarkdownRowSlot {
    Panel *panel;
    RichText *model;
    uint8_t isRich;
    float height;
    uint32_t cellStart;
    uint32_t textLen;
};

#define MARKDOWN_BASE_SIZE 14.0f
#define MARKDOWN_H1_SIZE 28.0f
#define MARKDOWN_H2_SIZE 22.0f
#define MARKDOWN_H3_SIZE 17.0f
#define MARKDOWN_H4_SIZE 15.0f
#define MARKDOWN_CODE_SIZE 12.0f
#define MARKDOWN_LINE_FACTOR 1.35f
#define MARKDOWN_TEXT_COLOR 0xFFFFFFFFu
#define MARKDOWN_CODE_BACKGROUND 0xFF222222u
#define MARKDOWN_DEFAULT_SPACING 4.0f

typedef enum {
    LINE_BLANK,
    LINE_FENCE,
    LINE_HEADING,
    LINE_BULLET,
    LINE_PARA
} LineKind;

// CONSTRUCTORS
// ============================================================================

MarkdownPanel *MarkdownPanel_0(void) {
    MarkdownPanel *s = (MarkdownPanel*) Memory_alloc(TYPE_MARKDOWN_PANEL_SINGLETON, sizeof(MarkdownPanel));
    if (!s)
        return nullptr;
    Panel *b = Panel_0();
    if (!b) {
        Memory_free(s);
        return nullptr;
    }
    (*s).base = (*b);
    Memory_free(b);
    ListContainer *box = ListContainer_0();
    if (!box) {
        Memory_free(s);
        return nullptr;
    }
    (*s).textBlock = nullptr;
    (*s).font = nullptr;
    (*s).codeBackground = MARKDOWN_CODE_BACKGROUND;
    (*s).rows = box;
    (*s).rowSpacing = MARKDOWN_DEFAULT_SPACING;
    (*s).highlightable = false;
    (*s).select = TextSelect_default();
    (*s).highlightColor = 0x662563EBu;
    (*s).textAlign = TEXT_ALIGN_LEFT;
    (*s).spacingWidth = 0.0f;
    (*s).spacingHeight = 0.0f;
    (*s).ligatures = true;
    ListContainer_setSpacing(box, MARKDOWN_DEFAULT_SPACING);
    (*s).slots = nullptr;
    (*s).rowCount = 0;
    (*s).rowCapacity = 0;
    Panel *self = &(*s).base;
    Panel_addContainer(self, &(*box).base);
    return s;
}

MarkdownPanel *MarkdownPanel_1(const char *text) {
    MarkdownPanel *s = MarkdownPanel_0();
    if (s)
        MarkdownPanel_setText(s, text);
    return s;
}

// CORE FUNCTIONS
// ============================================================================

static void markDirty(MarkdownPanel *s) {
    if (!s)
        return;
    Panel *b = &(*s).base;
    Container *c = &(*b).base;
    Container_markDirty(c);
}

static bool pushSlot(MarkdownPanel *s, Panel *row, RichText *model, uint8_t isRich, float height, uint32_t cellStart, uint32_t textLen) {
    if (!s || !row)
        return false;
    if ((*s).rowCount >= (*s).rowCapacity) {
        size_t grown = (*s).rowCapacity == 0 ? 8 : (*s).rowCapacity * 2;
        struct MarkdownRowSlot *next = (struct MarkdownRowSlot*) Memory_realloc((*s).slots, grown * sizeof(struct MarkdownRowSlot));
        if (!next)
            return false;
        (*s).slots = next;
        (*s).rowCapacity = grown;
    }
    struct MarkdownRowSlot *slots = (*s).slots;
    size_t at = (*s).rowCount;
    slots[at].panel = row;
    slots[at].model = model;
    slots[at].isRich = isRich;
    slots[at].height = height;
    slots[at].cellStart = cellStart;
    slots[at].textLen = textLen;
    (*s).rowCount = at + 1;
    return true;
}

static void clearRows(MarkdownPanel *s) {
    if (!s)
        return;
    ListContainer *box = (*s).rows;
    if (box) {
        while (ListContainer_count(box) > 0)
            ListContainer_remove(box, 0);
    }
    struct MarkdownRowSlot *slots = (*s).slots;
    size_t n = (*s).rowCount;
    for (size_t i = 0; i < n; i++) {
        struct MarkdownRowSlot *slot = &slots[i];
        RichText *model = (*slot).model;
        Panel *row = (*slot).panel;
        uint8_t rich = (*slot).isRich;
        if (model)
            RichText_free(model);
        if (row) {
            if (rich)
                Memory_free(row);
            else {
                Label *lbl = (Label*) row;
                Label_free(lbl);
            }
        }
    }
    if (slots)
        Memory_free(slots);
    (*s).slots = nullptr;
    (*s).rowCount = 0;
    (*s).rowCapacity = 0;
}

// Maps a point to a row's own text index (visible byte space for Label rows,
// model raw-string bytes for RichLabel rows). Both spaces are what the row's
// charIndexAt and setSelection already speak.
static int32_t markdownRowIndexAt(MarkdownPanel *s, size_t rowIndex, float localX, float localY, float rowY) {
    struct MarkdownRowSlot *slots = (*s).slots;
    struct MarkdownRowSlot *slot = &slots[rowIndex];
    if ((*slot).isRich) {
        RichLabel *rl = (RichLabel*) (*slot).panel;
        return RichLabel_charIndexAt(rl, localX, localY - rowY);
    }
    Label *lbl = (Label*) (*slot).panel;
    return Label_charIndexAt(lbl, localX);
}

// Document selection index: rows aggregate their text lengths into a single
// contiguous byte space (slot k starts at the sum of textLen of rows 0..k-1),
// so a drag that spans rows is one monotonic index. Rows stack at y=0 with
// height + rowSpacing, mirroring ListContainer's vertical layout.
static int32_t markdownDocIndexAt(MarkdownPanel *s, float localX, float localY) {
    if (!s)
        return 0;
    size_t n = (*s).rowCount;
    if (n == 0)
        return 0;
    struct MarkdownRowSlot *slots = (*s).slots;
    float spacing = (*s).rowSpacing;

    // Vertical clamping & midpoint boundary resolution:
    // Pointers above row 0 clamp to row 0; pointers below the last row clamp to row n-1.
    // Inter-row gaps divide at their halfway point to eliminate dead zones.
    size_t ri = 0;
    float accY = 0.0f;
    if (localY <= 0.0f) {
        ri = 0;
    } else {
        ri = n - 1;
        for (size_t i = 0; i < n; i++) {
            float h = slots[i].height;
            float boundary = accY + h + (i + 1 < n ? spacing * 0.5f : 0.0f);
            if (localY < boundary) {
                ri = i;
                break;
            }
            accY += h + spacing;
        }
    }
    struct MarkdownRowSlot *slot = &slots[ri];
    int32_t idx = 0;
    if (localY <= 0.0f && localX <= 0.0f) {
        idx = 0;
    } else if (localY >= accY + (*slot).height && localX >= 0.0f && ri == n - 1) {
        idx = (int32_t) (*slot).textLen;
    } else {
        idx = markdownRowIndexAt(s, ri, localX, localY, accY);
    }
    uint32_t cellStart = (*slot).cellStart;
    uint32_t textLen = (*slot).textLen;
    if (idx < 0)
        idx = 0;
    if ((uint32_t) idx > textLen)
        idx = (int32_t) textLen;
    return (int32_t)(cellStart + (uint32_t) idx);
}

// Clamps a normalized document range onto one row's own text and mirrors it
// into the row's selection (with the panel's highlight color). Rows whose
// span AND color already match are skipped: a drag fires per pointer event,
// and re-rasterizing every row per event (even untouched ones) collapses the
// present rate over a long multi-row hold.
static void applyRowSelection(MarkdownPanel *s, size_t rowIndex, int32_t lo, int32_t hi) {
    struct MarkdownRowSlot *slots = (*s).slots;
    struct MarkdownRowSlot *slot = &slots[rowIndex];
    int32_t a = lo - (int32_t) (*slot).cellStart;
    int32_t b = hi - (int32_t) (*slot).cellStart;
    if (a < 0)
        a = 0;
    int32_t textLen = (int32_t) (*slot).textLen;
    if (b > textLen)
        b = textLen;
    bool hit = a < b;
    uint32_t color = (*s).highlightColor;
    int32_t wantA = hit ? a : -1;
    int32_t wantB = hit ? b : -1;
    if ((*slot).isRich) {
        RichLabel *rl = (RichLabel*) (*slot).panel;
        int32_t curA = -1, curB = -1;
        RichLabel_getSelection(rl, &curA, &curB);
        if (RichLabel_getHighlightColor(rl) == color && curA == wantA && curB == wantB)
            return;
        RichLabel_setHighlightColor(rl, color);
        if (hit)
            RichLabel_setSelection(rl, a, b);
        else
            RichLabel_setSelection(rl, -1, -1);
    } else {
        Label *lbl = (Label*) (*slot).panel;
        int32_t curA = -1, curB = -1;
        Label_getSelection(lbl, &curA, &curB);
        if (Label_getHighlightColor(lbl) == color && curA == wantA && curB == wantB)
            return;
        Label_setHighlightColor(lbl, color);
        if (hit)
            Label_setSelection(lbl, a, b);
        else
            Label_setSelection(lbl, -1, -1);
    }
}

// Mirrors the panel selection onto every row after any anchor/active change.
static void refreshRowSelection(MarkdownPanel *s) {
    if (!s)
        return;
    int32_t lo = -1, hi = -1;
    (void) TextSelect_getSpan(&(*s).select, &lo, &hi);
    if (lo < 0) {
        lo = 0;
        hi = 0;
    }
    size_t n = (*s).rowCount;
    for (size_t i = 0; i < n; i++)
        applyRowSelection(s, i, lo, hi);
    markDirty(s);
}

void MarkdownPanel_handlePointer(MarkdownPanel *s, int32_t kind, float localX, float localY, void *window) {
    if (!s)
        return;
    if ((*s).rowCount == 0)
        return;
    Panel *b = &(*s).base;
    Container *c = &(*b).base;
    float w = Container_getWidth(c);
    float h = Container_getHeight(c);
    bool inside = localX >= 0.0f && localY >= 0.0f && localX <= w && localY <= h;

    // Shared hover caret-cursor lifecycle (TextSelect part): flip the hovered
    // flag and drive I-beam / default cursor. Hovering never touches
    // anchor/active and LEAVE never clears an in-progress selection.
    if (kind == PTR_LEAVE || (!inside && (kind == PTR_MOVE || kind == PTR_HOVER))) {
        if (TextSelect_setHovered(&(*s).select, false)) {
            if (window) {
                Cursor *defCursor = Cursor_getPredefined(CURSOR_DEFAULT);
                Cursor_apply(defCursor, window);
            }
            markDirty(s);
        }
        return;
    }

    if (inside && (kind == PTR_ENTER || kind == PTR_MOVE || kind == PTR_HOVER)) {
        if (TextSelect_setHovered(&(*s).select, true)) {
            if ((*s).highlightable && window) {
                Cursor *ibeam = Cursor_getPredefined(CURSOR_IBEAM);
                Cursor_apply(ibeam, window);
            }
            markDirty(s);
        } else if ((*s).highlightable && window) {
            Cursor *ibeam = Cursor_getPredefined(CURSOR_IBEAM);
            Cursor_apply(ibeam, window);
        }
    }

    if (!(*s).highlightable)
        return;
    if (kind == PTR_DOWN) {
        if (inside) {
            int32_t idx = markdownDocIndexAt(s, localX, localY);
            TextSelect_begin(&(*s).select, idx);
        } else {
            TextSelect_cancel(&(*s).select);
        }
        refreshRowSelection(s);
        return;
    }
    if (kind == PTR_DRAG) {
        if (!TextSelect_isActive(&(*s).select))
            return;
        if (TextSelect_drag(&(*s).select, markdownDocIndexAt(s, localX, localY)))
            refreshRowSelection(s);
        return;
    }
    if (kind == PTR_UP) {
        int32_t lo = -1, hi = -1;
        if (TextSelect_isActive(&(*s).select)) {
            TextSelect_drag(&(*s).select, markdownDocIndexAt(s, localX, localY));
            TextSelect_end(&(*s).select, &lo, &hi);
        }
        refreshRowSelection(s);
        return;
    }
}

// Zero-alloc line classifier: index arithmetic over (line, len) only.
static LineKind classifyLine(const char *line, size_t len, size_t *contentStart, size_t *contentLen, float *size) {
    size_t i = 0;
    while (i < len && (line[i] == ' ' || line[i] == '\t'))
        i++;
    if (i >= len) {
        (*contentStart) = len;
        (*contentLen) = 0;
        (*size) = MARKDOWN_BASE_SIZE;
        return LINE_BLANK;
    }
    if (len - i >= 3 && line[i] == '`' && line[i + 1] == '`' && line[i + 2] == '`') {
        (*contentStart) = len;
        (*contentLen) = 0;
        (*size) = MARKDOWN_CODE_SIZE;
        return LINE_FENCE;
    }
    if (line[i] == '#') {
        size_t hashes = 0;
        while (i + hashes < len && line[i + hashes] == '#' && hashes < 4)
            hashes++;
        size_t after = i + hashes;
        if (after >= len || line[after] == ' ' || line[after] == '\t') {
            size_t start = after;
            while (start < len && (line[start] == ' ' || line[start] == '\t'))
                start++;
            (*contentStart) = start;
            (*contentLen) = len - start;
            if (hashes == 1)
                (*size) = MARKDOWN_H1_SIZE;
            else if (hashes == 2)
                (*size) = MARKDOWN_H2_SIZE;
            else if (hashes == 3)
                (*size) = MARKDOWN_H3_SIZE;
            else
                (*size) = MARKDOWN_H4_SIZE;
            return LINE_HEADING;
        }
    }
    if ((line[i] == '-' || line[i] == '*') && i + 1 < len && line[i + 1] == ' ') {
        size_t start = i + 2;
        while (start < len && (line[start] == ' ' || line[start] == '\t'))
            start++;
        (*contentStart) = start;
        (*contentLen) = len - start;
        (*size) = MARKDOWN_BASE_SIZE;
        return LINE_BULLET;
    }
    (*contentStart) = i;
    (*contentLen) = len - i;
    (*size) = MARKDOWN_BASE_SIZE;
    return LINE_PARA;
}

// Zero-alloc inline-markup probe: true when a converter pass is worthwhile.
static bool hasInline(const char *line, size_t len) {
    for (size_t i = 0; i < len; i++) {
        char ch = line[i];
        if (ch == '`')
            return true;
        if (ch == '*' && i + 1 < len && line[i + 1] == '*')
            return true;
        if (ch == '*' && i > 0 && i + 1 < len)
            return true;
    }
    return false;
}

// Emits the combined style tag for the active set: [00] reset + [01|02|03].
static void emitActive(char *dest, size_t *at, bool bold, bool italic, bool code) {
    const char *reset = "[00]";
    size_t k = 0;
    while (reset[k] != '\0') {
        dest[(*at)] = reset[k];
        (*at)++;
        k++;
    }
    if (!bold && !italic && !code)
        return;
    dest[(*at)] = '[';
    (*at)++;
    bool first = true;
    if (bold) {
        dest[(*at)] = '0';
        (*at)++;
        dest[(*at)] = '1';
        (*at)++;
        first = false;
    }
    if (italic) {
        if (!first) {
            dest[(*at)] = '|';
            (*at)++;
        }
        dest[(*at)] = '0';
        (*at)++;
        dest[(*at)] = '2';
        (*at)++;
        first = false;
    }
    if (code) {
        if (!first) {
            dest[(*at)] = '|';
            (*at)++;
        }
        dest[(*at)] = '0';
        (*at)++;
        dest[(*at)] = '3';
        (*at)++;
    }
    dest[(*at)] = ']';
    (*at)++;
}

// Measures the tagged conversion length (counting pass, no output).
static size_t measureTagged(const char *line, size_t len) {
    size_t need = len + 1;
    for (size_t i = 0; i < len; i++) {
        char ch = line[i];
        if (ch == '[')
            need += 1;
        if (ch == '*' && i + 1 < len && line[i + 1] == '*') {
            need += 4;
            i++;
        } else if (ch == '*' || ch == '`') {
            need += 4;
        }
    }
    need += 16;
    return need;
}

// Fills dest with the RichText-tagged conversion; dest has measureTagged bytes.
static void fillTagged(const char *line, size_t len, char *dest) {
    size_t at = 0;
    bool bold = false;
    bool italic = false;
    bool code = false;
    for (size_t i = 0; i < len;) {
        char ch = line[i];
        if (ch == '*' && i + 1 < len && line[i + 1] == '*') {
            bold = !bold;
            emitActive(dest, &at, bold, italic, code);
            i += 2;
        } else if (ch == '*') {
            italic = !italic;
            emitActive(dest, &at, bold, italic, code);
            i++;
        } else if (ch == '`') {
            code = !code;
            emitActive(dest, &at, bold, italic, code);
            i++;
        } else {
            if (ch == '[') {
                dest[at] = '\\';
                at++;
            }
            dest[at] = ch;
            at++;
            i++;
        }
    }
    if (bold || italic || code)
        emitActive(dest, &at, false, false, false);
    dest[at] = '\0';
}

// Copies a line slice, stripping markdown markers for the plain Label path.
static void fillStripped(const char *line, size_t len, char *dest) {
    size_t at = 0;
    for (size_t i = 0; i < len;) {
        char ch = line[i];
        if (ch == '*' && i + 1 < len && line[i + 1] == '*') {
            i += 2;
        } else if (ch == '*' || ch == '`') {
            i++;
        } else {
            dest[at] = ch;
            at++;
            i++;
        }
    }
    dest[at] = '\0';
}

static float stackRow(MarkdownPanel *s, Panel *row, float cursor, float height) {
    // Y positions belong to the ListContainer now; rows only take their size.
    Panel *b = &(*s).base;
    Container *c = &(*b).base;
    float w = Container_getWidth(c);
    if (w < 0.0f)
        w = 0.0f;
    Panel_setSize(row, w, height);
    return cursor + height + (*s).rowSpacing;
}

static void addLabelRow(MarkdownPanel *s, const char *line, size_t len, bool bullet, float size, uint32_t color, uint32_t bg, float *cursor, size_t *cellBase) {
    ListContainer *box = (*s).rows;
    if (!box || !cursor || !cellBase)
        return;
    size_t extra = bullet ? 4 : 0;
    char *tmp = (char*) Memory_alloc(TYPE_ARRAY, len + extra + 1);
    if (!tmp)
        return;
    size_t at = 0;
    if (bullet) {
        tmp[0] = 0xE2;
        tmp[1] = 0x80;
        tmp[2] = 0xA2;
        tmp[3] = ' ';
        at = 4;
    }
    for (size_t i = 0; i < len; i++) {
        tmp[at] = line[i];
        at++;
    }
    tmp[at] = '\0';
    Label *lbl = Label_1((const char*) tmp);
    Memory_free(tmp);
    if (!lbl)
        return;
    Label_setFontSize(lbl, size);
    Label_setTextColor(lbl, color);
    Label_setTextAlign(lbl, (*s).textAlign);
    Label_setSpacingWidth(lbl, (*s).spacingWidth);
    Label_setSpacingHeight(lbl, (*s).spacingHeight);
    Label_setLigatures(lbl, (*s).ligatures);
    if (bg != 0u)
        Label_setBackgroundColor(lbl, bg);
    Panel *row = &(*lbl).base;
    ListContainer_add(box, row);
    float height = size * MARKDOWN_LINE_FACTOR;
    if (!pushSlot(s, row, nullptr, 0, height, (uint32_t) (*cellBase), (uint32_t) at)) {
        size_t n = ListContainer_count(box);
        if (n > 0)
            ListContainer_remove(box, (int32_t)(n - 1));
        Label_free(lbl);
        return;
    }
    (*cellBase) += at;
    (*cursor) = stackRow(s, row, (*cursor), height);
}

static void addRichRow(MarkdownPanel *s, const char *line, size_t len, bool bullet, float *cursor, size_t *cellBase) {
    ListContainer *box = (*s).rows;
    Font *font = (*s).font;
    if (!box || !font || !cursor || !cellBase)
        return;
    size_t rawLen = len + (bullet ? 2 : 0);
    char *raw = (char*) Memory_alloc(TYPE_ARRAY, rawLen + 1);
    if (!raw)
        return;
    size_t at = 0;
    if (bullet) {
        raw[0] = 0xE2;
        raw[1] = 0x80;
        raw[2] = 0xA2;
        raw[3] = ' ';
        at = 4;
    }
    for (size_t i = 0; i < len; i++) {
        raw[at] = line[i];
        at++;
    }
    raw[at] = '\0';
    size_t need = measureTagged(raw, at);
    char *tagged = (char*) Memory_alloc(TYPE_ARRAY, need);
    if (!tagged) {
        Memory_free(raw);
        return;
    }
    fillTagged(raw, at, tagged);
    Memory_free(raw);
    RichText *rt = RichText_new();
    if (!rt) {
        Memory_free(tagged);
        return;
    }
    RichText_setStyle(rt, 0, font, MARKDOWN_BASE_SIZE, MARKDOWN_TEXT_COLOR, false, DECOR_NONE);
    RichText_setStyle(rt, 1, font, MARKDOWN_BASE_SIZE, MARKDOWN_TEXT_COLOR, true, DECOR_NONE);
    RichText_setStyle(rt, 2, font, MARKDOWN_BASE_SIZE, MARKDOWN_TEXT_COLOR, false, DECOR_LINE);
    RichText_setStyle(rt, 3, font, MARKDOWN_CODE_SIZE, MARKDOWN_TEXT_COLOR, false, DECOR_NONE);
    RichText_setString(rt, tagged);
    size_t tagLen = tagged ? (uint32_t) strlen(tagged) : 0;
    Memory_free(tagged);
    RichText_layout(rt, 0.0f);
    RichLabel *rl = RichLabel_0();
    if (!rl) {
        RichText_free(rt);
        return;
    }
    RichLabel_setTextModel(rl, rt);
    RichLabel_setTextAlign(rl, (*s).textAlign);
    RichLabel_setSpacingWidth(rl, (*s).spacingWidth);
    RichLabel_setSpacingHeight(rl, (*s).spacingHeight);
    RichLabel_setLigatures(rl, (*s).ligatures);
    Panel *row = &(*rl).base;
    ListContainer_add(box, row);
    float height = (*rt).layoutHeight;
    if (height <= 0.0f)
        height = MARKDOWN_BASE_SIZE * MARKDOWN_LINE_FACTOR;
    if (!pushSlot(s, row, rt, 1, height, (uint32_t) (*cellBase), (uint32_t) tagLen)) {
        size_t n = ListContainer_count(box);
        if (n > 0)
            ListContainer_remove(box, (int32_t)(n - 1));
        RichText_free(rt);
        Memory_free(rl);
        return;
    }
    (*cellBase) += tagLen;
    (*cursor) = stackRow(s, row, (*cursor), height);
}

static void rebuild(MarkdownPanel *s) {
    if (!s)
        return;
    clearRows(s);
    ListContainer *box = (*s).rows;
    if (!box)
        return;
    uint8_t *block = (*s).textBlock;
    const char *src = block ? string_get(block) : nullptr;
    size_t total = block ? string_length(block) : 0;
    if (!src || total == 0) {
        Panel_setSize(&(*box).base, 0.0f, 0.0f);
        markDirty(s);
        return;
    }
    float cursor = 0.0f;
    size_t cellBase = 0;
    bool inFence = false;
    size_t start = 0;
    for (size_t i = 0; i <= total; i++) {
        bool edge = i == total || src[i] == '\n';
        if (!edge)
            continue;
        size_t len = i - start;
        const char *line = &src[start];
        start = i + 1;
        size_t contentStart = 0;
        size_t contentLen = 0;
        float size = MARKDOWN_BASE_SIZE;
        LineKind kind = classifyLine(line, len, &contentStart, &contentLen, &size);
        if (kind == LINE_FENCE) {
            inFence = !inFence;
            continue;
        }
        if (kind == LINE_BLANK)
            continue;
        if (inFence) {
            addLabelRow(s, line, len, false, MARKDOWN_CODE_SIZE, MARKDOWN_TEXT_COLOR, (*s).codeBackground, &cursor, &cellBase);
            continue;
        }
        const char *content = &line[contentStart];
        if (kind == LINE_HEADING) {
            addLabelRow(s, content, contentLen, false, size, MARKDOWN_TEXT_COLOR, 0u, &cursor, &cellBase);
        } else if (kind == LINE_BULLET) {
            Font *font = (*s).font;
            if (font && hasInline(content, contentLen))
                addRichRow(s, content, contentLen, true, &cursor, &cellBase);
            else {
                size_t stripped = contentLen + 1;
                char *tmp = (char*) Memory_alloc(TYPE_ARRAY, stripped);
                if (tmp) {
                    fillStripped(content, contentLen, tmp);
                    addLabelRow(s, tmp, strlen(tmp), true, size, MARKDOWN_TEXT_COLOR, 0u, &cursor, &cellBase);
                    Memory_free(tmp);
                }
            }
        } else {
            Font *font = (*s).font;
            if (font && hasInline(content, contentLen)) {
                addRichRow(s, content, contentLen, false, &cursor, &cellBase);
            } else {
                size_t stripped = contentLen + 1;
                char *tmp = (char*) Memory_alloc(TYPE_ARRAY, stripped);
                if (tmp) {
                    fillStripped(content, contentLen, tmp);
                    addLabelRow(s, tmp, strlen(tmp), false, size, MARKDOWN_TEXT_COLOR, 0u, &cursor, &cellBase);
                    Memory_free(tmp);
                }
            }
        }
    }
    if (cursor > 0.0f)
        cursor -= (*s).rowSpacing;
    Panel *b = &(*s).base;
    Container *c = &(*b).base;
    float w = Container_getWidth(c);
    if (w < 0.0f)
        w = 0.0f;
    Panel_setSize(b, w, cursor);
    Panel_setSize(&(*box).base, w, cursor);
    // Authoritative pass: rows were sized after their per-add layouts ran,
    // so re-stack once with final heights (cold path, documents only).
    ListContainer_layout(box);
    markDirty(s);
}

void MarkdownPanel_free(MarkdownPanel *s) {
    if (!s)
        return;
    clearRows(s);
    if ((*s).textBlock) {
        string_free((*s).textBlock);
        (*s).textBlock = nullptr;
    }
    if ((*s).rows) {
        Memory_free((*s).rows);
        (*s).rows = nullptr;
    }
    (*s).font = nullptr;
    Memory_free(s);
}

// SETTERS
// ============================================================================

void MarkdownPanel_setText(MarkdownPanel *s, const char *text) {
    if (!s)
        return;
    if ((*s).textBlock) {
        string_free((*s).textBlock);
        (*s).textBlock = nullptr;
    }
    if (text)
        (*s).textBlock = string_allocate(text);
    rebuild(s);
}

void MarkdownPanel_setFont(MarkdownPanel *s, Font *font) {
    if (!s)
        return;
    (*s).font = font;
    rebuild(s);
}

void MarkdownPanel_setCodeBackground(MarkdownPanel *s, uint32_t color) {
    if (!s)
        return;
    (*s).codeBackground = color;
    rebuild(s);
}

void MarkdownPanel_setRowSpacing(MarkdownPanel *s, float spacing) {
    if (!s)
        return;
    if (spacing < 0.0f)
        spacing = 0.0f;
    (*s).rowSpacing = spacing;
    if ((*s).rows)
        ListContainer_setSpacing((*s).rows, spacing);
    rebuild(s);
}

void MarkdownPanel_setLocation(MarkdownPanel *s, float x, float y) {
    if (!s)
        return;
    Panel *b = &(*s).base;
    Panel_setLocation(b, x, y);
}

void MarkdownPanel_setSize(MarkdownPanel *s, float w, float h) {
    if (!s)
        return;
    Panel *b = &(*s).base;
    Panel_setSize(b, w, h);
}

void MarkdownPanel_setBackgroundColor(MarkdownPanel *s, uint32_t color) {
    if (!s)
        return;
    Panel *b = &(*s).base;
    Panel_setBackgroundColor(b, color);
}

void MarkdownPanel_setHighlightable(MarkdownPanel *s, bool flag) {
    if (!s)
        return;
    (*s).highlightable = flag;
    if (!flag)
        TextSelect_reset(&(*s).select);
    struct MarkdownRowSlot *slots = (*s).slots;
    size_t n = (*s).rowCount;
    for (size_t i = 0; i < n; i++) {
        struct MarkdownRowSlot *slot = &slots[i];
        if ((*slot).isRich)
            RichLabel_setHighlightable((RichLabel*) (*slot).panel, flag);
        else
            Label_setHighlightable((Label*) (*slot).panel, flag);
    }
    refreshRowSelection(s);
}

void MarkdownPanel_setSelection(MarkdownPanel *s, int32_t start, int32_t end) {
    if (!s)
        return;
    if (start < 0 || end < 0) {
        TextSelect_reset(&(*s).select);
    } else {
        TextSelect_begin(&(*s).select, start);
        TextSelect_drag(&(*s).select, end);
    }
    refreshRowSelection(s);
}

void MarkdownPanel_setHighlightColor(MarkdownPanel *s, uint32_t color) {
    if (!s)
        return;
    (*s).highlightColor = color;
    refreshRowSelection(s);
}

void MarkdownPanel_setHighlightColorRGBA(MarkdownPanel *s, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    uint32_t packed = ((uint32_t) a << 24) | ((uint32_t) r << 16) | ((uint32_t) g << 8) | (uint32_t) b;
    MarkdownPanel_setHighlightColor(s, packed);
}

void MarkdownPanel_setTextAlign(MarkdownPanel *s, TextAlign align) {
    if (!s || (*s).textAlign == align)
        return;
    (*s).textAlign = align;
    rebuild(s);
}

void MarkdownPanel_setSpacingWidth(MarkdownPanel *s, float width) {
    if (!s)
        return;
    (*s).spacingWidth = width;
    rebuild(s);
}

void MarkdownPanel_setSpacingHeight(MarkdownPanel *s, float height) {
    if (!s)
        return;
    (*s).spacingHeight = height;
    rebuild(s);
}

void MarkdownPanel_setLigatures(MarkdownPanel *s, bool flag) {
    if (!s || (*s).ligatures == flag)
        return;
    (*s).ligatures = flag;
    rebuild(s);
}

// GETTERS
// ============================================================================

const char *MarkdownPanel_getText(const MarkdownPanel *s) {
    if (!s || !(*s).textBlock)
        return nullptr;
    uint8_t *block = (*s).textBlock;
    return string_get(block);
}

Font *MarkdownPanel_getFont(const MarkdownPanel *s) {
    return s ? (*s).font : nullptr;
}

uint32_t MarkdownPanel_getCodeBackground(const MarkdownPanel *s) {
    return s ? (*s).codeBackground : 0u;
}

Panel *MarkdownPanel_getRows(const MarkdownPanel *s) {
    if (!s || !(*s).rows)
        return nullptr;
    ListContainer *box = (*s).rows;
    return &(*box).base;
}

float MarkdownPanel_getRowSpacing(const MarkdownPanel *s) {
    return s ? (*s).rowSpacing : 0.0f;
}

size_t MarkdownPanel_getRowCount(const MarkdownPanel *s) {
    return s ? (*s).rowCount : 0;
}

Panel *MarkdownPanel_getRow(const MarkdownPanel *s, size_t index) {
    if (!s)
        return nullptr;
    if (index >= (*s).rowCount)
        return nullptr;
    struct MarkdownRowSlot *slots = (*s).slots;
    struct MarkdownRowSlot *slot = &slots[index];
    return (*slot).panel;
}

bool MarkdownPanel_isHighlightable(const MarkdownPanel *s) {
    return s ? (*s).highlightable : false;
}

void MarkdownPanel_getSelection(const MarkdownPanel *s, int32_t *outStart, int32_t *outEnd) {
    int32_t s0 = -1, s1 = -1;
    if (s)
        (void) TextSelect_getSpan(&(*s).select, &s0, &s1);
    if (outStart) (*outStart) = s0;
    if (outEnd) (*outEnd) = s1;
}

uint32_t MarkdownPanel_getHighlightColor(const MarkdownPanel *s) {
    return s ? (*s).highlightColor : 0u;
}

void MarkdownPanel_getHighlightColorRGBA(const MarkdownPanel *s, uint8_t *outR, uint8_t *outG, uint8_t *outB, uint8_t *outA) {
    uint32_t c = s ? (*s).highlightColor : 0u;
    if (outA)
        (*outA) = (uint8_t) ((c >> 24) & 0xFF);
    if (outR)
        (*outR) = (uint8_t) ((c >> 16) & 0xFF);
    if (outG)
        (*outG) = (uint8_t) ((c >> 8) & 0xFF);
    if (outB)
        (*outB) = (uint8_t) (c & 0xFF);
}

TextAlign MarkdownPanel_getTextAlign(const MarkdownPanel *s) {
    return s ? (*s).textAlign : TEXT_ALIGN_LEFT;
}

float MarkdownPanel_getSpacingWidth(const MarkdownPanel *s) {
    return s ? (*s).spacingWidth : 0.0f;
}

float MarkdownPanel_getSpacingHeight(const MarkdownPanel *s) {
    return s ? (*s).spacingHeight : 0.0f;
}

bool MarkdownPanel_hasLigatures(const MarkdownPanel *s) {
    return s ? (*s).ligatures : true;
}

// ----------------------------------------------------------------------------
// CORE FUNCTIONS (selection copy + key seam)
// ----------------------------------------------------------------------------

// End of a glyph's display bytes inside the tagged string: the window up to
// the next glyph whose source bytes are NOT a style tag. Tags start with '[';
// an escaped \[ emits a literal bracket. Mirrors rich_label's span walk.
static int32_t glyphSpanEnd(const char *s, int32_t start, int32_t next) {
    int32_t at = start;
    while (at < next && s[at] != '\0') {
        unsigned char b = (unsigned char) s[at];
        if (b == '[')
            return at;
        if (b == '\\' && at + 1 < next && s[at + 1] == '[') {
            at += 2;
            continue;
        }
        int32_t need = 1;
        if ((b & 0xE0) == 0xC0)
            need = 2;
        else if ((b & 0xF0) == 0xE0)
            need = 3;
        else if ((b & 0xF8) == 0xF0)
            need = 4;
        if (at + need > next)
            return next;
        at += need;
    }
    return at;
}

// Stripped plain byte count of the tagged range [a, b) over a RichText model.
static size_t countRichRange(const RichText *tm, int32_t a, int32_t b) {
    if (!tm || !(*tm).rawString || !(*tm).quads || (*tm).quadCount == 0)
        return 0;
    const char *s = (*tm).rawString;
    int32_t rawLen = (int32_t) strlen(s);
    size_t cap = 0;
    for (size_t i = 0; i < (*tm).quadCount; i++) {
        const TextQuad *q = &(*tm).quads[i];
        if ((*q).charIndex < 0 || (*q).advance <= 0)
            continue;
        int32_t c = (*q).charIndex;
        if (c < a || c >= b)
            continue;
        int32_t nextStart = rawLen;
        for (size_t j = i + 1; j < (*tm).quadCount; j++) {
            const TextQuad *nq = &(*tm).quads[j];
            if ((*nq).charIndex < 0 || (*nq).advance <= 0)
                continue;
            nextStart = (*nq).charIndex;
            break;
        }
        cap += (size_t) (glyphSpanEnd(s, c, nextStart) - c);
    }
    return cap;
}

// Fills out with the stripped plain bytes of [a, b); returns bytes written.
static size_t fillRichRange(const RichText *tm, int32_t a, int32_t b, char *out) {
    size_t at = 0;
    if (!tm || !(*tm).rawString || !(*tm).quads || (*tm).quadCount == 0)
        return 0;
    const char *s = (*tm).rawString;
    int32_t rawLen = (int32_t) strlen(s);
    for (size_t i = 0; i < (*tm).quadCount; i++) {
        const TextQuad *q = &(*tm).quads[i];
        if ((*q).charIndex < 0 || (*q).advance <= 0)
            continue;
        int32_t c = (*q).charIndex;
        if (c < a || c >= b)
            continue;
        int32_t nextStart = rawLen;
        for (size_t j = i + 1; j < (*tm).quadCount; j++) {
            const TextQuad *nq = &(*tm).quads[j];
            if ((*nq).charIndex < 0 || (*nq).advance <= 0)
                continue;
            nextStart = (*nq).charIndex;
            break;
        }
        int32_t end = glyphSpanEnd(s, c, nextStart);
        for (int32_t k = c; k < end; k++)
            out[at++] = s[k];
    }
    return at;
}

// Clamps a doc-space [lo, hi) onto one row and returns the row-local byte
// range via outA/outB; returns the byte count (0 when the row is untouched).
static int32_t clampRowRange(const MarkdownPanel *s, size_t rowIndex, int32_t lo, int32_t hi, int32_t *outA, int32_t *outB) {
    struct MarkdownRowSlot *slots = (*s).slots;
    struct MarkdownRowSlot *slot = &slots[rowIndex];
    int32_t cs = (int32_t) (*slot).cellStart;
    int32_t tl = (int32_t) (*slot).textLen;
    int32_t a = lo < cs ? 0 : lo - cs;
    int32_t b = hi - cs;
    if (b > tl)
        b = tl;
    if (a < 0)
        a = 0;
    if (a >= b)
        return 0;
    (*outA) = a;
    (*outB) = b;
    return b - a;
}

// Display-accurate plain copy of the committed selection (arena-allocated,
// caller Memory_free). Label rows slice their stored display string — markers
// were already stripped at row build and the "• " bullet literal lives at
// [0,4), so it is included exactly when the selection touches it. Rich rows
// strip [nn] style tags (and unescape \[) from the tagged model string.
char *MarkdownPanel_getSelectedText(const MarkdownPanel *s) {
    if (!s)
        return nullptr;
    int32_t lo = -1, hi = -1;
    if (!TextSelect_getSpan(&(*s).select, &lo, &hi))
        return nullptr;
    if (hi <= lo)
        return nullptr;
    size_t n = (*s).rowCount;
    if (n == 0)
        return nullptr;

    size_t cap = 0;
    size_t rowSelectedCount = 0;
    for (size_t i = 0; i < n; i++) {
        int32_t a = 0, b = 0;
        int32_t len = clampRowRange(s, i, lo, hi, &a, &b);
        if (len <= 0)
            continue;
        if (rowSelectedCount > 0)
            cap += 1;
        rowSelectedCount++;
        struct MarkdownRowSlot *slots = (*s).slots;
        struct MarkdownRowSlot *slot = &slots[i];
        if ((*slot).isRich)
            cap += countRichRange((*slot).model, a, b);
        else {
            const char *txt = Label_getText((const Label*) (*slot).panel);
            cap += txt ? (size_t) len : 0;
        }
    }
    if (cap == 0)
        return nullptr;

    char *out = (char*) Memory_alloc(TYPE_ARRAY, cap + 1);
    if (!out)
        return nullptr;
    size_t at = 0;
    bool hadPrevRow = false;
    for (size_t i = 0; i < n; i++) {
        int32_t a = 0, b = 0;
        int32_t len = clampRowRange(s, i, lo, hi, &a, &b);
        if (len <= 0)
            continue;
        if (hadPrevRow)
            out[at++] = '\n';
        hadPrevRow = true;
        struct MarkdownRowSlot *slots = (*s).slots;
        struct MarkdownRowSlot *slot = &slots[i];
        if ((*slot).isRich) {
            at += fillRichRange((*slot).model, a, b, out + at);
        } else {
            const char *txt = Label_getText((const Label*) (*slot).panel);
            if (!txt)
                continue;
            int32_t txtLen = (int32_t) strlen(txt);
            int32_t copyFrom = a < txtLen ? a : txtLen;
            int32_t copyTo = b < txtLen ? b : txtLen;
            if (copyFrom < copyTo) {
                memcpy(out + at, txt + copyFrom, (size_t)(copyTo - copyFrom));
                at += (size_t)(copyTo - copyFrom);
            }
        }
    }
    out[at] = '\0';
    return out;
}

// Key seam: Cmd/Ctrl+C copies the committed selection; Cmd/Ctrl+V replaces the
// whole document text with the clipboard (cold rebuild). Press-only, repeats
// ignored; detection by keyCode + modifier bits (cmd=8, ctrl=2), never by the
// decoded character.
void MarkdownPanel_handleKey(MarkdownPanel *s, const UIKeyEvent *ev) {
    if (!s || !ev)
        return;
    if (!UIKeyEvent_isPressed(ev))
        return;
    if (UIKeyEvent_isRepeat(ev))
        return;
    uint32_t mods = UIKeyEvent_getMods(ev);
    if ((mods & (8u | 2u)) == 0)
        return;
    if (!(*s).highlightable)
        return;
    int32_t code = UIKeyEvent_getKeyCode(ev);
    if (code == KEY_C) {
        char *sel = MarkdownPanel_getSelectedText(s);
        if (sel) {
            TextCore_copyToClipboard(sel);
            Memory_free(sel);
            UIKeyEvent_consume((UIKeyEvent*) ev);
        }
        return;
    }
    if (code == KEY_V) {
        char *clip = TextCore_pasteFromClipboard();
        if (clip) {
            MarkdownPanel_setText(s, clip);
            free(clip);
            UIKeyEvent_consume((UIKeyEvent*) ev);
        }
    }
}
