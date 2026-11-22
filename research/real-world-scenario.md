# Real-world scenarios: a place as an OpenTTD map

A place name becomes a playable OpenTTD map: resolved to explicit candidates, acquired from lawful
open data into a versioned snapshot, compiled offline into a heightmap, towns, industries and an
economy whose every entity is labelled OBSERVED, DERIVED, INFERRED or HYPOTHETICAL.

RESEARCH-ONLY, UNSTABLE. Rules `project.mcp-surfaces-are-derived`,
`project.panel-items-are-routable-pages`, `project.game-state-mutates-only-through-commands`,
`project.never-report-unrun-validation-as-pass`.

A person types a place ("Brno, Czech Republic"). The pipeline resolves it to explicit candidates,
fetches a bounded set of lawful open data for the chosen one, freezes it as a versioned snapshot,
and compiles the snapshot offline and deterministically into an OpenTTD heightmap, town data and a
GameScript that founds the towns, builds the industries and signs the landmarks through the game's
own commands. Every game entity and economy link says whether it is OBSERVED, DERIVED, INFERRED or
HYPOTHETICAL, why, with what confidence and by which versioned rule.

Nothing here is compiled into OpenTTD. The tool runs outside the game (`tools/research/real-world`,
Rust); the game consumes only its artifacts, through supported paths.

## Commands

```bash
# Offline (the default): compile the committed Brno snapshot and prove it is deterministic.
tools/research/real-world/rw-scenario compile --snapshot tools/research/real-world/fixtures/brno/snapshot.json --out build-real-world/brno
tools/research/real-world/rw-scenario replay  --snapshot tools/research/real-world/fixtures/brno/snapshot.json --expect tools/research/real-world/fixtures/brno/SHA256SUMS
tools/research/real-world/rw-scenario validate --scenario build-real-world/brno/scenario.json

# Headless proof that it loads and that the towns and industries are placed (loopback only).
tools/research/real-world/rw-scenario load-check --out build-real-world/brno --openttd <research build>/openttd

# Explicitly online: a new place. Uses the network only with --live, paced, cached, bounded.
tools/research/real-world/rw-scenario resolve --live --query "Brno, Czech Republic"
tools/research/real-world/rw-scenario acquire --live --query "Olomouc, Czech Republic" --out build-real-world/olomouc.snapshot.json

# Tests: determinism, replay, licensing, provenance, evidence rules, the landmark test.
(cd tools/research/real-world && cargo test --offline)
```

`rw-scenario help` lists every command and its exit codes (0 ok, 2 usage, 3 missing prerequisite,
4 ambiguous place, 5 acquisition failed, 6 partial, 7 runtime failure, 10 validation or determinism
failure). The wrapper builds the tool on first use (`cargo build --offline`, its own `target/`) and
exits with the tool's exit code. There is no Justfile in this repository; per-tool entry points under
`tools/research/` are its convention, and this is one more.

## The flow

| Stage | What happens | Where |
|---|---|---|
| Resolve | Nominatim `search` (one request) gives ranked candidates; rule `dominant-settlement/v1` chooses one or refuses (exit 4, AMBIGUOUS) and never falls back to a wider area | `src/resolve.rs` |
| Extent | a square around the candidate's box plus a margin, in the local CRS `local-eqr/v1`, clamped to 4..80 km; scale = side / map size | `src/geo.rs` |
| Acquire | one Overpass query generated from the rules' classify table, bounded by the extent; an elevation grid sampled from Copernicus DEM GLO-30 Cloud Optimized GeoTIFFs with HTTP range reads of the directories and only the internal tiles the grid touches | `src/acquire.rs`, `src/net.rs`, `src/cog.rs` |
| Snapshot | `real-world-snapshot/v1`: request, candidates and the choice, extent, every source with licence, attribution, terms checked, completeness and the SHA-256 of each raw answer, normalised features (all OBSERVED), the elevation grid, ordered transformations, and a content hash | `src/model.rs`, `src/snapshot.rs` |
| Compile | heightmap (bilinear resample, linear grey 1..255), towns, industries and landmark signs by the versioned rules, the economy graph, its chain completion | `src/compile.rs`, `src/economy.rs` |
| Emit | `heightmap.png`, `towns.json`, `game/RealWorldScenario/`, `openttd.cfg`, `ATTRIBUTION.txt`, `preview.svg`, `scenario.json` (`real-world-scenario/v1`), `SHA256SUMS` | `src/lib.rs`, `src/gamescript.rs`, `src/preview.rs` |
| Load | a loopback dedicated server on the heightmap with the GameScript, in a personal directory of its own; reads what it placed, saves the game, quits | `src/loadcheck.rs` |

