/* The overview's charts: the game's history as the panel keeps it (GET /api/overview/history, one
   sample per game day, kept server-side so a reload keeps it), pushed live by the event stream
   (SSE /api/events/stream) and polled as a fallback. Every chart goes through the one door
   (components.js ecInit), is registered in CHARTS, keeps its parameters in the address, and can be
   maximized (Escape restores). A mark leads somewhere: a company focuses it in every chart, the
   fleet leads to the vehicles resource page. */

import { ecInit, maximizable, PALETTE, $, whole, compact, INK, AXIS as axis, patchAddress } from './components.js';

const echarts = globalThis.echarts;
const DAY = 86400000;

export const METRICS = { net: 'Net worth (money − loan)', value: 'Company value', money: 'Money', loan: 'Loan' };
export const RANGES = { 90: 'Last 3 months', 365: 'Last year', 1825: 'Last 5 years', all: 'All' };

/** The registry: every chart of the overview, its parameters (address keys) and where a click goes. */
export const CHARTS = [
  { id: 'ov-companies', kind: 'line', params: ['ov_metric', 'ov_range', 'ov_focus'], drill: 'a company focuses it' },
  { id: 'ov-standing', kind: 'bar', params: ['ov_metric', 'ov_focus'], drill: 'a company focuses it' },
  { id: 'ov-monthly', kind: 'bar', params: ['ov_metric', 'ov_range', 'ov_focus'], drill: 'a company focuses it' },
  { id: 'ov-fleet', kind: 'line', params: ['ov_range'], drill: '/resource/vehicles' },
];
export const FLEET_TYPES = [['train', 'Trains'], ['road', 'Road vehicles'], ['ship', 'Ships'], ['aircraft', 'Aircraft']];

/* ---- address state ------------------------------------------------------------------------- */
const params = () => new URL(location.href).searchParams;
export function getState() {
  const p = params();
  const metric = METRICS[p.get('ov_metric')] ? p.get('ov_metric') : 'net';
  const range = RANGES[p.get('ov_range')] ? p.get('ov_range') : 'all';
  return { metric, range, focus: p.get('ov_focus') || '', max: p.get('max') || undefined };
}
const ADDRESS_KEYS = { metric: 'ov_metric', range: 'ov_range', focus: 'ov_focus', max: 'max' };
export function setState(patch) { patchAddress(patch, (k) => ADDRESS_KEYS[k]); }

/* ---- options (pure: samples in, option out; testable in Node) ------------------------------ */
const when = (s) => Date.parse(s.date) || 0;
export function companyIds(samples) {
  const ids = [];
  for (const s of samples) for (const c of s.companies) if (!ids.includes(String(c.id))) ids.push(String(c.id));
  return ids;
}
const nameOf = (samples, id) => {
  for (let i = samples.length - 1; i >= 0; i--) { const c = samples[i].companies.find((x) => String(x.id) === id); if (c && c.name) return c.name; }
  return `company ${id}`;
};
const colour = (ids, id) => PALETTE.dark[ids.indexOf(id) % PALETTE.dark.length];
const zoomFrom = (samples, range) => (range === 'all' || !samples.length ? undefined : when(samples[samples.length - 1]) - Number(range) * DAY);
const zoom = (start) => [
  { type: 'inside', zoomOnMouseWheel: 'ctrl', moveOnMouseWheel: false, startValue: start },
  { type: 'slider', startValue: start, height: 22, bottom: 8, textStyle: { color: INK.muted }, borderColor: INK.grid },
];

