/* The event store in the panel (ADR 0033, I0080): /data runs read-only SQL over this instance's
   DuckDB store (POST /api/db/query), keeps the query in the address (?sql=, so a query is a link),
   offers examples derived from the schema (GET /api/db/tables), shows the answer as a Flowbite table
   and, when a column is time-like or numeric and others are numeric, as lines through the one door
   (components.js ecInit). The bus card shows the ZeroMQ endpoint (GET /api/bus) and how to subscribe
   from Rust, Python, Node and Elixir. The pure parts are exported for node --test (test/data.test.mjs). */

import { ecInit, maximizable, PALETTE, UI, $, esc, whole, compact, INK, AXIS as axis, patchAddress, TONE } from './components.js';

/** The registry: every chart of /data, the address keys it reads and where a click goes. */
export const CHARTS = [
  { id: 'data-chart', page: '/data', kind: 'line', params: ['sql', 'x', 'y', 'by', 'max'], drill: 'the row of the answer table' },
];

/** The store's tables as ADR 0033 declares them; used only until GET /api/db/tables answers. */
export const ADR_SCHEMA = [
  { name: 'runs', columns: ['instance', 'session', 'seed', 'started_ms', 'panel_port', 'mcp_port', 'game_port'] },
  { name: 'events', columns: ['instance', 'seq', 'ts_ms', 'kind', 'game_tick', 'game_date', 'data'] },
  { name: 'samples', columns: ['instance', 'ts_ms', 'game_tick', 'game_date', 'company', 'name', 'money', 'loan', 'value', 'net', 'vehicles'] },
].map((t) => ({ name: t.name, columns: t.columns.map((name) => ({ name, type: '' })) }));

/* ---- address state ------------------------------------------------------------------------- */
export function getState(href = globalThis.location?.href) {
  const p = new URL(href || 'http://x/data').searchParams;
  return { sql: p.get('sql') || '', x: p.get('x') || '', y: (p.get('y') || '').split(',').filter(Boolean), by: p.get('by') || '', max: p.get('max') || undefined };
}
export function setState(patch) {
  patchAddress(Object.fromEntries(Object.entries(patch).map(([k, v]) => [k, Array.isArray(v) ? v.join(',') : v])));
}
/** The address of a query on this page: what an example, a cell or a topic links to. */
export const queryHref = (sql) => '/data?sql=' + encodeURIComponent(sql);

/* ---- the schema and the examples derived from it ------------------------------------------ */
const col = (c) => (typeof c === 'string' ? { name: c, type: '' } : { name: String(c?.name ?? c?.column_name ?? c?.column ?? ''), type: String(c?.type ?? c?.data_type ?? c?.column_type ?? '') });

/** Whatever shape GET /api/db/tables answers in, as [{name, columns: [{name, type}]}]: a list of
 *  tables, a map of table to columns, or a query answer of (table_name, column_name, data_type). */
export function normalizeSchema(answer) {
  if (!answer || typeof answer !== 'object') return [];
  if (Array.isArray(answer.columns) && Array.isArray(answer.rows)) {
    const names = answer.columns.map((c) => col(c).name);
    const ti = names.findIndex((n) => /^(table|table_name|name)$/.test(n));
    const ci = names.findIndex((n) => /^(column|column_name)$/.test(n));
    const yi = names.findIndex((n) => /^(type|data_type|column_type)$/.test(n));
    if (ti < 0 || ci < 0) return [];
    const by = new Map();
    for (const r of answer.rows) {
      const t = String(r[ti]);
      if (!by.has(t)) by.set(t, []);
      by.get(t).push({ name: String(r[ci]), type: yi < 0 ? '' : String(r[yi] ?? '') });
    }
    return [...by].map(([name, columns]) => ({ name, columns }));
  }
  const tables = answer.tables ?? answer;
  if (Array.isArray(tables)) {
    return tables.map((t) => (typeof t === 'string' ? { name: t, columns: [] }
      : { name: String(t.name ?? t.table ?? t.table_name ?? ''), columns: (t.columns || []).map(col) })).filter((t) => t.name);
  }
  return Object.entries(tables).filter(([, v]) => Array.isArray(v) || Array.isArray(v?.columns))
    .map(([name, v]) => ({ name, columns: (Array.isArray(v) ? v : v.columns).map(col) }));
}

