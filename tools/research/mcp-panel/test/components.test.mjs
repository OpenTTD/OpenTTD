// node --test tools/research/mcp-panel/test/ : the pure parts of static/components.js.
import test from 'node:test';
import assert from 'node:assert/strict';
import { schemaFields, coerce, callParams, parseHash, formatHash, groupOps, HubClient, formatPlan, parsePlan, trackPlanParams, planSummary, planInstances, railtypeOptions } from '../static/components.js';

const schema = { type: 'object', required: ['pause'], properties: {
  pause: { type: 'boolean' }, tile: { type: 'integer' }, mode: { enum: ['a', 'b'] }, name: { type: 'string' }, extra: { type: 'object' } } };

test('schema fields follow the schema, with kinds and required', () => {
  const f = schemaFields(schema);
  assert.deepEqual(f.map((x) => [x.name, x.kind, x.required]),
    [['pause', 'boolean', true], ['tile', 'number', false], ['mode', 'enum', false], ['name', 'string', false], ['extra', 'json', false]]);
  assert.deepEqual(schemaFields(undefined), []);
});

test('values are coerced, and bad numbers or JSON are refused', () => {
  assert.equal(coerce({ kind: 'number', name: 't' }, '12'), 12);
  assert.equal(coerce({ kind: 'boolean', name: 'p' }, true), true);
  assert.deepEqual(coerce({ kind: 'json', name: 'x' }, '{"a":1}'), { a: 1 });
  assert.equal(coerce({ kind: 'string', name: 's' }, ''), undefined);
  assert.throws(() => coerce({ kind: 'number', name: 't' }, 'x'), /not a number/);
  assert.throws(() => coerce({ kind: 'json', name: 'x' }, '{'), /not JSON/);
});

test('call params carry the body and query an operation declares', () => {
  const op = { instance: 'p', id: 'pause', schema, query: [{ name: 'since', required: false }] };
  assert.deepEqual(callParams(op, { pause: true, tile: '7', '?since': '3' }),
    { instance: 'p', id: 'pause', body: { pause: true, tile: 7 }, query: { since: '3' } });
  assert.throws(() => callParams(op, {}), /pause is required/);
  assert.deepEqual(callParams({ instance: 'p', id: 'status' }, {}), { instance: 'p', id: 'status' });
});

test('the page state round-trips through the URL hash', () => {
  const s = { instance: 'panel-8800', op: 'panel-8800/status', q: 'view zoom', kinds: ['game', 'company'], plan: '68,9,NE;45,9;ELRL' };
  assert.deepEqual(parseHash(formatHash(s)), s);
  assert.deepEqual(parseHash(''), { instance: '', op: '', q: '', kinds: [], plan: '' });
  assert.equal(formatHash({ kinds: [] }), '');
});

test('operations group by instance and tag and filter by text', () => {
  const ops = [{ instance: 'a', id: 'status', path: '/api/status', summary: '', tags: ['game'] },
               { instance: 'a', id: 'tool:openttd.view.zoom', path: 'openttd.view.zoom', summary: 'zoom', tags: ['tools'] },
               { instance: 'b', id: 'news', path: '/api/news', summary: '', tags: [] }];
  assert.deepEqual(Object.keys(groupOps(ops)), ['a', 'b']);
  assert.deepEqual(Object.keys(groupOps(ops).b), ['other']);
  assert.deepEqual(groupOps(ops, 'zoom'), { a: { tools: [ops[1]] } });
});

test('the hub client matches answers to requests, emits notifications and re-subscribes', async () => {
  const sent = [];
  class FakeWs { constructor() { FakeWs.last = this; } send(m) { sent.push(JSON.parse(m)); } }
  const c = new HubClient('ws://x/ws', { WebSocketImpl: FakeWs }).connect();
  const events = [];
  c.on('hub.event', (p) => events.push(p));
  const p = c.subscribe({ kinds: ['game'] });
  FakeWs.last.onmessage({ data: JSON.stringify({ jsonrpc: '2.0', id: sent[0].id, result: { subscribed: true } }) });
  assert.deepEqual(await p, { subscribed: true });
  FakeWs.last.onmessage({ data: JSON.stringify({ jsonrpc: '2.0', method: 'hub.event', params: { instance: 'a', event: { kind: 'game.month' } } }) });
  assert.equal(events[0].event.kind, 'game.month');
  const bad = c.call('hub.call', {});
  FakeWs.last.onmessage({ data: JSON.stringify({ jsonrpc: '2.0', id: sent[1].id, error: { code: -32602, message: 'no' } }) });
  await assert.rejects(bad, /no/);
  FakeWs.last.onopen();
  assert.equal(sent.at(-1).method, 'hub.subscribe', 'a reconnect re-subscribes with the same filter');
  assert.deepEqual(sent.at(-1).params, { kinds: ['game'] });
});

