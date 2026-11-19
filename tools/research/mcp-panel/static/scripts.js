/* The Scripts page (/scripts, /scripts/<name>): the panel's Rhai library as a workspace
   (src/panel_scripts.rs). A list of every script with what its own text says (the header comment,
   imports, who imports it, its service), an editor with Rhai highlighting, line numbers, an outline
   of its functions and a live parse check (POST /api/script/check, the error's line marked), Save
   (only text that parses), Run once (streamed, POST /api/rhai/stream), and Start/Stop as a service
   whose log streams below and which reloads by itself when its file, or a library it imports,
   changes (hot reload). A file changed outside (an editor, git) is loaded again while the page has
   no unsaved edit, and offered otherwise. The script is in the address, so a script is a link.
   The editor is CodeMirror 6 (static/rhai-editor.js: completion and hover from GET /api/rhai/functions,
   the parse error as a lint marker), the plain one below when CodeMirror cannot load.
   Keys: Ctrl/Cmd+S saves, Ctrl/Cmd+Enter runs, Escape stops a run, Tab indents.
   The pure parts are exported for node --test (test/scripts.test.mjs). */

import { UI, TONE, $, esc, badge } from './components.js';

/* ---- Rhai, token by token ------------------------------------------------------------------- */

/** Rhai's keywords (the language reference's list), the ones a script of this library uses first. */
export const KEYWORDS = new Set(['let', 'const', 'fn', 'if', 'else', 'switch', 'do', 'while', 'loop', 'until', 'for', 'in',
  'break', 'continue', 'return', 'throw', 'try', 'catch', 'import', 'export', 'as', 'private', 'global', 'this', 'is_shared',
  'true', 'false', 'Fn', 'call', 'curry', 'type_of', 'print', 'debug', 'eval']);

/** The tokens of a text: [{k, v}] with k one of comment, doc, string, number, keyword, module, call,
    ident, op, ws. Joined, the values are the text again, character for character. */
