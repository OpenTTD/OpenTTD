/* My bot: the AI that plays for the owner, its settings and the bots already running. Every field
   comes from what the game declares (GET /api/bots: openttd.ai.list, or the AI's info.nut), never
   listed by hand, and is laid out by its name:
   - `<base><N>_<field>` names with at least two line numbers form one table of lines (the bare
     `<base>_<field>` is line 1), one row per line; a line whose every field is -1 is unused and
     folds away;
   - the other settings group by their first word (air_a, air_b, air_planes -> air), and those that
     share it with nobody form "general".
   A running bot is changed through one Apply that asks for exactly the keys that differ; each key
   is its own approval (openttd.ai.set_setting), so the button says how many it will ask. */

const $ = (id) => document.getElementById(id);
const h = (v) => String(v ?? '').replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
const whole = new Intl.NumberFormat('en');

const LINE = /^([a-z]+?)(\d*)_([a-z]+)$/;
const INPUT = 'bg-gray-50 border border-gray-300 text-gray-900 text-sm rounded-lg focus:ring-primary-500 focus:border-primary-500 block dark:bg-gray-700 dark:border-gray-600 dark:text-white data-[dirty=true]:border-yellow-400 data-[dirty=true]:ring-1 data-[dirty=true]:ring-yellow-400 disabled:opacity-60';
const BTN = 'text-white bg-primary-700 hover:bg-primary-800 focus:ring-4 focus:ring-primary-300 font-medium rounded-lg text-sm px-4 py-2 dark:bg-primary-600 dark:hover:bg-primary-700 focus:outline-none dark:focus:ring-primary-800 disabled:opacity-50 disabled:cursor-not-allowed';
const BTN_ALT = 'text-gray-900 bg-white border border-gray-300 hover:bg-gray-100 focus:ring-4 focus:ring-gray-100 font-medium rounded-lg text-sm px-4 py-2 dark:bg-gray-800 dark:text-white dark:border-gray-600 dark:hover:bg-gray-700 dark:focus:ring-gray-700 disabled:opacity-50 disabled:cursor-not-allowed';
const badge = (tone, text) => `<span class="bg-${tone}-100 text-${tone}-800 text-xs font-medium px-1.5 py-0.5 rounded-sm dark:bg-${tone}-900 dark:text-${tone}-300">${h(text)}</span>`;

/* ---- layout, derived from the names --------------------------------------------------------- */

