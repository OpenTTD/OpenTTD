/* Real-world places in the panel (research/real-world-scenario.md): /realworld lists every place
   snapshot (GET /api/realworld), /realworld/<name> is one place compiled in process by the one
   compiler (GET /api/realworld/scenario): the source terrain, every source feature, every game
   entity and economy edge on one map, coloured by evidence class, the entity under the cursor
   explained (rationale, rule, confidence, its source record), and the artifacts written with a
   click (POST /api/realworld/compile). The map goes through the one door (components.js ecInit), is
   a row of CHARTS, keeps its filters and selection in the address and maximizes (Escape restores). */

import { ecInit, maximizable, UI, CLS, $, esc, whole, INK, AXIS as axis, patchAddress, badge } from './components.js';

/** The page's echarts, read when the map is drawn (a test installs its fake after importing). */
const echartsNow = () => globalThis.echarts;

/** The evidence classes, in the order the contract defines them, and the colour of each. */
export const EVIDENCE = ['OBSERVED', 'DERIVED', 'INFERRED', 'HYPOTHETICAL'];
export const EV_COLOUR = { OBSERVED: '#9ca3af', DERIVED: '#3987e5', INFERRED: '#c98500', HYPOTHETICAL: '#e66767' };
const EV_TONE = { OBSERVED: 'gray', DERIVED: 'blue', INFERRED: 'yellow', HYPOTHETICAL: 'red' };
const SYMBOL = { town: 'circle', industry: 'rect', sign: 'triangle' };

/** The registry: every chart of the real-world pages, the address keys it reads and where a click goes. */
export const CHARTS = [
  { id: 'rw-map', page: '/realworld/<name>', kind: 'custom+scatter+lines', params: ['hide', 'sel', 'max'], drill: 'the clicked entity or feature, then its source record' },
];

/* ---- address state ------------------------------------------------------------------------- */
export function getState(href = globalThis.location?.href) {
  const p = new URL(href).searchParams;
  const hide = (p.get('hide') || '').split(',').filter((e) => EVIDENCE.includes(e));
  return { hide, sel: p.get('sel') || '', q: p.get('q') || '', max: p.get('max') || undefined };
}
export function setState(patch) { patchAddress(patch); }

/** The OpenStreetMap page of a source record id `osm:<type>/<id>`. */
export function osmUrl(id) {
  const m = /^osm:(node|way|relation)\/(\d+)$/.exec(id || '');
  return m ? `https://www.openstreetmap.org/${m[1]}/${m[2]}` : null;
}

/** Tile of a WGS84 point in the extent (the compiler's local-eqr/v1 and clockwise rotation). */
export function tileOf(extent, at) {
  const o = extent.crs.origin;
  const R = extent.crs.radius_m;
  const rad = Math.PI / 180;
  const x = R * (at.lon - o.lon) * rad * Math.cos(o.lat * rad);
  const y = R * (at.lat - o.lat) * rad;
  const h = extent.side_m / 2;
  return { x: ((h - y) / extent.side_m) * extent.map_size, y: ((x + h) / extent.side_m) * extent.map_size };
}

