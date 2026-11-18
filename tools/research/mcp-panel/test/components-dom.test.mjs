// node --test tools/research/mcp-panel/test/ : the DOM components of static/components.js (the
// hub's operation forms, catalogue, instances, events, audit, track planner, the chart door and
// the maximizable card), driven through the fake browser of fake-dom.mjs with a fake hub client.
// Everything is fictional; nothing here talks to a hub or a game.
import './panel-ui.mjs'; // first: shell.js publishes PanelUI before any page script
import test from 'node:test';
import assert from 'node:assert/strict';
import { fakeDom } from './fake-dom.mjs';
import {
  UI, TONE, callParams, jsonView, operationForm, catalogueView, instancesView, eventsView, auditView,
  trackPlanView, ecInit, maximizable,
} from '../static/components.js';

const env = fakeDom();

/** Every element below `root` (appended children and parsed markup) whose tag is `tag`. */
const all = (root, tag) => root.querySelectorAll(tag);
/** A hub client whose answers come from `answer(method, params)`; it records every call. */
function hub(answer = () => ({})) {
  const calls = [];
  return { calls, call: async (method, params) => { calls.push([method, params]); return answer(method, params); } };
}
const click = (node) => node.dispatch('click');

test('a required query parameter is refused when it is missing', () => {
  assert.throws(() => callParams({ instance: 'p', id: 'x', query: [{ name: 'since', required: true }] }, {}),
    /since is required/);
});

test('JSON is shown pretty, and every openttd:// URI in it reads that resource', async () => {
  const value = { uri: 'openttd://map/terrain', list: ['openttd://world/graph'] };
  const read = [];
  const pre = jsonView(value, (u) => read.push(u));
  assert.equal(pre.localName, 'pre');
  assert.ok(pre.className.includes('text-xs'));
  assert.equal(pre.textContent, JSON.stringify(value, null, 2));
  const links = all(pre, 'a');
  assert.deepEqual(links.map((a) => a.textContent), ['openttd://map/terrain', 'openttd://world/graph']);
  const ev = await click(links[1]);
  assert.ok(ev.defaultPrevented);
  assert.deepEqual(read, ['openttd://world/graph']);
  assert.deepEqual(all(jsonView(value), 'a'), [], 'without a reader, the URIs stay text');
});

const schema = { type: 'object', required: ['pause'], properties: {
  pause: { type: 'boolean', description: 'pause the game' }, tile: { type: 'integer', default: 7 },
  mode: { enum: ['a', 'b'] }, name: { type: 'string' }, extra: { type: 'object' } } };

test('an operation form is its schema and query; submitting calls the hub and shows the answer', async () => {
  const client = hub(() => ({ ok: true, uri: 'openttd://map' }));
  const results = [];
  const op = { instance: 'g', id: 'pause', schema, query: [{ name: 'since' }] };
  const box = operationForm(op, client, { onResult: (o, r) => results.push([o.id, r]), onUri: () => {} });
  const [form, out] = box.kids;
  const fields = [...all(form, 'input'), ...all(form, 'select'), ...all(form, 'textarea')];
  assert.equal(fields.length, 6);
  const [pause, tile, name, since] = all(form, 'input');
  assert.deepEqual([pause.type, pause.className], ['checkbox', UI.check]);
  assert.equal(tile.value, '7', 'a default fills the field');
  assert.match(form.textContent, /pause \*pause the game/);
  assert.match(form.textContent, /\?since/);
  const submit = all(form, 'button')[0];
  assert.deepEqual([submit.textContent, submit.className], ['Call', UI.primary]);
  pause.checked = true;
  name.value = 'x';
  since.value = '3';
  all(form, 'select')[0].value = 'b';
  all(form, 'textarea')[0].value = '{"a":1}';
  const ev = await form.dispatch('submit');
  assert.ok(ev.defaultPrevented);
  assert.deepEqual(client.calls[0], ['hub.call', {
    instance: 'g', id: 'pause', body: { pause: true, tile: 7, mode: 'b', name: 'x', extra: { a: 1 } }, query: { since: '3' } }]);
  assert.equal(out.kids[0].localName, 'pre');
  assert.equal(all(out, 'a')[0].textContent, 'openttd://map', 'the answer links its URIs');
  assert.deepEqual(results, [['pause', { ok: true, uri: 'openttd://map' }]]);
  // A refused call and a form the schema refuses both show their message.
  all(form, 'textarea')[0].value = '{';
  await form.dispatch('submit');
  assert.equal(out.textContent, 'extra: not JSON');
  assert.ok(out.kids[0].className.includes('text-red-800'));
  assert.equal(client.calls.length, 1, 'nothing is sent when the form does not parse');
});