/** Which world list a setting picks from, read from its description ("(industry id; -1: none)"). */
export const pickerOf = (st) => { const m = /\((town|industry|cargo) id/.exec(st.description || ''); return m ? m[1] : null; };

/** The control a setting gets: a boolean flag toggles, labels select, a world id picks from the live
 *  world when the panel has it, a short range slides, anything else is a number. */
export function kindOf(st, choices = {}) {
  if ((st.flags || []).includes('boolean')) return 'toggle';
  if (Object.keys(st.labels || {}).length) return 'select';
  if (pickerOf(st) && (choices[pickerOf(st)] || []).length) return 'picker';
  return st.max - st.min > 0 && (st.max - st.min) / (st.step || 1) <= 2000 ? 'range' : 'number';
}

/** Settings laid out by name: { groups: [{ title, settings }], tables: [{ base, fields, rows }] }.
 *  A name the game declares twice is shown once (the first). */
export function layout(settings) {
  const seen = new Set();
  const list = settings.filter((st) => !seen.has(st.name) && seen.add(st.name));
  const bases = new Map();
  for (const st of list) {
    const m = LINE.exec(st.name);
    if (!m) continue;
    const n = m[2] ? Number(m[2]) : 1;
    if (!bases.has(m[1])) bases.set(m[1], new Map());
    const lines = bases.get(m[1]);
    if (!lines.has(n)) lines.set(n, {});
    lines.get(n)[m[3]] = st;
  }
  const tables = [];
  const inTable = new Set();
  for (const [base, lines] of bases) {
    if (lines.size < 2) continue;
    const fields = [];
    for (const row of lines.values()) for (const f of Object.keys(row)) if (!fields.includes(f)) fields.push(f);
    const rows = [...lines.entries()].sort((a, b) => a[0] - b[0]).map(([n, cells]) => {
      for (const st of Object.values(cells)) inTable.add(st.name);
      return { n, cells, unset: Object.values(cells).every((st) => Number(st.value ?? st.default) === -1) };
    });
    tables.push({ base, fields, rows });
  }
  const rest = list.filter((st) => !inTable.has(st.name));
  const word = (st) => st.name.split('_')[0];
  const count = new Map();
  for (const st of rest) count.set(word(st), (count.get(word(st)) || 0) + 1);
  const groups = [];
  const general = { title: 'general', settings: [] };
  for (const st of rest) {
    if (count.get(word(st)) < 2) { general.settings.push(st); continue; }
    let g = groups.find((x) => x.title === word(st));
    if (!g) groups.push(g = { title: word(st), settings: [] });
    g.settings.push(st);
  }
  if (general.settings.length) groups.unshift(general);
  return { groups, tables };
}

/** The keys whose value in `values` differs from the setting's current value, as asks. */
export function changes(settings, values) {
  return settings.filter((st) => st.name in values && values[st.name] !== null && Number(values[st.name]) !== Number(st.value))
    .map((st) => ({ name: st.name, from: Number(st.value), to: Number(values[st.name]) }));
}

/* ---- markup --------------------------------------------------------------------------------- */

const optionLabel = (kind, o) => (kind === 'town' ? `${o.name} (#${o.id}, ${whole.format(Number(o.population || 0))} people)`
  : kind === 'industry' ? `${o.name} (#${o.id}${o.produces && o.produces.length ? ', makes ' + o.produces.join('/') : ''})` : `${o.name} (#${o.id})`);

/** One setting's control alone (no label): `key` scopes ids and attributes to a bot or the form. */
export function controlMarkup(st, key, value, { disabled = false, choices = {}, compact = false } = {}) {
  const id = `set-${key}-${st.name}`.replace(/[^a-zA-Z0-9_-]/g, '_');
  const kind = kindOf(st, choices);
  const dis = disabled ? 'disabled' : '';
  const attrs = `id="${id}" data-setting="${h(st.name)}" data-kind="${kind}" data-key="${h(key)}" aria-label="${h(st.name)}" title="${h(st.description || st.name)}"`;
  const pad = compact ? 'py-1.5 ps-2.5 pe-8' : 'py-2 ps-3 pe-9';
  if (kind === 'toggle') {
    return `<label class="inline-flex items-center cursor-pointer"><input type="checkbox" class="sr-only peer" ${Number(value) ? 'checked' : ''} ${dis} ${attrs}>
      <div class="relative w-11 h-6 bg-gray-200 peer-focus:outline-none peer-focus:ring-4 peer-focus:ring-primary-300 dark:peer-focus:ring-primary-800 rounded-full peer dark:bg-gray-700 peer-checked:after:translate-x-full rtl:peer-checked:after:-translate-x-full peer-checked:after:border-white after:content-[''] after:absolute after:top-[2px] after:start-[2px] after:bg-white after:border-gray-300 after:border after:rounded-full after:h-5 after:w-5 after:transition-all dark:border-gray-600 peer-checked:bg-primary-600"></div>
      <span class="ms-3 text-sm text-gray-900 dark:text-gray-300" data-toggle-text>${Number(value) ? 'on' : 'off'}</span></label>`;
  }
  if (kind === 'select') {
    return `<select class="${INPUT} w-full ${pad}" ${dis} ${attrs}>${Object.entries(st.labels).map(([v, l]) => `<option value="${h(v)}" ${Number(v) === Number(value) ? 'selected' : ''}>${h(l)}</option>`).join('')}</select>`;
  }
  if (kind === 'picker') {
    const pk = pickerOf(st);
    const opts = choices[pk] || [];
    const known = opts.some((o) => o.id === Number(value));
    return `<select class="${INPUT} w-full ${pad}" ${dis} ${attrs} data-picker="${pk}">
      <option value="-1" ${Number(value) < 0 ? 'selected' : ''}>none</option>
      ${!known && Number(value) >= 0 ? `<option value="${h(value)}" selected>#${h(value)} (not in this world)</option>` : ''}
      ${opts.map((o) => `<option value="${o.id}" ${o.id === Number(value) ? 'selected' : ''}>${h(optionLabel(pk, o))}</option>`).join('')}</select>`;
  }
  const num = `<input type="number" min="${st.min}" max="${st.max}" step="${st.step || 1}" value="${h(value)}"`;
  if (kind === 'range') {
    return `<div class="flex items-center gap-3"><input id="${id}-r" type="range" min="${st.min}" max="${st.max}" step="${st.step || 1}" value="${h(value)}" class="flex-1 min-w-0 h-2 cursor-pointer accent-primary-600" ${dis} data-pair="${id}" tabindex="-1" aria-hidden="true">
      ${num} class="${INPUT} py-1.5 px-2.5 w-24 shrink-0" ${dis} ${attrs} data-pair="${id}-r"></div>`;
  }
  return `${num} class="${INPUT} w-full py-1.5 px-2.5" ${dis} ${attrs}>`;
}

/** What became of each key asked of a running bot, as a badge (never "applied" before the game reads
 *  the new value back). */
function stateMarkup(a, st, key) {
  if (!a) return '';
  if (a.state === 'pending' && Number(st.value) === a.to) a.state = 'applied';
  const tone = { applied: 'green', pending: 'yellow', refused: 'red', sent: 'blue' }[a.state] || 'gray';
  const text = { applied: `applied ${a.from} → ${a.to}`, pending: `approval #${a.approval}: ${a.from} → ${a.to}`, refused: `refused: ${a.reason}`, sent: 'sent…' }[a.state];
  const undo = a.state === 'applied' ? ` <button type="button" data-undo="${h(key)}:${h(st.name)}" class="ms-1 text-xs font-medium text-primary-700 dark:text-primary-400 hover:underline">undo</button>` : '';
  return `<div class="mt-1">${badge(tone, text)}${undo}</div>`;
}

const legend = (title, extra = '') => `<legend class="px-1.5 text-xs font-semibold tracking-wide text-gray-500 uppercase dark:text-gray-400">${h(title)}${extra}</legend>`;
const FIELDSET = 'min-w-0 p-3 border border-gray-200 rounded-lg dark:border-gray-700';

/** A bot's whole editor: the groups as fieldsets, then each table of lines. `opts.asked` maps a
 *  setting name to its ask; `opts.value` reads a setting's shown value. */
export function editorMarkup(settings, key, opts = {}) {
  const { groups, tables } = layout(settings);
  const value = opts.value || ((st) => st.value ?? st.default);
  const state = (st) => stateMarkup((opts.asked || {})[st.name], st, key);
  const field = (st) => `<div class="min-w-0">
      <div class="flex items-center gap-1.5 mb-1"><label for="${`set-${key}-${st.name}`.replace(/[^a-zA-Z0-9_-]/g, '_')}" class="text-sm font-medium text-gray-900 font-mono dark:text-white truncate">${h(st.name)}</label>${st.ingame ? badge('green', 'live') : ''}</div>
      ${controlMarkup(st, key, value(st), opts)}
      <p class="mt-1 text-xs text-gray-500 dark:text-gray-400 line-clamp-2" title="${h(st.description || '')}">${h(st.description || '')} <span class="whitespace-nowrap">[${h(st.min)}..${h(st.max)}]</span></p>${state(st)}</div>`;
  const group = (g) => `<fieldset class="${FIELDSET}" data-group="${h(g.title)}">${legend(g.title)}
      <div class="grid gap-x-4 gap-y-3 sm:grid-cols-2">${g.settings.map(field).join('')}</div></fieldset>`;
  const row = (t, r) => `<tr class="border-t border-gray-200 dark:border-gray-700 align-top" data-line="${r.n}">
      <th scope="row" class="px-2 py-2 font-mono text-xs font-medium text-gray-500 whitespace-nowrap dark:text-gray-400">${r.n}</th>
      ${t.fields.map((f) => (r.cells[f] ? `<td class="px-2 py-1.5 min-w-[12rem]">${controlMarkup(r.cells[f], key, value(r.cells[f]), { ...opts, compact: true })}${state(r.cells[f])}</td>` : '<td></td>')).join('')}</tr>`;
  const head = (t) => `<thead class="text-xs text-gray-700 uppercase bg-gray-50 dark:bg-gray-700 dark:text-gray-400"><tr><th scope="col" class="px-2 py-2">Line</th>${t.fields.map((f) => {
    const first = t.rows.find((r) => r.cells[f]);
    return `<th scope="col" class="px-2 py-2" title="${h(first ? first.cells[f].description : '')}">${h(f)}</th>`;
  }).join('')}</tr></thead>`;
  const table = (t) => {
    const used = t.rows.filter((r) => !r.unset);
    const unused = t.rows.filter((r) => r.unset);
    const span = !unused.length ? '' : unused.length === 1 ? `${unused[0].n}` : `${unused[0].n}–${unused.at(-1).n}`;
    const notes = t.fields.map((f) => { const first = t.rows.find((r) => r.cells[f]); return first ? `<li><span class="font-mono">${h(f)}</span>: ${h(first.cells[f].description || '')}</li>` : ''; }).join('');
    return `<fieldset class="${FIELDSET} xl:col-span-2" data-lines="${h(t.base)}">${legend(`${t.base} lines`, ` <span class="font-normal normal-case">· ${used.length} in use, ${unused.length} unused</span>`)}
      <div class="overflow-x-auto"><table class="w-full text-sm text-left text-gray-500 dark:text-gray-400">${head(t)}<tbody>${used.map((r) => row(t, r)).join('') || `<tr><td colspan="${t.fields.length + 1}" class="px-2 py-2">no line in use</td></tr>`}</tbody></table></div>
      ${unused.length ? `<details class="mt-2 group" data-unused><summary class="text-sm font-medium cursor-pointer text-primary-700 dark:text-primary-400 hover:underline">${unused.length} unused line${unused.length > 1 ? 's' : ''} (${span})</summary>
        <div class="mt-2 overflow-x-auto"><table class="w-full text-sm text-left text-gray-500 dark:text-gray-400">${head(t)}<tbody>${unused.map((r) => row(t, r)).join('')}</tbody></table></div></details>` : ''}
      <ul class="mt-2 space-y-0.5 text-xs text-gray-500 dark:text-gray-400">${notes}</ul></fieldset>`;
  };
  return `<div class="grid gap-4 xl:grid-cols-2">${groups.map(group).join('')}${tables.map(table).join('')}</div>`;
}

/* ---- the live card -------------------------------------------------------------------------- */

let bots = null;
let choices = { town: [], industry: [], cargo: [] };
let formSig = '';
/** Asks of running bots: company -> { name -> { from, to, state, approval, reason } }. */
const asked = new Map();

const api = (...a) => globalThis.api(...a);
const say = (html) => { $('bot-msg').innerHTML = html; };

/** The value a control holds: an integer, or null when it holds none. */
function read(el) {
  if (el.dataset.kind === 'toggle') return el.checked ? 1 : 0;
  const v = el.value.trim();
  return v === '' || !Number.isFinite(Number(v)) ? null : Number(v);
}
const valuesOf = (root) => Object.fromEntries([...root.querySelectorAll('[data-setting]')].map((el) => [el.dataset.setting, read(el)]));
const isDirty = () => [...document.querySelectorAll('#bot-running [data-dirty="true"]')].length > 0;

function renderForm(force = false) {
  const ai = (bots.ais || []).find((a) => a.ai === $('bot-ai').value);
  const sig = JSON.stringify([ai && ai.ai, ai && ai.settings.map((s) => s.name), Object.values(choices).map((c) => c.length)]);
  if (!force && sig === formSig) return;
  formSig = sig;
  $('bot-new-form').innerHTML = !ai ? '' : ai.settings.length ? editorMarkup(ai.settings, 'new', { choices, value: (st) => st.default })
    : '<p class="text-sm text-gray-500 dark:text-gray-400">This AI declares no setting; it starts with its own defaults.</p>';
}

function renderRunning() {
  const canSet = !!(bots.live && bots.live.set_setting);
  const cards = (bots.running || []).map((c) => {
    const live = (c.settings || []).filter((st) => st.ingame);
    const body = live.length ? editorMarkup(live, `c${c.id}`, { choices, disabled: !canSet, asked: asked.get(c.id) || {} })
      : `<p class="text-sm text-gray-500 dark:text-gray-400">${bots.live && bots.live.list ? 'This AI declares no setting that can change while it runs.' : 'The current values are not readable: this game binary has no openttd.ai.list.'}</p>`;
    return `<article class="p-4 border border-gray-200 rounded-lg bg-gray-50 dark:bg-gray-900/40 dark:border-gray-700" data-bot="${h(c.id)}">
      <header class="flex flex-wrap items-start justify-between gap-3 mb-3">
        <div class="min-w-0"><h4 class="text-base font-semibold text-gray-900 dark:text-white">${h(c.name || `Company ${c.id}`)}</h4>
          <p class="text-sm text-gray-500 dark:text-gray-400">company ${h(c.id)} · ${h(c.ai || 'AI')}${c.running === false ? ' (stopped)' : ''} · money ${typeof c.money === 'number' ? whole.format(c.money) : h(c.money ?? '-')}</p></div>
        ${live.length ? `<div class="flex shrink-0 items-center gap-2"><span class="text-sm text-gray-500 dark:text-gray-400" data-pending>no change</span>
          <button type="button" class="${BTN_ALT}" data-reset-bot="${h(c.id)}" disabled>Reset</button>
          <button type="button" class="${BTN}" data-apply-bot="${h(c.id)}" disabled>Apply</button></div>` : ''}
      </header>${body}</article>`;
  });
  $('bot-running').innerHTML = cards.join('') || '<p class="text-sm text-gray-500 dark:text-gray-400">No bot is running. Start one below.</p>';
  $('bot-new').open = !cards.length || $('bot-new').open;
  if (!canSet) say(`<div class="p-3 text-sm text-yellow-800 rounded-lg bg-yellow-50 dark:bg-gray-800 dark:text-yellow-300" role="alert">This game binary cannot change a running bot's settings (no openttd.ai.set_setting); start a new bot with the values you want.</div>`);
}

/** Mark the fields that differ from the game and say what Apply will ask. */
function refreshPending(card) {
  const company = Number(card.dataset.bot);
  const c = (bots.running || []).find((x) => x.id === company);
  if (!c) return;
  const values = valuesOf(card);
  const diff = changes(c.settings || [], values);
  const names = new Set(diff.map((d) => d.name));
  for (const el of card.querySelectorAll('[data-setting]')) el.dataset.dirty = String(names.has(el.dataset.setting));
  const n = diff.length;
  card.querySelector('[data-pending]').textContent = n ? `${n} change${n > 1 ? 's' : ''}, ${n} approval${n > 1 ? 's' : ''}` : 'no change';
  const apply = card.querySelector('[data-apply-bot]');
  apply.disabled = !n || !(bots.live && bots.live.set_setting);
  apply.textContent = n ? `Apply ${n}` : 'Apply';
  card.querySelector('[data-reset-bot]').disabled = !n;
}

async function ask(company, name, to, from) {
  if (!asked.has(company)) asked.set(company, {});
  const a = asked.get(company)[name] = { from, to, state: 'sent' };
  const r = await api('/api/bots/setting', { company, name, value: to });
  if (r.status === 'pending_approval') Object.assign(a, { state: 'pending', approval: r.approval_id });
  else if (r.error || r.errors || r.status === 'refused' || r.verdict === 'refused') Object.assign(a, { state: 'refused', reason: r.error || (r.errors || []).join('; ') || r.verdict });
  else Object.assign(a, { state: 'pending', approval: r.approval_id ?? '—' });
}

async function loadBots() {
  const fresh = await api('/api/bots');
  if (!fresh || fresh.error) return;
  // what the owner is editing survives a refresh: no re-render while a field has focus or differs
  const editing = document.activeElement && document.activeElement.closest && document.activeElement.closest('#bot');
  bots = bots && (editing || isDirty()) ? { ...fresh, running: bots.running } : fresh;
  $('bot-source').textContent = bots.source;
  const keep = $('bot-ai').value;
  const names = (bots.ais || []).map((a) => a.ai);
  if ([...$('bot-ai').options].map((o) => o.value).join() !== names.join()) {
    $('bot-ai').innerHTML = names.map((n) => `<option>${h(n)}</option>`).join('');
    if (names.includes(keep)) $('bot-ai').value = keep;
  }
  if (!editing) renderForm();
  if (!editing && !isDirty()) renderRunning();
}

async function loadChoices() {
  const c = await api('/api/bots/choices');
  if (c && !c.error) { choices = c; if (bots) { renderForm(); if (!isDirty()) renderRunning(); } }
}

export function mount() {
  if (!$('bot')) return;
  $('bot-ai').onchange = () => renderForm();
  $('bot-start').onclick = async () => {
    const settings = valuesOf($('bot-new-form'));
    const bad = Object.entries(settings).find(([, v]) => v === null);
    if (bad) { say(`${h(bad[0])}: give it a value`); return; }
    const r = await api('/api/bots/start', { ai: $('bot-ai').value, settings });
    say(r.status === 'pending_approval' ? `Start waits for approval #${h(r.approval_id)}.` : h(JSON.stringify(r)));
    setTimeout(loadBots, 3000);
  };
  const bot = $('bot');
  bot.addEventListener('input', (ev) => {
    const t = ev.target;
    if (t.dataset.pair) { const other = $(t.dataset.pair); if (other) other.value = t.value; }
    if (t.dataset.kind === 'toggle') { const s = t.closest('label').querySelector('[data-toggle-text]'); if (s) s.textContent = t.checked ? 'on' : 'off'; }
    const card = t.closest('[data-bot]');
    if (card) refreshPending(card);
  });
  bot.addEventListener('change', (ev) => { const card = ev.target.closest('[data-bot]'); if (card) refreshPending(card); });
  bot.addEventListener('click', async (ev) => {
    const reset = ev.target.closest('[data-reset-bot]');
    if (reset) { renderRunning(); return; }
    const apply = ev.target.closest('[data-apply-bot]');
    if (apply) {
      const company = Number(apply.dataset.applyBot);
      const card = apply.closest('[data-bot]');
      const c = (bots.running || []).find((x) => x.id === company);
      const diff = changes(c.settings || [], valuesOf(card));
      const byName = Object.fromEntries((c.settings || []).map((st) => [st.name, st]));
      const out = diff.find((d) => d.to < byName[d.name].min || d.to > byName[d.name].max);
      if (out) { say(`${h(out.name)}: ${h(out.to)} is outside ${h(byName[out.name].min)}..${h(byName[out.name].max)}`); return; }
      apply.disabled = true;
      for (const d of diff) await ask(company, d.name, d.to, d.from);
      say(`Asked ${diff.length} change${diff.length > 1 ? 's' : ''} of company ${h(company)}; each waits for its approval.`);
      renderRunning();
      globalThis.tick?.();
      return;
    }
    const undo = ev.target.closest('[data-undo]');
    if (undo) {
      const [key, name] = undo.dataset.undo.split(':');
      const company = Number(key.replace(/^c/, ''));
      const a = (asked.get(company) || {})[name];
      if (a) { await ask(company, name, a.from, a.to); renderRunning(); }
    }
  });
  loadChoices().catch(() => {});
  setInterval(() => loadChoices().catch(() => {}), 60000);
  loadBots().catch(() => {});
  setInterval(() => loadBots().catch(() => {}), 10000);
}

if (globalThis.document) mount();
