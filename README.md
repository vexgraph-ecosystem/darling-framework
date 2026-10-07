# darling-framework

## CLion: CMake is IDE metadata only

Open this repository root as a CMake project. `CMakeLists.txt` provides C23
source targets, include paths and flags for navigation, diagnostics and inlay
hints. Targets are excluded from the default build; no linking, dependency
downloads or application runner are wired into it. Set `VEXSPOKE_SOURCE_DIR`
and `GRAPHVEX_SOURCE_DIR` to local `src/` checkouts, and `HOTCWAP_SOURCE_DIR`
to the Hotcwap root containing `window/`. Missing headers stay real IDE errors;
no fake declarations are generated. IDE appearance is user-verified.

Build with [b](https://github.com/vex-graph/b), not this adapter. From the
Vexgraph workspace root: `./tools/b build darling`. IDE metadata is not widget
readiness, appearance approval or proof of standalone runtime dependency closure.

The remastered C23 retained UI framework over graphvex (R3) and hotcwap (R1).
The live substrate includes Frame, Panel, shared properties/events/cursors and
a **partial ScrollPanel viewport core**. Most higher-level components are drafts.

## R2 computation and storage

Vexspoke owns CPU computation, math, algorithms, synchronization and behavior.
Relational Engine owns memory/storage, stable row chunks, variable bindings and
native C search over Rust-owned spans. Migration is staged: existing Vexspoke
memory/container ABI and its default allocator remain until explicit migration
and owner proof. Darling's include allowlist stays Vexspoke + Graphvex + Hotcwap;
this split does not grant a direct engine dependency. R1 owns lifetimes/residency;
GPU shaders/dispatch remain Graphvex R3. No C/Rust atomic-layout compatibility,
automatic schema migration or Rust-backed widget implementation is implied.

## Rendering ownership and bounds

Darling provides widget interfaces, tree construction, layout policy, input/focus
and the native-window/application bridge. **Graphvex owns graphical element
composition**, isolation, filter execution and rendering bounds. Darling consumes
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

```sh
./tools/b build darling # from the Vexgraph workspace root
```

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
