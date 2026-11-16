# World View: the game's world in the external console

RESEARCH-ONLY. The hub (`research/mcp-hub.md`) serves `/viz`, a page built only on
`tools/research/mcp-panel/static/components.js` and the hub's WebSocket. Everything on it is read
live from one instance's own MCP resources through `hub.call`:

- `openttd://world/graph`
- `openttd://map`
- `openttd://map/ways`
- `openttd://companies`

Nothing on the page is a copy of game data. Parameters live in the address
(`#instance=…&mode=…&kinds=…&exaggeration=…&max=…`), so a configured view is a link.

| Card | Library | What it shows | Parameters |
|---|---|---|---|
| World graph | ECharts (graph, `layout: 'none'`) | Towns, industries, stations and vehicles (world-graph/v1), with `serves_town`, `near_town`, `order` and `cargo_link` edges. Cargo links are as wide as their usage. Node size is √degree. Labels appear only on the highest-degree nodes. A click shows the node and its edges, plus "Show in game", which centres the owner's game camera on it through the GUI client's `openttd.view.scroll_to`. | `mode`: on the map (the default, never a hairball), or a force layout computed by D3 (`d3-force`, deterministic start); `kinds` |
| Terrain and roads | three.js | The map in 3D from tile heights, coloured by tile type, with every tile that has a road way (from `openttd://map/ways`) drawn as road. Orbit with the mouse. | `exaggeration` (1–20) |
| Economy | ECharts | Each company's money over time while the page is open (one sample every 5 s; Ctrl+wheel zooms); the stations with the most waiting cargo | — |

Every card maximizes to the viewport with its button. Escape restores it, and the address carries
`max`.

The chart layer is shared through `components.js`:

- **`ecInit`** is the one door. It refuses a box with no size, takes the chart's description from
  the box's `aria-label`, animates unless the reader asked for reduced motion, and follows the
  box's size.
- **`maximizable`** provides the maximize frame.
- **Option builders** are pure functions: `worldGraphOption`, `waitingOption`, `moneyOption`.
- **`PALETTE`** is the validated 8-slot palette.

The builders are tested with Node (`node --test tools/research/mcp-panel/test/components.test.mjs`).

Libraries come from the panel's own `node_modules` (`npm ci` in `tools/research/mcp-panel`): echarts
6, three 0.187, d3 7.9. The hub serves them on an allow-listed route, `/vendor/npm/<package>/…`
(echarts, three, d3, flowbite; no `..`). Nothing is fetched from a CDN, and no copy is committed.

```bash
cd tools/research/mcp-panel && npm ci && cd -
tools/research/mcp-rhai/target/release/mcp-hub serve --port 8790   # then open http://127.0.0.1:8790/viz
```

## Verified, and not

- **Verified (2026-11-16):**
  - The Node tests pass (13/13).
  - ECharts server-side rendering on the live world graph of a running game (72 nodes, 58 edges)
    drew the graph (197 paths, 28 labels) and the waiting bars.
  - The hub served `/viz`, three.js and d3, and refused `/vendor/npm/../package.json`.
  - `viz.js` parses.
- **NOT RUN:** the page in a real browser, including the WebGL terrain, the clicks, maximize and
  Escape. The browser extension was not connected in that session.
