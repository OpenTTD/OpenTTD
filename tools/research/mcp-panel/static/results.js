/* Duels in the panel: /results lists every duel (GET /api/results, from research/results), charts
   their final values, and starts a new one with a click (POST /api/duel, progress from GET
   /api/duel); /result/<name> is one duel (GET /api/result): the companies month by month, the plan,
   the written reading and both bots' logs. Charts go through the one door (components.js ecInit),
   are rows of CHARTS, keep their parameters in the address and maximize (Escape restores). */

import { ecInit, maximizable, PALETTE } from './components.js';

const echarts = globalThis.echarts;
const $ = (id) => document.getElementById(id);
const esc = (s) => String(s ?? '').replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
const INK = { text: '#d1d5db', muted: '#9ca3af', grid: '#374151' };
const compact = new Intl.NumberFormat('en', { notation: 'compact', maximumFractionDigits: 1 });
const whole = new Intl.NumberFormat('en');
const card = 'p-4 mb-4 bg-white border border-gray-200 rounded-lg shadow-sm dark:border-gray-700 dark:bg-gray-800';
const input = 'bg-gray-50 border border-gray-300 text-gray-900 text-sm rounded-lg focus:ring-primary-500 focus:border-primary-500 block w-full p-2.5 dark:bg-gray-700 dark:border-gray-600 dark:text-white';
const axis = { axisLabel: { color: INK.muted, hideOverlap: true }, axisLine: { lineStyle: { color: INK.grid } }, splitLine: { lineStyle: { color: INK.grid, type: 'dashed' } } };

export const METRICS = { value: 'Company value', money: 'Money', loan: 'Loan', trains: 'Trains' };

/** The registry: every chart of the duel pages, the address keys it reads and where a click goes. */
export const CHARTS = [
  { id: 'duels-chart', page: '/results', kind: 'bar', params: ['seed', 'max'], drill: '/result/<name>' },
  { id: 'compare-chart', page: '/results', kind: 'line', params: ['compare', 'metric', 'max'], drill: '/result/<name>' },
  { id: 'duel-chart', page: '/result/<name>', kind: 'line', params: ['metric', 'max'], drill: 'static: the two companies of one game' },
];

/* ---- address state ------------------------------------------------------------------------- */
const params = () => new URL(location.href).searchParams;
export function getState() {
  const p = params();
  const compare = (p.get('compare') || '').split(',').filter((n) => /^duel-[A-Za-z0-9-]+$/.test(n));
  return { seed: p.get('seed') || '', compare, metric: METRICS[p.get('metric')] ? p.get('metric') : 'value', max: p.get('max') || undefined };
}
export function setState(patch) {
  const u = new URL(location.href);
  for (const [k, v] of Object.entries(patch)) {
    if (v === undefined || v === '' || v === null) u.searchParams.delete(k); else u.searchParams.set(k, v);
  }
  globalThis.history.replaceState(null, '', u);
}

/* ---- options (pure: data in, option out) --------------------------------------------------- */
export const shortName = (name) => {
  const m = /duel-(\d{4})(\d{2})(\d{2})T(\d{2})(\d{2})\d*-seed(\d+)/.exec(name || '');
  return m ? `${m[3]}.${m[2]}. ${m[4]}:${m[5]} · ${m[6]}` : name;
};

export function duelsOption(results) {
  const rows = results.slice().reverse();
  return {
    backgroundColor: 'transparent',
    tooltip: { trigger: 'axis', valueFormatter: (v) => (v == null ? '-' : whole.format(v)) },
    legend: { top: 0, textStyle: { color: INK.text } },
    grid: { left: 40, right: 16, top: 32, bottom: 72, containLabel: true },
    xAxis: { type: 'category', data: rows.map((r) => shortName(r.name)), ...axis, axisLabel: { color: INK.muted, rotate: 40, hideOverlap: true } },
    yAxis: { type: 'value', ...axis, axisLabel: { color: INK.muted, formatter: (v) => compact.format(v) } },
    dataZoom: [{ type: 'inside', zoomOnMouseWheel: 'ctrl', moveOnMouseWheel: false }, { type: 'slider', height: 18, bottom: 4, borderColor: INK.grid }],
    series: [
      { name: 'Planned bot (A)', type: 'bar', cursor: 'pointer', color: PALETTE.dark[0], data: rows.map((r) => ({ value: r.a.value, name: r.name })) },
      { name: 'Rival (B)', type: 'bar', cursor: 'pointer', color: PALETTE.dark[1], data: rows.map((r) => ({ value: r.b.value, name: r.name })) },
    ],
  };
}