test('the audit summary counts audited, skipped and duplicate pairs', async () => {
  const { auditSummary, scrollOp } = await import('../static/components.js');
  const r = [{ instance: 'a', stations: 8, duplicates: [{}, {}], depots: { duplicates: [{}] } }, { instance: 'b', error: 'no stations operation' },
             { instance: 'c', stations: 2, duplicates: [], depots: { error: 'no depots operation' } }];
  assert.deepEqual(auditSummary(r), { instances: 3, audited: 2, skipped: 1, pairs: 2, depotPairs: 1 });
  assert.equal(scrollOp([{ id: 'status' }, { id: 'clientOpenttdViewScrollTo' }]).id, 'clientOpenttdViewScrollTo');
  assert.equal(scrollOp([{ id: 'x' }, { id: 'tool:openttd.view.scroll_to' }]).id, 'tool:openttd.view.scroll_to');
  assert.equal(scrollOp([{ id: 'x' }]), undefined);
});

import { degrees, worldGraphOption, waitingOption, moneyOption, KINDS, PALETTE } from '../static/components.js';

const world = { schema: 'world-graph/v1',
  nodes: [
    { id: 'town:1', kind: 'town', name: 'A', x: 10, y: 10 }, { id: 'town:2', kind: 'town', name: 'B', x: 40, y: 12 },
    { id: 'station:5', kind: 'station', name: 'A Stop', x: 11, y: 10, waiting: [{ cargo: 0, count: 30 }] },
    { id: 'station:6', kind: 'station', name: 'B Stop', x: 39, y: 12, waiting: [] },
    { id: 'vehicle:9', kind: 'vehicle', name: 'Bus', x: 20, y: 11 } ],
  edges: [
    { from: 'station:5', to: 'town:1', kind: 'serves_town' }, { from: 'station:6', to: 'town:2', kind: 'serves_town' },
    { from: 'vehicle:9', to: 'station:5', kind: 'order', seq: 0 }, { from: 'vehicle:9', to: 'station:6', kind: 'order', seq: 1 },
    { from: 'station:5', to: 'station:6', kind: 'cargo_link', usage: 7 } ] };

test('the world graph becomes a map-placed ECharts graph with every node and edge kept', () => {
  const o = worldGraphOption(world);
  const s = o.series[0];
  assert.equal(s.layout, 'none');
  assert.equal(s.data.length, 5);
  assert.equal(s.links.length, 5);
  assert.deepEqual(s.data.find((n) => n.id === 'town:2').x, 40);
  assert.equal(s.data.find((n) => n.id === 'vehicle:9').category, KINDS.indexOf('vehicle'));
  assert.deepEqual(o.color, PALETTE.dark);
});

test('kinds filter nodes and drop edges to hidden nodes; labels go to the highest degree only', () => {
  const s = worldGraphOption(world, { kinds: ['town', 'station'], labels: 1 }).series[0];
  assert.ok(s.data.every((n) => n.id.startsWith('town') || n.id.startsWith('station')));
  assert.ok(s.links.every((l) => !l.source.startsWith('vehicle') && !l.target.startsWith('vehicle')));
  assert.equal(s.data.filter((n) => n.label.show).length, 1);
});

test('force mode uses the given positions', () => {
  const s = worldGraphOption(world, { mode: 'force', positions: new Map([['town:1', [100, 200]]]) }).series[0];
  const t = s.data.find((n) => n.id === 'town:1');
  assert.deepEqual([t.x, t.y], [100, 200]);
});

test('degrees count both ends of every edge', () => {
  assert.equal(degrees(world.edges).get('station:5'), 3);
});

test('waiting bars list only stations with cargo, most first', () => {
  const o = waitingOption(world);
  assert.deepEqual(o.yAxis.data, ['A Stop']);
  assert.equal(o.series[0].data[0].value, 30);
});

test('money series has one line per company over the samples', () => {
  const o = moneyOption([{ t: 1, companies: [{ id: 0, name: 'Co', money: 5 }] }, { t: 2, companies: [{ id: 0, name: 'Co', money: 7 }] }]);
  assert.equal(o.series.length, 1);
  assert.deepEqual(o.series[0].data, [[1, 5], [2, 7]]);
  assert.equal(o.dataZoom[0].zoomOnMouseWheel, 'ctrl');
});

