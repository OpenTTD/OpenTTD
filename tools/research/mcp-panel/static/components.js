// RESEARCH-ONLY: reusable components over the mcp-hub WebSocket surface (research/mcp-hub.md).
// Pure functions first (tested with node --test, test/components.test.mjs), DOM components after.
// Every list, form and stream is derived from what the hub returns; nothing is named here.

/** Flowbite classes already in the panel's compiled app.css (index.html uses each of them). */
export const CLS = {
  card: 'p-4 mb-4 bg-white border border-gray-200 rounded-lg shadow-sm dark:border-gray-700 dark:bg-gray-800',
  title: 'mb-3 text-lg font-semibold text-gray-900 dark:text-white',
  label: 'mb-2 text-sm font-semibold text-gray-900 dark:text-white',
  muted: 'text-xs text-gray-500 dark:text-gray-400',
  input: 'bg-gray-50 border border-gray-300 text-gray-900 text-sm rounded-lg block w-full p-2.5 dark:bg-gray-700 dark:border-gray-600 dark:text-white',
  primary: 'text-white bg-primary-700 hover:bg-primary-800 focus:ring-4 focus:ring-primary-300 font-medium rounded-lg text-sm px-4 py-2 dark:bg-primary-600 dark:hover:bg-primary-700 focus:outline-none dark:focus:ring-primary-800',
  secondary: 'text-gray-900 bg-white border border-gray-300 hover:bg-gray-100 focus:ring-4 focus:ring-gray-100 font-medium rounded-lg text-sm px-4 py-2 dark:bg-gray-800 dark:text-white dark:border-gray-600 dark:hover:bg-gray-700 dark:focus:ring-gray-700',
  table: 'w-full text-sm text-left text-gray-500 dark:text-gray-400',
  thead: 'text-xs text-gray-700 uppercase bg-gray-50 dark:bg-gray-700 dark:text-gray-400',
  cell: 'px-4 py-2',
  link: 'flex items-center p-2 text-base text-gray-900 rounded-lg hover:bg-gray-100 dark:text-gray-200 dark:hover:bg-gray-700',
  pre: 'w-full rounded-sm bg-gray-900',
  /** A form control of the panel's pages (the input above plus Flowbite's primary focus ring). */
  field: 'bg-gray-50 border border-gray-300 text-gray-900 text-sm rounded-lg focus:ring-primary-500 focus:border-primary-500 block w-full p-2.5 dark:bg-gray-700 dark:border-gray-600 dark:text-white',
};

/* ---- the kit every page module shares (classic scripts cannot import it, see app.js) -------- */

/** The element of an id. */
export const $ = (id) => document.getElementById(id);
/** HTML-escape a value for markup; null and undefined become ''. */
export const esc = (s) => String(s ?? '').replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
/** Numbers as the panel writes them: 1,234,567 and 1.2M. */
export const whole = new Intl.NumberFormat('en');
export const compact = new Intl.NumberFormat('en', { notation: 'compact', maximumFractionDigits: 1 });
/** Flowbite badge tones. */
export const TONE = {
  green: 'bg-green-100 text-green-800 dark:bg-green-900 dark:text-green-300',
  yellow: 'bg-yellow-100 text-yellow-800 dark:bg-yellow-900 dark:text-yellow-300',
  blue: 'bg-blue-100 text-blue-800 dark:bg-blue-900 dark:text-blue-300',
  gray: 'bg-gray-100 text-gray-800 dark:bg-gray-700 dark:text-gray-300',
};
/** A Flowbite badge of one of TONE's tones, its text escaped. */
export const badge = (text, tone = 'gray') => `<span class="whitespace-nowrap text-xs font-medium px-2 py-0.5 rounded-sm ${TONE[tone]}">${esc(text)}</span>`;
/** Chart ink on the dark theme, and the value/category axis style drawn with it. */
export const INK = { text: '#d1d5db', muted: '#9ca3af', grid: '#374151' };
export const AXIS = { axisLabel: { color: INK.muted, hideOverlap: true }, axisLine: { lineStyle: { color: INK.grid } }, splitLine: { lineStyle: { color: INK.grid, type: 'dashed' } } };

/**
 * Write a patch into the address's query (replaceState, no history entry): a key whose value is
 * undefined, null or '' is removed, any other is set. `keyOf` maps a patch key to its query key.
 */
