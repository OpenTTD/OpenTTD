// node --test tools/research/mcp-panel/test/ : the duel pages (static/results.js) in the fake browser
// of fake-dom.mjs: /results (the scoreboard card, the start form and its progress, the table, both
// charts and their address state) and /result/<name> (the stats, the chart, the log filter and Run
// this again). The fixtures are shaped like panel_results.rs answers (GET /api/results, /api/result,
// /api/duel, /api/scoreboard; POST /api/duel, /api/scoreboard) with fictional values; no duel starts
// and nothing here talks to a panel.
import './panel-ui.mjs'; // first: shell.js publishes PanelUI before any page script
import test from 'node:test';
import assert from 'node:assert/strict';
import { fakeDom, respond } from './fake-dom.mjs';

const A = 'duel-20261117T041705-seed31337';
const B = 'duel-20261117T053732-seed31337';
const C = 'duel-20261116T120701-seed4242';
const side = (name, value, trains) => ({ name, value, money: value / 2, loan: 0, trains, road: 2 });
/** One line of GET /api/results (panel_results.rs summary). */
const summary = (name, seed, a, b, extra = {}) => ({
  name, seed, months: 36, label: `fictional run ${name.slice(-5)}`, opponent: 'PrismaticAI', bot_settings: null,
  winner: 'A (planned)', a: side('Planned Transport', a, 7), b: side('Rival Transport', b, 0),
  ratio: b > 0 ? Math.round((a / b) * 10) / 10 : null, ...extra,
});
const results = { count: 3, results: [summary(B, 31337, 250000, 25000), summary(A, 31337, 500000, 0), summary(C, 4242, 300000, 30000)] };
const month = (m, a, b) => ({ month: m, date: `1950-${String(m + 1).padStart(2, '0')}-01`, companies: {
  1: { name: 'Planned Transport', money: a, loan: 300000, value: a, trains: m, road: 1, planes: 0 },
  2: { name: 'Rival Transport', money: b, loan: 50000, value: b, trains: 0, road: 3, planes: 0 } } });
const series = [month(1, 1000, 900), month(2, 5000, 1000), month(3, 9000, 1100)];
/** GET /api/result?name= (panel_results.rs h_result). */
const result = (name) => ({
  summary: summary(name, 31337, 500000, 0, { months: 24, opponent: 'AdmiralAI', bot_settings: 'own_lines=8', label: 'calibrated run' }),
  series,
  plan: ['planner: game unpaused at tick 1447 with 2 AI company(ies)', 'planner: calibration from research/calibration.json (3 games)'],
  bot_log: { 0: ['PrismaticAI: built rail at tile 34418', 'PrismaticAI: line laid Coal Mine -> Power Station'], 1: ['PrismaticAI heartbeat: money=13807'] },
  experiment: {},
  markdown: '# Duel\n\nThe planned bot won.',
});
const scoreboard = (readme = 'stale') => ({
  readme, seed: 31337, months: 36, duels: 47, wins: 43,
  best: { name: A, seed: 31337, months: 120, label: 'ten years', planned: 4919754, unplanned: 67202, won: true },
  series: [{ name: A }, { name: B }],
});

let duel = { running: false, started: false };
let duelPost = () => respond({ status: 'started', pid: 77, args: ['--months', '12', '--seed', '7'], log: '/tmp/x.log' }, { status: 202 });
let board = scoreboard();
const posts = [];
const routes = (url, init) => {
  const body = init.body ? JSON.parse(init.body) : undefined;
  if (init.method === 'POST') posts.push([url, body]);
  if (url === '/api/results') return results;
  if (url.startsWith('/api/result?name=')) {
    const name = decodeURIComponent(url.slice('/api/result?name='.length));
    return results.results.some((r) => r.name === name) ? result(name) : respond({ error: `no result ${name}` }, { status: 404 });
  }
  if (url === '/api/duel') return init.method === 'POST' ? duelPost(body) : duel;
  if (url === '/api/scoreboard') return init.method === 'POST' ? scoreboard('current') : board;
  return respond({ error: 'no route' }, { status: 404 });
};

