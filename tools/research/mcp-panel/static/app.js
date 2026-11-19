'use strict';
/* The panel's home (index.html): status, approvals, overview tables, the developer console, live
   notifications, the cluster, news, the action plan and content. A classic script (it cannot import
   components.js): the Pro class sets come from the shell (static/shell.js publishes them as
   PanelUI before this runs). The markup builders are pure and exported for node --test
   (test/app.test.mjs); everything below them wires them to the page. */

const $ = (id) => document.getElementById(id);
const esc = (s) => String(s ?? '').replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
/** The shell's Pro class sets, one per role (static/shell.js UI). */
const UI = globalThis.PanelUI;
/** The shell's badge tones (static/shell.js TONE). */
const TONE = globalThis.PanelTONE;
const tag = (text, tone) => `<span class="${UI.badge} ${TONE[tone]}">${esc(text)}</span>`;

async function api(path, body) {
  const opts = body === undefined ? {} : {
    method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body),
  };
  const r = await fetch(path, opts);
  const text = await r.text();
  try { return JSON.parse(text); } catch { return { status: r.status, text }; }
}

const money = (v) => (typeof v === 'number' ? v.toLocaleString('en') : v ?? '-');

/* ---- markup from what the panel answers (pure) --------------------------------------------- */
/** The decisions waiting for the owner, each with its Approve. */
const pendingHtml = (pending) => (pending.length ? pending.map((p) => `
    <li class="flex items-center justify-between gap-2 p-2 border border-gray-200 rounded-lg dark:border-gray-700">
      <span class="min-w-0 font-mono break-all">#${p.id} ${esc(p.tool)}(${esc(JSON.stringify(p.args))})</span>
      <button data-approve="${p.id}" type="button" class="shrink-0 ${UI.primary}">Approve</button>
    </li>`).join('') : '<li>none</li>');
/** A game string as a person reads it: OpenTTD's formatting codes (font, colour, sprites: the
 *  private use area its strings carry them in) and control characters dropped, line breaks folded
 *  into one space. */
const gameText = (s) => String(s ?? '').replace(/[\ue000-\uf8ff\u0000-\u0008\u000b-\u001f\u007f-\u009f]/g, '')
  .replace(/\s+/g, ' ').trim();
/** One point of a Flowbite timeline: a time line and its text. */
const timelineItem = (time, text, ink = 'text-gray-700 dark:text-gray-300') => `
    <li class="mb-3 ms-4"><div class="absolute w-3 h-3 bg-gray-200 rounded-full mt-1.5 -start-1.5 border border-white dark:border-gray-900 dark:bg-gray-700"></div>
      <time class="text-xs font-normal leading-none text-gray-400 dark:text-gray-500">${time}</time>
      <p class="text-sm ${ink}">${esc(gameText(text))}</p></li>`;
const eventsHtml = (events) => events.map((e) => timelineItem(`${esc(e.t)} · ${esc(e.kind)}`, e.text)).join('');
/** The fleet as a player counts it (primary vehicles by type); an older server only has the pool
    count, which also counts wagons, parts and effects, and says so. */
function fleetText(d) {
  const f = d.fleet;
  if (!f) return { total: d.num_vehicles, types: 'pool items (wagons, effects included)' };
  const words = [['train', 'trains'], ['road', 'road'], ['ship', 'ships'], ['aircraft', 'aircraft']];
  const types = words.filter(([k]) => f[k]).map(([k, w]) => `${f[k]} ${w}`).join(' · ');
  return { total: f.total, types: types || 'none yet' };
}
const companiesHtml = (cs) => (cs.length ? cs.map((c) => `
    <tr class="border-b dark:border-gray-700"><td class="px-4 py-2">${esc(c.id)}</td>
      <td class="px-4 py-2 font-medium text-gray-900 dark:text-white">${esc(c.name ?? '')}</td>
      <td class="px-4 py-2">${c.is_ai ? tag('AI', 'purple') : 'human'}</td>
      <td class="px-4 py-2">${money(c.money)}</td><td class="px-4 py-2">${money(c.company_value ?? c.value)}</td>
      <td class="px-4 py-2">${esc(c.inaugurated_year ?? '')}</td></tr>`).join('')
  : '<tr><td class="px-4 py-3" colspan="6">no company yet (the AI starts one)</td></tr>');
