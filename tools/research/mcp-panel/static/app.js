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
const out = (s) => { const o = $('console-out'); o.textContent += s + '\n'; o.scrollTop = o.scrollHeight; };
const toggleConsole = (show) => {
  const c = $('console'); const on = show ?? c.classList.contains('hidden');
  c.classList.toggle('hidden', !on); if (on) $('console-in').focus();
};
$('open-console').onclick = () => toggleConsole(true);
$('side-console').onclick = () => toggleConsole(true);
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
/rhai <script>                run a Rhai script against this panel (tools/research/mcp-panel/rhai)
/clear                        clear this console`;

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
    if (head === '/rhai') { const r = await api('/api/rhai', { script: arg }); return out(r.output ?? JSON.stringify(r)); }
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
