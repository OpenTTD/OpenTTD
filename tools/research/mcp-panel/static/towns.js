/* The town authority in the panel (research/mechanisms/town-authority.md): what a town's local
   authority rewards and punishes, what it refuses below which rating, the town actions and the
   rules that tie a town to a company's stations, all from the model derived from the engine sources
   (GET /api/town/model, tools/research/town-model/town_model.json), each rule with its source
   path:line; and, when a game is connected, every live town with one company's rating, band and the
   advice of town_lib.rhai (GET /api/town/live?company=N). The company lives in the address. */

import { UI, CLS, $, esc, whole, badge, patchAddress } from './components.js';

/* ---- address state ------------------------------------------------------------------------- */
export function getState(href = globalThis.location?.href) {
  const c = new URL(href).searchParams.get('company');
  return { company: c != null && /^\d+$/.test(c) ? Number(c) : undefined };
}

/* ---- pure helpers: model in, rows out ------------------------------------------------------- */

/** A rule's source as text: "path:line" as the model gives it, or the empty string. */
export const sourceText = (s) => (s == null ? '' : String(s));

/** A source cell: monospaced path:line. */
const src = (s) => `<code class="text-xs text-gray-500 dark:text-gray-400 whitespace-nowrap">${esc(sourceText(s))}</code>`;

/** A signed number: +7, -35. */
export const signed = (n) => (typeof n === 'number' ? (n > 0 ? `+${n}` : String(n)) : '');

/** What the town rewards and punishes: every effect with a numeric delta, split by its sign, the
 *  largest change first. Effects without a delta (resets, per-house values) are listed apart. */
export function likesDislikes(model) {
  const row = (e) => ({ trigger: e.trigger, function: e.function, delta: e.delta, limit: e.limit, limit_expr: e.limit_expr,
    delta_expr: e.delta_expr, hint: e.hint, source: e.source });
  const effects = model.effects || [];
  const num = effects.filter((e) => typeof e.delta === 'number');
  return {
    likes: num.filter((e) => e.delta > 0).map(row).sort((a, b) => b.delta - a.delta),
    dislikes: num.filter((e) => e.delta < 0).map(row).sort((a, b) => a.delta - b.delta),
    other: effects.filter((e) => typeof e.delta !== 'number').map((e) => ({ ...row(e), kind: e.kind, to: e.to, candidates: e.delta_candidates || [] })),
  };
}

/** The removal thresholds by tolerance: {checks, tolerances, cell(check, tolerance) -> gate}. */
export function removalMatrix(model) {
  const g = (model.gates || []).filter((x) => x.check && x.tolerance);
  const checks = [...new Set(g.map((x) => x.check))];
  const tolerances = [...new Set(g.map((x) => x.tolerance))];
  const at = new Map(g.map((x) => [`${x.check}/${x.tolerance}`, x]));
  return { checks, tolerances, cell: (c, t) => at.get(`${c}/${t}`) };
}

/** The gates that are not removal-by-tolerance thresholds. */
export const plainGates = (model) => (model.gates || []).filter((x) => !(x.check && x.tolerance));

/** The relations grouped by their source node, nodes in first-seen order. */
export function relationGroups(model) {
  const groups = new Map();
  for (const r of model.relations || []) {
    if (!groups.has(r.from)) groups.set(r.from, []);
    groups.get(r.from).push(r);
  }
  return [...groups].map(([node, edges]) => ({ node, edges }));
}

/** The company choices of a live answer: the game's companies, else the ids with ratings. */
export function companyChoices(live) {
  const cs = (live.companies || []).map((c) => ({ id: c.id, name: c.name || `company ${c.id}` }));
  if (cs.length) return cs;
  return (live.rated_companies || []).map((id) => ({ id, name: `company ${id}` }));
}

/* ---- renderers: data in, HTML out ----------------------------------------------------------- */

