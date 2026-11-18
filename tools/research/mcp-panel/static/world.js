'use strict';
import {
  ecInit as panelEcInit, PALETTE as THEME_PALETTE, $, esc as escHtml, UI, badge,
} from './components.js';
/* The world view: the whole map (bespoke canvas) and the road network (ECharts, map coordinates),
   both derived from MCP resources through the panel. Parameters live in the URL. */

const PALETTE = THEME_PALETTE.dark;
const TYPE_COLOURS = {
  C: '#4c6b3a', R: '#8a8a8a', r: '#d9d2b8', H: '#c98500', T: '#2e5d2a', S: '#e66767',
  W: '#2a6fb0', V: '#111111', I: '#9085e9', B: '#b0a070', O: '#6b6b6b',
};
const TYPE_NAMES = { C: 'clear', R: 'rail', r: 'road', H: 'house', T: 'trees', S: 'station', W: 'water', I: 'industry', B: 'bridge/tunnel', O: 'object' };

/* ---- URL state ----------------------------------------------------------------------------- */
/** The page's state from the address query. */
export function readState(search) {
  const params = new URLSearchParams(search);
  return {
    layer: params.get('layer') || 'types', component: params.get('component') || 'all', live: params.get('live') !== '0',
    // The planner's parameters, steerable here and kept in the address (a configured plan is a link).
    budget: +(params.get('budget') || 300000), payback: +(params.get('payback') || 24),
    modes: (params.get('modes') || 'bus,rail,air,truck,freight_rail').split(',').filter(Boolean),
  };
}
/** The address query of a state (readState's inverse). */
export function stateQuery(s) {
  return `?${new URLSearchParams({ layer: s.layer, component: s.component, live: s.live ? '1' : '0',
    budget: s.budget, payback: s.payback, modes: s.modes.join(',') })}`;
}
const state = readState(location.search);
function saveState() {
  history.replaceState(null, '', `${location.pathname}${stateQuery(state)}`);
}

/* ---- charts: through the one door (components.js ecInit), in the dark theme ------------------ */
const charts = {};
const darkEcharts = { init: (box, _theme, opts) => echarts.init(box, 'dark', opts), getInstanceByDom: (box) => echarts.getInstanceByDom(box) };
function ecInit(id) {
  if (charts[id]) return charts[id];
  const chart = panelEcInit($(id), darkEcharts);
  if (!chart) return null;
  const set = chart.setOption;
  chart.setOption = (opt, ...rest) => set({ backgroundColor: 'transparent', ...opt }, ...rest);
  charts[id] = chart;
  return chart;
}

/* ---- data -------------------------------------------------------------------------------------- */
async function get(path) {
  const r = await fetch(path);
  const j = await r.json();
  return j.data ?? j;
}
let map = null; let towns = []; let stations = []; let graph = null; let plan = null;
const MODE_COLOURS = { bus: PALETTE[2], rail: PALETTE[1], air: PALETTE[0], truck: PALETTE[3], freight_rail: PALETTE[4] };

/* ---- derived from the data (pure) ---------------------------------------------------------- */
/** A tile's colour on a layer, from its type letter, its height (two hex digits) and its owner. */
export function tileColour(layer, type, heightHex, owner) {
  if (layer === 'heights') {
    const v = Math.min(255, 40 + parseInt(heightHex, 16) * 12);
    return type === 'W' ? '#2a6fb0' : `rgb(${v},${v},${v})`;
  }
  if (layer === 'owners') {
    if (/[0-9a-e]/.test(owner)) return PALETTE[parseInt(owner, 16) % 8];
    return owner === 'T' ? '#6b6b6b' : (owner === 'W' ? '#2a6fb0' : '#1f2937');
  }
  return TYPE_COLOURS[type] || '#000';
}
export const legendHtml = (layer) => (layer === 'types'
  ? Object.entries(TYPE_NAMES).map(([k, n]) => `<span class="inline-flex items-center gap-1"><span class="inline-block w-3 h-3 rounded-sm" style="background:${TYPE_COLOURS[k]}"></span>${n}</span>`).join('')
  : (layer === 'owners' ? 'colour per company owner; grey town; blue water' : 'brighter is higher; blue water'));