test('a track plan round-trips through the hash, and a broken one is not a plan', () => {
  const p = { from: { x: 68, y: 9, dir: 'NE' }, to: { x: 45, y: 9 }, railtype: 'ELRL' };
  assert.equal(formatPlan(p), '68,9,NE;45,9;ELRL');
  assert.deepEqual(parsePlan(formatPlan(p)), p);
  assert.deepEqual(parsePlan('1,2,SW;3,4'), { from: { x: 1, y: 2, dir: 'SW' }, to: { x: 3, y: 4 }, railtype: '' });
  assert.equal(parsePlan('1,2;3,4'), null);          // the start needs a heading
  assert.equal(parsePlan('a,2,NE;3,4'), null);
  assert.equal(parseHash(formatHash({ plan: formatPlan(p) })).plan, '68,9,NE;45,9;ELRL');
});

test('the planner call, its answer and the games and rail types come from what the hub returns', () => {
  assert.deepEqual(trackPlanParams('g', { from: { x: 1, y: 2, dir: 'NE' }, to: { x: 3, y: 4 }, railtype: '' }),
    { instance: 'g', from: { x: 1, y: 2, dir: 'NE' }, to: { x: 3, y: 4 } });
  const s = planSummary({ ok: true, result: { status: 'found', money: 1650.4, priced: { exact: true }, counts: { tiles: 22, curves: 0, length: 22.5 }, pieces: [1, 2] } });
  assert.deepEqual(s, { status: 'found', money: 1650, exact: true, counts: [['tiles', 22], ['curves', 0], ['length', 22.5]], pieces: 2 });
  assert.equal(planSummary({ status: 'unreachable' }).status, 'unreachable');
  assert.deepEqual(planInstances([{ instance: 'a', id: 'resource:openttd://map' }, { instance: 'b', id: 'tool:x' }, { instance: 'a', id: 'resource:openttd://map' }]), ['a']);
  const mcp = { contents: [{ text: JSON.stringify({ data: { types: [{ label: 'RAIL', name: 'Railway', build_cost: 75 }] } }) }] };
  assert.deepEqual(railtypeOptions(mcp), [{ label: 'RAIL', name: 'Railway', cost: 75 }]);
  assert.deepEqual(railtypeOptions({ data: { types: [] } }), []);
});

import { CLS, $, esc, whole, compact, TONE, badge, INK, AXIS, patchAddress } from '../static/components.js';

test('the shared kit: esc, number formats and the element of an id', () => {
  assert.equal(esc('<a href="x">&</a>'), '&lt;a href=&quot;x&quot;&gt;&amp;&lt;/a&gt;');
  assert.equal(esc(null), '');
  assert.equal(esc(undefined), '');
  assert.equal(esc(0), '0');
  assert.equal(whole.format(1234567), '1,234,567');
  assert.equal(compact.format(1234567), '1.2M');
  const doc = globalThis.document;
  globalThis.document = { getElementById: (id) => ({ id }) };
  try { assert.deepEqual($('x'), { id: 'x' }); } finally { globalThis.document = doc; }
});

test('the shared kit: Flowbite classes, badges and chart ink keep their exact strings', () => {
  assert.equal(CLS.field, 'bg-gray-50 border border-gray-300 text-gray-900 text-sm rounded-lg focus:ring-primary-500 focus:border-primary-500 block w-full p-2.5 dark:bg-gray-700 dark:border-gray-600 dark:text-white');
  assert.equal(badge('<b>'), `<span class="whitespace-nowrap text-xs font-medium px-2 py-0.5 rounded-sm ${TONE.gray}">&lt;b&gt;</span>`);
  assert.equal(badge('ok', 'green'), '<span class="whitespace-nowrap text-xs font-medium px-2 py-0.5 rounded-sm bg-green-100 text-green-800 dark:bg-green-900 dark:text-green-300">ok</span>');
  assert.deepEqual(Object.keys(TONE), ['green', 'yellow', 'blue', 'gray']);
  assert.equal(AXIS.axisLabel.color, INK.muted);
  assert.equal(AXIS.splitLine.lineStyle.type, 'dashed');
});

test('patchAddress sets a value, removes an empty one, maps its keys and adds no history entry', () => {
  const calls = [];
  const win = { location: { href: 'http://p/results?seed=1&metric=money&keep=1' }, history: { replaceState: (...a) => calls.push(a) } };
  patchAddress({ seed: '', metric: 'loan', max: undefined, compare: null, page: 0 }, undefined, win);
  assert.equal(calls.length, 1);
  assert.equal(String(calls[0][2]), 'http://p/results?metric=loan&keep=1&page=0');
  patchAddress({ metric: 'net', focus: 'x' }, (k) => `ov_${k}`, win);
  assert.equal(String(calls[1][2]), 'http://p/results?seed=1&metric=money&keep=1&ov_metric=net&ov_focus=x');
});