const decisionsHtml = (ds) => (ds.length ? ds.slice().reverse().map((x) => `
    <tr class="border-b dark:border-gray-700"><td class="px-4 py-2">${esc(x.decision_id)}</td><td class="px-4 py-2">${esc(x.action_type)}</td>
      <td class="px-4 py-2">#${esc(x.approval_id)}</td><td class="px-4 py-2">${esc(x.command_result)}</td>
      <td class="px-4 py-2">${esc(x.verdict)}</td><td class="px-4 py-2">${esc(x.observed_tick)}</td></tr>`).join('')
  : '<tr><td class="px-4 py-3" colspan="6">none yet</td></tr>');

const clusterHtml = (instances) => instances.map((i) => {
  const panel = i.url || i.panel;
  return `<tr class="bg-white border-b dark:bg-gray-800 dark:border-gray-700"><td class="px-4 py-2 font-medium text-gray-900 dark:text-white">${esc(i.name)}</td><td class="px-4 py-2">${esc(i.role)}</td><td class="px-4 py-2">${i.alive ? tag('alive', 'green') : tag('down', 'red')}${i.error ? ` <span class="text-xs">${esc(i.error)}</span>` : ''}</td><td class="px-4 py-2"><code>${esc(i.endpoint || i.mcp)}</code></td><td class="px-4 py-2">${panel ? `<a class="font-medium text-blue-600 dark:text-blue-500 hover:underline" href="${esc(panel)}">${esc(panel)}</a>` : ''}</td></tr>`;
}).join('') || '<tr><td class="px-4 py-2" colspan="5">no instance registered</td></tr>';
const newsHtml = (news) => news.map((n) => timelineItem(`${esc(n.date)} · ${esc(n.type)}`, n.text, 'text-gray-900 dark:text-white')).join('')
  || '<li class="ms-4 text-sm">no news yet</li>';
const townOptions = (towns) => '<option value="">No viewport</option>'
  + towns.sort((a, b) => a.name.localeCompare(b.name)).map((t) => `<option value="${t.id}">${esc(t.name)}</option>`).join('');
const ANSWER = { yes: ['yes', 'green'], replaced: ['replaced', 'gray'], no: ['no', 'red'] };
const asksHtml = (asks) => asks.slice(0, 10).map((a) => `<li class="flex items-start gap-2"><span>${tag(...(ANSWER[a.answer] || ['waiting for you', 'yellow']))}</span><span class="text-gray-900 dark:text-white">#${a.id} ${esc(a.question)}</span></li>`).join('')
  || '<li>no question asked yet</li>';
const planSummary = (res) => `${res.steps.length} press(es) in ${res.waves} wave(s); final cash ${res.final.cash}, loan ${res.final.loan}; lowest cash ${res.lowest?.cash ?? res.lowest}`;
const planStepsHtml = (steps) => steps.map((s) => `<li>${s.n}. <b>${esc(s.action)}</b> ${s.presses > 1 ? `(${s.press}/${s.presses})` : ''} &rarr; cash ${s.cash}, loan ${s.loan}</li>`).join('');
/** The action plan as a DAG placed by wave (x) and order within its wave (y). */
function planGraphOption(graph) {
  const byWave = {};
  const nodes = graph.nodes.map((n) => { const i = (byWave[n.wave] = (byWave[n.wave] || 0) + 1) - 1; return { id: n.id, name: `${n.label || n.id}${n.presses > 1 ? ' x' + n.presses : ''}`, x: n.wave * 160, y: i * 70, symbolSize: 38, label: { show: true, color: '#e5e7eb' } }; });
  return { backgroundColor: 'transparent', tooltip: {}, series: [{ type: 'graph', layout: 'none', roam: 'move', edgeSymbol: ['none', 'arrow'], edgeSymbolSize: 10,
    data: nodes, links: graph.edges.map((e) => ({ source: e.from, target: e.to })), itemStyle: { color: '#3987e5' }, lineStyle: { color: '#9ca3af', width: 1.5 } }] };
}
const contentRowsHtml = (items) => items.map((it) => `<tr class="bg-white border-b dark:bg-gray-800 dark:border-gray-700">
      <td class="px-3 py-2"><input type="checkbox" value="${it.id}" ${/downloaded|installed/i.test(it.state) ? 'disabled' : ''} class="${UI.check}" aria-label="Select ${esc(it.name)}"></td>
      <td class="px-3 py-2 font-medium text-gray-900 dark:text-white">${esc(it.name)}</td>
      <td class="px-3 py-2">${esc(it.type)}</td><td class="px-3 py-2">${esc(it.state)}</td><td class="px-3 py-2 font-mono">${it.id}</td></tr>`).join('');