/** The plan's pairs over every mode, each pair once (a pair can be the best of several modes). */
export function planPairs(p) {
  const rows = []; const seen = new Set();
  for (const x of Object.values((p && p.by_mode) || {}).flat()) {
    const k = `${x.a.id}-${x.b.id}`;
    if (!seen.has(k)) { seen.add(k); rows.push(x); }
  }
  return rows;
}
/** The road graph of one component (or all): the component list, the chart's nodes and links. */
export function graphView(g, component) {
  const comps = [...new Set(g.nodes.map((n) => n.component))].sort((a, b) => a - b);
  const want = (n) => component === 'all' || String(n.component) === component;
  const kinds = ['town', 'station', 'junction', 'end'];
  const nodes = g.nodes.filter(want).map((n) => ({
    id: String(n.id), name: n.label || `${n.kind} (${n.x},${n.y})`, x: n.x, y: n.y, category: kinds.indexOf(n.kind),
    symbolSize: n.kind === 'town' ? 14 : (n.kind === 'station' ? 9 : 4),
    label: { show: n.kind === 'town', formatter: n.label, color: '#e5e7eb' }, value: n.component,
  }));
  const ids = new Set(nodes.map((n) => n.id));
  const links = g.edges.filter((e) => ids.has(String(e.source)) && ids.has(String(e.target)))
    .map((e) => ({ source: String(e.source), target: String(e.target), value: e.length }));
  return { comps, kinds, nodes, links };
}
/** A mode as a badge in the mode's own colour. */
const modeBadge = (m) => `<span class="${UI.badge} text-white" style="background:${MODE_COLOURS[m] || '#888'}">${m}</span>`;
export function portfolioText(pf) {
  const k = pf.kernel ? ` · kernel ${pf.kernel.id} (work ${pf.kernel.work}, plate ${pf.kernel.plate})` : '';
  return `solver: ${pf.solver}${k} · total score ${pf.total_score ?? ''} · invest ${pf.investment_k ?? ''}k of ${pf.budget_k ?? ''}k`;
}
export const portfolioHtml = (pf) => (pf.chosen || []).map((c) => {
  const from = (c.a || c.producer || {}); const to = (c.b || c.acceptor || {});
  const what = c.kind === 'freight' ? `${c.cargo} by ${c.mode}` : `passengers by ${c.mode}`;
  return `<li class="cursor-pointer hover:underline" data-x="${from.x}" data-y="${from.y}">${modeBadge(c.mode)} ${what}: ${from.name} → ${to.name}, score ${c.score}</li>`;
}).join('') || '<li>no investment fits</li>';
export const planRowsHtml = (rows) => rows.map((p, i) => `<tr data-i="${i}" class="bg-white border-b dark:bg-gray-800 dark:border-gray-700 hover:bg-gray-50 dark:hover:bg-gray-600 cursor-pointer"><td class="px-3 py-2">${modeBadge(p.mode)}</td><td class="px-3 py-2 font-medium text-gray-900 dark:text-white">${p.a.name}</td><td class="px-3 py-2">${p.b.name}</td><td class="px-3 py-2">${p.distance}</td><td class="px-3 py-2">${p.supply}</td><td class="px-3 py-2">${p.road_connected ? 'yes' : 'no'}</td><td class="px-3 py-2">${p.score}</td></tr>`).join('');
/** What a tile is, read from the map resource. */
export function tileText(m, x, y, what) {
  const t = m ? m.types[y][x] : '?';
  const z = m ? parseInt(m.heights[y].substr(x * 2, 2), 16) : '?';
  const o = m ? m.owners[y][x] : '?';
  return `${what || 'tile'} (${x},${y}): ${TYPE_NAMES[t] || t}, height ${z}, owner ${o}`;
}

function drawMap() {
  if (!map) return;
  const c = $('map'); const w = map.width; const h = map.height;
  const px = Math.max(1, Math.floor(c.clientWidth / w));
  c.width = w * px; c.height = h * px;
  const g = c.getContext('2d');
  for (let y = 0; y < h; y++) {
    const tr = map.types[y]; const hr = map.heights[y]; const orow = map.owners[y];
    for (let x = 0; x < w; x++) {
      g.fillStyle = tileColour(state.layer, tr[x], hr.substr(x * 2, 2), orow[x]);
      g.fillRect(x * px, y * px, px, px);
    }
  }
  if (plan) {
    /* The plan: the best pairs of each mode, a line per pair in the mode's colour. */
    for (const p of planPairs(plan)) {
      g.strokeStyle = MODE_COLOURS[p.mode] || '#fff'; g.lineWidth = Math.max(2, px); g.setLineDash(p.mode === 'air' ? [px * 4, px * 3] : []);
      g.beginPath(); g.moveTo(p.a.x * px, p.a.y * px); g.lineTo(p.b.x * px, p.b.y * px); g.stroke();
    }
    g.setLineDash([]);
  }
  g.font = `${Math.max(10, px * 4)}px Inter, sans-serif`; g.textAlign = 'center';
  for (const s of stations) { g.fillStyle = '#ffffff'; g.fillRect(s.x * px - px, s.y * px - px, px * 3, px * 3); }
  for (const t of towns) {
    g.fillStyle = 'rgba(0,0,0,0.6)'; g.fillText(t.name, t.x * px + 1, t.y * px - px * 2 + 1);
    g.fillStyle = '#ffffff'; g.fillText(t.name, t.x * px, t.y * px - px * 2);
  }
  $('map-legend').innerHTML = legendHtml(state.layer);
}