export function patchAddress(patch, keyOf = (k) => k, win = globalThis) {
  const u = new URL(win.location.href);
  for (const [k, v] of Object.entries(patch)) {
    if (v === undefined || v === '' || v === null) u.searchParams.delete(keyOf(k)); else u.searchParams.set(keyOf(k), v);
  }
  win.history.replaceState(null, '', u);
}

/** The input fields a JSON Schema object asks for, in property order. */
export function schemaFields(schema) {
  const props = (schema && schema.properties) || {};
  const required = new Set((schema && schema.required) || []);
  return Object.entries(props).map(([name, p]) => {
    let kind = 'json';
    if (Array.isArray(p.enum)) kind = 'enum';
    else if (p.type === 'boolean') kind = 'boolean';
    else if (p.type === 'integer' || p.type === 'number') kind = 'number';
    else if (p.type === 'string') kind = 'string';
    return { name, kind, required: required.has(name), options: p.enum || [],
             description: p.description || '', default: p.default };
  });
}

/** One field's raw text as the value the operation expects; undefined leaves it out. */
export function coerce(field, raw) {
  if (raw === '' || raw === undefined || raw === null) return undefined;
  switch (field.kind) {
    case 'number': { const n = Number(raw); if (Number.isNaN(n)) throw new Error(`${field.name}: not a number`); return n; }
    case 'boolean': return raw === true || raw === 'true' || raw === 'on';
    case 'json': try { return JSON.parse(raw); } catch { throw new Error(`${field.name}: not JSON`); }
    default: return String(raw);
  }
}

/** The hub.call params for an operation and the values a form holds. */
export function callParams(op, values) {
  const params = { instance: op.instance, id: op.id };
  const body = {};
  for (const f of schemaFields(op.schema)) {
    const v = coerce(f, values[f.name]);
    if (v !== undefined) body[f.name] = v;
    else if (f.required) throw new Error(`${f.name} is required`);
  }
  if (op.schema) params.body = body;
  const query = {};
  for (const q of op.query || []) {
    const v = values[`?${q.name}`];
    if (v !== undefined && v !== '') query[q.name] = v;
    else if (q.required) throw new Error(`${q.name} is required`);
  }
  if (Object.keys(query).length) params.query = query;
  return params;
}

/** The page state kept in the URL hash: instance, operation, filter text, event kinds and the track plan's inputs. */
export function parseHash(hash) {
  const out = { instance: '', op: '', q: '', kinds: [], plan: '' };
  for (const part of String(hash || '').replace(/^#/, '').split('&')) {
    const [k, v = ''] = part.split('=');
    if (!k) continue;
    const val = decodeURIComponent(v);
    if (k === 'kinds') out.kinds = val ? val.split(',') : [];
    else if (k in out) out[k] = val;
  }
  return out;
}

export function formatHash(state) {
  const parts = [];
  for (const k of ['instance', 'op', 'q', 'plan']) if (state[k]) parts.push(`${k}=${encodeURIComponent(state[k])}`);
  if (state.kinds && state.kinds.length) parts.push(`kinds=${encodeURIComponent(state.kinds.join(','))}`);
  return parts.length ? `#${parts.join('&')}` : '';
}

/** Operations grouped by instance, then tag, filtered by free text over id, path and summary. */
export function groupOps(ops, filter = '') {
  const f = filter.toLowerCase();
  const groups = {};
  for (const op of ops) {
    const hay = `${op.id} ${op.path} ${op.summary} ${(op.tags || []).join(' ')}`.toLowerCase();
    if (f && !hay.includes(f)) continue;
    const tag = (op.tags && op.tags[0]) || 'other';
    ((groups[op.instance] ||= {})[tag] ||= []).push(op);
  }
  return groups;
}

/** A WebSocket JSON-RPC client of the hub that reconnects and re-subscribes on its own. */
export class HubClient {
  constructor(url, { WebSocketImpl = globalThis.WebSocket, retryMs = 1500 } = {}) {
    Object.assign(this, { url, WebSocketImpl, retryMs, nextId: 1, pending: new Map(), listeners: new Map(), subscription: null });
  }

  on(method, fn) { (this.listeners.get(method) || this.listeners.set(method, []).get(method)).push(fn); return this; }

  emit(method, payload) { for (const fn of this.listeners.get(method) || []) fn(payload); }

  connect() {
    const ws = new this.WebSocketImpl(this.url);
    this.ws = ws;
    ws.onopen = () => { this.emit('open'); if (this.subscription) this.call('hub.subscribe', this.subscription); };
    ws.onclose = () => { this.emit('close'); for (const [, p] of this.pending) p.reject(new Error('closed')); this.pending.clear(); setTimeout(() => this.connect(), this.retryMs); };
    ws.onmessage = (m) => this.receive(JSON.parse(m.data));
    return this;
  }

  receive(msg) {
    if (msg.id !== undefined && this.pending.has(msg.id)) {
      const p = this.pending.get(msg.id); this.pending.delete(msg.id);
      if (msg.error) p.reject(Object.assign(new Error(msg.error.message), { code: msg.error.code })); else p.resolve(msg.result);
    } else if (msg.method) this.emit(msg.method, msg.params);
  }

  call(method, params = {}) {
    const id = this.nextId++;
    return new Promise((resolve, reject) => {
      this.pending.set(id, { resolve, reject });
      this.ws.send(JSON.stringify({ jsonrpc: '2.0', id, method, params }));
    });
  }

  subscribe(params = {}) { this.subscription = params; return this.call('hub.subscribe', params); }
}

// ---------------------------------------------------------------------------------------- DOM

function el(tag, attrs = {}, ...children) {
  const n = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs)) {
    if (k === 'class') n.className = v; else if (k.startsWith('on')) n.addEventListener(k.slice(2), v); else if (v !== undefined) n.setAttribute(k, v);
  }
  for (const c of children.flat()) if (c !== null && c !== undefined) n.append(c.nodeType ? c : document.createTextNode(String(c)));
  return n;
}

