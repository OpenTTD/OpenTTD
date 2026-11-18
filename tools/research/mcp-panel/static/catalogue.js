/* The MCP catalogue as one component: every server tool, GUI client tool and resource the panel's
   /openapi.json declares (itself derived from the live MCP tools/list and resources/list), faceted
   by facts the catalogue already states and nothing typed here:
     kind      - the OpenAPI tag (server tool, GUI client tool, resource)
     namespace - the name's own segment (openttd.<ns>.*, openttd://<ns>/...)
     category  - the game's command category the description names ("(landscape_construction; ...")
     safety    - approval-gated / read-only / other, from the description and the kind
   Search and every facet live in the address (prefix `c_` on the panel's home, none on /tools), so
   a filtered view is a link. The sunburst drills through kind > namespace > category with its own
   drill, and the drill IS the facet filter (one state: a ring sets kind/namespace/category, the
   breadcrumb clears the deeper ones, the cards follow); a leaf opens the item's own page.
   Pure functions are exported for the Node tests (test/catalogue.test.mjs). */

import { ecInit, maximizable, PALETTE, UI, CLS, esc, badge, patchAddress } from './components.js';

export const KIND_LABEL = { tool: 'Server tools', client: 'GUI client tools', resource: 'Resources' };
export const SAFETY_LABEL = { gated: 'Approval-gated', read: 'Read-only', other: 'Acts directly' };
const SAFETY_TONE = { gated: 'yellow', read: 'green', other: 'blue' };
const FACETS = ['kind', 'ns', 'cat', 'safety'];

/* ---- items and facets (pure) --------------------------------------------------------------- */

/** The catalogue's items from the panel's OpenAPI document. */
export function catalogueItems(doc) {
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
      const summary = op.summary || op.description || '';
      out.push({ kind, name, page, method: method.toUpperCase(), path, summary, ...facetsOf(kind, name, summary) });
    }
  }
  return out.sort((a, b) => a.name.localeCompare(b.name));
}

