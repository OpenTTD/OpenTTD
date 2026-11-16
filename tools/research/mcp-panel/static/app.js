'use strict';

const $ = (id) => document.getElementById(id);
const esc = (s) => String(s).replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));

async function api(path, body) {
  const opts = body === undefined ? {} : {
    method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body),
  };
  const r = await fetch(path, opts);
  const text = await r.text();
  try { return JSON.parse(text); } catch { return { status: r.status, text }; }
}

const money = (v) => (typeof v === 'number' ? v.toLocaleString('en') : v ?? '-');

async function refreshStatus() {
  const s = await api('/api/status');
  $('conn').textContent = s.mcp_up ? 'MCP up' : (s.server_running ? 'server starting' : 'server down');
  $('conn').className = 'text-xs font-medium px-2.5 py-0.5 rounded-sm ' + (s.mcp_up
    ? 'bg-green-100 text-green-800 dark:bg-green-900 dark:text-green-300'
    : 'bg-red-100 text-red-800 dark:bg-red-900 dark:text-red-300');
  $('endpoint').textContent = `${s.endpoint} · game 127.0.0.1:${s.game_port}`;
  $('auto').checked = !!s.auto_approve;
  $('pending').innerHTML = s.pending.length ? s.pending.map((p) => `
    <li class="flex items-center justify-between p-2 border border-gray-200 rounded-lg dark:border-gray-700">
      <span class="font-mono">#${p.id} ${esc(p.tool)}(${esc(JSON.stringify(p.args))})</span>
      <button data-approve="${p.id}" type="button" class="text-white bg-green-700 hover:bg-green-800 font-medium rounded-lg text-xs px-3 py-1.5 dark:bg-green-600 dark:hover:bg-green-700">Approve</button>
    </li>`).join('') : '<li>none</li>';
  $('events').innerHTML = s.events.map((e) => `
    <li class="mb-3 ms-4"><div class="absolute w-3 h-3 bg-gray-200 rounded-full mt-1.5 -start-1.5 border border-white dark:border-gray-900 dark:bg-gray-700"></div>
      <time class="text-xs font-normal leading-none text-gray-400 dark:text-gray-500">${esc(e.t)} · ${esc(e.kind)}</time>
      <p class="text-sm text-gray-700 dark:text-gray-300">${esc(e.text)}</p></li>`).join('');
  return s;
}

async function refreshOverview() {
  const o = await api('/api/overview');
  if (!o.state || !o.state.data) return;
  const d = o.state.data;
  $('s-date').textContent = o.state.game_date ?? '-';
  $('s-tick').textContent = money(o.state.game_tick);
  $('s-vehicles').textContent = d.num_vehicles;
  $('s-companies').textContent = d.num_companies;
  $('s-map').textContent = `${d.num_towns} · ${d.map_width}x${d.map_height}`;
  $('s-paused').textContent = d.paused ? 'paused' : 'running';
  const cs = (o.companies.data && o.companies.data.companies) || [];
  $('companies').innerHTML = cs.length ? cs.map((c) => `
    <tr class="border-b dark:border-gray-700"><td class="px-4 py-2">${esc(c.id)}</td>
      <td class="px-4 py-2 font-medium text-gray-900 dark:text-white">${esc(c.name ?? '')}</td>
      <td class="px-4 py-2">${c.is_ai ? '<span class="bg-purple-100 text-purple-800 text-xs font-medium px-2 py-0.5 rounded-sm dark:bg-purple-900 dark:text-purple-300">AI</span>' : 'human'}</td>
      <td class="px-4 py-2">${money(c.money)}</td><td class="px-4 py-2">${money(c.company_value ?? c.value)}</td>
      <td class="px-4 py-2">${esc(c.inaugurated_year ?? '')}</td></tr>`).join('')
    : '<tr><td class="px-4 py-3" colspan="6">no company yet (the AI starts one)</td></tr>';
  const ds = (o.decisions.data && o.decisions.data.decisions) || [];
  $('decisions').innerHTML = ds.length ? ds.slice().reverse().map((x) => `
    <tr class="border-b dark:border-gray-700"><td class="px-4 py-2">${esc(x.decision_id)}</td><td class="px-4 py-2">${esc(x.action_type)}</td>
      <td class="px-4 py-2">#${esc(x.approval_id)}</td><td class="px-4 py-2">${esc(x.command_result)}</td>
      <td class="px-4 py-2">${esc(x.verdict)}</td><td class="px-4 py-2">${esc(x.observed_tick)}</td></tr>`).join('')
    : '<tr><td class="px-4 py-3" colspan="6">none yet</td></tr>';
}