/** Pretty JSON in a dark block; every `openttd://` URI in it is a link that reads that resource. */
export function jsonView(value, onUri) {
  const pre = el('pre', { class: `${CLS.pre} p-3 text-xs text-gray-300 overflow-x-auto` });
  const text = JSON.stringify(value, null, 2);
  let last = 0;
  for (const m of text.matchAll(/openttd:\/\/[\w/.-]+/g)) {
    pre.append(text.slice(last, m.index));
    pre.append(onUri ? el('a', { href: '#', class: 'hover:underline', onclick: (e) => { e.preventDefault(); onUri(m[0]); } }, m[0]) : m[0]);
    last = m.index + m[0].length;
  }
  pre.append(text.slice(last));
  return pre;
}

/** A form for one operation, derived from its schema and query; submitting calls it. */
export function operationForm(op, client, { onResult, onUri } = {}) {
  const out = el('div', { class: 'mt-3' });
  const inputs = {};
  const fields = schemaFields(op.schema).map((f) => {
    let input;
    if (f.kind === 'enum') input = el('select', { class: CLS.input }, el('option', { value: '' }, '—'), ...f.options.map((o) => el('option', { value: o }, o)));
    else if (f.kind === 'boolean') input = el('input', { type: 'checkbox' });
    else if (f.kind === 'json') input = el('textarea', { class: CLS.input, rows: '3', placeholder: 'JSON' });
    else input = el('input', { class: CLS.input, type: f.kind === 'number' ? 'number' : 'text', value: f.default ?? undefined });
    inputs[f.name] = input;
    return el('label', { class: 'block mb-2' }, el('span', { class: CLS.label }, f.name + (f.required ? ' *' : '')), el('span', { class: `${CLS.muted} block` }, f.description), input);
  });
  const queries = (op.query || []).map((q) => {
    const input = el('input', { class: CLS.input, type: 'text' });
    inputs[`?${q.name}`] = input;
    return el('label', { class: 'block mb-2' }, el('span', { class: CLS.label }, `?${q.name}` + (q.required ? ' *' : '')), input);
  });
  const run = el('button', { type: 'submit', class: op.read_only ? CLS.secondary : CLS.primary }, op.read_only ? 'Read' : 'Call');
  const form = el('form', { onsubmit: async (e) => {
    e.preventDefault();
    const values = Object.fromEntries(Object.entries(inputs).map(([k, i]) => [k, i.type === 'checkbox' ? i.checked : i.value]));
    out.replaceChildren(el('p', { class: CLS.muted }, 'calling…'));
    try {
      const result = await client.call('hub.call', callParams(op, values));
      out.replaceChildren(jsonView(result, onUri));
      if (onResult) onResult(op, result);
    } catch (err) { out.replaceChildren(el('p', { class: 'text-sm text-red-800' }, err.message)); }
  } }, ...fields, ...queries, run);
  return el('div', {}, op.stream ? el('p', { class: CLS.muted }, 'A stream: it arrives in the event panel.') : form, out);
}

