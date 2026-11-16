---
schema: context/v1
id: fork.substrate.graphics-and-sprites
kind: context
title: "Graphics and sprites: blitters, sprite cache and loaders, zoom, palette animation, dirty-rect redraw, fonts, text layout and base sets"
description: "Read before changing a blitter, the sprite cache or a sprite loader, zoom or palette code, the dirty-block redraw, fonts or text layout, screenshots, or base set discovery."
status: active
scope: directory
providers: ["*"]
audience: [human, agent]
composition: extend
order: 210
tracks:
  - 'src/blitter/**'
  - 'src/spriteloader/**'
  - 'src/fontcache/**'
  - 'src/fontcache.*'
  - 'src/gfx*'
  - 'src/sprite.*'
  - 'src/spritecache*'
  - 'src/palette*'
  - 'src/zoom_*'
  - 'src/bmp.*'
  - 'src/screenshot_*'
  - 'src/base_media*'
  - 'src/bootstrap_gui.cpp'
  - 'src/texteff.*'
  - 'src/transparency*'
  - 'src/provider_manager.h'
  - 'src/os/*/font_*'
  - 'src/os/macosx/string_osx.*'
  - 'src/os/windows/string_uniscribe.*'
  - 'src/table/palettes.h'
  - 'src/table/palette_convert.h'
  - 'src/table/sprites.h'
  - 'src/table/landscape_sprite.h'
  - 'src/table/opengl_shader.h'
  - 'src/table/hlsl_shader.h'
  - 'src/table/string_colours.h'
  - 'src/tests/mock_fontcache.h'
  - 'src/tests/mock_spritecache.*'
---

# Graphics and sprites

Read at f8b1d52e80.

## Purpose and ownership

Owns how pixels are produced: the blitter registry and the active blitter, the sprite ID space and
its LRU cache, GRF sprite decoding, zoom levels, the palette and its animation, the dirty-block
redraw, font caches, paragraph layout, text effects, transparency options, screenshot encoders and
base graphics set discovery. Video drivers, the window system, viewports and `src/screenshot.cpp`
belong to [core loop](../core-loop/README.md) and [GUI and console](../gui-and-console/README.md);
NewGRF parsing to [NewGRF](../newgrf/README.md). None of this is game state: it may differ between
peers and must never feed back into the simulation (ADR 0016). No fork hunk in the tracked files
(`git log --oneline upstream/master..HEAD -- src/gfx.cpp src/blitter src/spritecache.cpp` is empty);
the fork's quiet screenshot lives in `src/screenshot.cpp`.

## Entry points

- `src/blitter/factory.hpp:BlitterFactory` - self-registering factories (`static FBlitter_X
  iFBlitter_X` in each `src/blitter/*.cpp`); `SelectBlitter`, `GetBlitterFactory` (default `null`
  under `DEDICATED`, `32bpp-anim` under Cocoa, else `8bpp-optimized`), `GetCurrentBlitter`.
- `src/blitter/base.hpp:Blitter` - drawing interface (`Draw`, `DrawRect`, `PaletteAnimate`,
  `UsePaletteAnimation`) and a `src/spriteloader/spriteloader.hpp:SpriteEncoder`.
- `src/gfxinit.cpp:GfxLoadSprites` - resets the sprite cache, `LoadSpriteTables` (base GRFs, climate
  overlays from `src/table/landscape_sprite.h`, then `LoadNewGRF`), `GfxInitPalettes`.
- `src/gfxinit.cpp:SwitchNewGRFBlitter` / `CheckBlitter` / `RealChangeBlitter` - pick the best blitter
  for base set depth, NewGRF depth, font anti-aliasing and full animation.
- `src/spritecache.cpp:GetRawSprite` (via `src/spritecache.h:GetSprite`) - load-on-demand, LRU,
  fallback to `SPR_IMG_QUERY`; `LoadNextSprite` / `DupSprite` record where a sprite lives.
- `src/spriteloader/grf.cpp:SpriteLoaderGrf::LoadSprite` - container v1/v2 decoding per zoom and
  depth; `src/spriteloader/makeindexed.cpp:SpriteLoaderMakeIndexed` converts 32bpp to 8bpp.
- `src/gfx.cpp:DrawSprite`, `GfxFillRect`, `DrawString`, `FillDrawPixelInfo`; dirty blocks:
  `src/gfx.cpp:AddDirtyBlock`, `MarkWholeScreenDirty`, `DrawDirtyBlocks`.
- `src/palette.cpp:DoPaletteAnimations`, `CopyPalette`; fonts `src/fontcache.h:FontCache`,
  `FontProviderManager`; layout `src/gfx_layout.h:Layouter`.