/* ---- the map option (pure: data in, option out) -------------------------------------------- */
export function mapOption(d, { hide = [], sel = '' } = {}) {
  const sc = d.scenario;
  const n = sc.extent.map_size;
  const g = d.terrain.grid;
  const m = d.terrain.metres;
  const lo = Math.min(...m.filter((v) => v != null));
  const hi = Math.max(...m.filter((v) => v != null));
  const cells = m.map((v, i) => [i % g, Math.floor(i / g), v]).filter((c) => c[2] != null);
  const step = n / (g - 1);
  const byId = new Map(sc.entities.map((e) => [e.id, e]));
  const at = (t) => [t.y + 0.5, t.x + 0.5]; // x axis: image column (tile y); y axis: image row (tile x)
  const shown = (ev) => !hide.includes(ev);
  const series = [
    {
      id: 'terrain', name: 'Terrain (source elevation)', type: 'custom', silent: true, z: 0,
      renderItem: (params, api) => {
        const [c, r, v] = [api.value(0), api.value(1), api.value(2)];
        const p0 = api.coord([Math.max(0, (c - 0.5) * step), Math.max(0, (r - 0.5) * step)]);
        const p1 = api.coord([Math.min(n, (c + 0.5) * step), Math.min(n, (r + 0.5) * step)]);
        const k = hi > lo ? (v - lo) / (hi - lo) : 0.5;
        const l = Math.round(40 + k * 150);
        return { type: 'rect', shape: { x: p0[0], y: p0[1], width: p1[0] - p0[0], height: p1[1] - p0[1] }, style: { fill: `rgb(${l},${l + 8},${l})` } };
      },
      data: cells,
    },
  ];
  if (shown('OBSERVED')) {
    series.push({
      id: 'features', name: 'OBSERVED', type: 'scatter', symbolSize: 4, z: 2, cursor: 'pointer', color: EV_COLOUR.OBSERVED,
      data: d.features.map((f) => { const t = tileOf(sc.extent, f.at); return { value: [t.y, t.x], name: f.id, label: `${f.class} ${f.name || ''}`.trim() }; }),
    });
  }
  const edges = sc.economy.edges.filter((e) => shown(e.evidence));
  series.push({
    id: 'edges', name: 'edges', type: 'lines', coordinateSystem: 'cartesian2d', z: 3, silent: false,
    data: edges.map((e) => ({
      name: e.id, coords: [at(byId.get(e.from).tile), at(byId.get(e.to).tile)],
      lineStyle: { color: EV_COLOUR[e.evidence], width: e.cargo === 'PASS' ? 1 : 2, type: e.evidence === 'HYPOTHETICAL' ? 'dashed' : 'solid', opacity: 0.85 },
      cargo: e.cargo,
    })),
  });
  for (const ev of EVIDENCE.slice(1)) {
    if (!shown(ev)) continue;
    series.push({
      id: `entities-${ev}`, name: ev, type: 'scatter', z: 4, cursor: 'pointer', color: EV_COLOUR[ev],
      label: { show: true, position: 'right', color: INK.text, fontSize: 10, formatter: (p) => (p.data.kind === 'industry' ? '' : p.data.label) },
      data: sc.entities.filter((e) => e.evidence === ev).map((e) => ({
        value: at(e.tile), name: e.id, label: e.name, kind: e.game.kind,
        symbol: SYMBOL[e.game.kind] || 'circle', symbolSize: e.id === sel ? 18 : e.game.kind === 'town' ? (e.city ? 14 : 10) : 9,
        itemStyle: e.id === sel ? { borderColor: '#fff', borderWidth: 2 } : undefined,
      })),
    });
  }
  return {
    backgroundColor: 'transparent',
    tooltip: { trigger: 'item', formatter: (p) => esc(p.data?.label ? `${p.data.label} · ${p.seriesName}` : p.data?.cargo ? `${p.data.cargo} · ${p.name}` : p.name || '') },
    legend: { top: 0, textStyle: { color: INK.text }, data: EVIDENCE, selected: Object.fromEntries(EVIDENCE.map((e) => [e, shown(e)])) },
    grid: { left: 8, right: 8, top: 32, bottom: 40, containLabel: true },
    xAxis: { type: 'value', min: 0, max: n, name: 'tile y (west → east)', nameLocation: 'middle', nameGap: 24, nameTextStyle: { color: INK.muted }, ...axis, splitLine: { show: false } },
    yAxis: { type: 'value', min: 0, max: n, inverse: true, name: 'tile x (north → south)', nameTextStyle: { color: INK.muted }, ...axis, splitLine: { show: false } },
    dataZoom: [{ type: 'inside', xAxisIndex: 0, filterMode: 'none', zoomOnMouseWheel: 'ctrl', moveOnMouseWheel: false }, { type: 'inside', yAxisIndex: 0, filterMode: 'none', zoomOnMouseWheel: 'ctrl', moveOnMouseWheel: false }],
    series,
  };
}

