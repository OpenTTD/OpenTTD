// node --test tools/research/mcp-panel/test/ : the /data page (static/data.js, ADR 0033): examples
// derived from the schema, the axes inferred from an answer, the chart option rendered server-side by
// ECharts itself, the bus snippets, and the registry against static/data.html.
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { createRequire } from 'node:module';
import {
  CHARTS, ADR_SCHEMA, normalizeSchema, exampleQueries, cellLink, columnKinds, inferAxes, chartOption,
  rowArrays, getState, queryHref, busSnippets, busTopics, parseEndpoint, cellText,
} from '../static/data.js';

const here = path.dirname(new URL(import.meta.url).pathname);
const STATIC = path.join(here, '..', 'static');
const echarts = createRequire(import.meta.url)('echarts');

function render(option) {
  const chart = echarts.init(null, null, { renderer: 'svg', ssr: true, width: 900, height: 420 });
  chart.setOption({ animation: false, ...option });
  const svg = chart.renderToSVGString();
  chart.dispose();
  return { texts: (svg.match(/<text/g) || []).length, paths: (svg.match(/<path/g) || []).length, svg };
}

const day = (i) => new Date(Date.UTC(1950, 0, 1 + i)).toISOString().slice(0, 10);
// Company value over time, long form as samples answers it: one row per company per day.
const valueAnswer = {
  columns: ['game_date', 'name', 'value'],
  rows: Array.from({ length: 30 }, (_, i) => [[day(i), 'Planned Transport', 1000 * i], [day(i), 'Rival Transport', 100 * i]]).flat(),
  count: 60, capped: false,
};

test('the schema is read from any shape the API answers in', () => {
  const list = normalizeSchema({ tables: [{ name: 'events', columns: [{ name: 'kind', type: 'VARCHAR' }, 'seq'] }] });
  assert.deepEqual(list, [{ name: 'events', columns: [{ name: 'kind', type: 'VARCHAR' }, { name: 'seq', type: '' }] }]);
  assert.deepEqual(normalizeSchema({ tables: { runs: ['seed'] } }), [{ name: 'runs', columns: [{ name: 'seed', type: '' }] }]);
  const described = normalizeSchema({ columns: ['table_name', 'column_name', 'data_type'],
    rows: [['events', 'kind', 'VARCHAR'], ['events', 'data', 'JSON'], ['samples', 'value', 'BIGINT']] });
  assert.deepEqual(described.map((t) => [t.name, t.columns.map((c) => c.name)]), [['events', ['kind', 'data']], ['samples', ['value']]]);
  assert.deepEqual(normalizeSchema(null), []);
  assert.deepEqual(ADR_SCHEMA.map((t) => t.name), ['runs', 'events', 'samples']);
});

test('examples are derived from the schema: each only when its table has the columns it reads', () => {
  const ex = exampleQueries(ADR_SCHEMA);
  const ids = ex.map((e) => e.id);
  for (const id of ['events-by-kind', 'latest-events', 'events-per-day', 'company-value', 'ai-log-word', 'runs']) assert.ok(ids.includes(id), id);
  assert.deepEqual(ids.filter((i) => i.startsWith('first-')), ['first-runs', 'first-events', 'first-samples'], 'one per table');
  const by = Object.fromEntries(ex.map((e) => [e.id, e]));
  assert.equal(by['events-by-kind'].sql, 'SELECT kind, count(*) AS n FROM events GROUP BY kind ORDER BY n DESC');
  assert.equal(by['latest-events'].sql, 'SELECT seq, ts_ms, kind, game_date, data FROM events ORDER BY seq DESC LIMIT 50');
  assert.equal(by['company-value'].sql, 'SELECT game_date, name, value FROM samples ORDER BY game_date, name');
  assert.match(by['ai-log-word'].sql, /WHERE kind = 'ai\.log' AND CAST\(data AS VARCHAR\) ILIKE '%station%'/);
  assert.match(exampleQueries(ADR_SCHEMA, { word: "o'clock" }).find((e) => e.id === 'ai-log-word').sql, /'%o''clock%'/, 'the word is quoted');
  assert.equal(by['latest-events'].href, queryHref(by['latest-events'].sql), 'an example is a link');
  assert.equal(getState('http://x' + by['company-value'].href).sql, by['company-value'].sql, 'and the link round-trips');

  const bare = exampleQueries([{ name: 'events', columns: [{ name: 'ts_ms', type: '' }] }, { name: 'Odd Table', columns: [] }]);
  assert.deepEqual(bare.map((e) => e.id), ['latest-events', 'first-events', 'first-Odd Table']);
  assert.equal(bare[0].sql, 'SELECT ts_ms FROM events ORDER BY ts_ms DESC LIMIT 50');
  assert.equal(bare[2].sql, 'SELECT * FROM "Odd Table" LIMIT 50', 'an identifier that needs quoting is quoted');
});