const env = fakeDom({ url: `http://127.0.0.1:8800/results?seed=31337&compare=${A}`, routes });
// One instance of the module per process (node's coverage report keeps one per file): later pages mount
// through its exported entry points, and its charts keep going through the first document's echarts.
const { compareOption, duelsOption, duelOption, listPage, resultPage } = await import('../static/results.js');
const $ = env.byId;
const chart = (id) => env.charts.find((c) => c.box === $(id));

test('/results: the scoreboard says the README is stale and offers to regenerate it', async () => {
  await env.settle();
  const sb = $('scoreboard');
  assert.match(sb.innerHTML, /README stale/);
  assert.equal(sb.querySelector('#sb-write').disabled, false, 'a stale README can be regenerated');
  assert.match(sb.innerHTML, /href="\/result\/duel-20261117T041705-seed31337">4,919,754</, 'the best run leads to its page');
  assert.match(sb.innerHTML, /43 of 47/);
  assert.match(sb.innerHTML, /73×/, 'the best ratio, rounded');
  assert.match(sb.innerHTML, /Seed 31337 revisions/);
});

test('/results: after layout the table lists the duels of the seed in the address, the compared run ticked', async () => {
  assert.equal($('duels-rows').innerHTML, '', 'drawn once the boxes have their size');
  await env.tick();
  assert.equal($('duels-n').textContent, '(2)');
  const rows = $('duels-rows').querySelectorAll('tr');
  assert.equal(rows.length, 2);
  assert.match(rows[0].innerHTML, /href="\/result\/duel-20261117T053732-seed31337"/);
  assert.match(rows[0].innerHTML, />10×</);
  assert.match(rows[1].innerHTML, />-</, 'a rival worth nothing has no ratio');
  assert.equal($('duels-rows').querySelector(`[data-cmp="${A}"]`).checked, true);
  assert.equal($('duels-rows').querySelector(`[data-cmp="${B}"]`).checked, false);
  assert.deepEqual($('seed').options.map((o) => o.value), ['', '4242', '31337'], 'the seeds come from the results');
  assert.equal($('seed').value, '31337');
});

test('/results: both charts draw after layout, from the results and the ticked run', async () => {
  const bars = chart('duels-chart').getOption();
  assert.deepEqual(bars.series[0].data.map((d) => d.name), [A, B], 'oldest first, the seed filter applied');
  assert.deepEqual(bars.series.map((s) => s.data.map((d) => d.value)), duelsOption(results.results.slice(0, 2)).series.map((s) => s.data.map((d) => d.value)));
  assert.ok(env.fetches.some((f) => f.url === `/api/result?name=${encodeURIComponent(A)}`), 'the ticked run is read');
  const lines = chart('compare-chart').getOption();
  assert.deepEqual(lines.series.map((s) => s.id), [`${A}-1`, `${A}-2`]);
  assert.deepEqual(lines.series[0].data, compareOption([{ name: A, series }]).series[0].data);
  chart('duels-chart').handlers.click({ data: { name: B } });
  assert.equal(env.location.pathname, `/result/${B}`, 'a bar leads to its run');
  chart('compare-chart').handlers.click({ seriesId: `${A}-2` });
  assert.equal(env.location.pathname, `/result/${A}`, 'a line leads to its run');
  env.location.href = `http://127.0.0.1:8800/results?seed=31337&compare=${A}`;
});

