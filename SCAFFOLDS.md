# Darling current-system scaffolds

This maps `repos/.ecosystem/darling.md` into the current remastered framework.
Its older green/yellow percentages are historical intent, not proof that these new files work.

Added 91 opaque class pairs and 35 procedural operation/bridge pairs.
Every new pair is explicitly **draft / incomplete**: zero storage and zero callable API.
Existing files are untouched. No IDs, event opt-ins, constructors returning fake success,
OS backends, independent schedulers or duplicate Color/Window implementations were added.

## Draft source homes

| Component/module | Source pair | Intended responsibility |
| :--- | :--- | :--- |
| `Anim` | `src/anim/anim.c` / `.h` | Owner-driven property-animation player with easing/spring policies; no independent tick thread or duplicated scheduler. |
| `Clipboard bridge` | `src/bridge/clipboard.c` / `.h` | Translate UI selection/copy/paste intent into existing vexspoke io/clipboard.h; do not redeclare or duplicate Clipboard_* native functions. |
| `Font bake bridge` | `src/bridge/font_bridge.c` / `.h` | UI font request/cache binding over borrowed graphvex font resources; no new font baker. |
| `Panel Cocoa bridge` | `src/bridge/panel_bridge.c` / `.h` | UI Panel/Board attachment and lifecycle glue over the existing single Frame presentation seam. |
| `Text ObjC bridge` | `src/bridge/text_bridge.c` / `.h` | Platform-capability routing for text shaping/rasterization supplied by the driver/host, with no native implementation in this draft. |
| `Window bridge` | `src/bridge/window_bridge.c` / `.h` | UI root attachment/resize/input glue over hotcwap Window; never a second Window class or native backend. |
| `Button` | `src/button/button.c` / `.h` | Pressable compound Panel with label/icon/accessory slots, variants, enabled/loading state and keyboard activation intent. |
| `Checkbox` | `src/button/checkbox.c` / `.h` | Checked/indeterminate Panel with label hit region, value notifications and keyboard activation intent. |
| `Switch` | `src/button/switch.c` / `.h` | Boolean-choice Panel with track/thumb composition, change notifications and eventual animated state transitions. |
| `Darling type registry` | `src/c23/darling-type.c` / `.h` | Future single Darling class registry; allocate no identities for unimplemented drafts and never duplicate lower-level registry IDs. |
| `Canvas` | `src/canvas/canvas.c` / `.h` | Coordinate a root content tree and borrowed Surface/Boards; fan dirty branches and keep multiwindow presentation independent. |
| `CodeField` | `src/code/code_field.c` / `.h` | Editable code-text widget with language tokens, line-number gutter, caret/selection and future folding/multicaret policies. |
| `Color helpers` | `src/color/color.c` / `.h` | UI color-format conversion policy using the existing graphvex Color scalar; do not introduce an incompatible second Color typedef. |
| `Select` | `src/combo/select.c` / `.h` | Compound selection widget with trigger/value/content/item roles, keyboard selection and overlay popup/filter policy. |
| `Compositor` | `src/compositor/compositor.c` / `.h` | Order borrowed scene/content/overlay Boards into the single Frame presentation seam, with no second OS window or duplicate rendering loop. |
| `Cursor` | `src/cursor/cursor.c` / `.h` | UI cursor-selection facade; native cursor handles and application remain with the R1 host. |
| `AlertDialog` | `src/dialog/alert_dialog.c` / `.h` | Dialog composition for title, body and confirmation actions, preserving modal close/cancel policy. |
| `ColorDialog` | `src/dialog/color_dialog.c` / `.h` | Dialog composition for a ColorPicker and live color-change/result notifications. |
| `Dialog` | `src/dialog/dialog.c` / `.h` | Modal UI wrapper over an owned/borrowed Frame contract; modal focus/close policies must use the R1 host window seam. |
| `FileDialog` | `src/dialog/file_dialog.c` / `.h` | Dialog composition for path/filter/entry selection; file access is borrowed from vexspoke, never a new filesystem backend. |
| `InputDialog` | `src/dialog/input_dialog.c` / `.h` | Dialog composition for a text prompt, bounded input, submit/cancel results and focus return. |
| `OptionDialog` | `src/dialog/option_dialog.c` / `.h` | Dialog composition for action choices and explicit result/cancellation ownership. |
| `Object3D` | `src/drawable/object_3d.c` / `.h` | UI scene-node wrapper for borrowed mesh/material resources and transform hierarchy; GPU mesh ownership remains in graphvex. |
| `Picture` | `src/drawable/picture.c` / `.h` | Image-display widget with borrowed Image backing, UV crop and contain/cover/stretch policy. |
| `Viewer3D` | `src/drawable/viewer_3d.c` / `.h` | Scene3D viewer composition with orbit camera, lighting controls and Object3D content. |
| `Emoji text support` | `src/emoji/emoji.c` / `.h` | Text fallback/emoji presentation policy for Label and rich-text content; shaping and native font capabilities remain borrowed. |
| `Action events` | `src/event/action.c` / `.h` | Abstract UI command dispatch and activation policy using borrowed handler/context lifetimes. |
| `Event bridge` | `src/event/bridge.c` / `.h` | Translate host event payloads into existing c23/event_invoke dispatch, preserving window-scoped coordinates and ownership. |
| `Document events` | `src/event/document.c` / `.h` | Text mutation notification policy for insertion/removal/replacement over existing document event carriers. |
| `Focus events` | `src/event/focus.c` / `.h` | Focus acquisition/release, traversal, focus-visible and modal trapping policy over the retained tree. |
| `Gesture events` | `src/event/gesture.c` / `.h` | Gesture-recognition policy over borrowed scroll/pinch/pan payloads and owner-driven timing. |
| `Key events` | `src/event/key.c` / `.h` | Focused-target keyboard delivery policy over shared event carriers; no Key struct duplication or blanket widget opt-ins. |
| `Tree events` | `src/event/tree.c` / `.h` | Hierarchy mutation notification policy for attach/detach/dirty changes without duplicating the live tree operations. |
| `Value events` | `src/event/value.c` / `.h` | Value-change notification policy for controls, with explicit delivery/reentrancy contracts to be defined. |
| `HtmlExporter` | `src/export/html_exporter.c` / `.h` | UI-tree-to-HTML serialization policy; source tree is borrowed and output ownership/escaping must be defined before implementation. |
| `FocusRing` | `src/feedback/focus_ring.c` / `.h` | Keyboard-focus indicator Panel tied to focus-visible policy, theme color and explicit offset. |
| `ProgressBar` | `src/feedback/progress_bar.c` / `.h` | Determinate/indeterminate progress Panel with normalized value, fill and owner-driven animation. |
| `Skeleton` | `src/feedback/skeleton.c` / `.h` | Loading-placeholder Panel with shape tokens and eventual owner-driven shimmer. |
| `Spinner` | `src/feedback/spinner.c` / `.h` | Loading-indicator Panel with owner-driven phase and rendering intent; no independent animation thread. |
| `CooldownButton` | `src/game/cooldown_button.c` / `.h` | Button composition with owner-driven cooldown phase, radial progress and hotkey/ready indicators. |
| `DamageNumbers` | `src/game/damage_numbers.c` / `.h` | Pooled floating-text Panel coordinator with bounded combat-text lifetime and owner-driven motion. |
| `DialogBox` | `src/game/dialog_box.c` / `.h` | NPC-dialogue Panel composition with portrait, text reveal and action choices. |
| `GamepadNav` | `src/game/gamepad_nav.c` / `.h` | Spatial focus-navigation policy for D-pad directions, bumper tabs and device glyph requests. |
| `HudBar` | `src/game/hud_bar.c` / `.h` | HUD value-bar Panel with primary fill, delayed ghost drain and damage-feedback intent. |
| `InventoryGrid` | `src/game/inventory_grid.c` / `.h` | GridPanel composition for item slots, stack counts, tier styles, locks and drag/drop intent. |
| `Minimap` | `src/game/minimap.c` / `.h` | Scene/Panel composition for map/radar content, player direction, pings and fog-mask intent. |
| `NodeEditor` | `src/graph/node_editor.c` / `.h` | Retained graph-canvas widget with node/pin/socket models, connection curves and edit/selection intent. |
| `Plot` | `src/graph/plot.c` / `.h` | Retained data-chart widget with borrowed data spans, line/bar/scatter modes, axes and data hit/tooltip intent. |
| `History` | `src/history/history.c` / `.h` | UI command-history model with undo/redo and explicit borrowed command/payload lifetime contracts. |
| `Input` | `src/input/input.c` / `.h` | Single-line text-entry widget with buffer ownership, caret, selection, placeholder, clipboard and submit/change intent. |
| `InputOTP` | `src/input/input_otp.c` / `.h` | Passcode-entry Panel with digit slots, focus advance, paste splitting and bounded code storage. |
| `Knob` | `src/input/knob.c` / `.h` | Rotary range-control Panel with angle math, detents, fine adjustment and value readout. |
| `ScrollBar` | `src/input/scroll_bar.c` / `.h` | Track/thumb Panel composition driven by viewport/content ratios, seek/drag intent and horizontal/vertical orientation. |
| `SearchField` | `src/input/search_field.c` / `.h` | Compound text input with search icon, clear action and optional shortcut badge. |
| `Slider` | `src/input/slider.c` / `.h` | Range-selection Panel with normalization, step snapping, orientation, track/thumb composition and keyboard nudging. |
| `Textarea` | `src/input/textarea.c` / `.h` | Multiline text-entry widget with document mutations, wrapping, line navigation and viewport/caret synchronization. |
| `Avatar` | `src/kit/avatar.c` / `.h` | Photo/initials Panel composition with optional presence badge and image fallback policy. |
| `Badge` | `src/kit/badge.c` / `.h` | Status-badge Panel with semantic tone, size and text/icon slots. |
| `Breadcrumb` | `src/kit/breadcrumb.c` / `.h` | Hierarchical navigation-path Panel with separators, current item and action intent. |
| `Chip` | `src/kit/chip.c` / `.h` | Interactive tag Panel with dismiss action, optional avatar and selection/enabled policy. |
| `Pagination` | `src/kit/pagination.c` / `.h` | Page-navigation Panel with page count/current page, boundaries and previous/next actions. |
| `Pill` | `src/kit/pill.c` / `.h` | Compact pill-shaped status/tag Panel, sharing Badge/theme conventions without duplicating layout machinery. |
| `StatCard` | `src/kit/stat_card.c` / `.h` | KPI CardPanel composition with value, trend and borrowed Plot/sparkline content. |
| `Kbd` | `src/label/kbd.c` / `.h` | Keycap text badge with monospaced typography, key-symbol presentation and theme spacing. |
| `Label` | `src/label/label.c` / `.h` | Text widget with owned/borrowed text contracts, font requests, glyph-position hit testing, selection and clipboard intent. |
| `RichLabel` | `src/label/rich_label.c` / `.h` | Text widget with multiple styled runs, shared selection coordinates and plain-text copy across markup. |
| `Container` | `src/layout/container.c` / `.h` | Layout policy adapter over the existing graphvex Element; anchors, pivots, padding and sizing policies must not introduce a second geometry authority. |
| `ListPanel` | `src/list/list_panel.c` / `.h` | Panel composition with indexed horizontal/vertical stacking, gaps, cross-axis alignment and content sizing. |
| `CommandPalette` | `src/overlay/command_palette.c` / `.h` | Quick-action overlay with query/filter results, command selection and focus restoration. |
| `ContextMenu` | `src/overlay/context_menu.c` / `.h` | Context-triggered Menu composition with point anchoring, action routing and dismissal/focus return. |
| `Menu` | `src/overlay/menu.c` / `.h` | Hierarchical action-menu overlay with shortcuts, item selection and keyboard traversal policy. |
| `OverlayRoot` | `src/overlay/overlay_root.c` / `.h` | Explicit top-level overlay tree that escapes content clips while retaining modal input isolation and one Frame presentation seam. |
| `Popover` | `src/overlay/popover.c` / `.h` | Click-anchored overlay card with light-dismiss, focus return and clipping escape policy. |
| `Toast` | `src/overlay/toast.c` / `.h` | Nonmodal notification Panel with message/action composition and owner-driven dismissal. |
| `ToastStack` | `src/overlay/toast_stack.c` / `.h` | Retained queue/layout coordinator for Toast children, deadlines and removal without owning a timer thread. |
| `Tooltip` | `src/overlay/tooltip.c` / `.h` | Hover-hint overlay with placement/arrow intent and bounded lifetime; timers are borrowed from the owner loop. |
| `Accordion` | `src/panel/accordion.c` / `.h` | Disclosure-group Panel composition with single/multiple expansion policies and eventual animated layout. |
| `CardPanel` | `src/panel/card_panel.c` / `.h` | Compound Panel with header, title, description, body and footer roles; elevation and spacing are properties. |
| `DockPanel` | `src/panel/dock_panel.c` / `.h` | Panel composition with named dock regions and splitters, borrowing Frame geometry rather than owning native windows. |
| `FlexPanel` | `src/panel/flex_panel.c` / `.h` | Panel composition with row/column flow, wrapping, gap, justification, alignment and hug/fill/fixed sizing policies. |
| `GridPanel` | `src/panel/grid_panel.c` / `.h` | Panel composition with rows, columns, independent gaps, cell placement and future spanning policies. |
| `LayeredPanel` | `src/panel/layered_panel.c` / `.h` | Overlapping child Panels with explicit ordering and topmost visible hit routing. |
| `MarkdownPanel` | `src/panel/markdown_panel.c` / `.h` | Markdown block/inline parsing into retained text children, with selection, links and future code/table composition. |
| `MaterialPanel` | `src/panel/material_panel.c` / `.h` | Panel facade for borrowed graphvex material-rendered content; no material shader or GPU ownership belongs here. |
| `RichTextPanel` | `src/panel/rich_text_panel.c` / `.h` | Retained rich-text document Panel with styled runs, wrapping and document-wide selection. |
| `SectionPanel` | `src/panel/section_panel.c` / `.h` | Compound titled header/body Panel with collapsible content and future animated height. |
| `SplitPanel` | `src/panel/split_panel.c` / `.h` | Panel composition with pane ratios, divider interaction and min/max pane constraints; child ownership follows Panel. |
| `SvgPanel` | `src/panel/svg_panel.c` / `.h` | Retained SVG display Panel with path/layout interpretation and borrowed driver rendering resources. |
| `TabPanel` | `src/panel/tab_panel.c` / `.h` | Panel composition with tab strip, active content, close/reorder policies and focus transfer. |
| `TablePanel` | `src/panel/table_panel.c` / `.h` | Multi-column data-grid Panel with sorting, headers, resize and paginated or virtualized rows. |
| `TreePanel` | `src/panel/tree_panel.c` / `.h` | Hierarchical disclosure rows for file/scene trees, selection and large-tree virtualization policies. |
| `VideoPanel` | `src/panel/video_panel.c` / `.h` | Panel facade for decoded media frames; decoder/process supervision and image upload remain with their lower-level owners. |
| `WebPanel` | `src/panel/web_panel.c` / `.h` | Capability-gated native-web-content Panel facade; platform handles are borrowed through a host bridge, not an embedded browser runtime. |
| `ColorPicker` | `src/picker/color_picker.c` / `.h` | Color-selection widget with HSV/hex editing, hue/saturation/value controls and borrowed sample intent. |
| `ColorSwatch` | `src/picker/color_swatch.c` / `.h` | Selectable color-chip Panel with selection/change notifications and shared Color representation. |
| `DatePicker` | `src/picker/date_picker.c` / `.h` | Date-selection widget with calendar model, month navigation and future popup/month-grid composition. |
| `set_alignment` | `src/properties/set_alignment.c` / `.h` | Future content/cross-axis alignment operation; do not conflate it with anchor/pivot. |
| `set_anchor` | `src/properties/set_anchor.c` / `.h` | Parent anchor property routing over existing Element/Panel placement; migrate existing setters here rather than define parallel behavior. |
| `set_background_color` | `src/properties/set_background_color.c` / `.h` | Background paint routing; eventually migrate current setters while preserving explicit window-transparency independence. |
| `set_blur` | `src/properties/set_blur.c` / `.h` | Own-edge/backdrop effect property routing with explicitly separate contracts and capability gates. |
| `set_border` | `src/properties/set_border.c` / `.h` | Border width/color property routing; eventually migrate current widget setters and preserve CPU/GPU paint contracts. |
| `set_enabled` | `src/properties/set_enabled.c` / `.h` | Future control enabled-state operation with input and appearance policy. |
| `set_focus` | `src/properties/set_focus.c` / `.h` | Future focus-state operation over the event/focus coordinator and retained tree. |
| `set_font` | `src/properties/set_font.c` / `.h` | Future typography/font request operation that borrows lower-level font resources. |
| `set_gap` | `src/properties/set_gap.c` / `.h` | Future flow/grid child-gap operation with layout revalidation policy. |
| `set_margin` | `src/properties/set_margin.c` / `.h` | Future additive external-spacing operation; semantics must remain separate from anchor/location and be defined before implementation. |
| `set_padding` | `src/properties/set_padding.c` / `.h` | Future inner-spacing operation; layout contribution and interaction with child sizing must be specified before implementation. |
| `set_pivot` | `src/properties/set_pivot.c` / `.h` | Self-pivot property routing over existing Element/Panel placement; migrate existing setters here rather than define parallel behavior. |
| `set_scroll_offset` | `src/properties/set_scroll_offset.c` / `.h` | Future shared scroll-offset operation preserving ScrollPanel clamp/revalidation semantics. |
| `set_shadow` | `src/properties/set_shadow.c` / `.h` | Shadow offset/blur/color property routing; eventually migrate existing setters and retain layout-versus-paint bounds. |
| `set_sizing_mode` | `src/properties/set_sizing_mode.c` / `.h` | Future fixed/hug/fill layout policy operation with explicit measurement semantics. |
| `set_text` | `src/properties/set_text.c` / `.h` | Future text-content ownership/mutation operation for text widgets; no setters declared until their types/lifetimes exist. |
| `set_theme` | `src/properties/set_theme.c` / `.h` | Future theme-token binding operation for widgets and compound parts. |
| `set_value` | `src/properties/set_value.c` / `.h` | Future typed control-value operation with range/clamp/notification contracts. |
| `set_visible` | `src/properties/set_visible.c` / `.h` | Visibility property routing and eventual revalidation/input consequences; migrate existing setters rather than add competing implementations. |
| `RadioGroup` | `src/radio/radio_group.c` / `.h` | Exclusive option-list Panel with selection, disabled-option policy and eventual arrow-key traversal. |
| `Scene` | `src/scene/scene.c` / `.h` | Common retained scene interface that borrows graphvex Board/Image targets and participates in the Frame revalidation cascade. |
| `Scene2D` | `src/scene/scene_2d.c` / `.h` | Retained 2D scene composition with transforms, ordering and pixel/percentage sizing; GPU execution remains in graphvex. |
| `Scene3D` | `src/scene/scene_3d.c` / `.h` | Retained 3D scene composition with camera, lighting and sizing policies; mesh rendering and resource retirement remain in graphvex. |
| `IconImage` | `src/shape/icon_image.c` / `.h` | Vector/SDF icon-display widget requesting borrowed icon atlas resources and theme-driven sizing/color. |
| `PropertyInspector` | `src/spatial/property_inspector.c` / `.h` | Retained property-editing Panel composition with explicit bindings for geometry, paint and text; edits use shared properties operations. |
| `SpatialCanvas` | `src/spatial/spatial_canvas.c` / `.h` | Retained pan/zoom content-canvas policy with guides, minimap and borrowed driver transforms; no separate render loop. |
| `TransformBox` | `src/spatial/transform_box.c` / `.h` | Interactive bounding-handle Panel with translation/scale/rotation intent, aspect locking and geometry constraints. |
| `TextCore` | `src/text/text_core.c` / `.h` | UI-side styled-run, text-layout and selection model; native glyph shaping/rasterization is a borrowed driver capability. |
| `Typography` | `src/text/typography.c` / `.h` | UI type-role policy mapping caption/body/title/display/mono roles to borrowed font resources and theme tokens. |
| `Theme` | `src/theme/theme.c` / `.h` | UI design tokens for color ramps, spacing, typography and elevation; styles reuse graphvex Color/Property rather than redefining them. |