/** Several runs over each other: the planned company of each (solid) and its rival (dashed), one
 *  colour per run, so an A/B of two bots on one map reads as two pairs of lines. */
export function compareOption(runs, { metric = 'value' } = {}) {
  return {
    backgroundColor: 'transparent',
    tooltip: { trigger: 'axis', valueFormatter: (v) => (v == null ? '-' : whole.format(v)) },
    legend: { type: 'scroll', top: 0, textStyle: { color: INK.text }, pageTextStyle: { color: INK.muted } },
    grid: { left: 8, right: 64, top: 36, bottom: 48, containLabel: true },
    xAxis: { type: 'value', name: 'month', nameTextStyle: { color: INK.muted }, minInterval: 1, min: 'dataMin', max: 'dataMax', ...axis, splitLine: { show: false } },
    yAxis: { type: 'value', name: METRICS[metric], nameTextStyle: { color: INK.muted, align: 'left' }, ...axis, axisLabel: { color: INK.muted, formatter: (v) => compact.format(v) } },
    dataZoom: [{ type: 'inside', zoomOnMouseWheel: 'ctrl', moveOnMouseWheel: false }, { type: 'slider', height: 18, bottom: 4, borderColor: INK.grid }],
    series: runs.flatMap((r, i) => ['1', '2'].map((k) => ({
      id: `${r.name}-${k}`, name: `${shortName(r.name)} · ${k === '1' ? 'A' : 'B'}`, type: 'line', showSymbol: false, cursor: 'pointer',
      step: metric === 'trains' ? 'end' : false, color: PALETTE.dark[i % PALETTE.dark.length],
      lineStyle: { type: k === '1' ? 'solid' : 'dashed', width: k === '1' ? 2.5 : 1.5 },
      endLabel: k === '1' ? { show: true, color: INK.text, formatter: (p) => compact.format(p.value[1] ?? 0) } : { show: false },
      data: (r.series || []).map((x) => [x.month, x.companies?.[k]?.[metric] ?? null]),
    }))),
  };
}

export function duelOption(series, { metric = 'value' } = {}) {
  const side = (k) => series.map((x) => [x.month, x.companies?.[k]?.[metric] ?? null]);
  const name = (k) => series.find((x) => x.companies?.[k]?.name)?.companies[k].name || (k === '1' ? 'A' : 'B');
  return {
    backgroundColor: 'transparent',
    tooltip: { trigger: 'axis', valueFormatter: (v) => (v == null ? '-' : whole.format(v)) },
    legend: { top: 0, textStyle: { color: INK.text } },
    grid: { left: 8, right: 64, top: 32, bottom: 48, containLabel: true },
    xAxis: { type: 'value', name: 'month', nameTextStyle: { color: INK.muted }, minInterval: 1, min: 'dataMin', max: 'dataMax', ...axis, splitLine: { show: false } },
    yAxis: { type: 'value', name: METRICS[metric], nameTextStyle: { color: INK.muted, align: 'left' }, ...axis, axisLabel: { color: INK.muted, formatter: (v) => compact.format(v) } },
    dataZoom: [{ type: 'inside', zoomOnMouseWheel: 'ctrl', moveOnMouseWheel: false }, { type: 'slider', height: 18, bottom: 4, borderColor: INK.grid }],
    series: [['1', 0, 'A (planned)'], ['2', 1, 'B']].map(([k, c, role]) => ({
      name: `${name(k)} · ${role}`, type: 'line', step: metric === 'trains' ? 'end' : false, showSymbol: false, color: PALETTE.dark[c],
      endLabel: { show: true, color: INK.text, formatter: (p) => compact.format(p.value[1] ?? 0) }, data: side(k),
    })),
  };
}