/** Namespace, category and safety of one item, read from its name and description. */
export function facetsOf(kind, name, summary) {
  let ns;
  if (kind === 'resource') ns = (name.replace(/^openttd:\/\//, '').split('/')[0]) || 'root';
  else { const parts = name.split('.'); ns = parts.length > 2 ? parts[1] : parts[0]; }
  const m = /\(([a-z_]+);/.exec(summary);
  const cat = m ? m[1] : ns;
  const safety = /approval-gated/i.test(summary) ? 'gated'
    : (kind === 'resource' || /\bread-only\b|without mutating|^read |^list |^every |^the /i.test(summary)) ? 'read' : 'other';
  return { ns, cat, safety };
}

export const humanize = (s) => String(s).replace(/_/g, ' ').replace(/^./, (c) => c.toUpperCase());

/** The items a state selects: search on name and description, then every facet that is set. */
export function filterItems(all, state, skip = '') {
  const q = (state.q || '').trim().toLowerCase();
  return all.filter((it) => (!q || it.name.toLowerCase().includes(q) || it.summary.toLowerCase().includes(q))
    && FACETS.every((f) => f === skip || !state[f] || it[f] === state[f]));
}

/** Counts per value of one facet over the items every other facet selects (faceted search). */
export function facetCounts(all, state, facet) {
  const counts = new Map();
  for (const it of filterItems(all, state, facet)) counts.set(it[facet], (counts.get(it[facet]) || 0) + 1);
  return [...counts.entries()].sort((a, b) => b[1] - a[1] || String(a[0]).localeCompare(String(b[0])));
}

/** The hierarchy kind > namespace > category > item; a category equal to its namespace collapses. */
export function hierarchy(items) {
  const root = new Map();
  for (const it of items) {
    const path = [it.kind, it.ns, ...(it.cat !== it.ns ? [it.cat] : [])];
    let level = root;
    for (const key of path) {
      if (!level.has(key)) level.set(key, new Map());
      level = level.get(key);
    }
    level.set('\u0000' + it.name, it);
  }
  const label = (key, depth) => (depth === 0 ? KIND_LABEL[key] || key : humanize(key));
  const build = (map, depth, prefix) => [...map.entries()].map(([key, v]) => {
    if (key.startsWith('\u0000')) return { name: v.name.replace(/^openttd(\.cmd)?\.|^openttd:\/\//, ''), value: 1, page: v.page, safety: v.safety };
    const id = prefix ? prefix + '/' + key : key;
    const children = build(v, depth + 1, id);
    return { name: label(key, depth), id, children, value: children.reduce((s, c) => s + c.value, 0) };
  }).sort((a, b) => b.value - a.value);
  return build(root, 0, '');
}

/** The subtree at a drill path (`kind/ns/cat`), or the whole tree. */
export function drillTo(tree, root) {
  if (!root) return tree;
  let nodes = tree;
  for (let i = 1, parts = root.split('/'); i <= parts.length; i++) {
    const n = nodes.find((x) => x.id === parts.slice(0, i).join('/'));
    if (!n) return tree;
    nodes = n.children;
  }
  return nodes;
}

export function sunburstOption(items, { root = '', dark = true } = {}) {
  const palette = PALETTE[dark ? 'dark' : 'light'];
  const tree = hierarchy(items);
  const data = drillTo(tree, root).map((n, i) => ({ ...n, itemStyle: { color: palette[i % palette.length] } }));
  return {
    backgroundColor: 'transparent',
    tooltip: { formatter: (p) => `${p.data.page ? 'Open ' : ''}<b>${p.name}</b><br>${p.value} item${p.value === 1 ? '' : 's'}${p.data.page ? '' : ' · click to zoom in'}` },
    series: [{
      type: 'sunburst', data, radius: ['14%', '96%'], nodeClick: false, sort: undefined,
      emphasis: { focus: 'ancestor' },
      itemStyle: { borderColor: dark ? '#1f2937' : '#ffffff', borderWidth: 1.5 },
      label: { color: dark ? '#f3f4f6' : '#111827', overflow: 'truncate', ellipsis: '…', minAngle: 10, fontSize: 11 },
      levels: [{}, { label: { rotate: 'tangential', fontWeight: 600 } }, { label: { rotate: 'radial', fontSize: 11 } },
        { label: { rotate: 'radial', fontSize: 10 } }, { label: { show: false } }],
    }],
  };
}

/* ---- address state ------------------------------------------------------------------------- */

const KEYS = ['q', ...FACETS, 'max'];
export function readState(prefix = '', search = globalThis.location?.search || '') {
  const p = new URLSearchParams(search);
  return Object.fromEntries(KEYS.map((k) => [k, p.get(prefix + k) || '']));
}
function writeState(prefix, state) {
  patchAddress(Object.fromEntries(KEYS.map((k) => [k, state[k]])), (k) => prefix + k);
}
/** The drill path the facets spell: kind, then namespace, then a category that is not the namespace. */
export function rootOf(state) {
  const parts = [];
  if (state.kind) {
    parts.push(state.kind);
    if (state.ns) { parts.push(state.ns); if (state.cat && state.cat !== state.ns) parts.push(state.cat); }
  }
  return parts.join('/');
}
/** The facets a drill path sets (the inverse of rootOf). */
export function facetsOfRoot(root) {
  const [kind = '', ns = '', cat = ''] = root ? root.split('/') : [];
  return { kind, ns, cat };
}

/** The same view as a /tools address (unprefixed), for "open the full catalogue". */
export function toolsHref(state) {
  const p = new URLSearchParams();
  for (const k of ['q', ...FACETS]) if (state[k]) p.set(k, state[k]);
  const s = p.toString();
  return '/tools' + (s ? '?' + s : '');
}

/* ---- the component ------------------------------------------------------------------------- */

const pill = (facet, value, label, count, active) => `<button type="button" data-facet="${facet}" data-value="${esc(value)}" aria-pressed="${active}"
  class="inline-flex items-center gap-1.5 px-3 py-1.5 text-sm font-medium rounded-lg border ${active
    ? 'text-white bg-primary-700 border-primary-700 dark:bg-primary-600 dark:border-primary-600'
    : 'text-gray-900 bg-white border-gray-300 hover:bg-gray-100 dark:bg-gray-800 dark:text-white dark:border-gray-600 dark:hover:bg-gray-700'}">${esc(label)}<span class="text-xs opacity-75">${count}</span></button>`;

function itemCard(it) {
  return `<a href="${esc(it.page)}" class="${UI.card} group flex flex-col h-full hover:border-primary-500 hover:shadow-md dark:hover:border-primary-500">
    <div class="flex flex-wrap items-center gap-1.5 mb-2">${badge(KIND_LABEL[it.kind].replace(/s$/, ''), 'gray')}${badge(SAFETY_LABEL[it.safety], SAFETY_TONE[it.safety])}${it.cat !== it.ns ? badge(humanize(it.cat)) : ''}</div>
    <h3 class="font-mono text-sm font-semibold text-gray-900 break-all group-hover:text-primary-600 dark:text-white dark:group-hover:text-primary-400">${esc(it.name)}</h3>
    <p class="mt-1 text-sm text-gray-500 line-clamp-3 dark:text-gray-400">${esc(it.summary.replace(/^approval-gated:?\s*/i, '').replace(/^./, (c) => c.toUpperCase()))}</p>
    <p class="mt-auto pt-3 text-xs font-mono text-gray-400 break-all dark:text-gray-500">${esc(it.method)} ${esc(it.path)}</p></a>`;
}

/**
 * Mount the catalogue into `box`. `compact` (the panel's home): fewer cards and a link to the full
 * /tools view with the same filters. `prefix`: the address keys' prefix.
 */
export function mountCatalogue(box, all, { compact = false, prefix = '', echarts = globalThis.echarts } = {}) {
  const state = readState(prefix);
  const page = compact ? 9 : 48;
  let limit = page;
  const chartId = (prefix || 'c') + 'catalogue-chart';
  box.innerHTML = `
    <div class="flex flex-col gap-3 mb-4">
      <div class="flex flex-col gap-3 sm:flex-row sm:items-center">
        <div class="relative grow"><label for="${prefix}cq" class="sr-only">Search the catalogue</label>
          <div class="absolute inset-y-0 start-0 flex items-center ps-3 pointer-events-none"><svg class="w-4 h-4 text-gray-500 dark:text-gray-400" aria-hidden="true" fill="none" viewBox="0 0 20 20"><path stroke="currentColor" stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="m19 19-4-4m0-7A7 7 0 1 1 1 8a7 7 0 0 1 14 0Z"/></svg></div>
          <input id="${prefix}cq" type="search" value="${esc(state.q)}" placeholder="Search ${all.length} tools and resources: name or what it does" class="${CLS.input} ps-10"></div>
        <span data-count class="text-sm text-gray-500 whitespace-nowrap dark:text-gray-400"></span>
        <button data-reset type="button" class="text-sm font-medium text-primary-600 hover:underline dark:text-primary-500 whitespace-nowrap">Clear filters</button>
      </div>
      <div data-pills="kind" class="flex flex-wrap gap-2" role="group" aria-label="Kind"></div>
      <div data-pills="safety" class="flex flex-wrap gap-2" role="group" aria-label="Safety"></div>
      <div class="grid gap-3 sm:grid-cols-2">
        <div><label for="${prefix}cns" class="block mb-1 text-xs font-medium text-gray-500 dark:text-gray-400">Namespace</label><select id="${prefix}cns" data-select="ns" class="${CLS.select}"></select></div>
        <div><label for="${prefix}ccat" class="block mb-1 text-xs font-medium text-gray-500 dark:text-gray-400">Category</label><select id="${prefix}ccat" data-select="cat" class="${CLS.select}"></select></div>
      </div>
    </div>
    <div class="grid gap-4 lg:grid-cols-3">
      <div id="${chartId}-card" class="${CLS.card} lg:col-span-1 !mb-0 self-start lg:sticky lg:top-20">
        <div class="flex flex-wrap items-center justify-between gap-2 mb-2">
          <div><h3 class="${UI.cardTitle}">Map of the catalogue</h3>
          <p class="mt-2 ${UI.cardSub}">A ring zooms in, a leaf opens its page</p></div>
          <div data-toolbar class="flex items-center gap-2"></div>
        </div>
        <nav aria-label="Catalogue drill path"><ol data-crumbs class="flex flex-wrap items-center gap-1 text-sm"></ol></nav>
        <div id="${chartId}" role="img" aria-label="The catalogue as rings: kind, namespace, category, item" class="w-full h-80 sm:h-96 lg:h-[28rem]"></div>
      </div>
      <div class="lg:col-span-2">
        <div data-cards class="grid gap-3 sm:grid-cols-2 ${compact ? 'xl:grid-cols-3' : 'xl:grid-cols-3'}"></div>
        <div class="flex flex-wrap items-center gap-3 mt-4">
          <button data-more type="button" class="hidden ${UI.secondary}">Show more</button>
          ${compact ? '<a data-full class="text-sm font-medium text-primary-600 hover:underline dark:text-primary-500"></a>' : ''}
        </div>
      </div>
    </div>`;
  const q = (sel) => box.querySelector(sel);
  let chart = null;

  const draw = () => {
    writeState(prefix, state);
    const shown = filterItems(all, state);
    q('[data-count]').textContent = `${shown.length} of ${all.length}`;
    for (const f of ['kind', 'safety']) {
      const labels = f === 'kind' ? KIND_LABEL : SAFETY_LABEL;
      const counts = new Map(facetCounts(all, state, f));
      q(`[data-pills="${f}"]`).innerHTML = pill(f, '', 'All', filterItems(all, state, f).length, !state[f])
        + Object.keys(labels).filter((v) => counts.has(v) || state[f] === v).map((v) => pill(f, v, labels[v], counts.get(v) || 0, state[f] === v)).join('');
    }
    for (const f of ['ns', 'cat']) {
      const counts = facetCounts(all, state, f);
      q(`[data-select="${f}"]`).innerHTML = `<option value="">Every ${f === 'ns' ? 'namespace' : 'category'}</option>`
        + counts.map(([v, n]) => `<option value="${esc(v)}"${state[f] === v ? ' selected' : ''}>${esc(humanize(v))} (${n})</option>`).join('');
    }
    q('[data-cards]').innerHTML = shown.slice(0, limit).map(itemCard).join('')
      || `<p class="text-sm text-gray-500 dark:text-gray-400">Nothing matches. <button data-reset type="button" class="underline">Clear filters</button></p>`;
    q('[data-more]').classList.toggle('hidden', compact || shown.length <= limit);
    q('[data-more]').textContent = `Show more (${shown.length - limit} left)`;
    if (compact) { const a = q('[data-full]'); a.href = toolsHref(state); a.textContent = `Open all ${shown.length} in the full catalogue →`; }
    drawChart(shown);
  };

  const drawChart = (shown) => {
    const root = rootOf(state);
    const parts = root ? root.split('/') : [];
    q('[data-crumbs]').innerHTML = [`<li><button type="button" data-root="" class="${parts.length ? 'text-primary-600 hover:underline dark:text-primary-500' : 'text-gray-500 dark:text-gray-400'}">All</button></li>`,
      ...parts.map((p, i) => `<li class="text-gray-400">/</li><li><button type="button" data-root="${esc(parts.slice(0, i + 1).join('/'))}" class="${i < parts.length - 1 ? 'text-primary-600 hover:underline dark:text-primary-500' : 'text-gray-500 dark:text-gray-400'}">${esc(i === 0 ? KIND_LABEL[p] || p : humanize(p))}</button></li>`)].join('');
    chart = chart || ecInit(document.getElementById(chartId), echarts);
    if (!chart) return;
    chart.setOption(sunburstOption(shown, { root, dark: document.documentElement.classList.contains('dark') }), true);
    if (!chart._wired) {
      chart._wired = true;
      chart.on('click', (p) => {
        if (p.data?.page) { location.href = p.data.page; return; }
        if (p.data?.id) { Object.assign(state, facetsOfRoot(p.data.id)); limit = page; draw(); }
      });
    }
    chart.resize();
  };

  box.addEventListener('click', (e) => {
    const t = e.target.closest('[data-facet],[data-root],[data-reset],[data-more]');
    if (!t) return;
    if (t.dataset.facet) { state[t.dataset.facet] = t.dataset.value; limit = page; }
    else if ('root' in t.dataset) { Object.assign(state, facetsOfRoot(t.dataset.root)); limit = page; }
    else if ('reset' in t.dataset) { for (const k of ['q', ...FACETS]) state[k] = ''; q(`#${prefix}cq`).value = ''; limit = page; }
    else if ('more' in t.dataset) limit += page;
    draw();
  });
  box.addEventListener('change', (e) => {
    const f = e.target.dataset?.select;
    if (f) { state[f] = e.target.value; limit = page; draw(); }
  });
  let timer;
  q(`#${prefix}cq`).addEventListener('input', (e) => { clearTimeout(timer); timer = setTimeout(() => { state.q = e.target.value; limit = page; draw(); }, 120); });
  const cardBox = document.getElementById(chartId + '-card');
  cardBox.querySelector('[data-toolbar]').append(maximizable(cardBox, 'catalogue', {
    getState: () => state, setState: (p) => { Object.assign(state, p); writeState(prefix, state); }, onResize: () => chart && chart.resize(),
  }));
  setTimeout(draw, 30);
  setTimeout(() => chart && chart.resize(), 300);
  return { redraw: draw };
}