const head = (title, sub, actions = '') => `<div class="${UI.cardHead} mb-4"><div class="${UI.cardHeadTitle}"><h2 class="${UI.cardTitle}">${esc(title)}</h2>${sub ? `<p class="mt-2 ${UI.cardSub}">${sub}</p>` : ''}</div>${actions ? `<div class="flex flex-wrap items-center gap-2">${actions}</div>` : ''}</div>`;
const api = (op) => `<a href="/docs#/default/${op}" class="text-sm text-primary-600 hover:underline dark:text-primary-500">API</a>`;
const table = (cols, rows, empty = 'nothing') => `<div class="${UI.tableWrap}"><table class="${UI.table}"><thead class="${UI.thead}"><tr>${cols.map((c) => `<th class="${CLS.cell}">${esc(c)}</th>`).join('')}</tr></thead>
  <tbody>${rows.length ? rows.join('') : `<tr><td class="${CLS.cell}" colspan="${cols.length}">${esc(empty)}</td></tr>`}</tbody></table></div>`;
const tr = (cells) => `<tr class="border-b dark:border-gray-700">${cells.map((c) => `<td class="${CLS.cell} align-top">${c}</td>`).join('')}</tr>`;
const card = (inner) => `<div class="${CLS.card}">${inner}</div>`;
const MODEL = 'from tools/research/town-model/town_model.json (derive.py over the engine sources; research/mechanisms/town-authority.md)';

