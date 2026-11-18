'use strict';
/* One page per MCP tool, GUI client tool and resource, and the catalogue of them, all derived from
   the panel's /openapi.json (itself derived from the live MCP catalogue): /tools, /tool/<name>,
   /client/<name>, /resource/<path>. A tool's form comes from its input schema; its values and the
   catalogue's search are kept in the address, so a filled-in call is a link. */

const $ = (id) => document.getElementById(id);
const esc = (s) => String(s ?? '').replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
/** The shell's Pro class sets, one per role (static/shell.js publishes them as PanelUI first). */
const UI = globalThis.PanelUI;
const card = `${UI.card} mb-4`;
const badge = (text, tone) => `<span class="${UI.badge} ${{
  green: 'bg-green-100 text-green-800 dark:bg-green-900 dark:text-green-300',
  yellow: 'bg-yellow-100 text-yellow-800 dark:bg-yellow-900 dark:text-yellow-300',
  blue: 'bg-blue-100 text-blue-800 dark:bg-blue-900 dark:text-blue-300',
  gray: 'bg-gray-100 text-gray-800 dark:bg-gray-700 dark:text-gray-300',
}[tone || 'gray']}">${esc(text)}</span>`;

/* The catalogue's items from OpenAPI: kind, name, page, the operation it calls, its schema. */
function items(doc) {
  const out = [];
  for (const [path, ops] of Object.entries(doc.paths || {})) {
    for (const [method, op] of Object.entries(ops)) {
      const tag = (op.tags || [])[0];
      let kind = null, name = null;
      if (tag === 'mcp tools' && path.startsWith('/api/tools/')) { kind = 'tool'; name = path.slice(11); }
      else if (tag === 'gui client tools' && path.startsWith('/api/client/')) { kind = 'client'; name = path.slice(12); }
      else if (tag === 'mcp resources' && path.startsWith('/api/resources/')) { kind = 'resource'; name = 'openttd://' + path.slice(15); }
      if (!kind) continue;
      const page = kind === 'resource' ? '/resource/' + path.slice(15) : `/${kind}/${encodeURIComponent(name)}`;
      const schema = op.requestBody?.content?.['application/json']?.schema || null;
      out.push({ kind, name, page, method: method.toUpperCase(), path, summary: op.summary || op.description || '', schema, opId: op.operationId });
    }
  }
  return out.sort((a, b) => a.name.localeCompare(b.name));
}

const gated = (it) => /approval-gated/i.test(it.summary);
const readOnly = (it) => it.kind === 'resource' || /read-only|readonly|^read /i.test(it.summary);

function setQuery(key, value) {
  const u = new URL(location.href);
  if (value === '' || value === undefined || value === null) u.searchParams.delete(key); else u.searchParams.set(key, value);
  history.replaceState(null, '', u);
}

/* ---- a form from a JSON schema ---------------------------------------------------------------- */
function field(key, prop, required, value) {
  const id = 'f-' + key, req = required ? ' required' : '';
  const label = `<label for="${id}" class="block mb-1 text-sm font-medium text-gray-900 dark:text-white">${esc(key)}${required ? ' <span class="text-red-500">*</span>' : ''}
    <span class="ms-1 text-xs font-normal text-gray-500">${esc(prop.type || 'any')}${prop.minimum !== undefined ? ` ≥ ${prop.minimum}` : ''}${prop.maximum !== undefined ? ` ≤ ${prop.maximum}` : ''}</span></label>`;
  const help = prop.description ? `<p class="mt-1 text-xs text-gray-500 dark:text-gray-400">${esc(prop.description)}</p>` : '';
  if (Array.isArray(prop.enum)) {
    return `<div>${label}<select id="${id}" data-key="${esc(key)}" data-type="enum" class="${UI.select} block w-full"${req}><option value="">-</option>${prop.enum.map((v) => `<option${String(v) === value ? ' selected' : ''}>${esc(v)}</option>`).join('')}</select>${help}</div>`;
  }
  if (prop.type === 'boolean') {
    return `<div class="flex items-center gap-2 pt-6"><input id="${id}" data-key="${esc(key)}" data-type="boolean" type="checkbox"${value === 'true' ? ' checked' : ''} class="${UI.check}"><label for="${id}" class="text-sm font-medium text-gray-900 dark:text-white">${esc(key)}</label>${help}</div>`;
  }
  if (prop.type === 'integer' || prop.type === 'number') {
    return `<div>${label}<input id="${id}" data-key="${esc(key)}" data-type="${prop.type}" type="number" step="${prop.type === 'integer' ? 1 : 'any'}" value="${esc(value ?? '')}"${prop.minimum !== undefined ? ` min="${prop.minimum}"` : ''}${prop.maximum !== undefined ? ` max="${prop.maximum}"` : ''} class="${UI.input} block w-full"${req}>${help}</div>`;
  }
  if (prop.type === 'object' || prop.type === 'array' || !prop.type) {
    return `<div class="md:col-span-2">${label}<textarea id="${id}" data-key="${esc(key)}" data-type="json" rows="3" placeholder="${prop.type === 'array' ? '[ ]' : '{ }'}" class="${UI.input} block w-full font-mono">${esc(value ?? '')}</textarea>${help}</div>`;
  }
  return `<div>${label}<input id="${id}" data-key="${esc(key)}" data-type="string" type="text" value="${esc(value ?? '')}" class="${UI.input} block w-full"${req}>${help}</div>`;
}

