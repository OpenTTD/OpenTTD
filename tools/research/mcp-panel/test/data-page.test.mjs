// node --test tools/research/mcp-panel/test/ : the /data page (static/data.js) mounted on the real
// data.html in the fake browser of fake-dom.mjs: the examples and tables from GET /api/db/tables, a
// query run through POST /api/db/query (form, Ctrl+Enter, a link), the answer table, its chart and
// the chart's address state, errors, and the bus card from GET /api/bus. The fixtures are shaped like
// panel_store.rs answers with fictional values; nothing here talks to a panel.
import './panel-ui.mjs'; // first: shell.js publishes PanelUI before any page script
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fakeDom, respond } from './fake-dom.mjs';
import { chartOption, page } from '../static/data.js';

const here = path.dirname(new URL(import.meta.url).pathname);
const DATA = fs.readFileSync(path.join(here, '..', 'static', 'data.html'), 'utf8');
const bodyOf = (html) => html.slice(html.indexOf('>', html.indexOf('<body')) + 1, html.lastIndexOf('</body>'));

/** GET /api/db/tables (panel_store.rs h_db_tables). */
const tables = {
  store: { file: '/tmp/fictional.duckdb', open: true },
  tables: [
    { name: 'events', columns: [{ name: 'instance', type: 'VARCHAR' }, { name: 'seq', type: 'BIGINT' }, { name: 'ts_ms', type: 'BIGINT' }, { name: 'kind', type: 'VARCHAR' }, { name: 'game_date', type: 'VARCHAR' }, { name: 'data', type: 'JSON' }] },
    { name: 'samples', columns: [{ name: 'game_date', type: 'VARCHAR' }, { name: 'company', type: 'INTEGER' }, { name: 'name', type: 'VARCHAR' }, { name: 'value', type: 'DOUBLE' }] },
  ],
};
/** GET /api/bus (panel_store.rs h_bus). */
const bus = {
  endpoint: 'tcp://127.0.0.1:5601', schema: 'prismatic-bus/v1', instance: 'panel-8800', session: 's1',
  topics_seen: { 'ai.log': 40, 'game.month': 12 }, published: 1052, error: null,
  frames: ['topic (the event kind)', 'envelope (JSON, schema prismatic-bus/v1)'],
};
const day = (i) => new Date(Date.UTC(1950, 0, 1 + i)).toISOString().slice(0, 10);
const VALUE = 'SELECT game_date, name, value FROM samples ORDER BY game_date, name';
const answers = {
  [VALUE]: { columns: ['game_date', 'name', 'value'], rows: Array.from({ length: 5 }, (_, i) => [[day(i), 'Planned Transport', 1000 * i], [day(i), 'Rival Transport', 100 * i]]).flat(), count: 10, capped: false },
  'SELECT kind, count(*) AS n FROM events GROUP BY kind ORDER BY n DESC': { columns: ['kind', 'n'], rows: [['ai.log', 40], ['game.month', 12]], count: 2, capped: false },
  'SELECT * FROM events LIMIT 2': { columns: ['seq', 'kind', 'data'], rows: [[1, 'game.month', { date: '1950-02-01', vehicles: 3 }], [2, 'ai.log', null]], count: 5000, capped: true },
};

let tablesAnswer = () => tables;
let busAnswer = () => bus;
const posts = [];
const routes = (url, init) => {
  const body = init.body ? JSON.parse(init.body) : undefined;
  if (init.method === 'POST') posts.push([url, body]);
  if (url === '/api/db/tables') return tablesAnswer();
  if (url === '/api/bus') return busAnswer();
  if (url === '/api/db/query') {
    if (body.sql === 'DROP TABLE events') return respond({ error: 'only one read-only statement (SELECT, WITH, DESCRIBE, SUMMARIZE, SHOW)' }, { status: 400 });
    return answers[body.sql] || { columns: [], rows: [], count: 0, capped: false };
  }
  return respond({ error: 'no route' }, { status: 404 });
};

const env = fakeDom({ url: 'http://127.0.0.1:8800/data', routes });
env.document.body.innerHTML = bodyOf(DATA);
page();
const $ = env.byId;
const chart = () => env.charts.find((c) => c.box === $('data-chart'));
const query = (k) => new URL(env.location.href).searchParams.get(k);
const submit = async (sql) => {
  $('sql').value = sql;
  await $('query-form').onsubmit({ preventDefault() {} });
  await env.settle();
};

test('the examples and tables come from the store schema; the first example waits in the box', async () => {
  await env.settle();
  const ids = $('example-list').querySelectorAll('[data-example]').map((a) => a.dataset.example);
  assert.ok(ids.includes('company-value') && ids.includes('ai-log-word'), ids.join());
  assert.ok(!ids.includes('runs'), 'a table the store does not have gives no example');
  assert.deepEqual($('table-list').querySelectorAll('[data-sql]').map((a) => a.textContent), ['events', 'samples']);
  assert.match($('table-list').innerHTML, /seq <span class="text-gray-400">bigint<\/span>/);
  assert.equal($('sql').value, 'SELECT seq, ts_ms, kind, game_date, data FROM events ORDER BY seq DESC LIMIT 50');
  assert.deepEqual(posts, [], 'nothing runs until the owner asks');
  assert.equal($('data-chart-empty').textContent, 'Run a query to chart its answer.');
  assert.ok($('data-chart').classList.contains('hidden'));
  assert.equal($('chart-x').disabled, true);
});

