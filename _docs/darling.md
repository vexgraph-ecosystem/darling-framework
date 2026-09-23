# Darling: indexed CodeField

This repository-local blueprint documents the migrated CodeField. The umbrella's
historical full catalog remains at `../../_docs/darling/darling.md`.

## Contract and ownership

`CodeField` lives in `darling/field/codefield.h` and `.c`, built into `darling`.
It embeds a base Panel plus numberPanel and textPanel. They share one paint target,
not independent native layers. The complete field and function registry is mirrored
in the implementation's `;;OVERVIEW`.

CodeField owns a doubling arena row array and copied text. A CodeFieldRow is a
behaviorless slot with text, borrowed content, declared height, resolved top/extent,
derived number, includeLineNumber, button function and borrowed params. Structural
edits invalidate row indices and borrowed text pointers. Custom content panels and
callback contexts outlive their attachment and are detached without freeing or
reparenting. Freeing the field first detaches it from its parent.

| Coordinate | Meaning |
|---|---|
| Row index | Zero-based slot, includes documentation and unnumbered rows |
| Line number | One-based count of included rows; zero when excluded |
| Selection | Anchor and active `(row index, ASCII byte offset)` |
| Screen position | Native pixels in resolved absolute bounds |
| Source-file position | Not inferred; a future document mapping owns it |

`CodeField_setIncludeLineNumber(self, index, flag)` re-numbers without changing row
height or index. `setRowHeight` accepts a concrete minimum or AUTO; AUTO custom
panels measure their absolute height, while text uses the configured row minimum.
Text glyph height plus padding establishes a minimum to avoid overlapping rows.
Scale changes preserve declared row height and scale the resolved footprint.
AUTO field dimensions measure content; explicit field dimensions bound scrolling.

## Public surface

- `CodeField()`, `CodeField(parent)`, `CodeField(parent, text)`, `CodeField_zero()`.
- `add`, `insertRow`, `removeRow`, `setRowText`, `setRowContent`, `setRowHeight`.
- `setText` splits explicit newlines into rows, including trailing empty rows.
- `insertText` replaces selection, splitting/joining rows atomically. It preserves
  the first row's decoration and gives new rows default numbered/AUTO metadata.
  Replacement intersecting a borrowed content panel returns false unchanged.
- `deleteBackward`, `deleteForward`, `copySelection` (bounded; truncation flagged).
- `selection_setRange`, `selection_getRange`, `selection_selectAll`, color accessors.
- `scrollVertical`, `scrollHorizontal`, `scrollToRow`, `scroll_setOffset/getOffset`,
  `scroll_getExtent`, `scroll_setBarSize/getBarSize`.
- `button_setFunction(self, index, fn, params)` and
  `button_getFunction(self, index, destParams)` bind/read without executing.
  `button_invoke` calls `(params, index, lineNumber)`; unnumbered rows receive zero.
- `text_*` controls color, minimum row height and bitmap glyph scale.
- `gutter_*` controls minimum number width, button width and colors. Width grows
  with the total numbered-row digit count. Numbers align right, buttons stay beside them.
- `text_getPanel` and `gutter_getPanel` expose borrowed embedded visual parts.
- `setReadOnly/isReadOnly`, `setFocused/isFocused`, `setOnChange/getOnChange`,
  `setCtx/getCtx`, `toString`, `toStringStruct`.

Header declarations specify exact signatures; setters/getters are symmetric for
editable state. Counts, capacity, measured extents and displayed numbers are derived.
All cold invalid inputs return false or leave state unchanged. Text/row allocation
failure preserves existing content. Read-only blocks interactive edits, not explicit
programmatic setters. Notifications run on the owner thread; callbacks may inspect
but must not destroy or structurally mutate the sender before the enclosing call returns.

## Rendering and input

The paint order is outer background, text-panel background, selection, text/caret,
scrollbars, and gutter. Visible rows are clipped; text and gutter use separate clips.
The incoming Graphics scissor is intersected and restored. Painting allocates zero
memory and no document-sized image. Both sides use the same vertical offset; only
text uses horizontal offset. Scrollbars currently reserve bottom/right tracks even
when no overflow exists. Track presses/drags reposition their offsets.

`pointer` accepts native-pixel coordinates; press places a caret or invokes a button,
drag extends selection. `handleKey` handles arrows, Home/End, Backspace, Delete,
Enter and Tab; Shift-style extension is explicit. The host routes committed text to
`insertText`, focus to `setFocused`, and wheel deltas to the scrolling functions.
Editing is a cold owner-thread transaction, separate from paint/layout.

## Verified status and limits

| Feature | Status | Proof |
|---|---|---|
| Indexed CodeField foundation | 🟧 partial, functional | `_tests/darling/codefield_test.c`; `codefield_demo` |
| Text scissor/opacity and nested clip restoration | Functional | `_tests/graphvex/text_clip_test.c` |

The canonical ecosystem wiki checkout is absent in this workspace. This local row
records proof without claiming an unavailable wiki was updated.

The current Graphvex font is bitmap ASCII. Printable ASCII and tabs are accepted;
unsupported bytes are rejected atomically. This is not Unicode shaping, IME,
syntax highlighting, Markdown parsing, source mapping, undo/redo, or LSP support.
Nested CodeField content is rejected to prevent recursive field painting. Custom panels are visual content; their internal pointer/focus routing is not supplied
by this field. Selection can cross their row, but text replacement cannot delete them.
The legacy parked Textarea/Vulkan wrapper has been replaced; its getEditor/getText
surface is not retained. Use row text getters and bounded selection copying.

## Build and demonstrate

From the umbrella root:

```sh
cmake -S . -B build
cmake --build build --target codefield_test text_clip_test codefield_demo
./build/ecosystem/darling-framework/codefield_test
./build/ecosystem/graphvex/text_clip_test
./build/ecosystem/darling-framework/codefield_demo
```

The macOS demo is a segregated test host, not a second production window owner.
It exercises the actual Graphics raster row; the current build has no GPU Graphics
row for this widget. Pass an absolute PNG path to render a reproducible offscreen
preview. Test sources live in each repo's `tests/` for standalone use and are linked
from umbrella `_tests/<subsystem>/` paths.