test('/results: ticking, the metric and the seed live in the address', async () => {
  const box = $('duels-rows').querySelector(`[data-cmp="${B}"]`);
  box.checked = true;
  await box.dispatch('change');
  await env.settle();
  assert.equal(new URL(env.location.href).searchParams.get('compare'), `${A},${B}`);
  assert.deepEqual(chart('compare-chart').getOption().series.map((s) => s.id), [`${A}-1`, `${A}-2`, `${B}-1`, `${B}-2`]);
  const reads = env.fetches.filter((f) => f.url.startsWith('/api/result?name=')).length;
  $('metric').value = 'trains';
  await $('metric').onchange({ target: $('metric') });
  assert.equal(new URL(env.location.href).searchParams.get('metric'), 'trains');
  const o = chart('compare-chart').getOption();
  assert.equal(o.yAxis.name, 'Trains');
  assert.equal(o.series[0].step, 'end');
  assert.equal(env.fetches.filter((f) => f.url.startsWith('/api/result?name=')).length, reads, 'a run read once is not read again');
  for (const n of [A, B]) {
    const b = $('duels-rows').querySelector(`[data-cmp="${n}"]`);
    b.checked = false;
    await b.dispatch('change');
  }
  await env.settle();
  assert.equal(new URL(env.location.href).searchParams.get('compare'), null);
  assert.match(chart('compare-chart').getOption().title.text, /Tick runs in the table/);
  $('seed').onchange({ target: { value: '' } });
  assert.equal(new URL(env.location.href).searchParams.get('seed'), null);
  assert.equal($('duels-n').textContent, '(3)', 'every seed');
});

test('/results: a chart maximizes, into the address, and Escape restores it', async () => {
  const card = $('duels-chart-card');
  const max = card.querySelector('[data-toolbar] button');
  assert.equal(max.textContent, 'Maximize');
  await max.click();
  assert.ok(card.classList.contains('fixed'));
  assert.equal(new URL(env.location.href).searchParams.get('max'), 'duels-chart');
  await env.document.dispatch('keydown', { key: 'Escape' });
  assert.ok(!card.classList.contains('fixed'));
  assert.equal(new URL(env.location.href).searchParams.get('max'), null);
});

test('/results: Start the duel posts the form and follows its progress to the result', async () => {
  $('d-months').value = '12';
  $('d-seed').value = '7';
  $('d-opponent').value = 'AdmiralAI';
  $('d-label').value = 'a fictional test';
  $('d-bot').value = 'own_lines=8';
  $('d-cal').checked = true;
  duel = { running: true, started: true, args: ['--months', '12', '--seed', '7'], seconds: 40, month: 'month 3 1950-04-01 A 9000 B 1100' };
  posts.length = 0;
  let prevented = false;
  await $('duel-form').onsubmit({ preventDefault: () => { prevented = true; } });
  await env.settle();
  assert.ok(prevented);
  assert.deepEqual(posts, [['/api/duel', { months: 12, label: 'a fictional test', opponent: 'AdmiralAI', bot_settings: 'own_lines=8', calibrated: true, seed: 7 }]]);
  assert.equal($('duel-msg').textContent, 'started: mcp-duel --months 12 --seed 7 (about a minute per game month)');
  assert.ok(!$('duel-run').classList.contains('hidden'));
  assert.equal($('duel-bar').style.width, '25%', 'month 3 of 12');
  assert.equal($('duel-pct').textContent, '25%');
  assert.equal($('duel-month').textContent, duel.month);
  duel = { running: false, started: true, exit: 0, args: duel.args, result: `mcp-duel: winner A (planned) research/results/${A}.md`, name: A };
  await env.tick();
  assert.equal($('duel-month').textContent, duel.result);
  assert.match($('duel-msg').innerHTML, /Done: <a class="underline" href="\/result\/duel-20261117T041705-seed31337">17\.11\. 04:17 · 31337<\/a>/);
});

test('/results: a refused start says why; an empty seed is left to the panel', async () => {
  duelPost = () => respond({ error: 'a duel is running; GET /api/duel shows it' }, { status: 409 });
  $('d-seed').value = '';
  $('d-months').value = '';
  posts.length = 0;
  await $('duel-form').onsubmit({ preventDefault() {} });
  assert.equal(posts[0][1].months, 36, 'an empty month count is the default 36');
  assert.ok(!('seed' in posts[0][1]), 'no seed: the duel picks one');
  assert.equal($('duel-msg').textContent, 'a duel is running; GET /api/duel shows it');
});

