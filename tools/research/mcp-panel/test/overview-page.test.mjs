// node --test tools/research/mcp-panel/test/ : the overview's live sections (static/overview.js)
// mounted on the real index.html in the fake browser of fake-dom.mjs: the history pulled from
// GET /api/overview/history?since= (panel.rs h_history, samples as sample_of writes them), the
// event stream that triggers a pull, the four charts, their focus and address state, the tiles and
// a new world starting the history again. The fixtures are fictional; nothing talks to a game.
import './panel-ui.mjs'; // first: shell.js publishes PanelUI before any page script
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fakeDom } from './fake-dom.mjs';

const here = path.dirname(new URL(import.meta.url).pathname);
const INDEX = fs.readFileSync(path.join(here, '..', 'static', 'index.html'), 'utf8');
const bodyOf = (html) => html.slice(html.indexOf('>', html.indexOf('<body')) + 1, html.lastIndexOf('</body>'));

const date = (i) => new Date(Date.UTC(1950, 0, 1 + i)).toISOString().slice(0, 10);
const company = (id, name, money, loan, value) => ({ id, name, is_ai: true, money, value, loan, net: money - loan, fleet: 3 });
/** One sample per game day, as panel.rs sample_of writes it. */
const sample = (i, companies, fleet = { train: Math.floor(i / 10), road: 2, ship: 0, aircraft: 0, total: 2 + Math.floor(i / 10) }) => ({
  t: 74 * (i + 1), date: date(i), vehicles: fleet ? fleet.total : 40, fleet, towns: 9, paused: false, companies,
});

let history = [sample(0, []), sample(1, [])];
let fail = false;
const routes = (url) => {
  if (fail) throw new Error('connection refused');
  const m = /^\/api\/overview\/history\?since=(-?[\d.]+)$/.exec(url);
  if (!m) return { error: 'no route' };
  const since = Number(m[1]);
  const samples = history.filter((s) => s.t > since);
  return { since, last: history.length ? history.at(-1).t : null, count: samples.length, samples };
};

const env = fakeDom({ url: 'http://127.0.0.1:8800/?ov_metric=value#overview', routes });
env.document.body.innerHTML = bodyOf(INDEX);
await import('../static/overview.js');
const $ = env.byId;
const chart = (id) => env.charts.find((c) => c.box === $(id));
const query = (k) => new URL(env.location.href).searchParams.get(k);
const hidden = (id) => $(id).classList.contains('hidden');

test('the controls come from the registry, the metric from the address, and #overview is held in view', () => {
  assert.deepEqual($('ov-metric').options.map((o) => o.value), ['net', 'value', 'money', 'loan']);
  assert.equal($('ov-metric').value, 'value');
  assert.deepEqual($('ov-range').options.map((o) => o.value), ['90', '365', '1825', 'all']);
  assert.equal($('ov-range').value, 'all');
  for (const id of ['ov-companies', 'ov-standing', 'ov-monthly', 'ov-fleet']) {
    const buttons = $(`${id}-card`).querySelectorAll('[data-toolbar] button');
    assert.equal(buttons.at(-1).textContent, 'Maximize', `${id} can maximize`);
  }
  assert.ok(env.windowListeners.wheel, 'the reader scrolling releases the anchor');
});

test('the event stream says it is live, and its moments pull the history', async () => {
  const es = env.sources.at(-1);
  assert.match(es.src, /^\/api\/events\/stream\?kinds=game\.month,vehicles\.count,company\.founded/);
  es.onopen();
  assert.equal($('ov-live').querySelector('[data-live-text]').textContent, 'live');
  assert.equal($('ov-live').dataset.state, 'live');
  es.onerror();
  assert.equal($('ov-live').querySelector('[data-live-text]').textContent, 'reconnecting');
  assert.equal($('ov-live').dataset.state, 'off');
  for (const k of ['game.month', 'vehicles.count', 'company.founded', 'company.removed', 'game.new_world']) assert.ok(es.listeners[k], k);
});

test('before the first company the charts say so instead of drawing an empty frame', async () => {
  await env.tick();
  assert.equal(env.fetches[0].url, '/api/overview/history?since=-1');
  assert.equal(env.fetches[1].url, '/api/overview/history?since=148', 'the next pull asks only what is newer');
  assert.equal($('ov-samples').textContent, '2 game days kept · 1950-01-01 to 1950-01-02');
  assert.ok(!hidden('ov-companies-empty'));
  assert.ok(!hidden('ov-standing-empty'));
  assert.equal($('ov-month-net').textContent, '-');
  assert.equal($('ov-month-sub').textContent, 'no company yet');
  assert.equal(chart('ov-companies').getOption().series.length, 0);
});