/** Every operation of every instance, grouped and filterable; clicking one opens its form. */
export function catalogueView(container, ops, client, state, { onSelect, onUri } = {}) {
  const groups = groupOps(ops.filter((o) => !state.instance || o.instance === state.instance), state.q);
  container.replaceChildren();
  for (const [inst, tags] of Object.entries(groups)) {
    const card = el('div', { class: CLS.card }, el('h3', { class: CLS.title }, inst));
    for (const [tag, list] of Object.entries(tags)) {
      card.append(el('p', { class: `${CLS.label} mt-3` }, `${tag} (${list.length})`));
      for (const op of list) {
        const key = `${op.instance}/${op.id}`;
        const body = el('div', {});
        const head = el('a', { href: '#', class: CLS.link, onclick: (e) => {
          e.preventDefault();
          if (body.childElementCount) { body.replaceChildren(); return; }
          body.replaceChildren(operationForm(op, client, { onUri }));
          if (onSelect) onSelect(op);
        } }, el('span', { class: 'font-mono text-xs' }, op.method), op.id, el('span', { class: `${CLS.muted} ms-2` }, op.summary));
        card.append(el('div', { id: key }, head, body));
        if (state.op === key) body.replaceChildren(operationForm(op, client, { onUri }));
      }
    }
    container.append(card);
  }
  if (!container.childElementCount) container.append(el('p', { class: CLS.muted }, 'No operation matches.'));
}

/** The instances as a clickable table; clicking one narrows the catalogue to it. */
export function instancesView(container, instances, state, onPick) {
  const row = (i) => el('tr', { class: 'border-b dark:border-gray-700' },
    el('td', { class: CLS.cell }, el('a', { href: '#', class: 'font-medium text-primary-600 dark:text-primary-500 hover:underline', onclick: (e) => { e.preventDefault(); onPick(state.instance === i.name ? '' : i.name); } }, i.name)),
    el('td', { class: CLS.cell }, i.kind), el('td', { class: CLS.cell }, i.access), el('td', { class: CLS.cell }, String(i.operations)),
    el('td', { class: `${CLS.cell} font-mono text-xs` }, i.base), el('td', { class: CLS.cell }, i.error || ''));
  container.replaceChildren(el('div', { class: 'overflow-x-auto' }, el('table', { class: CLS.table },
    el('thead', { class: CLS.thead }, el('tr', {}, ...['Instance', 'Kind', 'Access', 'Operations', 'Base', 'Error'].map((h) => el('th', { class: 'px-4 py-3' }, h)))),
    el('tbody', {}, ...instances.map(row)))));
}

/** The live event stream: newest first, bounded, each event's instance and kind clickable. */
export function eventsView(container, { limit = 200, onKind, onInstance } = {}) {
  const list = el('div', { class: 'space-y-1 max-h-80 overflow-y-auto' });
  container.replaceChildren(list);
  return (params) => {
    const e = params.event || {};
    const line = el('div', { class: 'text-xs font-mono text-gray-700 dark:text-gray-300' },
      el('a', { href: '#', class: 'text-primary-600 dark:text-primary-500 hover:underline', onclick: (ev) => { ev.preventDefault(); onInstance && onInstance(params.instance); } }, params.instance),
      el('a', { href: '#', class: 'font-semibold hover:underline', onclick: (ev) => { ev.preventDefault(); onKind && onKind(e.kind); } }, e.kind || '?'),
      JSON.stringify(Object.fromEntries(Object.entries(e).filter(([k]) => !['kind', 'seq'].includes(k)))));
    list.prepend(line);
    while (list.childElementCount > limit) list.lastChild.remove();
  };
}

/** Counts over a hub.audit answer: instances audited, skipped, and duplicate pairs. */
export function auditSummary(results) {
  const audited = results.filter((r) => !r.error);
  return { instances: results.length, audited: audited.length, skipped: results.length - audited.length,
           pairs: audited.reduce((n, r) => n + (r.duplicates || []).length, 0),
           depotPairs: audited.reduce((n, r) => n + ((r.depots && r.depots.duplicates) || []).length, 0) };
}

