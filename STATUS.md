# Darling implementation checklist

Source audit: **2026-10-04**. This is the current remastered framework, not the
retired implementation in `_trash/`. Update this file when a capability changes.
Per-file executed commands, content hashes and actual Unix completion timestamps
live in the workspace's `tests/test-checklist.md`.

**Three different things:** implemented behavior, a passing scoped lab check,
and user visual approval. A ✅ for scaffold compilation does **not** mean the
component is implemented. No visual approval is recorded here.

## What exists now

| Part | Implementation | Evidence / limits | Still missing |
| :--- | :--- | :--- | :--- |
| Frame | Real window/root tree and Surface/Board presentation/revalidation code | Existing implementation; no real-window execution in this audit | User appearance/interaction approval; this audit does not re-prove the native seam |
| Panel | Live Element wrapper, child ownership, geometry, paint properties and event adders | `panel_test`, `properties_test`, `panel_events_test` pass headlessly | A Panel is a primitive, not a completed library of controls |
| ScrollPanel | **Partial but real:** clipped viewport, content, explicit offsets, clamp/reclamp, revalidation | `scroll_panel_test` passes; `panel_events_test` passes with a manually installed scroll handler | Automatic wheel behavior, ready-to-use Panel/Frame attachment, scrollbar widgets and user visual approval |
| Shared properties | Add/remove, location, size, min/max bounds, corner radius, cursor and revalidation implementations | `properties_test` proves its Panel branches; not every operation/Frame branch | Many other `properties/` files remain drafts |
| Events and pointer bridge | Typed handler registration/bubbling and native pointer forwarding code | `event_invoke_test`, `event_kinds_test`, `panel_events_test` pass synthetic/headless checks | Focus/navigation/document/control policies remain drafts; native input not exercised here |
| Cursor selection | Per-node cursor preference/inheritance and masked hit selection | `set_cursor_test` passes headlessly | Native cursor appearance remains user-unverified |
| Text / Label / typography | **Not implemented:** draft declarations only | Scaffold compilation is structure proof only | Fonts, string-to-glyph rendering, measurement, Label storage/API/paint, selection and editing |
| Other widgets/layouts | **126 draft pairs:** 91 opaque class pairs + 35 procedural modules | `scaffolds.json` and `SCAFFOLDS.md` describe intent, not delivered features | Buttons, ScrollBar, Flex/Grid/List panels, inputs, themes, overlays, etc. |

There are **16 non-draft `.c` source homes**, including header-only overload
vocabulary and shared operation modules. This is not 16 finished widgets.

## ScrollPanel: core versus usable widget

Current source: `src/panel/scroll_panel.c` and `.h`.

- [x] Construct a viewport and attach an Element content tree.
- [x] Clamp explicit offsets and re-clamp when extents shrink.
- [x] Reflect scrolling into negative content placement; enable viewport clipping.
- [x] Register an explicit `ScrollPanel_addScrollEvent` callback that calls
  `ScrollPanel_scrollBy` (the synthetic event test does this).
- [ ] Install default wheel/trackpad behavior in a newly constructed ScrollPanel.
- [ ] Define nested-scroll consumption, direction/units and boundary behavior.
- [ ] Add typed ScrollPanel support to the shared attachment/ownership APIs:
  `Frame_add` and `Panel_add` currently accept **Panel**, not ScrollPanel.
  Low-level Element attachment exists; it is not equivalent wrapper ownership.
- [ ] Implement `src/input/scroll_bar.*` (currently an opaque draft).
- [ ] Implement `src/properties/set_scroll_offset.*` (currently a module draft;
  the existing functional setter is `ScrollPanel_setOffset`).
- [ ] Have the user inspect an intentionally provided scrolling view; do not
  infer visual approval from offset assertions or old captured-pixel tests.

## Text: actual blockers

- [ ] Implement a font/glyph/measurement path in the live R3 driver.
- [ ] Replace raster `drawText`'s no-op: it currently ignores the rectangle,
  string and brush and returns success without drawing text.
- [ ] Replace Vulkan `drawText`'s placeholder: it ignores the input string and
  submits glyph ID zero rather than shaping/rendering its characters.
- [ ] Implement TextCore / Label ownership, constructors, setters and paint.
- [ ] Add a headless text oracle that proves the input string changes pixels
  and verifies clipping/measurement; success return values alone are not proof.
- [ ] Add typography, rich text, caret/selection/editing only after basic text works.
- [ ] Obtain user visual feedback on actual readable text.

Relevant drafts: `src/text/text_core.*`, `src/text/typography.*`,
`src/label/label.*`, `src/label/rich_label.*`, `src/properties/set_text.*`,
`src/properties/set_font.*` and `src/bridge/text_bridge.*`.
The live graphvex source tree has no font/text implementation directories.

Retired reference code remains in `_trash/darling-framework/text/`,
`_trash/darling-framework/objc/text_core.m`,
`_trash/darling-framework/darling/label/` and `_trash/graphvex/src/font/`.
These are not live dependencies. Restoration needs a deliberate ownership/API
review, not a blind copy of old code into the remastered tree.

## Small next steps, not more scaffolds

Implemented starter/presentation contract: [Application lifecycle](APPLICATION_LIFECYCLE.md).
All Darling C starters enter Application lifetime; worker start events and
focus-aware Frame presentation caps now exist. Primitive tests now attach native
starter windows. See the new lifecycle/FPS owner tests for scoped proof, not
whole-platform or visual approval.

1. Make real glyph rendering + a basic Label work before adding text-dependent controls.
2. Finish ScrollPanel input/attachment/ownership ergonomics, then ScrollBar.
3. Prove those scopes with owner lab tests, then ask the user for visual feedback.
4. Expand to one usable control/layout at a time; do not promote the other drafts.

## Fresh headless checks for this audit

`panel_test`, `scroll_panel_test`, `panel_events_test`, `properties_test`,
`set_cursor_test`, `event_invoke_test` and `event_kinds_test`: **7 passed**.
Commands and timestamps are recorded in `tests/test-checklist.md`. No window,
demo, visual tour, whole-suite claim or text-rendering readiness was exercised.