async function loadCatalogue() {
  const c = await api('/api/catalogue');
  const res = (c.resources && c.resources.result && c.resources.result.resources) || [];
  $('resources').innerHTML = res.map((r) => `<li><a href="#" data-read="${esc(r.uri)}" class="text-primary-600 hover:underline dark:text-primary-500">${esc(r.uri)}</a></li>`).join('');
  const tools = (c.tools && c.tools.result && c.tools.result.tools) || [];
  $('tools').innerHTML = tools.map((t) => `<li title="${esc(t.description || '')}">${esc(t.name)}</li>`).join('');
}

async function tick() {
  try {
    const s = await refreshStatus();
    if (s.mcp_up) await refreshOverview();
  } catch (e) {
    $('conn').textContent = 'panel unreachable';
  }
}

document.addEventListener('click', async (ev) => {
  const t = ev.target.closest('[data-approve],[data-read]');
  if (!t) return;
  ev.preventDefault();
  if (t.dataset.approve) { await api('/api/approve', { id: Number(t.dataset.approve) }); tick(); }
  if (t.dataset.read) {
    const r = await api('/api/resource?uri=' + encodeURIComponent(t.dataset.read));
    $('resource-view').classList.remove('hidden');
    $('resource-view').textContent = JSON.stringify(r, null, 2);
  }
});
$('btn-pause').onclick = async () => { await api('/api/pause', { pause: true }); tick(); };
$('btn-resume').onclick = async () => { await api('/api/pause', { pause: false }); tick(); };
$('btn-ai').onclick = async () => { await api('/api/start-ai', {}); tick(); };
$('btn-catalogue').onclick = loadCatalogue;
$('auto').onchange = async (e) => { await api('/api/auto-approve', { on: e.target.checked }); tick(); };