## Existing and lower-level homes

| Intent | Current home | Boundary |
| :--- | :--- | :--- |
| Frame | `frame/frame` | Window/lifecycle/accessors; shared operations route through properties/. |
| Panel | `panel/panel` | Live wrapper over graphvex Element with existing ownership and paint API. |
| ScrollPanel | `panel/scroll_panel` | Live viewport over content, with clamped offsets and clipping. |
| Pointer events | `input/pointer` | Existing real-input bridge; no replacement dispatcher scaffold. |
| Event dispatch | `c23/event_invoke` | Existing handler registration/bubbling and typed event carriers. |
| c23/add | `properties/add` | Current canonical Frame/Panel attachment operations; no duplicate c23/add implementation. |
| remove/revalidate/size/location/radius | `properties/` | Existing shared operation files, left untouched. |
| Font bake / raster / Surface | `graphvex R3` | Driver-owned capabilities, not recreated as R4 implementations. |
| IO mmap / IO bake / native Clipboard | `vexspoke R2` | Leaf I/O/native clipboard capabilities, not duplicated. |
| Native Window | `hotcwap R1` | Host-owned native window; Frame/bridge only borrow it. |
| Color representation | `graphvex/graphics/graphics.h` | Reuse Color; color/color is a helper-policy placeholder, not another class. |

