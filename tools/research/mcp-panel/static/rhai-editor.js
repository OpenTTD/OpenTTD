/* RESEARCH-ONLY. The Scripts page's editor on CodeMirror 6 (I0089, research/rhai-ide.md): Rhai
   highlighting (static/rhai-lang.js token), the parser's error as a lint diagnostic, completion and
   hover from GET /api/rhai/functions, line numbers, folding, bracket matching, search, history and
   the page's keys (Ctrl/Cmd+S saves, Ctrl/Cmd+Enter runs, Escape stops a run).
   CodeMirror comes from the panel's own node_modules through its import map (src/panel_npm.rs);
   scripts.js falls back to its plain editor when it cannot load. Same interface as that editor:
   {value, focus(), markError(err), goto(line)}. */

import { EditorView, keymap, lineNumbers, highlightActiveLine, highlightActiveLineGutter, drawSelection, hoverTooltip } from '@codemirror/view';
import { EditorState, Compartment } from '@codemirror/state';
import { StreamLanguage, syntaxHighlighting, HighlightStyle, bracketMatching, indentOnInput, foldGutter, indentUnit } from '@codemirror/language';
import { defaultKeymap, history, historyKeymap, indentWithTab } from '@codemirror/commands';
import { autocompletion, closeBrackets, closeBracketsKeymap, completionKeymap } from '@codemirror/autocomplete';
import { setDiagnostics, lintGutter } from '@codemirror/lint';
import { searchKeymap, highlightSelectionMatches } from '@codemirror/search';
import { tags as t } from '@lezer/highlight';
import { token, startState, completions, hoverInfo, hoverHtml, wordBefore, wordAt, diagnostic } from './rhai-lang.js';

/** Rhai for CodeMirror: the token function of rhai-lang.js, each style a highlight tag. */
export const rhai = StreamLanguage.define({
  name: 'rhai',
  startState,
  token,
  copyState: (s) => ({ ...s }),
  languageData: { commentTokens: { line: '//', block: { open: '/*', close: '*/' } }, closeBrackets: { brackets: ['(', '[', '{', '"', '`'] } },
  tokenTable: {
    comment: t.lineComment, docComment: t.docComment, string: t.string, number: t.number, keyword: t.keyword,
    namespace: t.namespace, functionCall: t.function(t.variableName), definition: t.definition(t.function(t.variableName)),
    variable: t.variableName, operator: t.operator,
  },
});

/** The page's token tones (the Flowbite palette of the plain editor), on the dark editor surface. */
export const rhaiHighlight = HighlightStyle.define([
  { tag: t.lineComment, color: '#6b7280', fontStyle: 'italic' },
  { tag: t.docComment, color: '#4ade80', fontStyle: 'italic' },
  { tag: t.string, color: '#fcd34d' },
  { tag: t.number, color: '#d8b4fe' },
  { tag: t.keyword, color: '#f472b6', fontWeight: '600' },
  { tag: t.namespace, color: '#67e8f9' },
  { tag: t.function(t.variableName), color: '#93c5fd' },
  { tag: t.definition(t.function(t.variableName)), color: '#93c5fd', fontWeight: '600' },
  { tag: t.variableName, color: '#f3f4f6' },
  { tag: t.operator, color: '#9ca3af' },
]);

const theme = EditorView.theme({
  '&': { height: '100%', backgroundColor: '#111827', color: '#f3f4f6', fontSize: '14px' },
  '.cm-content': { fontFamily: 'ui-monospace, SFMono-Regular, Menlo, monospace', caretColor: '#fff' },
  '.cm-gutters': { backgroundColor: '#1f2937', color: '#6b7280', borderRight: '1px solid #374151' },
  '.cm-activeLine': { backgroundColor: '#1f293780' },
  '.cm-activeLineGutter': { backgroundColor: '#374151' },
  '&.cm-focused .cm-selectionBackground, .cm-selectionBackground': { backgroundColor: '#1e40af99' },
  '.cm-tooltip': { backgroundColor: '#1f2937', border: '1px solid #374151', color: '#f3f4f6', borderRadius: '0.5rem' },
  '.cm-tooltip-autocomplete ul li[aria-selected]': { backgroundColor: '#1d4ed8' },
  '.cm-scroller': { overflow: 'auto' },
}, { dark: true });

