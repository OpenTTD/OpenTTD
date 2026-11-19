// node --test tools/research/mcp-panel/test/ : Rhai in the Scripts page's editor (static/rhai-lang.js,
// static/rhai-editor.js; I0089, research/rhai-ide.md). The token function against a minimal
// stream, completion, hover and diagnostics against a catalogue shaped like GET /api/rhai/functions,
// and the CodeMirror language itself highlighting a document through CodeMirror's own parse tree.
import test from 'node:test';
import assert from 'node:assert/strict';
import { token, startState, STYLES, KEYWORDS, completions, hoverInfo, hoverHtml, diagnostic, importAliases, localFunctions, wordBefore, wordAt } from '../static/rhai-lang.js';

/** A minimal StringStream (the part of CodeMirror's that token() uses), over one line. */
function stream(line) {
  return {
    string: line, pos: 0, start: 0,
    eol() { return this.pos >= this.string.length; },
    peek() { return this.string.charAt(this.pos) || undefined; },
    next() { if (this.pos < this.string.length) return this.string.charAt(this.pos++); return undefined; },
    eatSpace() { const s = this.pos; while (/\s/.test(this.string.charAt(this.pos))) this.pos++; return this.pos > s; },
    skipToEnd() { this.pos = this.string.length; },
    skipTo(ch) { const i = this.string.indexOf(ch, this.pos); if (i > -1) { this.pos = i; return true; } return false; },
    match(p, consume = true) {
      if (typeof p === 'string') { const ok = this.string.startsWith(p, this.pos); if (ok && consume) this.pos += p.length; return ok; }
      const m = this.string.slice(this.pos).match(p);
      if (m && m.index > 0) return null;
      if (m && consume) this.pos += m[0].length;
      return m;
    },
  };
}

/** Every token of some lines: [[text, style]], whitespace left out. */
function tokens(lines) {
  const st = startState();
  const out = [];
  for (const line of lines) {
    const s = stream(line);
    while (!s.eol()) {
      s.start = s.pos;
      const style = token(s, st);
      assert.ok(s.pos > s.start, `token() advances on ${JSON.stringify(line.slice(s.start))}`);
      if (style) out.push([line.slice(s.start, s.pos), style]);
    }
  }
  return out;
}

test('the token function names keywords, calls, modules, definitions, strings, numbers and comments', () => {
  const t = tokens(['import "ttd" as ttd;', 'fn setup(seed) { let n = 0x1F + 2.5e3; ttd::new_game(seed); }', '/// doc', '// note']);
  const of = (text) => t.find(([x]) => x === text)?.[1];
  assert.equal(of('import'), 'keyword');
  assert.equal(of('"ttd"'), 'string');
  assert.equal(of('setup'), 'definition');
  assert.equal(of('seed'), 'variable');
  assert.equal(of('0x1F'), 'number');
  assert.equal(of('2.5e3'), 'number');
  assert.ok(t.some(([x, s]) => x === 'ttd' && s === 'namespace'));
  assert.equal(of('new_game'), 'functionCall');
  assert.equal(of('/// doc'), 'docComment');
  assert.equal(of('// note'), 'comment');
  for (const [, s] of t) assert.ok(STYLES.includes(s), s);
});

test('block comments and strings carry escapes and span lines', () => {
  const t = tokens(['let s = "a \\" b"; /* one', 'two */ let x = `t`;', "/** doc */"]);
  assert.ok(t.some(([x, s]) => x === '"a \\" b"' && s === 'string'));
  assert.ok(t.some(([x, s]) => x === '/* one' && s === 'comment'));
  assert.ok(t.some(([x, s]) => x === 'two */' && s === 'comment'));
  assert.ok(t.some(([x, s]) => x === '`t`' && s === 'string'));
  assert.ok(t.some(([x, s]) => x === '/** doc */' && s === 'docComment'));
  assert.ok(tokens(['"unterminated']).some(([, s]) => s === 'string'));
});

const CAT = {
  functions: [
    { name: 'rpc', kind: 'host', doc: 'rpc(method) -> map raw MCP JSON-RPC answer', signatures: ['rpc(method: &str) -> Dynamic', 'rpc(method: &str, params: Map) -> Dynamic'] },
    { name: 'read', kind: 'host', doc: 'one resource', signatures: ['read(uri: &str) -> Dynamic'] },
    { name: 'openttd_game_set_pause', kind: 'tool', doc: 'Pause the game', signatures: ['openttd_game_set_pause(args: Map) -> Dynamic'], schema: { type: 'object' } },
    { name: 'api_bots', kind: 'route', doc: 'Every bot', route: 'GET /api/bots', signatures: ['api_bots() -> Dynamic'] },
    { name: 'ttd::new_game', kind: 'library', module: 'ttd', doc: 'A new world.', signatures: ['ttd::new_game(seed)'] },
  ],
};