async function refreshStatus() {
  // The connection badge (#conn) is the shell's: it polls the same /api/status on every page.
  const s = await api('/api/status');
  $('endpoint').textContent = `${s.endpoint} · game 127.0.0.1:${s.game_port}`;
  $('auto').checked = !!s.auto_approve;
  $('pending').innerHTML = pendingHtml(s.pending);
  $('events').innerHTML = eventsHtml(s.events);
  return s;
}

async function refreshOverview() {
  const o = await api('/api/overview');
  if (!o.state || !o.state.data) return;
  const d = o.state.data;
  $('s-date').textContent = o.state.game_date ?? '-';
  $('s-tick').textContent = 'tick ' + money(o.state.game_tick);
  const fleet = fleetText(d);
  $('s-vehicles').textContent = fleet.total;
  $('s-fleet-types').textContent = fleet.types;
  $('s-companies').textContent = d.num_companies;
  $('s-map').textContent = d.num_towns;
  $('s-map-size').textContent = `map ${d.map_width}×${d.map_height}`;
  $('s-paused').textContent = d.paused ? 'paused' : 'running';
  $('companies').innerHTML = companiesHtml((o.companies.data && o.companies.data.companies) || []);
  $('decisions').innerHTML = decisionsHtml((o.decisions.data && o.decisions.data.decisions) || []);
}

async function loadCatalogue() {
  // The same component as /tools (catalogue.js), compact, its filters kept in the address as c_*.
  const [m, doc] = await Promise.all([import('/static/catalogue.js'), fetch('/openapi.json').then((r) => r.json())]);
  m.mountCatalogue($('catalogue-mount'), m.catalogueItems(doc), { compact: true, prefix: 'c_' });
}

async function tick() {
  try {
    const s = await refreshStatus();
    if (s.mcp_up) await refreshOverview();
  } catch (e) { /* the shell's #conn says the panel is unreachable */ }
}

