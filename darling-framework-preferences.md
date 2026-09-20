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
| **Pane-of-Glass Law** | R4 UI Toolkit | Mandatory for `darling-framework` |
| **Sub-Part Field Segregation Law & the `Class_part_verb` Law** | R4 UI Toolkit | Mandatory for `darling-framework` |
| **Living Darling Docs Law (Zero Drift Between Code and `_docs/darling.md`)** | R4 UI Toolkit | Mandatory for `darling-framework` |
| **Panel Gravity Law** | R4 UI Toolkit | Mandatory for `darling-framework` |
| **Window Decoupling Law (a Window is just a Window)** | R4 UI Toolkit | Mandatory for `darling-framework` |

## 2. Exclusive Repo-Local Laws (FULL PROSE RESTATEMENT)

### Window Board Root Lock Law (Dimension Override Law)

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

---

### Pane-of-Glass Law

A scene child renders either as a COMPOSITED layer (retained offscreen render
targets — the `VkLayer` registry in graphvex — collaged into the canvas by the
composite pass; the default, one `CAMetalLayer` total) or a DIRECT pane (its
own `CAMetalLayer` + dedicated per-pane Vulkan swapchain — `VkPane` registry
in graphvex, `PanelCocoa_newMetal` in darling — every panel is a Vulkan rect).
DIRECT is the managed exception (the Conflict Triage Law) for full-window or
latency-locked scenes that must not pay the composite copy. The flight
machinery — registry, stable slot index, dual flight slots, acquire/render
semaphores, bounded 100ms fences, dirty bit — is shared verbatim between both
modes; only the destination differs: a presentable swapchain image (DIRECT)
vs a compositable color image the canvas samples (COMPOSITED). A layer/pane's
pixel size is FIXED at register/resize time —
`VkPane_resize`/`VkLayer_resize` is a no-op when the requested size is
unchanged, so fixed targets never rebuild.

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

Each Metal pane layer gets its `contentsGravity` and
`anchorPoint` set from the panel's `selfAnchor`. This keeps the rendered pixel
content pinned to the correct corner during live window resize (before the next
frame is ready). The mapping is:

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

The `CAMetalLayer` hosting a Vulkan swapchain has `geometryFlipped = YES` so
that Vulkan's top-down coordinate space maps correctly onto CoreAnimation's
bottom-up space. Every pane layer in the stack carries it for the
same reason.

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
transaction, resize panes, attach boards, and read render generation. The
window never renders, never ticks, and never schedules; it answers the
bridge and gets out of the way.

The No-Transaction-Across-Event-Dispatch Law governs how the bridge's event
pump presents. The window presents nothing on its own — it only publishes
live frames as events; graphvex (R3) consumes them through the
`resizeRenderFn` hook and `GfxLoop_modalTick` seam and presents only when
actually registered (Window is a dumb surface + bridge, never a renderer,
per this law).

---

## 3. Repo-Local Extensions (managed, per the Conflict Triage Law)

;;INTENTION("R4 UI Toolkit: retained-mode presentation; locked root boards; sub-part field segregation; zero runtime allocation in layout passes.")

---

## 4. Readiness Cross-Reference (the Living Feature Readiness Law)

- Feature readiness matrix tracked in [`../../_repositories/.ecosystem/darling-framework.md`](../../_repositories/.ecosystem/darling-framework.md) (rendered as `[[darling-framework]]` wiki page).