test('completion offers the catalogue under the script\'s own aliases, its own functions and keywords', () => {
  const text = 'import "ttd" as t;\n/// Mine.\nfn report_all(x) { }\nr';
  const labels = (p) => completions(CAT, text, p).map((c) => c.label);
  assert.deepEqual(labels('r'), ['report_all', 'read', 'rpc', 'return']);
  assert.deepEqual(labels('t::'), ['t::new_game']);
  assert.equal(completions(CAT, text, 't::')[0].detail, 't::new_game(seed)');
  assert.ok(!labels('').includes('ttd::new_game'), 'a library function only under an imported alias');
  assert.equal(completions(CAT, text, 'api_')[0].info, 'Every bot');
  assert.equal(completions(CAT, text, 'report')[0].info, 'Mine.');
  assert.equal(completions(null, '', 'whi')[0].label, 'while');
  assert.ok(completions(CAT, text, '').length >= CAT.functions.length - 1 + KEYWORDS.size);
});

test('hover resolves aliases, locals, keywords and the catalogue', () => {
  const text = 'import "ttd" as t;\nfn go(a, b) {}\n';
  assert.equal(hoverInfo(CAT, text, 't::new_game').doc, 'A new world.');
  assert.deepEqual(hoverInfo(CAT, text, 'go').signatures, ['go(a, b)']);
  assert.equal(hoverInfo(CAT, text, 'rpc').signatures.length, 2);
  assert.equal(hoverInfo(CAT, text, 'api_bots').route, 'GET /api/bots');
  assert.equal(hoverInfo(CAT, text, 'while').kind, 'keyword');
  assert.equal(hoverInfo(CAT, text, 'nothing'), null);
  assert.equal(hoverInfo(CAT, text, ''), null);
  const html = hoverHtml(hoverInfo(CAT, text, 'api_bots'));
  assert.match(html, /GET \/api\/bots/);
  assert.equal(hoverHtml({ kind: 'x<', title: 'a&b', signatures: [], doc: '<i>' }).includes('<i>'), false);
  assert.equal(hoverHtml(null), '');
});

test('the parser\'s error becomes a diagnostic on its line and column', () => {
  const text = 'let a = 1;\nlet b = foo(;\n';
  const d = diagnostic(text, { line: 2, column: 9, message: 'oops' });
  assert.deepEqual([text.slice(d.from, d.to), d.severity, d.message], ['foo', 'error', 'oops']);
  const past = diagnostic(text, { line: 2, column: 99, message: 'x' });
  assert.equal(text.slice(past.from, past.to), '');
  assert.equal(past.from, text.indexOf('\n', 11));
  const clamp = diagnostic(text, { line: 99, column: 1 });
  assert.ok(clamp.from <= text.length && clamp.message === 'does not parse');
  assert.equal(diagnostic(text, null), null);
});

test('words, aliases and local functions are read from the text', () => {
  assert.deepEqual(importAliases('import "a" as x; import "b";import "c" as y'), { x: 'a', y: 'c' });
  assert.deepEqual(localFunctions('/// d1\n/// d2\nprivate fn f(a, b) {}\nfn g() {}').map((f) => [f.name, f.params, f.doc]), [['f', ['a', 'b'], 'd1 d2'], ['g', [], '']]);
  assert.deepEqual(wordBefore('x = ttd::ne', 11), { from: 4, text: 'ttd::ne' });
  assert.deepEqual(wordAt('call ttd::new_game(1)', 12), { from: 5, to: 18, text: 'ttd::new_game' });
  assert.equal(wordAt('a  b', 2), null);
});

test('CodeMirror highlights a Rhai document through the language and the page\'s tones', async () => {
  const { EditorState } = await import('@codemirror/state');
  const { ensureSyntaxTree } = await import('@codemirror/language');
  const { highlightTree } = await import('@lezer/highlight');
  const { rhai, rhaiHighlight } = await import('../static/rhai-editor.js');
  const doc = 'fn go(x) { ttd::report(x) } // done';
  const state = EditorState.create({ doc, extensions: [rhai] });
  const tree = ensureSyntaxTree(state, doc.length, 5000);
  assert.ok(tree, 'the stream language parses the whole document');
  const spans = [];
  highlightTree(tree, rhaiHighlight, (from, to, cls) => spans.push([doc.slice(from, to), cls]));
  const cls = (text) => spans.find(([x]) => x === text)?.[1];
  assert.ok(cls('fn') && cls('go') && cls('ttd') && cls('report') && cls('// done'), JSON.stringify(spans));
  assert.notEqual(cls('fn'), cls('go'));
  assert.notEqual(cls('ttd'), cls('report'));
});
