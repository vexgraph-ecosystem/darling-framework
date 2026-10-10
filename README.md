# darling-framework

## Current State

**Role:** R4 retained UI toolkit — widget interfaces, tree construction, layout
policy, input/focus and the native-window/application bridge over the graphvex
(R3) compositor and hotcwap (R1) windows.

**Implemented and proven (macOS arm64, window tests opt-in):** `frame.c` (the
retained UI host: window + panel/element tree, Surface/Board revalidation,
double-buffered IOSurface GPU present with RGBA fallback, capture/PNG, Application
lifecycle, FPS policy), `panel.c` ownership, `scroll_panel.c` (partial viewport:
clamp/clip/revalidate/events), nine `properties/*` operations, `event_invoke.c`
dispatch, `hit.c` resolver, `pointer.c` synthetic+OS input, and `picture.c`.
Owner tests exist under `tests/darling/`; `BATTLE_TESTS.md` records a full run.

**Draft / not implemented (source-verified):** of 142 `.c` files, **125 are
37-line `;;DRAFT`/`;;INCOMPLETE` scaffolds with zero API** — all buttons, most
inputs, every layout panel, overlays, dialogs, kit/game/scene/spatial widgets,
theme, bridges, and **Text/Label/typography**. `event/focus.c` and
`layout/container.c` are drafts (no focus traversal, no layout-policy module).
The real CPU image-group compositor is graphvex's, not here.

**Platforms proven:** macOS arm64 only; Linux/Windows native execution unproven.
Visual approval is the user's.

**Evidence:** `tests/darling/` (window-based, opt-in) and `tests/test-checklist.md`
— most `src/**` rows are still ❌ untested; test files existing is not per-file proof.

The remastered C23 retained UI framework over graphvex (R3) and hotcwap (R1).
The live substrate includes Frame, Panel, shared properties/events/cursors and
a **partial ScrollPanel viewport core**. Most higher-level components are drafts.

## R2 computation and storage

Vexspoke owns CPU computation, math, algorithms, synchronization and behavior.
Relational Engine owns memory/storage, stable row chunks, variable bindings and
native C search over Rust-owned spans. Native IO/NIO is now supplied by RE in
default builds, preserving the C ABI, not rewritten into Rust. Broader collection
migration remains staged. Darling may borrow RE + Vexspoke + Graphvex + Hotcwap.
R1 owns lifetimes/residency;
GPU shaders/dispatch remain Graphvex R3. No C/Rust atomic-layout compatibility,
automatic schema migration or Rust-backed widget implementation is implied.

## Rendering ownership and bounds

Darling provides widget interfaces, tree construction, layout policy, input/focus
and the native-window/application bridge. In this generation only the
window/application bridge (`frame.c`), the panel tree and pointer input are
implemented; layout policy (`layout/container.c`) and focus (`event/focus.c`)
are drafts. **Graphvex owns graphical element composition**, isolation, filter execution and rendering bounds. Darling consumes
that R3 compositor; it does not implement a competing rendering/filter engine.
Scene widgets supply independently produced scene images to Graphvex composition.

The event bound remains the resolved layout/hit geometry; the absolute bound
describes expanded paint, including descendants/effects. A blur halo does not
enlarge mouse targeting or move layout. See Graphvex's
[ownership and bounds laws](../graphvex/graphvex-preferences.md).
The first compositor slice is a CPU image-group API; automatic widget-stack and
GPU compositor integration are not yet implied. Existing Frame painting remains
a migration bridge, not an alternate architectural owner.

**Start with [STATUS.md](STATUS.md)** for the current implementation checklist,
known text/scrolling gaps and next steps. [SCAFFOLDS.md](SCAFFOLDS.md) inventories
draft intent; compiling those files does not mean their widgets work.

Text/Label is **not implemented** in this generation. ScrollPanel has clamped
offsets and clipping, but default wheel handling, scrollbar widgets and typed
attachment ergonomics are unfinished. The user owns all visual approval.

## Build

Build with [b](https://github.com/vex-graph/b).

```sh
./tools/b build darling # from the Vexgraph workspace root
```

## Scope and Limitations

**Scope:** R4 widget interfaces, tree construction, layout policy, input/focus and
the native-window/application bridge. Graphvex owns graphical element composition,
filters and render bounds; Darling consumes that R3 compositor.

**Deliberately not covered:**
- No competing renderer/compositor: the real CPU image-group compositor is
  graphvex's; Darling's `src/compositor/compositor.c` is a draft.
- No text/label engine in this generation (Text/Label are drafts).
- No R1/R5 or `api-haven` ownership.

**Known limits and gaps:**
- 125 of 142 `.c` files are drafts with zero API; only Frame/Panel/ScrollPanel,
  the nine properties, event dispatch, hit, pointer and picture are implemented.
- No focus traversal (`event/focus.c` draft) and no layout-policy module
  (`layout/container.c` draft); ScrollPanel lacks default wheel handling, a
  scrollbar and typed attachment ergonomics.
- macOS arm64 only; all visual appearance is user-owned.

<details>
<summary>Historical darling-editor vision — not current framework capabilities</summary>

## darling-editor, by Vex.

The Figma + Miro Inspired Spatial Canvas & Interface Builder.

`darling-editor` is a bare-metal, high-performance spatial whiteboard and UI design studio built directly on `darling-framework` and Vulkan. It unites the collaborative freedom of Miro with the interface construction power of Figma—running locally with zero Electron or webview overhead.

---

## Core Capabilities

1. **Infinite Spatial Canvas**: Smooth 120Hz pan/zoom board for brainstorming, architecture diagrams, sticky notes, and visual mindmaps.
2. **Interactive UI Prototyping**: Drag-and-drop construction of `darling-framework` widgets (`Button`, `CardPanel`, `Switch`, `CodeField`, containers).
3. **Multi-Format Export Engine**:
   - **C23 Code**: Emits idiomatic `darling` construction code (`Class(...)` with 9-grid anchors).
   - **HTML / CSS**: Transpiles canvas interfaces into clean, standalone semantic HTML.
   - **Vector SVG**: Exports vector shapes, connectors, and diagrams as standards-compliant SVGs.
   - **Raster Images**: High-resolution PNG and JPEG rendering via `graphvex` FrameImporter.
4. **Icon Subsystem & SVG Baking**:
   - Comprehensive icon catalog (Lucide, Tabler, Material Icons, Feather).
   - Crisp, infinite-resolution vector rendering baked into Signed Distance Fields (SDFs) using GPU Jump Flooding Algorithms (`sdf_jfa`).
5. **Real-Time Multiplayer Sync**: Pairs with `../sesh` for live multi-user cursors, collaborative note taking, and remote canvas sharing.

</details>
