/* RESEARCH-ONLY. Rhai as the Scripts page's editor understands it (I0089, research/rhai-ide.md):
   the token function of CodeMirror's StreamLanguage, completion and hover from the panel's function
   catalogue (GET /api/rhai/functions, src/rhai_catalogue.rs), and the parser's verdict as a
   diagnostic. Pure functions with no import, so node --test checks them (test/rhai-lang.test.mjs);
   static/rhai-editor.js wires them into CodeMirror. */

/** Rhai's keywords (the language reference's list). */
export const KEYWORDS = new Set(['let', 'const', 'fn', 'if', 'else', 'switch', 'do', 'while', 'loop', 'until', 'for', 'in',
  'break', 'continue', 'return', 'throw', 'try', 'catch', 'import', 'export', 'as', 'private', 'global', 'this', 'is_shared',
  'true', 'false', 'Fn', 'call', 'curry', 'type_of', 'print', 'debug', 'eval']);

/** The style names the token function returns, each mapped to a highlight tag by the editor. */
export const STYLES = ['comment', 'docComment', 'string', 'number', 'keyword', 'namespace', 'functionCall', 'definition', 'variable', 'operator'];

export const startState = () => ({ block: false, doc: false, afterFn: false });

/** The token function of a CodeMirror StreamLanguage: reads one token from `stream` and returns its
    style name (one of STYLES) or null. Block comments carry over lines in `state`. */
