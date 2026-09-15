# darling, by Vex, truly.

A flexible, node-oriented UI C API — human to graphic interaction at hardware speed.

A play on the affectionate lineage of **Dear ImGui** and **LÖVE2D** — where *Dear ImGui* brought warmth and approachable intimacy to developer graphics and *LÖVE* made creative game scripting joyful, `darling` takes that same human intimacy and brings it down to bare-metal C23 as a **retained-mode, off-heap UI toolkit**.

While the low-level engine thrives on cold bit manipulation and atomic CAS, the UI is the one layer where human hands touch graphics. `darling` rejects heavy polymorphic class hierarchies, DOM abstraction layers, and garbage-collected UI trees.

Instead, `darling` combines **self-describing C23 memory blocks** with a **hardware-native hybrid compositor**, allowing vector UI panels, rich text, and Vulkan 3D scenes to coexist on the exact same display surface with zero double-rendering overhead. It is a darling to write, a darling to read, and a darling to interact with.

---

## Key Architecture & Strengths

* **The "No Double-Render Law"**: Floating UI panels, HUDs, and interactive widgets are backed by hardware-pixel `IOSurface` layers. The macOS WindowServer (AppKit / CoreAnimation) composites these overlays directly on top of the Vulkan swapchain with native Retina crispness. The Vulkan swapchain blit loop skips non-scene panels entirely, saving precious GPU rasterization cycles.
* **Dual-Path Typography Engine**:
  * **Sharp Path**: Subpixel, line-by-line native Apple CoreText rasterization (`objc/text_core.m`) directly into 1:1 pixel-accurate textured quads.
  * **SDF Path**: High-precision signed distance field generation accelerated via the **GPU Jump-Flood Algorithm (JFA)** using Vulkan compute shaders (`sdf_jfa.comp`, `sdf_combine.comp`) for scale-invariant rendering and outline/glow effects.
* **TrueType Baking & VFS**: Built-in TrueType font parsing (`stb_truetype.h`), atlas baking, and custom binary font packaging (`.antifont`) stored in `~/vex/fonts` — engine lives in `graphvex` (`font/`, `fontbake` CLI), darling consumes it via link.
* **Symmetric Getter/Setter Completeness (Rule 24)**: Modeled after the ergonomics of an idiomatic Java or C# library, every UI node provides complete, type-safe getters and setters (`setText`/`getText`, `setFontSize`/`getFontSize`). Callers never have to manually pierce internal nested struct pointers.
* **The Living `;;OVERVIEW` Blueprint (Rule 23)**: Every implementation file documents its struct fields, inheritance hierarchy, and four-tier method index (`constructor`, `core functions`, `setters`, `getters`) in the first 100–150 lines.

---

## Workspace Integration & How to Use It

`darling` is an R2 feature in the supervisor order (Rule 17: `R0 hotcwap > R1 vexspoke > R1.5 graphvex > R2 features > R3 engines`), registering into the R0 Kernel via `Application` + `HotModule`. It depends on `vexspoke` (core memory & math), `graphvex` (GPU), and `hotcwap` (native OS windowing):

```
workspace/
├── cmake-build-debug/           # Out-of-tree CMake build artifacts & staged SPVs
├── projects/                    # Vertically integrated subsystem repositories
│   ├── vexspoke/                # Bedrock C23 platform runtime (Layer 1)
│   ├── hotcwap/                 # Dynamic hot-reloading & native OS windowing (Layer 2)
│   ├── darling/                 # Retained-mode UI nodes & Vulkan render passes (this library)
│   │   └── src/                 # Canvas, panels, labels, CoreText, SDF
│   ├── api-haven/               # Telemetry schemas & Discord webhook transmitters (Layer 4)
│   └── [other projects connecting to each other go here]
├── CMakeLists.txt               # Umbrella workspace orchestrator
└── preferences.md               # Engine architectural style preferences (Rules 1–n)
```

### 1. In-Tree Integration (Subdirectory)
When integrated inside an umbrella workspace:

```cmake
# In your top-level CMakeLists.txt
add_subdirectory(projects/darling)

add_executable(my_app spoke.c)
target_link_libraries(my_app PRIVATE darling hotcwap vexspoke)
```

### 2. Standalone Integration (FetchContent Seam)
When building standalone or in downstream projects:

```cmake
if (NOT TARGET darling)
  include(FetchContent)
  FetchContent_Declare(
          darling
          GIT_REPOSITORY https://github.com/vexgraph-dev/darling.git
          GIT_TAG font
  )
  FetchContent_MakeAvailable(darling)
endif ()

target_link_libraries(my_app PRIVATE darling)
```

---

## What's in this repo

* **`darling/`** — Retained-mode UI nodes:
  * `canvas.h/.c` — Top-level root surface container managing dirty layout propagation.
  * `container.h/.c` — Hierarchical layout node supporting parent/self anchoring, bounds, and child iteration.
  * `panel.h/.c` — Visual container with background colors, custom render hooks, and layout anchoring.
  * `picture.h/.c` — Off-heap image display node with UV cropping and aspect-ratio scaling.
  * `label.h/.c` — High-performance text view with CoreText caching and SDF fallback.
  * `rich_label.h/.c` — Multi-line styled rich text node.
  * `scene.h/.c` — 3D/Vulkan view stamping node.
* **`text/`** — `rich_text.h/.c` layout parser and `text_core.h` platform bridge interface (font atlas/bake engine lives in `graphvex`).
* **`text/`** — `rich_text.h/.c` layout parser and `text_core.h` platform bridge interface.
* **`render/`** — Software rasterization: `raster.h/.c` and `surface.h/.c` for off-screen CPU drawing.
* **`io/`** — `bake.c`, `mmap.c`, and `vfs.c` for fast virtual filesystem resolution and asset memory mapping.
* **`vulkan/`** — GPU rendering pipelines:
  * `sdf_gpu.h/.c` — Compute-shader Jump-Flood Algorithm (JFA) distance-field generator.
  * `vk_iosurface.h/.c` — Zero-copy Vulkan-to-IOSurface hardware bridge.
  * `vk_scene.h/.c` & `vk_view.h/.c` — Scene drawing passes and display cache managers.
  * `texture/texture.h/.c` — Texture uploading, caching, and sampling.
  * `shaders/` & `spv/` — Source compute/vertex/fragment shaders and precompiled SPIR-V binaries.
* **`objc/`** — macOS platform bridges: `text_core.m` (CoreText rasterizer) and `panel_cocoa.m` (CALayer / IOSurface compositing). System font resolution (`font_cocoa.m`) moved down to `graphvex` with the font engine.

---

## Requirements

* C23 compiler (Clang with `-std=gnu23`).
* Apple Silicon (macOS with Cocoa, AppKit, QuartzCore, Metal, IOSurface) or Linux.
* CMake $\ge$ 4.3.
* Vulkan SDK (MoltenVK on macOS).