test('a read-only operation reads, a failing call says why, and a stream has no form', async () => {
  const client = { call: async () => { throw new Error('no such instance'); } };
  const box = operationForm({ instance: 'g', id: 'status', read_only: true }, client);
  const [form, out] = box.kids;
  const button = all(form, 'button')[0];
  assert.deepEqual([button.textContent, button.className], ['Read', UI.secondary]);
  await form.dispatch('submit');
  assert.equal(out.textContent, 'no such instance');
  const stream = operationForm({ instance: 'g', id: 'events', stream: true }, client);
  assert.equal(stream.kids[0].textContent, 'A stream: it arrives in the event panel.');
});

const ops = [
  { instance: 'a', id: 'status', method: 'GET', path: '/api/status', summary: 'game status', tags: ['game'] },
  { instance: 'a', id: 'news', method: 'GET', path: '/api/news', summary: 'news', tags: ['game'] },
  { instance: 'b', id: 'tool:openttd.view.zoom', method: 'TOOL', path: 'openttd.view.zoom', summary: 'zoom', tags: [] },
];
/** The operation rows of a catalogue card: [{head, body}] by key. */
const rows = (container) => Object.fromEntries(container.kids.flatMap((card) => card.kids
  .filter((k) => k.localName === 'div').map((d) => [d.getAttribute('id'), { head: d.kids[0], body: d.kids[1] }])));

test('the hub catalogue: one card per instance, rows per tag, a click opens and closes a form', async () => {
  const container = env.document.createElement('div');
  const picked = [];
  catalogueView(container, ops, hub(), { instance: '', q: '', op: 'a/news' }, { onSelect: (o) => picked.push(o.id) });
  assert.deepEqual(container.kids.map((c) => c.kids[0].textContent), ['a', 'b']);
  assert.deepEqual(container.kids.map((c) => c.kids[1].textContent), ['game (2)', 'other (1)']);
  const r = rows(container);
  assert.deepEqual(Object.keys(r), ['a/status', 'a/news', 'b/tool:openttd.view.zoom']);
  assert.equal(r['a/news'].body.childElementCount, 1, 'the operation of the address is open');
  assert.equal(r['a/status'].body.childElementCount, 0);
  assert.ok(r['a/status'].head.className.includes('break-all'));
  assert.equal(r['a/status'].head.textContent, 'GETstatusgame status');
  const ev = await click(r['a/status'].head);
  assert.ok(ev.defaultPrevented);
  assert.equal(r['a/status'].body.childElementCount, 1);
  assert.deepEqual(picked, ['status']);
  await click(r['a/status'].head);
  assert.equal(r['a/status'].body.childElementCount, 0, 'a second click closes it');
  catalogueView(container, ops, hub(), { instance: 'b', q: '' });
  assert.deepEqual(Object.keys(rows(container)), ['b/tool:openttd.view.zoom']);
  catalogueView(container, ops, hub(), { instance: '', q: 'nothing like it' });
  assert.equal(container.textContent, 'No operation matches.');
});