The contract of both documents is `tools/research/schema/real-world-scenario.v1.schema.json`; the
stricter checks (ordering, hashes, evidence rules) are `snapshot::validate` and `compile::validate`.
A snapshot whose content hash does not match is refused; so is one whose features or elevation
layer failed, or one without licence or attribution.

### How the game is reached (the supported loading path)

- **Terrain**: `openttd -g heightmap.png` starts a new game from a heightmap (`src/openttd.cpp`,
  case `'g'`, `FiosGetHeightmapListCallback`), with the settings of `openttd.cfg`
  (`game_creation.heightmap_rotation = 1`, clockwise: tile x is the image row, tile y the column,
  `src/heightmap.cpp` `GrayscaleToMapHeights`; `heightmap_height` the number of levels).
- **Towns, industries, landmarks**: the generated GameScript runs as deity and calls
  `GSTown.FoundTown`, `GSTown.ExpandTown`, `GSIndustryType.BuildIndustry` and `GSSign.BuildSign`,
  which issue the game's own commands, so placement is deterministic for a seed and safe in a network
  game. It resolves every industry role against the running game by cargo labels (an industry type
  whose produced and accepted labels equal the role's), never by a type number, searches an outward
  ring of tiles when the exact tile is not buildable, and logs each result and its displacement with
  the prefix `rw:`. It puts the attribution in the game view as a news item.
- **Town data JSON**: `towns.json` is OpenTTD's own format (docs/importing_town_data.md,
  `ParseTownData` in `src/genworld.cpp`) for the Scenario Editor's *Load town data*. That path is
  reachable only from the editor's file dialog (`src/fios_gui.cpp`, `LoadTownData`), so it is not a
  headless path; the GameScript is.
- **Names**: OpenTTD holds at most 31 characters in a town or sign name (`src/town_type.h`,
  `src/signs_type.h`); longer names are shortened with an ellipsis and the full name is kept in the
  diagnostics. The settings go into one `[section]` each: OpenTTD's ini reader keeps only the first
  section of a name, so a repeated header silently drops the settings under it.
- **Isolation of the load**: with `-c <file>` OpenTTD makes that file's directory its personal
  directory, so the check's save lands in `<out>/loadcheck/save/`; `HOME` points into the work
  directory too (the GameScript is found there), and the base sets are linked read-only. The user's
  own OpenTTD directory is not written.
- **Engine-generated extras**: a new game always generates at least one town of its own
  (`GenerateTowns` clamps the number to at least 1, `src/town_cmd.cpp`). The GameScript renames every
  town it did not found to "Generated town <n>" (`GSTown.SetName`) and gives it the town text
  "engine-generated, not a real place (HYPOTHETICAL)" (`GSTown.SetText`); `load-check` lists them
  under `engine_generated` and the panel page shows them. `industry_density = 0` (funded only) keeps
  the generator from adding industries.
- **In the game view**: every founded town and built industry carries its evidence class, its
  evidence and anchor records and its rule as its town or industry text (`GSTown.SetText`,
  `GSIndustry.SetText`); the attribution of every source is posted as a news item at the start.
- **Several industries per town**: `economy.multiple_industry_per_town = true`. With the default,
  the game refuses a second industry of a type whose nearest town already has one
  (`ERR_UNKNOWN`, `src/industry_cmd.cpp`), which a map placed by evidence runs into.
- **Saved games**: the GameScript saves `placed = true` and a reloaded game resumes without placing
  anything again; `load-check` saves, reloads its own save and fails unless the town, industry and
  sign counts are unchanged.

## Evidence classes

| Class | Means | Examples in the Brno slice |
|---|---|---|
| OBSERVED | the source records it | every snapshot feature: a `place=*` node, `leisure=nature_reserve` relation, `landuse=quarry` area, `power=plant` with its `plant:source` |
| DERIVED | a deterministic rule made a game construct of the same kind | a settlement with a population tag becomes a town; farmland becomes a farm; a forest a forest; a mapped sawmill the sawmill; a peak or nature reserve becomes a sign |
| INFERRED | a judgement under uncertainty | a gas- or waste-fired plant is the power station (that it accepts coal is the game's rule, not the plant's fuel); a town without a population tag gets a default; every proximity edge |
| HYPOTHETICAL | no source supports it as it is in the game | a gameplay stand-in at a mapped site whose class no temperate industry matches (a sand, clay or stone quarry gets a coal mine, a works or industrial area a factory), named "Coal Mine (gameplay stand-in at extraction osm:…)", never after the site or its operator; a consumer or supplier the chain lacks (rule `complete-chain/v1`), named "(hypothetical)"; every edge that touches either; confidence 0 |

