// node --test tools/research/mcp-panel/test/ : the overview's chart options (static/overview.js),
// rendered server-side by ECharts itself, and the one-door rule over every panel script.
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { createRequire } from 'node:module';
import { CHARTS, METRICS, companiesOption, standingOption, fleetOption, delta, companyIds } from '../static/overview.js';

const here = path.dirname(new URL(import.meta.url).pathname);
const STATIC = path.join(here, '..', 'static');
const echarts = createRequire(import.meta.url)('echarts');

const day = (i) => new Date(Date.UTC(1950, 0, 1 + i)).toISOString().slice(0, 10);
const samples = Array.from({ length: 400 }, (_, i) => ({
  t: 74 * i, date: day(i), vehicles: Math.floor(i / 20), towns: 9, paused: false,
  companies: [
    { id: 0, name: 'Planned Transport', money: 1000 * i, value: 2000 * i, loan: 300000 - 500 * i, net: 1000 * i - (300000 - 500 * i) },
    ...(i > 100 ? [{ id: 1, name: 'Rival Transport', money: 100 * i, value: 200 * i, loan: 300000, net: 100 * i - 300000 }] : []),
  ],
}));

function render(option) {
  const chart = echarts.init(null, null, { renderer: 'svg', ssr: true, width: 900, height: 420 });
  chart.setOption({ animation: false, ...option });
  const svg = chart.renderToSVGString();
  chart.dispose();
  return { texts: (svg.match(/<text/g) || []).length, paths: (svg.match(/<path/g) || []).length, svg };
}

test('companies over time: one line per company, the chosen metric, a focus dims the others', () => {
  const o = companiesOption(samples, { metric: 'value', range: '90', focus: '1' });
  assert.deepEqual(o.series.map((s) => s.name), ['Planned Transport', 'Rival Transport']);
  assert.equal(o.series[0].data.at(-1)[1], 2000 * 399);
  assert.equal(o.series[1].data[0][1], null, 'a company that did not exist yet has no value');
  assert.equal(o.series[0].lineStyle.opacity, 0.25);
  assert.equal(o.series[1].lineStyle.width, 3.5);
  assert.equal(o.yAxis.name, METRICS.value);
  assert.equal(o.dataZoom[0].zoomOnMouseWheel, 'ctrl', 'the wheel zooms only with Ctrl');
  assert.equal(o.dataZoom[0].startValue, Date.parse(day(399)) - 90 * 86400000);
  assert.equal(companiesOption(samples, { range: 'all' }).dataZoom[0].startValue, undefined);
  const r = render(o);
  assert.ok(r.texts > 10 && r.paths > 4, `rendered ${r.texts} texts, ${r.paths} paths`);
  assert.ok(r.svg.includes('Rival Transport'));
});

test('standing: the last day, sorted, each bar carrying its company id for the focus', () => {
  const o = standingOption(samples, { metric: 'net' });
  assert.deepEqual(o.yAxis.data, ['Rival Transport', 'Planned Transport']);
  assert.deepEqual(o.series[0].data.map((d) => d.id), ['1', '0']);
  assert.ok(render(o).texts >= 4);
  assert.deepEqual(standingOption([], {}).series[0].data, [], 'no history, no bars');
});

test('fleet and the change over a period', () => {
  const o = fleetOption(samples, {});
  assert.equal(o.series[0].data.length, samples.length);
  assert.ok(render(o).paths >= 2);
  assert.equal(delta(samples, (s) => s.vehicles, 30), '+1 in 30 days');
  assert.equal(delta(samples.slice(0, 1), (s) => s.vehicles), '');
  assert.deepEqual(companyIds(samples), ['0', '1']);
});