const ident = (n) => (/^[a-z_][a-z0-9_]*$/.test(n) ? n : `"${String(n).replace(/"/g, '""')}"`);
const lit = (s) => `'${String(s).replace(/'/g, "''")}'`;

/** The example queries a schema allows: each one only when its table has the columns it reads. */
export function exampleQueries(tables, { word = 'station' } = {}) {
  const t = new Map(tables.map((x) => [x.name, new Set(x.columns.map((c) => c.name))]));
  const has = (table, ...cols) => t.has(table) && cols.every((c) => t.get(table).has(c));
  const pick = (table, cols) => cols.filter((c) => t.get(table).has(c));
  const out = [];
  if (has('events', 'kind')) {
    out.push({ id: 'events-by-kind', label: 'Events by kind', sql: 'SELECT kind, count(*) AS n FROM events GROUP BY kind ORDER BY n DESC' });
  }
  if (t.has('events')) {
    const order = pick('events', ['seq', 'ts_ms'])[0];
    const cols = pick('events', ['seq', 'ts_ms', 'kind', 'game_date', 'data']);
    out.push({ id: 'latest-events', label: 'Latest 50 events',
      sql: `SELECT ${cols.length ? cols.join(', ') : '*'} FROM events${order ? ` ORDER BY ${order} DESC` : ''} LIMIT 50` });
  }
  if (has('events', 'game_date')) {
    out.push({ id: 'events-per-day', label: 'Events per game day',
      sql: 'SELECT game_date, count(*) AS events FROM events WHERE game_date IS NOT NULL GROUP BY game_date ORDER BY game_date' });
  }
  if (has('samples', 'value')) {
    const x = pick('samples', ['game_date', 'ts_ms', 'game_tick'])[0];
    const who = pick('samples', ['name', 'company'])[0];
    if (x) {
      out.push({ id: 'company-value', label: 'Company value over time',
        sql: `SELECT ${[x, who, 'value'].filter(Boolean).join(', ')} FROM samples ORDER BY ${x}${who ? `, ${who}` : ''}` });
    }
  }
  if (has('events', 'kind', 'data')) {
    const cols = pick('events', ['seq', 'game_date', 'data']);
    const order = pick('events', ['seq', 'ts_ms'])[0];
    out.push({ id: 'ai-log-word', label: `AI log lines containing "${word}"`,
      sql: `SELECT ${cols.join(', ')} FROM events WHERE kind = 'ai.log' AND CAST(data AS VARCHAR) ILIKE ${lit(`%${word}%`)}${order ? ` ORDER BY ${order} DESC` : ''} LIMIT 100` });
  }
  if (has('runs', 'started_ms')) out.push({ id: 'runs', label: 'Panel starts, newest first', sql: 'SELECT * FROM runs ORDER BY started_ms DESC' });
  for (const x of tables) out.push({ id: `first-${x.name}`, label: `First rows of ${x.name}`, table: x.name, sql: `SELECT * FROM ${ident(x.name)} LIMIT 50` });
  return out.map((e) => ({ ...e, href: queryHref(e.sql) }));
}

/** Where a cell of the answer leads: an event kind to its events, a company to its samples. */
export function cellLink(column, value, tables = ADR_SCHEMA) {
  if (value === null || value === undefined || value === '' || typeof value === 'object') return '';
  const t = new Map(tables.map((x) => [x.name, new Set(x.columns.map((c) => c.name))]));
  if (column === 'kind' && t.get('events')?.has('kind')) {
    const order = t.get('events').has('seq') ? ' ORDER BY seq DESC' : '';
    return queryHref(`SELECT * FROM events WHERE kind = ${lit(value)}${order} LIMIT 50`);
  }
  if (column === 'company' && t.get('samples')?.has('company')) {
    const x = ['game_date', 'ts_ms'].find((c) => t.get('samples').has(c));
    return queryHref(`SELECT * FROM samples WHERE company = ${/^-?\d+$/.test(String(value)) ? value : lit(value)}${x ? ` ORDER BY ${x}` : ''}`);
  }
  return '';
}