/** The operation that moves the owner's camera, wherever it is offered (a GUI client's MCP tool or
 * a panel's derived client route), found in the catalogue rather than named per instance. */
export function scrollOp(ops) {
  return ops.find((o) => o.id === 'tool:openttd.view.scroll_to') || ops.find((o) => /view[._]?scroll[._]?to$/i.test(o.id));
}

/** The audit as cards: every duplicate pair with both stations clickable to centre the camera. */
export function auditView(container, results, { onStation } = {}) {
  const s = auditSummary(results);
  const head = el('p', { class: CLS.muted }, `${s.audited} instance(s) audited, ${s.skipped} skipped, ${s.pairs} duplicate station pair(s), ${s.depotPairs} duplicate depot pair(s).`);
  const rows = [];
  for (const r of results) {
    if (r.error) { rows.push(el('p', { class: CLS.muted }, `${r.instance}: ${r.error}`)); continue; }
    const dups = r.duplicates || [];
    rows.push(el('p', { class: `${CLS.label} mt-3` }, `${r.instance}: ${dups.length ? `${dups.length} duplicate pair(s)` : 'no duplicate'} (${r.stations} stations, catchment from the ${r.catchment})`));
    for (const d of dups) {
      const link = (st) => el('a', { href: '#', class: 'font-medium text-primary-600 dark:text-primary-500 hover:underline', onclick: (e) => { e.preventDefault(); onStation && onStation(st); } }, `${st.name} (${st.x},${st.y})`);
      rows.push(el('div', { class: 'text-sm text-gray-700 dark:text-gray-300' }, `owner ${d.owner}, ${d.facilities.join('+')}: `, link(d.a), ' and ', link(d.b), `, ${d.distance} tile(s) apart`));
    }
    const dep = r.depots || {};
    if (dep.error) { rows.push(el('p', { class: CLS.muted }, `depots: ${dep.error}`)); continue; }
    rows.push(el('p', { class: CLS.muted }, `depots: ${dep.depots} (service reach ${dep.reach} tiles, ${dep.network ? 'along the road network' : 'straight distance only'})`));
    for (const d of dep.duplicates || []) {
      const link = (st) => el('a', { href: '#', class: 'font-medium text-primary-600 dark:text-primary-500 hover:underline', onclick: (e) => { e.preventDefault(); onStation && onStation(st); } }, `depot ${st.id} (${st.x},${st.y})`);
      rows.push(el('div', { class: 'text-sm text-gray-700 dark:text-gray-300' }, `owner ${d.owner}, ${d.type} depots: `, link(d.a), ' and ', link(d.b), `, ${d.distance} tile(s) by ${d.by}`));
    }
  }
  container.replaceChildren(head, ...rows);
  return s;
}

/* ---- the track planner: AlgoGlyph through the hub (algoglyph.track_plan, I0073) ------------- */

export const HEADINGS = ['NE', 'SE', 'SW', 'NW'];

/** A plan's inputs as one hash value: "x,y,DIR;x,y[,DIR];railtype". */
export function formatPlan(p) {
  if (!p || !p.from || !p.to) return '';
  const end = (e) => [e.x, e.y, e.dir].filter((v) => v !== undefined && v !== '').join(',');
  return [end(p.from), end(p.to), p.railtype || ''].join(';').replace(/;$/, '');
}

/** The inverse of formatPlan; null when the value is not a plan. */
export function parsePlan(s) {
  const [a, b, railtype = ''] = String(s || '').split(';');
  const end = (v) => {
    const [x, y, dir] = String(v || '').split(',');
    if (!/^\d+$/.test(x || '') || !/^\d+$/.test(y || '')) return null;
    return { x: Number(x), y: Number(y), ...(HEADINGS.includes(dir) ? { dir } : {}) };
  };
  const from = end(a), to = end(b);
  if (!from || !to || !from.dir) return null;
  return { from, to, railtype };
}

/** The params of algoglyph.track_plan for an instance and a plan. */
export function trackPlanParams(instance, p) {
  return { instance, from: p.from, to: p.to, ...(p.railtype ? { railtype: p.railtype } : {}) };
}

