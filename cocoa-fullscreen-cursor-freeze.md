# Cocoa fullscreen: frozen in-game cursor at top-left

## Summary

With **hardware acceleration** and **fullscreen** enabled on macOS, OpenTTD selects the **`cocoa-opengl`** video backend. Shortly after startup the in-game cursor stays at `(0, 0)` (top-left) for several seconds while the OS pointer still moves. Logs show that during the macOS fullscreen transition, **mouse-move events do not update `_cursor.pos`**, even though the OS mouse position is known.

Reproduce diagnostics with `-d driver=1`.

## Observed behaviour

1. Game starts fullscreen on `cocoa-opengl`.
2. In-game mouse sprite is frozen at the top-left.
3. OS mouse cursor moves freely over the window.
4. After ~4–10 seconds (fullscreen transition completes), the in-game cursor jumps to the mouse and tracks normally; the OS cursor is hidden again as expected.

## Log evidence (annotated)

Backend confirmation:

```text
Successfully probed video driver 'cocoa-opengl'
video backend: name='cocoa-opengl' ... fullscreen=true hw_accel=true
```

Fullscreen is requested during driver start (before the game loop is fully settled):

```text
cursor: Cocoa ToggleFullscreen request=true IsFullscreen=false _fullscreen=false
cursor: Cocoa ToggleFullscreen applied IsFullscreen=true _fullscreen=true
cursor[cocoa-toggle-fullscreen]: pos=(0, 0) device_abs=(0, 0) ...
```

While the freeze is visible, polled OS coordinates move but in-game position does not — and there are **no** `cursor-update` lines:

```text
cursor[cocoa-poll]: pos=(0, 0) device_abs=(1359, 698) ...
cursor[cocoa-poll]: pos=(0, 0) device_abs=(1916, 936) ...
cursor[cocoa-poll]: pos=(0, 0) device_abs=(2103, 1031) ...
```

Interpretation:

| Field | Meaning during freeze |
| --- | --- |
| `pos=(0, 0)` | In-game cursor (`_cursor.pos`) never updated |
| `device_abs=(x, y)` changing | OS mouse location is known via poll |
| Missing `cursor-update` | `UpdateCursorPosition` was not called (no delivered `mouseMoved`) |

Fullscreen settle then arrives; hit-test says the mouse is outside the view, so `mouseEntered` is not synthesised from that path:

```text
cursor: windowDidEnterFullScreen loc=(1419.5, 703.0) inside=false in_window=true
cursor: windowDidEnterFullScreen did not synthesise mouseEntered (mouse outside view)
```

After a burst of enter/exit and the first real move events, tracking recovers:

```text
cursor: cocoa mouseEntered
cursor: cocoa mouseExited
cursor: cocoa mouseEntered
cursor[cursor-update]: pos=(1856, 947) device_abs=(1856, 947) ...
```

## Root cause

`_cursor.pos` is only updated from Cocoa mouse-move handlers. During the asynchronous macOS fullscreen animation started at driver `Start`, those move events are not delivered (or not processed into `UpdateCursorPosition`). The in-game cursor therefore remains at its default `(0, 0)` until the transition finishes and move events resume.

The OS pointer still moves because it is the system cursor; OpenGL draws the in-game cursor from the stale `_cursor.pos`.

### 1. Fullscreen is entered during OpenGL driver start

