#include "rich_label.h"
#include "vulkan/vk.h"
#include "nio/mem.h"
#include "oop/type.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include "input/key.h"
#include <string.h>

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: RichLabel
 * ============================================================================
 * Retained-mode rich-text view: a Panel hosting an owned RichText layout model
 * with an inherited wrap mode, rendered through the SDF atlas path. Selection
 * uses a fixed drag anchor (browser-like): pointer-down pins the anchor, drag
 * moves only the active edge, so dragging left then right past the anchor
 * selects exactly [anchor, active] — never a rolling union; the shared
 * TextSelect part commits a nonzero range on pointer-up and persists the
 * highlight and getSelectedText. RichLabel never shows a caret — labels are
 * non-editable surfaces (carets belong to Input). The text model is owned by
 * the label; glyph quads carry charIndex/advance so pointer hits map to
 * source byte indices.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: RichLabel (inherits Panel)
 * LEVEL: L2 — Behavior (rich text UI view behavior API)
 * ============================================================================
 * Retained-mode rich-text view: a Panel hosting a RichText layout model with
 * an inherited wrap mode, rendered through the SDF atlas path. Supports
 * selectable text with a fixed drag anchor (browser-like): pointer-down is
 * the anchor, drag moves only the active edge, so dragging left (backward)
 * then right past the anchor selects exactly [anchor, active] — never a
 * rolling union. Selection maps pointer (localX, localY) to a source byte
 * index via each glyph quad's charIndex/advance; highlights are drawn as
 * flat per-line spans behind the glyph quads. Selection state lives in the
 * shared TextSelect part (COMMITS a nonzero range on UP; collapsed clicks
 * clear), so the highlight and getSelectedText persist after pointer-up.
 * RichLabel never shows a caret — labels are non-editable surfaces (carets
 * belong to Input).
 *
 * STRUCT FIELDS (Mirroring darling/label/rich_label.h):
 * ----------------------------------------------------------------------------
 *   Panel base;              // Inherited layout, bounds, and hierarchy state
 *   RichText *textModel;     // Owned styled-text layout model (see rich_text.h)
 *   WrapMode wrapMode;       // Line-wrap policy inherited by the layout engine
 *   Cursor *cursor;          // Active mouse cursor style (I-beam when highlightable)
 *   bool highlightable;      // Enables text selection drag (no caret)
 *   TextSelect select;       // Shared selection part (anchor/active edge + hover)
 *   uint32_t highlightColor; // Packed 0xAARRGGBB selection fill
 *
 * PRIVATE HELPERS (kept file-local pure-data only):
 * ----------------------------------------------------------------------------
 *   Selection span accumulator (scalar locals only — no struct):
 *     spanX0, spanX1, spanY0, spanY1 — running per-line highlight rect state
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - RichLabel_0(void)
 *   - RichLabel_1(parent)
 *
 * Core Functions:
 *   - richPaintText(panel, rend, cmd, ...) : Stage 2 spans + glyph quads
 *     (via Panel_setTextFn; background stays Panel default)
 *   - RichLabel_charIndexAt(label, localX, localY)                        : Byte index from point
 *   - RichLabel_handlePointer(label, kind, localX, localY, window)        : Pointer event dispatcher
 *   - RichLabel_onPointer(label, ev, window)                              : PointerEvent wrapper
 *   - RichLabel_handleKey(label, ev)                                      : Key event (Cmd+C copy)
 *   - RichLabel_getSelectedText(label)                                    : Tag-stripped plain copy
 *
 * Setters:
 *   - RichLabel_setTextModel(label, model)
 *   - RichLabel_setWrapMode(label, mode)
 *   - RichLabel_setHighlightable(label, flag)
 *   - RichLabel_setSelection(label, start, end)
 *   - RichLabel_setHighlightColor(label, color)
 *   - RichLabel_setHighlightColorRGBA(label, r, g, b, a)
 *   - RichLabel_setHovered(label, hovered)
 *   - RichLabel_setCursor(label, cursor)
 *   - RichLabel_free(label)
 *
 * Getters:
 *   - RichLabel_isHighlightable(const label)
 *   - RichLabel_getSelection(const label, outStart, outEnd)
 *   - RichLabel_getHighlightColor(const label)
 *   - RichLabel_getHighlightColorRGBA(const label, outR, outG, outB, outA)
 *   - RichLabel_isHovered(const label)
 *   - RichLabel_getCursor(const label)
 * ============================================================================
 */