export function token(stream, state) {
  if (state.block) {
    if (stream.skipTo('*/')) { stream.next(); stream.next(); state.block = false; } else stream.skipToEnd();
    return state.doc ? 'docComment' : 'comment';
  }
  if (stream.eatSpace()) return null;
  if (stream.match('///')) { stream.skipToEnd(); return 'docComment'; }
  if (stream.match('//')) { stream.skipToEnd(); return 'comment'; }
  if (stream.match('/*')) {
    state.doc = stream.peek() === '*';
    state.block = true;
    return token(stream, state);
  }
  const quote = stream.peek();
  if (quote === '"' || quote === '`' || quote === "'") {
    stream.next();
    let escaped = false;
    for (;;) {
      const c = stream.next();
      if (c === undefined || c === null) break;
      if (!escaped && c === quote) break;
      escaped = !escaped && c === '\\';
    }
    return 'string';
  }
  if (stream.match(/^(?:0x[0-9a-fA-F_]+|0b[01_]+|0o[0-7_]+|\d[\d_]*(?:\.\d[\d_]*)?(?:e[+-]?\d+)?)/)) return 'number';
  const word = stream.match(/^[A-Za-z_][A-Za-z0-9_]*/);
  if (word) {
    const w = word[0];
    if (state.afterFn) { state.afterFn = false; return 'definition'; }
    if (KEYWORDS.has(w)) { if (w === 'fn') state.afterFn = true; return 'keyword'; }
    if (stream.match('::', false)) return 'namespace';
    if (stream.match(/^\s*\(/, false)) return 'functionCall';
    return 'variable';
  }
  state.afterFn = false;
  stream.next();
  return 'operator';
}

/** The aliases a script imports: `import "ttd" as t;` gives {t: "ttd"}; an import without `as`
    binds nothing callable by name. */
export function importAliases(text) {
  const out = {};
  for (const m of String(text).matchAll(/\bimport\s+"([^"]+)"\s+as\s+([A-Za-z_][A-Za-z0-9_]*)/g)) out[m[2]] = m[1];
  return out;
}

/** The functions a script defines itself: [{name, params, doc}], doc from the `///` lines above. */
export function localFunctions(text) {
  const lines = String(text).split('\n');
  const out = [];
  lines.forEach((line, i) => {
    const m = /^\s*(?:private\s+)?fn\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(([^)]*)\)/.exec(line);
    if (!m) return;
    const doc = [];
    for (let j = i - 1; j >= 0 && /^\s*\/\/\//.test(lines[j]); j--) doc.unshift(lines[j].replace(/^\s*\/\/\/\s?/, ''));
    out.push({ name: m[1], params: m[2].split(',').map((a) => a.trim()).filter(Boolean), doc: doc.join(' ') });
  });
  return out;
}

/** The word being typed before `pos` (identifier characters and `::`): {from, text}. */
export function wordBefore(text, pos) {
  const m = /[A-Za-z0-9_:]*$/.exec(String(text).slice(0, pos));
  return { from: pos - m[0].length, text: m[0] };
}

/** The whole word around `pos` (identifier characters and `::`): {from, to, text} or null. */
export function wordAt(text, pos) {
  const s = String(text);
  let from = pos;
  let to = pos;
  while (from > 0 && /[A-Za-z0-9_:]/.test(s[from - 1])) from--;
  while (to < s.length && /[A-Za-z0-9_:]/.test(s[to])) to++;
  if (from === to) return null;
  return { from, to, text: s.slice(from, to).replace(/^:+|:+$/g, '') };
}

const KIND_TYPE = { host: 'function', tool: 'method', route: 'method', console: 'function', hub: 'method', library: 'function', local: 'function' };

/** The completions for `prefix` in a script `text`: the catalogue's functions (a library function
    under each alias the script imports it as), the script's own functions and the keywords, best
    first: [{label, type, detail, info, kind}]. Nothing is listed here: every name comes from the
    catalogue or the text. */
export function completions(catalogue, text, prefix) {
  const p = String(prefix || '');
  const aliases = importAliases(text);
  const byModule = {};
  for (const [alias, mod] of Object.entries(aliases)) (byModule[mod] ||= []).push(alias);
  const out = [];
  const seen = new Set();
  const add = (label, kind, detail, info) => {
    if (seen.has(label) || !label.startsWith(p)) return;
    seen.add(label);
    out.push({ label, type: KIND_TYPE[kind] || 'keyword', kind, detail, info });
  };
  for (const f of localFunctions(text)) add(f.name, 'local', `${f.name}(${f.params.join(', ')})`, f.doc);
  for (const f of catalogue?.functions || []) {
    const sig = (f.signatures || [])[0] || f.name;
    if (f.kind === 'library') {
      const [mod, fn] = f.name.split('::');
      for (const alias of byModule[mod] || []) add(`${alias}::${fn}`, 'library', sig.replace(`${mod}::`, `${alias}::`), f.doc || '');
    } else add(f.name, f.kind, sig, f.doc || (f.route ? f.route : ''));
  }
  for (const k of KEYWORDS) add(k, 'keyword', 'keyword', '');
  const rank = (c) => (c.kind === 'local' ? 0 : c.kind === 'library' ? 1 : c.kind === 'host' ? 2 : c.kind === 'keyword' ? 4 : 3);
  return out.sort((a, b) => rank(a) - rank(b) || a.label.localeCompare(b.label));
}

/** What hovering `word` in `text` shows: {title, kind, signatures, doc, route, schema} or null. */
export function hoverInfo(catalogue, text, word) {
  const w = String(word || '');
  if (!w) return null;
  const local = localFunctions(text).find((f) => f.name === w);
  if (local) return { title: w, kind: 'local', signatures: [`${w}(${local.params.join(', ')})`], doc: local.doc };
  let name = w;
  const parts = w.split('::');
  if (parts.length === 2) {
    const mod = importAliases(text)[parts[0]];
    if (mod) name = `${mod}::${parts[1]}`;
  }
  const f = (catalogue?.functions || []).find((x) => x.name === name);
  if (!f) return KEYWORDS.has(w) ? { title: w, kind: 'keyword', signatures: [], doc: 'a Rhai keyword' } : null;
  return { title: w, kind: f.kind, signatures: f.signatures || [], doc: f.doc || '', route: f.route || '', schema: f.schema || null };
}

/** The parser's verdict (`{line, column, message}`, 1-based, from POST /api/script/check) as a
    diagnostic over the text: {from, to, severity, message}; the whole line when the column is
    past its end. null for no error. */
export function diagnostic(text, err) {
  if (!err) return null;
  const s = String(text);
  const lines = s.split('\n');
  const line = Math.min(Math.max(1, Number(err.line) || 1), lines.length);
  let start = 0;
  for (let i = 0; i < line - 1; i++) start += lines[i].length + 1;
  const len = lines[line - 1].length;
  const col = Math.max(1, Number(err.column) || 1);
  const from = start + Math.min(col - 1, len);
  const to = col - 1 < len ? Math.min(start + len, from + Math.max(1, (/^[A-Za-z0-9_]+/.exec(lines[line - 1].slice(col - 1)) || [''])[0].length)) : start + len;
  return { from: Math.min(from, to), to: Math.max(from, to), severity: 'error', message: String(err.message || 'does not parse') };
}

/** A catalogue entry as hover HTML (escaped): kind, signatures, route and doc. */
export function hoverHtml(info, esc = (t) => String(t).replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]))) {
  if (!info) return '';
  const sigs = (info.signatures || []).map((s) => `<div class="font-mono text-xs text-blue-300">${esc(s)}</div>`).join('');
  return `<div class="max-w-md p-2 text-sm"><div class="mb-1 text-xs uppercase tracking-wide text-gray-400">${esc(info.kind)}</div>${sigs || `<div class="font-mono text-xs text-blue-300">${esc(info.title)}</div>`}`
    + (info.route ? `<div class="mt-1 font-mono text-xs text-gray-400">${esc(info.route)}</div>` : '')
    + (info.doc ? `<div class="mt-1 text-gray-200 whitespace-pre-wrap">${esc(info.doc)}</div>` : '') + '</div>';
}