## Legacy names

Container-era names map to the modern Panel compositions; do not create two competing types.

- `ListContainer` -> `ListPanel`
- `GridContainer` -> `GridPanel`
- `SectionContainer` -> `SectionPanel`
- `LayeredContainer` -> `LayeredPanel`
- `SplitContainer` -> `SplitPanel`
- `ScrollContainer` -> `ScrollPanel`
- `DataTable` -> `TablePanel`
- `Theme System` -> `Theme`

## Implementation order

1. Specify the class contract: fields, ownership/borrow rules, constructor arities, errors and thread affinity.
2. Implement one coherent class pair and its symmetric field accessors; shared operations go in `properties/`.
3. Opt in to only the events the actual widget supports. Allocate class identities only when the type becomes real.
4. Keep Element/Property geometry, R3 graphics and R1 native resource ownership canonical.
5. Add mirrored owner tests and integration pixel/input/lifetime tests in `tests/darling/`.
6. Record automated lab evidence with exact timestamps and stated gaps; the user owns visual approval.
7. Remove draft/incomplete markers only for the scope truly implemented and tested.

## Scaffold checks

`python3 -B tests/darling/scaffold/scaffold_lab_test.py` checks the manifest,
blueprints, absence of callable stub APIs, paired paths, aggregate header compilation
and fresh syntax compilation of every draft implementation.
`./tools/b build darling` discovers and compiles the new `.c` files without launching windows.
These are structure/compilation checks, not component behavior tests.

Authored using the permitted shell fallback because the session lacked direct write/edit tools.