function readForm() {
  const body = {};
  for (const el of document.querySelectorAll('[data-key]')) {
    const k = el.dataset.key, t = el.dataset.type;
    if (t === 'boolean') { if (el.checked) body[k] = true; continue; }
    const v = el.value.trim();
    if (v === '') continue;
    if (t === 'integer') body[k] = parseInt(v, 10);
    else if (t === 'number') body[k] = Number(v);
    else if (t === 'json') body[k] = JSON.parse(v);
    else body[k] = v;
  }
  return body;
}

const shell = (s) => (/^[A-Za-z0-9_./:=-]+$/.test(s) ? s : `'${s.replace(/'/g, "'\\''")}'`);

/* ---- the same call or read on every surface (pure) ------------------------------------------- */
function toolSurfaces(it, body, host) {
  const kv = Object.entries(body).map(([k, v]) => `${k}=${typeof v === 'object' ? JSON.stringify(v) : v}`).join(' ');
  return [
    ['REST (this panel)', `curl -s -X POST -H 'Content-Type: application/json' -d ${shell(JSON.stringify(body))} http://${host}${it.path}`],
    ['MCP JSON-RPC', JSON.stringify({ jsonrpc: '2.0', id: 1, method: 'tools/call', params: { name: it.name, arguments: body } })],
    ['In-game console ~', it.kind === 'client' ? '(the GUI client has its own MCP server; use the panel or REST)' : `mcp ${it.name}${kv ? ' ' + kv : ''}`],
    ['OpenAPI', `${it.method} ${it.path} · operationId ${it.opId} · /docs`],
  ];
}
const resourceSurfaces = (it, host) => [['REST (this panel)', `curl -s http://${host}${it.path}`],
  ['MCP JSON-RPC', JSON.stringify({ jsonrpc: '2.0', id: 1, method: 'resources/read', params: { uri: it.name } })],
  ['In-game console ~', `mcp read ${it.name}`], ['OpenAPI', `GET ${it.path} · operationId ${it.opId} · /docs`]];
const surfacesHtml = (rows) => rows.map(([k, v]) => `<div><dt class="font-medium text-gray-900 dark:text-white">${esc(k)}</dt><dd class="mt-1 p-2 font-mono text-xs break-all bg-gray-50 rounded-lg dark:bg-gray-900 text-gray-800 dark:text-gray-200">${esc(v)}</dd></div>`).join('');

/* ---- /tool/<name> and /client/<name> ------------------------------------------------------------- */
function renderTool(it) {
  const params = new URL(location.href).searchParams;
  const props = it.schema?.properties || {};
  const required = it.schema?.required || [];
  $('badges').innerHTML = [badge(it.kind === 'client' ? 'GUI client tool' : 'server tool', 'blue'),
    gated(it) ? badge('approval-gated: a change waits for the owner', 'yellow') : '', readOnly(it) ? badge('read-only', 'green') : ''].join('');
  $('page').innerHTML = `
    <div class="${card}">
      <h2 class="mb-4 ${UI.cardTitle}">Call it</h2>
      <form id="call" class="grid gap-4 md:grid-cols-2">${Object.keys(props).length
        ? Object.entries(props).map(([k, p]) => field(k, p, required.includes(k), params.get(k))).join('')
        : '<p class="text-sm text-gray-500 dark:text-gray-400 md:col-span-2">This tool takes no parameters.</p>'}
        <div class="md:col-span-2 flex gap-2">
          <button type="submit" class="${UI.primary}">Call ${esc(it.name)}</button>
          <button type="button" id="share" class="${UI.secondary}">Copy link with these values</button>
        </div>
      </form>
      <div id="answer" class="mt-4"></div>
    </div>
    <div class="${card}">
      <h2 class="mb-4 ${UI.cardTitle}">The same call elsewhere</h2>
      <dl id="surfaces" class="grid gap-3 text-sm"></dl>
    </div>`;
  const surfaces = () => {
    let body = {}; try { body = readForm(); } catch { /* typing */ }
    $('surfaces').innerHTML = surfacesHtml(toolSurfaces(it, body, location.host));
  };
  $('call').oninput = () => { surfaces(); for (const el of document.querySelectorAll('[data-key]')) setQuery(el.dataset.key, el.type === 'checkbox' ? (el.checked ? 'true' : '') : el.value); };
  $('share').onclick = () => navigator.clipboard?.writeText(location.href);
  $('call').onsubmit = async (ev) => {
    ev.preventDefault();
    let body;
    try { body = readForm(); } catch (e) { $('answer').innerHTML = alert('A JSON field does not parse: ' + e.message, 'red'); return; }
    $('answer').innerHTML = '<p class="text-sm text-gray-500">Calling…</p>';
    const r = await fetch(it.path, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) });
    const text = await r.text();
    let v; try { v = JSON.parse(text); } catch { v = text; }
    const pending = v && v.status === 'pending_approval';
    $('answer').innerHTML = (pending
      ? alert(`Waiting for the owner's approval #${esc(v.approval_id)}. Approve it in the panel (<a href="/#control" class="underline">Control and approvals</a>) or in the game: prismatic_mcp approve ${esc(v.approval_id)}. Then call again with the same values.`, 'yellow')
      : r.ok ? '' : alert(`HTTP ${r.status}`, 'red'))
      + `<pre class="p-3 max-h-[32rem] overflow-auto text-xs bg-gray-50 rounded-lg dark:bg-gray-900 text-gray-800 dark:text-gray-200">${esc(typeof v === 'string' ? v : JSON.stringify(v, null, 2))}</pre>`;
  };
  surfaces();
}

