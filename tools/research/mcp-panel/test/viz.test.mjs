// node --test tools/research/mcp-panel/test/ : the hub's world view (static/viz.js), driven over a
// fake hub WebSocket in the fake browser of fake-dom.mjs. The fixtures are fictional; nothing here
// talks to a game or a hub.
import { UI } from './panel-ui.mjs';
import test from 'node:test';
import assert from 'node:assert/strict';
import { fakeDom } from './fake-dom.mjs';

const world = {
  schema: 'world-graph/v1',
  nodes: [
    { id: 'town:1', kind: 'town', name: 'Aton', x: 10, y: 10 },
    {
      id: 'station:5',
      kind: 'station',
      name: 'Aton Stop',
      x: 11,
      y: 10,
      waiting: [{ cargo: 0, count: 30 }],
    },
    { id: 'vehicle:9', kind: 'vehicle', name: 'Bus', x: 20, y: 11 },
  ],
  edges: [
    { from: 'station:5', to: 'town:1', kind: 'serves_town' },
    { from: 'vehicle:9', to: 'station:5', kind: 'order', seq: 0 },
  ],
};
const map = { width: 3, height: 2, types: ['CRW', 'S?C'], heights: ['000102', '0a0b0c'] };
const ops = [
  { instance: 'g1', id: 'resource:openttd://world/graph' },
  { instance: 'g1', id: 'tool:openttd.view.scroll_to' },
  { instance: 'g2', id: 'resource:openttd://world/graph' },
  { instance: 'g1', id: 'resource:openttd://map' },
];
const calls = [];
const answer = (m) => {
  if (m.method === 'hub.catalogue') return ops;
  calls.push(m.params);
  const id = m.params.id;
  if (id === 'resource:openttd://world/graph')
    return { contents: [{ text: JSON.stringify({ data: world }) }] };
  if (id === 'resource:openttd://map') return { data: map };
  if (id === 'resource:openttd://map/ways') return { data: { roads: ['010', '000'] } };
  if (id === 'resource:openttd://companies')
    return JSON.stringify({
      data: {
        companies: [
          { id: 0, name: 'Co', money: 5 },
          { id: 1, money: 7 },
        ],
      },
    });
  return {};
};
const env = fakeDom({ url: 'http://127.0.0.1:8900/viz#instance=g2&exaggeration=3' });
globalThis.d3 = await import('d3');
const viz = await import('../static/viz.js');
const $ = env.byId;
const ws = env.sockets[0];
ws.send = (raw) => {
  const m = JSON.parse(raw);
  queueMicrotask(() =>
    ws.onmessage({ data: JSON.stringify({ jsonrpc: '2.0', id: m.id, result: answer(m) }) }),
  );
};

test('the address is the state: defaults stay out of it, a change goes in', () => {
  assert.deepEqual(viz.getState(), {
    instance: 'g2',
    mode: 'map',
    max: undefined,
    kinds: ['town', 'industry', 'station', 'vehicle'],
    exaggeration: 3,
  });
  viz.setState({ mode: 'force', kinds: ['town'], exaggeration: 6, max: 'graph' });
  assert.equal(env.location.hash, '#instance=g2&mode=force&kinds=town&max=graph');
  viz.setState({
    mode: 'map',
    kinds: ['town', 'industry', 'station', 'vehicle'],
    max: undefined,
    exaggeration: 3,
  });
  assert.equal(env.location.hash, '#instance=g2&exaggeration=3');
});

test("candidates publish the world graph, once each, in the hub's order", () => {
  assert.deepEqual(viz.candidates(ops), ['g1', 'g2']);
  assert.deepEqual(viz.candidates([]), []);
});