// ============================================================================
// CORE FUNCTIONS
// ============================================================================

static int32_t textLength(const RichLabel *rl) {
    if (!rl || !(*rl).textModel)
        return 0;
    const RichText *tm = (*rl).textModel;
    if ((*tm).rawString)
        return (int32_t) strlen((*tm).rawString);
    return 0;
}

// Maps (localX, localY) to a source byte index by nearest glyph origin.
// Glyph quad displacements are laid out in string order, so each main quad
// (charIndex >= 0, advance > 0) is a candidate anchor. Vertical distance is
// weighted so the pointer stays on its own line across mixed-size runs.
int32_t RichLabel_charIndexAt(const RichLabel *label, float localX, float localY) {
    if (!label || !(*label).textModel)
        return 0;
    const RichText *tm = (*label).textModel;
    if (!(*tm).quads || (*tm).quadCount == 0)
        return 0;
    int32_t bestIdx = 0;
    float bestD = -1.0f;
    bool haveGlyph = false;
    float firstOrigin = 0.0f;
    float lastRight = 0.0f;
    for (size_t i = 0; i < (*tm).quadCount; i++) {
        const TextQuad *q = &(*tm).quads[i];
        if ((*q).charIndex < 0 || (*q).advance <= 0)
            continue;
        float right = (*q).x + (*q).advance;
        if (!haveGlyph || (*q).x < firstOrigin)
            firstOrigin = (*q).x;
        if (!haveGlyph || right > lastRight)
            lastRight = right;
        float dx = localX - (*q).x;
        float dy = (localY - (*q).y) * 1.6f;
        float d = dx * dx + dy * dy;
        if (!haveGlyph || d < bestD) {
            bestD = d;
            bestIdx = (*q).charIndex;
            haveGlyph = true;
        }
    }
    if (!haveGlyph)
        return 0;
    int32_t len = textLength(label);
    if (localX < firstOrigin)
        return 0;
    if (localX >= lastRight)
        return len;
    if (bestIdx < 0)
        bestIdx = 0;
    if (bestIdx > len)
        bestIdx = len;
    return bestIdx;
}

void RichLabel_handlePointer(RichLabel *label, int kind, float localX, float localY, void *window) {
    if (!label)
        return;
    Panel *p = &(*label).base;
    Component *c = &(*p).component;
    float w = (*c).w;
    float h = (*c).h;
    const RichText *tm = (*label).textModel;
    if (w <= 0.0f && tm)
        w = (*tm).layoutWidth;
    if (h <= 0.0f && tm)
        h = (*tm).layoutHeight;

    bool inside = (localX >= 0.0f && localX <= w && localY >= 0.0f && localY <= h);

    if (kind == PTR_LEAVE || (!inside && (kind == PTR_MOVE || kind == PTR_HOVER))) {
        if (TextSelect_setHovered(&(*label).select, false)) {
            if (window) {
                Cursor *defCursor = Cursor_getPredefined(CURSOR_DEFAULT);
                Cursor_apply(defCursor, window);
            }
        }
        return;
    }

    if (inside && (kind == PTR_ENTER || kind == PTR_MOVE || kind == PTR_HOVER)) {
        if (TextSelect_setHovered(&(*label).select, true)) {
            if ((*label).highlightable && window)
                Cursor_apply((*label).cursor, window);
        } else if ((*label).highlightable && window) {
            Cursor_apply((*label).cursor, window);
        }
    }

    if ((*label).highlightable) {
        if (kind == PTR_DOWN) {
            // Outside-down clears any in-progress selection.
            if (!inside) {
                if (TextSelect_isActive(&(*label).select)) {
                    TextSelect_cancel(&(*label).select);
                }
                return;
            }
            // Down = fixed anchor + collapsed selection. Drag then moves only the
            // active edge, so dragging left (backward) then right past the anchor
            // selects exactly [anchor, active] — never a rolling union.
            int32_t idx = RichLabel_charIndexAt(label, localX, localY);
            TextSelect_begin(&(*label).select, idx);
        } else if (kind == PTR_DRAG) {
            if (TextSelect_drag(&(*label).select, RichLabel_charIndexAt(label, localX, localY))) {
            }
        } else if (kind == PTR_UP) {
            int32_t lo = -1, hi = -1;
            TextSelect_end(&(*label).select, &lo, &hi);
        }
    }
}

