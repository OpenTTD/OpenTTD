'use strict';
/* The world view: the whole map (bespoke canvas) and the road network (ECharts, map coordinates),
   both derived from MCP resources through the panel. Parameters live in the URL. */

const $ = (id) => document.getElementById(id);
const PALETTE = ['#3987e5', '#d95926', '#199e70', '#c98500', '#d55181', '#008300', '#9085e9', '#e66767'];
const TYPE_COLOURS = {
  C: '#4c6b3a', R: '#8a8a8a', r: '#d9d2b8', H: '#c98500', T: '#2e5d2a', S: '#e66767',
  W: '#2a6fb0', V: '#111111', I: '#9085e9', B: '#b0a070', O: '#6b6b6b',
};
const TYPE_NAMES = { C: 'clear', R: 'rail', r: 'road', H: 'house', T: 'trees', S: 'station', W: 'water', I: 'industry', B: 'bridge/tunnel', O: 'object' };
const reduced = window.matchMedia('(prefers-reduced-motion: reduce)').matches;

/* ---- URL state ----------------------------------------------------------------------------- */
const params = new URLSearchParams(location.search);
const state = { layer: params.get('layer') || 'types', component: params.get('component') || 'all', live: params.get('live') !== '0' };
function saveState() {
  const p = new URLSearchParams({ layer: state.layer, component: state.component, live: state.live ? '1' : '0' });
  history.replaceState(null, '', `${location.pathname}?${p}`);
}