function drawGraph() {
  if (!graph) return;
  const chart = ecInit('graph'); if (!chart) return;
  const sel = $('p-component'); const keep = state.component;
  const { comps, kinds, nodes, links } = graphView(graph, state.component);
  sel.innerHTML = '<option value="all">All components</option>' + comps.map((c) => `<option value="${c}">Component ${c}</option>`).join('');
  sel.value = comps.map(String).includes(keep) ? keep : 'all';
  chart.setOption({
    tooltip: { formatter: (p) => p.dataType === 'edge' ? `road, ${p.data.value} tiles` : `${p.data.name}<br>component ${p.data.value}` },
    legend: [{ data: kinds, textStyle: { color: '#d1d5db' }, top: 0 }],
    series: [{
      type: 'graph', layout: 'none', coordinateSystem: null, roam: 'move', zoom: 1,
      categories: kinds.map((k, i) => ({ name: k, itemStyle: { color: PALETTE[i] } })),
      data: nodes, links, lineStyle: { color: '#9ca3af', width: 1.2, opacity: 0.7 },
      emphasis: { focus: 'adjacency' }, labelLayout: { hideOverlap: true },
    }],
  }, true);
  $('w-stats').textContent = `${graph.stats.road_tiles} road tiles · ${graph.stats.nodes} nodes · ${graph.stats.edges} edges · ${graph.stats.components} components`;
}

function drawPortfolio() {
  const pf = plan && plan.portfolio; if (!pf) return;
  $('portfolio-solver').textContent = portfolioText(pf);
  $('portfolio-body').innerHTML = portfolioHtml(pf);
  $('portfolio-body').querySelectorAll('li[data-x]').forEach((li) => li.addEventListener('click', () => goTo(+li.dataset.x, +li.dataset.y, li.textContent)));
}

function drawPlan() {
  if (!plan) return;
  drawPortfolio();
  const rows = planPairs(plan).sort((x, y) => y.score - x.score);
  $('plan-body').innerHTML = planRowsHtml(rows);
  $('plan-body').querySelectorAll('tr').forEach((tr) => tr.addEventListener('click', () => { const p = rows[+tr.dataset.i]; goTo(p.a.x, p.a.y, `${p.mode}: ${p.a.name} - ${p.b.name}`); }));
}

async function refresh() {
  try {
    [map, { towns }, { stations }, graph] = await Promise.all([
      get('/api/resources/map'), get('/api/resources/towns'), get('/api/resources/stations'), fetch('/api/graph').then((r) => r.json())]);
    try { plan = await (await fetch('/api/plan?limit=10&params=' + encodeURIComponent(JSON.stringify({ budget: state.budget, payback_months: state.payback, modes: state.modes })))).json(); } catch (e) { plan = null; }
    drawMap(); drawGraph(); drawPlan();
    $('w-msg').textContent = `updated ${new Date().toLocaleTimeString()} from openttd://map, openttd://towns, openttd://stations and /api/graph`;
  } catch (e) { $('w-msg').textContent = `not reachable: ${e.message}`; }
}

/* ---- click to drive the owner's camera -------------------------------------------------------- */
async function viewTool(name, args) {
  const r = await fetch('/api/view/tool', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ name, arguments: args }) });
  return r.json();
}
async function goTo(x, y, what) {
  $('w-msg').textContent = `${tileText(map, x, y, what)}; moving your camera there`;
  try {
    await viewTool('openttd.view.scroll_to', { x, y });
    await viewTool('openttd.view.mark', { x, y });
  } catch (e) { $('w-msg').textContent += ` (failed: ${e.message})`; }
}
$('map').addEventListener('click', (e) => {
  if (!map) return;
  const c = $('map'); const r = c.getBoundingClientRect();
  const x = Math.floor((e.clientX - r.left) / r.width * map.width);
  const y = Math.floor((e.clientY - r.top) / r.height * map.height);
  if (x >= 0 && y >= 0 && x < map.width && y < map.height) goTo(x, y);
});
$('map').style.cursor = 'crosshair';
setTimeout(() => {
  const chart = ecInit('graph');
  if (chart) chart.on('click', (p) => { if (p.dataType === 'node') goTo(p.data.x, p.data.y, p.data.name); });
}, 60);