export function companiesOption(samples, { metric = 'net', range = 'all', focus = '' } = {}) {
  const ids = companyIds(samples);
  return {
    backgroundColor: 'transparent',
    tooltip: { trigger: 'axis', valueFormatter: (v) => (v == null ? '-' : whole.format(Math.round(v))) },
    legend: { type: 'scroll', top: 0, textStyle: { color: INK.text }, pageTextStyle: { color: INK.muted } },
    grid: { left: 28, right: 56, top: 36, bottom: 48, containLabel: true },
    xAxis: { type: 'time', ...axis, splitLine: { show: false } },
    // The axis name runs along the axis: at its top it collided with the legend on a phone.
    yAxis: { type: 'value', name: METRICS[metric], nameLocation: 'middle', nameGap: 44, nameTextStyle: { color: INK.muted }, ...axis, axisLabel: { color: INK.muted, formatter: (v) => compact.format(v) } },
    dataZoom: zoom(zoomFrom(samples, range)),
    series: ids.map((id) => ({
      id, name: nameOf(samples, id), type: 'line', showSymbol: samples.length < 3, symbolSize: 6,
      color: colour(ids, id),
      lineStyle: { width: focus === id ? 3.5 : 2, opacity: focus && focus !== id ? 0.25 : 1 },
      itemStyle: { opacity: focus && focus !== id ? 0.25 : 1 },
      emphasis: { focus: 'series' },
      // The legend names the line; its end carries the last value.
      endLabel: { show: true, color: INK.text, formatter: (p) => compact.format(p.value[1] ?? 0) },
      data: samples.map((s) => { const c = s.companies.find((x) => String(x.id) === id); return [when(s), c ? c[metric] : null]; }),
    })),
  };
}

/** With one company there is nobody to rank: its balance sheet instead (money, loan, value, net). */
export function balanceOption(samples) {
  const last = samples[samples.length - 1];
  const c = last && last.companies[0];
  const rows = c ? Object.entries(METRICS).map(([k, label]) => ({ k, label, v: c[k] ?? 0 })) : [];
  return {
    backgroundColor: 'transparent',
    tooltip: { trigger: 'item', valueFormatter: (v) => whole.format(Math.round(v)) },
    grid: { left: 8, right: 64, top: 8, bottom: 8, containLabel: true },
    xAxis: { type: 'value', splitNumber: 3, ...axis, axisLabel: { color: INK.muted, hideOverlap: true, formatter: (v) => compact.format(v) } },
    yAxis: { type: 'category', inverse: true, data: rows.map((r) => r.label), axisLabel: { color: INK.text, width: 130, overflow: 'truncate', ellipsis: '…' }, axisLine: { lineStyle: { color: INK.grid } } },
    series: [{
      id: 'standing', type: 'bar', barMaxWidth: 28,
      label: { show: true, position: 'right', color: INK.text, formatter: (p) => compact.format(p.value) },
      data: rows.map((r, i) => ({ value: r.v, metric: r.k, itemStyle: { color: PALETTE.dark[[2, 3, 0, 4][i] ?? 0], borderRadius: [0, 4, 4, 0] } })),
    }],
  };
}

export function standingOption(samples, { metric = 'net', focus = '' } = {}) {
  const ids = companyIds(samples);
  if (ids.length === 1) return balanceOption(samples);
  const last = samples[samples.length - 1];
  const rows = last ? last.companies.map((c) => ({ id: String(c.id), name: c.name || `company ${c.id}`, v: c[metric] })) : [];
  rows.sort((a, b) => a.v - b.v);
  return {
    backgroundColor: 'transparent',
    tooltip: { trigger: 'item', valueFormatter: (v) => whole.format(Math.round(v)) },
    grid: { left: 8, right: 56, top: 8, bottom: 8, containLabel: true },
    xAxis: { type: 'value', splitNumber: 3, ...axis, axisLabel: { color: INK.muted, hideOverlap: true, formatter: (v) => compact.format(v) } },
    yAxis: { type: 'category', data: rows.map((r) => r.name), axisLabel: { color: INK.text, width: 120, overflow: 'truncate', ellipsis: '…' }, axisLine: { lineStyle: { color: INK.grid } } },
    // A stable id: replaceMerge then updates the bars in place instead of replaying their entrance.
    series: [{
      id: 'standing', type: 'bar', name: METRICS[metric], barMaxWidth: 28, cursor: 'pointer',
      label: { show: true, position: 'right', color: INK.text, formatter: (p) => compact.format(p.value) },
      data: rows.map((r) => ({ value: r.v, id: r.id, itemStyle: { color: colour(ids, r.id), opacity: focus && focus !== r.id ? 0.3 : 1, borderRadius: [0, 4, 4, 0] } })),
    }],
  };
}