function alert(html, tone) {
  const c = { red: 'text-red-800 bg-red-50 dark:bg-gray-800 dark:text-red-400', yellow: 'text-yellow-800 bg-yellow-50 dark:bg-gray-800 dark:text-yellow-300' }[tone];
  return `<div class="p-3 mb-3 text-sm rounded-lg ${c}" role="alert">${html}</div>`;
}

/* ---- /resource/<path> --------------------------------------------------------------------------- */
function renderResource(it) {
  const live = new URL(location.href).searchParams.get('live') === '1';
  $('badges').innerHTML = badge('resource', 'blue') + badge('read-only', 'green');
  $('page').innerHTML = `
    <div class="${card}">
      <div class="flex flex-wrap items-center gap-3 mb-3">
        <button id="refresh" type="button" class="${UI.primary}">Read now</button>
        <label class="inline-flex items-center gap-2 text-sm text-gray-900 dark:text-white"><input id="live" type="checkbox"${live ? ' checked' : ''} class="${UI.check}"> Live (every 2 s, kept in the address)</label>
        <span id="read-at" class="text-xs text-gray-500"></span>
      </div>
      <pre id="data" class="p-3 max-h-[40rem] overflow-auto text-xs bg-gray-50 rounded-lg dark:bg-gray-900 text-gray-800 dark:text-gray-200"></pre>
    </div>
    <div class="${card}">
      <h2 class="mb-4 ${UI.cardTitle}">The same read elsewhere</h2>
      <dl class="grid gap-3 text-sm">
        ${surfacesHtml(resourceSurfaces(it, location.host))}
      </dl>
    </div>`;
  let timer = null;
  const read = async () => {
    const r = await fetch(it.path);
    const text = await r.text();
    let v; try { v = JSON.parse(text); } catch { v = text; }
    $('data').textContent = typeof v === 'string' ? v : JSON.stringify(v, null, 2);
    $('read-at').textContent = `read ${new Date().toLocaleTimeString()} · HTTP ${r.status}`;
  };
  const arm = () => { clearInterval(timer); if ($('live').checked) timer = setInterval(read, 2000); setQuery('live', $('live').checked ? '1' : ''); };
  $('refresh').onclick = read;
  $('live').onchange = arm;
  read(); arm();
}

const boot = (async () => {
  const doc = await (await fetch('/openapi.json')).json();
  const all = items(doc);
  const path = decodeURIComponent(location.pathname);
  // The catalogue is one component (catalogue.js), shared with the panel's home (#catalogue).
  if (path === '/tools') return import('/static/catalogue.js').then((m) => m.mountCatalogue($('page'), m.catalogueItems(doc)));
  const it = all.find((x) => decodeURIComponent(x.page) === path);
  if (!it) {
    $('page').innerHTML = alert('Not in the live catalogue (the game may not offer it, or the GUI client is not attached). <a href="/tools" class="underline">All tools and resources</a>.', 'red');
    return;
  }
  $('crumbs').insertAdjacentHTML('beforeend', `<li class="text-gray-400">/</li><li class="text-gray-500 dark:text-gray-400">${esc({ tool: 'Server tools', client: 'GUI client tools', resource: 'Resources' }[it.kind])}</li>`);
  return it.kind === 'resource' ? renderResource(it) : renderTool(it);
})();

if (typeof module !== 'undefined') {
  module.exports = {
    items, gated, readOnly, field, readForm, shell, toolSurfaces, resourceSurfaces, surfacesHtml, alert, badge, boot,
  };
}