/* ---- frame: maximize, Escape restores ---------------------------------------------------------- */
document.querySelectorAll('[data-maximize]').forEach((b) => b.addEventListener('click', () => {
  const f = b.closest('[data-frame]');
  const on = !f.classList.contains('fixed');
  f.classList.toggle('fixed', on); f.classList.toggle('inset-0', on); f.classList.toggle('z-[60]', on); f.classList.toggle('overflow-auto', on);
  b.textContent = on ? 'Restore (Esc)' : 'Maximize';
  setTimeout(() => { drawMap(); Object.values(charts).forEach((c) => c.resize()); }, 30);
}));
document.addEventListener('keydown', (e) => {
  if (e.key !== 'Escape') return;
  document.querySelectorAll('[data-frame].fixed [data-maximize]').forEach((b) => b.click());
});

$('p-layer').value = state.layer;
$('p-live').checked = state.live;
$('p-budget').value = state.budget;
$('p-payback').value = state.payback;
document.querySelectorAll('[data-mode]').forEach((c) => {
  c.checked = state.modes.includes(c.dataset.mode);
  c.onchange = () => { state.modes = [...document.querySelectorAll('[data-mode]:checked')].map((x) => x.dataset.mode); saveState(); refresh(); };
});
$('p-budget').onchange = (e) => { state.budget = Math.max(0, +e.target.value || 0); saveState(); refresh(); };
$('p-payback').onchange = (e) => { state.payback = Math.max(1, +e.target.value || 24); saveState(); refresh(); };
$('p-layer').onchange = (e) => { state.layer = e.target.value; saveState(); drawMap(); };
$('p-component').onchange = (e) => { state.component = e.target.value; saveState(); drawGraph(); };
$('p-live').onchange = (e) => { state.live = e.target.checked; saveState(); };
saveState();
setTimeout(refresh, 30);
/* ---- transfer yards, proven offline before a bot builds one (GET /api/plan?track&yards) -------- */
const money = (v) => (v == null ? '-' : Math.round(v).toLocaleString('en'));
export function yardRow(y) {
  const verdict = y.proven ? badge('proven', 'green') : badge('refuted', 'red');
  const why = y.proven ? `side ${y.side > 0 ? '+1' : '-1'}, B ${y.extra > 0 ? '+' : ''}${money(y.extra)}` : (y.tried || []).map((t) => `side ${t.side}: ${t.result}`).join('; ') || (y.a_route ? '' : 'A has no route');
  const e = y.shared && y.shared.end;
  return `<tr data-x="${e ? e.x : ''}" data-y="${e ? e.y : ''}" class="bg-white border-b dark:bg-gray-800 dark:border-gray-700 hover:bg-gray-50 dark:hover:bg-gray-600 cursor-pointer">
    <td class="px-3 py-2">${verdict}</td><td class="px-3 py-2 font-medium text-gray-900 dark:text-white">${escHtml(y.a)}</td><td class="px-3 py-2">${escHtml(y.b)}</td>
    <td class="px-3 py-2 whitespace-nowrap">${e ? `(${e.x}, ${e.y})` : '-'}</td><td class="px-3 py-2 text-right">${money(y.b_alone)}</td><td class="px-3 py-2 text-right">${money(y.b_with_yard)}</td><td class="px-3 py-2">${escHtml(why)}</td></tr>`;
}
$('yards-run').addEventListener('click', async () => {
  const btn = $('yards-run');
  btn.disabled = true; btn.textContent = 'Proving…';
  $('yards-body').innerHTML = '<tr><td class="px-3 py-2" colspan="7">Laying the routes with AlgoGlyph track-planning…</td></tr>';
  try {
    const r = await fetch('/api/plan?limit=10&track=6&yards=4');
    const d = await r.json();
    const pairs = (d.yards && d.yards.pairs) || [];
    $('yards-body').innerHTML = r.ok
      ? (pairs.map(yardRow).join('') || '<tr><td class="px-3 py-2" colspan="7">No two priced lines meet at one station site.</td></tr>')
      : `<tr><td class="px-3 py-2 text-red-600 dark:text-red-400" colspan="7">${escHtml(d.error || 'HTTP ' + r.status)}</td></tr>`;
  } catch (e) {
    $('yards-body').innerHTML = `<tr><td class="px-3 py-2 text-red-600" colspan="7">${escHtml(e.message)}</td></tr>`;
  }
  btn.disabled = false; btn.textContent = 'Prove yards';
});
$('yards-body').addEventListener('click', (e) => {
  const tr = e.target.closest('tr[data-x]');
  if (tr && tr.dataset.x !== '') goTo(+tr.dataset.x, +tr.dataset.y, 'shared end of a yard');
});

