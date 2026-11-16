// node --test tools/research/mcp-panel/test/ : the pure parts of static/components.js.
import test from 'node:test';
import assert from 'node:assert/strict';
import { schemaFields, coerce, callParams, parseHash, formatHash, groupOps, HubClient } from '../static/components.js';

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
  const s = { instance: 'panel-8800', op: 'panel-8800/status', q: 'view zoom', kinds: ['game', 'company'] };
  assert.deepEqual(parseHash(formatHash(s)), s);
  assert.deepEqual(parseHash(''), { instance: '', op: '', q: '', kinds: [] });
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