void RichLabel_onPointer(RichLabel *label, PointerEvent *ev, void *window) {
    if (!label || !ev)
        return;
    int kind = PointerEvent_getKind(ev);
    float x = PointerEvent_getX(ev);
    float y = PointerEvent_getY(ev);
    RichLabel_handlePointer(label, kind, x, y, window);
}

// Key seam: Cmd/Ctrl+C copies the committed selection (tag-stripped); V is a
// no-op on read-only labels. Detection by keyCode + modifier bits (cmd=8,
// ctrl=2 local bridge bits), never by the decoded character.
void RichLabel_handleKey(RichLabel *label, const UIKeyEvent *ev) {
    if (!label || !ev)
        return;
    if (!UIKeyEvent_isPressed(ev))
        return;
    if (UIKeyEvent_isRepeat(ev))
        return;
    uint32_t mods = UIKeyEvent_getMods(ev);
    if ((mods & (8u | 2u)) == 0)
        return;
    if (!(*label).highlightable)
        return;
    int32_t code = UIKeyEvent_getKeyCode(ev);
    if (code != KEY_C && code != KEY_V)
        return;
    if (code == KEY_C) {
        char *sel = RichLabel_getSelectedText(label);
        if (sel) {
            TextCore_copyToClipboard(sel);
            Memory_free(sel);
            UIKeyEvent_consume((UIKeyEvent*) ev);
        }
    }
}

// Draws per-line highlight spans behind the glyph quads whose charIndex falls
// inside [selStart, selEnd). Lines are clustered by vertical tolerance: cursorY
// grows monotonically across wraps, while runs on the same line differ only by
// ascent, so |dy| across a wrap far exceeds 0.5 * max glyph height.
static void drawSelectionSpans(void *cmdBuffer, float surfaceW, float surfaceH,
                               float x, float y, const RichText *tm,
                               int32_t selStart, int32_t selEnd, uint32_t hlColor, float op) {
    if (selStart < 0 || selEnd <= selStart)
        return;
    float ba = ((hlColor >> 24) & 0xFF) / 255.0f * op;
    if (ba <= 0.0f)
        return;
    float br = ((hlColor >> 16) & 0xFF) / 255.0f;
    float bgc = ((hlColor >> 8) & 0xFF) / 255.0f;
    float bb = (hlColor & 0xFF) / 255.0f;

    float maxGlyphH = 0.0f;
    for (size_t i = 0; i < (*tm).quadCount; i++) {
        const TextQuad *q = &(*tm).quads[i];
        if ((*q).charIndex < 0 || (*q).advance <= 0)
            continue;
        if ((*q).h > maxGlyphH)
            maxGlyphH = (*q).h;
    }
    float eps = maxGlyphH > 0.0f ? maxGlyphH * 0.5f : 4.0f;

    bool inSpan = false;
    float sx0 = 0.0f, sx1 = 0.0f, sy0 = 0.0f, sy1 = 0.0f;
    for (size_t i = 0; i < (*tm).quadCount; i++) {
        const TextQuad *q = &(*tm).quads[i];
        if ((*q).charIndex < 0 || (*q).advance <= 0)
            continue;
        if ((*q).charIndex < selStart || (*q).charIndex >= selEnd)
            continue;
        if (!inSpan) {
            sx0 = (*q).x;
            sx1 = (*q).x + (*q).advance;
            sy0 = (*q).y;
            sy1 = (*q).y + (*q).h;
            inSpan = true;
            continue;
        }
        if ((*q).y - sy0 > eps) {
            Vk_fillRect(cmdBuffer, surfaceW, surfaceH, x + sx0, y + sy0, sx1 - sx0, sy1 - sy0, br, bgc, bb, ba);
            sx0 = (*q).x;
            sx1 = (*q).x + (*q).advance;
            sy0 = (*q).y;
            sy1 = (*q).y + (*q).h;
            continue;
        }
        if ((*q).x + (*q).advance > sx1)
            sx1 = (*q).x + (*q).advance;
        if ((*q).y < sy0)
            sy0 = (*q).y;
        if ((*q).y + (*q).h > sy1)
            sy1 = (*q).y + (*q).h;
    }
    if (inSpan)
        Vk_fillRect(cmdBuffer, surfaceW, surfaceH, x + sx0, y + sy0, sx1 - sx0, sy1 - sy0, br, bgc, bb, ba);
}