test('the instances table: a name narrows the catalogue to it, a second click widens it again', async () => {
  const container = env.document.createElement('div');
  const picks = [];
  const instances = [{ name: 'g1', kind: 'panel', access: 'rw', operations: 12, base: 'http://h:1' },
    { name: 'g2', kind: 'gui', access: 'ro', operations: 3, base: 'http://h:2', error: 'down' }];
  instancesView(container, instances, { instance: 'g2' }, (n) => picks.push(n));
  const wrap = container.kids[0];
  assert.equal(wrap.className, UI.tableWrap);
  const table = wrap.kids[0];
  assert.equal(table.className, UI.table);
  assert.equal(table.kids[0].className, UI.thead);
  assert.deepEqual(all(table, 'th').map((t) => t.textContent), ['Instance', 'Kind', 'Access', 'Operations', 'Base', 'Error']);
  assert.deepEqual(all(table, 'td').map((t) => t.textContent).slice(6), ['g2', 'gui', 'ro', '3', 'http://h:2', 'down']);
  const [a1, a2] = all(table, 'a');
  await click(a1);
  await click(a2);
  assert.deepEqual(picks, ['g1', ''], 'the current instance toggles off');
});

test('the event stream: newest first, bounded, its instance and kind clickable', async () => {
  const container = env.document.createElement('div');
  const kinds = [], instances = [];
  const push = eventsView(container, { limit: 2, onKind: (k) => kinds.push(k), onInstance: (i) => instances.push(i) });
  push({ instance: 'a', event: { kind: 'game.month', seq: 1, month: 3 } });
  push({ instance: 'b', event: { kind: 'company.new', seq: 2, id: 1 } });
  push({ instance: 'c', event: { seq: 3 } });
  const list = container.kids[0];
  assert.equal(list.childElementCount, 2, 'the oldest line is dropped');
  assert.deepEqual(list.kids.map((l) => l.textContent), ['c?{}', 'bcompany.new{"id":1}']);
  const [inst, kind] = all(list.kids[1], 'a');
  await click(inst);
  await click(kind);
  assert.deepEqual([instances, kinds], [['b'], ['company.new']]);
  const quietBox = env.document.createElement('div');
  eventsView(quietBox)({ instance: 'x', event: { kind: 'k' } });
  // Without handlers a click does nothing but is still taken from the page.
  for (const a of all(quietBox, 'a')) assert.ok((await click(a)).defaultPrevented);
});

test('the audit: every duplicate pair of stations and depots, each end centring the camera', async () => {
  const container = env.document.createElement('div');
  const centred = [];
  const st = (id, name) => ({ id, name, x: id, y: id + 1 });
  const results = [
    { instance: 'a', stations: 8, catchment: 'game', duplicates: [{ owner: 0, facilities: ['bus', 'truck'], a: st(1, 'A'), b: st(2, 'B'), distance: 1 }],
      depots: { depots: 4, reach: 16, network: true, duplicates: [{ owner: 0, type: 'road', a: st(5), b: st(6), distance: 3, by: 'road' }] } },
    { instance: 'b', error: 'no stations operation' },
    { instance: 'c', stations: 2, catchment: 'radius', duplicates: [], depots: { error: 'no depots operation' } },
    { instance: 'd', stations: 1, catchment: 'radius', depots: { depots: 0, reach: 8, network: false } },
  ];
  const s = auditView(container, results, { onStation: (x) => centred.push(x.id) });
  assert.deepEqual(s, { instances: 4, audited: 3, skipped: 1, pairs: 1, depotPairs: 1 });
  const text = container.kids.map((k) => k.textContent);
  assert.equal(text[0], '3 instance(s) audited, 1 skipped, 1 duplicate station pair(s), 1 duplicate depot pair(s).');
  assert.ok(text.includes('a: 1 duplicate pair(s) (8 stations, catchment from the game)'));
  assert.ok(text.includes('owner 0, bus+truck: A (1,2) and B (2,3), 1 tile(s) apart'));
  assert.ok(text.includes('depots: 4 (service reach 16 tiles, along the road network)'));
  assert.ok(text.includes('owner 0, road depots: depot 5 (5,6) and depot 6 (6,7), 3 tile(s) by road'));
  assert.ok(text.includes('b: no stations operation'));
  assert.ok(text.includes('c: no duplicate (2 stations, catchment from the radius)'));
  assert.ok(text.includes('depots: no depots operation'));
  assert.ok(text.includes('depots: 0 (service reach 8 tiles, straight distance only)'));
  for (const a of all(container, 'a')) await click(a);
  assert.deepEqual(centred, [1, 2, 5, 6]);
  const quiet = env.document.createElement('div');
  auditView(quiet, results);
  for (const a of all(quiet, 'a')) assert.ok((await click(a)).defaultPrevented, 'no handler, no harm');
});