test('one company gets its balance sheet; its lines, monthly bars and fleet are drawn', async () => {
  const one = (i) => [company(0, 'Planned Transport', 100000 + 1000 * i, 300000, 50000 + 2000 * i)];
  history = [...history, ...Array.from({ length: 60 }, (_, k) => sample(k + 2, one(k + 2)))];
  await env.sources.at(-1).listeners['game.month'][0]();
  await env.settle();
  assert.ok(hidden('ov-companies-empty'));
  assert.equal($('ov-standing-title').textContent, 'Balance sheet today');
  assert.match($('ov-standing-note').textContent, /^Planned Transport: money, loan, value and net worth/);
  assert.equal($('ov-leader').textContent, 'Planned Transport');
  assert.equal($('ov-samples').textContent, '62 game days kept · 1950-01-01 to 1950-03-03');
  assert.equal($('ov-vehicles-delta').textContent, '+3 in 30 days');
  const lines = chart('ov-companies').getOption();
  assert.equal(lines.yAxis.name, 'Company value', 'the metric of the address');
  assert.equal(lines.series[0].data.at(-1)[1], 50000 + 2000 * 61);
  assert.deepEqual(chart('ov-standing').getOption().series[0].data.map((d) => d.metric), ['net', 'value', 'money', 'loan']);
  assert.deepEqual(chart('ov-monthly').getOption().xAxis.data, ['1950-01', '1950-02', '1950-03']);
  assert.deepEqual(chart('ov-fleet').getOption().series.map((s) => s.id), ['fleet-train', 'fleet-road']);
});

test('two companies are ranked; a click focuses one in every chart and the address keeps it', async () => {
  const two = (i) => [company(0, 'Planned Transport', 100000 + 1000 * i, 300000, 50000 + 2000 * i), company(1, 'Rival Transport', 900000, 0, 10000)];
  history = [...history, sample(62, two(62)), sample(63, two(63))];
  await env.tick();
  assert.equal($('ov-standing-title').textContent, 'Standing today');
  assert.equal($('ov-leader').textContent, 'leader: Planned Transport', 'leader by company value');
  assert.ok(hidden('ov-focus-clear'));
  chart('ov-standing').handlers.click({ data: { id: '1' } });
  assert.equal(query('ov_focus'), '1');
  assert.ok(!hidden('ov-focus-clear'));
  assert.equal(chart('ov-companies').getOption().series.find((s) => s.id === '0').lineStyle.opacity, 0.25, 'the others dim');
  chart('ov-standing').handlers.click({ data: { id: '1' } });
  assert.equal(query('ov_focus'), null, 'a second click on the same company releases it');
  chart('ov-companies').handlers.click({ seriesId: '0' });
  assert.equal(query('ov_focus'), '0');
  chart('ov-monthly').handlers.click({ seriesId: '1' });
  assert.equal(query('ov_focus'), '1');
  $('ov-focus-clear').onclick();
  assert.equal(query('ov_focus'), null);
  chart('ov-fleet').handlers.click({});
  assert.equal(env.location.pathname, '/resource/vehicles', 'the fleet leads to the vehicles');
  env.location.href = 'http://127.0.0.1:8800/?ov_metric=value#overview';
});

test('the metric and the range go into the address and redraw every chart', async () => {
  $('ov-metric').onchange({ target: { value: 'loan' } });
  assert.equal(query('ov_metric'), 'loan');
  assert.equal(chart('ov-companies').getOption().yAxis.name, 'Loan');
  assert.match($('ov-monthly-note').textContent, /each company's loan moved in each game month/);
  assert.equal($('ov-month-sub').textContent, 'loan, 1950-02, all companies', 'the last complete month');
  $('ov-range').onchange({ target: { value: '90' } });
  assert.equal(query('ov_range'), '90');
  assert.equal(chart('ov-companies').getOption().dataZoom[0].startValue, Date.parse(date(63)) - 90 * 86400000);
});

test('a chart maximizes into the address and resizes to its new box', async () => {
  const card = $('ov-fleet-card');
  const before = chart('ov-fleet').resized;
  await card.querySelector('[data-toolbar] button').click();
  assert.ok(card.classList.contains('fixed'));
  assert.equal(query('max'), 'ov-fleet');
  await env.tick();
  assert.ok(chart('ov-fleet').resized > before);
  await env.document.dispatch('keydown', { key: 'Escape' });
  assert.equal(query('max'), null);
});

test('a new world starts the history again; a panel that is down is retried on the next pull', async () => {
  history = [sample(0, [company(4, null, 5, 0, 5)])];
  await env.sources.at(-1).listeners['game.new_world'][0]();
  await env.settle();
  assert.ok(env.fetches.at(-1).url.endsWith('since=-1'), 'a history older than the cursor is read from the start');
  assert.equal($('ov-samples').textContent, '1 game days kept · 1950-01-01 to 1950-01-01');
  assert.equal($('ov-leader').textContent, 'company 4', 'a company without a name yet is named by its id');
  assert.equal(chart('ov-companies').getOption().series[0].name, 'company 4');
  fail = true;
  await env.sources.at(-1).listeners['vehicles.count'][0]();
  assert.equal($('ov-samples').textContent, '1 game days kept · 1950-01-01 to 1950-01-01', 'nothing lost');
  fail = false;
});
