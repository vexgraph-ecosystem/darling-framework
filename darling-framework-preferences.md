# darling-framework — Repo-Local Living Preferences
> Exclusive repository-level preferences (the Living Preferences Law).
> Universal Supreme Constitution: preferences.md (vexspoke).

;;SYNC("mirrors ecosystem/vexspoke/preferences.md @ 2026.09-universal")

## 0. Constitution Link (supreme)
- [preferences.md](https://github.com/vexgraph-dev/vexspoke/blob/main/preferences.md) (canonical, vexspoke) — accessible locally at ../../preferences.md
- All universal laws in `preferences.md` are mandatory and binding across the ecosystem.
- This document codifies **exclusive** preferences that apply uniquely to `darling-framework` (R4 UI Toolkit).

## 1. Exclusive Preferences Binding Matrix

| Law Title | Scope | Enforcement |
| :--- | :--- | :--- |
| **Window Board Root Lock Law (Dimension Override Law)** | R4 UI Toolkit | Mandatory for `darling-framework` |
| **Single-Seam Canvas Law** | R4 UI Toolkit | Mandatory for `darling-framework` |
| **Sub-Part Field Segregation Law & the `Class_part_verb` Law** | R4 UI Toolkit | Mandatory for `darling-framework` |
| **Living Darling Docs Law (Zero Drift Between Code and `_docs/darling.md`)** | R4 UI Toolkit | Mandatory for `darling-framework` |
| **Panel Gravity Law** | R4 UI Toolkit | Mandatory for `darling-framework` |
| **Window Decoupling Law (a Window is just a Window)** | R4 UI Toolkit | Mandatory for `darling-framework` |
| **Forward Rendering & Bounded Surface Law** | R4 UI Toolkit | Mandatory for `darling-framework` |
| **Absolute Size and Location Law** | R4 UI Toolkit | Mandatory for `darling-framework` |

## 2. Exclusive Repo-Local Laws (FULL PROSE RESTATEMENT)

### Forward Rendering & Bounded Surface Law (Virtual Geometry vs Physical Allocation)

#### Definition:
A container, canvas, or panel's logical coordinate space is completely decoupled from its physical surface memory allocation. A panel may occupy arbitrary logical extents (e.g. 100,000 x 100,000 px for virtual canvases, huge document scrolls, or infinite layout nodes). Physical memory allocation is strictly bounded to the visible screen/viewport bounds (viewportW * viewportH * 4 bytes). Rendering is strictly forward: visible primitives are scissored and forward-rendered into the active target during the paint pass, rather than allocating intermediate deferred textures for off-screen extents.

#### The Why:
Allocating full-extent textures for large or virtual panels exhausts GPU memory instantly (a 100,000 px surface would require gigabytes of VRAM). Virtual panels are pure mathematical metadata; physical memory exists only for what the user can physically see on display hardware.

#### The Rule:
1. **Logical Scale Invariant:** Panel coordinates (x, y, w, h) scale to arbitrary positive dimensions without triggering proportional texture allocations.
2. **Viewport Bounded Allocation:** GPU backing buffers (`IOSurface` or swapchain images) never exceed window or viewport dimensions * backing scale factor.
3. **Forward Clipped Paint:** Primitives intersecting the viewport scissor rect render forward directly into the canvas; off-screen elements are culled before draw submission.

---

### Window Board Root Lock Law (Dimension Override Law, Panel Override Law)

#### Definition:
An empty `Frame` contains no content pane or scene pane by default. When any panel-derived type (`Panel`, `Scene2D`, `Scene3D`, `ListPanel`, `LayeredContainer`, `SplitPanel`, `SectionContainer`, `GridPanel`, `FlexPanel`, `ExpandableListContainer`, `ScrollPanel`, or any future subclass that embeds `Panel` as its first member) is set as the `contentPane` or `scenePane` of a `Frame`, its position, anchor, pivot, and dimensions are **strictly overridden and locked** to the host window's dimensions. The root pane's geometry is $(0, 0, \text{winW}, \text{winH})$, anchor `CONTAINER_ANCHOR_TOP_LEFT` (0), pivot `CONTAINER_PIVOT_TOP_LEFT` (0). Even if the caller subsequently calls `Container_setSize`, `Container_setLocation`, `Container_setAnchor`, or `Container_setPivot` on the root pane, the locked values are silently restored from the frame on the next layout pass and the root pane never escapes its host window bounds.

#### The Why:
A root board pane is not a freely positioned child — it IS the window. Every child in the UI tree descends from a rectangle that is, definitionally, the full window surface. If a root pane were ever allowed to be a different size, children that anchor or percent-place against the parent would compute against the wrong extents, breaking every responsive layout rule in the tree. Locking the root geometry removes an entire class of "why is my layout wrong at startup" bugs that arise when the caller forgets to match the pane size to the window size.

#### The Rule:
1. **`contentPane` and `scenePane` are always locked.** `Frame_setContentPane` and `Frame_setScenePane` (and their polymorphic `Frame_setContentPanel` / `Frame_setScenePanel` macros) apply `lockedRoot = 1`, anchor `TOP_LEFT`, pivot `TOP_LEFT`, location `(0, 0)`, and size `(frame.width, frame.height)` to the incoming panel's `Container` base immediately on set.
2. **`Container_setSize` and `Container_setLocation` silently no-op on locked-root containers.** No assertion, no error, no crash — the call returns without modifying the node. User code that tries to resize a root pane simply does nothing; it is not an API contract violation. (This rule protects code that generically resizes all its panels without knowing which is the root.)
3. **`Frame_resize` force-updates locked roots.** `Frame_resize(frame, w, h)` calls `Container_forceSize` on both `contentPane` and `scenePane` (non-null only) so the root geometry tracks the window live, including during live-resize drag events. `Container_forceSize` bypasses the `lockedRoot` guard and is internal to `darling-framework`; it is not part of the public API surface.
4. **Polymorphic arity: `Frame_setContentPanel` and `Frame_setScenePanel`.** These are `_Generic`-free macros that cast any panel-derived `void*`-compatible pointer to `Panel*` and forward to `Frame_setContentPane` / `Frame_setScenePane`. Any type whose first member is `Container base` inside a `Panel base` (the two-layer embedding pattern used by all darling subclasses) is accepted. The caller must ensure the pointer is a live panel-derived type; no runtime type check is performed (the Single Class Per File Law bans vtable complexity here).
5. **Default root geometry.** The starting point is always $(0, 0)$ top-left. The anchor is `CONTAINER_ANCHOR_TOP_LEFT` (0). The pivot is `CONTAINER_PIVOT_TOP_LEFT` (0). These values match the resolve baseline so the first layout pass never produces a delta from the previous-base reference, preventing a spurious re-layout on the very first frame.
6. **Scope.** The lock applies only to the two board-root slots (`contentPane`, `scenePane`). `rootPanel` and arbitrary child panels are not affected. A panel removed from `contentPane` or `scenePane` (replaced with nullptr or swapped) loses its `lockedRoot` status only if the new holder explicitly calls `Container_setLockedRoot(c, false)` — the flag stays 1 until cleared, which is the correct behavior for recycled panel references.
7. **Panel Override Law: the embedded Component metadata is overridden too.** `Frame_setContentPane` / `Frame_setScenePane` mirror the override into `panel.component` (`COMPONENT_ANCHOR_TOP_LEFT`, `COMPONENT_PIVOT_TOP_LEFT`, location `(0, 0)`, size `(frame.width, frame.height)`) via the plain Component setters. The Component carries no lock flag of its own — the `Panel_*` facades enforce the lock (clause 8) — so the override stays exact on panels whose min/max constraints are unset.
8. **Panel Override Law: facades guard both members, resize forces both.** `Panel_setLocation` / `Panel_setSize` skip the Component write when `Container_isLockedRoot` reports locked (the Container call no-ops internally, unchanged behavior). `Frame_resize` / `Frame_syncResize` call `Component_setSize` on the embedded metadata of every root they force-size (`contentPane`, `scenePane`, detached `rootPanel`) right after the matching `Container_forceSize` — plain setters are the force path on the lock-free member. Readers stay on the Container base until the Component cascade wires up (Shift 2c); the two members never observably diverge through the public facades.

---

### Single-Seam Canvas Law

One window, one blur view, exactly ONE on-screen `CAMetalLayer`: the seam
canvas, owned by the Frame (R4). The seam pass composites the two retained
offscreen board images — content top, scene bottom (the Window Compositing
Layer Order Law) — and presents on demand (the Present-On-Demand Law). Every
scene child is a COMPOSITED layer: a retained offscreen render target in the
`VkLayer` registry (graphvex), rendered on demand by the present loop and
collaged into its board as a sampled quad. There is no DIRECT mode: no
per-scene `CAMetalLayer`, no per-pane swapchain, and no `VkPane` registry —
that machinery is retired (the Conflict Triage Law managed exception ended
when the composite copy became the universal path). The flight machinery —
registry, stable slot index, dual flight slots, acquire/render semaphores,
bounded 100ms fences, dirty bit — is shared verbatim with the retired pane
mode; only the destination differs: a compositable color image the canvas
samples instead of a swapchain image. A layer's pixel size is FIXED at
register/resize time — `VkLayer_resize` is a no-op when the requested size
is unchanged, so fixed targets never rebuild; the window crops the
monitor-sized fixed-buffer seam via non-resizing gravity (the fixed-buffer
plaster).

---

### Sub-Part Field Segregation Law & the `Class_part_verb` Law

A widget that owns a sub-object (caret, scrollbar, gutter, thumb) exposes it
only through `Class_part_*` verbs — never by piercing `->field`. The struct
in the header must segregate fields under part banners so a reader sees at a
glance which state is the owner's and which belongs to each part:

```c
typedef struct Input {
    // --- Input core (owner fields: text state, not any part) ---
    Panel base;
    char *text;
    ...
    // --- Caret part (field.caret.verb; views only, never pierce) ---
    int caretMode;
    uint32_t caretColor;
    ...
    Panel *caretView;   // borrowed visual (null = default); detach-only
    ...
} Input;
```

- Owner core first, then one banner per part, in the order the parts are
  documented in the `;;OVERVIEW`. A part that forwards to another node
  (e.g. ScrollContainer's `panel_*` over content) owns NO fields — say so in a
  `NOTE:` line, since new stored state there is a design smell.
- Sub-object pointers are views: borrowed, detach-only, never freed or
  reparented by the owner. Replacing a view (`scrollbar_setBar`,
  `caret_setView`) detaches the old one and attaches the new one; the arena
  owns the memory, the owner owns the relationship.
- The `;;OVERVIEW` STRUCT FIELDS section mirrors the same banners verbatim
  (the Living `;;OVERVIEW` Blueprint Law) so header and source can never
  drift apart.
- Every part field keeps symmetric getters/setters per the Symmetric
  Getter/Setter Completeness Law — the part API is ergonomic precisely so
  nobody reaches for `->`.

#### The Container-vs-Panel Law
A Container is a multi-child layer-owner (owns N child
layers, manages attach/detach/surfaces, the Sub-Part Field Segregation Law
parts); a Panel is a single-surface leaf painter (one IOSurface, no child
layer management). Multi-child managers are named *Container and embed Panel
as first member; leaf painters are named *Panel or own widget class.
TabbedContainer is banned — tabs are a mode of SectionContainer.

---

### Living Darling Docs Law (Zero Drift Between Code and `_docs/darling.md`)

`_docs/darling.md` (1699+ lines: every widget field, compartment, function,
getter/setter, plus compositor / WindowServer / IOSurface / Vulkan) is a
load-bearing artifact, not a snapshot. An out-of-date section is a defect,
same as a stale `;;OVERVIEW` under the Living `;;OVERVIEW` Blueprint Law.

- **Scope — every class root in darling.** Each class struct / file pair under
  `../../projects/darling/` is a root: `Container`, `Panel`, `Canvas`, every widget
  (`ListContainer`, `GridContainer`, `ScrollContainer`, `SectionContainer`, `LayeredContainer`, `SplitContainer`,
  `MarkdownPanel`, `RichTextPanel`, `Button`, `Switch`, `Checkbox`,
  `RadioGroup`, `Slider`, `Knob`, `Input`, `Textarea`, `InputOTP`, `Select`,
  `DatePicker`, `ColorPicker`, `ColorSwatch`, `ScrollBar`, `Label`,
  `RichLabel`, `Typography`, `Kbd`, `Dialog`, `AlertDialog`, `FileDialog`,
  `ColorDialog`, `Picture`, `Plot`, `Scene`/`Scene2D`/`Scene3D`, `RichText`),
  plus the substrate (`darling-type.h`, compositor, events/dispatch/bridge,
  anim, raster/surface, io/mmap/VFS/fontbake, ObjC bridges, shaders/SPV).
- **Triggers — any of these must update the docs:** struct field
  added/removed/renamed/retyped/default-changed; constructor/core/setter/getter
  signature or behavior change; layout algorithm, clamp, dirty-flag, or
  ownership (`owned` vs `borrowed`) change; compartment move (e.g. a field
  changing from `style` to `state`, a new `feel` or `bridge` slot); compositor /
  IOSurface / Vulkan / WindowServer / event / anim contract change; new or
  removed widget.
- **Same-commit law (extends the Cohesive Commits Law and the Living
  `;;OVERVIEW` Blueprint Law).** Code + `;;OVERVIEW` header + the matching
  `_docs/darling.md` section land in the SAME granular per-class commit.
  Never a follow-up "update docs" commit — follow-ups never happen at 2am.
  A commit that changes darling behavior without its docs section is a broken
  intermediate state per the Commit and Push Discipline Law: keep it dirty on
  disk, do not commit.
- **What "updated" means.** Field table row (type, default, compartment,
  role); function entry with exact C signature + side effects (dirty? layout?
  clamp? reparent?); compartment label corrected if the field moved; one `why`
  sentence if the rationale changed; TOC entry if a section is added/renamed.
  If 2am-you cannot reconstruct the behavior from the docs section alone, the
  update was incomplete — say so in the commit message and finish it.
- **Reviewer checklist (every darling commit):**
  1. `;;OVERVIEW` STRUCT FIELDS + FUNCTION REGISTRY mirror the new struct/API?
  2. `_docs/darling.md` section mirrors the new fields/functions/compartments?
  3. Stub-vs-live status corrected (`;;INCOMPLETE` gained or retired)?
  4. Backend sections (sections 41–48) touched if pixels, events, or teardown changed?
Same-commit law is per file pair: code pair plus overview plus the matching
`_docs/darling.md` section land together; splitting them across commits is a
broken intermediate state.

---

### Panel Gravity Law

The seam canvas layer's `contentsGravity` and `anchorPoint` are pinned from
the window's crop contract: the fixed-buffer seam (monitor-sized drawable)
is drawn 1:1 and cropped top-left, so the canvas carries `kCAGravityTopLeft`
with `anchorPoint (0,0)` — non-resizing gravity that keeps the LAST rendered
frame pinned during a live window resize, before the next frame is ready.
The mapping table below is the canonical currency for any future layer
attach (boards are retained offscreen `VkLayer` targets and gravity-free):

```
TOP_LEFT     → kCAGravityTopLeft    / anchorPoint (0,0)
TOP_CENTER   → kCAGravityTop        / anchorPoint (0.5,0)
TOP_RIGHT    → kCAGravityTopRight   / anchorPoint (1,0)
MIDDLE_LEFT  → kCAGravityLeft       / anchorPoint (0,0.5)
CENTER       → kCAGravityCenter     / anchorPoint (0.5,0.5)
MIDDLE_RIGHT → kCAGravityRight      / anchorPoint (1,0.5)
BOTTOM_LEFT  → kCAGravityBottomLeft / anchorPoint (0,1)
BOTTOM_CENTER→ kCAGravityBottom     / anchorPoint (0.5,1)
BOTTOM_RIGHT → kCAGravityBottomRight/ anchorPoint (1,1)
```

The seam `CAMetalLayer` has `geometryFlipped = YES` so
that Vulkan's top-down coordinate space maps correctly onto CoreAnimation's
bottom-up space.

---

### Window Decoupling Law (a Window is just a Window)

The graphics loop boots only when a window actually hosts a render surface —
`contentPanel` (top board) or `scenePanel` (bottom board) attached, which
happens when an Application *borrows* the Window and registers it into
graphvex's `GfxLoop`. A bare window with no borrower is a plain AppKit
window: native free resize, zero swapchain, zero warm-up presents, zero
`GfxLoop` registration, zero present thread. Rendering never owns the
window's resize; the swapchain machinery is additive on top of an
already-resizable host window, and the loop lives in graphvex (the Vertical
Integration Law) — never inside the Kernel.

A Window is a **dumb surface + callback bridge**: it carries no presentation
logic of its own — only the `CAMetalLayer` frames, the input adapters, and a
set of exported C functions (the bridge) that graphvex calls to present with
transaction, resize boards, attach panels, and read render generation. The
window never renders, never ticks, and never schedules; it answers the
bridge and gets out of the way.

The No-Transaction-Across-Event-Dispatch Law governs how the bridge's event
pump presents. The window presents nothing on its own — it only publishes
live frames as events; graphvex (R3) consumes them through the
`resizeRenderFn` hook and `GfxLoop_modalTick` seam and presents only when
actually registered (Window is a dumb surface + bridge, never a renderer,
per this law).

---

### Absolute Size and Location Law

#### Definition:
Every element carries TWO rects, and they are never confused. The **declared**
rect — the raw `(x, y, w, h)` the author sets — is intent. The **absolute**
rect — the TRUE size and TRUE location of the element on screen — is a
function of everything that can move or grow it: **scale, padding, margin,
filters, visual effects, rotation, transform**, and the ancestors' layout.
The regular size IS NOT the absolute size; the regular location IS NOT the
absolute location. Layout, hit-testing, culling, and scrolling read the
absolute rect; authoring writes the declared rect; resolution (declared →
absolute) runs eagerly on every geometry setter.

Scale is a first-class citizen of the absolute: an element scaled 2× (or N×)
keeps its **conceptual pixel coordinates** — children, hit-testing, and layout
math do not move — while its absolute footprint absorbs the factor. A scaled
element is bigger on screen and identical in layout space.

Any of `x`, `y`, `w`, `h` may carry `SIZE_AUTO` (the åuto FourCC,
`lang/size.h`): a negative dimension reads as AUTO. AUTO position defers to
the owner's layout (the parent places you); AUTO size derives from content
(a Label resolves font size + padding; a ScrollPanel resolves AUTO content
to its viewport). AUTO is an owner-level protocol — components store raw
values and resolve a zero abs for AUTO dims; owners remember the intent and
re-measure on every render/layout.

#### The Why:
Conflating declared and absolute rects is how scrollbars drift, thumbs lie,
and scaled elements mis-hit: one code path reads the author's numbers while
another paints the transformed truth. A single declared→absolute resolve,
run eagerly and read everywhere, keeps every consumer (paint, hit-test,
scroll geometry, culling) on the same truth. AUTO removes hand-measured
boxes — content *is* the size — while keeping the sentinel debugger-visible
as åuto and the check a single negativity test.

#### The Rule:
1. **Two rects, one resolve.** Declared `(x, y, w, h)` is written by authors;
   absolute `(absX, absY, absW, absH)` is resolved eagerly and read by
   consumers. No consumer reads declared dims for screen truth.
2. **Absolute absorbs everything.** Scale, padding, margin, filters, visual
   effects, rotation, and transform all feed the absolute. Conceptual pixel
   coordinates are invariant under scale.
3. **AUTO on all four lanes.** `x`, `y`, `w`, `h` each accept `SIZE_AUTO`;
   position-AUTO defers to the owner, size-AUTO derives from content.
4. **The sentinel lives in the declared field; the equivalence resolves it.**
   `w == SIZE_AUTO` is the check that swaps in the element's AUTO
   equivalence — the size the class defaults to — stored by the owner via
   `GraphicsComponent_setMeasuredSize` (0 for a dumb element, measured text
   for a Label). The declared sentinel survives, so AUTO re-resolves every
   render. Layout-currency reads (`getWidth`/`getHeight` on the widget
   layer) return the resolved value, never the sentinel.

---

### One Layout Record Law (darling has no layout struct of its own)

#### Definition:
The layout/paint metadata record is **graphvex's `GraphicsComponent`**, one
type, embedded by value as the first member of every widget
(`Panel`, `Container`, `ScrollPanel`, `Label`, fields, ...). The old
`darling/component.c` (40+ functions) is **deleted**; `darling/component.h`
is a burn-down compat shim (`typedef GraphicsComponent Component;` + constant
aliases + the two layout-currency reads `Component_getWidth/Height` that
resolve AUTO before returning) that dies file-by-file as widgets move onto
`GraphicsComponent_*` spellings directly.

#### The Why:
Two layout records with identical fields is how the ecosystem drifted —
darling's record was a field-for-field duplicate of the language's, and every
new language feature (AUTO, measured equivalence, Transform) missed the
duplicate. One record means one resolve path, one AUTO protocol, one truth.

#### The Rule:
1. **One type.** Widget geometry/presentation fields live on graphvex
   `GraphicsComponent` and nowhere else. No darling file re-declares a layout
   struct.
2. **Burn-down alias.** `Component`, `COMPONENT_*` constants, and the
   `Component_getWidth/Height` (resolved) reads remain only inside
   `darling/component.h`; every widget file conversion drops them for
   `GraphicsComponent_*` until the shim is deleted.
3. **"Component" always means the language's tree node** (the element with
   name/type/parent/children) once a file includes `lang/component.h` — the
   shim alias and the tree node are never both live in one file.

---

## 3. Repo-Local Extensions (managed, per the Conflict Triage Law)

;;INTENTION("R4 UI Toolkit: retained-mode presentation; locked root boards; sub-part field segregation; zero runtime allocation in layout passes.")

---

## 4. Readiness Cross-Reference (the Living Feature Readiness Law)

- Feature readiness matrix tracked in [`../../_repositories/.ecosystem/darling-framework.md`](../../_repositories/.ecosystem/darling-framework.md) (rendered as `[[darling-framework]]` wiki page).