export function tokenize(text) {
  const out = [];
  const rules = [
    ['doc', /^\/\/\/[^\n]*/], ['comment', /^\/\/[^\n]*/], ['comment', /^\/\*[\s\S]*?(\*\/|$)/],
    ['string', /^`(?:\\[\s\S]|[^`\\])*`?/], ['string', /^"(?:\\.|[^"\\\n])*"?/], ['string', /^'(?:\\.|[^'\\\n])*'?/],
    ['number', /^(?:0x[0-9a-fA-F_]+|\d[\d_]*(?:\.\d[\d_]*)?(?:e[+-]?\d+)?)/],
    ['ws', /^\s+/], ['word', /^[A-Za-z_][A-Za-z0-9_]*/], ['op', /^(?:::|=>|\?\?|[=!<>]=|&&|\|\||[-+*/%^&|<>!?=.,;:(){}[\]#@$~])/],
  ];
  let i = 0;
  while (i < text.length) {
    const rest = text.slice(i);
    let hit = null;
    for (const [k, re] of rules) { const m = re.exec(rest); if (m && m[0]) { hit = [k, m[0]]; break; } }
    if (!hit) hit = ['op', rest[0]];
    let [k, v] = hit;
    if (k === 'word') {
      const after = text.slice(i + v.length);
      k = KEYWORDS.has(v) ? 'keyword' : after.startsWith('::') ? 'module' : /^\s*\(/.test(after) ? 'call' : 'ident';
    }
    out.push({ k, v });
    i += v.length;
  }
  return out;
}

/** The Flowbite tones of each token kind (dark editor surface). */
export const TOKEN_CLASS = {
  comment: 'text-gray-500 italic', doc: 'text-green-400 italic', string: 'text-amber-300', number: 'text-purple-300',
  keyword: 'text-pink-400 font-semibold', module: 'text-cyan-300', call: 'text-blue-300', ident: 'text-gray-100', op: 'text-gray-400', ws: '',
};

/** A text as highlighted HTML (escaped), one span per token that has a tone. */
export function highlightHtml(text) {
  return tokenize(text).map(({ k, v }) => (TOKEN_CLASS[k] ? `<span class="${TOKEN_CLASS[k]}">${esc(v)}</span>` : esc(v))).join('')
    // A last empty line still takes a line in the <pre>, as it does in the textarea.
    + (text.endsWith('\n') ? ' ' : '');
}

/** The functions a script defines, each with its line and the `///` doc lines just above it. */
export function outline(text) {
  const lines = text.split('\n');
  const out = [];
  lines.forEach((line, i) => {
    const m = /^\s*(?:private\s+)?fn\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(([^)]*)\)/.exec(line);
    if (!m) return;
    const doc = [];
    for (let j = i - 1; j >= 0 && /^\s*\/\/\//.test(lines[j]); j--) doc.unshift(lines[j].replace(/^\s*\/\/\/\s?/, ''));
    out.push({ name: m[1], args: m[2].split(',').map((a) => a.trim()).filter(Boolean), line: i + 1, doc: doc.join(' ') });
  });
  return out;
}

/** The 1-based line of a character offset. */
export const lineAt = (text, offset) => text.slice(0, offset).split('\n').length;

/** The character offset where a 1-based line starts. */
export function offsetOfLine(text, line) {
  let at = 0;
  for (let n = 1; n < line; n++) { const nl = text.indexOf('\n', at); if (nl < 0) return text.length; at = nl + 1; }
  return at;
}

/** Tab: four spaces at the caret, or before every selected line (Shift+Tab takes them off). */
export function indent(text, start, end, outdent = false) {
  const from = text.lastIndexOf('\n', start - 1) + 1;
  if (start === end && !outdent) return { text: text.slice(0, start) + '    ' + text.slice(end), start: start + 4, end: start + 4 };
  const block = text.slice(from, end);
  const lines = block.split('\n');
  const changed = lines.map((l) => (outdent ? l.replace(/^ {1,4}/, '') : '    ' + l));
  const out = changed.join('\n');
  const first = (changed[0].length - lines[0].length);
  return { text: text.slice(0, from) + out + text.slice(end), start: Math.max(from, start + first), end: end + (out.length - block.length) };
}

/** The complete frames of a /api/rhai/stream buffer and what is left of it: [{event, data, text}],
    where `out` and `err` carry one printed line as a JSON string (text is that line) and `exit` the
    exit status (src/panel.rs stream_rhai). */
export function parseSse(buf) {
  const frames = [];
  let cut;
  while ((cut = buf.indexOf('\n\n')) >= 0) {
    const frame = buf.slice(0, cut);
    buf = buf.slice(cut + 2);
    const event = /^event: (.*)$/m.exec(frame)?.[1] || 'message';
    const data = frame.split('\n').filter((l) => l.startsWith('data: ')).map((l) => l.slice(6)).join('\n');
    let text = data;
    try { const v = JSON.parse(data); if (typeof v === 'string') text = v; } catch { /* a bare value */ }
    frames.push({ event, data, text });
  }
  return { frames, rest: buf };
}

/** The script a path names: /scripts/<name> -> name, /scripts -> ''. */
export const scriptOfPath = (path) => decodeURIComponent((/^\/scripts\/([^/?#]+)/.exec(path) || [])[1] || '');

/** A valid new script name (the server's rule, src/panel_scripts.rs valid_name). */
export const validName = (name) => /^[A-Za-z0-9_]{1,64}$/.test(name);

/** The badges of a script in the list and the header: library, service state, reloads. */
export function scriptBadges(s) {
  const out = [];
  if (s.library) out.push(['library', 'purple']);
  if (s.service?.running) out.push(['service running', 'green']);
  else if (s.service) out.push(['service stopped', 'gray']);
  if (s.service?.reloads) out.push([`${s.service.reloads} reload${s.service.reloads === 1 ? '' : 's'}`, 'blue']);
  return out;
}

/** The shared memory's entries (GET /api/rhai/shared) as table rows, escaped: key, value (JSON,
    cut at 160 characters with the whole in its title), revision, writer and time. */
export function sharedRows(entries, now = Date.now() / 1000) {
  const ago = (at) => { const s = Math.max(0, Math.round(now - at)); return s < 60 ? `${s} s ago` : s < 3600 ? `${Math.round(s / 60)} min ago` : `${Math.round(s / 3600)} h ago`; };
  return (entries || []).map((e) => {
    const json = JSON.stringify(e.value);
    const shown = json.length > 160 ? `${json.slice(0, 159)}…` : json;
    return `<tr class="bg-white border-b dark:bg-gray-800 dark:border-gray-700" data-key="${esc(e.key)}"><td class="px-4 py-2 font-mono text-gray-900 dark:text-white">${esc(e.key)}</td>`
      + `<td class="px-4 py-2 font-mono text-xs break-all" title="${esc(json)}">${esc(shown)}</td><td class="px-4 py-2">${Number(e.rev) || 0}</td>`
      + `<td class="px-4 py-2 font-mono text-xs">${esc(e.writer)}</td><td class="px-4 py-2 whitespace-nowrap">${esc(ago(e.at))}</td></tr>`;
  }).join('') || '<tr><td colspan="5" class="px-4 py-3 text-gray-500 dark:text-gray-400">Nothing shared yet.</td></tr>';
}

/** Which debug buttons work in a session's status (src/rhai_debug.rs): idle or finished starts one;
    paused steers it; running can only be stopped. */
export function debugControls(status) {
  const paused = status === 'paused';
  const live = paused || status === 'running';
  return { start: !live, continue: paused, step: paused, over: paused, out: paused, stop: live };
}

/** A paused session's variables as table rows, escaped; a name declared twice shows its latest. */
export function varRows(vars) {
  const latest = new Map();
  for (const v of vars || []) latest.set(v.name, v);
  return [...latest.values()].map((v) => `<tr class="bg-white border-b dark:bg-gray-800 dark:border-gray-700"><td class="px-4 py-2 font-mono text-gray-900 dark:text-white">${esc(v.name)}${v.constant ? ' <span class="text-xs text-gray-400">const</span>' : ''}</td>`
    + `<td class="px-4 py-2 font-mono text-xs">${esc(v.type)}</td><td class="px-4 py-2 font-mono text-xs break-all">${esc(v.value)}</td></tr>`).join('')
    || '<tr><td colspan="3" class="px-4 py-3 text-gray-500 dark:text-gray-400">Nothing in scope.</td></tr>';
}

/** A paused session's call stack, innermost first, each frame a button to its call's line. */
export function stackItems(stack, line) {
  const frames = (stack || []).map((f) => `<li><button type="button" data-line="${Number(f.line) || ''}" class="w-full p-2 text-left rounded-lg hover:bg-gray-100 dark:hover:bg-gray-700"><span class="font-mono text-blue-600 dark:text-blue-300">${esc(f.function)}</span><span class="font-mono text-xs text-gray-500 dark:text-gray-400">(${(f.args || []).map((a) => esc(a.value)).join(', ')})</span>`
    + `<span class="float-right text-xs text-gray-400">${f.source ? `${esc(f.source)}:` : 'line '}${Number(f.line) || '?'}</span></button></li>`);
  return [`<li class="p-2 font-mono text-xs text-gray-500 dark:text-gray-400">paused at line ${Number(line) || '?'}</li>`, ...frames].join('');
}

/** The first line of a doc, for the list. */
export const firstLine = (doc) => String(doc || '').split('\n').find((l) => l.trim()) || '';

/* ---- the editor ---------------------------------------------------------------------------- */

/** The editor component: a field-set frame (UI.input's look) holding the line numbers, the
    highlighted copy of the text and, over it, a transparent textarea that takes the typing. */
export function editor(host, { onChange, onSave, onRun, onStop } = {}) {
  host.className = 'relative flex w-full overflow-hidden font-mono text-sm leading-6 bg-gray-900 border border-gray-300 rounded-lg dark:border-gray-600 focus-within:ring-2 focus-within:ring-primary-500 h-[60vh] min-h-80';
  host.innerHTML = `<div data-gutter class="select-none py-2 ps-2 pe-3 text-right text-gray-500 bg-gray-800 border-e border-gray-700 overflow-hidden" aria-hidden="true"></div>
    <div class="relative flex-1 overflow-hidden">
      <pre data-code aria-hidden="true" class="absolute inset-0 m-0 p-2 overflow-hidden whitespace-pre text-gray-100 pointer-events-none"></pre>
      <textarea data-input spellcheck="false" autocomplete="off" autocapitalize="off" aria-label="Script text" wrap="off"
        class="absolute inset-0 w-full h-full m-0 p-2 overflow-auto whitespace-pre bg-transparent border-0 resize-none text-transparent caret-white focus:ring-0 focus:outline-none selection:bg-primary-700/60"></textarea>
    </div>`;
  const ta = host.querySelector('[data-input]');
  const code = host.querySelector('[data-code]');
  const gutter = host.querySelector('[data-gutter]');
  let errorLine = 0;
  const paint = () => {
    code.innerHTML = highlightHtml(ta.value);
    const n = ta.value.split('\n').length;
    gutter.innerHTML = Array.from({ length: n }, (_, i) => `<div class="${i + 1 === errorLine ? 'text-red-400 font-bold' : ''}">${i + 1}</div>`).join('');
    sync();
  };
  const sync = () => { code.scrollTop = ta.scrollTop; code.scrollLeft = ta.scrollLeft; gutter.scrollTop = ta.scrollTop; };
  ta.addEventListener('scroll', sync);
  ta.addEventListener('input', () => { paint(); onChange?.(ta.value); });
  ta.addEventListener('keydown', (e) => {
    const mod = e.ctrlKey || e.metaKey;
    if (e.key === 'Tab') {
      e.preventDefault();
      const r = indent(ta.value, ta.selectionStart, ta.selectionEnd, e.shiftKey);
      ta.value = r.text; ta.selectionStart = r.start; ta.selectionEnd = r.end;
      paint(); onChange?.(ta.value);
    } else if (mod && e.key.toLowerCase() === 's') { e.preventDefault(); onSave?.(); }
    else if (mod && e.key === 'Enter') { e.preventDefault(); onRun?.(); }
    else if (e.key === 'Escape') { onStop?.(); }
  });
  return {
    get value() { return ta.value; },
    set value(v) { ta.value = v; paint(); },
    focus: () => ta.focus(),
    markError(err) { errorLine = (err && typeof err === 'object' ? err.line : err) || 0; paint(); },
    goto(line) {
      const at = offsetOfLine(ta.value, line);
      ta.focus(); ta.selectionStart = ta.selectionEnd = at;
      ta.scrollTop = Math.max(0, (line - 4) * 24);
      sync();
    },
  };
}

/* ---- the page ------------------------------------------------------------------------------ */

const api = async (path, body) => {
  const r = await fetch(path, body === undefined ? {} : { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) });
  const j = await r.json().catch(() => ({}));
  return { status: r.status, ...j };
};

/** The CodeMirror 6 editor (static/rhai-editor.js, through the page's import map), or the plain
    editor above when CodeMirror cannot load (no npm ci in tools/research/mcp-panel). */
export async function makeEditor(host, opts, load = () => import('./rhai-editor.js')) {
  try { return (await load()).rhaiEditor(host, opts); } catch (e) {
    console.warn('CodeMirror did not load; the plain editor runs instead:', e?.message || e);
    return editor(host, opts);
  }
}

async function boot() {
  const state = { name: scriptOfPath(location.pathname), saved: '', modified: null, scripts: [], seq: 0, run: null, checkTimer: 0 };
  const ed = await makeEditor($('editor'), {
    onChange: () => { markDirty(); scheduleCheck(); },
    onSave: () => save(), onRun: () => runOnce(), onStop: () => stopRun(),
  });
  // The editor for the page's browser check (tools/research/mcp-scripts-check) and the console:
  // its text is window.scriptsEditor.value whichever editor runs.
  window.scriptsEditor = ed;
  const term = (box, line, tone = '') => {
    const div = document.createElement('div');
    div.className = tone;
    div.textContent = line;
    box.appendChild(div);
    while (box.childElementCount > 2000) box.firstElementChild.remove();
    box.scrollTop = box.scrollHeight;
  };
  const toneOf = (t) => (/^error|mcp-rhai:|Runtime error|refused|denied|does not parse|failed/i.test(t) ? 'text-red-400'
    : /^== /.test(t) ? 'text-blue-300' : '');
  const dirty = () => ed.value !== state.saved;
  function markDirty() { $('dirty').classList.toggle('hidden', !dirty()); }
  function scheduleCheck() {
    clearTimeout(state.checkTimer);
    state.checkTimer = setTimeout(async () => {
      const r = await api('/api/script/check', { text: ed.value });
      showError(r.error);
    }, 350);
  }
  function showError(err) {
    const box = $('check');
    if (!err) {
      ed.markError(null);
      box.className = `${UI.badge} ${TONE.green}`;
      box.textContent = 'parses';
      $('check-detail').textContent = '';
      return;
    }
    ed.markError(err);
    box.className = `${UI.badge} ${TONE.red}`;
    box.textContent = `line ${err.line ?? '?'}, column ${err.column ?? '?'}`;
    $('check-detail').innerHTML = `<button type="button" data-line="${err.line}" class="text-left text-red-400 hover:underline">${esc(err.message)}</button>`;
  }
  $('check-detail').addEventListener('click', (e) => { const l = e.target.closest('[data-line]')?.dataset.line; if (l) ed.goto(Number(l)); });

  function renderList() {
    const filter = ($('filter').value || '').toLowerCase();
    $('script-list').innerHTML = state.scripts
      .filter((s) => !filter || s.name.toLowerCase().includes(filter) || (s.doc || '').toLowerCase().includes(filter))
      .map((s) => `<li><a href="/scripts/${encodeURIComponent(s.name)}" data-name="${esc(s.name)}" class="block p-3 rounded-lg hover:bg-gray-100 dark:hover:bg-gray-700 ${s.name === state.name ? 'bg-gray-100 dark:bg-gray-700' : ''}"${s.name === state.name ? ' aria-current="page"' : ''}>
          <div class="flex flex-wrap items-center gap-2"><span class="font-mono text-sm font-semibold text-gray-900 dark:text-white">${esc(s.name)}</span>${scriptBadges(s).map(([t, tone]) => badge(t, tone)).join('')}</div>
          <p class="mt-1 text-xs text-gray-500 dark:text-gray-400 line-clamp-2">${esc(firstLine(s.doc))}</p></a></li>`).join('')
      || '<li class="p-3 text-sm text-gray-500 dark:text-gray-400">No script matches.</li>';
    $('script-count').textContent = `${state.scripts.length}`;
  }
  $('filter').addEventListener('input', renderList);
  $('script-list').addEventListener('click', (e) => {
    const a = e.target.closest('a[data-name]');
    if (!a) return;
    e.preventDefault();
    open(a.dataset.name);
  });

  async function loadList() {
    const r = await api('/api/scripts');
    state.scripts = r.scripts || [];
    $('library-dir').textContent = r.dir || '';
    renderList();
    renderMeta();
  }

  function renderMeta() {
    const s = state.scripts.find((x) => x.name === state.name);
    $('doc').textContent = s?.doc || (state.name ? 'This script has no header comment yet: its first // lines are its documentation.' : '');
    const link = (n) => `<a href="/scripts/${encodeURIComponent(n)}" data-name="${esc(n)}" class="font-mono text-primary-600 hover:underline dark:text-primary-500">${esc(n)}</a>`;
    $('imports').innerHTML = (s?.imports || []).map(link).join(', ') || '<span class="text-gray-500 dark:text-gray-400">none</span>';
    $('imported-by').innerHTML = (s?.imported_by || []).map(link).join(', ') || '<span class="text-gray-500 dark:text-gray-400">none</span>';
    $('svc-badges').innerHTML = s ? scriptBadges(s).map(([t, tone]) => badge(t, tone)).join(' ') : '';
    const running = !!s?.service?.running;
    $('svc-start').disabled = !state.name || running;
    $('svc-stop').disabled = !running;
    $('svc-watched').textContent = s?.service?.watched?.length ? `watching ${s.service.watched.join(', ')} for changes` : '';
    const fns = outline(ed.value);
    $('outline').innerHTML = fns.map((f) => `<li><button type="button" data-line="${f.line}" class="w-full p-2 text-left rounded-lg hover:bg-gray-100 dark:hover:bg-gray-700" title="${esc(f.doc)}"><span class="font-mono text-sm text-blue-600 dark:text-blue-300">${esc(f.name)}</span><span class="font-mono text-xs text-gray-500 dark:text-gray-400">(${esc(f.args.join(', '))})</span><span class="float-right text-xs text-gray-400">${f.line}</span></button></li>`).join('')
      || '<li class="p-2 text-sm text-gray-500 dark:text-gray-400">No functions.</li>';
  }
  $('outline').addEventListener('click', (e) => { const l = e.target.closest('[data-line]')?.dataset.line; if (l) ed.goto(Number(l)); });
  for (const id of ['imports', 'imported-by']) $(id).addEventListener('click', (e) => { const a = e.target.closest('a[data-name]'); if (a) { e.preventDefault(); open(a.dataset.name); } });

  async function open(name, push = true) {
    if (dirty() && state.name && name !== state.name) {
      $('msg').textContent = `${state.name} has unsaved edits: save them first, or Revert`;
      return;
    }
    state.name = name;
    if (push) history.pushState({}, '', name ? `/scripts/${encodeURIComponent(name)}` : '/scripts');
    $('script-name').textContent = name ? `${name}.rhai` : 'No script open';
    if (!name) { ed.value = ''; state.saved = ''; renderList(); renderMeta(); return; }
    const r = await api(`/api/script?name=${encodeURIComponent(name)}`);
    if (r.status !== 200) { $('msg').textContent = r.error || `no script ${name}`; return; }
    ed.value = r.text; state.saved = r.text; state.modified = r.modified; state.seq = 0;
    $('svc-log').textContent = '';
    showError(r.error); markDirty(); renderList(); renderMeta();
    document.title = `${name}.rhai · Scripts · OpenTTD MCP`;
  }
  window.addEventListener('popstate', () => open(scriptOfPath(location.pathname), false));

  async function save() {
    if (!state.name) return;
    const r = await api('/api/script/save', { name: state.name, text: ed.value });
    if (r.saved) {
      state.saved = ed.value; state.modified = r.modified;
      $('msg').textContent = `saved ${state.name}.rhai; a service running it reloads within a second`;
      showError(null);
      await loadList();
    } else {
      showError(r.error);
      $('msg').textContent = `not saved: ${r.error?.message || r.error || 'refused'}`;
    }
    markDirty();
  }
  $('save').addEventListener('click', save);
  $('revert').addEventListener('click', () => { ed.value = state.saved; markDirty(); scheduleCheck(); });

  async function runOnce() {
    stopRun();
    const out = $('run-out');
    out.textContent = '';
    const ctl = new AbortController();
    state.run = ctl;
    $('run').disabled = true; $('run-stop').disabled = false;
    term(out, `== run ${state.name || 'unsaved text'}`, 'text-blue-300');
    try {
      const r = await fetch('/api/rhai/stream', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ script: ed.value }), signal: ctl.signal });
      const reader = r.body.getReader();
      const dec = new TextDecoder();
      let buf = '';
      for (;;) {
        const { value, done } = await reader.read();
        if (done) break;
        buf += dec.decode(value, { stream: true });
        const { frames, rest } = parseSse(buf);
        buf = rest;
        for (const f of frames) {
          if (f.event === 'exit') term(out, `== exit ${f.data}`, f.data === '0' ? 'text-green-400' : 'text-red-400');
          else term(out, f.text, f.event === 'err' ? 'text-red-400' : toneOf(f.text));
        }
      }
    } catch (e) {
      if (e.name !== 'AbortError') term(out, `error: ${e.message}`, 'text-red-400');
      else term(out, '== stopped', 'text-yellow-300');
    } finally {
      state.run = null; $('run').disabled = false; $('run-stop').disabled = true;
    }
  }
  function stopRun() { if (state.run) state.run.abort(); }
  $('run').addEventListener('click', runOnce);
  $('run-stop').addEventListener('click', stopRun);

  $('svc-start').addEventListener('click', async () => {
    if (dirty()) await save();
    const r = await api('/api/services/start', { name: state.name });
    $('msg').textContent = r.error ? `not started: ${r.error}` : `${state.name} runs as a service`;
    await loadList();
  });
  $('svc-stop').addEventListener('click', async () => {
    const r = await api('/api/services/stop', { name: state.name });
    $('msg').textContent = r.error ? `not stopped: ${r.error}` : `${state.name} stops at its next operation`;
    setTimeout(loadList, 600);
  });

  $('new-form').addEventListener('submit', async (e) => {
    e.preventDefault();
    const name = $('new-name').value.trim();
    if (!validName(name)) { $('msg').textContent = 'a name is letters, digits and _ (it becomes <name>.rhai)'; return; }
    if (state.scripts.some((s) => s.name === name)) { open(name); return; }
    const text = `// ${name}: what this script does, in one line (its documentation on the Scripts page).\nimport "ttd" as ttd;\n\nprint(ttd::report());\n`;
    const r = await api('/api/script/save', { name, text });
    if (!r.saved) { $('msg').textContent = `not created: ${r.error?.message || r.error}`; return; }
    $('new-name').value = '';
    await loadList();
    open(name);
  });

  // The service's log, and the file as it is on disk (hot reload's other half: an edit outside).
  setInterval(async () => {
    if (!state.name) return;
    const s = state.scripts.find((x) => x.name === state.name);
    if (s?.service) {
      const r = await api(`/api/services/log?name=${encodeURIComponent(state.name)}&since=${state.seq}`);
      for (const l of r.lines || []) term($('svc-log'), l.text, toneOf(l.text));
      if (r.last_seq) state.seq = r.last_seq;
      if ((r.running ?? false) !== !!s.service.running || (r.lines || []).some((l) => /^== (reloaded|exit|started)/.test(l.text))) loadList();
    }
    const f = await api(`/api/script?name=${encodeURIComponent(state.name)}`);
    if (f.status === 200 && f.modified !== state.modified) {
      if (!dirty()) {
        ed.value = f.text; state.saved = f.text; state.modified = f.modified;
        showError(f.error); renderMeta();
        $('msg').textContent = `${state.name}.rhai changed on disk; loaded again`;
      } else {
        state.modified = f.modified;
        $('msg').textContent = `${state.name}.rhai changed on disk while you edit it: Save keeps yours, Revert after reloading takes theirs`;
      }
    }
  }, 1000);

  // The debugger (src/rhai_debug.rs, I0090): a session pauses at the gutter's breakpoints.
  const dbg = { id: 0, status: '', timer: 0 };
  function renderDebug(s) {
    dbg.status = s?.status || '';
    const ctl = debugControls(dbg.status);
    for (const [k, id] of [['start', 'dbg-start'], ['continue', 'dbg-continue'], ['step', 'dbg-step'], ['over', 'dbg-over'], ['out', 'dbg-out'], ['stop', 'dbg-stop']]) $(id).disabled = !ctl[k];
    const tone = { paused: 'yellow', running: 'blue', finished: s?.finished?.ok ? 'green' : 'red' }[dbg.status] || 'gray';
    const label = dbg.status === 'paused' ? `paused at line ${s.line}${s.source ? ` of ${s.source}` : ''}`
      : dbg.status === 'finished' ? (s.finished?.ok ? `finished: ${s.finished.value?.value ?? ''}` : (s.finished?.stopped ? 'stopped' : `error: ${s.finished?.error || ''}`)) : dbg.status;
    $('dbg-status').innerHTML = dbg.status ? badge(label, tone) : '';
    $('dbg-vars').innerHTML = dbg.status === 'paused' ? varRows(s.vars) : '';
    $('dbg-stack').innerHTML = dbg.status === 'paused' ? stackItems(s.stack, s.line) : '';
    $('dbg-out-log').textContent = (s?.output || []).join('\n');
    ed.paused?.(dbg.status === 'paused' && !s.source ? s.line : null);
    clearTimeout(dbg.timer);
    if (dbg.status === 'running') dbg.timer = setTimeout(async () => renderDebug(await api(`/api/rhai/debug?id=${dbg.id}`)), 500);
  }
  async function debugStart() {
    if (!debugControls(dbg.status).start) return;
    // The session's own `status` (paused, running, finished) overrides api()'s HTTP status: read `id`.
    const r = await api('/api/rhai/debug', { script: ed.value, breakpoints: ed.breakpoints?.() || [] });
    if (r.error || !r.id) { $('msg').textContent = `not debugged: ${r.error || 'no session'}`; return; }
    dbg.id = r.id;
    renderDebug(r);
  }
  async function debugCommand(command) {
    if (!dbg.id || !debugControls(dbg.status)[command === 'step' ? 'step' : command]) return;
    const r = await api('/api/rhai/debug/command', { id: dbg.id, command, breakpoints: ed.breakpoints?.() || [] });
    renderDebug(r.error ? (r.session || null) : r);
    if (r.error) $('msg').textContent = r.error;
  }
  $('dbg-start').addEventListener('click', debugStart);
  for (const c of ['continue', 'step', 'over', 'out', 'stop']) $(`dbg-${c}`).addEventListener('click', () => debugCommand(c));
  $('dbg-stack').addEventListener('click', (e) => { const l = Number(e.target.closest('[data-line]')?.dataset.line); if (l) ed.goto(l); });
  document.addEventListener('keydown', (e) => {
    const key = `${e.shiftKey ? 'Shift+' : ''}${e.key}`;
    const action = { F5: () => (dbg.status === 'paused' ? debugCommand('continue') : debugStart()), 'Shift+F5': () => debugCommand('stop'),
      F10: () => debugCommand('over'), F11: () => debugCommand('step'), 'Shift+F11': () => debugCommand('out') }[key];
    if (action) { e.preventDefault(); action(); }
  });
  renderDebug(null);

  // The shared memory, live: every second, under the typed prefix (src/rhai_shared.rs, I0091).
  async function loadShared() {
    const prefix = $('shared-filter').value.trim();
    const r = await api(`/api/rhai/shared${prefix ? `?prefix=${encodeURIComponent(prefix)}` : ''}`);
    $('shared-rows').innerHTML = sharedRows(r.entries);
    $('shared-count').textContent = `${r.count ?? 0}`;
  }
  $('shared-filter').addEventListener('input', loadShared);
  setInterval(loadShared, 1000);
  loadShared();

  loadList().then(() => open(state.name || state.scripts.find((s) => !s.library)?.name || '', !scriptOfPath(location.pathname) && !!state.scripts.length));
}

if (typeof document !== 'undefined' && document.getElementById('editor')) boot();