/** The catalogue of GET /api/rhai/functions, fetched once and again every 30 seconds. */
function catalogueSource(fetcher = fetch) {
  let cat = { functions: [] };
  let at = 0;
  const load = async () => {
    if (Date.now() - at < 30000) return cat;
    at = Date.now();
    try { const r = await fetcher('/api/rhai/functions'); if (r.ok) cat = await r.json(); } catch { /* the last one stays */ }
    return cat;
  };
  return { load, get: () => cat };
}

/** The editor in `host`, with the page's callbacks. */
export function rhaiEditor(host, { onChange, onSave, onRun, onStop } = {}) {
  host.className = 'relative w-full overflow-hidden border border-gray-300 rounded-lg dark:border-gray-600 focus-within:ring-2 focus-within:ring-primary-500 h-[60vh] min-h-80';
  host.innerHTML = '';
  host.setAttribute('data-editor', 'codemirror');
  const cat = catalogueSource();
  cat.load();
  const complete = async (ctx) => {
    const w = wordBefore(ctx.state.doc.toString(), ctx.pos);
    if (!w.text && !ctx.explicit) return null;
    const c = await cat.load();
    const options = completions(c, ctx.state.doc.toString(), w.text).slice(0, 200).map((o) => ({
      label: o.label, type: o.type, detail: o.detail, info: o.info || undefined, boost: o.kind === 'local' ? 2 : 0,
    }));
    return { from: w.from, options, validFor: /^[A-Za-z0-9_:]*$/ };
  };
  const hover = hoverTooltip((view, pos) => {
    const w = wordAt(view.state.doc.toString(), pos);
    if (!w) return null;
    const info = hoverInfo(cat.get(), view.state.doc.toString(), w.text);
    if (!info) return null;
    return { pos: w.from, end: w.to, above: true, create: () => { const dom = document.createElement('div'); dom.innerHTML = hoverHtml(info); return { dom }; } };
  });
  const keys = keymap.of([
    { key: 'Mod-s', preventDefault: true, run: () => { onSave?.(); return true; } },
    { key: 'Mod-Enter', preventDefault: true, run: () => { onRun?.(); return true; } },
    { key: 'Escape', run: () => { onStop?.(); return false; } },
    indentWithTab, ...closeBracketsKeymap, ...completionKeymap, ...searchKeymap, ...historyKeymap, ...defaultKeymap,
  ]);
  let silent = false;
  const view = new EditorView({
    parent: host,
    state: EditorState.create({
      doc: '',
      extensions: [
        keys, lineNumbers(), highlightActiveLineGutter(), foldGutter(), lintGutter(), history(), drawSelection(),
        indentOnInput(), indentUnit.of('    '), bracketMatching(), closeBrackets(), highlightActiveLine(), highlightSelectionMatches(),
        rhai, syntaxHighlighting(rhaiHighlight), theme, autocompletion({ override: [complete] }), hover,
        EditorView.contentAttributes.of({ 'aria-label': 'Script text', spellcheck: 'false' }),
        EditorView.updateListener.of((u) => { if (u.docChanged && !silent) onChange?.(view.state.doc.toString()); }),
      ],
    }),
  });
  return {
    view,
    get value() { return view.state.doc.toString(); },
    set value(v) {
      silent = true;
      view.dispatch({ changes: { from: 0, to: view.state.doc.length, insert: String(v ?? '') }, selection: { anchor: 0 } });
      silent = false;
    },
    focus: () => view.focus(),
    /** The parser's error ({line, column, message}) as a diagnostic, or none for null. */
    markError(err) {
      const d = err && typeof err === 'object' ? diagnostic(view.state.doc.toString(), err) : null;
      view.dispatch(setDiagnostics(view.state, d ? [d] : []));
    },
    goto(line) {
      const l = view.state.doc.line(Math.min(Math.max(1, line), view.state.doc.lines));
      view.dispatch({ selection: { anchor: l.from }, effects: EditorView.scrollIntoView(l.from, { y: 'center' }) });
      view.focus();
    },
  };
}

export { Compartment };