/* ---- the answer: columns, their kinds, the axes ------------------------------------------- */
const NUM = /^-?\d+(\.\d+)?([eE][-+]?\d+)?$/;
const DATE = /^\d{4}-\d{2}-\d{2}([T ][\d:.]+Z?)?$/;
const EPOCH = /(^|_)(ts|time|timestamp)$|_ms$|^ms$/i;
const ORDERED = /(^|_)(tick|seq|month|day|year|week|date)$/i;
const IDISH = /^(instance|seq|company|session|seed|id)$|_(id|port|ms)$|^game_tick$|^ts$/i;
const GROUPS = ['name', 'company', 'kind', 'instance'];

/** Column names of an answer ({columns: ['a'] or [{name}], rows}). */
export const columnNames = (answer) => (answer?.columns || []).map((c) => col(c).name);
/** The rows of an answer as arrays in column order (an object row is read by column name). */
export function rowArrays(answer) {
  const names = columnNames(answer);
  return (answer?.rows || []).map((r) => (Array.isArray(r) ? r : names.map((n) => r?.[n])));
}
const isNum = (v) => typeof v === 'number' ? Number.isFinite(v) : typeof v === 'string' && NUM.test(v.trim());

/** What each column holds: time (date strings or epoch milliseconds), number, text or json. */
export function columnKinds(answer) {
  const names = columnNames(answer);
  const rows = rowArrays(answer);
  return names.map((name, i) => {
    const vals = rows.map((r) => r[i]).filter((v) => v !== null && v !== undefined);
    const distinct = new Set(vals.map((v) => (typeof v === 'object' ? JSON.stringify(v) : String(v)))).size;
    let kind = 'text';
    if (!vals.length) kind = 'empty';
    else if (vals.some((v) => typeof v === 'object')) kind = 'json';
    else if (vals.every((v) => typeof v === 'string' && DATE.test(v))) kind = 'date';
    else if (vals.every(isNum)) kind = EPOCH.test(name) ? 'epoch' : 'number';
    return { name, index: i, kind, distinct };
  });
}

/** The axes of a line chart over an answer, or null when it has none: x a time-like or numeric
 *  column, y numeric columns, by an optional grouping column (one line per value). `picks` are the
 *  address's x, y and by; a pick that is not a usable column is ignored, `by=none` splits nothing. */
export function inferAxes(answer, picks = {}) {
  const kinds = columnKinds(answer);
  const numeric = kinds.filter((k) => k.kind === 'number' || k.kind === 'epoch');
  const timeLike = kinds.filter((k) => k.kind === 'date' || k.kind === 'epoch' || (k.kind === 'number' && ORDERED.test(k.name)));
  const xs = [...timeLike, ...numeric.filter((k) => !timeLike.includes(k))];
  if (!xs.length) return null;
  let x = xs.find((k) => k.name === picks.x);
  if (!x) x = timeLike[0] || (numeric.length > 1 ? numeric[0] : null);
  if (!x) return null;
  const ys = numeric.filter((k) => k !== x);
  if (!ys.length) return null;
  const groupable = kinds.filter((k) => k !== x && (k.kind === 'text' || (k.kind === 'number' && GROUPS.includes(k.name))) && k.distinct >= 2 && k.distinct <= 24);
  let by = null;
  if (picks.by && picks.by !== 'none') by = groupable.find((k) => k.name === picks.by) || null;
  else if (!picks.by) by = GROUPS.map((g) => groupable.find((k) => k.name === g)).find(Boolean) || groupable.find((k) => k.kind === 'text') || null;
  const free = ys.filter((k) => k !== by);
  let y = (picks.y || []).map((n) => free.find((k) => k.name === n)).filter(Boolean);
  if (!y.length) {
    y = free.filter((k) => !IDISH.test(k.name)).slice(0, 4);
    if (!y.length) y = free.slice(0, 1);
  }
  if (!y.length) return null;
  return { x: x.name, xType: x.kind === 'date' || x.kind === 'epoch' ? 'time' : 'value', y: y.map((k) => k.name), by: by ? by.name : '',
    xs: xs.map((k) => k.name), ys: ys.map((k) => k.name), groups: groupable.map((k) => k.name) };
}

const xValue = (v, type) => (v === null || v === undefined ? null : type === 'time' && typeof v === 'string' && !NUM.test(v) ? Date.parse(v) : Number(v));
const yValue = (v) => (v === null || v === undefined || !isNum(v) ? null : Number(v));