/* ---- developer console (~) ---------------------------------------------------------------- */
const history = []; let hpos = 0;
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
const out = (s) => {
  const o = $('console-out');
  const text = String(s);
  let html;
  const trimmed = text.trim();
  if ((trimmed.startsWith('{') || trimmed.startsWith('[')) && trimmed.length > 1) {
    try { html = `<span class="block">${highlightJson(JSON.parse(trimmed))}</span>`; } catch { html = null; }
  }
  if (!html) html = text.split('\n').map((l) => `<span class="block ${lineClass(l)}">${esc(l)}</span>`).join('');
  o.insertAdjacentHTML('beforeend', html);
  while (o.childElementCount > 2000) o.firstElementChild.remove();
  o.scrollTop = o.scrollHeight;
};
const toggleConsole = (show) => {
  const c = $('console'); const on = show ?? c.classList.contains('hidden');
  c.classList.toggle('hidden', !on); if (on) $('console-in').focus();
};
$('open-console').onclick = () => toggleConsole(true);
$('side-console').onclick = () => toggleConsole(true);
if (location.hash === '#console') toggleConsole(true); // a link from another page opens the console
$('close-console').onclick = () => toggleConsole(false);
document.addEventListener('keydown', (e) => {
  if ((e.key === '`' || e.key === '~') && document.activeElement !== $('console-in')) { e.preventDefault(); toggleConsole(); }
  else if ((e.key === '`' || e.key === '~') && document.activeElement === $('console-in') && $('console-in').value === '') { e.preventDefault(); toggleConsole(false); }
});
$('console-in').addEventListener('keydown', (e) => {
  if (e.key === 'ArrowUp' && hpos > 0) { hpos -= 1; e.target.value = history[hpos]; e.preventDefault(); }
  if (e.key === 'ArrowDown') { hpos = Math.min(history.length, hpos + 1); e.target.value = history[hpos] ?? ''; e.preventDefault(); }
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
let tailSource = null;
function tail(kinds) {
  if (tailSource) tailSource.close();
  tailSource = new EventSource('/api/events/stream?kinds=' + encodeURIComponent(kinds));
  tailSource.onmessage = null;
  const show = (m) => { const e = JSON.parse(m.data); out(`[${e.kind}] ${e.data.text || e.data.line || JSON.stringify(e.data)}`); };
  kinds.split(',').forEach((k) => tailSource.addEventListener(k, show));
  // Children of a kind (company.founded under company) arrive under their own event names.
  ['ai.log', 'console.line', 'company.founded', 'company.renamed', 'company.removed', 'company.observed', 'decision.recorded', 'decision.updated',
    'game.month', 'game.paused', 'game.resumed', 'game.new_world', 'game.observed', 'vehicles.count', 'panel.mcp', 'panel.operator', 'panel.console', 'panel.rhai', 'panel.ai']
    .filter((k) => kinds.split(',').some((x) => k === x || k.startsWith(x + '.')) && !kinds.split(',').includes(k))
    .forEach((k) => tailSource.addEventListener(k, show));
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
  history.push(line); hpos = history.length; $('console-in').value = '';
  await run(line); tick();
});

out('OpenTTD MCP developer console. /help for commands. Press ~ to toggle.');
tick(); loadCatalogue().catch(() => {});
setInterval(tick, 1000);

/* ---- my bot ----------------------------------------------------------------------------- */
let bots = null;
const input = 'bg-gray-50 border border-gray-300 text-gray-900 text-sm rounded-lg focus:ring-primary-500 focus:border-primary-500 block w-full p-2 dark:bg-gray-700 dark:border-gray-600 dark:text-white';
function renderBotSettings() {
  const ai = (bots.ais || []).find((a) => a.ai === $('bot-ai').value);
  $('bot-settings').innerHTML = ai ? ai.settings.map((st) => `
    <div><label for="set-${esc(st.name)}" class="block mb-1 text-sm font-medium text-gray-900 dark:text-white">${esc(st.name)}${st.ingame ? ' <span class="bg-green-100 text-green-800 text-xs font-medium px-1.5 py-0.5 rounded-sm dark:bg-green-900 dark:text-green-300">live</span>' : ''}</label>
      <input id="set-${esc(st.name)}" data-setting="${esc(st.name)}" type="number" min="${st.min}" max="${st.max}" step="${st.step}" value="${st.default}" class="${input}">
      <p class="mt-1 text-xs text-gray-500 dark:text-gray-400">${esc(st.description || '')} [${st.min}..${st.max}]</p></div>`).join('') : '';
}
async function loadBots() {
  bots = await api('/api/bots');
  if (!bots || bots.error) return;
  $('bot-source').textContent = bots.source;
  const keep = $('bot-ai').value;
  $('bot-ai').innerHTML = (bots.ais || []).map((a) => `<option>${esc(a.ai)}</option>`).join('');
  if (keep) $('bot-ai').value = keep;
  renderBotSettings();
  const ingame = ((bots.ais || []).find((a) => a.ai === $('bot-ai').value) || { settings: [] }).settings.filter((x) => x.ingame);
  $('bot-running').innerHTML = (bots.running || []).length ? bots.running.map((c) => `
    <tr class="border-b dark:border-gray-700"><td class="px-4 py-2">${esc(c.id)}</td><td class="px-4 py-2 font-medium text-gray-900 dark:text-white">${esc(c.name || '')}</td>
      <td class="px-4 py-2">${money(c.money)}</td>
      <td class="px-4 py-2"><div class="flex flex-wrap gap-2">${ingame.map((st) => `
        <button type="button" data-company="${c.id}" data-live="${esc(st.name)}" class="text-gray-900 bg-white border border-gray-300 hover:bg-gray-100 font-medium rounded-lg text-xs px-2 py-1 dark:bg-gray-800 dark:text-white dark:border-gray-600 dark:hover:bg-gray-700">set ${esc(st.name)}</button>`).join('')}</div></td></tr>`).join('')
    : '<tr><td class="px-4 py-2" colspan="4">none</td></tr>';
}
$('bot-ai').onchange = renderBotSettings;
$('bot-start').onclick = async () => {
  const settings = {};
  document.querySelectorAll('[data-setting]').forEach((el) => { settings[el.dataset.setting] = Number(el.value); });
  const r = await api('/api/bots/start', { ai: $('bot-ai').value, settings });
  $('bot-msg').textContent = JSON.stringify(r);
  setTimeout(loadBots, 3000);
};
document.addEventListener('click', async (ev) => {
  const t = ev.target.closest('[data-live]'); if (!t) return;
  const el = document.querySelector(`[data-setting="${t.dataset.live}"]`);
  const r = await api('/api/bots/setting', { company: Number(t.dataset.company), name: t.dataset.live, value: Number(el ? el.value : 0) });
  $('bot-msg').textContent = JSON.stringify(r);
  tick();
});
loadBots().catch(() => {});
setInterval(() => loadBots().catch(() => {}), 10000);
setInterval(() => { if (!$('resources').children.length) loadCatalogue().catch(() => {}); }, 3000);


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
function toast(kind, text) {
  const el = document.createElement('div');
  el.className = 'flex items-start w-full p-4 text-gray-500 bg-white rounded-lg shadow-sm dark:text-gray-400 dark:bg-gray-800 border border-gray-200 dark:border-gray-700';
  el.setAttribute('role', 'alert');
  el.innerHTML = `<div class="ms-1 text-sm font-normal"><span class="mb-1 text-xs font-semibold text-gray-900 dark:text-white block">${esc(kind)}</span>${esc(text)}</div>
    <button type="button" class="ms-auto -mx-1.5 -my-1.5 bg-white text-gray-400 hover:text-gray-900 rounded-lg focus:ring-2 focus:ring-gray-300 p-1.5 hover:bg-gray-100 inline-flex items-center justify-center h-8 w-8 dark:text-gray-500 dark:hover:text-white dark:bg-gray-800 dark:hover:bg-gray-700" aria-label="Close">&times;</button>`;
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
  const esc = (v) => String(v ?? '').replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
  const badge = (ok) => ok
    ? '<span class="bg-green-100 text-green-800 text-xs font-medium px-2.5 py-0.5 rounded-sm dark:bg-green-900 dark:text-green-300">alive</span>'
    : '<span class="bg-red-100 text-red-800 text-xs font-medium px-2.5 py-0.5 rounded-sm dark:bg-red-900 dark:text-red-300">down</span>';
  async function load() {
    try {
      const d = await (await fetch('/api/instances')).json();
      document.getElementById('cluster-reg').textContent = `registry ${d.registry} · ${d.instances.length} instance(s)`;
      body.innerHTML = d.instances.map((i) => {
        const panel = i.url || i.panel;
        return `<tr class="bg-white border-b dark:bg-gray-800 dark:border-gray-700"><td class="px-4 py-2 font-medium text-gray-900 dark:text-white">${esc(i.name)}</td><td class="px-4 py-2">${esc(i.role)}</td><td class="px-4 py-2">${badge(i.alive)}${i.error ? ` <span class="text-xs">${esc(i.error)}</span>` : ''}</td><td class="px-4 py-2"><code>${esc(i.endpoint || i.mcp)}</code></td><td class="px-4 py-2">${panel ? `<a class="font-medium text-blue-600 dark:text-blue-500 hover:underline" href="${esc(panel)}">${esc(panel)}</a>` : ''}</td></tr>`;
      }).join('') || '<tr><td class="px-4 py-2" colspan="5">no instance registered</td></tr>';
    } catch (e) { body.innerHTML = `<tr><td class="px-4 py-2" colspan="5">${esc(e.message)}</td></tr>`; }
  }
  load(); setInterval(load, 5000);
})();