/** The entities a search keeps: name, id, role, kind or evidence containing the text. */
export function filterEntities(entities, q) {
  const t = (q || '').trim().toLowerCase();
  if (!t) return entities;
  return entities.filter((e) => [e.name, e.id, e.game.kind, e.game.role, e.evidence].some((v) => String(v || '').toLowerCase().includes(t)));
}

/* ---- the pages ------------------------------------------------------------------------------- */
const stat = (label, v, sub, href) => `<div class="${CLS.card} mb-0"><p class="text-sm text-gray-500 dark:text-gray-400">${esc(label)}</p>
  <p class="mt-1 ${UI.cardTitle} sm:text-2xl">${href ? `<a class="hover:underline" href="${esc(href)}">${esc(v)}</a>` : esc(v)}</p><p class="text-xs text-gray-500 dark:text-gray-400">${esc(sub)}</p></div>`;
const evBadge = (ev) => badge(ev, EV_TONE[ev] || 'gray');
const api = (op) => `<a href="/docs#/default/${op}" class="text-sm text-primary-600 hover:underline dark:text-primary-500">API</a>`;

/** /realworld; exported so a test can mount it in a fresh document (test/realworld.test.mjs). */
export async function listPage() {
  const d = await (await fetch('/api/realworld')).json();
  const rows = (d.scenarios || []).map((s) => {
    const ent = Object.entries(s.compiled?.entities || {}).map(([k, v]) => `${v} ${k}`).join(', ');
    return `<tr class="border-b dark:border-gray-700">
      <td class="px-3 py-2 whitespace-nowrap"><a href="/realworld/${esc(s.name)}" class="font-medium text-primary-600 hover:underline dark:text-primary-500">${esc(s.query)}</a></td>
      <td class="px-3 py-2">${esc(s.chosen_name || s.chosen || 'ambiguous')}${s.ambiguous ? ' ' + badge('AMBIGUOUS', 'red') : ''}</td>
      <td class="px-3 py-2">${esc(s.map_size)}² · ${esc(s.metres_per_tile)} m</td>
      <td class="px-3 py-2">${esc(s.features)}</td>
      <td class="px-3 py-2">${esc(s.compiled?.error || ent)}</td></tr>`;
  }).join('') || '<tr><td class="px-3 py-2" colspan="5">no place snapshot yet</td></tr>';
  $('page').innerHTML = `<div class="${CLS.card}">
    <div class="${UI.cardHead} mb-3"><h2 class="${UI.cardHeadTitle} ${UI.cardTitle}">Places <span class="${UI.cardSub}">(${esc(d.count ?? 0)})</span></h2>${api('realWorld')}</div>
    <p class="mb-3 text-sm text-gray-500 dark:text-gray-400">Each is a snapshot under tools/research/real-world/fixtures, compiled offline with ${esc(d.rules || 'the rules')}. A new place: <code>tools/research/real-world/rw-scenario acquire --live --query "…" --out …</code></p>
    <div class="${UI.tableWrap}"><table class="${UI.table}"><thead class="${UI.thead}"><tr><th class="px-3 py-2">Query</th><th class="px-3 py-2">Resolved to</th><th class="px-3 py-2">Map</th><th class="px-3 py-2">Features</th><th class="px-3 py-2">Game entities by evidence</th></tr></thead>
    <tbody id="rw-rows">${rows}</tbody></table></div></div>`;
}

/** The last `rw-scenario load-check` of this place: placed counts, the reload check, the engine's
 *  own towns (HYPOTHETICAL: not from any source) and every placement moved beyond the threshold. */