/* ---- back-propagation: real profit against the plan (GET /api/plan?calibrate=1) --------------- */
/** Each mode's factor, real over predicted, as a badge: green from 0.9, yellow from 0.5, else red. */
export const calModesHtml = (modes) => Object.entries(modes || {}).map(([m, f]) => badge(
  `${m} × ${f.factor} (${f.lines} line${f.lines === 1 ? '' : 's'}, real / predicted)`,
  f.factor >= 0.9 ? 'green' : f.factor >= 0.5 ? 'yellow' : 'red',
)).join('') || '<span class="text-xs text-gray-500">no line measured yet</span>';
/** One row per measured line; `name` names a station id. */
export const calRowsHtml = (lines, name) => (lines || []).map((l) => `<tr data-st="${l.stations[0]}" class="bg-white border-b dark:bg-gray-800 dark:border-gray-700 hover:bg-gray-50 dark:hover:bg-gray-600 cursor-pointer">
      <td class="px-3 py-2 font-medium text-gray-900 dark:text-white">${escHtml(name(l.stations[0]))} - ${escHtml(name(l.stations[1]))}</td>
      <td class="px-3 py-2">${escHtml(l.mode || l.type)}</td><td class="px-3 py-2">${escHtml(l.candidate || '-')}</td>
      <td class="px-3 py-2 text-right">${money(l.real_per_vehicle_month)}</td><td class="px-3 py-2 text-right">${money(l.predicted_per_vehicle_month)}</td>
      <td class="px-3 py-2 text-right">${l.ratio == null ? '-' : l.ratio}</td><td class="px-3 py-2">${escHtml(l.why || '')}</td></tr>`).join('')
  || '<tr><td class="px-3 py-2" colspan="7">No vehicle shuttles between two stations yet.</td></tr>';
let calStations = [];
$('cal-run').addEventListener('click', async () => {
  const btn = $('cal-run');
  btn.disabled = true; btn.textContent = 'Calibrating…';
  try {
    const [r, st] = await Promise.all([fetch('/api/plan?calibrate=1'), get('/api/resources/stations')]);
    const d = await r.json();
    calStations = st.stations || [];
    const c = d.calibration;
    if (!r.ok || !c) throw new Error(d.error || 'no calibration in the answer');
    const name = (id) => (calStations.find((s) => s.id === id) || {}).name || `#${id}`;
    $('cal-modes').innerHTML = calModesHtml(c.modes);
    $('cal-body').innerHTML = calRowsHtml(c.lines, name);
    $('cal-params').textContent = 'next plan: /api/plan?params=' + JSON.stringify(c.params);
  } catch (e) {
    $('cal-body').innerHTML = `<tr><td class="px-3 py-2 text-red-600" colspan="7">${escHtml(e.message)}</td></tr>`;
  }
  btn.disabled = false; btn.textContent = 'Calibrate';
});
$('cal-body').addEventListener('click', (e) => {
  const tr = e.target.closest('tr[data-st]');
  const s = tr && calStations.find((x) => String(x.id) === tr.dataset.st);
  if (s) goTo(s.x, s.y, `station ${s.name}`);
});

setInterval(() => { if (state.live) refresh(); }, 5000);
window.addEventListener('resize', () => setTimeout(drawMap, 30));

/* ---- live view of the owner's game window --------------------------------------------------- */
(function liveView() {
  const img = document.getElementById('live-img'); if (!img) return;
  let busy = false;
  setInterval(() => {
    if (busy || document.hidden) return; busy = true;
    const next = new Image();
    next.onload = () => { img.src = next.src; busy = false; document.getElementById('live-msg').textContent = 'updated ' + new Date().toLocaleTimeString(); };
    next.onerror = () => { busy = false; document.getElementById('live-msg').textContent = 'no GUI client attached yet'; };
    next.src = '/api/view/screenshot?t=' + Date.now();
  }, 1200);
})();
