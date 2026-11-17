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
