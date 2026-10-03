# darling-framework

The remastered C23 retained UI framework over graphvex (R3) and hotcwap (R1).
The live substrate includes Frame, Panel, shared properties/events/cursors and
a **partial ScrollPanel viewport core**. Most higher-level components are drafts.

**Start with [STATUS.md](STATUS.md)** for the current implementation checklist,
known text/scrolling gaps and next steps. [SCAFFOLDS.md](SCAFFOLDS.md) inventories
draft intent; compiling those files does not mean their widgets work.

Text/Label is **not implemented** in this generation. ScrollPanel has clamped
offsets and clipping, but default wheel handling, scrollbar widgets and typed
attachment ergonomics are unfinished. The user owns all visual approval.

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