/** The line chart of an answer (pure: answer in, ECharts option out), or null when it has no axes.
 *  Every point carries the index of its row, so a click leads to that row of the table. */
export function chartOption(answer, picks = {}) {
  const a = inferAxes(answer, picks);
  if (!a) return null;
  const names = columnNames(answer);
  const rows = rowArrays(answer);
  const xi = names.indexOf(a.x), bi = a.by ? names.indexOf(a.by) : -1;
  const groups = bi < 0 ? [''] : [...new Set(rows.map((r) => String(r[bi] ?? '')))];
  const series = [];
  for (const g of groups) {
    for (const yn of a.y) {
      const yi = names.indexOf(yn);
      const data = rows.map((r, row) => ({ r, row })).filter(({ r }) => bi < 0 || String(r[bi] ?? '') === g)
        .map(({ r, row }) => ({ value: [xValue(r[xi], a.xType), yValue(r[yi])], row }))
        .filter((d) => d.value[0] !== null && !Number.isNaN(d.value[0]))
        .sort((p, q) => p.value[0] - q.value[0]);
      const name = bi < 0 ? yn : a.y.length > 1 ? `${g} · ${yn}` : g;
      series.push({ id: `${g}\u0000${yn}`, name, type: 'line', showSymbol: data.length <= 60, symbolSize: 5, cursor: 'pointer',
        color: PALETTE.dark[series.length % PALETTE.dark.length], data });
    }
  }
  return {
    backgroundColor: 'transparent',
    tooltip: { trigger: 'axis', valueFormatter: (v) => (v == null ? '-' : whole.format(v)) },
    legend: { type: 'scroll', top: 0, textStyle: { color: INK.text }, pageTextStyle: { color: INK.muted } },
    grid: { left: 8, right: 24, top: 36, bottom: 48, containLabel: true },
    xAxis: { type: a.xType, name: a.x, nameLocation: 'middle', nameGap: 28, nameTextStyle: { color: INK.muted }, min: 'dataMin', max: 'dataMax', ...axis, splitLine: { show: false } },
    yAxis: { type: 'value', name: a.y.length === 1 ? a.y[0] : '', nameTextStyle: { color: INK.muted, align: 'left' }, scale: true, ...axis, axisLabel: { color: INK.muted, formatter: (v) => compact.format(v) } },
    dataZoom: [{ type: 'inside', zoomOnMouseWheel: 'ctrl', moveOnMouseWheel: false }, { type: 'slider', height: 18, bottom: 4, borderColor: INK.grid }],
    series,
  };
}

/** A cell as text: NULL, JSON for objects, the value otherwise. */
export const cellText = (v) => (v === null || v === undefined ? 'NULL' : typeof v === 'object' ? JSON.stringify(v) : String(v));

/* ---- the bus ------------------------------------------------------------------------------- */
/** host and port of a tcp:// endpoint. */
export function parseEndpoint(endpoint) {
  const m = /^tcp:\/\/([^:/]+|\[[^\]]+\]):(\d+)$/.exec(String(endpoint || ''));
  return m ? { host: m[1].replace(/^\[|\]$/g, ''), port: Number(m[2]) } : null;
}

/** The topics a bus has seen as [{topic, count}], from a list or a map of topic to count. */
export function busTopics(bus) {
  const t = bus?.topics_seen;
  if (Array.isArray(t)) return t.map((x) => (typeof x === 'string' ? { topic: x, count: null } : { topic: String(x.topic ?? x.kind ?? x.name), count: x.count ?? null }));
  if (t && typeof t === 'object') return Object.entries(t).map(([topic, count]) => ({ topic, count: typeof count === 'number' ? count : null })).sort((a, b) => (b.count ?? 0) - (a.count ?? 0));
  return [];
}