Each entity names two records: `evidence_feature`, the record that supports it as it is in the game
(none exactly when it is HYPOTHETICAL), and `anchor_feature`, the record whose position it takes (a
stand-in names its site here). Suburbs (`place=suburb`) are parts of their city, never towns of their
own (`part_of_city_places`); the city's one town stands for them, and `lost_detail` says how many.

A game entity is never OBSERVED: its evidence feature is. Population, cargo, production and the
balance curve (`population_scale x pop^population_exponent`, clamped) are game assumptions, kept in
each town's `population_basis`, never presented as facts. A quarry, plant or works becoming an
industry is not a claim that the business exists, operates or produces anything.

**Klajdovka** is the epistemic test. In OpenStreetMap it is the nature monument *přírodní památka
Velká Klajdovka* (`relation/2331971`, `leisure=nature_reserve`, `boundary=protected_area`), a hotel,
a restaurant and a hiking guidepost; nothing named Klajdovka is mapped as a peak, viewpoint, mine or
quarry. The snapshot keeps it as `protected_area` (OBSERVED); the rules map that class to a sign only,
and `tests/pipeline.rs` fails if a landmark or protected area ever becomes an industry or if anything
named Klajdovka is one. The real extraction sites nearby are separate features with their own ids
(for example `relation/10870019`, a sand pit, `way/591315837`, a stone quarry). No temperate industry
extracts or accepts aggregate or clay, so each becomes a HYPOTHETICAL coal-mine stand-in anchored at
the site and evidenced by nothing; `disused`/`abandoned` quarries are skipped.

## Sources and terms

Every provider and layer is a row of one table, `tools/research/real-world/providers.v1.json`
(compiled into the tool): endpoint, licence, attribution, allowed purpose, retention,
redistribution, when its terms were read. Sources are built from it and a snapshot is validated by
layer kind (`geocoder`, `vector`, `elevation`), never by provider id. Any live place search goes to
Nominatim under its usage policy, which applies to every use of this tool:
<https://operations.osmfoundation.org/policies/nominatim/>.

Checked on 2026-11-18 (this session, plus the reading of peer session openttd-80 where stated).