const planOps = [{ instance: 'g1', id: 'resource:openttd://map' }, { instance: 'g2', id: 'resource:openttd://map' }];
const railtypes = { contents: [{ text: JSON.stringify({ data: { types: [
  { label: 'RAIL', name: 'Railway', build_cost: 75 }, { label: 'ELRL', name: 'Electrified', build_cost: 90 }] } }) }] };

test('the track planner: its inputs from the address, rail types from the game, the plan from AlgoGlyph', async () => {
  const container = env.document.createElement('div');
  let plan = { ok: true, result: { status: 'found', money: 1650.4, priced: { exact: true }, counts: { tiles: 22, length: 22.5 }, pieces: [1, 2] } };
  const client = hub((method) => (method === 'hub.call' ? railtypes : plan));
  const planned = [], started = [];
  trackPlanView(container, client, { ops: planOps, plan: '68,9,SW;45,9;ELRL', instance: 'g2',
    onPlan: (p, g) => planned.push([p, g]), onStart: (from) => started.push(from) });
  await env.settle();
  const [form, out] = container.kids;
  const [game, fd, rail] = all(form, 'select');
  const [fx, fy, tx, ty] = all(form, 'input');
  assert.equal(game.value, 'g2', 'the instance of the address');
  assert.deepEqual([fx.value, fy.value, fd.value, tx.value, ty.value], ['68', '9', 'SW', '45', '9']);
  assert.deepEqual(client.calls[0], ['hub.call', { instance: 'g2', id: 'resource:openttd://railtypes' }]);
  assert.deepEqual(rail.kids.map((o) => o.textContent), ['plain rail', 'Railway (RAIL, 75 a tile)', 'Electrified (ELRL, 90 a tile)']);
  assert.equal(rail.value, 'ELRL', 'the rail type of the address, once the game offers it');
  await form.dispatch('submit');
  assert.deepEqual(planned, [['68,9,SW;45,9;ELRL', 'g2']]);
  assert.deepEqual(client.calls.at(-1), ['algoglyph.track_plan',
    { instance: 'g2', from: { x: 68, y: 9, dir: 'SW' }, to: { x: 45, y: 9 }, railtype: 'ELRL' }]);
  const [head, table, start] = out.kids;
  assert.equal(head.kids[0].className, `${TONE.green} text-xs font-medium px-2.5 py-0.5 rounded-sm`);
  assert.equal(head.textContent, 'found1,650 (game money, exact)2 piece(s)');
  assert.deepEqual(all(table, 'tr').map((r) => r.textContent), ['tiles22', 'length22.5']);
  await click(start);
  assert.deepEqual(started, [{ x: 68, y: 9, dir: 'SW' }]);
  // Unreachable and unpriced; then a planner error.
  plan = { status: 'unreachable' };
  await form.dispatch('submit');
  assert.ok(out.kids[0].kids[0].className.startsWith(TONE.yellow));
  assert.equal(out.kids[0].textContent, 'unreachable0 piece(s)');
  plan = Promise.reject(new Error('no route'));
  plan.catch(() => {});
  await form.dispatch('submit');
  assert.equal(out.textContent, 'no route');
});

