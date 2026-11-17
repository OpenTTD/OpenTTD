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
  // The same component as /tools (catalogue.js), compact, its filters kept in the address as c_*.
  const [m, doc] = await Promise.all([import('/static/catalogue.js'), fetch('/openapi.json').then((r) => r.json())]);
  m.mountCatalogue($('catalogue-mount'), m.catalogueItems(doc), { compact: true, prefix: 'c_' });
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
  typed.push(line); hpos = typed.length; $('console-in').value = '';
  await run(line); tick();
});

out('OpenTTD MCP developer console. /help for commands. Press ~ to toggle.');
tick(); loadCatalogue().catch(() => {});
setInterval(tick, 1000);

/* ---- my bot ----------------------------------------------------------------------------- */
// One control per setting, chosen from what the setting declares, never listed by hand: a boolean
// flag is a toggle, labels are a select, a description naming a town, industry or cargo id is a
// searchable picker over the live world, and any other range is a slider paired with a number box.
let bots = null;
let choices = { town: [], industry: [], cargo: [] };
/** Changes asked of running bots, keyed company:name -> {from, to, state, approval}. */
const asked = new Map();
const input = 'bg-gray-50 border border-gray-300 text-gray-900 text-sm rounded-lg focus:ring-primary-500 focus:border-primary-500 block w-full p-2 dark:bg-gray-700 dark:border-gray-600 dark:text-white';
const badge = (tone, text) => `<span class="bg-${tone}-100 text-${tone}-800 text-xs font-medium px-1.5 py-0.5 rounded-sm dark:bg-${tone}-900 dark:text-${tone}-300">${esc(text)}</span>`;
const pickerOf = (st) => { const m = /\((town|industry|cargo) id/.exec(st.description || ''); return m ? m[1] : null; };
const kindOf = (st) => ((st.flags || []).includes('boolean') ? 'toggle'
  : Object.keys(st.labels || {}).length ? 'select' : pickerOf(st) && (choices[pickerOf(st)] || []).length ? 'picker'
    : (st.max - st.min) / (st.step || 1) <= 2000 ? 'range' : 'number');
const optionLabel = (kind, o) => (kind === 'town' ? `${o.name} (#${o.id}, ${Number(o.population || 0).toLocaleString('en')} people)`
  : kind === 'industry' ? `${o.name} (#${o.id}${o.produces && o.produces.length ? ', makes ' + o.produces.join('/') : ''})` : `${o.name} (#${o.id})`);

/** The markup of one setting's control; `key` makes its ids unique, `value` is what it shows. */
function control(st, key, value, disabled) {
  const id = `set-${key}-${st.name}`.replace(/[^a-zA-Z0-9_-]/g, '_');
  const kind = kindOf(st);
  const dis = disabled ? 'disabled' : '';
  const attrs = `data-setting="${esc(st.name)}" data-kind="${kind}" data-key="${esc(key)}"`;
  let body;
  if (kind === 'toggle') {
    body = `<label class="inline-flex items-center cursor-pointer"><input id="${id}" type="checkbox" class="sr-only peer" ${Number(value) ? 'checked' : ''} ${dis} ${attrs}>
      <div class="relative w-11 h-6 bg-gray-200 peer-focus:outline-none peer-focus:ring-4 peer-focus:ring-primary-300 dark:peer-focus:ring-primary-800 rounded-full peer dark:bg-gray-700 peer-checked:after:translate-x-full rtl:peer-checked:after:-translate-x-full peer-checked:after:border-white after:content-[''] after:absolute after:top-[2px] after:start-[2px] after:bg-white after:border-gray-300 after:border after:rounded-full after:h-5 after:w-5 after:transition-all dark:border-gray-600 peer-checked:bg-primary-600"></div>
      <span class="ms-3 text-sm text-gray-900 dark:text-gray-300">${Number(value) ? 'on' : 'off'}</span></label>`;
  } else if (kind === 'select') {
    body = `<select id="${id}" class="${input}" ${dis} ${attrs}>${Object.entries(st.labels).map(([v, l]) => `<option value="${esc(v)}" ${Number(v) === Number(value) ? 'selected' : ''}>${esc(l)}</option>`).join('')}</select>`;
  } else if (kind === 'picker') {
    const pk = pickerOf(st);
    const opts = choices[pk] || [];
    const cur = opts.find((o) => o.id === Number(value));
    body = `<input id="${id}" list="${id}-list" class="${input}" placeholder="search ${pk}s… (-1: none)" value="${Number(value) < 0 ? '' : esc(cur ? optionLabel(pk, cur) : '#' + value)}" ${dis} ${attrs} data-picker="${pk}" autocomplete="off">
      <datalist id="${id}-list">${opts.map((o) => `<option value="${esc(optionLabel(pk, o))}"></option>`).join('')}</datalist>`;
  } else if (kind === 'range') {
    body = `<div class="flex items-center gap-3"><input id="${id}" type="range" min="${st.min}" max="${st.max}" step="${st.step || 1}" value="${value}" class="w-full h-2 bg-gray-200 rounded-lg appearance-none cursor-pointer dark:bg-gray-700 accent-primary-600" ${dis} ${attrs} data-pair="${id}-n">
      <input id="${id}-n" type="number" min="${st.min}" max="${st.max}" step="${st.step || 1}" value="${value}" class="${input} w-28" ${dis} data-pair="${id}" aria-label="${esc(st.name)} value"></div>`;
  } else {
    body = `<div class="relative flex items-center max-w-[12rem]"><button type="button" data-step="-1" data-for="${id}" class="bg-gray-100 dark:bg-gray-700 dark:hover:bg-gray-600 dark:border-gray-600 hover:bg-gray-200 border border-gray-300 rounded-s-lg p-3 h-10" ${dis} aria-label="decrease">−</button>
      <input id="${id}" type="number" min="${st.min}" max="${st.max}" step="${st.step || 1}" value="${value}" class="bg-gray-50 border-x-0 border-gray-300 h-10 text-center text-gray-900 text-sm block w-full py-2.5 dark:bg-gray-700 dark:border-gray-600 dark:text-white" ${dis} ${attrs}>
      <button type="button" data-step="1" data-for="${id}" class="bg-gray-100 dark:bg-gray-700 dark:hover:bg-gray-600 dark:border-gray-600 hover:bg-gray-200 border border-gray-300 rounded-e-lg p-3 h-10" ${dis} aria-label="increase">+</button></div>`;
  }
  return `<div><label for="${id}" class="block mb-1 text-sm font-medium text-gray-900 dark:text-white">${esc(st.name)} ${st.ingame ? badge('green', 'live') : ''}</label>${body}
    <p class="mt-1 text-xs text-gray-500 dark:text-gray-400">${esc(st.description || '')} [${st.min}..${st.max}]</p></div>`;
}

/** The value a control holds now: an integer, or null when a picker's text names nothing it knows. */
function readControl(el) {
  if (el.dataset.kind === 'toggle') return el.checked ? 1 : 0;
  if (el.dataset.kind === 'picker') {
    const t = el.value.trim();
    if (!t || t === '-1') return -1;
    const m = /#(-?\d+)/.exec(t);
    return m ? Number(m[1]) : null;
  }
  return Number(el.value);
}

function renderBotSettings() {
  const ai = (bots.ais || []).find((a) => a.ai === $('bot-ai').value);
  $('bot-settings').innerHTML = ai ? ai.settings.map((st) => control(st, 'new', st.default, false)).join('') : '';
}

/** A change asked of a running bot, as it stands: asked, waiting for approval, applied (the game
 *  reads the new value back), or refused. Never shown as applied before the game says so. */
function changeState(c, st) {
  const a = asked.get(`${c.id}:${st.name}`);
  if (!a) return '';
  if (a.state === 'pending' && Number(st.value) === a.to) a.state = 'applied';
  const tone = { applied: 'green', pending: 'yellow', refused: 'red', sent: 'blue' }[a.state] || 'gray';
  const text = { applied: `applied: ${a.from} → ${a.to}`, pending: `waiting for approval #${a.approval}: ${a.from} → ${a.to}`, refused: `refused: ${a.reason}`, sent: 'sent…' }[a.state];
  const undo = a.state === 'applied' ? ` <button type="button" data-undo="${c.id}:${esc(st.name)}" class="ms-1 text-xs text-primary-700 dark:text-primary-400 hover:underline">undo</button>` : '';
  return `<div class="mt-1">${badge(tone, text)}${undo}</div>`;
}

function renderRunning() {
  const rows = (bots.running || []).map((c) => {
    const live = (c.settings || []).filter((st) => st.ingame);
    const editors = live.length ? `<div class="grid gap-3 md:grid-cols-2 xl:grid-cols-3">${live.map((st) => `<div>${control(st, 'c' + c.id, st.value, !st.editable || !bots.live.set_setting)}
        <button type="button" data-apply="${c.id}" data-name="${esc(st.name)}" class="mt-1 text-white bg-primary-700 hover:bg-primary-800 focus:ring-4 focus:ring-primary-300 font-medium rounded-lg text-xs px-3 py-1.5 dark:bg-primary-600 dark:hover:bg-primary-700 focus:outline-none dark:focus:ring-primary-800 disabled:opacity-50" ${!st.editable || !bots.live.set_setting ? 'disabled' : ''}>Apply</button>${changeState(c, st)}</div>`).join('')}</div>`
      : `<p class="text-sm text-gray-500 dark:text-gray-400">${bots.live.list ? 'This AI declares no setting that can change while it runs.' : 'The current values are not readable: this game binary has no openttd.ai.list.'}</p>`;
    return `<tr class="border-b dark:border-gray-700 align-top"><td class="px-4 py-2">${esc(c.id)}</td><td class="px-4 py-2 font-medium text-gray-900 dark:text-white">${esc(c.name || '')}<div class="text-xs text-gray-500">${esc(c.ai || '')}${c.running === false ? ' (stopped)' : ''}</div></td>
      <td class="px-4 py-2">${money(c.money)}</td><td class="px-4 py-2">${editors}</td></tr>`;
  });
  $('bot-running').innerHTML = rows.length ? rows.join('') : '<tr><td class="px-4 py-2" colspan="4">none</td></tr>';
  if (!bots.live.set_setting) {
    $('bot-msg').innerHTML = `<div class="p-3 text-sm text-yellow-800 rounded-lg bg-yellow-50 dark:bg-gray-800 dark:text-yellow-300" role="alert">This game binary cannot change a running bot's settings (no openttd.ai.set_setting); the controls above start a new bot with the values you set.</div>`;
  }
}

async function loadBots() {
  const fresh = await api('/api/bots');
  if (!fresh || fresh.error) return;
  // keep what the owner is editing: a refresh never overwrites a control that has focus
  if (document.activeElement && document.activeElement.closest && document.activeElement.closest('#bot-running, #bot-settings')) return;
  bots = fresh;
  $('bot-source').textContent = bots.source;
  const keep = $('bot-ai').value;
  $('bot-ai').innerHTML = (bots.ais || []).map((a) => `<option>${esc(a.ai)}</option>`).join('');
  if (keep) $('bot-ai').value = keep;
  renderBotSettings();
  renderRunning();
}
async function loadChoices() {
  const c = await api('/api/bots/choices');
  if (c && !c.error) { choices = c; if (bots) { renderBotSettings(); renderRunning(); } }
}
$('bot-ai').onchange = renderBotSettings;
$('bot-start').onclick = async () => {
  const settings = {};
  for (const el of document.querySelectorAll('#bot-settings [data-setting]')) {
    const v = readControl(el);
    if (v === null) { $('bot-msg').textContent = `${el.dataset.setting}: pick one of the listed ${el.dataset.picker}s or leave it empty`; return; }
    settings[el.dataset.setting] = v;
  }
  const r = await api('/api/bots/start', { ai: $('bot-ai').value, settings });
  $('bot-msg').textContent = r.status === 'pending_approval' ? `start waits for approval #${r.approval_id}` : JSON.stringify(r);
  setTimeout(loadBots, 3000);
};
async function ask(company, name, value, from) {
  const key = `${company}:${name}`;
  asked.set(key, { from, to: value, state: 'sent' });
  renderRunning();
  const r = await api('/api/bots/setting', { company, name, value });
  const a = asked.get(key);
  if (r.status === 'pending_approval') Object.assign(a, { state: 'pending', approval: r.approval_id });
  else if (r.error || r.errors || r.status === 'refused' || r.verdict === 'refused') Object.assign(a, { state: 'refused', reason: r.error || (r.errors || []).join('; ') || r.verdict });
  else Object.assign(a, { state: 'pending', approval: r.approval_id ?? '—' });
  renderRunning();
  tick();
}
document.addEventListener('input', (ev) => {
  const t = ev.target;
  if (t.dataset && t.dataset.pair) { const other = $(t.dataset.pair); if (other) other.value = t.value; }
  if (t.dataset && t.dataset.kind === 'toggle') { const s = t.parentElement.querySelector('span'); if (s) s.textContent = t.checked ? 'on' : 'off'; }
});
document.addEventListener('click', async (ev) => {
  const step = ev.target.closest('[data-step]');
  if (step) { const el = $(step.dataset.for); const v = Number(el.value) + Number(step.dataset.step) * Number(el.step || 1); el.value = Math.min(Number(el.max), Math.max(Number(el.min), v)); return; }
  const apply = ev.target.closest('[data-apply]');
  if (apply) {
    const company = Number(apply.dataset.apply); const name = apply.dataset.name;
    const el = document.querySelector(`[data-key="c${company}"][data-setting="${CSS.escape(name)}"]`);
    const st = ((bots.running || []).find((c) => c.id === company) || { settings: [] }).settings.find((x) => x.name === name);
    const v = el ? readControl(el) : null;
    if (v === null || !st) { $('bot-msg').textContent = `${name}: pick one of the listed options`; return; }
    if (v < st.min || v > st.max) { $('bot-msg').textContent = `${name}: ${v} is outside ${st.min}..${st.max}`; return; }
    return ask(company, name, v, Number(st.value));
  }
  const undo = ev.target.closest('[data-undo]');
  if (undo) {
    const [company, name] = undo.dataset.undo.split(':');
    const a = asked.get(undo.dataset.undo);
    if (a) return ask(Number(company), name, a.from, a.to);
  }
});
loadChoices().catch(() => {});
setInterval(() => loadChoices().catch(() => {}), 60000);
loadBots().catch(() => {});
setInterval(() => loadBots().catch(() => {}), 10000);
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

/* ---- plan before acting: AlgoGlyph action-plan, drawn as a DAG ------------------------------ */
(function actionPlan() {
  const box = document.getElementById('plan-graph'); if (!box || !window.echarts) return;
  let chart = null;
  // Through the one door (components.js ecInit, published by overview.js).
  const ec = () => { if (!chart && globalThis.ecInit) chart = globalThis.ecInit(box, echarts); return chart; };
  const esc = (v) => String(v ?? '').replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
  document.getElementById('plan-run').onclick = async () => {
    let actions;
    try { actions = JSON.parse(document.getElementById('plan-actions').value); } catch (e) { document.getElementById('plan-msg').textContent = 'actions are not JSON: ' + e.message; return; }
    const r = await fetch('/api/actionplan', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ actions }) });
    const d = await r.json();
    if (!d.ok) { document.getElementById('plan-msg').textContent = 'refused: ' + JSON.stringify(d.errors || d.error); document.getElementById('plan-steps').innerHTML = ''; return; }
    const res = d.result;
    document.getElementById('plan-msg').textContent = `${res.steps.length} press(es) in ${res.waves} wave(s); final cash ${res.final.cash}, loan ${res.final.loan}; lowest cash ${res.lowest?.cash ?? res.lowest}`;
    document.getElementById('plan-steps').innerHTML = res.steps.map((s) => `<li>${s.n}. <b>${esc(s.action)}</b> ${s.presses > 1 ? `(${s.press}/${s.presses})` : ''} &rarr; cash ${s.cash}, loan ${s.loan}</li>`).join('');
    const byWave = {};
    const nodes = res.graph.nodes.map((n) => { const i = (byWave[n.wave] = (byWave[n.wave] || 0) + 1) - 1; return { id: n.id, name: `${n.label || n.id}${n.presses > 1 ? ' x' + n.presses : ''}`, x: n.wave * 160, y: i * 70, symbolSize: 38, label: { show: true, color: '#e5e7eb' } }; });
    const c = ec(); if (!c) return;
    c.setOption({ backgroundColor: 'transparent', tooltip: {}, series: [{ type: 'graph', layout: 'none', roam: 'move', edgeSymbol: ['none', 'arrow'], edgeSymbolSize: 10,
      data: nodes, links: res.graph.edges.map((e) => ({ source: e.from, target: e.to })), itemStyle: { color: '#3987e5' }, lineStyle: { color: '#9ca3af', width: 1.5 } }] }, true);
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
    rows.innerHTML = r.items.map((it) => `<tr class="bg-white border-b dark:bg-gray-800 dark:border-gray-700">
      <td class="px-3 py-2"><input type="checkbox" value="${it.id}" ${/downloaded|installed/i.test(it.state) ? 'disabled' : ''} class="w-4 h-4 text-blue-600 bg-gray-100 border-gray-300 rounded-sm" aria-label="Select ${esc(it.name)}"></td>
      <td class="px-3 py-2 font-medium text-gray-900 dark:text-white">${esc(it.name)}</td>
      <td class="px-3 py-2">${esc(it.type)}</td><td class="px-3 py-2">${esc(it.state)}</td><td class="px-3 py-2 font-mono">${it.id}</td></tr>`).join('');
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
