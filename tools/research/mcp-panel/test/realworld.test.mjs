// node --test tools/research/mcp-panel/test/ : the real-world pages (static/realworld.js) in the fake
// browser of fake-dom.mjs: /realworld (the list) and /realworld/<name> (the stats, the map through
// the one door, its evidence filter and selection in the address, the entity table, the artifacts
// button). The fixtures are shaped like panel_realworld.rs answers (GET /api/realworld,
// /api/realworld/scenario; POST /api/realworld/compile) with fictional values; nothing here talks
// to a panel or the network.
import './panel-ui.mjs'; // first: shell.js publishes PanelUI before any page script
import test from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import { fakeDom, respond } from './fake-dom.mjs';

// Before the fake browser: ECharts reads navigator when it loads.
const echartsSsr = createRequire(import.meta.url)('echarts');

const extent = { crs: { id: 'local-eqr/v1', description: '', origin: { lat: 45, lon: 7 }, radius_m: 6371008.8 }, side_m: 12000, bbox: { south: 44.94, west: 6.92, north: 45.05, east: 7.08 }, map_size: 128, metres_per_tile: 93.75 };
const ent = (id, name, kind, role, evidence, x, y, extra = {}) => ({
  id, name, game: { kind, role, produces: [], accepts: [] }, tile: { x, y }, evidence_feature: evidence === 'HYPOTHETICAL' ? null : 'osm:node/' + x,
  anchor_feature: 'osm:node/' + x,
  source_label: null, evidence, confidence: 0.5, rationale: `why ${name}`, rule: 'map.x@rw-rules/test/v1', population: null, population_basis: null, city: false, ...extra,
});
const scenario = {
  schema: 'real-world-scenario/v1', query: 'Synthville', chosen: 'osm:relation/1', extent,
  heightmap: { min_elevation_m: 200, max_elevation_m: 260, levels: 5, metres_per_level: 15 },
  entities: [
    ent('industry:coal_mine:osm:way/4', 'Test Quarry', 'industry', 'coal_mine', 'INFERRED', 80, 90),
    ent('industry:power_station:hypothetical-for-coal', 'power_station (hypothetical)', 'industry', 'power_station', 'HYPOTHETICAL', 90, 100),
    ent('sign:osm:node/3', 'Testberg', 'sign', '', 'DERIVED', 79, 88),
    ent('town:osm:node/1', 'Synthville', 'town', '', 'DERIVED', 64, 64, { population: 1200, population_basis: 'OBSERVED population tag', city: true }),
  ],
  economy: { edges: [{ id: 'e1', from: 'industry:coal_mine:osm:way/4', to: 'industry:power_station:hypothetical-for-coal', cargo: 'COAL', distance_tiles: 20, basis: 'nearest', evidence: 'HYPOTHETICAL', confidence: 0 }], isolated: [], chains: ['COAL -> power_station'] },
  lost_detail: ['roads are not built'], diagnostics: ['one'], attribution: ['test'],
};
const detail = {
  summary: { name: 'synth' }, scenario,
  resolution: { rule: 'dominant-settlement/v1', reason: 'the only settlement', chosen: 'osm:relation/1', candidates: [{ rank: 1, id: 'osm:relation/1', display_name: 'Synthville, Testland', importance: 0.5, settlement: true }] },
  sources: [{ provider: 'synthetic', completeness: 'complete', license: 'CC0', retrieved_at: '2026-01-01T00:00:00Z', requests: 0, attribution: 'synthetic', terms_checked: 'n/a' }],
  features: [{ id: 'osm:node/3', class: 'landmark', name: 'Testberg', at: { lat: 44.98, lon: 7.03 }, tags: { natural: 'peak' } }],
  terrain: { grid: 3, source: 'synthetic', metres: [200, 210, 220, 230, null, 250, 255, 258, 260] },
  heightmap: 'data:image/png;base64,AA==',
  validation: { snapshot: [], scenario: [] },
  loadcheck: {
    placed: { towns: [1, 1], industries: [2, 2], signs: [1, 1] }, failures: [], displacement_threshold_tiles: 8,
    displaced_beyond_threshold: [{ id: 'industry:coal_mine:osm:way/4', tiles: 12 }],
    engine_generated: [{ id: '0', name: 'Generated town 0', engine_name: 'Brentwood', tile: { x: 10, y: 20 }, population: 900, evidence: 'HYPOTHETICAL' }],
    reload: { loaded: true, resumed: true, unchanged: true },
  },
};
const list = { count: 1, rules: 'tools/research/real-world/rules/temperate-default.v1.json', scenarios: [{ name: 'synth', query: 'Synthville', chosen: 'osm:relation/1', chosen_name: 'Synthville, Testland', ambiguous: false, map_size: 128, metres_per_tile: 93.75, features: 5, compiled: { entities: { 'town:DERIVED': 1 } } }] };
const posts = [];
const routes = (url, init) => {
  if (init.method === 'POST') posts.push([url, JSON.parse(init.body)]);
  if (url === '/api/realworld') return list;
  if (url === '/api/realworld/scenario?name=synth') return detail;
  if (url.startsWith('/api/realworld/scenario')) return respond({ error: 'no snapshot' }, { status: 404 });
  if (url === '/api/realworld/compile') return { out: 'build-real-world/synth', sha256sums: 'abc  heightmap.png\n', matches_recorded: true, next: 'load-check' };
  return respond({ error: 'no route' }, { status: 404 });
};

