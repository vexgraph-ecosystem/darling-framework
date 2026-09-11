#include "rich_label.h"
#include "vulkan/vk.h"
#include "nio/mem.h"
#include "oop/type.h"
#include "annotation/overview.h"
#include <string.h>

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
 * flat per-line spans behind the glyph quads. RichLabel never shows a
 * caret — labels are non-editable surfaces (carets belong to Input).
 *
 * STRUCT FIELDS (Mirroring darling/label/rich_label.h):
 * ----------------------------------------------------------------------------
 *   Panel base;              // Inherited layout, bounds, and hierarchy state
 *   RichText *textModel;     // Owned styled-text layout model (see rich_text.h)
 *   WrapMode wrapMode;       // Line-wrap policy inherited by the layout engine
 *   Cursor *cursor;          // Active mouse cursor style (I-beam when highlightable)
 *   bool highlightable;      // Enables text selection drag (no caret)
 *   int32_t selectionStart;  // Fixed selection anchor (byte index, -1 = none)
 *   int32_t selectionEnd;    // Active drag edge (byte index, -1 = none)
 *   uint32_t highlightColor; // Packed 0xAARRGGBB selection fill
 *   bool hovered;            // True if pointer is currently hovering within bounds
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
 *   - RichLabel_renderFn(panel, rend, cmd, surfaceW, surfaceH, x, y, w, h) : Draw handler
 *   - RichLabel_charIndexAt(label, localX, localY)                        : Byte index from point
 *   - RichLabel_handlePointer(label, kind, localX, localY, window)        : Pointer event dispatcher
 *   - RichLabel_onPointer(label, ev, window)                              : PointerEvent wrapper
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
    Container *c = &(*p).base;
    float w = (*c).w;
    float h = (*c).h;
    const RichText *tm = (*label).textModel;
    if (w <= 0.0f && tm)
        w = (*tm).layoutWidth;
    if (h <= 0.0f && tm)
        h = (*tm).layoutHeight;

    bool inside = (localX >= 0.0f && localX <= w && localY >= 0.0f && localY <= h);

    if (kind == PTR_LEAVE || (!inside && (kind == PTR_MOVE || kind == PTR_HOVER))) {
        if ((*label).hovered) {
            (*label).hovered = false;
            if (window) {
                Cursor *defCursor = Cursor_getPredefined(CURSOR_DEFAULT);
                Cursor_apply(defCursor, window);
            }
            Container_markDirty(&(*label).base.base);
        }
        return;
    }

    if (inside && (kind == PTR_ENTER || kind == PTR_MOVE || kind == PTR_HOVER)) {
        if (!(*label).hovered) {
            (*label).hovered = true;
            if ((*label).highlightable && window)
                Cursor_apply((*label).cursor, window);
            Container_markDirty(&(*label).base.base);
        } else if ((*label).highlightable && window) {
            Cursor_apply((*label).cursor, window);
        }
    }

    if ((*label).highlightable) {
        if (kind == PTR_DOWN) {
            if (!inside) {
                if ((*label).selectionStart != -1 || (*label).selectionEnd != -1) {
                    (*label).selectionStart = -1;
                    (*label).selectionEnd = -1;
                    Container_markDirty(&(*label).base.base);
                }
                return;
            }
            // Down = fixed anchor + collapsed selection. Drag then moves only the
            // active edge, so dragging left (backward) then right past the anchor
            // selects exactly [anchor, active] — never a rolling union.
            int32_t idx = RichLabel_charIndexAt(label, localX, localY);
            (*label).selectionStart = idx;
            (*label).selectionEnd = idx;
            Container_markDirty(&(*label).base.base);
        } else if (kind == PTR_DRAG) {
            if ((*label).selectionStart < 0)
                return;
            int32_t idx = RichLabel_charIndexAt(label, localX, localY);
            (*label).selectionEnd = idx;
            Container_markDirty(&(*label).base.base);
        } else if (kind == PTR_UP) {
            if ((*label).selectionStart < 0)
                return;
            if ((*label).selectionStart > (*label).selectionEnd) {
                int32_t tmp = (*label).selectionStart;
                (*label).selectionStart = (*label).selectionEnd;
                (*label).selectionEnd = tmp;
            }
            if ((*label).selectionStart == (*label).selectionEnd) {
                (*label).selectionStart = -1;
                (*label).selectionEnd = -1;
            }
            Container_markDirty(&(*label).base.base);
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

static void RichLabel_renderFn(Panel *panel, void *renderer, void *cmdBuffer, float surfaceW, float surfaceH,
                               float x, float y, float w, float h) {
    RichLabel *rl = (RichLabel*) panel;
    (void)renderer;

    float op = panel ? Container_getOpacity(&(*panel).base) : 1.0f;
    if (op <= 0.0f)
        return;
    uint32_t bgColor = Panel_getBackgroundColor(panel);
    if ((bgColor >> 24) > 0) {
        float br = ((bgColor >> 16) & 0xFF) / 255.0f;
        float bg = ((bgColor >> 8) & 0xFF) / 255.0f;
        float bb = (bgColor & 0xFF) / 255.0f;
        float ba = ((bgColor >> 24) & 0xFF) / 255.0f * op;
        if (ba > 0.0f)
            Vk_fillRect(cmdBuffer, surfaceW, surfaceH, x, y, w, h, br, bg, bb, ba);
    }

    RichText *tm = (*rl).textModel;
    if (!tm || !(*tm).quads)
        return;

    int32_t selStart = -1, selEnd = -1;
    if ((*rl).highlightable) {
        selStart = (*rl).selectionStart;
        selEnd = (*rl).selectionEnd;
        if (selStart >= 0 && selEnd >= 0 && selStart > selEnd) {
            int32_t tmp = selStart;
            selStart = selEnd;
            selEnd = tmp;
        }
        if (selStart >= 0 && selEnd > selStart)
            drawSelectionSpans(cmdBuffer, surfaceW, surfaceH, x, y, tm, selStart, selEnd,
                               (*rl).highlightColor, op);
    }

    for (size_t i = 0; i < (*tm).quadCount; i++) {
        TextQuad *q = &(*tm).quads[i];

        float cr = (((*q).color >> 16) & 0xFF) / 255.0f;
        float cg = (((*q).color >> 8) & 0xFF) / 255.0f;
        float cb = ((*q).color & 0xFF) / 255.0f;
        float ca = (((*q).color >> 24) & 0xFF) / 255.0f * op;

        float qx = x + (*q).x;
        float qy = y + (*q).y;

        if ((*q).decor != DECOR_NONE || (*q).textureId < 0) {
            // TODO: Pass decor to a specialized shader for Dash/Squiggle.
            // For now, it draws a solid line.
            Vk_fillRect(cmdBuffer, surfaceW, surfaceH, qx, qy, (*q).w, (*q).h, cr, cg, cb, ca);
        } else if ((*q).isColor) {
            Vk_drawColorGlyph(cmdBuffer, surfaceW, surfaceH, qx, qy, (*q).w, (*q).h,
                              ca, (*q).textureId, (*q).u0, (*q).v0, (*q).u1, (*q).v1);
        } else {
            Vk_drawSDFText(cmdBuffer, surfaceW, surfaceH, qx, qy, (*q).w, (*q).h,
                           cr, cg, cb, ca, (*q).textureId, (*q).bold, 0.5f,
                           (*q).u0, (*q).v0, (*q).u1, (*q).v1);
        }
    }
}

// ============================================================================
// CONSTRUCTORS
// ============================================================================

RichLabel *RichLabel_0(void) {
    RichLabel *rl = (RichLabel*) Memory_alloc(TYPE_PANEL_SINGLETON, sizeof(RichLabel));
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
    (*rl).cursor = Cursor_getPredefined(CURSOR_DEFAULT);
    (*rl).highlightable = false;
    (*rl).selectionStart = -1;
    (*rl).selectionEnd = -1;
    (*rl).highlightColor = 0x662563EBu;
    (*rl).hovered = false;
    Panel_setRenderHandler(&(*rl).base, RichLabel_renderFn);

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
    Container_markDirty(&(*label).base.base);
}

void RichLabel_setWrapMode(RichLabel *label, WrapMode mode) {
    if (!label) return;
    (*label).wrapMode = mode;
    if ((*label).textModel) {
        RichText_setWrapMode((*label).textModel, mode);
    }
    Container_markDirty(&(*label).base.base);
}

void RichLabel_setHighlightable(RichLabel *label, bool flag) {
    if (!label) return;
    (*label).highlightable = flag;
    if (flag) {
        (*label).cursor = Cursor_getPredefined(CURSOR_IBEAM);
    } else {
        (*label).cursor = Cursor_getPredefined(CURSOR_DEFAULT);
        (*label).selectionStart = -1;
        (*label).selectionEnd = -1;
        (*label).hovered = false;
    }
    Container_markDirty(&(*label).base.base);
}

void RichLabel_setSelection(RichLabel *label, int32_t start, int32_t end) {
    if (!label) return;
    (*label).selectionStart = start;
    (*label).selectionEnd = end;
    Container_markDirty(&(*label).base.base);
}

void RichLabel_setHighlightColor(RichLabel *label, uint32_t color) {
    if (!label) return;
    (*label).highlightColor = color;
    Container_markDirty(&(*label).base.base);
}

void RichLabel_setHighlightColorRGBA(RichLabel *label, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!label) return;
    uint32_t packed = ((uint32_t) a << 24) | ((uint32_t) r << 16) | ((uint32_t) g << 8) | (uint32_t) b;
    RichLabel_setHighlightColor(label, packed);
}

void RichLabel_setHovered(RichLabel *label, bool hovered) {
    if (!label) return;
    (*label).hovered = hovered;
    Container_markDirty(&(*label).base.base);
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
    if (!label) {
        if (outStart) (*outStart) = -1;
        if (outEnd) (*outEnd) = -1;
        return;
    }
    int32_t s0 = (*label).selectionStart;
    int32_t s1 = (*label).selectionEnd;
    if (s0 >= 0 && s1 >= 0 && s0 > s1) {
        int32_t tmp = s0;
        s0 = s1;
        s1 = tmp;
    }
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
    return label ? (*label).hovered : false;
}

Cursor *RichLabel_getCursor(const RichLabel *label) {
    return label ? (*label).cursor : nullptr;
}