// Stage 2: selection spans beneath glyph quads, in tree order.
// Background stays the Panel default (stage 0). Selection-under-glyphs order
// is preserved verbatim from the legacy monolith: spans first, then quads.
static bool richPaintText(Panel *panel, void *renderer, void *cmdBuffer,
                          float surfaceW, float surfaceH,
                          float x, float y, float w, float h) {
    RichLabel *rl = (RichLabel*) panel;
    (void)renderer;
    (void) w;
    (void) h;
    if (!rl || !cmdBuffer)
        return false;
    Component *c = &(*panel).component;
    float op = GraphicsComponent_getOpacity(c);
    if (op <= 0.0f)
        return false;
    RichText *tm = (*rl).textModel;
    if (!tm || !(*tm).quads)
        return false;
    int32_t selStart = -1, selEnd = -1;
    TextSelect *sel = &(*rl).select;
    if ((*rl).highlightable && TextSelect_getSpan(sel, &selStart, &selEnd)) {
        if (selEnd > selStart)
            drawSelectionSpans(cmdBuffer, surfaceW, surfaceH, x, y, tm, selStart, selEnd,
                               (*rl).highlightColor, op);
    }
    bool drew = (selEnd > selStart) && (*rl).highlightable;
    for (size_t i = 0; i < (*tm).quadCount; i++) {
        TextQuad *q = &(*tm).quads[i];
        float cr = (((*q).color >> 16) & 0xFF) / 255.0f;
        float cg = (((*q).color >> 8) & 0xFF) / 255.0f;
        float cb = ((*q).color & 0xFF) / 255.0f;
        float ca = (((*q).color >> 24) & 0xFF) / 255.0f * op;
        if (ca <= 0.0f)
            continue;
        float qx = x + (*q).x;
        float qy = y + (*q).y;
        if ((*q).decor != DECOR_NONE || (*q).textureId < 0) {
            Vk_fillRect(cmdBuffer, surfaceW, surfaceH, qx, qy, (*q).w, (*q).h, cr, cg, cb, ca);
        } else if ((*q).isColor) {
            Vk_drawColorGlyph(cmdBuffer, surfaceW, surfaceH, qx, qy, (*q).w, (*q).h,
                              ca, (*q).textureId, (*q).u0, (*q).v0, (*q).u1, (*q).v1);
        } else {
            Vk_drawSDFText(cmdBuffer, surfaceW, surfaceH, qx, qy, (*q).w, (*q).h,
                           cr, cg, cb, ca, (*q).textureId, (*q).bold, 0.5f,
                           (*q).u0, (*q).v0, (*q).u1, (*q).v1);
        }
        drew = true;
    }
    return drew;
}

// ============================================================================
// CONSTRUCTORS
// ============================================================================