test('a cell leads to its page: a kind to its events, a company to its samples', () => {
  assert.equal(getState('http://x' + cellLink('kind', "game.month")).sql, "SELECT * FROM events WHERE kind = 'game.month' ORDER BY seq DESC LIMIT 50");
  assert.equal(getState('http://x' + cellLink('company', 1)).sql, 'SELECT * FROM samples WHERE company = 1 ORDER BY game_date');
  assert.equal(cellLink('value', 5), '');
  assert.equal(cellLink('kind', null), '');
  assert.equal(cellLink('kind', 'x', [{ name: 'runs', columns: [] }]), '', 'no events table, no link');
});

test('columns are typed from their values: dates, epoch milliseconds, numbers, text, json', () => {
  const kinds = columnKinds({ columns: [{ name: 'game_date' }, 'ts_ms', 'money', 'kind', 'data', 'nothing'],
    rows: [['1950-01-01', 1700000000000, '12', 'a', { x: 1 }, null], ['1950-01-02', 1700000001000, 13.5, 'b', null, null]] });
  assert.deepEqual(kinds.map((k) => k.kind), ['date', 'epoch', 'number', 'text', 'json', 'empty']);
  assert.deepEqual(rowArrays({ columns: ['a', 'b'], rows: [{ b: 2, a: 1 }] }), [[1, 2]], 'an object row is read by column name');
  assert.equal(cellText({ a: 1 }), '{"a":1}');
  assert.equal(cellText(null), 'NULL');
});

test('axes: a time-like x, numeric y without ids, one line per company; picks from the address win', () => {
  const a = inferAxes(valueAnswer);
  assert.deepEqual([a.x, a.xType, a.y, a.by], ['game_date', 'time', ['value'], 'name']);
  const samples = { columns: ['ts_ms', 'game_tick', 'company', 'money', 'loan'], rows: [[1, 74, 0, 10, 5], [2, 148, 1, 20, 5], [3, 222, 0, 30, 4]] };
  const s = inferAxes(samples);
  assert.equal(s.x, 'ts_ms', 'epoch milliseconds before a tick');
  assert.equal(s.by, 'company', 'a company id groups');
  assert.deepEqual(s.y, ['money', 'loan'], 'ids are not drawn by default');
  assert.deepEqual(inferAxes(samples, { x: 'game_tick', y: ['loan'], by: 'none' }), { ...s, x: 'game_tick', xType: 'value', y: ['loan'], by: '',
    xs: ['ts_ms', 'game_tick', 'company', 'money', 'loan'], ys: ['ts_ms', 'company', 'money', 'loan'] });
  const ignored = inferAxes(samples, { x: 'nope', y: ['kind'], by: 'nope' });
  assert.equal(ignored.x, 'ts_ms', 'a pick that is not a column is ignored');
  assert.equal(ignored.by, '', 'an unusable by splits nothing');
  assert.equal(inferAxes({ columns: ['kind', 'n'], rows: [['a', 1], ['b', 2]] }), null, 'a category and a count: no line chart');
  assert.equal(inferAxes({ columns: ['n'], rows: [[1]] }), null, 'one number: nothing to draw against');
  assert.equal(inferAxes({ columns: ['count_star()'], rows: [] }), null, 'no rows');
  assert.equal(inferAxes({ columns: ['a', 'b'], rows: [[1, 2], [3, 4]] }).x, 'a', 'two plain numbers: the first across');
});