test('/results: Regenerate README posts and shows the new scoreboard', async () => {
  posts.length = 0;
  await $('sb-write').onclick();
  assert.deepEqual(posts, [['/api/scoreboard', undefined]]);
  assert.match($('scoreboard').innerHTML, /README current/);
  assert.match($('scoreboard').innerHTML, /README\.md regenerated; review the diff, then commit and push\./);
  assert.equal($('sb-write').disabled, true, 'a current README has nothing to regenerate');
});

test('/results: a scoreboard the panel cannot derive says so', async () => {
  const env2 = fakeDom({ url: 'http://127.0.0.1:8800/results', routes });
  board = respond({ error: 'readme-scoreboard --json: no results' }, { status: 500 });
  results.results.length = 0;
  duel = { running: false, started: false };
  await listPage();
  await env2.tick();
  assert.match(env2.byId('scoreboard').innerHTML, /README scoreboard: readme-scoreboard --json: no results/);
  assert.match(env2.byId('duels-rows').innerHTML, /no duel yet/);
  assert.equal(env2.byId('duel-run').classList.contains('hidden'), true, 'no duel started, no progress bar');
});

test('/result/<name>: the stats, the plan, the reading, the log and the chart of one duel', async () => {
  results.results.push(summary(A, 31337, 500000, 0));
  const env3 = fakeDom({ url: `http://127.0.0.1:8800/result/${A}?metric=trains`, routes });
  await resultPage(A);
  await env3.settle();
  const $3 = env3.byId;
  const page = $3('page').innerHTML;
  assert.match($3('crumbs').innerHTML, /17\.11\. 04:17 · 31337/);
  assert.match(page, /500,000/);
  assert.match(page, /Planned Transport · 7 trains/);
  assert.match(page, /seed 31337/);
  assert.match(page, /24 months · opponent AdmiralAI · bot own_lines=8/);
  assert.match(page, /<li>planner: calibration from research\/calibration\.json \(3 games\)<\/li>/);
  assert.match(page, /# Duel/);
  assert.match(page, /The bots' logs <span[^>]*>\(3 lines\)/);
  assert.match(page, new RegExp(`href="/results\\?compare=${A}"`), 'the run leads to the comparison');
  assert.equal($3('log').textContent.split('\n').length, 3);
  assert.match($3('log').textContent, /^\[A\] PrismaticAI: built rail/);
  $3('log-q').value = 'LINE LAID';
  $3('log-q').oninput();
  assert.equal($3('log').textContent, '[A] PrismaticAI: line laid Coal Mine -> Power Station', 'the filter ignores case');
  assert.equal($3('metric').value, 'trains', 'the measure comes from the address');
  await env3.tick();
  const c = env.charts.find((x) => x.box === $3('duel-chart'));
  assert.deepEqual(c.getOption().series[0].data, duelOption(series, { metric: 'trains' }).series[0].data);
  $3('metric').onchange({ target: { value: 'money' } });
  assert.equal(new URL(env3.location.href).searchParams.get('metric'), 'money');
  assert.equal(c.getOption().yAxis.name, 'Money');

  posts.length = 0;
  duelPost = () => respond({ status: 'started', args: [] }, { status: 202 });
  await $3('again').onclick();
  assert.deepEqual(posts, [['/api/duel', { months: 24, seed: 31337, label: 'again: calibrated run', opponent: 'AdmiralAI', bot_settings: 'own_lines=8', calibrated: true }]],
    'the same map, months, opponent and bot settings, calibrated as the plan says it was');
  assert.match($3('again-msg').innerHTML, /started; <a class="underline" href="\/results\?compare=duel-20261117T041705-seed31337">/);
  duelPost = () => respond({ error: 'months: 1 to 240' }, { status: 400 });
  await $3('again').onclick();
  assert.equal($3('again-msg').innerHTML, 'months: 1 to 240');
});

test('/result/<name>: a duel the panel does not have says so', async () => {
  const env4 = fakeDom({ url: 'http://127.0.0.1:8800/result/duel-20990101T000000-seed1', routes });
  await resultPage('duel-20990101T000000-seed1');
  await env4.settle();
  assert.match(env4.byId('page').innerHTML, /No such duel\. <a class="underline" href="\/results">All duels<\/a>/);
});