test('the bus card shows the endpoint, the topics as links to their events, and four snippets', () => {
  assert.equal($('bus-endpoint').textContent, 'tcp://127.0.0.1:5601');
  assert.equal($('bus-schema').textContent, 'prismatic-bus/v1');
  assert.equal($('bus-published').textContent, '1,052');
  const topics = $('bus-topics').querySelectorAll('a');
  assert.deepEqual(topics.map((a) => a.textContent.split(' ')[0]), ['ai.log', 'game.month'], 'most seen first');
  assert.equal(new URL(topics[1].href).searchParams.get('sql'), "SELECT * FROM events WHERE kind = 'game.month' ORDER BY seq DESC LIMIT 50");
  assert.deepEqual($('bus-snippets').querySelectorAll('[data-copy]').map((b) => b.dataset.copy), ['rust', 'python', 'node', 'elixir']);
  assert.match($('snippet-python').textContent, /sock\.connect\("tcp:\/\/127\.0\.0\.1:5601"\)/);
});

test('a query runs through POST /api/db/query and fills the table, the chart and its pickers', async () => {
  await submit(VALUE);
  assert.deepEqual(posts.at(-1), ['/api/db/query', { sql: VALUE }]);
  assert.equal(query('sql'), VALUE, 'the query is a link');
  assert.match($('run-msg').textContent, /^10 row\(s\) in \d+ ms$/);
  assert.equal($('result-count').textContent, '(10 rows)');
  assert.deepEqual($('data-head').querySelectorAll('th').map((t) => t.textContent), ['game_date', 'name', 'value']);
  assert.equal($('data-rows').querySelectorAll('tr').length, 10);
  assert.ok($('capped').classList.contains('hidden'));
  assert.ok(!$('data-chart').classList.contains('hidden'));
  assert.deepEqual(chart().getOption().series.map((s) => s.name), chartOption(answers[VALUE]).series.map((s) => s.name));
  assert.equal($('chart-x').value, 'game_date');
  assert.equal($('chart-by').value, 'name');
  assert.deepEqual($('chart-by').options.map((o) => o.value), ['none', 'name']);
});

test('a point leads to its row; the pickers go into the address and redraw', async () => {
  chart().handlers.click({ data: { row: 3 } });
  const picked = env.document.querySelector('#data-rows tr[data-picked]');
  assert.equal(picked.dataset.row, '3');
  assert.ok(picked.classList.contains('bg-primary-50'));
  assert.equal(picked.scrolled.length, 1, 'scrolled into view');
  chart().handlers.click({ data: { row: 5 } });
  assert.deepEqual(env.document.querySelectorAll('#data-rows tr[data-picked]').map((t) => t.dataset.row), ['5'], 'one row picked at a time');
  chart().handlers.click({ data: { row: 99 } });
  $('chart-by').onchange({ target: { value: 'none' } });
  assert.equal(query('by'), 'none');
  assert.deepEqual(chart().getOption().series.map((s) => s.name), ['value'], 'one line for everything');
  $('chart-y').onchange({ target: { value: 'value' } });
  assert.equal(query('y'), 'value');
  $('chart-x').onchange({ target: { value: 'game_date' } });
  assert.equal(query('x'), 'game_date');
  $('chart-y').onchange({ target: { value: '' } });
  assert.equal(query('y'), null);
});

test('an example, a table and a cell are links that run in place; a modified click is left to the browser', async () => {
  const example = $('example-list').querySelector('[data-example="events-by-kind"]');
  await example.dispatch('click', { button: 0 });
  await env.settle();
  assert.equal(posts.at(-1)[1].sql, 'SELECT kind, count(*) AS n FROM events GROUP BY kind ORDER BY n DESC');
  assert.equal($('query').scrolled.length, 1, 'the query box comes into view');
  assert.equal(query('by'), null, 'a new query forgets the previous pickers');
  assert.equal($('data-chart-empty').textContent, 'No chart for this answer: it needs a time-like or numeric column and another numeric one.');
  assert.ok(chart().cleared >= 1, 'the old lines are cleared');
  const table = $('table-list').querySelectorAll('[data-sql]').find((t) => t.textContent === 'samples');
  await table.dispatch('click', { button: 0 });
  await env.settle();
  assert.equal(posts.at(-1)[1].sql, 'SELECT * FROM samples LIMIT 50', 'a table runs its first rows');
  await example.dispatch('click', { button: 0 });
  await env.settle();
  const cell = $('data-rows').querySelector('[data-sql-link]');
  assert.equal(cell.textContent, 'ai.log');
  const n = posts.length;
  const ev = await cell.dispatch('click', { button: 0, ctrlKey: true });
  assert.equal(ev.defaultPrevented, false);
  assert.equal(posts.length, n, 'Ctrl+click opens a tab instead');
  await cell.dispatch('click', { button: 0 });
  await env.settle();
  assert.equal(posts.at(-1)[1].sql, "SELECT * FROM events WHERE kind = 'ai.log' ORDER BY seq DESC LIMIT 50");
});

