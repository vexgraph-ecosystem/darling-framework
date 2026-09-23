# Darling UI Catalog & Theme System

> Retained-mode, off-heap UI for C23. Every node embeds `Container` as its first member (`&(*node).base` upgrades to layout), extends `Panel` for background + tree, and exposes symmetric `set/get` pairs (the Symmetric Getter/Setter Completeness Law). Containers are detach-only: `add/insert` attach, `remove` detaches, never frees.

This document elaborates **every widget darling has today + every widget proposed** for everyday / SaaS / game use, plus the **theme system** that makes them all look polished.

Related sources:
- `../../projects/darling/darling/` — nodes (`button/`, `field/`, `label/`, `panel/`, `dialog/`, `overlay/`, `picture/`, `plot/`, `scene/`)
- `../../projects/darling/darling/container.h` — anchors, percent, margin, radius, z, clip
- `../../projects/darling/darling/panel/panel.h` — bg color, render-handler slot, tree
- `../../trash/darling_gallery.c` — widget gallery + suite (S1–S7)
- `main/darlingtest.c` — two-thread decoupled compositor demo

---

## Table of Contents

1. [Theme System](#1-theme-system)
2. [Base & Substrate](#2-base--substrate)
3. [Panels / Containers (today)](#3-panels--containers-today)
4. [Buttons & Choice (today)](#4-buttons--choice-today)
5. [Fields & Inputs (today)](#5-fields--inputs-today)
6. [Labels & Text (today)](#6-labels--text-today)
7. [Dialogs & Overlays (today)](#7-dialogs--overlays-today)
8. [Media & Data (today)](#8-media--data-today)
9. [Polish Substrate (proposed P0)](#9-polish-substrate-proposed-p0)
10. [More Containers (proposed P1)](#10-more-containers-proposed-p1)
11. [Everyday Widgets (proposed)](#11-everyday-widgets-proposed)
12. [SaaS Kit (proposed)](#12-saas-kit-proposed)
13. [Game Kit (proposed)](#13-game-kit-proposed)
14. [What to Build First](#14-what-to-build-first)

---

## 1. Theme System

### 1.1 Why a theme, not per-widget colors

Today every widget takes raw `uint32_t 0xAARRGGBB` colors (`Panel_setBackgroundColor`, `Button_setBackground`, `Label_setTextColor`). That is flexible but unpolished: dark mode becomes a hunt-every-callsite override, SaaS cards and game HUDs drift apart, contrast breaks.

A theme centralizes **design tokens** so widgets ask "what role am I?" instead of "what hex am I?":

```c
Theme *th = Theme_dark();       // or Theme_light(), Theme_zenith(), Theme_m3(...)
Theme_apply(th);                // sets the process-global token table
// widgets resolve roles internally:
uint32_t bg = Theme_color(THEME_SURFACE);
uint32_t fg = Theme_color(THEME_ON_SURFACE);
```

Manual overrides still work — `Panel_setBackgroundColor` always wins over the token. Theme is the default, not a lock.

### 1.2 Token roles

| Group | Tokens | Used by |
|-------|--------|---------|
| Surfaces | `SURFACE, SURFACE_DIM, SURFACE_CONTAINER, SURFACE_CONTAINER_HI` | Panel, Card, Sidebar, Dialog bg |
| Text | `ON_SURFACE, ON_SURFACE_DIM, ON_SURFACE_FAINT` | Label, Input text, Kbd |
| Brand | `PRIMARY, ON_PRIMARY, PRIMARY_CONTAINER` | Button bg, active Tab, Slider fill, links |
| Status | `SUCCESS, WARNING, DANGER, INFO + ON_*` | Badge, Toast, HudBar, StatCard delta |
| Lines | `OUTLINE, OUTLINE_DIM, FOCUS_RING` | borders, dividers, focus |
| Overlay | `SCRIM, GLASS_TINT, GLASS_BLUR` | Modal scrim, frosted Card, Tooltip |

Plus numeric scales:

```c
Theme_setRadiusScale(th, 6.0f, 10.0f, 16.0f);   // sm / md / lg
Theme_setElevation(th, 0, 2, 6, 16);            // shadow radii per level
Theme_setFontScale(th, 12, 14, 18, 22, 32);     // caption/body/title/display
Theme_setSpacing(th, 4, 8, 12, 16, 24);         // single spacing source
Theme_setMotion(th, 120, 200, 320);             // fast/med/slow ms + easing
```

### 1.3 Built-in themes (why so many?)

| Theme | Look | When to use it |
|-------|------|----------------|
| `Theme_dark()` | Slate `#0F172A` surface, sky accent `#7DD3FC` | Default. Matches `darling_gallery.c` gallery today. Games + devtools. |
| `Theme_light()` | White/`#F8FAFC`, slate text, blue primary | Docs, daytime SaaS, settings pages |
| `Theme_zenith()` | Pure achromatic (black/white/gray only, no hue) | 2026 monochrome SaaS trend. Finance, legal. Tests that your UI works without color. |
| `Theme_m3(dynamic)` | Material 3 dynamic-color from wallpaper/seed | Companion/launcher apps mirroring system wallpaper. `Theme_m3FromSeed(0xFF6750A4)` |
| `Theme_highContrast()` | WCAG AAA: max fg/bg distance, 3px focus ring | A11y cert, XR legibility pass, 720p trailer test |
| `Theme_gameNeon()` | Deep navy + amber/cyan glow, chunky radius | HUD prototype without hiring an artist (Kenney-wavelength) |
| `Theme_retro()` | Cream paper + ink + red accent | Game menus, patch notes, quest log flavor |

You only ever ship 1–2. The rest are one-line swaps for testing:

```c
Theme *t = Theme_dark();
Theme_setAccent(t, 0xFFA78BFAu);  // rebrand without touching widgets
Theme_apply(t);
```

### 1.4 Dark mode done right

Not a CSS override. Each role has both ramps baked in:

```c
typedef struct Theme {
    uint32_t light[THEME_COUNT];
    uint32_t dark[THEME_COUNT];
    int mode; // THEME_MODE_LIGHT / DARK / SYSTEM
} Theme;
uint32_t Theme_color(int role); // resolves via mode at paint time
```

`Window` reports `SYSTEM` changes (macOS `effectiveAppearance`) → `Theme_systemChanged()` re-resolves, marks dirty, no caller code. Contrast is verified at theme-build time: `Theme_audit(th)` fails the suite if any `ON_*` pair drops below 4.5:1 (body) or 3:1 (large/display).

### 1.5 Typography + shape + motion

```c
// Typography.h side:
Label_setTextRole(l, TEXT_BODY);   // instead of setFontSize(14) everywhere
Label_setTextRole(l, TEXT_TITLE);  // 18 semibold, tracking -0.5
Kbd / Typography resolve font + size + weight + tracking from theme.

// Shape:
Panel_setRadiusPreset(p, RADIUS_MD); // follows theme scale, not magic 10.0f
Panel_setRadiusMode(p, CORNER_SUPERELLIPSE); // iOS-smooth cards

// Motion:
Container_setTransition(c, 200, EASE_OUT_CUBIC); // hover/press/open animate
Dialog_setEnterAnimation(d, DIALOG_FADE_SCALE);
Toast_setLifetime(t, 4000);
```

---

## 2. Base & Substrate

### `Container` — layout truth
Position, size, scale, two-anchor system (self 4-corner + parent 9-grid), pivot, percent, z, visible/enabled/dirty/clip, min/max, additive margin (`final = location + margin`), corner radius + mode. `Container_resolve()` produces the screen rect; `hitTest()` gates on visibility. The "subtle law": state edits (color/hover) never recapture base or anchored panels jump on resize.

### `Panel` — visual + tree node
`Container base + color + filters + image + renderHandler + source + parent + children`. `renderHandler == NULL` means solid quad; otherwise the handler records into the open pass (Vulkan cmd or software `Raster`). Views (`Panel_add`) deep-copy structure but alias payloads — dirty fans out via parent-ref set. Every widget below embeds this, so `Panel_setLocation/Size/Margin/Radius/Visible` work uniformly.

### `Canvas` / `Scene`
`Canvas` is the root surface owning dirty-layout propagation. `Scene` is a `Panel` with a pixel/percent mode flag — the compositor stamps `Surface`s at resolved rects (`darlingtest.c` sky/tri/ui pattern). Game 3D views and SaaS charts both live here.

---

## 3. Panels / Containers (today)

### `ListPanel` — indexed stack
Vertical/horizontal ordered rows; index IS the API (`add/insert/get/remove/count/layout`). SaaS chat, settings rows, inventory list. Gallery S5 uses it for chat bubbles with `setSpacing(8)` + child radius/margin. Detach-only.
```c
ListPanel *chat = ListPanel_0();
ListPanel_setSpacing(chat, 8.0f);
ListPanel_add(chat, bubble); // bubble = Panel with radius 12 + Label inside
```

### `GridPanel` — excel core
`GridPanel_2(rows, cols)` + `setCell(r,c,panel)` / `getCell` + `setGap(gx,gy)`. Gallery S6 builds a 3×3 with diagonal highlight. Future home of inventory grids, calendars, DataTable bodies.
```c
GridPanel *g = GridPanel_2(3,3);
GridPanel_setGap(g, 6, 6);
GridPanel_setCell(g, 1, 1, cell);
```

### `ScrollPanel` — viewport + content + offsets
`ScrollPanel_2(vw,vh)` + `setContent(panel)` + `setOffset/getOffset` with end-clamp. Gallery root: full-window `ScrollPanel` over the `ListPanel` canvas, trackpad `onMouseScroll` feeds offsets with natural-scroll signs. Every SaaS page and game log needs one.

### `SectionPanel` / `LayeredPanel`
`SectionPanel`: titled group (header Label + body) — settings pages, form sections. `LayeredPanel`: z-stacked siblings — HUD over minimap, badge over avatar, modal over page.

### `MarkdownPanel` / `RichTextPanel`
`MarkdownPanel_1("# Hi\n\nHello *world*")` scans to row Labels (gallery S2 + suite `getRowCount > 0`). Patch notes, docs, quest text. `RichTextPanel` is the styled-run sibling (bold/italic/code/links via `text/rich_text.c`).

---

## 4. Buttons & Choice (today)

### `Button` — pressable shell
Panel + owned label + font/size + `bg/bgHover/bgPressed/borderColor/radius/borderWidth` + `disabled/hovered/pressed` + `onPress(ctx)`. Gallery S3: blue 140×36 radius-8 button. Polish gap: needs focus-ring + theme roles (`PRIMARY/ON_PRIMARY`) + motion.
```c
Button *ok = Button_1("Press me");
Button_setOnPress(ok, onOk, ctx);
Panel_setBackgroundColor(&(*ok).base, 0xFF2563EBu);
```

### `Switch` — on/off toggle
Track + thumb, `setOn/isOn`. Settings (dark mode, notifications), game options. Animate thumb with `Container_setTransition`.

### `Checkbox` — square check
`setChecked/isChecked`. Multi-select, filters, todo. Pair with Label row for hit area.

### `RadioGroup` — exclusive choice
Owned option list + `selected` index + `onSelect`. Payment tier, difficulty, plan picker. Renders as stacked radio rows; keyboard arrows move selection.

---

## 5. Fields & Inputs (today)

### `Input` — single-line editor
Owned bounded buffer (`text/cap/placeholder/password/readonly/cursor/font/onChange/onSubmit/ctx`). Gallery S4 `setText("type here...")`. Needs: caret walker, selection, clear button, leading icon slot (see `SearchField` proposal).

### `Textarea` — multi-line editor
Same buffer, wrapped. Feedback form, chat compose, code notes. Lives in ScrollPanel when long.

### `InputOTP` — one-time code boxes
N-box code entry with auto-advance + paste-split. Auth, 2FA, device pairing. `setLength(6)`, `getCode()`.

### `Slider` — 0..1 (or ranged) drag
`setValue/getValue` + track/fill/thumb colors (suite checks `0.5f` round-trip). Volume, brightness, price filter. Keyboard: arrows ±step.

### `Knob` — rotary dial
Angle-mapped `Slider` sibling. Synth cutoff, game settings, car HUD. Drag vertical or circular.

### `Scrollbar` — two modes
`SCROLL_BAR_GESTURE` (trackpad-style, fades) vs `SCROLL_BAR_POINT` (precise thumb, gallery S7 side-by-side). `setRange/setValue` with clamp (suite: `9.0 → 1.0`, `-9.0 → 0.0`).

### `Select` — dropdown
Owned item list + `selected/open/filter/onSelect`. Country, plan, resolution. Filter string makes it a combo box; popup renders in `OverlayRoot` (proposed) so it escapes clipping.

### `DatePicker` — calendar popup + field
`setDate/getDate`, min/max, locale. Booking, billing, quest reset. Grid body reuses `GridPanel`.

### `ColorPicker` / `ColorSwatch`
`ColorSwatch`: single chip (`setColor`). `ColorPicker`: hue ring + sat/val quad + alpha + hex Input + recent row. Theme editor, avatar creator, level tint.

---

## 6. Labels & Text (today)

### `Label` — plain text view
CoreText cached quads (sharp path) + SDF fallback (GPU JFA for glow/outline/scale). `setText/TextColor/FontSize/BackgroundColor`. Gallery S1 shows plain / tinted-22pt / on-tint-block. Body of everything.

### `RichLabel` — styled runs
Multi-run single block (bold/link/code spans). Superset of Label for prices, chat names, error text with inline action.

### `Typography` — scale tokens
`TEXT_CAPTION/BODY/TITLE/DISPLAY + MONO` mapping to font/size/weight/tracking from theme. Replaces magic `setFontSize(18)` with `setTextRole(TITLE)`.

### `Kbd` — keycap hint
`<kbd>⌘K</kbd>` chip. Command palette rows, settings shortcuts, game bindings. `Kbd_1("⌘K")`, theme outline + mono font.

---

## 7. Dialogs & Overlays (today)

All are struct-complete, behavior-pending on the proposed `OverlayRoot/ModalRoot`. That is intentional: build the root once, all four come alive.

### `Dialog` — modal family root
`title + content(borrowed) + modal + onClose`. `AlertDialog` (title/message/buttons) and `ColorDialog` embed it like subclasses. `modal=true` blocks sibling input via focus capture.

### `AlertDialog` — confirm/deny
`setMessage`, `addButton("Delete", DESTRUCTIVE, cb)`. Unsaved changes, destructive SaaS actions, quit-to-menu.

### `FileDialog` — modal browser
`path[512]/filter[64]/showHidden/entries/selected/onOpen/onCancel`. `refresh()` scans via `io/`, `choose(i)` confirms. Save/open level, export CSV, attach file.

### `ColorDialog` — picker in a modal
`Dialog` chrome + `ColorPicker` body. "Change theme accent" without leaving settings.

---

## 8. Media & Data (today)

### `Picture` — image node
Off-heap image + UV crop + aspect fit/fill. Avatars, thumbnails, minimap base, item icons. `Picture_setSource(path)`, `setFit(FIT_COVER)`, shared payload via `Panel image` slot.

### `Plot` — data-first chart
Borrowed `xs/ys` float `Buffer`s (never owned) + `count + autoRange + min/max + line/fill/grid/axis colors + title/xlabel/ylabel + showGrid/Axes/Legend + onSelect`. Kinds: `LINE/BAR/SCATTER/HIST/AREA`. SaaS revenue, game DPS graph. Proposed extension adds `DONUT/SPARK/HEAT` + tooltip picking (`hitPick` exists as stub).
```c
Plot *p = Plot_0();
Plot_setKind(p, PLOT_LINE);
Plot_setData(p, xs, ys, n);
Plot_setTitle(p, "MRR");
```

---

## 9. Polish Substrate (proposed P0)

Do these before any new widget — they make old widgets look new.

| Node / API | What it is | C sketch |
|------------|-----------|----------|
| `Theme` | Token table + light/dark ramps + radius/elevation/type/spacing/motion (#1) | `Theme_apply(Theme_dark())` |
| `Elevation/Border/Blur` | `Panel_setElevation(p,2)`, `setBorder(p,color,w)`, `setBlur(p,12)` | Frosted card = `SURFACE @ 210 alpha + blur 16 + border OUTLINE_DIM` |
| `ProgressBar` | Determinate bar + indeterminate sweep | `ProgressBar_setValue(b,0.65)` — uploads, health regen, onboarding |
| `Spinner` | Infinite arc, theme accent | `Spinner_0()` — loading table, matchmaking |
| `Skeleton` | Shimmer placeholder rect/rows | `Skeleton_1(SKELETON_ROWS)` — table/image loading state |
| `Toast + ToastStack` | Non-modal queue, auto-dismiss, action | `ToastStack_push(stack,"Saved",TOAST_SUCCESS)` |
| `FocusRing` | Theme `FOCUS_RING` outline on `:focus-visible` | Tab through SaaS form / gamepad bumper through menu |
| `Motion` | `Container_setTransition(ms,easing)` on layout + color + open | 120 ms hover, 200 ms open, 320 ms page |

---

## 10. More Containers (proposed P1)

| Container | Layout law | Replaces / enables |
|-----------|-----------|--------------------|
| `FlexPanel` | `direction ROW/COL + wrap + gap + justify/align + grow/shrink` | Manual `ListPanel` math; SaaS toolbars, chip rows, responsive cards |
| `SplitPanel` | Two children + draggable divider + `ratio` + min sizes | Editor (file tree + viewport), mail (list + reader), diff |
| `TabPanel` | Tab strip + body stack, `addTab/removeTab/select`, closable | Settings sections, editor files, character sheets |
| `DockPanel` | `dock(left/right/top/bottom/center)` regions + splitters | IDE / level editor; ImGui `DockSpace` equivalent, retained |
| `CardPanel` | `header/body/footer` slots + elevation + radius preset | SaaS stat card, pricing, game item card |
| `Toolbar` | Leading/title/actions + overflow `...` menu | App top bars, editor tool strips |
| `Sidebar` | Collapsible icon rail + sections + badges | SaaS nav, game lobby nav, tool palette |
| `AppShell` | `sidebar + topbar + content + toasts + modal` in one | Every SaaS page; one `AppShell_0()` boots the shell |
| `OverlayRoot` | Full-window layer escaping clip; hosts popups/modals/toasts | Makes `Select` popup, `Tooltip`, `Dialog` actually work |
| `TreePanel` | Indented expandable rows, `setExpanded/onToggle` | File tree, org chart, skill tree, JSON viewer |
| `TablePanel` | Columns + rows + header + row select + column resize | Precursor to `DataTable`; inventory, logs |

```c
FlexPanel *row = FlexPanel_1(FLEX_ROW);
FlexPanel_setGap(row, 8);
FlexPanel_add(row, avatar, FLEX_FIXED);
FlexPanel_add(row, nameLabel, FLEX_GROW);
TabPanel_addTab(tabs, "General", generalPage);
SplitPanel_setRatio(split, 0.28f);
```

---

## 11. Everyday Widgets (proposed)

| Widget | Behavior | Example |
|--------|----------|---------|
| `Tooltip` | Hover/focus delay, arrow, `setText`, escapes clip via OverlayRoot | Icon-button hints, chart point values |
| `Popover` | Click-anchored card, light-dismiss, focus trap-lite | Filter builder, color quick-pick, help |
| `Menu` / `Dropdown` | Item list + icons + shortcuts + separators + submenu | `...` row actions, right-click canvas |
| `Breadcrumb` | Path segments + separators + overflow | Files, settings depth, docs |
| `Pagination` | Page numbers + prev/next + page-size | Table footer, search results |
| `Accordion` | Expand/collapse sections + animated height | FAQ, settings groups, patch notes |
| `Carousel` | Paged swipe/strip + dots + autoplay | Onboarding, screenshots, store |
| `Drawer` / `BottomSheet` | Edge slide-in + scrim + swipe-dismiss | Mobile nav, filters, now-playing |
| `Avatar` | Initials/photo + presence dot + size scale | Assignees, players, comments |
| `Badge` / `Pill` | Status dot + label, `SUCCESS/WARN/DANGER/INFO` tones | Plan tier, build status, rarity |
| `Chip` | Removable token + icon | Tags, recipients, filters |
| `SearchField` | Input + leading icon + clear × + `onSubmit` + shortcut hint | Cmd+K input, table filter, inventory search |
| `Form` | Label + field + hint + error row + required mark | Every settings/create/edit flow |
| `Stepper` / `Wizard` | Numbered steps + back/next + validation gate | Checkout, onboarding, character creator |
| `Timeline` | Dots + rails + timestamps | Activity feed, deploy history, quest chain |
| `Calendar` | Month grid (on GridPanel) + range select + events dots | Booking, sprints, cooldown calendar |

---

## 12. SaaS Kit (proposed)

Composed from #9–11. Nothing exotic — just the exact patterns shadcn dashboards repeat in 2026.

| Kit piece | Composition | Notes |
|-----------|-------------|-------|
| `StatCard` | `CardPanel + delta Pill + spark Plot + title/value` | MRR/ARR/churn/CAC/LTV. `StatCard_setDelta(c,"+12.4%",UP)` |
| `DataTable` | `TablePanel + sort + filter + pagination + selection + visibility + saved views` | Client-side to ~5k rows (TanStack rule), then server mode: `setPageFetcher(cb)`. Notion-style filters + Cmd+K live here. |
| `CommandPalette` | `OverlayRoot + SearchField + grouped ListPanel + Kbd hints + fuzzy filter` | `Palette_registerAction("Go to billing", cb)`. Power-user navigation. |
| `KanbanBoard` | N `ListPanel` columns + draggable `CardPanel`s | Tasks, hiring, support. Drag lands via gesture events. |
| `SettingsPage` | `Sidebar tabs + Form sections + save bar + Toast` | Billing/profile/team/API keys scaffolds. |
| `Plot extras` | `PLOT_DONUT/SPARK/HEAT + tooltip + empty state` | Donut for share, spark inside StatCard, heat for activity. Empty state matters more than the chart. |

DataTable column sketch (darling-idiomatic):

```c
DataTable *t = DataTable_0();
DataTable_addColumn(t, "Customer", COL_TEXT, 220);
DataTable_addColumn(t, "Plan", COL_PILL, 120);
DataTable_addColumn(t, "MRR", COL_MONEY, 110);
DataTable_setRows(t, rows, n);
DataTable_setSortable(t, true);
DataTable_setRowActions(t, actions); // ... menu per row
```

---

## 13. Game Kit (proposed)

| Kit piece | Behavior | Notes |
|-----------|----------|-------|
| `HudBar` | `value/max + ghost lag bar + flash on damage/heal + low-pulse` | Health/mana/XP/stamina. `HudBar_hit(b, 18)` animates ghost drain like MMORPG HUDs. |
| `CooldownButton` | `Button + radial/linear sweep + keybind label + dim while on cooldown` | Skill bar. `CooldownButton_fire(b, 8.0)` starts 8 s sweep; `isReady()`. |
| `InventoryGrid` | `GridPanel + Slot{icon,count,rarity,equipped} + drag + SearchField filter` | Reuses S6 gallery pattern + `Picture` icons + `Badge` rarity. |
| `SkillBar` | Row of `CooldownButton` + bind numbers + pagination | WoW/MMORPG bar. Gamepad bumpers page it. |
| `Minimap` | `Picture + player cone + pings + fog + zoom` | `Minimap_ping(m, x, y, ALLY)` |
| `DialogBox` | Typewriter `Label` + portrait `Picture` + choice `Buttons` + advance input | NPC/quest/visual-novel. `DialogBox_play(d, lines)` |
| `QuestLog` / `ObjectiveTracker` | Pinned objectives HUD + full log (TreePanel) | "0/8 boars" tracker + journal |
| `ChatBox` | `ListPanel` bubbles (S5 pattern) + channels + `Input` +© mute/block | Reuses chat bubbles verbatim |
| `Leaderboard` | `DataTable` preset: rank/name/score + highlight self + season tabs | PvP, speedrun |
| `VirtualStick` + `TouchButton` | Analog nub + deadzone + pressure buttons | Mobile twin-stick; reports via gesture events |
| `DamageNumbers` | Floating pooled Labels + crit scale + fade/rise motion | Pure retained Labels with transitions |
| `NodeEditor` | `Node{titlebar+pins} + Link curves + pan/zoom + selection rect` | ImNodes-equivalent for blueprints/dialogue trees/quests. Biggest tool win. |
| `SettingsMenu` | `TabPanel(Graphics/Audio/Input/A11y) + Slider/Select/Switch rows` | Pause menu standard |

HUD sketch:

```c
HudBar *hp = HudBar_0();
HudBar_setMax(hp, 100);
HudBar_setValue(hp, 72);          // ghost animates from old value
HudBar_setKind(hp, HUD_HEALTH);
CooldownButton *q = CooldownButton_1("Fireball");
CooldownButton_setBind(q, "Q");
CooldownButton_fire(q, 6.5f);     // sweep starts
```

---

## 14. What to Build First

Suggested order — each unlocks the next, no dead ends:

1. **P0 polish:** `Theme + Elevation/Border/Blur + FocusRing + Motion + Spinner/Progress/Skeleton + ToastStack + OverlayRoot`. Old gallery looks new overnight; dialogs become implementable.
2. **P1 containers:** `FlexPanel + CardPanel + TabPanel + SplitPanel + TreePanel + TablePanel + AppShell(Sidebar+Toolbar)`. SaaS pages and editor shells compose from these.
3. **SaaS pack:** `StatCard + DataTable + CommandPalette + SearchField + Form`. Shippable dashboard.
4. **Game pack:** `HudBar + CooldownButton + InventoryGrid + DialogBox + Minimap + ChatBox`. Shippable HUD + menus.
5. **Tools:** `NodeEditor + DockPanel + Menu + Keybindings`. Editor/blueprint payoff.

If you want, say the word and I start at P0 with `darling/theme/theme.h/.c` + `Toast/Spinner/Progress` in darling style (constructors + symmetric getters/setters + `;;OVERVIEW` headers), wired into `../../trash/darling_gallery.c` S8.

## Indexed CodeField migration

🟧 Functional foundation: indexed variable-height rows, optional numbering, borrowed documentation panels, row actions, selection/editing and two-axis scrolling. Built against Graphics; no retired Vulkan/Texture/Textarea dependency. Proof: `tests/codefield_test.c` and `codefield_demo`. API and remaining work: [`_docs/darling.md`](_docs/darling.md).