test('the planner falls back to plain rail and refuses without a game that publishes the map', async () => {
  const container = env.document.createElement('div');
  let fail = false;
  const client = hub(() => { if (fail) throw new Error('no railtypes'); return railtypes; });
  trackPlanView(container, client, { ops: planOps, plan: 'not a plan' });
  await env.settle();
  const [game, , rail] = all(container.kids[0], 'select');
  assert.equal(game.value, 'g1', 'the first game without one in the address');
  assert.equal(rail.value, '', 'a rail type the address does not name is plain rail');
  fail = true;
  game.value = 'g2';
  await game.dispatch('change');
  await env.settle();
  assert.equal(rail.kids.length, 3, 'a game without openttd://railtypes keeps the list it had');
  const none = env.document.createElement('div');
  const quiet = hub();
  trackPlanView(none, quiet);
  await env.settle();
  assert.equal(none.kids[0].textContent, 'No instance publishes openttd://map.');
  assert.deepEqual(quiet.calls, [], 'no game, no rail types asked for');
});

test('the one chart door refuses an empty box, names the chart and follows its box', () => {
  assert.equal(ecInit(null, env.echarts), null);
  const box = env.document.createElement('div');
  box.setAttribute('aria-label', 'Money per company');
  box.offsetWidth = 0;
  assert.equal(ecInit(box, globalThis.echarts), null, 'a zero-width box would draw at zero size');
  box.offsetWidth = 640;
  const observed = [];
  const RO = globalThis.ResizeObserver;
  globalThis.ResizeObserver = class { constructor(f) { this.f = f; observed.push(this); } observe(b) { this.box = b; } };
  const mm = globalThis.matchMedia;
  globalThis.matchMedia = () => ({ matches: true });
  try {
    const chart = ecInit(box, globalThis.echarts);
    chart.setOption({ series: [] }, true);
    const o = chart.getOption();
    assert.equal(o.animation, false, 'reduced motion is honoured');
    assert.equal(o.aria.label.description, 'Money per company');
    assert.equal(o.animationDelay(100), 420);
    assert.deepEqual(o.series, []);
    assert.equal(observed.length, 1);
    observed[0].f();
    assert.equal(chart.resized, 1, 'a new box size redraws the chart');
    assert.equal(ecInit(box, globalThis.echarts).box, box, 'the same box keeps its chart');
    assert.equal(observed.length, 1, 'and its one observer');
  } finally {
    globalThis.ResizeObserver = RO;
    globalThis.matchMedia = mm;
  }
});

test('a maximizable card: the button, Escape and the address agree', async () => {
  const card = env.document.createElement('div');
  const states = [];
  let resized = 0;
  const button = maximizable(card, 'map', { getState: () => ({ max: '' }), setState: (p) => states.push(p), onResize: () => resized++ });
  assert.equal(button.className, UI.secondary);
  assert.equal(card.classList.contains('fixed'), false);
  await click(button);
  for (const c of ['fixed', 'inset-0', 'z-50', 'm-0', 'overflow-auto']) assert.ok(card.classList.contains(c), c);
  assert.ok(env.document.body.classList.contains('overflow-hidden'));
  await env.document.dispatch('keydown', { key: 'Enter' });
  assert.ok(card.classList.contains('fixed'), 'only Escape restores');
  await env.document.dispatch('keydown', { key: 'Escape' });
  assert.equal(card.classList.contains('fixed'), false);
  assert.equal(env.document.activeElement, button, 'focus returns to the button');
  assert.deepEqual(states, [{ max: 'map' }, { max: undefined }]);
  await env.tick();
  assert.equal(resized, 2);
  await env.document.dispatch('keydown', { key: 'Escape' });
  assert.equal(states.length, 2, 'Escape on a card that is not maximized does nothing');
  const bare = env.document.createElement('div');
  await click(maximizable(bare, 'x'));
  assert.ok(bare.classList.contains('fixed'), 'without state or resize hooks it still maximizes');
});