/* ---- the pages ------------------------------------------------------------------------------- */
function chartCard(id, title, note, label, height = 'h-72 sm:h-80 lg:h-96') {
  return `<div id="${id}-card" data-maximize="${id}" data-chart="${id}" class="${card} flex flex-col">
    <div class="flex flex-wrap items-center justify-between gap-2 mb-3">
      <div><h2 class="text-lg font-semibold text-gray-900 dark:text-white">${esc(title)}</h2><p class="text-xs text-gray-500 dark:text-gray-400">${esc(note)}</p></div>
      <div data-toolbar class="flex flex-wrap items-center gap-2"></div>
    </div>
    <div id="${id}" role="img" aria-label="${esc(label)}" class="w-full ${height} grow"></div></div>`;
}

function arm(id, draw) {
  const box = $(id + '-card');
  box.querySelector('[data-toolbar]').append(maximizable(box, id, { getState, setState, onResize: () => draw() }));
}

async function listPage() {
  const state = getState();
  const all = (await (await fetch('/api/results')).json()).results || [];
  const seeds = [...new Set(all.map((r) => r.seed))].sort((a, b) => a - b);
  $('page').innerHTML = `
    <div class="${card}">
      <h2 class="mb-3 text-lg font-semibold text-gray-900 dark:text-white">Start a duel</h2>
      <form id="duel-form" class="grid gap-3 sm:grid-cols-2 lg:grid-cols-5">
        <div><label for="d-months" class="block mb-1 text-sm font-medium text-gray-900 dark:text-white">Months</label><input id="d-months" type="number" min="1" max="240" value="36" class="${input}"></div>
        <div><label for="d-seed" class="block mb-1 text-sm font-medium text-gray-900 dark:text-white">Seed (map)</label><input id="d-seed" type="number" placeholder="random" class="${input}"></div>
        <div><label for="d-opponent" class="block mb-1 text-sm font-medium text-gray-900 dark:text-white">Opponent AI</label><input id="d-opponent" placeholder="PrismaticAI unplanned" class="${input}"></div>
        <div class="lg:col-span-2"><label for="d-label" class="block mb-1 text-sm font-medium text-gray-900 dark:text-white">Label</label><input id="d-label" placeholder="what this run tests" class="${input}"></div>
        <div class="sm:col-span-2 lg:col-span-5 flex flex-wrap items-center gap-3">
          <button type="submit" class="text-white bg-primary-700 hover:bg-primary-800 focus:ring-4 focus:ring-primary-300 font-medium rounded-lg text-sm px-5 py-2.5 dark:bg-primary-600 dark:hover:bg-primary-700">Start the duel</button>
          <span id="duel-msg" class="text-sm text-gray-500 dark:text-gray-400"></span>
        </div>
      </form>
      <div id="duel-run" class="hidden mt-4">
        <div class="flex justify-between mb-1 text-sm text-gray-700 dark:text-gray-300"><span id="duel-month">starting</span><span id="duel-pct"></span></div>
        <div class="w-full bg-gray-200 rounded-full h-2.5 dark:bg-gray-700"><div id="duel-bar" class="bg-primary-600 h-2.5 rounded-full" style="width: 0%"></div></div>
      </div>
    </div>
    ${chartCard('duels-chart', 'Final company value by duel', 'Planned bot against its rival at the end of each run; click a run for its page', 'Final company value of the planned bot and its rival, one pair of bars per duel')}
    ${chartCard('compare-chart', 'Runs side by side', 'Tick runs in the table: each planned company solid, its rival dashed, month by month', 'The ticked runs month by month, planned companies solid and rivals dashed')}
    <div class="${card}">
      <div class="flex flex-wrap items-center justify-between gap-2 mb-3">
        <h2 class="text-lg font-semibold text-gray-900 dark:text-white">All duels <span id="duels-n" class="text-sm font-normal text-gray-500"></span></h2>
        <label class="flex items-center gap-2 text-sm text-gray-900 dark:text-white">Seed <select id="seed" class="${input} w-auto"><option value="">all</option>${seeds.map((s) => `<option${String(s) === state.seed ? ' selected' : ''}>${esc(s)}</option>`).join('')}</select></label>
      </div>
      <div class="relative overflow-x-auto"><table class="w-full text-sm text-left text-gray-500 dark:text-gray-400">
        <thead class="text-xs text-gray-700 uppercase bg-gray-50 dark:bg-gray-700 dark:text-gray-400"><tr><th class="px-3 py-2"><span class="sr-only">Compare</span></th><th class="px-3 py-2">Run</th><th class="px-3 py-2">Seed</th><th class="px-3 py-2">Months</th><th class="px-3 py-2 text-right">A</th><th class="px-3 py-2 text-right">B</th><th class="px-3 py-2 text-right">A : B</th><th class="px-3 py-2">What it tests</th></tr></thead>
        <tbody id="duels-rows"></tbody></table></div>
    </div>`;
  let chart = null;
  const shown = () => all.filter((r) => !state.seed || String(r.seed) === state.seed);
  const draw = () => {
    const rows = shown();
    $('duels-n').textContent = `(${rows.length})`;
    $('duels-rows').innerHTML = rows.map((r) => `<tr class="border-b dark:border-gray-700">
      <td class="px-3 py-2"><input type="checkbox" data-cmp="${esc(r.name)}"${state.compare.includes(r.name) ? ' checked' : ''} aria-label="Compare ${esc(shortName(r.name))}" class="w-4 h-4 rounded-sm"></td>
      <td class="px-3 py-2 whitespace-nowrap"><a href="/result/${esc(r.name)}" class="font-medium text-primary-600 hover:underline dark:text-primary-500">${esc(shortName(r.name))}</a></td>
      <td class="px-3 py-2">${esc(r.seed)}</td><td class="px-3 py-2">${esc(r.months)}</td>
      <td class="px-3 py-2 text-right font-medium text-gray-900 dark:text-white">${esc(whole.format(r.a.value ?? 0))}</td>
      <td class="px-3 py-2 text-right">${esc(whole.format(r.b.value ?? 0))}</td>
      <td class="px-3 py-2 text-right">${r.ratio == null ? '-' : esc(r.ratio + '×')}</td>
      <td class="px-3 py-2">${esc(r.label)}</td></tr>`).join('') || '<tr><td class="px-3 py-2" colspan="8">no duel yet</td></tr>';
    chart = chart || ecInit($('duels-chart'), echarts);
    if (chart) {
      chart.setOption(duelsOption(rows), true);
      if (!chart._wired) { chart._wired = true; chart.on('click', (p) => { if (p.data?.name) location.href = '/result/' + p.data.name; }); }
      chart.resize();
    }
  };
  $('seed').onchange = (e) => { state.seed = e.target.value; setState({ seed: state.seed }); draw(); };
  arm('duels-chart', draw);

  // Runs side by side: the ticked runs (kept in the address as ?compare=) month by month.
  const cache = {};
  let cmp = null;
  const tb = $('compare-chart-card').querySelector('[data-toolbar]');
  tb.insertAdjacentHTML('afterbegin', `<label for="metric" class="sr-only">Measure</label><select id="metric" class="${input} w-auto">${Object.entries(METRICS).map(([k, v]) => `<option value="${k}"${k === state.metric ? ' selected' : ''}>${v}</option>`).join('')}</select>`);
  const drawCompare = async () => {
    const runs = [];
    for (const n of state.compare) {
      cache[n] = cache[n] || await (await fetch('/api/result?name=' + encodeURIComponent(n))).json();
      if (cache[n].series) runs.push({ name: n, series: cache[n].series });
    }
    cmp = cmp || ecInit($('compare-chart'), echarts);
    if (!cmp) return;
    cmp.setOption(runs.length ? compareOption(runs, state) : { title: { text: 'Tick runs in the table below to compare them', left: 'center', top: 'middle', textStyle: { color: INK.muted, fontSize: 14 } }, series: [] }, true);
    if (!cmp._wired) { cmp._wired = true; cmp.on('click', (p) => { const n = String(p.seriesId || '').replace(/-[12]$/, ''); if (n) location.href = '/result/' + n; }); }
    cmp.resize();
  };
  $('duels-rows').addEventListener('change', (e) => {
    const n = e.target.dataset?.cmp;
    if (!n) return;
    state.compare = e.target.checked ? [...new Set([...state.compare, n])] : state.compare.filter((x) => x !== n);
    setState({ compare: state.compare.join(',') });
    drawCompare();
  });
  $('metric').onchange = (e) => { state.metric = e.target.value; setState({ metric: state.metric }); drawCompare(); };
  arm('compare-chart', drawCompare);
  setTimeout(() => { draw(); drawCompare(); }, 30);

  const poll = async () => {
    const d = await (await fetch('/api/duel')).json();
    if (!d.started) return;
    $('duel-run').classList.remove('hidden');
    const months = Number((d.args || [])[(d.args || []).indexOf('--months') + 1]) || 36;
    const m = /^month +(\d+)/.exec(d.month || '');
    const pct = m ? Math.min(100, Math.round((Number(m[1]) / months) * 100)) : 0;
    $('duel-bar').style.width = pct + '%';
    $('duel-pct').textContent = pct + '%';
    $('duel-month').textContent = d.running ? (d.month || `starting the two companies (${d.seconds} s)`) : (d.result || `finished (exit ${d.exit})`);
    if (d.running) setTimeout(poll, 3000);
    else if (d.name) $('duel-msg').innerHTML = `Done: <a class="underline" href="/result/${esc(d.name)}">${esc(shortName(d.name))}</a>`;
  };
  $('duel-form').onsubmit = async (ev) => {
    ev.preventDefault();
    const body = { months: Number($('d-months').value) || 36, label: $('d-label').value, opponent: $('d-opponent').value };
    if ($('d-seed').value !== '') body.seed = Number($('d-seed').value);
    const r = await fetch('/api/duel', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) });
    const d = await r.json();
    $('duel-msg').textContent = r.ok ? `started: mcp-duel ${d.args.join(' ')} (about a minute per game month)` : (d.error || `HTTP ${r.status}`);
    poll();
  };
  poll();
}