/** Likes and dislikes: trigger, delta, limit and source of every rating change. */
export function likesHtml(model) {
  const { likes, dislikes, other } = likesDislikes(model);
  const rows = (xs, tone) => xs.map((e) => tr([
    `<span class="text-gray-900 dark:text-white">${esc(e.trigger)}</span><div class="${CLS.muted}">${esc(e.function)}${e.hint ? ' · ' + esc(e.hint) : ''}</div>`,
    badge(`${signed(e.delta)} (${e.delta_expr})`, tone),
    e.limit == null ? '' : `${esc(e.limit)} <span class="${CLS.muted}">${esc(e.limit_expr || '')}</span>`,
    src(e.source)]));
  const cols = ['Trigger', 'Delta', 'Stops at', 'Source'];
  const others = other.map((e) => tr([
    `<span class="text-gray-900 dark:text-white">${esc(e.trigger)}</span><div class="${CLS.muted}">${esc(e.function)}${e.hint ? ' · ' + esc(e.hint) : ''}</div>`,
    `${esc(e.kind)} ${e.to != null ? `to ${esc(e.to)}` : esc(e.delta_expr)}${e.candidates.length ? `<div class="${CLS.muted}">${e.candidates.map((c) => `${esc(c.expr)}${c.value != null ? ' = ' + esc(c.value) : ''}${c.note ? ' (' + esc(c.note) + ')' : ''}`).join('; ')}</div>` : ''}`,
    e.limit == null ? '' : `${esc(e.limit)} <span class="${CLS.muted}">${esc(e.limit_expr || '')}</span>`,
    src(e.source)]));
  return card(`${head('What the town likes and dislikes', `Every change of a company's rating in a town, ${esc(MODEL)}. A change stops at its limit and never crosses it.`, api('townModel'))}
    <h3 class="${CLS.label}">Likes (${likes.length})</h3>${table(cols, rows(likes, 'green'))}
    <h3 class="mt-4 ${CLS.label}">Dislikes (${dislikes.length})</h3>${table(cols, rows(dislikes, 'red'))}
    <h3 class="mt-4 ${CLS.label}">Set, reset or per house (${other.length})</h3>${table(['Trigger', 'Change', 'Stops at', 'Source'], others)}`);
}

/** Gates: what the authority refuses below which rating, and removal thresholds by tolerance. */
export function gatesHtml(model) {
  const rows = plainGates(model).map((x) => tr([
    `<span class="text-gray-900 dark:text-white">${esc(x.function)}</span><div class="${CLS.muted}">${esc(x.guards)}${x.hint ? ' · ' + esc(x.hint) : ''}</div>`,
    `<code>rating ${esc(x.op)} ${esc(x.threshold ?? x.threshold_expr)}</code><div class="${CLS.muted}">${esc(x.threshold_expr)}</div>`,
    src(x.source)]));
  const m = removalMatrix(model);
  const mrows = m.checks.map((c) => tr([`<span class="text-gray-900 dark:text-white">${esc(c)}</span>`,
    ...m.tolerances.map((t) => { const g = m.cell(c, t); return g ? `<span title="${esc(g.threshold_expr)} · ${esc(g.source)}">${esc(g.op)} ${esc(g.threshold)}</span><div>${src(g.source)}</div>` : ''; })]));
  return card(`${head('Gates: what the authority refuses', `Each comparison of a company's rating with a threshold: below it, the action is refused. ${esc(MODEL)}.`, api('townModel'))}
    ${table(['Where', 'Allowed when', 'Source'], rows)}
    <h3 class="mt-4 ${CLS.label}">Removing town roads, tunnels and bridges, by difficulty.town_council_tolerance</h3>
    ${table(['Check', ...m.tolerances], mrows)}`);
}

/** Town actions: cost factor and every effect. */
export function actionsHtml(model) {
  const rows = (model.actions || []).map((a) => tr([
    `<span class="text-gray-900 dark:text-white">${esc(a.name)}</span><div class="${CLS.muted}">${esc(a.doc)}</div>`,
    `${esc(a.cost_factor)}<div class="${CLS.muted}">${esc(a.cost)}</div>`,
    `<ul class="space-y-1">${(a.effects || []).map((e) => `<li><code>${esc(e.field)}</code> ${esc(e.value)}${e.radius != null ? ` within ${esc(e.radius)} tiles` : ''}${e.note ? ` <span class="${CLS.muted}">${esc(e.note)}</span>` : ''} ${src(e.source)}</li>`).join('')}</ul>`,
    `${src(a.source)}<div>${src(a.proc_source)}</div>`]));
  return card(`${head('Town actions', `What a company can buy from the local authority: the cost is the base town-action price times the factor, shifted right by 8. ${esc(MODEL)}.`, api('townModel'))}
    ${table(['Action', 'Cost factor', 'Effects', 'Source'], rows)}`);
}

/** Links: statue, exclusivity, noise, station service and the growth table; then the settings. */
export function linksHtml(model) {
  const links = model.links || [];
  const rows = links.map((l) => tr([`<code>${esc(l.id)}</code>`, esc(l.effect),
    esc([l.value != null ? `value ${l.value}` : '', l.periods != null ? `${l.periods} periods` : '', l.setting ? `setting ${l.setting}` : ''].filter(Boolean).join(' · ')),
    src(l.source)]));
  const growth = links.find((l) => Array.isArray(l.normal));
  const gt = growth ? `<h3 class="mt-4 ${CLS.label}">Town ticks between growth steps by active stations</h3>
    ${table(['Active stations', ...growth.normal.map((_, i) => (i === growth.normal.length - 1 ? `${i}+` : String(i)))], [
      tr([esc(growth.normal_doc || 'normal'), ...growth.normal.map((v) => esc(v))]),
      ...(growth.funded ? [tr([esc(growth.funded_doc || 'funded'), ...growth.funded.map((v) => esc(v))])] : [])])}
    <p class="mt-1 ${CLS.muted}">Fewer ticks is faster growth. ${src(growth.source)}</p>` : '';
  const settings = Object.entries(model.settings || {}).map(([k, s]) => tr([`<code>${esc(k)}</code>`, esc(s.default),
    esc(s.min != null ? `${s.min}..${s.max}` : ''), src(s.source)]));
  return card(`${head('Links between towns and stations', `Rules that tie a town to a company's stations. ${esc(MODEL)}.`, api('townModel'))}
    ${table(['Rule', 'Effect', 'Values', 'Source'], rows)}${gt}
    <h3 class="mt-4 ${CLS.label}">Settings these rules read</h3>${table(['Setting', 'Default', 'Range', 'Source'], settings)}`);
}

/** Relations: the model's graph, one block per source node. */
export function relationsHtml(model) {
  const groups = relationGroups(model);
  const rows = groups.flatMap((g) => g.edges.map((r, i) => tr([
    i === 0 ? `<code class="text-gray-900 dark:text-white">${esc(g.node)}</code>` : '',
    badge(r.kind, { raises: 'green', lowers: 'red', allows: 'blue', enables: 'blue', sets: 'yellow' }[r.kind] || 'gray'),
    `<code>${esc(r.to)}</code>`, esc(r.label || ''), src(r.source)])));
  return card(`${head('Relations', `The graph the model implies: trigger → rating → gate, action → effect (${groups.length} nodes, ${(model.relations || []).length} edges).`, api('townModel'))}
    ${table(['From', 'Kind', 'To', 'Label', 'Source'], rows)}`);
}

/** One advice item: what, with why and the rule's source on hover and under it. */
export const adviceItem = (a) => `<li title="${esc(`${a.why} (${sourceText(a.source)})`)}"><span class="text-gray-900 dark:text-white">${esc(a.what)}</span>
  <div class="${CLS.muted}">${esc(a.why)} · ${src(a.source)}</div></li>`;

/** The live towns table rows. */
export function liveRows(live) {
  return (live.towns || []).map((t) => tr([
    `<span class="font-medium text-gray-900 dark:text-white">${esc(t.name)}</span><div class="${CLS.muted}">#${esc(t.id)} · tile ${esc(t.x)},${esc(t.y)} · pop. ${esc(whole.format(t.population ?? 0))}</div>`,
    t.rating == null ? `<span title="the game does not publish ratings yet: advice reads the rating as at least ${esc(t.rating_floor)}">unknown</span>` : esc(t.rating),
    t.band ? badge(String(t.band).replace(/^TOWN_RATING_/, ''), /POOR|APPALLING/.test(t.band) ? 'red' : 'gray') : '',
    `<ul class="space-y-1">${(t.advice || []).map(adviceItem).join('')}</ul>`]));
}

