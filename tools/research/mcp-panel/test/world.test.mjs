// node --test tools/research/mcp-panel/test/ : the world view (static/world.js): map, road graph,
// plan, yards and calibration drawn from fixtures in the fake browser of fake-dom.mjs. The fixtures
// are fictional; nothing here talks to a game.
import { UI } from './panel-ui.mjs';
import test from 'node:test';
import assert from 'node:assert/strict';
import { fakeDom, respond } from './fake-dom.mjs';

const map = {
  width: 4,
  height: 2,
  types: ['CRrH', 'TSWI'],
  heights: ['00010203', '04050a0f'],
  owners: ['01TW', 'e2xx'],
};
const graph = {
  nodes: [
    { id: 1, kind: 'town', label: 'Aton', x: 1, y: 0, component: 0 },
    { id: 2, kind: 'junction', x: 2, y: 1, component: 1 },
    { id: 3, kind: 'station', x: 3, y: 1, component: 0 },
  ],
  edges: [
    { source: 1, target: 3, length: 4 },
    { source: 1, target: 2, length: 2 },
  ],
  stats: { road_tiles: 9, nodes: 3, edges: 2, components: 2 },
};
const pair = (mode, score) => ({
  a: { id: 1, name: 'Aton', x: 1, y: 0 },
  b: { id: 2, name: 'Bee', x: 3, y: 1 },
  mode,
  distance: 3,
  supply: 9,
  road_connected: mode === 'bus',
  score,
});
const plan = {
  by_mode: {
    bus: [pair('bus', 5)],
    air: [pair('air', 4), { ...pair('air', 8), b: { id: 3, name: 'Cee', x: 0, y: 1 } }],
  },
  portfolio: {
    solver: 'greedy',
    kernel: { id: 'k1', work: 1, plate: 2 },
    total_score: 7,
    investment_k: 100,
    budget_k: 300,
    chosen: [
      {
        kind: 'freight',
        cargo: 'Coal',
        mode: 'rail',
        producer: { name: 'Mine', x: 1, y: 1 },
        acceptor: { name: 'Plant' },
        score: 3,
      },
      {
        kind: 'passengers',
        mode: 'bus',
        a: { name: 'Aton', x: 1, y: 0 },
        b: { name: 'Bee' },
        score: 2,
      },
    ],
  },
};
const yards = {
  yards: {
    pairs: [
      {
        proven: true,
        a: 'L1',
        b: 'L2 <x>',
        side: 1,
        extra: 500,
        shared: { end: { x: 2, y: 1 } },
        b_alone: 1000.4,
        b_with_yard: 1500,
      },
      {
        proven: false,
        a: 'L3',
        b: 'L4',
        tried: [
          { side: 1, result: 'blocked' },
          { side: -1, result: 'no site' },
        ],
      },
      { proven: false, a: 'L5', b: 'L6', side: -1, extra: -5 },
    ],
  },
};
const calibration = {
  calibration: {
    params: { k: 1 },
    modes: {
      bus: { factor: 0.95, lines: 1 },
      rail: { factor: 0.6, lines: 2 },
      air: { factor: 0.2, lines: 1 },
    },
    lines: [
      {
        stations: [5, 6],
        mode: 'bus',
        candidate: 'A-B',
        real_per_vehicle_month: 1200.4,
        predicted_per_vehicle_month: 1000,
        ratio: 1.2,
        why: 'ok',
      },
      { stations: [5, 9], type: 'road', real_per_vehicle_month: null, ratio: null },
    ],
  },
};
const stations = {
  data: {
    stations: [
      { id: 5, name: 'Aton Stop', x: 2, y: 1 },
      { id: 6, name: 'Bee Stop', x: 3, y: 1 },
    ],
  },
};
const views = [];
let answers = {};
const routes = (url, init) => {
  if (url in answers) return answers[url];
  if (url === '/api/resources/map') return { data: map };
  if (url === '/api/resources/towns') return { data: { towns: [{ name: 'Aton', x: 1, y: 0 }] } };
  if (url === '/api/resources/stations') return stations;
  if (url === '/api/graph') return graph;
  if (url.startsWith('/api/plan?limit=10&params=')) return plan;
  if (url === '/api/plan?limit=10&track=6&yards=4') return yards;
  if (url === '/api/plan?calibrate=1') return calibration;
  if (url === '/api/view/tool') {
    views.push(JSON.parse(init.body));
    return { ok: true };
  }
  return respond({ error: 'no route' }, { status: 404 });
};
const env = fakeDom({
  url: 'http://127.0.0.1:8800/world?layer=owners&modes=bus,air&budget=5000',
  routes,
});
env.document.body.innerHTML = [
  '<div data-frame id="frame"><button data-maximize id="max">Maximize</button></div>',
  '<input type="checkbox" data-mode="bus" id="m-bus">',
  '<input type="checkbox" data-mode="rail" id="m-rail">',
].join('');
const w = await import('../static/world.js');
const $ = env.byId;