async function resultPage(name) {
  const state = getState();
  const r = await fetch('/api/result?name=' + encodeURIComponent(name));
  if (!r.ok) { $('page').innerHTML = `<div class="${card}">No such duel. <a class="underline" href="/results">All duels</a>.</div>`; return; }
  const d = await r.json();
  const s = d.summary;
  $('crumbs').insertAdjacentHTML('beforeend', `<li class="text-gray-400">/</li><li class="text-gray-500 dark:text-gray-400">${esc(shortName(name))}</li>`);
  const stat = (label, v, sub) => `<div class="${card} mb-0"><p class="text-sm text-gray-500 dark:text-gray-400">${esc(label)}</p><p class="mt-1 text-xl font-bold text-gray-900 sm:text-2xl dark:text-white">${esc(v)}</p><p class="text-xs text-gray-500 dark:text-gray-400">${esc(sub)}</p></div>`;
  const log = Object.entries(d.bot_log || {}).flatMap(([k, lines]) => (lines || []).map((l) => [k, l]));
  $('page').innerHTML = `
    <div class="grid grid-cols-2 gap-3 mb-4 md:grid-cols-4">
      ${stat('Planned bot (A)', whole.format(s.a.value ?? 0), `${s.a.name ?? ''} · ${s.a.trains ?? 0} trains`)}
      ${stat('Rival (B)', whole.format(s.b.value ?? 0), `${s.b.name ?? ''} · ${s.b.trains ?? 0} trains`)}
      ${stat('A : B', s.ratio == null ? '-' : s.ratio + '×', String(s.winner ?? ''))}
      ${stat('Map and length', `seed ${s.seed}`, `${s.months} months · opponent ${s.opponent || 'PrismaticAI unplanned'}`)}
    </div>
    <div class="flex flex-wrap items-center gap-3 mb-4">
      <button id="again" type="button" class="text-white bg-primary-700 hover:bg-primary-800 focus:ring-4 focus:ring-primary-300 font-medium rounded-lg text-sm px-5 py-2.5 dark:bg-primary-600 dark:hover:bg-primary-700">Run this again (same map, months and opponent)</button>
      <a href="/results?compare=${encodeURIComponent(name)}" class="text-sm text-primary-600 hover:underline dark:text-primary-500">Compare with other runs</a>
      <span id="again-msg" class="text-sm text-gray-500 dark:text-gray-400"></span>
    </div>
    ${chartCard('duel-chart', 'The two companies, month by month', 'Pick what to compare; Ctrl+wheel zooms', 'The planned bot and its rival month by month, by the chosen measure')}
    <div class="grid gap-4 xl:grid-cols-2">
      <div class="${card}"><h2 class="mb-3 text-lg font-semibold text-gray-900 dark:text-white">The plan</h2>
        <ul class="space-y-1 text-sm text-gray-700 dark:text-gray-300">${(d.plan || []).map((p) => `<li>${esc(p)}</li>`).join('') || '<li>no plan recorded</li>'}</ul></div>
      <div class="${card}"><h2 class="mb-3 text-lg font-semibold text-gray-900 dark:text-white">The reading</h2>
        <pre class="p-3 max-h-[32rem] overflow-auto text-xs whitespace-pre-wrap bg-gray-50 rounded-lg dark:bg-gray-900 text-gray-800 dark:text-gray-200">${esc(d.markdown || 'no reading written')}</pre></div>
    </div>
    <div class="${card}">
      <div class="flex flex-wrap items-center justify-between gap-2 mb-3">
        <h2 class="text-lg font-semibold text-gray-900 dark:text-white">The bots' logs <span class="text-sm font-normal text-gray-500">(${log.length} lines)</span></h2>
        <input id="log-q" type="search" placeholder="filter, e.g. line laid" class="${input} w-full sm:w-72">
      </div>
      <pre id="log" class="p-3 max-h-[32rem] overflow-auto text-xs whitespace-pre-wrap bg-gray-50 rounded-lg dark:bg-gray-900 text-gray-800 dark:text-gray-200"></pre>
    </div>`;
  const tb = $('duel-chart-card').querySelector('[data-toolbar]');
  tb.insertAdjacentHTML('afterbegin', `<label for="metric" class="sr-only">Measure</label><select id="metric" class="${input} w-auto">${Object.entries(METRICS).map(([k, v]) => `<option value="${k}"${k === state.metric ? ' selected' : ''}>${v}</option>`).join('')}</select>`);
  let chart = null;
  const draw = () => { chart = chart || ecInit($('duel-chart'), echarts); if (chart) { chart.setOption(duelOption(d.series || [], state), true); chart.resize(); } };
  $('metric').onchange = (e) => { state.metric = e.target.value; setState({ metric: state.metric }); draw(); };
  arm('duel-chart', draw);
  const showLog = () => {
    const q = $('log-q').value.trim().toLowerCase();
    $('log').textContent = log.filter(([, l]) => !q || l.toLowerCase().includes(q)).map(([k, l]) => `[${k === '0' ? 'A' : 'B'}] ${l}`).join('\n');
  };
  $('log-q').oninput = showLog;
  $('again').onclick = async () => {
    const body = { months: Number(s.months) || 36, seed: Number(s.seed), label: `again: ${s.label || name}`.slice(0, 200) };
    if (s.opponent && s.opponent !== 'PrismaticAI') body.opponent = s.opponent;
    const r2 = await fetch('/api/duel', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) });
    const d2 = await r2.json();
    $('again-msg').innerHTML = r2.ok ? `started; <a class="underline" href="/results?compare=${encodeURIComponent(name)}">follow it on the duels page</a>` : esc(d2.error || `HTTP ${r2.status}`);
  };
  showLog();
  setTimeout(draw, 30);
}

if (globalThis.document && $('page')) {
  const path = decodeURIComponent(location.pathname);
  if (path === '/results') listPage(); else if (path.startsWith('/result/')) resultPage(path.slice(8));
}