[cocoa_ogl.mm](/src/video/cocoa/cocoa_ogl.mm#L209) saves `_fullscreen` and, after creating the window, calls `ToggleFullscreen`:

```209:217:src/video/cocoa/cocoa_ogl.mm
	bool fullscreen = _fullscreen;
	if (!this->MakeWindow(_cur_resolution.width, _cur_resolution.height)) {
		this->Stop();
		return "Could not create window";
	}

	this->AllocateBackingStore(true);

	if (fullscreen) this->ToggleFullscreen(fullscreen);
```

(The quartz path does the same in [cocoa_v.mm](/src/video/cocoa/cocoa_v.mm#L614).)

### 2. ToggleFullscreen kicks off an async macOS transition

[ToggleFullscreen](/src/video/cocoa/cocoa_v.mm#L182) uses `NSWindow toggleFullScreen:`. That returns quickly; the UI actually finishes later via `windowDidEnterFullScreen`.

```182:200:src/video/cocoa/cocoa_v.mm
bool VideoDriver_Cocoa::ToggleFullscreen(bool full_screen)
{
	...
	if ([ this->window respondsToSelector:@selector(toggleFullScreen:) ]) {
		[ this->window performSelector:@selector(toggleFullScreen:) withObject:this->window ];
		...
		return true;
	}
	...
}
```

### 3. In-game position only updates from mouse-move events

[internalMouseMoveEvent](/src/video/cocoa/cocoa_wnd.mm#L667) is the path that feeds absolute coordinates into the engine:

```667:677:src/video/cocoa/cocoa_wnd.mm
- (void)internalMouseMoveEvent:(NSEvent *)event
{
	if (_cursor.fix_at) {
		_cursor.UpdateCursorPositionRelative(event.deltaX * self.getContentsScale, event.deltaY * self.getContentsScale);
	} else {
		NSPoint pt = [ self mousePositionFromEvent:event ];
		_cursor.UpdateCursorPosition(pt.x, pt.y);
	}

	HandleMouseEvents();
}
```

[UpdateCursorPosition](/src/gfx.cpp#L1826) is what actually writes `_cursor.pos`:

```1826:1841:src/gfx.cpp
bool CursorVars::UpdateCursorPosition(int x, int y)
{
	this->delta.x = x - this->pos.x;
	this->delta.y = y - this->pos.y;

	bool warp = false;
	if (this->fix_at) {
		warp = this->delta.x != 0 || this->delta.y != 0;
	} else if (this->pos.x != x || this->pos.y != y) {
		this->dirty = true;
		this->pos.x = x;
		this->pos.y = y;
	}

	DebugCursorState("cursor-update", x, y, false);
	return warp;
}
```

During the freeze, logs show **`cocoa-poll` without `cursor-update`**, so this path is idle while the OS mouse moves.

### 4. Poll can see the mouse; it does not (yet) drive `_cursor.pos`

Diagnostic polling in [InputLoop](/src/video/cocoa/cocoa_v.mm#L449) reads `[NSEvent mouseLocation]` and logs it as `device_abs`, but does not call `UpdateCursorPosition`:

```462:469:src/video/cocoa/cocoa_v.mm
	/* Poll OS mouse position even when no mouseMoved events arrive (e.g. during fullscreen settle). */
	if (_debug_driver_level >= 1 && this->window != nil && this->cocoaview != nil) {
		NSPoint screen_mouse = [ NSEvent mouseLocation ];
		...
		DebugCursorState("cocoa-poll", static_cast<int>(real_pt.x), static_cast<int>(real_pt.y), false);
	}
```

That is why logs can show `device_abs` changing while `pos` stays `(0, 0)`.

### 5. Fullscreen completion does not always seed a cursor position

When the transition ends, [windowDidEnterFullScreen](/src/video/cocoa/cocoa_wnd.mm#L1486) only synthesises `mouseEntered` if a hit-test says the pointer is inside the view. In the captured run `inside=false`, so that path did not run; tracking only resumed after later enter/exit + real `mouseMoved` events.

```1486:1502:src/video/cocoa/cocoa_wnd.mm
- (void)windowDidEnterFullScreen:(NSNotification *)aNotification
{
	NSPoint loc = [ driver->cocoaview convertPoint:[ [ aNotification object ] mouseLocationOutsideOfEventStream ] fromView:nil ];
	BOOL inside = ([ driver->cocoaview hitTest:loc ] == driver->cocoaview);
	...
	if (inside) {
		NSEvent *e = [ [ NSEvent alloc ] init ];
		[ driver->cocoaview mouseEntered:e ];
		[ e release ];
	} else {
		Debug(driver, 1, "cursor: windowDidEnterFullScreen did not synthesise mouseEntered (mouse outside view)");
	}
	...
}
```

`mouseEntered` only sets `_cursor.in_window`; it does **not** set `_cursor.pos`.

## Causal chain

```text
_fullscreen=true at config load
        │
        ▼
cocoa-opengl Start() ──► ToggleFullscreen() ──► NSWindow toggleFullScreen: (async)
        │
        ▼
game loop runs; OpenGL draws cursor at _cursor.pos == (0,0)
        │
        ├─ cocoa-poll: OS mouse moves          ✅
        └─ mouseMoved → UpdateCursorPosition   ❌ (until FS settle)
        │
        ▼
windowDidEnterFullScreen (+ enter/exit)
        │
        ▼
mouseMoved resumes → pos tracks → freeze ends
```

## Related settings / selection

- `fullscreen` / `video_hw_accel` globals and UI: [misc_settings.ini](/src/table/settings/misc_settings.ini), [settings_gui.cpp](/src/settings_gui.cpp)
- Video driver probe prefers OpenGL when hw accel is on: [driver.cpp](/src/driver.cpp)
- Startup backend log: [LogVideoBackend](/src/driver.cpp#L45)

## Notes for a fix (not implemented here)

Possible approaches:

1. While fullscreen is settling (or whenever move events are absent), apply the same coordinates used by `cocoa-poll` via `UpdateCursorPosition`.
2. On `windowDidEnterFullScreen`, always sample the mouse and call `UpdateCursorPosition`, even when `inside=false`.
3. Defer `ToggleFullscreen` until after the first successful draw / input loop, reducing the race with startup.

Any of these should keep `_cursor.pos` aligned with the OS pointer during the transition.
