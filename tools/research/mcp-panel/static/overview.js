/* The overview's charts: the game's history as the panel keeps it (GET /api/overview/history, one
   sample per game day, kept server-side so a reload keeps it), pushed live by the event stream
   (SSE /api/events/stream) and polled as a fallback. Every chart goes through the one door
   (components.js ecInit), is registered in CHARTS, keeps its parameters in the address, and can be
   maximized (Escape restores). A mark leads somewhere: a company focuses it in every chart, the
   fleet leads to the vehicles resource page. */

import { ecInit, maximizable, PALETTE } from './components.js';

const echarts = globalThis.echarts;
const $ = (id) => document.getElementById(id);
const DAY = 86400000;
const INK = { text: '#d1d5db', muted: '#9ca3af', grid: '#374151' };
const compact = new Intl.NumberFormat('en', { notation: 'compact', maximumFractionDigits: 1 });
const whole = new Intl.NumberFormat('en');

export const METRICS = { net: 'Net worth (money − loan)', value: 'Company value', money: 'Money', loan: 'Loan' };
export const RANGES = { 90: 'Last 3 months', 365: 'Last year', 1825: 'Last 5 years', all: 'All' };

/** The registry: every chart of the overview, its parameters (address keys) and where a click goes. */
export const CHARTS = [
  { id: 'ov-companies', kind: 'line', params: ['ov_metric', 'ov_range', 'ov_focus'], drill: 'a company focuses it' },
  { id: 'ov-standing', kind: 'bar', params: ['ov_metric', 'ov_focus'], drill: 'a company focuses it' },
  { id: 'ov-fleet', kind: 'line', params: ['ov_range'], drill: '/resource/vehicles' },
];

/* ---- address state ------------------------------------------------------------------------- */
const params = () => new URL(location.href).searchParams;
export function getState() {
  const p = params();
  const metric = METRICS[p.get('ov_metric')] ? p.get('ov_metric') : 'net';
  const range = RANGES[p.get('ov_range')] ? p.get('ov_range') : 'all';
  return { metric, range, focus: p.get('ov_focus') || '', max: p.get('max') || undefined };
}
export function setState(patch) {
  const u = new URL(location.href);
  const keys = { metric: 'ov_metric', range: 'ov_range', focus: 'ov_focus', max: 'max' };
  for (const [k, v] of Object.entries(patch)) {
    if (v === undefined || v === '' || v === null) u.searchParams.delete(keys[k]); else u.searchParams.set(keys[k], v);
  }
  globalThis.history.replaceState(null, '', u);
}

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
const axis = { axisLabel: { color: INK.muted, hideOverlap: true }, axisLine: { lineStyle: { color: INK.grid } }, splitLine: { lineStyle: { color: INK.grid, type: 'dashed' } } };
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
    grid: { left: 8, right: 56, top: 36, bottom: 48, containLabel: true },
    xAxis: { type: 'time', ...axis, splitLine: { show: false } },
    yAxis: { type: 'value', name: METRICS[metric], nameTextStyle: { color: INK.muted, align: 'left' }, ...axis, axisLabel: { color: INK.muted, formatter: (v) => compact.format(v) } },
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

export function standingOption(samples, { metric = 'net', focus = '' } = {}) {
  const ids = companyIds(samples);
  const last = samples[samples.length - 1];
  const rows = last ? last.companies.map((c) => ({ id: String(c.id), name: c.name || `company ${c.id}`, v: c[metric] })) : [];
  rows.sort((a, b) => a.v - b.v);
  return {
    backgroundColor: 'transparent',
    tooltip: { trigger: 'item', valueFormatter: (v) => whole.format(Math.round(v)) },
    grid: { left: 8, right: 56, top: 8, bottom: 8, containLabel: true },
    xAxis: { type: 'value', splitNumber: 3, ...axis, axisLabel: { color: INK.muted, hideOverlap: true, formatter: (v) => compact.format(v) } },
    yAxis: { type: 'category', data: rows.map((r) => r.name), axisLabel: { color: INK.text, width: 120, overflow: 'truncate', ellipsis: '…' }, axisLine: { lineStyle: { color: INK.grid } } },
    series: [{
      type: 'bar', name: METRICS[metric], barMaxWidth: 28, cursor: 'pointer',
      label: { show: true, position: 'right', color: INK.text, formatter: (p) => compact.format(p.value) },
      data: rows.map((r) => ({ value: r.v, id: r.id, itemStyle: { color: colour(ids, r.id), opacity: focus && focus !== r.id ? 0.3 : 1, borderRadius: [0, 4, 4, 0] } })),
    }],
  };
}

export function fleetOption(samples, { range = 'all' } = {}) {
  return {
    backgroundColor: 'transparent',
    tooltip: { trigger: 'axis' },
    grid: { left: 8, right: 24, top: 16, bottom: 48, containLabel: true },
    xAxis: { type: 'time', ...axis, splitLine: { show: false } },
    yAxis: { type: 'value', minInterval: 1, ...axis },
    dataZoom: zoom(zoomFrom(samples, range)),
    series: [{
      name: 'Vehicles', type: 'line', step: 'end', showSymbol: false, cursor: 'pointer', color: PALETTE.dark[2],
      areaStyle: { opacity: 0.18 }, data: samples.map((s) => [when(s), s.vehicles]),
    }],
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

/* ---- the page ------------------------------------------------------------------------------- */
function mount() {
  if (!$('ov-companies') || !echarts) return;
  let samples = [], last = -1, state = getState();
  const charts = {};

  const draw = () => {
    for (const [id, option] of [['ov-companies', companiesOption], ['ov-standing', standingOption], ['ov-fleet', fleetOption]]) {
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
    if (charts['ov-fleet'] && !charts['ov-fleet']._wired) {
      charts['ov-fleet']._wired = true;
      charts['ov-fleet'].on('click', () => { location.href = '/resource/vehicles'; });
    }
    const vehicles = delta(samples, (s) => s.vehicles);
    $('ov-vehicles-delta').textContent = vehicles;
    $('ov-samples').textContent = samples.length ? `${samples.length} game days kept · ${samples[0].date} to ${samples[samples.length - 1].date}` : 'waiting for the first game day';
    $('ov-focus-clear').classList.toggle('hidden', !state.focus);
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
  try {
    const es = new EventSource('/api/events/stream?kinds=game.month,vehicles.count,company.founded,company.removed,game.new_world,game.paused,game.resumed');
    es.onopen = () => { live.textContent = 'live'; live.dataset.state = 'live'; };
    es.onerror = () => { live.textContent = 'reconnecting'; live.dataset.state = 'off'; };
    es.onmessage = pull;
    for (const k of ['game.month', 'vehicles.count', 'company.founded', 'company.removed', 'game.new_world']) es.addEventListener(k, pull);
  } catch { live.textContent = 'polling'; }
  setInterval(pull, 3000);

  // Draw when the boxes have a size (not on a hidden tab), then once more after layout settles.
  setTimeout(() => { pull(); }, 30);
  setTimeout(() => { draw(); for (const c of Object.values(charts)) c && c.resize(); }, 300);
}

if (globalThis.document) mount();
// The one door for the classic scripts of this page (app.js's plan graph).
globalThis.ecInit = ecInit;