test('Ctrl+Enter runs; a capped answer says so; JSON and NULL cells are shown as such', async () => {
  $('sql').value = 'SELECT * FROM events LIMIT 2';
  const ev = { key: 'Enter', ctrlKey: true, defaultPrevented: false, preventDefault() { this.defaultPrevented = true; } };
  for (const f of $('sql').listeners.keydown) f(ev);
  await env.settle();
  assert.ok(ev.defaultPrevented);
  assert.equal(posts.at(-1)[1].sql, 'SELECT * FROM events LIMIT 2');
  assert.equal($('result-count').textContent, '(5,000 rows)');
  assert.ok(!$('capped').classList.contains('hidden'));
  assert.equal($('capped').textContent, 'The answer is capped at 2 rows; add a LIMIT or an aggregate to see what you need.');
  const cells = $('data-rows').querySelectorAll('td');
  assert.equal(cells[2].textContent, '{"date":"1950-02-01","vehicles":3}');
  assert.ok(cells[2].classList.contains('font-mono'));
  assert.equal(cells[5].textContent, 'NULL');
  for (const f of $('sql').listeners.keydown) f({ key: 'Enter', preventDefault() { assert.fail('a bare Enter is a new line'); } });
});

test('a refused statement shows the error and clears the answer; an empty box runs nothing', async () => {
  await submit('DROP TABLE events');
  assert.ok(!$('query-error').classList.contains('hidden'));
  assert.equal($('query-error').textContent, 'only one read-only statement (SELECT, WITH, DESCRIBE, SUMMARIZE, SHOW)');
  assert.equal($('data-rows').innerHTML, '');
  assert.equal($('result-count').textContent, '');
  assert.equal($('data-chart-empty').textContent, 'Run a query to chart its answer.');
  const n = posts.length;
  await submit('   ');
  assert.equal(posts.length, n);
  await submit('SELECT 1 WHERE false');
  assert.ok($('query-error').classList.contains('hidden'), 'the next query clears the error');
  assert.match($('data-rows').innerHTML, /no rows/);
});

test('Copy puts a snippet on the clipboard, or says to select it when the browser refuses', async () => {
  const btn = $('bus-snippets').querySelector('[data-copy="node"]');
  await btn.dispatch('click');
  assert.equal(env.copied, $('snippet-node').textContent);
  assert.equal(btn.textContent, 'Copied');
  await env.tick();
  assert.equal(btn.textContent, 'Copy');
  navigator.clipboard.writeText = async () => { throw new Error('denied'); };
  await btn.dispatch('click');
  assert.equal(btn.textContent, 'Select and copy');
});

test('a query in the address runs on load with its pickers; a panel without the store says so', async () => {
  const sql = VALUE;
  const env2 = fakeDom({ url: `http://127.0.0.1:8800/data?sql=${encodeURIComponent(sql)}&by=none&y=value`, routes: (url, init) => {
    if (url === '/api/db/query') return respond({ error: 'no route' }, { status: 404 });
    return routes(url, init);
  } });
  env2.document.body.innerHTML = bodyOf(DATA);
  tablesAnswer = () => { throw new Error('connection refused'); };
  busAnswer = () => respond({ error: 'the bus tap is not running' }, { status: 503 });
  page();
  await env2.settle();
  const $2 = env2.byId;
  assert.equal($2('sql').value, sql);
  assert.match($2('query-error').textContent, /^This panel does not serve the event store yet \(no \/api\/db\/query; ADR 0033\)\. no route$/);
  assert.equal(new URL(env2.location.href).searchParams.get('by'), 'none', 'the pickers of a linked query are kept');
  assert.ok($2('example-list').querySelector('[data-example="runs"]'), 'without the schema the ADR\'s tables stand in');
  assert.equal($2('bus-endpoint').textContent, 'not served by this panel yet');
  assert.equal($2('bus-published').textContent, '-');
  assert.equal($2('bus-topics').innerHTML, 'none yet');
  assert.match($2('snippet-rust').textContent, /tcp:\/\/127\.0\.0\.1:<port>/);
});

test('with the store answering, a linked query draws with the pickers of its address', async () => {
  const env3 = fakeDom({ url: `http://127.0.0.1:8800/data?sql=${encodeURIComponent(VALUE)}&by=none&y=value`, routes });
  env3.document.body.innerHTML = bodyOf(DATA);
  tablesAnswer = () => ({ tables: [] });
  page();
  await env3.settle();
  const c = env3.charts.find((x) => x.box === env3.byId('data-chart'));
  assert.deepEqual(c.getOption().series.map((s) => s.name), ['value'], 'by=none from the address');
  assert.equal(env3.byId('chart-y').value, 'value');
});