export function loadHtml(l) {
  if (!l) return '<p class="text-sm text-gray-500 dark:text-gray-400">Not loaded yet: <code>tools/research/real-world/rw-scenario load-check --out build-real-world/&lt;name&gt; --openttd &lt;binary&gt;</code></p>';
  const p = l.placed || {};
  const frac = (k) => (p[k] ? `${p[k][0]}/${p[k][1]}` : '-');
  const r = l.reload || {};
  return `<p class="text-sm text-gray-700 dark:text-gray-300">Placed: towns ${esc(frac('towns'))}, industries ${esc(frac('industries'))}, signs ${esc(frac('signs'))} ·
    reload ${r.unchanged ? badge('unchanged', 'green') : badge(r.loaded ? 'CHANGED' : 'not run', 'red')}</p>
    <ul class="mt-2 space-y-1 text-sm text-gray-700 dark:text-gray-300">${(l.engine_generated || []).map((t) => `<li>${badge('HYPOTHETICAL', 'red')} ${esc(t.name)} (engine's "${esc(t.engine_name)}") at ${esc(t.tile.x)},${esc(t.tile.y)}, population ${esc(t.population)}: engine-generated, not a real place</li>`).join('')}
    ${(l.displaced_beyond_threshold || []).map((x) => `<li>${badge('moved', 'yellow')} <a class="hover:underline" href="?sel=${encodeURIComponent(x.id)}" data-sel="${esc(x.id)}">${esc(x.id)}</a> ${esc(x.tiles)} tiles (threshold ${esc(l.displacement_threshold_tiles)})</li>`).join('')}
    ${(l.relocated_outside_site || []).map((x) => `<li>${badge('off site', 'red')} <a class="hover:underline" href="?sel=${encodeURIComponent(x.id)}" data-sel="${esc(x.id)}">${esc(x.id)}</a> relocated outside its mapped site, ${esc(x.tiles)} tiles from its anchor</li>`).join('')}
    ${(l.failures || []).map((x) => `<li>${badge('failed', 'red')} ${esc(String(x).split('rw: ')[1] || x)}</li>`).join('')}</ul>`;
}

function detailHtml(d, id) {
  const e = d.scenario.entities.find((x) => x.id === id);
  const f = d.features.find((x) => x.id === id || (e && x.id === (e.evidence_feature || e.anchor_feature)));
  if (!e && !f) return `<p class="text-sm text-gray-500 dark:text-gray-400">Click a mark on the map or a row below.</p>`;
  const anchor = e && !e.evidence_feature && e.anchor_feature ? `<p class="mt-2 text-sm">Anchored at <a class="text-primary-600 hover:underline dark:text-primary-500" href="${esc(osmUrl(e.anchor_feature) || '#')}" rel="noopener">${esc(e.anchor_feature)}</a> as a gameplay stand-in: that record is not evidence for this industry.</p>` : '';
  const src = f && (!e || e.evidence_feature) ? `<p class="mt-2 text-sm">Evidence record: <a class="text-primary-600 hover:underline dark:text-primary-500" href="${esc(osmUrl(f.id) || '#')}" rel="noopener">${esc(f.id)}</a> ${evBadge('OBSERVED')} · ${esc(f.class)} · ${esc(Object.entries(f.tags).map(([k, v]) => `${k}=${v}`).join(', '))}</p>` : '<p class="mt-2 text-sm">No source record: invented for playability.</p>';
  if (!e) return `<h3 class="${UI.cardTitle}">${esc(f.name || f.id)}</h3>${src}<p class="mt-2 text-sm text-gray-500 dark:text-gray-400">Not a game entity (filtered, merged, outside the map, or a class the rules do not build).</p>`;
  const edges = d.scenario.economy.edges.filter((x) => x.from === e.id || x.to === e.id);
  return `<h3 class="${UI.cardTitle}">${esc(e.name)} ${evBadge(e.evidence)}</h3>
    <p class="mt-2 text-sm text-gray-700 dark:text-gray-300">${esc(e.game.kind)}${e.game.role ? ' · ' + esc(e.game.role) : ''} at tile ${esc(e.tile.x)},${esc(e.tile.y)} · confidence ${esc(e.confidence)} · rule ${esc(e.rule)}</p>
    <p class="mt-2 text-sm text-gray-700 dark:text-gray-300">${esc(e.rationale)}</p>
    ${e.population_basis ? `<p class="mt-2 text-sm text-gray-500 dark:text-gray-400">Population ${esc(whole.format(e.population))}: ${esc(e.population_basis)}</p>` : ''}
    ${e.source_label ? `<p class="mt-2 text-sm text-gray-500 dark:text-gray-400">Source label: ${esc(e.source_label)}</p>` : ''}
    ${src}${anchor}
    <ul class="mt-2 space-y-1 text-sm text-gray-700 dark:text-gray-300">${edges.map((x) => `<li>${evBadge(x.evidence)} ${esc(x.cargo)} ${x.from === e.id ? '→' : '←'} <a class="hover:underline" href="?sel=${encodeURIComponent(x.from === e.id ? x.to : x.from)}" data-sel="${esc(x.from === e.id ? x.to : x.from)}">${esc(d.scenario.entities.find((y) => y.id === (x.from === e.id ? x.to : x.from))?.name || '')}</a> · ${esc(x.basis)}</li>`).join('')}</ul>`;
}

/** /realworld/<name>; exported for the same reason. */
export async function placePage(name) {
  const state = getState();
  const r = await fetch('/api/realworld/scenario?name=' + encodeURIComponent(name));
  if (!r.ok) { $('page').innerHTML = `<div class="${CLS.card}">No such place. <a class="underline" href="/realworld">All places</a>.</div>`; return; }
  const d = await r.json();
  const sc = d.scenario;
  $('crumbs')?.insertAdjacentHTML('beforeend', `<li class="text-gray-400">/</li><li class="text-gray-500 dark:text-gray-400">${esc(name)}</li>`);
  const cand = d.resolution.candidates.find((c) => c.id === d.resolution.chosen);
  const ok = !d.validation.snapshot.length && !d.validation.scenario.length;
  $('page').innerHTML = `
    <div class="grid grid-cols-2 gap-3 mb-4 md:grid-cols-4">
      ${stat('Place', sc.query, cand ? cand.display_name : 'ambiguous', osmUrl(d.resolution.chosen))}
      ${stat('Map', `${sc.extent.map_size}×${sc.extent.map_size}`, `${sc.extent.metres_per_tile} m a tile · ${(sc.extent.side_m / 1000).toFixed(1)} km a side`)}
      ${stat('Terrain', `${sc.heightmap.min_elevation_m}–${sc.heightmap.max_elevation_m} m`, `${sc.heightmap.levels} height levels · ${sc.heightmap.metres_per_level} m each`)}
      ${stat('Contract', ok ? 'valid' : 'INVALID', `${sc.entities.length} entities · ${sc.economy.edges.length} edges · ${sc.economy.isolated.length} isolated`)}
    </div>
    <div id="rw-map-card" data-maximize="rw-map" data-chart="rw-map" class="${CLS.card} flex flex-col">
      <div class="${UI.cardHead} mb-3">
        <div class="${UI.cardHeadTitle}"><h2 class="${UI.cardTitle}">Terrain, features, entities and links</h2><p class="mt-2 ${UI.cardSub}">Source elevation as the ground; grey dots are source features, marks are game entities (circle town, square industry, triangle landmark), lines economy edges; the legend filters by evidence class; Ctrl+wheel zooms</p></div>
        <div data-toolbar class="flex flex-wrap items-center gap-2">${api('realWorldScenario')}</div>
      </div>
      <div id="rw-map" role="img" aria-label="The place's terrain with its source features, game entities and economy edges, coloured by evidence class" class="w-full h-96 sm:h-[32rem] grow"></div>
    </div>
    <div class="grid gap-4 xl:grid-cols-2">
      <div class="${CLS.card}"><h2 class="mb-3 ${UI.cardTitle}">Selected</h2><div id="rw-detail" aria-live="polite"></div></div>
      <div class="${CLS.card}">
        <div class="${UI.cardHead} mb-3"><h2 class="${UI.cardHeadTitle} ${UI.cardTitle}">Artifacts</h2>${api('realWorldCompile')}</div>
        <p class="mb-3 text-sm text-gray-500 dark:text-gray-400">heightmap.png, towns.json, the RealWorldScenario GameScript, openttd.cfg, preview.svg, scenario.json and SHA256SUMS, written to build-real-world/${esc(name)}/. Offline and deterministic.</p>
        <button id="rw-compile" type="button" class="${UI.primary}">Write the artifacts</button>
        <pre id="rw-compile-out" class="mt-3 p-3 max-h-64 overflow-auto text-xs whitespace-pre-wrap bg-gray-50 rounded-lg dark:bg-gray-900 text-gray-800 dark:text-gray-200"></pre>
        <img alt="The compiled heightmap as OpenTTD reads it (grey 1 to 255; north up)" src="${esc(d.heightmap)}" class="mt-3 w-48 h-48 rounded-lg">
      </div>
    </div>
    <div class="${CLS.card}">
      <div class="${UI.cardHead} mb-3"><h2 class="${UI.cardHeadTitle} ${UI.cardTitle}">Game entities <span id="rw-n" class="${UI.cardSub}"></span></h2>
        <input id="rw-q" type="search" value="${esc(state.q)}" placeholder="filter: name, role, evidence" class="${UI.input} w-full sm:w-72"></div>
      <div class="${UI.tableWrap}"><table class="${UI.table}"><thead class="${UI.thead}"><tr><th class="px-3 py-2">Name</th><th class="px-3 py-2">Game</th><th class="px-3 py-2">Evidence</th><th class="px-3 py-2">Confidence</th><th class="px-3 py-2">Source</th></tr></thead>
      <tbody id="rw-rows"></tbody></table></div>
    </div>
    <div class="grid gap-4 xl:grid-cols-2">
      <div class="${CLS.card}"><h2 class="mb-3 ${UI.cardTitle}">Sources, licences and attribution</h2>
        <ul class="space-y-3 text-sm text-gray-700 dark:text-gray-300">${d.sources.map((s) => `<li><span class="font-semibold">${esc(s.provider)}</span> ${badge(s.completeness, s.completeness === 'complete' ? 'green' : 'red')}<br>${esc(s.license)} · retrieved ${esc(s.retrieved_at)} · ${esc(s.requests)} request(s)<br>${esc(s.attribution)}<br><span class="text-gray-500 dark:text-gray-400">${esc(s.terms_checked)}</span></li>`).join('')}</ul></div>
      <div class="${CLS.card}"><h2 class="mb-3 ${UI.cardTitle}">Place resolution</h2>
        <p class="mb-2 text-sm text-gray-700 dark:text-gray-300">${esc(d.resolution.rule)}: ${esc(d.resolution.reason)}</p>
        <ul class="space-y-1 text-sm text-gray-700 dark:text-gray-300">${d.resolution.candidates.map((c) => `<li>${esc(c.rank)}. <a class="hover:underline" href="${esc(osmUrl(c.id))}" rel="noopener">${esc(c.display_name)}</a> · importance ${esc(c.importance)}${c.settlement ? ' · settlement' : ''}${c.id === d.resolution.chosen ? ' ' + badge('chosen', 'green') : ''}</li>`).join('')}</ul></div>
      <div class="${CLS.card}"><h2 class="mb-3 ${UI.cardTitle}">Headless load</h2><div id="rw-load">${loadHtml(d.loadcheck)}</div></div>
      <div class="${CLS.card}"><h2 class="mb-3 ${UI.cardTitle}">Economy chains</h2>
        <ul class="space-y-1 text-sm text-gray-700 dark:text-gray-300">${sc.economy.chains.map((c) => `<li>${esc(c)}</li>`).join('')}</ul></div>
      <div class="${CLS.card}"><h2 class="mb-3 ${UI.cardTitle}">Lost detail and diagnostics</h2>
        <ul class="space-y-1 text-sm text-gray-700 dark:text-gray-300">${sc.lost_detail.map((c) => `<li>${esc(c)}</li>`).join('')}</ul>
        <details class="mt-3 text-sm text-gray-500 dark:text-gray-400"><summary>${esc(sc.diagnostics.length)} compiler diagnostics</summary><ul class="mt-2 space-y-1">${sc.diagnostics.map((c) => `<li>${esc(c)}</li>`).join('')}</ul></details></div>
    </div>`;

  let chart = null;
  const select = (id) => { state.sel = id; setState({ sel: id || undefined }); $('rw-detail').innerHTML = detailHtml(d, id); draw(); };
  const draw = () => {
    chart = chart || ecInit($('rw-map'), echartsNow());
    if (!chart) return;
    chart.setOption(mapOption(d, state), true);
    if (!chart._wired) {
      chart._wired = true;
      chart.on('click', (p) => { if (p.data?.name && p.seriesType === 'scatter') select(p.data.name); });
      chart.on('legendselectchanged', (p) => { state.hide = EVIDENCE.filter((e) => p.selected[e] === false); setState({ hide: state.hide.join(',') || undefined }); draw(); });
    }
    chart.resize();
  };
  const rows = () => {
    const list = filterEntities(sc.entities, state.q);
    $('rw-n').textContent = `(${list.length} of ${sc.entities.length})`;
    $('rw-rows').innerHTML = list.map((e) => `<tr class="border-b dark:border-gray-700">
      <td class="px-3 py-2"><a href="?sel=${encodeURIComponent(e.id)}" data-sel="${esc(e.id)}" class="font-medium text-primary-600 hover:underline dark:text-primary-500">${esc(e.name)}</a></td>
      <td class="px-3 py-2">${esc(e.game.kind)}${e.game.role ? ' · ' + esc(e.game.role) : ''}</td>
      <td class="px-3 py-2">${evBadge(e.evidence)}</td><td class="px-3 py-2">${esc(e.confidence)}</td>
      <td class="px-3 py-2">${e.evidence_feature ? `<a class="hover:underline" href="${esc(osmUrl(e.evidence_feature))}" rel="noopener">${esc(e.evidence_feature)}</a>` : e.anchor_feature ? `none (HYPOTHETICAL stand-in at <a class="hover:underline" href="${esc(osmUrl(e.anchor_feature))}" rel="noopener">${esc(e.anchor_feature)}</a>)` : 'none (HYPOTHETICAL)'}</td></tr>`).join('');
  };
  const onSel = (ev) => { const id = ev.target?.dataset?.sel; if (id) { ev.preventDefault?.(); select(id); } };
  $('rw-rows').addEventListener('click', onSel);
  $('rw-detail').addEventListener('click', onSel);
  $('rw-q').oninput = (ev) => { state.q = ev.target.value; setState({ q: state.q || undefined }); rows(); };
  $('rw-compile').onclick = async () => {
    const b = $('rw-compile');
    b.disabled = true;
    const r2 = await fetch('/api/realworld/compile', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ name }) });
    const o = await r2.json();
    $('rw-compile-out').textContent = r2.ok ? `${o.out}\n${o.sha256sums}\nmatches the recorded hashes: ${o.matches_recorded}\n${o.next}` : (o.error || `HTTP ${r2.status}`);
    b.disabled = false;
  };
  const box = $('rw-map-card');
  box.querySelector('[data-toolbar]').append(maximizable(box, 'rw-map', { getState, setState, onResize: () => draw() }));
  $('rw-detail').innerHTML = detailHtml(d, state.sel);
  rows();
  setTimeout(draw, 30);
}

if (globalThis.document && $('page')) {
  const path = decodeURIComponent(location.pathname);
  if (path === '/realworld') listPage(); else if (path.startsWith('/realworld/')) placePage(path.slice('/realworld/'.length));
}