| Source | Use | Terms that bind the pipeline |
|---|---|---|
| Nominatim (`nominatim.openstreetmap.org`) | place candidates | [usage policy](https://operations.osmfoundation.org/policies/nominatim/): at most 1 request a second, an identifying User-Agent (generic: `openttd-real-world-research/0.1 (research fork; rw-scenario)`, no person named and no URL: the only project URL carries the owner's personal handle, which the brief keeps out of every request), results cached, no bulk, no autocomplete, no systematic queries. Data ODbL 1.0. |
| Overpass API (`overpass-api.de`) | vector features | [fair use](https://wiki.openstreetmap.org/wiki/Overpass_API): far below 10 000 queries and 1 GB a day (a hundredth for regular use), no parallel scripts, back off 30 s after HTTP 429. One bounded query per snapshot. Data ODbL 1.0, © OpenStreetMap contributors. |
| Copernicus DEM GLO-30 (Copernicus WorldDEM™-30), AWS Open Data bucket `copernicus-dem-30m` | elevation grid | [registry entry](https://registry.opendata.aws/copernicus-dem/): public, no AWS account. COP-DEM-GLO-30-F licence (free and open; Art. 6(a)-(e) notices). Read with anonymous HTTP range requests: the TIFF directories and only the internal 1024x1024 tiles the grid touches, cached. Every attribution carries the Art. 6(b) notice and the Art. 6(c) liability sentence verbatim, a non-endorsement sentence (6(d)) that also names the OpenStreetMap Foundation, and that Subsequent Users are bound by the same obligations (6(e)). A Digital Surface Model: buildings and canopy are in it. |
| Google Maps | none | excluded: its terms forbid creating content or terrain models from Google Maps content. |

Licensing of the outputs: the snapshot and every compiled artifact that carries names or positions
(towns, industries, signs, `towns.json`, `scenario.json`, the GameScript's data) are derived from
OpenStreetMap and are an ODbL 1.0 derivative database, not covered by the repository's code licence.
`fixtures/brno/LICENSE-DATA.txt` and each output's `ATTRIBUTION.txt` say so with the full attribution.
Raw provider answers stay in the ignored cache (`tools/research/real-world/.cache/`, each with the
time it was fetched); the snapshot keeps their SHA-256 only.

The Brno snapshot was acquired live on 2026-11-18 (2 Nominatim and 2 Overpass requests; the
first version also sent 20 Open-Meteo requests, four answered HTTP 429 with a 30 s back-off). On
2026-11-19 the elevation layer moved to Copernicus GLO-30: 5 anonymous HTTP range requests to
`copernicus-dem-30m.s3.amazonaws.com` read the header and 4 of the 16 internal tiles of
`Copernicus_DSM_COG_10_N49_00_E016_00_DEM` for a 129x129 grid; Nominatim and Overpass answered
from the cache, and a second acquisition sent no request at all. The
test of an ambiguous place uses one stored Nominatim answer for "Springfield"
(`fixtures/springfield/`), ten US settlements of comparable importance: rule
`dominant-settlement/v1` refuses to choose.

### Decision: elevation from Copernicus GLO-30 (owner's answers)

The first version used the Open-Meteo elevation API (Copernicus GLO-90 behind a non-commercial API).
The owner decided, twice, for the source policy's Copernicus GLO-30 from the anonymous AWS bucket.
Recorded verbatim:

1. Session b841bd49, 2026-11-19. Q: 'Odkud má pipeline pro reálné scénáře brát výšková data? Teď je
   to Open-Meteo (Copernicus GLO-90, API jen pro nekomerční použití, vzorkováno po ~642 m). Zdrojová
   politika doporučuje Copernicus GLO-30 z anonymního AWS bucketu.' A: 'Přejít na GLO-30
   (Recommended)'.
2. To session cd8d20a8, 2026-11-18 ~14:50. Q: 'The real-world scenario slice (session openttd-07)
   uses Open-Meteo for elevation (Copernicus GLO-90 behind a non-commercial API, sampled at about
   642 m) instead of the source policy's Copernicus GLO-30 from AWS (30 m, no account, open licence).
   Which should it use?' A: 'Copernicus GLO-30 (Recommended)'.

The provider registry has one elevation row, `copernicus-glo30-aws`. The reader (`src/cog.rs`) handles
what these files are: classic little-endian TIFF, tiled, one 32-bit float band, DEFLATE with the
floating-point predictor; anything else is refused with its tag value.

## The panel

The panel serves the one compiler, linked as a library (`tools/research/mcp-rhai/src/panel_realworld.rs`),
so every surface derives from the route table: `GET /api/realworld`, `GET /api/realworld/scenario?name=`,
`POST /api/realworld/compile` are in OpenAPI, Swagger UI, the hub's catalogue and GraphQL, and Rhai.
`/realworld` lists every snapshot; `/realworld/<name>` is a page per place (title and description from
its snapshot, server-side): the source terrain with every feature, entity and edge on one ECharts map
by evidence class (the legend filter and the selection live in the address, maximize with Escape),
the selected entity's rationale, rule and its OpenStreetMap record, the entity table, sources and
licences, the resolution, the chains, lost detail and diagnostics, and the button that writes the
artifacts. `preview.svg` is the static counterpart written with the artifacts, for review without a
server.

## Limits

- Elevation is GLO-30 at full resolution (1 arc-second, about 30 m by 20 m at Brno's latitude),
  sampled bilinearly on a 129x129 grid (about 196 m apart for Brno) and resampled to 256x256 tiles of
  about 98 m; a DSM, so it includes buildings and canopy. Inland water cannot be represented
  (OpenTTD heightmaps have water only at height 0); rivers, roads, railways and buildings are not
  built.
- The map is a game, not a survey: the engine smooths slopes, the GameScript displaces a town or
  industry to the nearest buildable tile in a ring (recorded), towns grow their own roads.
- The economy uses the default temperate industries only; roles are checked against the running game,
  NewGRF industry sets are not mapped.
- Population and production are balance assumptions. Proximity edges are not observed flows.
- Hashes are recorded on macOS arm64. The compiler uses `f64` `cos`, `powf`, `hypot` and division
  from the platform's maths library, whose last bits may differ elsewhere; another host may need to
  re-record `fixtures/brno/SHA256SUMS` (`replay --expect` says so) while its output stays valid.
- Placement inside the game is deterministic for a seed; a game saved before the GameScript finished
  placing is not marked placed and places again on reload.
- Settlements closer than `min_spacing_tiles` are merged into the larger one, and at most
  `max_towns` are kept; signs are capped per class and spaced, so a small landmark such as Velká
  Klajdovka can be in the snapshot and not on the map (it is, in this slice).

## Decision: the slice is committed

The brief forbade a commit without a fresh owner instruction. Two answers gave it:

1. To session cd8d20a8, 2026-11-19 ~04:00 local, relayed by it: asked what to do to get 3-7
   merges, the owner chose, among others, "Commit real-world slice" ("Let openttd-07 commit and push
   its reviewed real-world scenario slice to OpenTTD master (it was told 'no commit'); one open
   MEDIUM (site-box flag) stays recorded.").
2. In session b841bd49 (AskUserQuestion, 2026-11-19): "Mám teď commitnout a pushnout slice pro
   reálné scénáře (Brno) na OpenTTD master? Brief původně commit zakazoval. Stav: cargo test 28/28,
   load-check exit 0 (17/17 podniků, štítky 31/31), tři nezávislé revize opravené. NOT RUN zůstane:
   testy mcp-rhai a gate.sh change (disk 28–33 GiB, cold build by ho stáhl pod společný limit 35
   GiB)." Answer: "Ano, commit a push (Recommended)".

The site-box flag the first answer calls open was done before the commit (see the evidence below).

## Evidence of the Brno slice (2026-11-19, GLO-30 terrain)

OBSERVED with the research build of this checkout's master (`build-gate-research/openttd`,
RelWithDebInfo, `OPTION_RESEARCH_INSTRUMENTATION=ON`; a research build, not a release build) and
`load-check --seed 31337`, exit 0:

- The map loads headless from `heightmap.png` (256x256, clockwise rotation) and the GameScript runs:
  every industry role resolved by cargo labels.
- Towns 14/14, industries 17/17, signs 24/24; every placed town and industry carries its in-game
  note (31/31 labelled: evidence class, source records, rule, and the move from its anchor when
  there is one).
- The one town the engine adds itself is renamed "Generated town 0" and labelled HYPOTHETICAL
  (`engine_generated` in `loadcheck.json`; it was "Brentwood").
- Save, reload, and the GameScript resumes without placing again: towns, industries and signs are
  unchanged after the reload (`reload.unchanged = true`).
- Six industries were placed more than 8 tiles from their anchor and are flagged in
  `displaced_beyond_threshold`: the factory stand-in at `way/54843056` (33 tiles), the forests
  `relation/10553677` (29), `relation/116315` (24) and `relation/10553542` (9), the coal-mine
  stand-in at `way/591315837` (11) and the power station `way/53367307` (9). On the 30 m terrain
  industries search up to 40 tiles from their anchor, skipping tiles off the map or whose 4x4
  neighbourhood is not buildable before any command is tried; the distance in each note and in
  `loadcheck.json` is measured from the location the game reports, not from the offset tried.
  A placement line `load-check` cannot read fails the run (an earlier run wrapped an industry
  across the map edge and the parser skipped it silently).
- Each anchored entity carries its anchor's mapped extent in tiles (`site`, the feature's bounding
  box, clamped to the map where the map cuts it), so a move is judged against the site's size, not
  only the flat 8-tile threshold. In the game note and in `loadcheck.json`
  (`relocated_outside_site`), an entity is "still within its mapped site" when it stays inside that
  box plus one tile. In this run the large forests `relation/10553677` (29 tiles), `relation/116315`
  (24) and `relation/10553542` (9) stayed within their sites, as the independent review measured;
  four left theirs: the factory stand-in `way/54843056` (33), the coal-mine stand-in
  `way/591315837` (11), the power station `way/53367307` (9) and the sawmill `way/769231272` (6).
  The panel's headless-load card lists them as "off site".
- History: the first run (Open-Meteo terrain) placed 17/18 industries; the cause, found by the
  independent review and not by this author, was the default
  `economy.multiple_industry_per_town = false`, which the compiler now sets to `true`.

Two defects were found by running it, not by reading: a repeated `[section]` header in the
generated config silently dropped the heightmap rotation and the town count, and a script file
without a byte-order mark garbled every non-ASCII name. Both are fixed and covered (the config is
grouped by section, the GameScript starts with a UTF-8 BOM and a test checks it).

## Decision: closed with three items NOT RUN, two of them run since

Asked in session b841bd49 (AskUserQuestion, 2026-11-20): "Disk has 16 GiB free, below the shared
35 GiB floor. Every remaining step of the real-world slice needs a new build: gate.sh change, the
live second-place run, and the MCP plain-text fix. Deleting the OpenTTD build dirs only frees about
2.5 GiB; the big consumers are prismatic-platform (68 GiB), v6-wt (15), oscilla-wt (12),
majordomus-reel (12) and Library/Caches (8). How should I proceed?" Answer: "Close with NOT RUN
(Recommended)".

So the slice closes with these NOT RUN, each because it needs a build the disk floor forbade:

- `tools/gate.sh change` on the slice (a cold research build of OpenTTD).
- A live acquisition and `load-check` of a second place. Place independence is shown only by the
  synthetic-snapshot test in `tests/pipeline.rs`, not by real data.
- The schema check of the snapshot by an external validator (`uvx jsonschema` was refused earlier);
  the snapshot is checked by the crate's own tests only.

Two of the three were run afterwards, on 2026-11-22, when the owner asked in session b841bd49 to
finish everything ("ano, vse dotahni / porad se s ostatnimi llms/ais/nodes/peers"). This Mac was
still under the floor, so they ran on lundra, a tailnet build host. The run used Linux Mint 22.3
with GCC and CMake there, and a detached worktree of its own under `~/ow07`, outside lundra's
checkout. The third, the external schema validator, is still NOT RUN.

The build was dedicated (`-DOPTION_DEDICATED=ON`), because lundra has no SDL3 or Allegro, so the
GUI was not compiled. The liblzma headers came from `apt-get download liblzma-dev`, unpacked in user
space, with no package installed, so the regression saves load. A research build of the slice state
then ran these checks:

- **`tools/gate.sh change` and `tools/gate.sh research`: PASS (exit 0).** The tree under test was
  2e7c376c64^ with the slice (2e7c376c64, e93942533b, 87f6a84251) as an uncommitted change, plus
  the gate's base-set probe fix (a2721e5f9c), plus the load-check fix below. ctest passed 98/98
  including `regression_*`, the research ctest passed 123/123, `[prismatic_mcp]` passed 796
  assertions, the crate's tests passed and the Brno replay matched.
- **A live second place: PASS. No core code or rule was changed for it.**
  - `resolve --live --query "Olomouc, Czech Republic"` found 3 candidates and chose the settlement
    `osm:relation/437057`. The two administrative districts of the same name were listed, not
    chosen.
  - `acquire --live` wrote 275 features. It sent 3 range requests to the GLO-30 bucket and 1 to
    Overpass; Nominatim was answered from the cache.
  - `compile` built a 256x256 map at 75.742 m per tile with 14 towns (DERIVED) and 13 industries
    (7 DERIVED, 1 INFERRED, 5 HYPOTHETICAL). It made 21 landmark signs and 32 edges (12 INFERRED,
    20 HYPOTHETICAL) over 6 chains, with no isolated node.
  - `replay` matched the hashes.
  - `load-check --seed 31337 --baseset ~/.local/share/openttd/baseset` exited 0:
    - towns 14/14, industries 13/13 and signs 21/21 were placed, and 27/27 carry their note;
    - the reload was unchanged and the GameScript resumed;
    - the one town the engine adds was renamed and labelled ("Generated town 0");
    - three entities moved more than 8 tiles from their anchor: `town:osm:node/1599306066` (14),
      the factory stand-in at `way/152803381` (12) and the forest `relation/21438978` (24);
    - two factory stand-ins left their mapped site.

Running it found two defects, both fixed:

- `tools/gate.sh` piped `tar -tf` into `grep -q` under pipefail. On GNU tar that sometimes
  reported a present OpenGFX tar as missing, so the regression cases were skipped. It was fixed in
  a2721e5f9c.
- `load-check` put the GameScript and base set only where macOS looks. With `-c` the config file's
  directory is the personal directory on every platform, so both now go there as well. That fix was
  not re-run on macOS.

## Validation

What ran, with exit codes, is recorded in the task's report; never read a row here as a pass that
was not run.