test('the terrain is sampled to at most 256 cells a side, roads dark, unknown types grey', () => {
  const t = viz.terrainCells(map, { roads: ['010', '000'] }, 10);
  assert.deepEqual([t.cols, t.rows], [3, 2]);
  assert.deepEqual(
    t.ys.map((y) => Math.round(y)),
    [0, 1, 2, 10, 11, 12],
  );
  assert.deepEqual(t.hexes, [0x6b8e23, 0x2b2b2b, 0x1e6fbf, 0x9370db, 0x808080, 0x6b8e23]);
  const big = viz.terrainCells(
    {
      width: 600,
      height: 2,
      types: ['C'.repeat(600), 'C'.repeat(600)],
      heights: ['00'.repeat(600), 'zz'.repeat(600)],
    },
    null,
    1,
  );
  assert.deepEqual([big.cols, big.rows], [200, 1], 'every third tile of a 600-wide map');
  assert.equal(big.ys[0], 0);
});

test('force positions are deterministic and only for the kinds shown', () => {
  const a = viz.forcePositions(world, ['town', 'station']);
  assert.deepEqual([...a.keys()], ['town:1', 'station:5']);
  assert.deepEqual(viz.forcePositions(world, ['town', 'station']), a);
});

test('connected, the page picks the instance, reads the world and draws its charts', async () => {
  ws.onopen();
  await viz.started;
  await env.settle();
  assert.equal($('conn').textContent, 'connected');
  assert.equal($('instance').value, 'g2', 'the instance in the address');
  assert.deepEqual(
    $('instance').kids.map((o) => o.value),
    ['g1', 'g2'],
  );
  await env.tick();
  const graph = env.charts.find((c) => c.box === $('graph'));
  assert.equal(graph.getOption().series[0].data.length, 3);
  const money = env.charts.find((c) => c.box === $('money'));
  assert.deepEqual(
    money.getOption().series.map((s) => s.name),
    ['Co', 'company 1'],
  );
  const controls = $('graph-controls').kids;
  assert.equal(controls.filter((k) => k.localName === 'label').length, 4, 'one box per node kind');
  assert.ok(controls[0].kids[0].className.startsWith(UI.check), "a kind box is the shell's check");
  assert.ok(controls.find((k) => k.localName === 'select').className.includes(UI.select));
});

test('a node click shows its details and a button that centres the game on it', async () => {
  const graph = env.charts.find((c) => c.box === $('graph'));
  graph.handlers.click({ dataType: 'edge' });
  graph.handlers.click({ dataType: 'node', data: { id: 'station:5' } });
  const [button, view] = $('details').kids;
  assert.equal(button.textContent, 'Show in game');
  assert.equal(button.className, `${UI.primary} mb-2`);
  assert.match(
    view.textContent || view.innerHTML || JSON.stringify(view.kids.map((k) => k.textContent)),
    /station:5/,
  );
  await button.onclick();
  await env.settle();
  assert.deepEqual(calls.at(-1), {
    instance: 'g1',
    id: 'tool:openttd.view.scroll_to',
    body: { x: 11, y: 10 },
  });
});

test('the controls redraw and keep their values in the address', async () => {
  const controls = $('graph-controls').kids;
  const mode = controls.find((k) => k.localName === 'select');
  mode.value = 'force';
  mode.onchange();
  assert.match(env.location.hash, /mode=force/);
  const townBox = controls[0].kids[0];
  townBox.checked = false;
  townBox.onchange();
  assert.match(env.location.hash, /kinds=industry%2Cstation%2Cvehicle/);
  townBox.checked = true;
  townBox.onchange();
  // The terrain needs WebGL (three.js), which node has not: its slider is checked, not moved.
  const range = $('terrain-controls').kids[0];
  assert.deepEqual(
    [range.type, String(range.value), range.title],
    ['range', '3', 'height exaggeration'],
  );
  assert.match(
    $('terrain').textContent,
    /^terrain: /,
    'without WebGL the card says why it is empty',
  );
  const instance = $('instance');
  instance.value = 'g1';
  instance.onchange();
  assert.equal(env.reloads(), 1, 'another instance reloads the page at its address');
  await env.tick();
  await env.tick();
});