RichLabel *RichLabel_0(void) {
    RichLabel *rl = (RichLabel*) Memory_alloc(TYPE_RICH_LABEL_SINGLETON, sizeof(RichLabel));
    if (!rl) return NULL;

    Panel *p = Panel_0();
    if (!p) {
        Memory_free(rl);
        return NULL;
    }

    (*rl).base = *p;
    Memory_free(p);

    (*rl).textModel = NULL;
    (*rl).wrapMode = WRAP_WORD;
    (*rl).textAlign = TEXT_ALIGN_LEFT;
    (*rl).spacingWidth = 0.0f;
    (*rl).spacingHeight = 0.0f;
    (*rl).ligatures = true;
    (*rl).cursor = Cursor_getPredefined(CURSOR_DEFAULT);
    (*rl).highlightable = false;
    (*rl).select = TextSelect_default();
    (*rl).highlightColor = 0x662563EBu;
    Panel *rp = &(*rl).base;
    Panel_setRenderHandler(rp, nullptr);
    Panel_setTextFn(rp, richPaintText);

    return rl;
}

RichLabel *RichLabel_1(Panel *parent) {
    RichLabel *rl = RichLabel_0();
    if (rl && parent) {
        Panel_addContainer(parent, &(*rl).base);
    }
    return rl;
}

void RichLabel_free(RichLabel *label) {
    if (!label)
        return;
    (*label).textModel = nullptr;
    (*label).cursor = nullptr;
    Memory_free(label);
}

// ============================================================================
// SETTERS
// ============================================================================

void RichLabel_setTextModel(RichLabel *label, RichText *model) {
    if (!label) return;
    (*label).textModel = model;
}

void RichLabel_setWrapMode(RichLabel *label, WrapMode mode) {
    if (!label) return;
    (*label).wrapMode = mode;
    if ((*label).textModel) {
        RichText_setWrapMode((*label).textModel, mode);
    }
}

void RichLabel_setTextAlign(RichLabel *label, TextAlign align) {
    if (!label) return;
    (*label).textAlign = align;
}

TextAlign RichLabel_getTextAlign(const RichLabel *label) {
    return label ? (*label).textAlign : TEXT_ALIGN_LEFT;
}

void RichLabel_setSpacingWidth(RichLabel *label, float width) {
    if (!label) return;
    (*label).spacingWidth = width;
}

float RichLabel_getSpacingWidth(const RichLabel *label) {
    return label ? (*label).spacingWidth : 0.0f;
}

void RichLabel_setSpacingHeight(RichLabel *label, float height) {
    if (!label) return;
    (*label).spacingHeight = height;
}

float RichLabel_getSpacingHeight(const RichLabel *label) {
    return label ? (*label).spacingHeight : 0.0f;
}

void RichLabel_setLigatures(RichLabel *label, bool flag) {
    if (!label) return;
    (*label).ligatures = flag;
}

bool RichLabel_hasLigatures(const RichLabel *label) {
    return label ? (*label).ligatures : true;
}

void RichLabel_setHighlightable(RichLabel *label, bool flag) {
    if (!label) return;
    (*label).highlightable = flag;
    if (flag) {
        (*label).cursor = Cursor_getPredefined(CURSOR_IBEAM);
    } else {
        (*label).cursor = Cursor_getPredefined(CURSOR_DEFAULT);
        TextSelect_reset(&(*label).select);
    }
}

void RichLabel_setSelection(RichLabel *label, int32_t start, int32_t end) {
    if (!label) return;
    if (start < 0 || end < 0) {
        TextSelect_reset(&(*label).select);
    } else {
        TextSelect_begin(&(*label).select, start);
        TextSelect_drag(&(*label).select, end);
    }
}

void RichLabel_setHighlightColor(RichLabel *label, uint32_t color) {
    if (!label) return;
    (*label).highlightColor = color;
}

void RichLabel_setHighlightColorRGBA(RichLabel *label, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!label) return;
    uint32_t packed = ((uint32_t) a << 24) | ((uint32_t) r << 16) | ((uint32_t) g << 8) | (uint32_t) b;
    RichLabel_setHighlightColor(label, packed);
}