/** The live card: the company selector, tolerance, fields the game publishes, and the table. */
export function liveHtml(live, company) {
  const choices = companyChoices(live);
  const sel = `<label class="text-sm font-medium text-gray-900 dark:text-white">Company
    <select id="t-company" class="${UI.select} ms-2"><option value="">${choices.length ? 'first rated' : 'none'}</option>
    ${choices.map((c) => `<option value="${esc(c.id)}"${c.id === company ? ' selected' : ''}>${esc(c.name)} (#${esc(c.id)})</option>`).join('')}</select></label>`;
  const sub = live.reason
    ? `No live towns: ${esc(live.reason)}.`
    : `${esc(live.count)} towns from openttd://towns; advice from town_lib.rhai advice(town, tolerance), run in the panel. Tolerance ${badge(live.tolerance || 'lenient', 'blue')} (${esc(live.tolerance_source || '')}). Fields the game publishes: ${esc((live.fields || []).join(', ') || 'none beyond id, name, tile and population')}.${live.advice_error ? ' Advice failed: ' + esc(live.advice_error) : ''}`;
  return card(`${head('Live towns', sub, `${sel}${api('townLive')}`)}
    ${table(['Town', 'Rating', 'Band', 'Advice (hover for why and rule)'], liveRows(live), live.reason ? 'no game connected' : 'no towns')}`);
}

/* ---- the page ------------------------------------------------------------------------------- */
async function loadLive(company) {
  const q = company == null ? '' : `?company=${company}`;
  const live = await (await fetch('/api/town/live' + q)).json();
  $('live').innerHTML = liveHtml(live, company ?? live.company);
  $('t-company').onchange = (ev) => {
    const v = ev.target.value === '' ? undefined : Number(ev.target.value);
    patchAddress({ company: v });
    loadLive(v);
  };
  return live;
}

/** The whole page; exported so a test mounts it in the fake browser (test/towns.test.mjs). */
export async function mount() {
  const state = getState();
  const r = await fetch('/api/town/model');
  if (r.ok) {
    const model = await r.json();
    $('likes').innerHTML = likesHtml(model);
    $('gates').innerHTML = gatesHtml(model);
    $('actions').innerHTML = actionsHtml(model);
    $('links').innerHTML = linksHtml(model);
    $('relations').innerHTML = relationsHtml(model);
    $('t-msg').textContent = `Model ${model.schema} from source revision ${model.source_rev}, generated by ${model.generated_by}.`;
  } else {
    $('t-msg').textContent = `The town model is missing (${(await r.json()).error || r.status}).`;
  }
  await loadLive(state.company);
}

if (globalThis.document && $('live') && $('likes')) mount();