document.addEventListener('click', async (ev) => {
  const t = ev.target.closest('[data-approve],[data-read]');
  if (!t) return;
  ev.preventDefault();
  if (t.dataset.approve) { await api('/api/approve', { id: Number(t.dataset.approve) }); tick(); }
  if (t.dataset.read) location.href = '/resource/' + t.dataset.read.replace(/^openttd:\/\//, '');
});
$('btn-pause').onclick = async () => { await api('/api/pause', { pause: true }); tick(); };
$('btn-resume').onclick = async () => { await api('/api/pause', { pause: false }); tick(); };
$('btn-ai').onclick = async () => { await api('/api/start-ai', {}); tick(); };
$('btn-catalogue').onclick = loadCatalogue;
$('auto').onchange = async (e) => { await api('/api/auto-approve', { on: e.target.checked }); tick(); };

/* ---- developer console (~) ---------------------------------------------------------------- */
const typed = []; let hpos = 0; // the console lines typed, for ArrowUp/ArrowDown (not window.history)
/* Every console line is classified and highlighted: errors, warnings, passes, JSON (pretty, keys and
   values coloured), and events by kind. Classification is by content, so a new kind needs no code. */
const highlightJson = (v) => esc(JSON.stringify(v, null, 2))
  .replace(/&quot;([^&]*?)&quot;(\s*:)/g, '<span class="text-sky-300">&quot;$1&quot;</span>$2')
  .replace(/: &quot;(.*?)&quot;/g, ': <span class="text-amber-200">&quot;$1&quot;</span>')
  .replace(/: (-?\d+(\.\d+)?)/g, ': <span class="text-purple-300">$1</span>')
  .replace(/: (true|false|null)/g, ': <span class="text-pink-300">$1</span>');
function lineClass(t) {
  if (/^> /.test(t)) return 'text-white font-semibold';
  if (/error|FAIL|mcp-rhai:|Runtime error|refused|denied/i.test(t)) return 'text-red-400';
  if (/warn|NOT RUN|PARTIAL|pending/i.test(t)) return 'text-yellow-300';
  if (/PASS|executed|succeeded|VERIFIED|started|built a bus route|route \d+\//.test(t)) return 'text-green-400';
  if (/^\[(ai|game|company|decision|vehicles|panel|console)\./.test(t)) return 'text-sky-300';
  if (/dbg: \[/.test(t)) return 'text-gray-500';
  return 'text-green-200';
}
/** One console output: JSON pretty and coloured, any other text line by line, each classified. */
function consoleHtml(s) {
  const text = String(s);
  const trimmed = text.trim();
  if ((trimmed.startsWith('{') || trimmed.startsWith('[')) && trimmed.length > 1) {
    try { return `<span class="block">${highlightJson(JSON.parse(trimmed))}</span>`; } catch { /* not JSON */ }
  }
  return text.split('\n').map((l) => `<span class="block ${lineClass(l)}">${esc(l)}</span>`).join('');
}
const out = (s) => {
  const o = $('console-out');
  o.insertAdjacentHTML('beforeend', consoleHtml(s));
  while (o.childElementCount > 2000) o.firstElementChild.remove();
  o.scrollTop = o.scrollHeight;
};
/* The drawer is display:flex when open (header, output that fills, input) and hidden when closed;
   while open, <main> keeps room below its last card (console-open) so nothing hides under it. */
const toggleConsole = (show) => {
  const c = $('console'); const on = show ?? c.classList.contains('hidden');
  c.classList.toggle('hidden', !on); c.classList.toggle('flex', on);
  document.body.classList.toggle('console-open', on);
  if (on) $('console-in').focus();
};
/* Expand: from a strip (h-80) to most of the screen (h-[85vh]) and back. */
const expandConsole = (big) => {
  const c = $('console'); const on = big ?? !c.classList.contains('h-[85vh]');
  c.classList.toggle('h-[85vh]', on); c.classList.toggle('h-80', !on);
  const b = $('expand-console'); if (b) { b.textContent = on ? 'Shrink' : 'Expand'; b.setAttribute('aria-pressed', String(on)); }
};
if ($('expand-console')) $('expand-console').onclick = () => expandConsole();
if ($('clear-console')) $('clear-console').onclick = () => { $('console-out').textContent = ''; $('console-in').focus(); };
$('open-console').onclick = () => toggleConsole(true);
$('side-console').onclick = () => toggleConsole(true);
if (location.hash === '#console') toggleConsole(true); // a link from another page opens the console
$('close-console').onclick = () => toggleConsole(false);
document.addEventListener('keydown', (e) => {
  if ((e.key === '`' || e.key === '~') && document.activeElement !== $('console-in')) { e.preventDefault(); toggleConsole(); }
  else if ((e.key === '`' || e.key === '~') && document.activeElement === $('console-in') && $('console-in').value === '') { e.preventDefault(); toggleConsole(false); }
});
$('console-in').addEventListener('keydown', (e) => {
  if (e.key === 'ArrowUp' && hpos > 0) { hpos -= 1; e.target.value = typed[hpos]; e.preventDefault(); }
  if (e.key === 'ArrowDown') { hpos = Math.min(typed.length, hpos + 1); e.target.value = typed[hpos] ?? ''; e.preventDefault(); }
});

const HELP = `OpenTTD console commands go straight to the server console (e.g. companies, list_ai, date, start_ai PrismaticAI).
/mcp <method> [json-params]   raw MCP JSON-RPC, e.g. /mcp tools/list   /mcp resources/read {"uri":"openttd://game/state"}
/read <uri>                   read a resource, e.g. /read openttd://companies
/pause  /resume               set_pause through the approval flow
/approve <id>                 operator approval of a pending decision
/auto on|off                  auto-approve (full auto)
/rhai <script>                run a Rhai script; every line it prints streams here as it is printed
/tail [kinds]                 stream the live event feed here (default ai.log,company,decision,game)
/untail                       stop it
/clear                        clear this console`;

/* /rhai: every line the script prints, as it prints it. */
async function rhaiStream(script) {
  const r = await fetch('/api/rhai/stream', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ script }) });
  const reader = r.body.getReader(); const dec = new TextDecoder(); let buf = '';
  for (;;) {
    const { done, value } = await reader.read(); if (done) break;
    buf += dec.decode(value, { stream: true });
    let i;
    while ((i = buf.indexOf('\n\n')) >= 0) {
      const block = buf.slice(0, i); buf = buf.slice(i + 2);
      const ev = (block.match(/^event: (.*)$/m) || [])[1]; const data = (block.match(/^data: (.*)$/m) || [])[1];
      if (ev === 'exit') { out(`rhai exit ${data}`); return; }
      if (data !== undefined) out(JSON.parse(data));
    }
  }
}
/* /tail [kinds]: the live event feed streamed into the console. */
const EVENT_NAMES = ['ai.log', 'console.line', 'company.founded', 'company.renamed', 'company.removed', 'company.observed', 'decision.recorded', 'decision.updated',
  'game.month', 'game.paused', 'game.resumed', 'game.new_world', 'game.observed', 'vehicles.count', 'panel.mcp', 'panel.operator', 'panel.console', 'panel.rhai', 'panel.ai'];
/** The event names to listen to for kinds "a,b": each kind, then the children of a kind
    (company.founded under company), which arrive under their own event names. */
const tailNames = (kinds) => {
  const asked = kinds.split(',');
  const child = (k) => asked.some((x) => k.startsWith(x + '.')) && !asked.includes(k);
  return [...asked, ...EVENT_NAMES.filter(child)];
};
let tailSource = null;
function tail(kinds) {
  if (tailSource) tailSource.close();
  tailSource = new EventSource('/api/events/stream?kinds=' + encodeURIComponent(kinds));
  tailSource.onmessage = null;
  const show = (m) => { const e = JSON.parse(m.data); out(`[${e.kind}] ${e.data.text || e.data.line || JSON.stringify(e.data)}`); };
  tailNames(kinds).forEach((k) => tailSource.addEventListener(k, show));
  out(`tailing ${kinds} (/untail to stop)`);
}

async function run(line) {
  out('> ' + line);
  const [head, ...rest] = line.split(' ');
  const arg = rest.join(' ');
  try {
    if (head === '/help') return out(HELP);
    if (head === '/clear') { $('console-out').textContent = ''; return; }
    if (head === '/mcp') {
      const [method, ...p] = arg.split(' ');
      const params = p.length ? JSON.parse(p.join(' ')) : undefined;
      return out(JSON.stringify(await api('/api/rpc', { method, params }), null, 2));
    }
    if (head === '/read') return out(JSON.stringify(await api('/api/resource?uri=' + encodeURIComponent(arg)), null, 2));
    if (head === '/pause' || head === '/resume') return out(JSON.stringify(await api('/api/pause', { pause: head === '/pause' })));
    if (head === '/approve') return out(JSON.stringify(await api('/api/approve', { id: Number(arg) })));
    if (head === '/auto') return out(JSON.stringify(await api('/api/auto-approve', { on: arg === 'on' })));
    if (head === '/rhai') return rhaiStream(arg);
    if (head === '/tail') return tail(arg || 'ai.log,company,decision,game');
    if (head === '/untail') { if (tailSource) tailSource.close(); tailSource = null; return out('tail stopped'); }
    const r = await api('/api/console', { command: line });
    out((r.output || []).join('\n') || JSON.stringify(r));
  } catch (e) { out('error: ' + e.message); }
}
$('console-form').addEventListener('submit', async (e) => {
  e.preventDefault();
  const line = $('console-in').value.trim(); if (!line) return;
  typed.push(line); hpos = typed.length; $('console-in').value = '';
  await run(line); tick();
});

out('OpenTTD MCP developer console. /help for commands. Press ~ to toggle.');
tick(); loadCatalogue().catch(() => {});
setInterval(tick, 1000);

/* ---- my bot: static/bots.js (a module) ------------------------------------------------- */
setInterval(() => { if (!document.querySelector('#catalogue-mount [data-cards]')) loadCatalogue().catch(() => {}); }, 3000);


/* ---- live notifications (SSE) ------------------------------------------------------------ */
// What the bot and the game do, as it happens: the same feed /api/events/stream serves to anyone.
const NOTABLE = (e) => {
  const d = e.data || {};
  if (e.kind === 'ai.log') {
    const t = d.text || '';
    return /built a bus route|: route |behaviour:|no new route/.test(t) || /error/.test(d.level) ? t.replace(/^PrismaticAI:? /, '') : null;
  }
  if (e.kind === 'company.founded') return `Company #${d.id} founded`;
  if (e.kind === 'company.renamed') return `Company #${d.id} is now ${d.to}`;
  if (e.kind === 'decision.recorded') return `Decision #${d.decision_id}: ${d.action_type} ${d.command_result}`;
  if (e.kind === 'game.paused') return `Paused at ${d.date}`;
  if (e.kind === 'game.resumed') return `Resumed at ${d.date}`;
  if (e.kind === 'game.month') return `${d.date}: ${d.vehicles} vehicles`;
  if (e.kind === 'game.new_world') return `New world, ${d.towns} towns`;
  if (e.kind === 'panel.mcp' && /pending approval/.test(d.text || '')) return `Approval needed: ${d.text}`;
  return null;
};
/** A notification's body: its kind, its text and a close button. */
const toastHtml = (kind, text) => `<div class="ms-1 text-sm font-normal"><span class="mb-1 text-xs font-semibold text-gray-900 dark:text-white block">${esc(kind)}</span>${esc(text)}</div>
    <button type="button" class="ms-auto -mx-1.5 -my-1.5 bg-white text-gray-400 hover:text-gray-900 rounded-lg focus:ring-2 focus:ring-gray-300 p-1.5 hover:bg-gray-100 inline-flex items-center justify-center h-8 w-8 dark:text-gray-500 dark:hover:text-white dark:bg-gray-800 dark:hover:bg-gray-700" aria-label="Close">&times;</button>`;
function toast(kind, text) {
  const el = document.createElement('div');
  el.className = `${UI.card} flex items-start w-full text-gray-500 dark:text-gray-400`;
  el.setAttribute('role', 'alert');
  el.innerHTML = toastHtml(kind, text);
  el.querySelector('button').onclick = () => el.remove();
  $('toasts').prepend(el);
  setTimeout(() => el.remove(), kind.startsWith('panel') ? 30000 : 8000);
  while ($('toasts').children.length > 5) $('toasts').lastChild.remove();
}
if (window.EventSource) {
  const es = new EventSource('/api/events/stream?kinds=ai.log,company,decision,game,panel.mcp');
  es.onmessage = null;
  ['ai.log', 'company.founded', 'company.renamed', 'decision.recorded', 'game.paused', 'game.resumed', 'game.month', 'game.new_world', 'panel.mcp']
    .forEach((k) => es.addEventListener(k, (m) => { const e = JSON.parse(m.data); const t = NOTABLE(e); if (t) toast(e.kind, t); }));
}

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

/* ---- cluster: every registered instance, probed live ------------------------------------------ */
(function cluster() {
  const body = document.getElementById('cluster-body'); if (!body) return;
  async function load() {
    try {
      const d = await (await fetch('/api/instances')).json();
      document.getElementById('cluster-reg').textContent = `registry ${d.registry} · ${d.instances.length} instance(s)`;
      body.innerHTML = clusterHtml(d.instances);
    } catch (e) { body.innerHTML = `<tr><td class="px-4 py-2" colspan="5">${esc(e.message)}</td></tr>`; }
  }
  load(); setInterval(load, 5000);
})();

/* ---- news in the owner's game window: read it, and post to it ------------------------------- */
(function news() {
  const list = document.getElementById('news-list'); if (!list) return;
  const msg = (t) => { document.getElementById('news-msg').textContent = t; };
  const post = async (path, body) => (await fetch(path, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body || {}) })).json();
  async function load() {
    try {
      const d = await (await fetch('/api/news?limit=15')).json();
      list.innerHTML = newsHtml(d.news || []);
    } catch (e) { msg('no GUI client attached yet'); }
  }
  async function towns() {
    try {
      const d = await (await fetch('/api/resources/towns')).json();
      const sel = document.getElementById('news-town');
      sel.innerHTML = townOptions(d.data.towns);
    } catch (e) { /* the game is still starting */ }
  }
  document.getElementById('news-form').addEventListener('submit', async (e) => {
    e.preventDefault();
    const body = { text: document.getElementById('news-text').value };
    const town = document.getElementById('news-town').value; const style = document.getElementById('news-style').value;
    if (town) body.town = +town;
    if (style) body.style = style;
    const r = await post('/api/news', body);
    msg(r.ok ? `shown in your game: ${r.shown}` : `refused: ${r.error}`);
    if (r.ok) { document.getElementById('news-text').value = ''; setTimeout(load, 800); }
  });
  document.getElementById('news-close').addEventListener('click', async () => { await post('/api/news/close'); msg('news window closed'); });
  async function asks() {
    try {
      const d = await (await fetch('/api/ask')).json();
      document.getElementById('ask-list').innerHTML = asksHtml(d.asks || []);
    } catch (e) { /* no GUI client yet */ }
  }
  load(); towns(); asks(); setInterval(load, 4000); setInterval(asks, 3000); setInterval(towns, 60000);
})();