/** How to subscribe to the bus from four languages; `topic` is the prefix filter ('' for every event). */
export function busSnippets(endpoint, topic = '') {
  const ep = endpoint || 'tcp://127.0.0.1:<port>';
  const hp = parseEndpoint(ep) || { host: '127.0.0.1', port: '<port>' };
  const q = JSON.stringify(topic);
  return [
    { id: 'rust', label: 'Rust (zeromq crate)', install: 'cargo add zeromq serde_json && cargo add tokio --features full', code:
`use zeromq::{Socket, SocketRecv, SubSocket};

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let mut sock = SubSocket::new();
    sock.connect("${ep}").await?;
    sock.subscribe(${q}).await?;
    loop {
        let msg = sock.recv().await?;
        let topic = String::from_utf8_lossy(msg.get(0).unwrap());
        let envelope: serde_json::Value = serde_json::from_slice(msg.get(1).unwrap())?;
        println!("{topic} {envelope}");
    }
}` },
    { id: 'python', label: 'Python (pyzmq)', install: 'pip install pyzmq', code:
`import json
import zmq

sock = zmq.Context().socket(zmq.SUB)
sock.connect("${ep}")
sock.setsockopt_string(zmq.SUBSCRIBE, ${q})
while True:
    topic, envelope = sock.recv_multipart()
    print(topic.decode(), json.loads(envelope))` },
    { id: 'node', label: 'Node (zeromq)', install: 'yarn add zeromq', code:
`import { Subscriber } from 'zeromq';

const sock = new Subscriber();
sock.connect('${ep}');
sock.subscribe(${topic ? `'${topic.replace(/'/g, "\\'")}'` : ''});
for await (const [topic, envelope] of sock) {
  console.log(topic.toString(), JSON.parse(envelope.toString()));
}` },
    { id: 'elixir', label: 'Elixir (chumak)', install: '{:chumak, "~> 1.4"} in mix.exs deps', code:
`{:ok, sock} = :chumak.socket(:sub)
:ok = :chumak.subscribe(sock, ${q})
{:ok, _peer} = :chumak.connect(sock, :tcp, ~c"${hp.host}", ${hp.port})

recv = fn recv ->
  {:ok, [topic, envelope]} = :chumak.recv_multipart(sock)
  IO.inspect(Jason.decode!(envelope), label: topic)
  recv.(recv)
end
recv.(recv)` },
  ];
}

/* ---- the page ------------------------------------------------------------------------------- */
function page() {
  const echarts = globalThis.echarts;
  const state = getState();
  let tables = ADR_SCHEMA;
  let answer = null;
  let chart = null;

  const renderExamples = () => {
    $('example-list').innerHTML = exampleQueries(tables).map((e) => `<li><a href="${esc(e.href)}" data-example="${esc(e.id)}" class="block p-2 rounded-lg text-primary-600 hover:bg-gray-100 hover:underline dark:text-primary-500 dark:hover:bg-gray-700" title="${esc(e.sql)}">${esc(e.label)}</a></li>`).join('');
    $('table-list').innerHTML = tables.map((t) => `<li><a href="${esc(queryHref(`SELECT * FROM ${ident(t.name)} LIMIT 50`))}" data-sql="${esc(`SELECT * FROM ${ident(t.name)} LIMIT 50`)}" class="font-mono font-medium text-gray-900 hover:underline dark:text-white">${esc(t.name)}</a>
      <span class="text-xs text-gray-500 dark:text-gray-400">${t.columns.map((c) => esc(c.name) + (c.type ? ` <span class="text-gray-400">${esc(c.type.toLowerCase())}</span>` : '')).join(', ')}</span></li>`).join('');
  };

  const fill = (sel, options, value) => {
    sel.innerHTML = options.map(([v, label]) => `<option value="${esc(v)}"${v === value ? ' selected' : ''}>${esc(label)}</option>`).join('');
    sel.disabled = options.length <= 1;
  };

  const drawChart = () => {
    const box = $('data-chart');
    const empty = $('data-chart-empty');
    const axes = answer && inferAxes(answer, state);
    if (!axes) {
      fill($('chart-x'), [['', 'across: -']], ''); fill($('chart-y'), [['', 'up: -']], ''); fill($('chart-by'), [['', 'lines: -']], '');
      empty.textContent = answer ? 'No chart for this answer: it needs a time-like or numeric column and another numeric one.' : 'Run a query to chart its answer.';
      empty.classList.remove('hidden');
      box.classList.add('hidden');
      if (chart) chart.clear();
      return;
    }
    fill($('chart-x'), axes.xs.map((n) => [n, `across: ${n}`]), axes.x);
    fill($('chart-y'), [['', `up: ${axes.y.join(', ')}`], ...axes.ys.filter((n) => n !== axes.by).map((n) => [n, `up: ${n}`])], state.y.length === 1 ? state.y[0] : '');
    fill($('chart-by'), [['none', 'one line per: nothing'], ...axes.groups.map((n) => [n, `one line per: ${n}`])], axes.by || 'none');
    $('chart-by').disabled = !axes.groups.length;
    empty.classList.add('hidden');
    box.classList.remove('hidden');
    chart = chart || ecInit(box, echarts);
    if (!chart) return;
    chart.setOption(chartOption(answer, state), true);
    if (!chart._wired) {
      chart._wired = true;
      chart.on('click', (p) => {
        const tr = document.querySelector(`#data-rows tr[data-row="${p.data?.row}"]`);
        if (!tr) return;
        document.querySelectorAll('#data-rows tr[data-picked]').forEach((x) => { x.removeAttribute('data-picked'); x.classList.remove('bg-primary-50', 'dark:bg-primary-900/40'); });
        tr.setAttribute('data-picked', '');
        tr.classList.add('bg-primary-50', 'dark:bg-primary-900/40');
        tr.scrollIntoView({ block: 'center', behavior: 'smooth' });
      });
    }
    chart.resize();
  };

  const renderTable = () => {
    const names = columnNames(answer);
    const rows = rowArrays(answer);
    $('data-head').innerHTML = `<tr>${names.map((n) => `<th scope="col" class="px-3 py-2 whitespace-nowrap">${esc(n)}</th>`).join('')}</tr>`;
    $('data-rows').innerHTML = rows.map((r, i) => `<tr data-row="${i}" class="bg-white border-b dark:bg-gray-800 dark:border-gray-700">${r.map((v, j) => {
      const text = cellText(v);
      const link = cellLink(names[j], v, tables);
      const shown = text.length > 160 ? text.slice(0, 160) + '…' : text;
      const inner = v === null || v === undefined ? '<span class="text-gray-400 dark:text-gray-500">NULL</span>'
        : link ? `<a href="${esc(link)}" data-sql-link class="font-medium text-primary-600 hover:underline dark:text-primary-500">${esc(shown)}</a>` : esc(shown);
      return `<td class="px-3 py-2 align-top${isNum(v) ? ' text-right tabular-nums' : ''}${typeof v === 'object' && v !== null || text.length > 40 ? ' font-mono text-xs' : ''}"${text.length > 160 ? ` title="${esc(text.slice(0, 2000))}"` : ''}>${inner}</td>`;
    }).join('')}</tr>`).join('') || `<tr><td class="px-3 py-2" colspan="${Math.max(1, names.length)}">no rows</td></tr>`;
    const count = answer.count ?? rows.length;
    $('result-count').textContent = `(${whole.format(count)} row${count === 1 ? '' : 's'})`;
    $('capped').classList.toggle('hidden', !answer.capped);
    $('capped').textContent = answer.capped ? `The answer is capped at ${whole.format(rows.length)} rows; add a LIMIT or an aggregate to see what you need.` : '';
  };

  const run = async (sql, { fresh = true } = {}) => {
    sql = String(sql || '').trim();
    if (!sql) return;
    $('sql').value = sql;
    if (fresh && sql !== state.sql) { state.x = ''; state.y = []; state.by = ''; }
    state.sql = sql;
    setState({ sql, x: state.x, y: state.y, by: state.by });
    $('run-msg').textContent = 'running…';
    $('query-error').classList.add('hidden');
    const t0 = performance.now();
    let r, d;
    try {
      r = await fetch('/api/db/query', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ sql }) });
      d = await r.json().catch(() => ({ error: `HTTP ${r.status}` }));
    } catch (e) { r = { ok: false, status: 0 }; d = { error: String(e) }; }
    if (!r.ok || d.error) {
      const missing = r.status === 404 ? 'This panel does not serve the event store yet (no /api/db/query; ADR 0033). ' : '';
      $('query-error').textContent = missing + (d.error || `HTTP ${r.status}`);
      $('query-error').classList.remove('hidden');
      $('run-msg').textContent = '';
      answer = null;
      $('data-head').innerHTML = ''; $('data-rows').innerHTML = ''; $('result-count').textContent = ''; $('capped').classList.add('hidden');
      drawChart();
      return;
    }
    answer = d;
    $('run-msg').textContent = `${whole.format(d.count ?? (d.rows || []).length)} row(s) in ${Math.round(performance.now() - t0)} ms`;
    renderTable();
    drawChart();
  };

  // Links to a query on this page (examples, tables, cells, topics) run in place, the address kept.
  document.getElementById('page').addEventListener('click', (e) => {
    const a = e.target.closest('a[href^="/data?sql="]');
    if (!a || e.ctrlKey || e.metaKey || e.shiftKey || e.button !== 0) return;
    e.preventDefault();
    run(new URL(a.href).searchParams.get('sql'));
    $('query').scrollIntoView({ block: 'start', behavior: 'smooth' });
  });
  $('query-form').onsubmit = (e) => { e.preventDefault(); run($('sql').value); };
  $('sql').addEventListener('keydown', (e) => { if (e.key === 'Enter' && (e.ctrlKey || e.metaKey)) { e.preventDefault(); run($('sql').value); } });
  $('chart-x').onchange = (e) => { state.x = e.target.value; setState({ x: state.x }); drawChart(); };
  $('chart-y').onchange = (e) => { state.y = e.target.value ? [e.target.value] : []; setState({ y: state.y }); drawChart(); };
  $('chart-by').onchange = (e) => { state.by = e.target.value; setState({ by: state.by }); drawChart(); };
  const card = $('data-chart-card');
  card.querySelector('[data-toolbar]').append(maximizable(card, 'data-chart', { getState: () => getState(), setState, onResize: () => drawChart() }));

  const schema = async () => {
    try {
      const r = await fetch('/api/db/tables');
      if (r.ok) { const t = normalizeSchema(await r.json()); if (t.length) tables = t; }
    } catch { /* the ADR's schema stands in */ }
    renderExamples();
  };

  const bus = async () => {
    let b = null;
    try { const r = await fetch('/api/bus'); if (r.ok) b = await r.json(); } catch { /* not served */ }
    $('bus-endpoint').textContent = b?.endpoint || 'not served by this panel yet';
    $('bus-schema').textContent = b?.schema || '-';
    $('bus-published').textContent = b?.published == null ? '-' : whole.format(b.published);
    const topics = busTopics(b);
    $('bus-topics').innerHTML = topics.map((t) => `<a href="${esc(cellLink('kind', t.topic, tables) || queryHref(`SELECT * FROM events WHERE kind = ${lit(t.topic)} LIMIT 50`))}" class="${UI.badge || ''} ${TONE.gray} hover:underline">${esc(t.topic)}${t.count == null ? '' : ` <span class="text-gray-500 dark:text-gray-400">${esc(whole.format(t.count))}</span>`}</a>`).join('') || 'none yet';
    $('bus-snippets').innerHTML = busSnippets(b?.endpoint).map((s) => `<div class="p-3 rounded-lg bg-gray-50 dark:bg-gray-700/50 min-w-0">
      <div class="flex flex-wrap items-center justify-between gap-2 mb-2"><div><h4 class="text-sm font-semibold text-gray-900 dark:text-white">${esc(s.label)}</h4><p class="font-mono text-xs text-gray-500 dark:text-gray-400">${esc(s.install)}</p></div>
      <button type="button" data-copy="${esc(s.id)}" class="${UI.secondary || ''}">Copy</button></div>
      <pre class="p-3 overflow-x-auto text-xs bg-gray-900 rounded-lg text-gray-100"><code id="snippet-${esc(s.id)}">${esc(s.code)}</code></pre></div>`).join('');
  };
  $('bus-snippets').addEventListener('click', async (e) => {
    const b = e.target.closest('[data-copy]');
    if (!b) return;
    const text = $('snippet-' + b.dataset.copy).textContent;
    try { await navigator.clipboard.writeText(text); b.textContent = 'Copied'; } catch { b.textContent = 'Select and copy'; }
    setTimeout(() => { b.textContent = 'Copy'; }, 1500);
  });

  renderExamples();
  drawChart();
  schema().then(() => {
    const first = exampleQueries(tables).find((e) => e.id === 'latest-events') || exampleQueries(tables)[0];
    if (state.sql) run(state.sql, { fresh: false }); else if (first) $('sql').value = first.sql;
  });
  bus();
}

if (globalThis.document && document.getElementById('data-chart')) page();
