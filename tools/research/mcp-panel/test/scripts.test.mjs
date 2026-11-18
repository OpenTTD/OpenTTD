// node --test tools/research/mcp-panel/test/ : the Scripts page (static/scripts.js): Rhai tokens and
// highlighting, the outline, indenting, the stream frames, the address, and the registry and page
// against static/scripts.html and every script of the library.
import './panel-ui.mjs'; // first: shell.js publishes PanelUI before any page script
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { createRequire } from 'node:module';
import {
  tokenize, highlightHtml, outline, lineAt, offsetOfLine, indent, parseSse, scriptOfPath, validName,
  scriptBadges, firstLine, KEYWORDS, TOKEN_CLASS,
} from '../static/scripts.js';

const here = path.dirname(new URL(import.meta.url).pathname);
const STATIC = path.join(here, '..', 'static');
const RHAI = path.join(here, '..', 'rhai');
const { PAGES, pageOf } = createRequire(import.meta.url)('../static/shell.js');

test('tokens join back to the text, character for character, for every script of the library', () => {
  const files = fs.readdirSync(RHAI).filter((f) => f.endsWith('.rhai'));
  assert.ok(files.length >= 5, 'the library is read from its folder');
  for (const f of files) {
    const text = fs.readFileSync(path.join(RHAI, f), 'utf8');
    assert.equal(tokenize(text).map((t) => t.v).join(''), text, f);
  }
});

test('a token kind follows from the text around it', () => {
  const kinds = (s) => tokenize(s).filter((t) => t.k !== 'ws').map((t) => `${t.k}:${t.v}`);
  assert.deepEqual(kinds('let x = rl::target(line, 2.5); // why'),
    ['keyword:let', 'ident:x', 'op:=', 'module:rl', 'op:::', 'call:target', 'op:(', 'ident:line', 'op:,', 'number:2.5', 'op:)', 'op:;', 'comment:// why']);
  assert.deepEqual(kinds('/// doc\nfn f() { `a ${b}` + "c\\"d" }'),
    ['doc:/// doc', 'keyword:fn', 'call:f', 'op:(', 'op:)', 'op:{', 'string:`a ${b}`', 'op:+', 'string:"c\\"d"', 'op:}']);
  assert.ok(KEYWORDS.has('import') && KEYWORDS.has('loop'));
  for (const k of ['comment', 'doc', 'string', 'number', 'keyword', 'module', 'call', 'ident', 'op']) assert.ok(k in TOKEN_CLASS, k);
});

test('the highlighted copy is escaped and keeps a last empty line', () => {
  const html = highlightHtml('let a = "<b>";\n');
  assert.match(html, /&lt;b&gt;/);
  assert.doesNotMatch(html, /<b>/);
  assert.ok(html.endsWith(' '), 'a trailing newline still takes a line in the <pre>');
});

test('the outline lists every fn with its line, arguments and /// doc', () => {
  const text = 'import "x" as x;\n/// The answer.\n/// Twice.\nfn answer(a, b) { 42 }\n\nprivate fn hidden() {}\n';
  assert.deepEqual(outline(text), [
    { name: 'answer', args: ['a', 'b'], line: 4, doc: 'The answer. Twice.' },
    { name: 'hidden', args: [], line: 6, doc: '' },
  ]);
  const lib = fs.readFileSync(path.join(RHAI, 'rail_lib.rhai'), 'utf8');
  const names = outline(lib).map((f) => f.name);
  for (const n of ['parse_record', 'lines_of', 'target', 'choose', 'renewals', 'problems']) assert.ok(names.includes(n), n);
  assert.ok(outline(lib).every((f) => f.doc), 'every function of rail_lib carries a /// doc');
});

test('lines and offsets agree', () => {
  const t = 'a\nbb\nccc';
  assert.equal(lineAt(t, 0), 1);
  assert.equal(lineAt(t, 3), 2);
  assert.equal(offsetOfLine(t, 3), 5);
  assert.equal(t.slice(offsetOfLine(t, 2), offsetOfLine(t, 3)), 'bb\n');
  assert.equal(offsetOfLine(t, 99), t.length);
});

test('Tab indents the caret or every selected line, Shift+Tab takes it off', () => {
  assert.deepEqual(indent('ab', 1, 1), { text: 'a    b', start: 5, end: 5 });
  const r = indent('x\ny\nz', 0, 3);
  assert.equal(r.text, '    x\n    y\nz');
  const back = indent(r.text, 0, r.end, true);
  assert.equal(back.text, 'x\ny\nz');
});

test('the run stream is read frame by frame, a partial frame kept for the next chunk', () => {
  const { frames, rest } = parseSse('event: out\ndata: "hello \\"x\\""\n\nevent: err\ndata: "mcp-rhai: boom"\n\nevent: exit\ndata: 1\n\nevent: out\nda');
  assert.deepEqual(frames.map((f) => [f.event, f.text]), [['out', 'hello "x"'], ['err', 'mcp-rhai: boom'], ['exit', '1']]);
  assert.equal(rest, 'event: out\nda');
});

test('a script is in the address, and a name is what the server accepts', () => {
  assert.equal(scriptOfPath('/scripts/rail_steward'), 'rail_steward');
  assert.equal(scriptOfPath('/scripts'), '');
  assert.equal(pageOf('/scripts/rail_lib'), 'scripts');
  assert.ok(PAGES.some((p) => p.key === 'scripts' && p.href === '/scripts'), 'the shell registry has the page');
  assert.ok(validName('rail_lib') && !validName('../x') && !validName('a.rhai') && !validName(''));
});

test('badges say library, service state and reloads', () => {
  assert.deepEqual(scriptBadges({ library: true }), [['library', 'purple']]);
  assert.deepEqual(scriptBadges({ service: { running: true, reloads: 2 } }), [['service running', 'green'], ['2 reloads', 'blue']]);
  assert.deepEqual(scriptBadges({ service: { running: false, reloads: 1 } }), [['service stopped', 'gray'], ['1 reload', 'blue']]);
  assert.equal(firstLine('\n  \nRail steward: x\nmore'), 'Rail steward: x');
});

test('the page has every element its script reads', () => {
  const html = fs.readFileSync(path.join(STATIC, 'scripts.html'), 'utf8');
  const js = fs.readFileSync(path.join(STATIC, 'scripts.js'), 'utf8');
  const ids = new Set([...js.matchAll(/\$\('([a-z0-9-]+)'\)/g)].map((m) => m[1]));
  for (const id of ids) assert.match(html, new RegExp(`id="${id}"`), `#${id}`);
});