/** The fleet by type, stacked; days from a server without `fleet` show the pool count, dashed. */
export function fleetOption(samples, { range = 'all' } = {}) {
  const typed = samples.filter((x) => x.fleet);
  const legacy = samples.filter((x) => !x.fleet);
  const series = FLEET_TYPES.filter(([k]) => typed.some((x) => x.fleet[k])).map(([k, name], i) => ({
    id: 'fleet-' + k, name, type: 'line', stack: 'fleet', step: 'end', showSymbol: false, cursor: 'pointer',
    color: PALETTE.dark[[0, 2, 1, 3][i] ?? i], lineStyle: { width: 1.5 }, areaStyle: { opacity: 0.35 },
    emphasis: { focus: 'series' }, data: typed.map((x) => [when(x), x.fleet[k]]),
  }));
  if (legacy.length) {
    series.push({ id: 'fleet-pool', name: 'Pool count (older server: wagons, effects included)', type: 'line', showSymbol: false,
      color: INK.muted, lineStyle: { type: 'dashed', width: 1 }, data: legacy.map((x) => [when(x), x.vehicles]) });
  }
  return {
    backgroundColor: 'transparent',
    tooltip: { trigger: 'axis' },
    legend: { type: 'scroll', top: 0, textStyle: { color: INK.text }, pageTextStyle: { color: INK.muted } },
    grid: { left: 8, right: 24, top: 32, bottom: 48, containLabel: true },
    xAxis: { type: 'time', ...axis, splitLine: { show: false } },
    yAxis: { type: 'value', minInterval: 1, ...axis },
    dataZoom: zoom(zoomFrom(samples, range)),
    series,
  };
}

/** Per company and game month: the chosen metric at the month's last sample minus the previous
    month's last ({month: 'YYYY-MM', id, name, change}). The first month of a company counts from
    its first sample. */
export function monthlyChanges(samples, metric = 'net') {
  const ends = new Map(); // 'YYYY-MM' -> last sample of that month
  for (const x of samples) ends.set(String(x.date).slice(0, 7), x);
  const months = [...ends.keys()].sort();
  const out = [];
  const prev = new Map();
  for (const x of samples) for (const c of x.companies) if (!prev.has(String(c.id))) prev.set(String(c.id), c[metric]);
  for (const m of months) {
    for (const c of ends.get(m).companies) {
      const id = String(c.id);
      out.push({ month: m, id, name: c.name || `company ${id}`, change: (c[metric] ?? 0) - (prev.get(id) ?? 0) });
      prev.set(id, c[metric] ?? 0);
    }
  }
  return out;
}

export function monthlyOption(samples, { metric = 'net', range = 'all', focus = '' } = {}) {
  const ids = companyIds(samples);
  let rows = monthlyChanges(samples, metric);
  if (range !== 'all' && samples.length) {
    const from = new Date(zoomFrom(samples, range)).toISOString().slice(0, 7);
    rows = rows.filter((r) => r.month >= from);
  }
  const months = [...new Set(rows.map((r) => r.month))];
  return {
    backgroundColor: 'transparent',
    tooltip: { trigger: 'axis', axisPointer: { type: 'shadow' }, valueFormatter: (v) => (v == null ? '-' : (v > 0 ? '+' : '') + whole.format(Math.round(v))) },
    legend: { type: 'scroll', top: 0, textStyle: { color: INK.text }, pageTextStyle: { color: INK.muted } },
    grid: { left: 8, right: 16, top: 32, bottom: 48, containLabel: true },
    xAxis: { type: 'category', data: months, ...axis, splitLine: { show: false } },
    yAxis: { type: 'value', ...axis, axisLabel: { color: INK.muted, formatter: (v) => compact.format(v) } },
    dataZoom: [{ type: 'inside', zoomOnMouseWheel: 'ctrl', moveOnMouseWheel: false, startValue: Math.max(0, months.length - 36) },
      { type: 'slider', height: 22, bottom: 8, textStyle: { color: INK.muted }, borderColor: INK.grid, startValue: Math.max(0, months.length - 36) }],
    series: ids.map((id) => ({
      id, name: nameOf(samples, id), type: 'bar', cursor: 'pointer', barMaxWidth: 18, color: colour(ids, id),
      itemStyle: { opacity: focus && focus !== id ? 0.25 : 1 },
      data: months.map((m) => { const r = rows.find((x) => x.month === m && x.id === id); return r ? r.change : null; }),
    })),
  };
}