- `src/base_media_base.h:BaseMedia` (`FindSets`, `SetSetByName`, `GetUsedSet`),
  `src/gfxinit.cpp:DetermineBestSet`; screenshots `src/screenshot_type.h:ScreenshotProvider`.

## Lifecycle

- Startup (`src/openttd.cpp:openttd_main`): `FontCache::LoadFontCaches`, `BaseGraphics::FindSets`
  and `SetSetByName`/`SetSetByShortname`, `GfxInitPalettes`, initial blitter guess (user's `-b` or
  config; else `32bpp-anim` unless 8bpp is wanted), then the video driver, `UpdateGUIZoom`,
  `src/bootstrap_gui.cpp:HandleBootstrap` when no graphics set was found.
- Every world (`src/genworld.cpp`, `src/saveload/afterload.cpp`, `ReloadNewGRFData`):
  `GfxLoadSprites` then `LoadStringWidthTable`.
- Draw: `DrawSprite` -> `GetSprite` -> `GetRawSprite` (cache miss: `ReadSprite` -> loader ->
  `ResizeSprites`/padding -> `SpriteEncoder::Encode` of the current blitter) -> `GfxBlitter` ->
  `Blitter::Draw`.
- Redraw: callers mark rectangles with `AddDirtyBlock` (64x8 px blocks plus a bounding
  `_invalid_rect`); `src/window.cpp:UpdateWindows` calls `DrawDirtyBlocks`, which coalesces blocks
  down then right and calls `RedrawScreenRect` -> `DrawOverlappedWindowForAll` and
  `VideoDriver::MakeDirty`.
- Per real-time frame `src/openttd.cpp:GameLoop` calls `IncreaseSpriteLRU` (evicts down to
  `_sprite_cache_size` MiB scaled by screen depth) and, when not paused and full animation is on,
  `DoPaletteAnimations`; the video driver pulls the palette with `CopyPalette`.

## Invariants

- **[verified]** Cached sprites are encoded for one blitter. A switch clears the system sprites, the
  font caches and the sprite cache and re-inits all windows (`src/gfxinit.cpp:RealChangeBlitter`).
- **[verified]** The blitter is never switched when the user chose it (`_blitter_autodetected`) or
  when the current blitter has depth 0 (`null`), and a switch is queued with
  `VideoDriver::QueueOnMainThread`, not done in place (`src/gfxinit.cpp:SwitchNewGRFBlitter`).
- **[verified]** Font anti-aliasing (`GetFontAAState`) or any 32bpp NewGRF forces a 32bpp blitter;
  `40bpp-anim` is usable only with a video driver that has an animation buffer
  (`src/blitter/40bpp_anim.hpp:FBlitter_40bppAnim`).
- **[verified]** A sprite ID requested with the wrong `SpriteType` or one that does not exist yields
  `SPR_IMG_QUERY` (or `PALETTE_TO_DARK_BLUE` for recolour), a font request may retype a normal
  sprite, and a NewGRF that breaks those fallbacks is a `UserError`
  (`src/spritecache.cpp:HandleInvalidSpriteRequest`).
- **[verified]** Map generator sprites are stored raw, never encoded, and a GRF may not change their
  type (`src/spritecache.cpp:ReadSprite`, `LoadNextSprite`).
- **[verified]** Font sprites are encoded at `_font_zoom` into the `ZoomLevel::Min` slot, so a font
  zoom change must call `GfxClearFontSpriteCache` (`src/gfx.cpp:AdjustGUIZoom`).
- **[verified]** `_cur_palette` is guarded by `_palette_mutex`; animation writes it on the game loop
  and drivers copy it with `CopyPalette`, which resets `count_dirty` (`src/palette.cpp`).
- **[verified]** Palette animation uses its own static counter, not the game's `Random`, and stops
  while paused (`src/palette.cpp:DoPaletteAnimations`, `src/openttd.cpp:GameLoop`).
- **[verified]** `Blitter_Null` keeps sprite width, height and offsets but no pixels
  (`src/blitter/null.cpp:Blitter_Null::Encode`), so size queries still work headless.
- **[inferred]** Screen pixels, glyph widths, GUI zoom and font choice vary per client; any game logic
  that branched on them would desync (ADR 0016).

## Extension points

- New blitter: a `Blitter` subclass plus a `BlitterFactory` subclass with a static instance and an
  `IsUsable` override; list it in `src/blitter/CMakeLists.txt` with its `CONDITION` and, to be
  auto-picked, in the `replacement_blitters` table of `SwitchNewGRFBlitter`.