/* ---- plan before acting: AlgoGlyph action-plan, drawn as a DAG ------------------------------ */
(function actionPlan() {
  const box = document.getElementById('plan-graph'); if (!box || !window.echarts) return;
  let chart = null;
  // Through the one door (components.js ecInit, published by overview.js).
  const ec = () => { if (!chart && globalThis.ecInit) chart = globalThis.ecInit(box, echarts); return chart; };
  document.getElementById('plan-run').onclick = async () => {
    let actions;
    try { actions = JSON.parse(document.getElementById('plan-actions').value); } catch (e) { document.getElementById('plan-msg').textContent = 'actions are not JSON: ' + e.message; return; }
    const r = await fetch('/api/actionplan', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ actions }) });
    const d = await r.json();
    if (!d.ok) { document.getElementById('plan-msg').textContent = 'refused: ' + JSON.stringify(d.errors || d.error); document.getElementById('plan-steps').innerHTML = ''; return; }
    const res = d.result;
    document.getElementById('plan-msg').textContent = planSummary(res);
    document.getElementById('plan-steps').innerHTML = planStepsHtml(res.steps);
    const c = ec(); if (!c) return;
    c.setOption(planGraphOption(res.graph), true);
  };
})();

// Content (BaNaNaS): search the content server through the game, tick, download (the owner's click).
(() => {
  const form = $('content-form'); if (!form) return;
  const rows = $('content-rows'), dl = $('content-download'), msg = $('content-msg');
  const ticked = () => [...rows.querySelectorAll('input[type=checkbox]:checked')].map((b) => Number(b.value));
  const sync = () => { dl.disabled = ticked().length === 0; };
  form.onsubmit = async (ev) => {
    ev.preventDefault();
    msg.textContent = 'Asking the content server through the game...';
    const r = await api('/api/content?q=' + encodeURIComponent($('content-q').value));
    if (!r.items) { msg.textContent = 'Not available: ' + (r.error || r.text || r.status); return; }
    rows.innerHTML = contentRowsHtml(r.items);
    rows.querySelectorAll('input[type=checkbox]').forEach((b) => { b.onchange = sync; });
    sync();
    msg.textContent = `${r.count} item(s) for "${r.query}".`;
  };
  dl.onclick = async () => {
    const ids = ticked(); if (!ids.length) return;
    dl.disabled = true;
    msg.textContent = `Downloading ${ids.length} item(s) and the libraries they need...`;
    const r = await api('/api/content/download', { ids });
    const lines = (r.output || []).filter((l) => !/heartbeat/.test(l)).slice(-6);
    msg.innerHTML = r.ids ? `Done: ${esc(lines.join(' | ') || 'downloaded')}. Search again to see the new state.` : 'Refused: ' + esc(r.error || r.text || r.status);
  };
})();

if (typeof module !== 'undefined') {
  module.exports = {
    money, gameText, pendingHtml, eventsHtml, fleetText, companiesHtml, decisionsHtml, clusterHtml, newsHtml,
    townOptions, asksHtml, planSummary, planStepsHtml, planGraphOption, contentRowsHtml, highlightJson,
    lineClass, consoleHtml, tailNames, NOTABLE, toastHtml, HELP,
  };
}