void RichLabel_setHovered(RichLabel *label, bool hovered) {
    if (!label) return;
    TextSelect_setHovered(&(*label).select, hovered);
}

void RichLabel_setCursor(RichLabel *label, Cursor *cursor) {
    if (!label) return;
    (*label).cursor = cursor;
}

// ============================================================================
// GETTERS
// ============================================================================

bool RichLabel_isHighlightable(const RichLabel *label) {
    return label ? (*label).highlightable : false;
}

void RichLabel_getSelection(const RichLabel *label, int32_t *outStart, int32_t *outEnd) {
    int32_t s0 = -1, s1 = -1;
    if (label)
        (void) TextSelect_getSpan(&(*label).select, &s0, &s1);
    if (outStart) (*outStart) = s0;
    if (outEnd) (*outEnd) = s1;
}

uint32_t RichLabel_getHighlightColor(const RichLabel *label) {
    return label ? (*label).highlightColor : 0;
}

void RichLabel_getHighlightColorRGBA(const RichLabel *label, uint8_t *outR, uint8_t *outG, uint8_t *outB, uint8_t *outA) {
    uint32_t c = label ? (*label).highlightColor : 0;
    if (outA) (*outA) = (uint8_t) ((c >> 24) & 0xFF);
    if (outR) (*outR) = (uint8_t) ((c >> 16) & 0xFF);
    if (outG) (*outG) = (uint8_t) ((c >> 8) & 0xFF);
    if (outB) (*outB) = (uint8_t) (c & 0xFF);
}

bool RichLabel_isHovered(const RichLabel *label) {
    return label ? TextSelect_isHovered(&(*label).select) : false;
}

Cursor *RichLabel_getCursor(const RichLabel *label) {
    return label ? (*label).cursor : nullptr;
}

// Tag-stripped plain copy of the committed selection. Glyph quads are laid out
// in string order and carry their starting byte offset into the TAGGED string;
// for each selected quad we emit exactly its source bytes — the window up to
// the next glyph — minus style tags, so neither tags nor decor quads (charIndex
// -1) leak into the clipboard. Escaped \[ is emitted as a literal bracket.
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

char *RichLabel_getSelectedText(const RichLabel *label) {
    if (!label || !(*label).textModel)
        return nullptr;
    int32_t lo = -1, hi = -1;
    if (!TextSelect_getSpan(&(*label).select, &lo, &hi))
        return nullptr;
    if (hi <= lo)
        return nullptr;
    const RichText *tm = (*label).textModel;
    if (!(*tm).rawString || !(*tm).quads || (*tm).quadCount == 0)
        return nullptr;
    const char *s = (*tm).rawString;
    int32_t rawLen = (int32_t) strlen(s);
    if (lo < 0)
        lo = 0;
    if (hi > rawLen)
        hi = rawLen;
    if (hi <= lo)
        return nullptr;

    // Two passes over the selected glyph quads: count the stripped byte size,
    // then emit. Both are cold-path (only on explicit Cmd+C), never on frames.
    size_t cap = 0;
    for (size_t i = 0; i < (*tm).quadCount; i++) {
        const TextQuad *q = &(*tm).quads[i];
        if ((*q).charIndex < 0 || (*q).advance <= 0)
            continue;
        int32_t c = (*q).charIndex;
        if (c < lo || c >= hi)
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
    if (cap == 0)
        return nullptr;

    char *out = (char*) Memory_alloc(TYPE_ARRAY, cap + 1);
    if (!out)
        return nullptr;
    size_t at = 0;
    for (size_t i = 0; i < (*tm).quadCount && at < cap; i++) {
        const TextQuad *q = &(*tm).quads[i];
        if ((*q).charIndex < 0 || (*q).advance <= 0)
            continue;
        int32_t c = (*q).charIndex;
        if (c < lo || c >= hi)
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
    out[at] = '\0';
    return out;
}