test('the one door: only components.js creates a chart; every chart box is registered and named', () => {
  for (const f of fs.readdirSync(STATIC).filter((x) => x.endsWith('.js'))) {
    const src = fs.readFileSync(path.join(STATIC, f), 'utf8');
    const calls = src.split('\n').filter((l) => /echarts\.init\(/.test(l));
    if (f === 'components.js') { assert.equal(calls.length, 1, 'components.js ecInit is the door'); continue; }
    // A page may hand the door an echarts handle that only picks the theme.
    for (const l of calls) assert.match(l, /init: \(box, _theme, opts\) => echarts\.init\(box, 'dark', opts\)/, `${f} creates a chart outside ecInit: ${l.trim()}`);
  }
  const html = fs.readFileSync(path.join(STATIC, 'index.html'), 'utf8');
  for (const c of CHARTS) {
    const box = html.match(new RegExp(`<div id="${c.id}"[^>]*>`));
    assert.ok(box, `${c.id} has a box`);
    assert.match(box[0], /role="img"/);
    assert.match(box[0], /aria-label="[^"]+"/);
    assert.ok(html.includes(`data-chart="${c.id}"`), `${c.id} has a maximizable card`);
    assert.ok(c.params.length && c.drill, `${c.id} declares its parameters and where a click goes`);
  }
  const boxes = [...html.matchAll(/data-chart="([^"]+)"/g)].map((m) => m[1]);
  for (const b of boxes) assert.ok(CHARTS.some((c) => c.id === b), `${b} is in the registry`);
});

test('app.css is built from the current sources (tailwindcss -i input.css -o static/app.css --minify)', { skip: !process.env.PANEL_CSS_CHECK && 'set PANEL_CSS_CHECK=1 (needs the tailwindcss CLI)' }, async () => {
  const { execFileSync } = await import('node:child_process');
  const out = path.join(fs.mkdtempSync(path.join(process.env.TMPDIR || '/tmp', 'panel-css-')), 'app.css');
  execFileSync('tailwindcss', ['-i', 'input.css', '-o', out, '--minify'], { cwd: path.join(here, '..'), stdio: 'ignore' });
  assert.equal(fs.readFileSync(out, 'utf8'), fs.readFileSync(path.join(STATIC, 'app.css'), 'utf8'), 'rebuild static/app.css and commit it');
});

test('duel pages: options from results, every registered chart has its card', async () => {
  const { CHARTS, duelsOption, duelOption, shortName } = await import('../static/results.js');
  const results = [
    { name: 'duel-20261117T041705-seed31337', seed: 31337, a: { value: 558331 }, b: { value: 27408 } },
    { name: 'duel-20261117T020835-seed4242', seed: 4242, a: { value: 330926 }, b: { value: 21331 } },
  ];
  const o = duelsOption(results);
  assert.deepEqual(o.series[0].data.map((d) => d.value), [330926, 558331], 'oldest first, left to right');
  assert.equal(o.series[0].data[1].name, 'duel-20261117T041705-seed31337', 'a bar carries its run for the click');
  assert.equal(shortName('duel-20261117T041705-seed31337'), '17.11. 04:17 · 31337');
  const series = [{ month: 1, companies: { 1: { name: 'A', value: 10, trains: 0 }, 2: { name: 'B', value: 20, trains: 0 } } },
    { month: 2, companies: { 1: { name: 'A', value: 30, trains: 1 }, 2: { name: 'B', value: 25, trains: 0 } } }];
  const d = duelOption(series, { metric: 'trains' });
  assert.deepEqual(d.series[0].data, [[1, 0], [2, 1]]);
  assert.equal(d.series[0].step, 'end');
  assert.equal(d.dataZoom[0].zoomOnMouseWheel, 'ctrl');
  assert.ok(render(o).texts > 4 && render(d).paths > 2);
  const src = fs.readFileSync(path.join(STATIC, 'results.js'), 'utf8');
  for (const c of CHARTS) assert.ok(src.includes(`chartCard('${c.id}'`), `${c.id} has a maximizable card with role=img and aria-label`);
});

test('runs side by side: each run a solid planned line and a dashed rival, one colour per run', async () => {
  const { compareOption } = await import('../static/results.js');
  const run = (name, a) => ({ name, series: [1, 2].map((m) => ({ month: m, companies: { 1: { value: a * m }, 2: { value: 10 * m } } })) });
  const o = compareOption([run('duel-20261117T041705-seed31337', 100), run('duel-20261117T053732-seed31337', 50)], { metric: 'value' });
  assert.equal(o.series.length, 4);
  assert.deepEqual(o.series.map((s) => s.lineStyle.type), ['solid', 'dashed', 'solid', 'dashed']);
  assert.equal(o.series[0].color, o.series[1].color, 'a run keeps one colour');
  assert.notEqual(o.series[0].color, o.series[2].color);
  assert.deepEqual(o.series[2].data, [[1, 50], [2, 100]]);
  assert.equal(o.series[0].id, 'duel-20261117T041705-seed31337-1', 'a line carries its run for the click');
  assert.ok(render(o).paths >= 4);
});

test('the overview cards share one header row and say when no company exists yet', async () => {
  const html = fs.readFileSync(path.join(STATIC, 'index.html'), 'utf8');
  const section = html.slice(html.indexOf('<section id="overview"'), html.indexOf('</section>', html.indexOf('<section id="overview"')));
  for (const c of CHARTS) {
    const card = section.slice(section.indexOf(`data-chart="${c.id}"`));
    assert.match(card.slice(0, 900), /data-toolbar class="flex shrink-0/, `${c.id}: the toolbar keeps its place beside the title`);
  }
  for (const id of ['ov-companies-empty', 'ov-standing-empty']) assert.ok(section.includes(`id="${id}"`), `${id} exists`);
  for (const sel of section.match(/<select [^>]*>/g)) assert.match(sel, /\bpe-9\b/, 'a select keeps room for its chevron');
  const { holdAnchor } = await import('../static/overview.js');
  const scrolled = [], listeners = {};
  let observed;
  const target = { scrollIntoView: () => scrolled.push(1) };
  const main = { contains: (e) => e === target };
  const win = {
    location: { hash: '#overview' }, setTimeout: () => {}, addEventListener: (k, f) => { listeners[k] = f; },
    ResizeObserver: class { constructor(f) { observed = f; } observe() {} disconnect() { observed = null; } },
  };
  holdAnchor({ getElementById: (id) => (id === 'overview' ? target : null), querySelector: () => main }, win);
  observed(); observed();
  assert.equal(scrolled.length, 2, 'the section is brought back while the cards above it grow');
  listeners.wheel();
  assert.equal(observed, null, 'the reader scrolling releases it');
});

test('every overview series has a stable id, so a new game day updates it in place', () => {
  for (const option of [companiesOption(samples), standingOption(samples), fleetOption(samples)]) {
    for (const s of option.series) assert.ok(s.id !== undefined && s.id !== '', `${s.name}: replaceMerge needs an id or the series replays its entrance on every redraw`);
  }
});

import { monthlyChanges, monthlyOption, balanceOption, summaryTiles } from '../static/overview.js';

const mday = (date, net, fleet, extra = []) => ({ date, t: Date.parse(date) / 1000, vehicles: 99, fleet,
  companies: [{ id: 0, name: 'A', net, money: net, loan: 0, value: net * 2 }, ...extra] });

test('monthly changes: month end minus the previous month end, the first month from the first sample', () => {
  const s = [mday('1950-01-01', 100), mday('1950-01-31', 150), mday('1950-02-10', 120), mday('1950-02-28', 90), mday('1950-03-05', 200)];
  assert.deepEqual(monthlyChanges(s).map((r) => [r.month, r.change]), [['1950-01', 50], ['1950-02', -60], ['1950-03', 110]]);
  const opt = monthlyOption(s);
  assert.deepEqual(opt.xAxis.data, ['1950-01', '1950-02', '1950-03']);
  assert.deepEqual(opt.series[0].data, [50, -60, 110]);
});

test('the fleet is stacked by type; days from an older server show the pool count, dashed and named', () => {
  const s = [mday('1950-01-01', 0), mday('1950-01-02', 0, { train: 2, road: 3, ship: 0, aircraft: 0, total: 5 })];
  const opt = fleetOption(s);
  assert.deepEqual(opt.series.map((x) => x.id), ['fleet-train', 'fleet-road', 'fleet-pool']);
  assert.equal(opt.series[0].stack, 'fleet');
  assert.match(opt.series[2].name, /older server/);
});

test('one company gets its balance sheet; two get ranked', () => {
  const one = [mday('1950-01-01', 100)];
  assert.equal(standingOption(one).yAxis.data.length, 4);
  assert.deepEqual(balanceOption(one).series[0].data.map((d) => d.metric), ['net', 'value', 'money', 'loan']);
  const two = [mday('1950-01-01', 100, null, [{ id: 1, name: 'B', net: 300, money: 300, loan: 0, value: 1 }])];
  assert.deepEqual(standingOption(two).yAxis.data, ['A', 'B']);
  assert.equal(summaryTiles(two).leader, 'leader: B');
});