/** What the answer says, whether it came wrapped ({ok, result}) or bare: status, money, counts. */
export function planSummary(answer) {
  const r = (answer && answer.result && typeof answer.result === 'object' && !Array.isArray(answer.result)) ? answer.result : (answer || {});
  const counts = Object.entries(r.counts || {}).filter(([, v]) => typeof v === 'number');
  return { status: r.status || 'unknown', money: typeof r.money === 'number' ? Math.round(r.money) : null,
           exact: !!(r.priced && r.priced.exact), counts, pieces: Array.isArray(r.pieces) ? r.pieces.length : 0 };
}

/** The instances that publish what a plan reads (openttd://map), found in the catalogue. */
export function planInstances(ops) {
  return [...new Set(ops.filter((o) => o.id === 'resource:openttd://map').map((o) => o.instance))];
}

/** The rail types a game offers, from its openttd://railtypes answer (MCP or panel envelope). */
export function railtypeOptions(answer) {
  let v = answer;
  const text = v && v.contents && v.contents[0] && v.contents[0].text;
  if (text) { try { v = JSON.parse(text); } catch { v = {}; } }
  const data = (v && v.data && typeof v.data === 'object') ? v.data : (v || {});
  return (data.types || []).map((t) => ({ label: t.label, name: t.name, cost: t.build_cost }));
}

/** The planner card: pick a game, two ends and a rail type; AlgoGlyph answers with the route. */
export function trackPlanView(container, client, { ops = [], plan = '', instance = '', onPlan, onStart } = {}) {
  const games = planInstances(ops);
  const p = parsePlan(plan) || { from: { x: '', y: '', dir: 'NE' }, to: { x: '', y: '' }, railtype: '' };
  const num = (v) => el('input', { class: CLS.input, type: 'number', min: '0', value: v === '' ? undefined : v });
  const select = (options, value) => {
    const s = el('select', { class: CLS.input }, ...options.map(([v, label]) => el('option', { value: v }, label)));
    s.value = value;
    return s;
  };
  const game = select(games.map((g) => [g, g]), games.includes(instance) ? instance : (games[0] || ''));
  const fx = num(p.from.x), fy = num(p.from.y), fd = select(HEADINGS.map((h) => [h, h]), p.from.dir || 'NE');
  const tx = num(p.to.x), ty = num(p.to.y);
  const rail = select([['', 'plain rail']], p.railtype || '');
  const out = el('div', { class: 'mt-3' });
  const loadRail = async () => {
    if (!game.value) return;
    try {
      const options = railtypeOptions(await client.call('hub.call', { instance: game.value, id: 'resource:openttd://railtypes' }));
      const keep = rail.value || p.railtype;
      rail.replaceChildren(el('option', { value: '' }, 'plain rail'), ...options.map((o) => el('option', { value: o.label }, `${o.name} (${o.label}, ${o.cost} a tile)`)));
      rail.value = options.some((o) => o.label === keep) ? keep : '';
    } catch { /* a game without openttd://railtypes plans on plain rail */ }
  };
  game.addEventListener('change', loadRail);
  const field = (label, ...inputs) => el('label', { class: 'block mb-2' }, el('span', { class: CLS.label }, label), el('div', { class: 'flex gap-2' }, ...inputs));
  const form = el('form', { onsubmit: async (e) => {
    e.preventDefault();
    const plan = { from: { x: Number(fx.value), y: Number(fy.value), dir: fd.value }, to: { x: Number(tx.value), y: Number(ty.value) }, railtype: rail.value };
    if (onPlan) onPlan(formatPlan(plan), game.value);
    out.replaceChildren(el('p', { class: CLS.muted }, 'planning…'));
    try {
      const s = planSummary(await client.call('algoglyph.track_plan', trackPlanParams(game.value, plan)));
      const tone = TONE[s.status === 'found' ? 'green' : 'yellow'];
      out.replaceChildren(
        el('div', { class: 'flex items-center gap-2 mb-2' },
          el('span', { class: `${tone} text-xs font-medium px-2.5 py-0.5 rounded-sm` }, s.status),
          s.money === null ? null : el('span', { class: 'text-sm font-semibold text-gray-900 dark:text-white' }, `${s.money.toLocaleString()} (game money${s.exact ? ', exact' : ''})`),
          el('span', { class: CLS.muted }, `${s.pieces} piece(s)`)),
        el('table', { class: CLS.table }, el('tbody', {}, ...s.counts.map(([k, v]) => el('tr', { class: 'border-b dark:border-gray-700' },
          el('th', { class: `${CLS.cell} font-medium text-gray-900 dark:text-white` }, k), el('td', { class: CLS.cell }, Number.isInteger(v) ? v : v.toFixed(1)))))),
        onStart ? el('button', { type: 'button', class: `${CLS.secondary} mt-3`, onclick: () => onStart(plan.from) }, 'Centre the camera on the start') : null);
    } catch (err) { out.replaceChildren(el('p', { class: 'text-sm text-red-800 dark:text-red-300' }, err.message)); }
  } },
  el('label', { class: 'block mb-2' }, el('span', { class: CLS.label }, 'Game'), game),
  field('From (x, y, heading)', fx, fy, fd),
  field('To (x, y)', tx, ty),
  el('label', { class: 'block mb-2' }, el('span', { class: CLS.label }, 'Rail type'), rail),
  el('button', { type: 'submit', class: CLS.primary }, 'Plan'));
  container.replaceChildren(games.length ? form : el('p', { class: CLS.muted }, 'No instance publishes openttd://map.'), out);
  loadRail();
}

