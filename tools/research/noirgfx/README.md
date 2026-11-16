# noirgfx

RESEARCH-ONLY. Builds **NoirGFX**, a base graphics set that recuts every OpenGFX sprite as a
red-black linocut in the SIG NIHL style: strict ink / paper / signal red, carved hatching, 32bpp at
normal, 2x and 4x zoom, seamless across tiles. Issue: `.ai/repo/project/issues/I0079.yaml`.

It plugs in the way upstream designed graphics to be replaced (ADR 0028): a `.obg` plus six `.grf`
files in a `baseset/` directory, chosen in *Game Options → Base graphics set* or with
`openttd -I NoirGFX`. No engine change.

```bash
tools/research/noirgfx/noirgfx bootstrap              # pinned grfcodec + OpenGFX 8.0 into ~/.cache/noirgfx
tools/research/noirgfx/noirgfx curate ZIP2 ZIP3       # Kenney models from the two bundles (not in git)
tools/research/noirgfx/noirgfx preview --out /tmp/pv  # iso scene + contact sheet, seconds
tools/research/noirgfx/noirgfx build                  # whole set -> ~/.cache/noirgfx/dist/noirgfx-<v>.tar
tools/research/noirgfx/noirgfx check [--rebuild]      # invariants (below)
tools/research/noirgfx/noirgfx install                # copy the tar into ~/Documents/OpenTTD/baseset
tools/research/noirgfx/noirgfx shot build/openttd D   # new game in an isolated HOME, screenshots to D
```

Needs `uv`, `cmake`, a C++ compiler, `git`, `unzip` and Blender (`brew install --cask blender`;
`NOIRGFX_BLENDER` overrides the binary). Nothing generated is written into the tree.

## How it works

1. **Decode.** grfcodec (pinned to `4c71c0a`) turns each OpenGFX GRF into sprite sheets and an NFO.
2. **Stylize** (`src/noirgfx/stylize.py`), every 8bpp sprite, once per zoom level (1x, 2x, 4x):
   - *Upscale* palette indices with Scale2x (objects) or nearest neighbour (ground, so tiles keep
     tiling pixel-perfectly).
   - *Material* per palette colour from hue and saturation (vegetation, earth, neutral, water,
     other), because OpenGFX's luminance does not separate grass from asphalt. Each material has a
     tone curve and a **cut direction**: areas of equal value are told apart by line direction, as in
     a woodcut.
   - *Line screen*: ink where the distance to the nearest cut line is below the ink density, single
     hatch, then cross hatch, then solid. Lines wobble and swell with a broad grain.
   - *Seamless*: every texture is a function of screen position modulo the tile lattice
     ((32, 8) px per zoom step: a tile step and one height level), so two sprites that meet show
     the same pattern across the join, at any height. `check` proves it per sprite.
   - *Cuts*: a paper line where vegetation meets built ground or water (kerbs, ballast, shores),
     only inside a sprite so no tile grid appears; objects get a carved ink outline, ink internal
     edges and a paper highlight on the lit upper-left rim.
   - *Signal red* for red/orange source colours on objects, cleaned into contiguous shapes.
   - *Company colours* keep their palette mask (indices 198-205), so players' colours still apply.
3. **Replace with Kenney models** (`mapping.toml`, `src/noirgfx/render.py`,
   `src/noirgfx/blender_render.py`). A mapped sprite is rendered from a CC0 Kenney glTF model
   (`assets/kenney-curated/`, see *Kenney models* below) by headless Blender: orthographic camera at OpenTTD's
   projection (30 degrees down, 45 degrees round, one tile = 64 px across at 1x), fitted to the
   height of the OpenGFX sprite it replaces and anchored where the model meets the ground, so the
   game places it exactly as before. The render gives a lit colour image and a world-normal image;
   `run_rgb` cuts it with the direction chosen by the face (roof, left wall, right wall), carves
   creases where the normals turn, and frees the figure from the dark ground with a paper rim.
   Renders are cached by the hash of model, parameters and renderer, so a rebuild renders only
   what changed. Mapped so far: every temperate, sub-arctic (with snow on upward faces) and
   sub-tropical tree, all seven growth stages (dying stages darken, dead is near black).
4. **Encode.** Every sprite keeps its 8bpp original and gains three 32bpp alternatives (`zi4`,
   `zi2`, `normal`) with masks; grfcodec writes container version 2 GRFs.
5. **Pack.** `.obg` with OpenTTD's checksums (data section only, see `build.py:grf_md5`), OpenGFX's
   GPL-2 licence, a readme, a deterministic tar and `manifest.json`.

All colours, thresholds and the AI prompt live in [`style.toml`](style.toml); nothing else carries
a number of its own.

## Limits (v1)

- The three sprite fonts are left as OpenGFX draws them (the game recolours them per text colour).
- Palette animation is not reproduced: water and lights are restyled as static images.
- Window chrome is drawn by the engine from palette colours, not from sprites; a base set cannot
  restyle it.
- Classification is geometric (tile-shaped at the tile origin = ground, top-left anchored = GUI,
  otherwise object); a sprite misclassified gets the other treatment.

## Next replacements

`mapping.toml` grows by kind: town houses from City Kit Suburban / Commercial and Modular Buildings
(each house's tile layout in `src/table/town_land.h` gives the footprint and sprite per stage),
road vehicles from Car Kit (eight directions per vehicle), ships from Watercraft. Terrain, slopes,
track and road beds stay restyled from OpenGFX: Kenney has no set in OpenTTD's slope geometry.

## AI refinement (planned, opt-in)

The owner chose AI img2img by category on top of the deterministic set. The design: sprites of one
category are laid out on a sheet at 4x, refined by the model in `style.toml` with the prompt there,
cut back, re-quantised to the three inks, alpha locked to the deterministic master (the silhouette
cannot change), and cached by content hash so a rebuild never calls the model again.

## Kenney models

The models are CC0 (public domain; credit to Kenney, www.kenney.nl, is appreciated and given here)
but **not in git**: GitHub refuses Git LFS objects on a public fork, and the owner chose not to
commit 16 MB of binaries instead (answer 8, `research/noirgfx/owner-answers.md`).
`noirgfx curate "Kenney Game Assets 2 version 22.zip" "Kenney Game Assets 3 version 28.zip"`
extracts both bundles into `assets/kenney/`, copies the eight packs NoirGFX uses (with each pack's
`License.txt`) into `assets/kenney-curated/`, and verifies every file against the committed
`assets/kenney-curated.sha256`, so a build anywhere uses exactly the models this set was made from.
Both directories are gitignored. Without them `noirgfx build` stops and names the command.

| Directory | Source pack | Bundle |
|---|---|---|
| `city-commercial/` | City Kit - Commercial | Kenney Game Assets 3 v28 |
| `city-roads/` | City Kit - Roads | Kenney Game Assets 3 v28 |
| `city-suburban/` | City Kit - Suburban | Kenney Game Assets 3 v28 |
| `car-kit/` | Car Kit | Kenney Game Assets 3 v28 |
| `nature-kit/` | Nature Kit | Kenney Game Assets 3 v28 |
| `retro-urban/` | Retro Urban Kit | Kenney Game Assets 3 v28 |
| `modular-buildings/` | Modular Buildings (100 assets) | Kenney Game Assets 2 v22 |
| `watercraft/` | Watercraft pack (30 assets) | Kenney Game Assets 2 v22 |