/** Change over the last `days` game days, as text ("+3 in 30 days"); '' without enough history. */
export function delta(samples, pick, days = 30) {
  if (samples.length < 2) return '';
  const end = samples[samples.length - 1];
  const from = when(end) - days * DAY;
  const start = samples.find((s) => when(s) >= from) || samples[0];
  const d = pick(end) - pick(start);
  return `${d > 0 ? '+' : ''}${whole.format(Math.round(d))} in ${Math.round((when(end) - when(start)) / DAY)} days`;
}

/** The derived tiles: the leader by the chosen metric, and the last complete month's change. */
export function summaryTiles(samples, metric = 'net') {
  const last = samples[samples.length - 1];
  if (!last || !last.companies.length) return { leader: '', month: '-', monthSub: 'no company yet' };
  const top = last.companies.slice().sort((a, b) => (b[metric] ?? 0) - (a[metric] ?? 0))[0];
  const leader = `${last.companies.length > 1 ? 'leader: ' : ''}${top.name || 'company ' + top.id}`;
  const rows = monthlyChanges(samples, metric);
  const months = [...new Set(rows.map((r) => r.month))];
  const m = months.length > 1 ? months[months.length - 2] : months[0]; // the last complete month
  const sum = rows.filter((r) => r.month === m).reduce((a, r) => a + r.change, 0);
  const short = { net: 'net worth', value: 'value', money: 'money', loan: 'loan' }[metric] || metric;
  return { leader, month: (sum > 0 ? '+' : '') + compact.format(sum), monthSub: `${short}, ${m}, all companies` };
}

/* ---- the page ------------------------------------------------------------------------------- */
/** A link to a section of this page (the sidebar's #overview) lands below the navbar and stays
    there while the cards above it fill in (live, plan, news and cluster load after the first
    paint and push it down), until the reader scrolls on their own or the page settles. */
export function holdAnchor(doc = document, win = globalThis) {
  const id = decodeURIComponent(win.location.hash.slice(1));
  const target = id && doc.getElementById(id);
  const main = doc.querySelector('main');
  if (!target || !main || !main.contains(target) || !win.ResizeObserver) return;
  const ro = new win.ResizeObserver(() => target.scrollIntoView({ block: 'start' }));
  const release = () => ro.disconnect();
  ro.observe(main);
  for (const ev of ['wheel', 'touchstart', 'keydown', 'mousedown']) win.addEventListener(ev, release, { once: true, passive: true });
  win.setTimeout(release, 8000);
}

