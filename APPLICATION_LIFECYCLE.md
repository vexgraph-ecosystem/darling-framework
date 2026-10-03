# Application lifecycle and Frame presentation

**Implemented contract, 2026-10-04; scoped macOS lab evidence only.**
Every Darling C test/executable now enters Application lifetime, either directly
or through `tests/darling/test_application.h`. Manual viewers are compiled, not
automatically launched for evidence. Linux/Windows native execution remains unverified.

;;INTENTION("Application is the lifetime boundary for Darling test executables;
registered Frames/windows stay alive until all windows close or Application_close
requests application-wide shutdown. Startup work runs off the UI thread, while
native event handling and rendering retain their canonical owners.")

## Starter and shutdown

- Each Darling executable creates an Application. `Frame_attachApplication`
  registers a Frame's borrowed native window and presentation service callback.
  Frames constructed during owner-thread Application callbacks auto-attach.
- Flow: construct app/Frames → attach windows → register start work →
  `Application_start(app)` → shutdown → release resources → return test result.
- `Application_start` keeps the starter alive until
  the last attached window closes or `Application_close(app)` is requested.
  Returning from a startup callback does not end the Application.
- `Application_close` requests closure of **all windows attached to that app**,
  not every window in the process or other Applications. It must be safe to
  request from a worker and idempotent; native close operations run on their
  required thread.
- Shutdown first signals workers to stop and stops presentation, then waits for
  in-flight callbacks/presents, and finally releases owned resources. Do not
  free Frame-owned windows twice or join a callback worker from itself.
- Automated tests use bounded work, timeouts and explicit close requests; manual
  viewers remain alive for user interaction. Neither mode proves visual approval.
- The migration covers **all Darling C test executables**, including primitive
  checks: those now attach a small native starter window. They are no longer
  fully headless and skip when no native window can be created. Scaffold Python
  compilation checks remain non-graphical tools, not Darling applications.
- Application closes native windows but retains borrowed C handles. Destroy
  Frames before freeing the Application (`Frame_destroyApplicationFrames` is
  available); Frame destruction unregisters its window and service callback.
- Removing the last registered window also completes the app. An app that has
  never registered a window waits for explicit close, allowing startup to attach
  windows through `Application_invoke`.

## Start event and concurrent work

;;INTENTION("Application_addStartEvent schedules supervised startup work on a
worker, not the native UI/event thread. Callback completion is not application
completion, and presentation pacing is not a scene-update clock.")

- `Application_addStartEvent(app, callback, userdata)` takes a typed
  `void callback(Application *, void *)`, not non-C lambda syntax.
- Up to 16 start events run in registration order on one supervised worker.
  Register before starting; null/full/active registrations fail. A concurrent
  start is refused. A completed app can be armed again, but closed windows are
  not resurrected; callers must supply live window registrations.
- Work can call thread-safe spoke functions or run a cancellable activity/focus
  loop while the Application remains active. Park/wait rather than busy-spin.
- Native window/UI work must marshal to the native owner thread. Moving a
  callback to a worker does not make arbitrary Frame/Window calls thread-safe.
- `Application_invoke` synchronously marshals work to that owner; admitted work
  completes before the worker is joined. Do not hold UI locks while invoking.
- Workers must cooperate with shutdown: check `Application_isRunning` and
  return. There is no unsafe force-kill of callbacks that ignore cancellation.
- Cancellation and join are supervised. Captured/userdata resources must outlive
  the callback; queued UI operations must not target destroyed Frames.

Minimal starter (no caller-written keep-alive loop):

```c
static void onStart(Application *app, void *userdata) {
    (void)app; (void)userdata;
    // Worker work here. Use Application_invoke for native/UI work.
    // Return normally to keep windows alive; Application_close(app) to quit all.
}

int main(void) {
    Application *app = Application("example");
    Frame *frame = Frame("example", 800, 600);
    if (!app || !frame || !Frame_attachApplication(frame, app) ||
        !Application_addStartEvent(app, onStart, NULL)) {
        Frame_destroy(frame); Application_free(app); return 1;
    }
    Frame_setFPSCap(frame, 120);
    Frame_setFPSCapWhenFocusGain(frame, -1);
    Frame_setFPSCapWhenFocusLost(frame, 1);
    Application_start(app);
    Frame_destroyApplicationFrames(app);
    Application_free(app);
    return 0;
}
```

## Frame FPS policy: presentation only

;;INTENTION("Frame FPS caps limit rendering/presentation submissions, independently
of scene or simulation workers. Focus selects presentation policy; it must not
retime scene threads. Platform defaults follow display/compositor capability,
not a promise that Windows always presents faster than macOS.")

Implemented class-named operations, with symmetric getters:

| Operation | Intended meaning |
| :--- | :--- |
| `Frame_setFPSCap(frame, number)` | Base maximum presentation rate in frames/second |
| `Frame_setFPSCapWhenFocusGain(frame, -1)` | Focused override: no application-imposed cap; compositor/vsync still applies |
| `Frame_setFPSCapWhenFocusLost(frame, 1)` | Unfocused override: at most one presentation/second |

- Focus-specific overrides select the effective cap; **0 inherits** the base
  cap. The Application service bridge rechecks focus/pending render demand in
  5ms parked slices. The default base is 60, focused inherits, unfocused is 1.
- `-1` means uncapped by this policy, **not** unlimited physical display refresh.
  Base zero and values below -1 are rejected (leave existing policy intact).
- Current default is conservative **60 FPS**. **120 FPS** or higher is an explicit
  setter choice when the display/compositor supports it. Automatic display-rate
  probing is not implemented; no claim that Windows is intrinsically faster.
- Caps are ceilings, not mandatory repaint rates: unchanged content may remain
  idle. Hidden/minimized behavior and close/resize responsiveness need explicit
  policies; the cap itself never sleeps or blocks native events/shutdown.
- `Surface_revalidate` coalesces paced render/publication demand; `Surface_poll`
  services it when due. Independent scene threads are not throttled.
  `Frame_capture` renders fresh offscreen pixels while host publication still
  respects the cap. Synchronous cascade tests explicitly choose `-1`.
- Frame exposes policy; R3 retains graphics scheduling/presentation ownership,
  and R1 retains native events/focus. No new independent R4 scheduler.

## Ownership and compatibility

`ecosystem/hotcwap/kernel/application.h` and `.c` currently provide:

- `Application_start`: blocking begin/service/close/join lifetime.
- `Application_close` / `Application_stop`: thread-safe close requests.
- `Application_addWindow`: a borrowed window registry, not Frame ownership.
- `Application_run`: lifecycle service loop; can also begin a dormant app.
- Kernel's multi-app reactor uses nonblocking `Application_begin`, services
  owner callbacks, then finishes/joins each Application before end hooks.

`Frame_run` is a compatibility Application starter, not an independent R4
event loop. Already attached Frames must return to the active lifecycle instead
of nesting starters. UI creation, registries and FPS setters are owner-affine;
only close requests and documented atomics/marshalling cross threads.

## Implementation/migration checklist

- [x] Blocking start, multiwindow close-all, hidden-window keepalive and worker join.
- [x] Start callbacks, owner-thread invocation and borrowed Frame attachment.
- [x] Focused/unfocused presentation policy implemented in the R3 Surface seam.
- [x] `frame_application_lifecycle_test.c`: two windows, callback return,
  hidden second window, last-window close and worker close-all.
- [x] `frame_fps_focus_test.c`: native focus requests plus deterministic-clock
  R3 submission counts for -1, 1 and inherited 120; coalesced demand.
- [x] Migrate all Darling C starters; manual viewers return to the Application
  instead of timed keep-alive/pump loops.
- [ ] Inject thread-allocation failure and exhaustively prove registration/
  cancellation boundaries; current lab scope is not full-contract battle testing.
- [ ] Native Linux/Windows execution, automatic display-rate defaults and
  explicit minimized/occluded presentation policy.
- [ ] User visual approval; never inferred from numeric/pixel lab results.