/* ---- the visual layer: one door for every chart (I0048) ----------------------------------- */

/** The validated categorical palette (8 slots), light and dark. */
export const PALETTE = {
  light: ['#2a78d6', '#eb6834', '#1baf7a', '#eda100', '#e87ba4', '#008300', '#6250d6', '#e34948'],
  dark: ['#3987e5', '#d95926', '#199e70', '#c98500', '#d55181', '#008300', '#9085e9', '#e66767'],
};

/** Node kinds of world-graph/v1, in legend order. */
export const KINDS = ['town', 'industry', 'station', 'vehicle'];

/** Degree of every node id in a graph's edge list. */
export function degrees(edges) {
  const d = new Map();
  for (const e of edges) { d.set(e.from, (d.get(e.from) || 0) + 1); d.set(e.to, (d.get(e.to) || 0) + 1); }
  return d;
}

/**
 * An ECharts graph option from a world-graph/v1 document (openttd://world/graph). Pure.
 * mode 'map' places every node at its tile (y grows downwards, as on the map); mode 'force' uses
 * positions computed elsewhere (positions: Map id -> [x, y]). kinds limits which node kinds are
 * shown; labels go to the `labels` nodes of highest degree only, so the view never opens on a
 * wall of text. Every node keeps its id, so a click leads to its details.
 */
export function worldGraphOption(graph, { mode = 'map', positions = new Map(), kinds = KINDS, labels = 30, dark = true } = {}) {
  const shown = new Set(graph.nodes.filter((n) => kinds.includes(n.kind)).map((n) => n.id));
  const deg = degrees(graph.edges.filter((e) => shown.has(e.from) && shown.has(e.to)));
  const labelled = new Set([...shown].sort((a, b) => (deg.get(b) || 0) - (deg.get(a) || 0)).slice(0, labels));
  const nodes = graph.nodes.filter((n) => shown.has(n.id)).map((n) => {
    const [x, y] = mode === 'force' && positions.has(n.id) ? positions.get(n.id) : [n.x, n.y];
    return {
      id: n.id, name: n.name || n.id, x, y, category: KINDS.indexOf(n.kind),
      symbolSize: 4 + 3 * Math.sqrt(deg.get(n.id) || 0), value: deg.get(n.id) || 0,
      label: { show: labelled.has(n.id) },
    };
  });
  const links = graph.edges.filter((e) => shown.has(e.from) && shown.has(e.to))
    .map((e) => ({ source: e.from, target: e.to, value: e.kind, lineStyle: e.kind === 'cargo_link' ? { width: 1 + Math.log2(1 + (e.usage || 0)) } : {} }));
  const palette = dark ? PALETTE.dark : PALETTE.light;
  return {
    color: palette,
    tooltip: { formatter: (p) => (p.dataType === 'edge' ? p.data.value : `${p.data.name} (${p.data.value} links)`) },
    legend: { data: KINDS, textStyle: { color: dark ? '#d1d5db' : '#374151' } },
    series: [{
      type: 'graph', layout: 'none', roam: 'move', zoom: 1,
      categories: KINDS.map((k) => ({ name: k })),
      data: nodes, links, edgeSymbol: ['none', 'arrow'], edgeSymbolSize: 4,
      labelLayout: { hideOverlap: true }, emphasis: { focus: 'adjacency' },
      label: { position: 'right', color: dark ? '#e5e7eb' : '#111827', fontSize: 11 },
      lineStyle: { opacity: 0.35, curveness: 0.1 },
    }],
  };
}