// The page module and its server-side render first, before the fake browser replaces the globals
// ECharts measures text with.
const rw = await import('../static/realworld.js');
const ssr = (() => {
  const c = echartsSsr.init(null, null, { renderer: 'svg', ssr: true, width: 600, height: 600 });
  c.setOption({ animation: false, ...rw.mapOption(detail, {}) });
  const svg = c.renderToSVGString();
  c.dispose();
  return svg;
})();
const env = fakeDom({ url: 'http://127.0.0.1:8800/realworld/synth?hide=OBSERVED&sel=town:osm:node/1', routes });
const $ = env.byId;
const chart = (id) => env.charts.find((c) => c.box === $(id));

test('the address holds the evidence filter and the selection; unknown classes are dropped', () => {
  const s = rw.getState('http://x/realworld/a?hide=OBSERVED,NONSENSE&sel=town:1&q=coal');
  assert.deepEqual(s.hide, ['OBSERVED']);
  assert.equal(s.sel, 'town:1');
  assert.equal(s.q, 'coal');
  assert.equal(rw.osmUrl('osm:way/42'), 'https://www.openstreetmap.org/way/42');
  assert.equal(rw.osmUrl('../x'), null);
});

test('the map option draws terrain, entities by evidence class and edges, and hides what the address hides', () => {
  const o = rw.mapOption(detail, { hide: [], sel: '' });
  const ids = o.series.map((s) => s.id);
  assert.deepEqual(ids, ['terrain', 'features', 'edges', 'entities-DERIVED', 'entities-INFERRED', 'entities-HYPOTHETICAL']);
  assert.equal(o.series[0].data.length, 8, 'a missing elevation sample is not drawn');
  const hyp = o.series.find((s) => s.id === 'entities-HYPOTHETICAL');
  assert.equal(hyp.data[0].name, 'industry:power_station:hypothetical-for-coal');
  assert.equal(o.series.find((s) => s.id === 'edges').data[0].lineStyle.type, 'dashed', 'a HYPOTHETICAL edge is dashed');
  const hidden = rw.mapOption(detail, { hide: ['OBSERVED', 'HYPOTHETICAL'] });
  assert.ok(!hidden.series.some((s) => s.id === 'features' || s.id === 'entities-HYPOTHETICAL'));
  assert.equal(hidden.series.find((s) => s.id === 'edges').data.length, 0);
  assert.equal(o.yAxis.inverse, true, 'north up: image row 0 at the top');
  const t = rw.tileOf(extent, { lat: 45, lon: 7 });
  assert.ok(Math.abs(t.x - 64) < 1e-9 && Math.abs(t.y - 64) < 1e-9, 'the origin is the centre tile');
});

test('the map option renders server-side through ECharts', () => {
  assert.match(ssr, /<svg/);
  assert.match(ssr, /<path|<rect/, 'terrain cells and marks are drawn');
});

test('/realworld/<name>: stats, the map through the one door, the selected entity and the table', async () => {
  await rw.placePage('synth');
  await env.settle?.();
  assert.match($('page').innerHTML, /Synthville, Testland/);
  assert.match($('page').innerHTML, /128×128/);
  assert.match($('rw-detail').innerHTML, /OBSERVED population tag/, 'the selection in the address is explained');
  assert.match($('rw-rows').innerHTML, /HYPOTHETICAL stand-in at/);
  assert.match($('rw-load').innerHTML, /Generated town 0/, 'the engine town is shown as not real');
  assert.match($('rw-load').innerHTML, /unchanged/, 'the reload check is shown');
  assert.match($('rw-load').innerHTML, /12 tiles/, 'a placement moved beyond the threshold is flagged');
  await env.tick?.();
  const c = chart('rw-map');
  assert.ok(c, 'the map is drawn through ecInit');
  const o = c.getOption();
  assert.ok(!o.series.some((s) => s.id === 'features'), 'hide=OBSERVED from the address');
  c.handlers.click({ seriesType: 'scatter', data: { name: 'industry:coal_mine:osm:way/4' } });
  assert.equal(new URL(env.location.href).searchParams.get('sel'), 'industry:coal_mine:osm:way/4');
  assert.match($('rw-detail').innerHTML, /why Test Quarry/);
  await $('rw-compile').onclick();
  assert.deepEqual(posts.at(-1), ['/api/realworld/compile', { name: 'synth' }]);
  assert.match($('rw-compile-out').textContent, /matches the recorded hashes: true/);
});

test('/realworld: every place links to its page', async () => {
  await rw.listPage();
  assert.match($('rw-rows').innerHTML, /href="\/realworld\/synth"/);
  assert.match($('page').innerHTML, /\/docs#\/default\/realWorld/, 'the API link names its operation');
});