- New font backend: a `FontCacheFactory` (sorted by `src/provider_manager.h:ProviderManager`);
  TrueType backends derive from `src/fontcache/truetypefontcache.h:TrueTypeFontCache`. Caches stack:
  `src/fontcache.cpp:FontCache::Register` puts the new one on top of the `SpriteFontCache` parent.
- New text layouter: a `*ParagraphLayoutFactory` tried in order in the `Layouter` constructor (ICU,
  Uniscribe, CoreText, then `src/gfx_layout_fallback.h:FallbackParagraphLayoutFactory`).
- New screenshot format: a `ScreenshotProvider` (a `PriorityBaseProvider`) with a static instance.

## Traps and forbidden operations

- `GfxLoadSprites` is not only graphics: `LoadSpriteTables` runs `LoadNewGRF`, which defines engines,
  industries and every other NewGRF feature, on dedicated servers too (knowledge
  `gfxloadsprites-loads-every-newgrf`).
- `src/spritecache.cpp:AllocateSpriteCache` may grow the vector; never keep a `SpriteCache *` across
  it (`DupSprite` allocates the new entry first for this reason).
- A pointer from `GetSprite` is valid only until the next eviction (`IncreaseSpriteLRU`, a blitter
  switch, `GfxClearSpriteCache`); never store it.
- `_screen_disable_anim` is set for giant screenshots; the 32bpp-anim blitters assert it is clear in
  several paths, so do not draw to the real screen while it is set.
- Text effects are local view state: `src/texteff.cpp:AddTextEffect` returns `INVALID_TE_ID` in the
  menu and moves on a `TimerWindow`; a `TextEffectID` held by a game object differs between peers
  (knowledge `text-effect-ids-are-client-local`).
- On macOS a missing graphics set under a headless run hangs on a modal error instead of failing
  (knowledge `macos-missing-baseset-hangs-headless-runs`).

## Tests and validation

- Catch2 (`openttd_test`): only `src/tests/test_window_desc.cpp` touches this area, through
  `src/tests/mock_environment.h` (`MockGfxLoadSprites`, `MockFontCache`). Run
  `ctest -R WindowDesc` from `build/`.
- Regression (`cmake/scripts/Regression.cmake`) runs `-vnull -snull -mnull`, so only the null blitter
  is exercised; a blitter, palette or layout change needs a GUI run (`tools/gate.sh change`, then a
  manual `-b <blitter>` launch). `openttd -h` lists blitters and base sets.
- Debug: `-d sprite=<n>` (cache loads, evictions, fallback sprites), `-d driver=<n>` (blitter
  probe/switch), `-d fontcache=<n>`, `-d grf=<n>` for GRF decode problems.
- Performance: the framerate window's `Drawing` and `Video` elements
  (`src/framerate_type.h:PerformanceElement`).

## Decisions, rules and knowledge

- ADR 0022 (blitters and fonts are runtime-chosen providers), ADR 0028 (base sets and NewGRFs, no
  proprietary data), ADR 0016 (view state never enters the simulation), ADR 0027 (`add_files`
  conditions decide which blitters exist).
- Rules: `project.game-state-mutates-only-through-commands`, `project.never-report-unrun-validation-as-pass`.
- Knowledge: `gfxloadsprites-loads-every-newgrf`, `text-effect-ids-are-client-local`,
  `macos-missing-baseset-hangs-headless-runs`, `game-state-is-owned-by-the-game-thread-not-the-main-thread`.
- Related maps: [NewGRF](../newgrf/README.md), [GUI and console](../gui-and-console/README.md),
  [core loop](../core-loop/README.md), [sound and music](../sound-and-music/README.md).

## Typical tasks

- To fix a wrong or missing sprite: run with `-d sprite=3`, start at `GetRawSprite` /
  `HandleInvalidSpriteRequest`, then `SpriteLoaderGrf::LoadSprite`; check the ID range in
  `src/table/sprites.h` (`SPR_NEWGRFS_BASE`, `MAX_SPRITES`).
- To add or tune a blitter: copy the nearest `src/blitter/32bpp_*` pair, register its factory, add it
  to `SwitchNewGRFBlitter`, launch with `-b <name>` and compare frame times.
- To change what is redrawn: mark with `AddDirtyBlock` or `SetDirty` on the window, never paint
  outside `DrawDirtyBlocks`; verify with `src/toolbar_gui.cpp:ToggleDirtyBlocks` (needs
  `newgrf_developer_tools`), which tints each redrawn block.
- To change base set selection: start at `BaseMedia::FindSets` and `DetermineBestSet`, keep
  `HandleBootstrap` failing cleanly when there is no screen.