/** Bars of the stations with the most waiting cargo, from a world-graph/v1 document. Pure. */
export function waitingOption(graph, { top = 12, dark = true } = {}) {
  const rows = graph.nodes.filter((n) => n.kind === 'station')
    .map((n) => ({ id: n.id, name: n.name, total: (n.waiting || []).reduce((s, w) => s + (w.count || 0), 0) }))
    .filter((r) => r.total > 0).sort((a, b) => b.total - a.total).slice(0, top).reverse();
  return {
    color: dark ? PALETTE.dark : PALETTE.light,
    grid: { left: 140, right: 24, top: 16, bottom: 24 },
    tooltip: { trigger: 'axis' },
    xAxis: { type: 'value', name: 'waiting' },
    yAxis: { type: 'category', data: rows.map((r) => r.name), axisLabel: { width: 130, overflow: 'truncate', ellipsis: '…' } },
    series: [{ type: 'bar', data: rows.map((r) => ({ value: r.total, id: r.id })) }],
  };
}

/** A time series of each company's money, from samples [{t, companies:[{id, name, money}]}]. Pure. */
export function moneyOption(samples, { dark = true } = {}) {
  const ids = [...new Set(samples.flatMap((s) => s.companies.map((c) => c.id)))];
  return {
    color: dark ? PALETTE.dark : PALETTE.light,
    tooltip: { trigger: 'axis' },
    legend: { textStyle: { color: dark ? '#d1d5db' : '#374151' } },
    grid: { left: 64, right: 24, top: 36, bottom: 56 },
    xAxis: { type: 'time' },
    yAxis: { type: 'value', name: 'money' },
    dataZoom: [{ type: 'inside', zoomOnMouseWheel: 'ctrl', moveOnMouseWheel: false }, { type: 'slider' }],
    series: ids.map((id) => ({
      type: 'line', showSymbol: samples.length < 3, // a line needs two points; show the first ones
      name: samples.flatMap((s) => s.companies).find((c) => c.id === id)?.name || `company ${id}`,
      data: samples.map((s) => [s.t, s.companies.find((c) => c.id === id)?.money ?? null]),
    })),
  };
}

/**
 * The one door: every chart of the panel's pages starts here. Refuses a hidden or zero-width box
 * (it would draw at zero size), names the chart for assistive technology from the box's
 * aria-label, animates unless the reader asked for reduced motion, and follows the box's size.
 */
export function ecInit(box, echarts) {
  if (!box || box.offsetWidth === 0 || box.offsetHeight === 0) return null;
  const reduced = globalThis.matchMedia && matchMedia('(prefers-reduced-motion: reduce)').matches;
  const chart = echarts.getInstanceByDom(box) || echarts.init(box, null, { renderer: 'canvas' });
  const set = chart.setOption.bind(chart);
  chart.setOption = (option, ...rest) => set({
    animation: !reduced, animationDuration: 850, animationEasing: 'cubicOut', animationDurationUpdate: 380,
    animationDelay: (i) => Math.min(i * 14, 420),
    aria: { enabled: true, label: { description: box.getAttribute('aria-label') || '' } },
    ...option,
  }, ...rest);
  if (globalThis.ResizeObserver && !box._resizer) { box._resizer = new ResizeObserver(() => chart.resize()); box._resizer.observe(box); }
  return chart;
}

/**
 * A card that maximizes to the viewport and back: the button, Escape and the address (key `max`)
 * all agree, so a maximized view is a link. `onResize` redraws the content to its new box.
 */
export function maximizable(card, key, { getState, setState, onResize } = {}) {
  const apply = (on) => {
    card.classList.toggle('fixed', on); card.classList.toggle('inset-0', on); card.classList.toggle('z-50', on);
    card.classList.toggle('m-0', on); card.classList.toggle('overflow-auto', on);
    document.body.classList.toggle('overflow-hidden', on);
    if (setState) setState({ max: on ? key : undefined });
    if (onResize) setTimeout(onResize, 30);
  };
  const button = el('button', { type: 'button', class: CLS.secondary, onclick: () => apply(!card.classList.contains('fixed')) }, 'Maximize');
  document.addEventListener('keydown', (e) => { if (e.key === 'Escape' && card.classList.contains('fixed')) { apply(false); button.focus(); } });
  if (getState && getState().max === key) apply(true);
  return button;
}