/* ---- news in the owner's game window: read it, and post to it ------------------------------- */
(function news() {
  const list = document.getElementById('news-list'); if (!list) return;
  const esc = (v) => String(v ?? '').replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
  const msg = (t) => { document.getElementById('news-msg').textContent = t; };
  const post = async (path, body) => (await fetch(path, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body || {}) })).json();
  async function load() {
    try {
      const d = await (await fetch('/api/news?limit=15')).json();
      list.innerHTML = (d.news || []).map((n) => `<li class="mb-3 ms-4"><div class="absolute w-3 h-3 bg-gray-200 rounded-full mt-1.5 -start-1.5 border border-white dark:border-gray-900 dark:bg-gray-700"></div><time class="mb-1 text-xs font-normal leading-none text-gray-400 dark:text-gray-500">${esc(n.date)} · ${esc(n.type)}</time><p class="text-sm text-gray-900 dark:text-white">${esc(n.text)}</p></li>`).join('') || '<li class="ms-4 text-sm">no news yet</li>';
    } catch (e) { msg('no GUI client attached yet'); }
  }
  async function towns() {
    try {
      const d = await (await fetch('/api/resources/towns')).json();
      const sel = document.getElementById('news-town');
      sel.innerHTML = '<option value="">No viewport</option>' + d.data.towns.sort((a, b) => a.name.localeCompare(b.name)).map((t) => `<option value="${t.id}">${esc(t.name)}</option>`).join('');
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
      const tag = (a) => a === 'yes' ? '<span class="bg-green-100 text-green-800 text-xs font-medium px-2.5 py-0.5 rounded-sm dark:bg-green-900 dark:text-green-300">yes</span>'
        : a === 'replaced' ? '<span class="bg-gray-100 text-gray-800 text-xs font-medium px-2.5 py-0.5 rounded-sm dark:bg-gray-700 dark:text-gray-300">replaced</span>'
          : a === 'no' ? '<span class="bg-red-100 text-red-800 text-xs font-medium px-2.5 py-0.5 rounded-sm dark:bg-red-900 dark:text-red-300">no</span>'
          : '<span class="bg-yellow-100 text-yellow-800 text-xs font-medium px-2.5 py-0.5 rounded-sm dark:bg-yellow-900 dark:text-yellow-300">waiting for you</span>';
      document.getElementById('ask-list').innerHTML = (d.asks || []).slice(0, 10).map((a) => `<li class="flex items-start gap-2"><span>${tag(a.answer)}</span><span class="text-gray-900 dark:text-white">#${a.id} ${esc(a.question)}</span></li>`).join('') || '<li>no question asked yet</li>';
    } catch (e) { /* no GUI client yet */ }
  }
  load(); towns(); asks(); setInterval(load, 4000); setInterval(asks, 3000); setInterval(towns, 60000);
})();
