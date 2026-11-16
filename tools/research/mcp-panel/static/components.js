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
};

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

/** The page state kept in the URL hash: instance, operation, filter text and event kinds. */
export function parseHash(hash) {
  const out = { instance: '', op: '', q: '', kinds: [] };
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
  for (const k of ['instance', 'op', 'q']) if (state[k]) parts.push(`${k}=${encodeURIComponent(state[k])}`);
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
           pairs: audited.reduce((n, r) => n + (r.duplicates || []).length, 0) };
}

/** The operation that moves the owner's camera, wherever it is offered (a GUI client's MCP tool or
 * a panel's derived client route), found in the catalogue rather than named per instance. */
export function scrollOp(ops) {
  return ops.find((o) => o.id === 'tool:openttd.view.scroll_to') || ops.find((o) => /view[._]?scroll[._]?to$/i.test(o.id));
}

/** The audit as cards: every duplicate pair with both stations clickable to centre the camera. */
export function auditView(container, results, { onStation } = {}) {
  const s = auditSummary(results);
  const head = el('p', { class: CLS.muted }, `${s.audited} instance(s) audited, ${s.skipped} skipped, ${s.pairs} duplicate pair(s).`);
  const rows = [];
  for (const r of results) {
    if (r.error) { rows.push(el('p', { class: CLS.muted }, `${r.instance}: ${r.error}`)); continue; }
    const dups = r.duplicates || [];
    rows.push(el('p', { class: `${CLS.label} mt-3` }, `${r.instance}: ${dups.length ? `${dups.length} duplicate pair(s)` : 'no duplicate'} (${r.stations} stations, catchment from the ${r.catchment})`));
    for (const d of dups) {
      const link = (st) => el('a', { href: '#', class: 'font-medium text-primary-600 dark:text-primary-500 hover:underline', onclick: (e) => { e.preventDefault(); onStation && onStation(st); } }, `${st.name} (${st.x},${st.y})`);
      rows.push(el('div', { class: 'text-sm text-gray-700 dark:text-gray-300' }, `owner ${d.owner}, ${d.facilities.join('+')}: `, link(d.a), ' and ', link(d.b), `, ${d.distance} tile(s) apart`));
    }
  }
  container.replaceChildren(head, ...rows);
  return s;
}
