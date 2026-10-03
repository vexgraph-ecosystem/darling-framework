# Hover cursor properties

`properties/set_cursor.{c,h}` owns class-named cursor setters. Widget files
do not contain repeated cursor implementations.

```c
Panel_setCursor(buttonPanel, CURSOR_POINTER);
ScrollPanel_setCursor(viewport, CURSOR_ARROW);
Frame_setCursor(frame, CURSOR_CROSSHAIR);
Element_setCursor(content, CURSOR_TEXT);
```

The preference is stored on the individual Element, not its pooled paint bound.
Children inherit by default. The deepest visible hit with an explicit preference
wins; `CURSOR_ARROW` overrides inherited preferences and `CURSOR_INHERIT` removes
an override. Rounded masks and scroll viewport clipping also constrain hits.
A miss resolves to arrow.

Frames automatically attach the existing pointer bridge. Native move/drag
callbacks dispatch handlers, resolve the resulting tree, and apply the native
cursor through `Window_setCursorType`. Setters configure the next motion update;
they do not immediately change a global cursor while the pointer is stationary.
No additional event registration or cursor registry is needed, and preference
storage disappears with its Element.

An implemented class opts in with `DECLARE_CURSOR(Class)` in its header and
`IMPLEMENT_CURSOR(Class)` in the shared property implementation, using its
`Class_graphics` accessor. `IMPLEMENT_CURSOR_WITH(Class, accessor)` handles
different accessor names, such as `Frame_element`.

Textarea, Graph and other draft-only widgets have no runtime Element accessor
yet. They do not receive fake setters; once implemented they get their own
class-named setters, never calls through `Panel_setCursor` on another type.

## Evidence and limits

`set_cursor_test` is headless automated evidence for selection, inheritance,
overrides, clipping, invalid/null input, macro-generated class APIs and lifetime
independence. Compilation checks cover native bridge wiring. Native cursor
appearance, window-entry/exit behavior and locked-pointer/drag-capture cursor
policy are not verified by this test. Appearance acceptance remains user-owned.
