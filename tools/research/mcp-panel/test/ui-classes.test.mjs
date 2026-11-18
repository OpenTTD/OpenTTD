// node --test tools/research/mcp-panel/test/ : markup drawn from JS takes the shell's class sets
// (static/shell.js UI, one per role) and never a hand-typed copy of one (rule
// project.panel-items-are-routable-pages: one shell, derive, extend the shared mechanism).
// shell.test.mjs holds every static/*.html to the same sets; this holds every static/*.js.
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { UI } from './panel-ui.mjs';

const here = path.dirname(new URL(import.meta.url).pathname);
const STATIC = path.join(here, '..', 'static');
// Every script in static/, derived, not listed; shell.js is where the sets are declared.
const SCRIPTS = fs
  .readdirSync(STATIC)
  .filter((f) => f.endsWith('.js') && f !== 'shell.js')
  .sort();
// Every role, derived from UI itself: its name and its class set.
const ROLES = Object.entries(UI).map(([role, cls]) => [role, new Set(cls.split(/\s+/))]);
const CLASS = /^[!a-z-][^\s'"`<>{}$]*$/;

/** Every class list a script writes literally: class="..." values and class-like '...' strings,
    each with its line and literal tokens (an interpolation such as ${UI.card} is not one). */
function classLists(src) {
  const out = [];
  const add = (at, text) => {
    const tokens = new Set(
      text
        .replace(/\$\{[^}]*\}/g, ' ')
        .split(/\s+/)
        .filter(Boolean),
    );
    if (tokens.size >= 3 && [...tokens].every((t) => CLASS.test(t))) {
      out.push({ line: src.slice(0, at).split('\n').length, text, tokens });
    }
  };
  for (const m of src.matchAll(/class="([^"]*)"/g)) add(m.index, m[1]);
  for (const m of src.matchAll(/'([^'\n\\]*)'/g)) add(m.index, m[1]);
  return out;
}

/** The role a class list copies: the one it shares most of, when that is at least 60% of the
    role's set and at least four classes. Fewer is a list of its own (a muted line, a cell). */
function copiedRole(tokens) {
  let best = null;
  for (const [role, set] of ROLES) {
    const hits = [...set].filter((c) => tokens.has(c)).length;
    const share = hits / set.size;
    if (hits >= 4 && share >= 0.6 && (!best || share > best.share))
      best = { role, share, hits, size: set.size };
  }
  return best;
}

test('the check knows a copy when it sees one, and a list of its own when it does not', () => {
  const copy = new Set(`${UI.primary.split(' ').slice(0, 12).join(' ')} px-5 py-2.5`.split(' '));
  assert.equal(copiedRole(copy)?.role, 'primary', 'a primary button with its padding changed');
  assert.equal(copiedRole(new Set(UI.thead.split(' '))).role, 'thead');
  assert.equal(
    copiedRole(new Set('text-sm text-gray-500 dark:text-gray-400'.split(' '))),
    null,
    'a muted line',
  );
  assert.equal(
    copiedRole(new Set('px-3 py-2 font-medium text-gray-900 dark:text-white'.split(' '))),
    null,
    'a cell',
  );
  const lists = classLists(
    'x = `<b class="${UI.card} mt-2">`; y = \'w-full text-sm text-left text-gray-500\';',
  );
  assert.deepEqual(
    lists.map((l) => [...l.tokens]),
    [['w-full', 'text-sm', 'text-left', 'text-gray-500']],
    'an interpolated set is not literal; a quoted class list is',
  );
});

for (const f of SCRIPTS) {
  test(`${f}: no hand-typed copy of a role the shell's UI declares`, () => {
    const src = fs.readFileSync(path.join(STATIC, f), 'utf8');
    const copies = classLists(src)
      .map((l) => ({ ...l, copy: copiedRole(l.tokens) }))
      .filter((l) => l.copy)
      .map((l) => {
        const { hits, size, role } = l.copy;
        const what = `${hits}/${size} of UI.${role}, use \${UI.${role}}`;
        return `line ${l.line}: ${what}: ${l.text.slice(0, 70)}`;
      });
    assert.deepEqual(copies, []);
  });

  test(`${f}: a card title is UI.cardTitle, and every UI role it names exists`, () => {
    const src = fs.readFileSync(path.join(STATIC, f), 'utf8');
    // The same role shell.test.mjs gives an <h2> of a page (a visible one is a card title).
    for (const m of src.matchAll(/<h2\b[^>]*>/g)) {
      if (/class="[^"]*\bsr-only\b/.test(m[0])) continue;
      assert.match(
        m[0],
        /class="[^"]*\$\{UI\.cardTitle\}/,
        `line ${src.slice(0, m.index).split('\n').length}: ${m[0]}`,
      );
    }
    for (const m of src.matchAll(/\bUI\.([A-Za-z]+)/g))
      assert.ok(m[1] in UI, `UI.${m[1]} is no role of the shell`);
  });
}