test('the chart option: one line per group, points in x order carrying their row, rendered by ECharts', () => {
  const o = chartOption(valueAnswer);
  assert.deepEqual(o.series.map((s) => s.name), ['Planned Transport', 'Rival Transport']);
  assert.equal(o.xAxis.type, 'time');
  assert.equal(o.yAxis.name, 'value');
  assert.equal(o.dataZoom[0].zoomOnMouseWheel, 'ctrl', 'the wheel zooms only with Ctrl');
  assert.deepEqual(o.series[0].data[1].value, [Date.parse(day(1)), 1000]);
  assert.equal(o.series[1].data[1].row, 3, 'a point leads back to its row of the table');
  assert.ok(o.series.every((s) => s.id), 'every series has a stable id');
  const shuffled = chartOption({ columns: ['game_tick', 'value'], rows: [[3, 30], [1, 10], [2, 20]] });
  assert.deepEqual(shuffled.series[0].data.map((d) => d.value[0]), [1, 2, 3], 'sorted by x');
  const two = chartOption({ columns: ['game_tick', 'company', 'money', 'loan'], rows: [[1, 0, 1, 2], [1, 1, 3, 4], [2, 0, 5, 6]] });
  assert.deepEqual(two.series.map((s) => s.name), ['0 · money', '0 · loan', '1 · money', '1 · loan']);
  assert.equal(chartOption({ columns: ['kind', 'n'], rows: [['a', 1]] }), null);
  const r = render(o);
  assert.ok(r.texts > 8 && r.paths > 3, `rendered ${r.texts} texts, ${r.paths} paths`);
  assert.ok(r.svg.includes('Rival Transport'));
  assert.ok(render(two).paths >= 4);
});

test('the bus: endpoint, topics from a list or a map, and four ways to subscribe', () => {
  assert.deepEqual(parseEndpoint('tcp://127.0.0.1:5555'), { host: '127.0.0.1', port: 5555 });
  assert.equal(parseEndpoint('ipc:///tmp/x'), null);
  assert.deepEqual(busTopics({ topics_seen: ['game.month'] }), [{ topic: 'game.month', count: null }]);
  assert.deepEqual(busTopics({ topics_seen: { 'ai.log': 2, 'game.month': 9 } }).map((t) => t.topic), ['game.month', 'ai.log'], 'most first');
  assert.deepEqual(busTopics({}), []);
  const s = busSnippets('tcp://127.0.0.1:5555');
  assert.deepEqual(s.map((x) => x.id), ['rust', 'python', 'node', 'elixir']);
  for (const x of s.slice(0, 3)) assert.ok(x.code.includes('tcp://127.0.0.1:5555'), `${x.id} connects to the endpoint`);
  assert.match(s[3].code, /:chumak\.connect\(sock, :tcp, ~c"127\.0\.0\.1", 5555\)/);
  assert.match(s[1].code, /recv_multipart/, 'the topic and the envelope are two frames');
  assert.match(busSnippets('tcp://127.0.0.1:5555', 'game.').find((x) => x.id === 'python').code, /SUBSCRIBE, "game\."/);
  assert.match(busSnippets(undefined)[0].code, /<port>/, 'without a bus the snippets say where the port goes');
});

test('the registry against data.html: every chart has a named box on a maximizable card, and every card is registered', () => {
  const html = fs.readFileSync(path.join(STATIC, 'data.html'), 'utf8');
  for (const c of CHARTS) {
    const box = html.match(new RegExp(`<div id="${c.id}"[^>]*>`));
    assert.ok(box, `${c.id} has a box`);
    assert.match(box[0], /role="img"/);
    assert.match(box[0], /aria-label="[^"]+"/);
    assert.ok(html.includes(`data-chart="${c.id}"`), `${c.id} has a maximizable card`);
    assert.ok(c.params.includes('max') && c.drill, `${c.id} declares its parameters and where a click goes`);
  }
  for (const m of html.matchAll(/data-chart="([^"]+)"/g)) assert.ok(CHARTS.some((c) => c.id === m[1]), `${m[1]} is in the registry`);
  assert.match(html, /<script type="module" src="\/static\/data\.js"><\/script>/);
  for (const ph of ['__TITLE__', '__DESCRIPTION__']) assert.ok(html.includes(ph), `${ph} is filled by the panel`);
  const src = fs.readFileSync(path.join(STATIC, 'data.js'), 'utf8');
  assert.match(src, /maximizable\(card, 'data-chart'/, 'the card maximizes through components.js');
  assert.match(src, /ecInit\(box, echarts\)/, 'the chart goes through the one door');
});

test('the sidebar has the page, next to the duels', () => {
  const { PAGES, pageOf } = createRequire(import.meta.url)('../static/shell.js');
  const keys = PAGES.map((p) => p.key);
  assert.equal(keys[keys.indexOf('results') + 1], 'data');
  assert.equal(PAGES.find((p) => p.key === 'data').label, 'Data: query the event store');
  assert.equal(pageOf('/data'), 'data');
});