function mount() {
  holdAnchor();
  if (!$('ov-companies') || !echarts) return;
  let samples = [], last = -1, state = getState();
  const charts = {};

  const draw = () => {
    for (const [id, option] of [['ov-companies', companiesOption], ['ov-standing', standingOption], ['ov-monthly', monthlyOption], ['ov-fleet', fleetOption]]) {
      charts[id] = charts[id] || ecInit($(id), echarts);
      if (charts[id]) charts[id].setOption(option(samples, state), { replaceMerge: ['series'] });
    }
    if (charts['ov-standing'] && !charts['ov-standing']._wired) {
      charts['ov-standing']._wired = true;
      charts['ov-standing'].on('click', (p) => focus(p.data && p.data.id));
    }
    if (charts['ov-companies'] && !charts['ov-companies']._wired) {
      charts['ov-companies']._wired = true;
      charts['ov-companies'].on('click', (p) => focus(p.seriesId));
    }
    if (charts['ov-monthly'] && !charts['ov-monthly']._wired) {
      charts['ov-monthly']._wired = true;
      charts['ov-monthly'].on('click', (p) => focus(p.seriesId));
    }
    if (charts['ov-fleet'] && !charts['ov-fleet']._wired) {
      charts['ov-fleet']._wired = true;
      charts['ov-fleet'].on('click', () => { location.href = '/resource/vehicles'; });
    }
    const vehicles = delta(samples, (s) => s.vehicles);
    $('ov-vehicles-delta').textContent = vehicles;
    const single = companyIds(samples).length === 1;
    $('ov-standing-title').textContent = single ? 'Balance sheet today' : 'Standing today';
    $('ov-standing-note').textContent = single ? `${nameOf(samples, companyIds(samples)[0])}: money, loan, value and net worth on the last game day` : 'The same metric, the last game day; click a company to focus it';
    const tiles = summaryTiles(samples, state.metric);
    $('ov-leader').textContent = tiles.leader;
    $('ov-month-net').textContent = tiles.month;
    $('ov-month-sub').textContent = tiles.monthSub;
    $('ov-monthly-note').textContent = `How much each company's ${METRICS[state.metric].toLowerCase()} moved in each game month (the metric above): above zero it gained, below it lost; click a bar to focus the company`;
    $('ov-samples').textContent = samples.length ? `${samples.length} game days kept · ${samples[0].date} to ${samples[samples.length - 1].date}` : 'waiting for the first game day';
    $('ov-focus-clear').classList.toggle('hidden', !state.focus);
    // Before the first company there is nothing to draw: say so instead of an empty frame.
    const none = !companyIds(samples).length;
    for (const id of ['ov-companies-empty', 'ov-standing-empty']) $(id).classList.toggle('hidden', !none);
  };
  const focus = (id) => {
    state.focus = id && id !== state.focus ? String(id) : '';
    setState({ focus: state.focus });
    draw();
  };

  const pull = async () => {
    try {
      const r = await fetch(`/api/overview/history?since=${last}`);
      const d = await r.json();
      if (typeof d.last === 'number' && d.last < last) { samples = []; last = -1; return pull(); }
      if (d.samples.length) {
        samples = samples.concat(d.samples).filter((s, i, all) => i === all.length - 1 || all[i + 1].date !== s.date);
        last = samples[samples.length - 1].t;
        draw();
      }
    } catch { /* the panel restarts; the next pull retries */ }
  };

  // Controls: the metric and range selects, the focus reset, the maximize buttons.
  $('ov-metric').innerHTML = Object.entries(METRICS).map(([k, v]) => `<option value="${k}"${k === state.metric ? ' selected' : ''}>${v}</option>`).join('');
  $('ov-range').innerHTML = Object.entries(RANGES).map(([k, v]) => `<option value="${k}"${k === state.range ? ' selected' : ''}>${v}</option>`).join('');
  $('ov-metric').onchange = (e) => { state.metric = e.target.value; setState({ metric: state.metric }); draw(); };
  $('ov-range').onchange = (e) => { state.range = e.target.value; setState({ range: state.range }); draw(); };
  $('ov-focus-clear').onclick = () => focus('');
  for (const box of document.querySelectorAll('[data-maximize]')) {
    const id = box.dataset.maximize;
    box.querySelector('[data-toolbar]').append(maximizable(box, id, { getState, setState, onResize: () => charts[box.dataset.chart]?.resize() }));
  }

  // Live: the event stream pushes the moments worth a redraw; the poll covers a dropped stream.
  const live = $('ov-live');
  const say = (text, state) => { live.querySelector('[data-live-text]').textContent = text; live.dataset.state = state; };
  try {
    const es = new EventSource('/api/events/stream?kinds=game.month,vehicles.count,company.founded,company.removed,game.new_world,game.paused,game.resumed');
    es.onopen = () => say('live', 'live');
    es.onerror = () => say('reconnecting', 'off');
    es.onmessage = pull;
    for (const k of ['game.month', 'vehicles.count', 'company.founded', 'company.removed', 'game.new_world']) es.addEventListener(k, pull);
  } catch { say('polling', 'off'); }
  setInterval(pull, 3000);

  // Draw when the boxes have a size (not on a hidden tab), then once more after layout settles.
  setTimeout(() => { pull(); }, 30);
  setTimeout(() => { draw(); for (const c of Object.values(charts)) c && c.resize(); }, 300);
}

if (globalThis.document) mount();
// The one door for the classic scripts of this page (app.js's plan graph).
globalThis.ecInit = ecInit;