/* ---- the one door to ECharts ---------------------------------------------------------------- */
const charts = {};
function ecInit(id) {
  const el = $(id);
  if (!el || el.offsetWidth === 0) return null;
  if (charts[id]) return charts[id];
  const chart = echarts.init(el, 'dark', { renderer: 'canvas' });
  const set = chart.setOption.bind(chart);
  chart.setOption = (opt, ...rest) => set({
    animationDuration: reduced ? 0 : 850, animationEasing: 'cubicOut', animationDurationUpdate: reduced ? 0 : 380,
    aria: { enabled: true, label: { description: el.getAttribute('aria-label') } }, backgroundColor: 'transparent', ...opt,
  }, ...rest);
  new ResizeObserver(() => chart.resize()).observe(el);
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

function drawMap() {
  if (!map) return;
  const c = $('map'); const w = map.width; const h = map.height;
  const px = Math.max(1, Math.floor(c.clientWidth / w));
  c.width = w * px; c.height = h * px;
  const g = c.getContext('2d');
  for (let y = 0; y < h; y++) {
    const tr = map.types[y]; const hr = map.heights[y]; const orow = map.owners[y];
    for (let x = 0; x < w; x++) {
      let col;
      if (state.layer === 'heights') {
        const z = parseInt(hr.substr(x * 2, 2), 16); const v = Math.min(255, 40 + z * 12);
        col = tr[x] === 'W' ? '#2a6fb0' : `rgb(${v},${v},${v})`;
      } else if (state.layer === 'owners') {
        const o = orow[x];
        col = /[0-9a-e]/.test(o) ? PALETTE[parseInt(o, 16) % 8] : (o === 'T' ? '#6b6b6b' : (o === 'W' ? '#2a6fb0' : '#1f2937'));
      } else col = TYPE_COLOURS[tr[x]] || '#000';
      g.fillStyle = col; g.fillRect(x * px, y * px, px, px);
    }
  }
  if (plan) {
    /* The plan: the best pairs of each mode, a line per pair in the mode's colour. */
    const seen = new Set();
    for (const ps of Object.values(plan.by_mode || {})) for (const p of ps) {
      const k = `${p.a.id}-${p.b.id}`; if (seen.has(k)) continue; seen.add(k);
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
  $('map-legend').innerHTML = state.layer === 'types'
    ? Object.entries(TYPE_NAMES).map(([k, n]) => `<span class="inline-flex items-center gap-1"><span class="inline-block w-3 h-3 rounded-sm" style="background:${TYPE_COLOURS[k]}"></span>${n}</span>`).join('')
    : (state.layer === 'owners' ? 'colour per company owner; grey town; blue water' : 'brighter is higher; blue water');
}

function drawGraph() {
  if (!graph) return;
  const chart = ecInit('graph'); if (!chart) return;
  const sel = $('p-component'); const keep = state.component;
  const comps = [...new Set(graph.nodes.map((n) => n.component))].sort((a, b) => a - b);
  sel.innerHTML = '<option value="all">All components</option>' + comps.map((c) => `<option value="${c}">Component ${c}</option>`).join('');
  sel.value = comps.map(String).includes(keep) ? keep : 'all';
  const want = (n) => state.component === 'all' || String(n.component) === state.component;
  const kinds = ['town', 'station', 'junction', 'end'];
  const nodes = graph.nodes.filter(want).map((n) => ({
    id: String(n.id), name: n.label || `${n.kind} (${n.x},${n.y})`, x: n.x, y: n.y, category: kinds.indexOf(n.kind),
    symbolSize: n.kind === 'town' ? 14 : (n.kind === 'station' ? 9 : 4),
    label: { show: n.kind === 'town', formatter: n.label, color: '#e5e7eb' }, value: n.component,
  }));
  const ids = new Set(nodes.map((n) => n.id));
  const links = graph.edges.filter((e) => ids.has(String(e.source)) && ids.has(String(e.target)))
    .map((e) => ({ source: String(e.source), target: String(e.target), value: e.length }));
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
  const k = pf.kernel ? ` · kernel ${pf.kernel.id} (work ${pf.kernel.work}, plate ${pf.kernel.plate})` : '';
  $('portfolio-solver').textContent = `solver: ${pf.solver}${k} · total score ${pf.total_score ?? ''} · invest ${pf.investment_k ?? ''}k of ${pf.budget_k ?? ''}k`;
  $('portfolio-body').innerHTML = (pf.chosen || []).map((c) => {
    const from = (c.a || c.producer || {}); const to = (c.b || c.acceptor || {});
    const what = c.kind === 'freight' ? `${c.cargo} by ${c.mode}` : `passengers by ${c.mode}`;
    return `<li class="cursor-pointer hover:underline" data-x="${from.x}" data-y="${from.y}"><span class="text-xs font-medium px-2 py-0.5 rounded-sm text-white" style="background:${MODE_COLOURS[c.mode] || '#888'}">${c.mode}</span> ${what}: ${from.name} → ${to.name}, score ${c.score}</li>`;
  }).join('') || '<li>no investment fits</li>';
  $('portfolio-body').querySelectorAll('li[data-x]').forEach((li) => li.addEventListener('click', () => goTo(+li.dataset.x, +li.dataset.y, li.textContent)));
}

function drawPlan() {
  if (!plan) return;
  drawPortfolio();
  const badge = (m) => `<span class="text-xs font-medium px-2.5 py-0.5 rounded-sm text-white" style="background:${MODE_COLOURS[m]}">${m}</span>`;
  const rows = []; const seen = new Set();
  for (const ps of Object.values(plan.by_mode || {})) for (const p of ps) { const k = `${p.a.id}-${p.b.id}`; if (!seen.has(k)) { seen.add(k); rows.push(p); } }
  rows.sort((x, y) => y.score - x.score);
  $('plan-body').innerHTML = rows.map((p, i) => `<tr data-i="${i}" class="bg-white border-b dark:bg-gray-800 dark:border-gray-700 hover:bg-gray-50 dark:hover:bg-gray-600 cursor-pointer"><td class="px-3 py-2">${badge(p.mode)}</td><td class="px-3 py-2 font-medium text-gray-900 dark:text-white">${p.a.name}</td><td class="px-3 py-2">${p.b.name}</td><td class="px-3 py-2">${p.distance}</td><td class="px-3 py-2">${p.supply}</td><td class="px-3 py-2">${p.road_connected ? 'yes' : 'no'}</td><td class="px-3 py-2">${p.score}</td></tr>`).join('');
  $('plan-body').querySelectorAll('tr').forEach((tr) => tr.addEventListener('click', () => { const p = rows[+tr.dataset.i]; goTo(p.a.x, p.a.y, `${p.mode}: ${p.a.name} - ${p.b.name}`); }));
}

async function refresh() {
  try {
    [map, { towns }, { stations }, graph] = await Promise.all([
      get('/api/resources/map'), get('/api/resources/towns'), get('/api/resources/stations'), fetch('/api/graph').then((r) => r.json())]);
    try { plan = await (await fetch('/api/plan?limit=10&params=' + encodeURIComponent(JSON.stringify({ budget: 300000 })))).json(); } catch (e) { plan = null; }
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
  const t = map ? map.types[y][x] : '?';
  const z = map ? parseInt(map.heights[y].substr(x * 2, 2), 16) : '?';
  const o = map ? map.owners[y][x] : '?';
  $('w-msg').textContent = `${what || 'tile'} (${x},${y}): ${TYPE_NAMES[t] || t}, height ${z}, owner ${o}; moving your camera there`;
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
$('p-layer').onchange = (e) => { state.layer = e.target.value; saveState(); drawMap(); };
$('p-component').onchange = (e) => { state.component = e.target.value; saveState(); drawGraph(); };
$('p-live').onchange = (e) => { state.live = e.target.checked; saveState(); };
saveState();
setTimeout(refresh, 30);
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