test('the address is the state, read and written back', () => {
  const s = w.readState('?layer=heights&component=2&live=0&budget=10&payback=6&modes=air');
  assert.deepEqual(s, {
    layer: 'heights',
    component: '2',
    live: false,
    budget: 10,
    payback: 6,
    modes: ['air'],
  });
  assert.deepEqual(w.readState(w.stateQuery(s)), s);
  assert.deepEqual(w.readState('').modes, ['bus', 'rail', 'air', 'truck', 'freight_rail']);
  assert.equal(
    env.location.search,
    '?layer=owners&component=all&live=1&budget=5000&payback=24&modes=bus%2Cair',
  );
});

test('a tile takes its colour from the layer; the legend says what the colours mean', () => {
  assert.equal(w.tileColour('types', 'W', '00', '0'), '#2a6fb0');
  assert.equal(w.tileColour('types', '?', '00', '0'), '#000');
  assert.equal(w.tileColour('heights', 'C', '0a', '0'), 'rgb(160,160,160)');
  assert.equal(w.tileColour('heights', 'C', 'ff', '0'), 'rgb(255,255,255)', 'capped');
  assert.equal(w.tileColour('heights', 'W', '03', '0'), '#2a6fb0');
  assert.equal(
    w.tileColour('owners', 'C', '00', '9'),
    w.tileColour('owners', 'C', '00', '1'),
    'eight colours, wrapped',
  );
  assert.deepEqual(
    ['T', 'W', 'x'].map((o) => w.tileColour('owners', 'C', '00', o)),
    ['#6b6b6b', '#2a6fb0', '#1f2937'],
  );
  assert.match(w.legendHtml('types'), /background:#2a6fb0"><\/span>water/);
  assert.match(w.legendHtml('owners'), /company owner/);
  assert.match(w.legendHtml('heights'), /brighter is higher/);
  assert.equal(w.tileText(map, 2, 1, 'here'), 'here (2,1): water, height 10, owner x');
  assert.equal(w.tileText(null, 0, 0), 'tile (0,0): ?, height ?, owner ?');
});

test("the plan's pairs are each kept once; the road graph narrows to one component", () => {
  assert.deepEqual(
    w.planPairs(plan).map((p) => `${p.mode} ${p.b.name}`),
    ['bus Bee', 'air Cee'],
  );
  assert.deepEqual(w.planPairs(null), []);
  const all = w.graphView(graph, 'all');
  assert.deepEqual(all.comps, [0, 1]);
  assert.deepEqual(
    all.nodes.map((n) => [n.name, n.symbolSize, n.category]),
    [
      ['Aton', 14, 0],
      ['junction (2,1)', 4, 2],
      ['station (3,1)', 9, 1],
    ],
  );
  assert.equal(all.links.length, 2);
  const one = w.graphView(graph, '0');
  assert.deepEqual(
    one.nodes.map((n) => n.id),
    ['1', '3'],
  );
  assert.deepEqual(
    one.links,
    [{ source: '1', target: '3', value: 4 }],
    'an edge to a hidden node is dropped',
  );
});

test("the portfolio, plan rows, yards and calibration are drawn with the shell's badge", () => {
  assert.equal(
    w.portfolioText(plan.portfolio),
    'solver: greedy · kernel k1 (work 1, plate 2) · total score 7 · invest 100k of 300k',
  );
  assert.equal(w.portfolioText({ solver: 's' }), 'solver: s · total score  · invest k of k');
  const pf = w.portfolioHtml(plan.portfolio);
  assert.match(
    pf,
    /data-x="1" data-y="1"><span [^>]*>rail<\/span> Coal by rail: Mine → Plant, score 3/,
  );
  assert.match(pf, /passengers by bus: Aton → Bee/);
  assert.ok(pf.includes(`class="${UI.badge} text-white"`));
  assert.equal(w.portfolioHtml({}), '<li>no investment fits</li>');
  assert.match(w.planRowsHtml(w.planPairs(plan)), /data-i="1"[^]*Cee[^]*>no</);
  const proven = w.yardRow(yards.yards.pairs[0]);
  assert.match(proven, /data-x="2" data-y="1"/);
  assert.match(
    proven,
    />proven<[^]*L2 &lt;x&gt;[^]*\(2, 1\)[^]*1,000[^]*1,500[^]*side \+1, B \+500/,
  );
  assert.match(w.yardRow(yards.yards.pairs[1]), />refuted<[^]*side 1: blocked; side -1: no site/);
  assert.match(w.yardRow(yards.yards.pairs[2]), /A has no route/);
  assert.match(w.yardRow({ proven: true, a: 'a', b: 'b', side: -1, extra: -5 }), /side -1, B -5/);
  const modes = w.calModesHtml(calibration.calibration.modes);
  assert.match(modes, /bg-green-100[^>]*>bus × 0.95 \(1 line, real \/ predicted\)/);
  assert.match(modes, /bg-yellow-100[^>]*>rail × 0.6 \(2 lines/);
  assert.match(modes, /bg-red-100[^>]*>air × 0.2/);
  assert.match(w.calModesHtml({}), /no line measured yet/);
  const rows = w.calRowsHtml(calibration.calibration.lines, (id) => `#${id}`);
  assert.match(rows, /#5 - #6[^]*1,200[^]*1,000[^]*1.2/);
  assert.match(rows, /road[^]*>-<\/td>/);
  assert.match(w.calRowsHtml([], String), /No vehicle shuttles/);
});

test('the page draws the map, the graph and the plan from the resources', async () => {
  await env.tick();
  assert.equal($('p-layer').value, 'owners');
  assert.ok($('m-bus').checked && !$('m-rail').checked, 'the mode boxes follow the address');
  assert.ok(
    $('map').drawn.some(([k]) => k === 'fillRect'),
    'the map is painted',
  );
  assert.ok(
    $('map').drawn.some(([k]) => k === 'stroke'),
    "with the plan's lines",
  );
  assert.match($('map-legend').innerHTML, /company owner/);
  const chart = env.charts.find((c) => c.box === $('graph'));
  assert.equal(chart.theme, 'dark');
  assert.equal(chart.getOption().series[0].data.length, 3);
  assert.equal(chart.getOption().backgroundColor, 'transparent');
  assert.match($('w-stats').textContent, /9 road tiles · 3 nodes · 2 edges · 2 components/);
  assert.match($('p-component').innerHTML, /Component 1/);
  assert.match($('portfolio-solver').textContent, /^solver: greedy/);
  assert.match($('plan-body').innerHTML, /Cee/);
  assert.match($('w-msg').textContent, /^updated /);
  assert.match(
    chart.getOption().tooltip.formatter({ dataType: 'edge', data: { value: 4 } }),
    /road, 4 tiles/,
  );
  assert.match(
    chart.getOption().tooltip.formatter({ dataType: 'node', data: { name: 'A', value: 0 } }),
    /A<br>component 0/,
  );
});

test("a click on the map, a row, a line or a node moves the owner's camera", async () => {
  await $('map').listeners.click[0]({ clientX: 500, clientY: 300 });
  await env.settle();
  assert.deepEqual(
    views.slice(-2).map((v) => v.name),
    ['openttd.view.scroll_to', 'openttd.view.mark'],
  );
  assert.deepEqual(views.at(-1).arguments, { x: 2, y: 1 });
  assert.match(
    $('w-msg').textContent,
    /tile \(2,1\): water, height 10, owner x; moving your camera there/,
  );
  await $('map').listeners.click[0]({ clientX: -5, clientY: 0 });
  await $('plan-body').querySelector('tr').listeners.click[0]();
  await env.settle();
  assert.match($('w-msg').textContent, /^air: Aton - Cee/, 'rows run from the best score');
  await $('portfolio-body').querySelector('li[data-x]').listeners.click[0]();
  await env.settle();
  assert.deepEqual(views.at(-1).arguments, { x: 1, y: 1 });
  const chart = env.charts.find((c) => c.box === $('graph'));
  chart.handlers.click({ dataType: 'node', data: { x: 3, y: 1, name: 'station' } });
  chart.handlers.click({ dataType: 'edge', data: {} });
  await env.settle();
  assert.deepEqual(views.at(-1).arguments, { x: 3, y: 1 });
  answers = { '/api/view/tool': respond('not json') };
  await $('map').listeners.click[0]({ clientX: 0, clientY: 0 });
  await env.settle();
  assert.match($('w-msg').textContent, /\(failed: /);
  answers = {};
});

test('the controls redraw and keep their values in the address', async () => {
  $('p-layer').onchange({ target: { value: 'heights' } });
  assert.match($('map-legend').innerHTML, /brighter is higher/);
  $('p-layer').onchange({ target: { value: 'types' } });
  assert.match($('map-legend').innerHTML, /water/);
  $('p-component').onchange({ target: { value: '1' } });
  assert.equal(env.charts.find((c) => c.box === $('graph')).getOption().series[0].data.length, 1);
  $('p-budget').onchange({ target: { value: '-3' } });
  $('p-payback').onchange({ target: { value: '' } });
  $('p-live').onchange({ target: { checked: false } });
  $('m-rail').checked = true;
  $('m-rail').onchange();
  assert.equal(
    env.location.search,
    '?layer=types&component=1&live=0&budget=0&payback=24&modes=bus%2Crail',
  );
  await env.settle();
});

test('yards and calibration are proven on a click, and their rows lead to the map', async () => {
  await $('yards-run').listeners.click[0]();
  assert.match($('yards-body').innerHTML, />proven<[^]*>refuted</);
  assert.equal($('yards-run').textContent, 'Prove yards');
  $('yards-body').listeners.click[0]({ target: $('yards-body').querySelector('tr[data-x]') });
  await env.settle();
  assert.deepEqual(views.at(-1).arguments, { x: 2, y: 1 });
  $('yards-body').listeners.click[0]({ target: $('yards-body') });
  answers = { '/api/plan?limit=10&track=6&yards=4': { yards: { pairs: [] } } };
  await $('yards-run').listeners.click[0]();
  assert.match($('yards-body').innerHTML, /No two priced lines meet/);
  answers = {
    '/api/plan?limit=10&track=6&yards=4': respond({ error: 'no plan' }, { status: 500 }),
  };
  await $('yards-run').listeners.click[0]();
  assert.match($('yards-body').innerHTML, /text-red-600[^]*no plan/);
  answers = { '/api/plan?limit=10&track=6&yards=4': respond('{') };
  await $('yards-run').listeners.click[0]();
  assert.match($('yards-body').innerHTML, /text-red-600/);
  answers = {};

  await $('cal-run').listeners.click[0]();
  assert.match($('cal-modes').innerHTML, /bus × 0.95/);
  assert.match($('cal-body').innerHTML, /Aton Stop - Bee Stop[^]*Aton Stop - #9/);
  assert.equal($('cal-params').textContent, 'next plan: /api/plan?params={"k":1}');
  assert.equal($('cal-run').textContent, 'Calibrate');
  $('cal-body').listeners.click[0]({ target: $('cal-body').querySelector('tr[data-st]') });
  await env.settle();
  assert.deepEqual(views.at(-1).arguments, { x: 2, y: 1 });
  answers = { '/api/plan?calibrate=1': {} };
  await $('cal-run').listeners.click[0]();
  assert.match($('cal-body').innerHTML, /no calibration in the answer/);
  answers = {};
});

test('maximize, Escape, resize, the live view and an unreachable panel', async () => {
  await $('max').click();
  assert.ok($('frame').classList.contains('fixed'));
  assert.equal($('max').textContent, 'Restore (Esc)');
  await env.document.dispatch('keydown', { key: 'x' });
  await env.document.dispatch('keydown', { key: 'Escape' });
  assert.ok(!$('frame').classList.contains('fixed'));
  env.windowListeners.resize();
  await env.tick();
  env.images.at(-1).onload();
  assert.match($('live-msg').textContent, /^updated /);
  await env.tick();
  env.images.at(-1).onerror();
  assert.equal($('live-msg').textContent, 'no GUI client attached yet');
  $('p-live').onchange({ target: { checked: true } });
  env.routes = () => {
    throw new Error('down');
  };
  await env.tick();
  assert.equal($('w-msg').textContent, 'not reachable: down');
});
