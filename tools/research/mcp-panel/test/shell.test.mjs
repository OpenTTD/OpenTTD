// node --test tools/research/mcp-panel/test/ : the one shell (static/shell.js) and the rule that every
// panel page uses it (project.panel-items-are-routable-pages): no page draws its own navbar or sidebar.
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { createRequire } from 'node:module';

const here = path.dirname(new URL(import.meta.url).pathname);
const STATIC = path.join(here, '..', 'static');
const { PAGES, FRAME, pageOf, sidebarHtml, sectionsHtml, navbarHtml, asideHtml } = createRequire(import.meta.url)('../static/shell.js');

// Every page in static/, derived, not listed: the panel's (/, /ui, /world, the /tools and /results templates)
// and the hub's (/, /viz). A new page that skips the shell fails here.
const PANEL_PAGES = fs.readdirSync(STATIC).filter((f) => f.endsWith('.html')).sort();
const { APPS } = createRequire(import.meta.url)('../static/shell.js');

test('a path belongs to its page: item pages to the catalogue, runs to the duels', () => {
  assert.equal(pageOf('/'), 'home');
  assert.equal(pageOf('/world'), 'world');
  assert.equal(pageOf('/tool/openttd.ai.list'), 'tools');
  assert.equal(pageOf('/resource/map/terrain'), 'tools');
  assert.equal(pageOf('/result/duel-20261117T112500-seed31337'), 'results');
  assert.equal(pageOf('/nope'), '');
});

test('the sidebar lists every page once and marks only the current one', () => {
  const html = sidebarHtml('world');
  for (const p of PAGES) assert.equal(html.split(`href="${p.href}"`).length - 1, 1, p.href);
  assert.equal(html.split('aria-current="page"').length - 1, 1);
  assert.match(html, /href="\/world" aria-current="page"/);
});

test('"On this page" is derived from the sections given, and absent without any', () => {
  assert.equal(sectionsHtml([]), '');
  const html = sectionsHtml([{ id: 'yards', label: 'Transfer yards <proof>' }]);
  assert.match(html, /href="#yards"/);
  assert.match(html, /Transfer yards &lt;proof&gt;/, 'labels are escaped');
});

test('the shell carries the elements every page relies on', () => {
  const shell = navbarHtml() + asideHtml('home');
  for (const id of ['shell-nav', 'conn', 'nav-extra', 'open-console', 'sidebar', 'side-console', 'shell-sections']) {
    assert.match(shell, new RegExp(`id="${id}"`), id);
  }
  assert.match(shell, /data-drawer-toggle="sidebar"/, 'the Flowbite drawer opens the sidebar on a phone');
  assert.deepEqual(FRAME, ['pt-20', 'px-4', 'max-w-screen-2xl', 'mx-auto', 'lg:ml-64']);
});

for (const name of PANEL_PAGES) {
  test(`${name} uses the one shell and draws no navbar or sidebar of its own`, () => {
    const html = fs.readFileSync(path.join(STATIC, name), 'utf8');
    assert.doesNotMatch(html, /<nav class="fixed/, 'a page of its own navbar');
    assert.doesNotMatch(html, /<aside/, 'a sidebar of its own');
    const body = /<body[^>]*\sdata-page="([^"]+)"[^>]*>\s*<script src="\/static\/shell\.js"><\/script>/.exec(html);
    assert.ok(body, 'shell.js is the first element of <body>, which names its page');
    const app = /data-app="([^"]+)"/.exec(html)?.[1] || 'panel';
    assert.ok(APPS[app], `data-app="${app}" is an app of the shell`);
    assert.ok(APPS[app].pages.some((p) => p.key === body[1]), `data-page="${body[1]}" is a page of ${app}`);
    const main = /<main(?: class="([^"]*)")?>/.exec(html);
    assert.ok(main, 'one <main>');
    for (const cls of FRAME) assert.ok(!(main[1] || '').split(/\s+/).includes(cls), `${cls} comes from the shell, not the page`);
    for (const m of html.matchAll(/<[^>]+data-nav="[^"]+"[^>]*>/g)) assert.match(m[0], /\sid="[^"]+"/, 'a data-nav section has an id to link to');
  });
}

for (const name of PANEL_PAGES) {
  test(`${name}: its classic scripts share one scope without a clashing name`, async () => {
    // Derived from the page itself: every local classic script it loads, in order, compiled as one
    // script, so a top-level const declared twice (shell.js and app.js both had esc) is a SyntaxError.
    const vm = await import('node:vm');
    const html = fs.readFileSync(path.join(STATIC, name), 'utf8');
    const srcs = [...html.matchAll(/<script(?![^>]*type="module")[^>]*src="\/static\/([^"]+\.js)"[^>]*><\/script>/g)].map((m) => m[1]);
    assert.ok(srcs.includes('shell.js'));
    const code = srcs.map((f) => fs.readFileSync(path.join(STATIC, f), 'utf8')).join('\n;\n');
    assert.doesNotThrow(() => new vm.Script(code, { filename: `${name} classic scripts` }), srcs.join(' + '));
  });
}

test('the hub has its own pages, brand and no console, through the same shell', () => {
  const nav = navbarHtml(APPS.hub);
  assert.match(nav, />MCP Hub</);
  assert.doesNotMatch(nav, /open-console/);
  assert.match(asideHtml('viz', APPS.hub), /href="\/viz" aria-current="page"/);
  assert.equal(pageOf('/viz', 'hub'), 'viz');
});

/* ---- the page header and the Pro class sets, centralised in the shell ------------------------- */
const { UI, crumbsHtml, headerHtml } = createRequire(import.meta.url)('../static/shell.js');

test('the page header is the shell\'s: breadcrumb from the registry, H1, description, badges', () => {
  const html = headerHtml({ current: 'world', path: '/world', title: 'World <map>' });
  for (const id of ['page-header', 'crumbs', 'title', 'description', 'badges']) assert.match(html, new RegExp(`id="${id}"`), id);
  assert.match(html, /<nav class="flex mb-5" aria-label="Breadcrumb">/);
  assert.match(html, /World &lt;map&gt;<\/h1>/, 'the title is escaped');
  for (const cls of UI.h1.split(' ')) assert.match(html, new RegExp(`<h1 id="title" class="[^"]*(?<=["\\s])${cls.replace(/[:]/g, '\\$&')}(?=[\\s"])`), cls);
  // Home first, then the page: the current page is a span, a page above the path a link.
  const here = crumbsHtml('world', '/world');
  assert.match(here, /^<li data-crumb class="inline-flex items-center"><a href="\/" [^>]*>.*Panel<\/a><\/li>/);
  assert.match(here, /<span [^>]*aria-current="page">World<\/span>/);
  assert.match(crumbsHtml('tools', '/tool/openttd.ai.list'), /<a href="\/tools" [^>]*>Catalogue<\/a>/, 'a tool page links back to the catalogue');
  assert.equal(crumbsHtml('home', '/').split('data-crumb').length - 1, 1, 'the home page is only its home crumb');
  assert.match(crumbsHtml('viz', '/viz', APPS.hub), />Hub<\/a>.*>World view<\/span>/s, 'the hub derives its own');
  assert.match(headerHtml({ current: 'ui', path: '/ui' }), /<h1 [^>]*>Interface: every window<\/h1>/, 'without a title, the page label');
});

/** Every tag of a page with its class list: [{tag, attrs, cls: Set}]. */
const tags = (html) => [...html.matchAll(/<([a-z][a-z0-9]*)\b([^>]*)>/g)].map(([, tag, attrs]) => (
  { tag, attrs, cls: new Set((/\sclass="([^"]*)"/.exec(attrs)?.[1] || '').split(/\s+/).filter(Boolean)) }));
const lacks = (el, set) => set.split(' ').filter((c) => !el.cls.has(c));

for (const name of PANEL_PAGES) {
  test(`${name}: no page header of its own, and every card, title, button, field and table in the Pro set`, () => {
    const html = fs.readFileSync(path.join(STATIC, name), 'utf8');
    assert.doesNotMatch(html, /aria-label="Breadcrumb"/, 'a breadcrumb of its own: the shell draws it');
    assert.doesNotMatch(html, /<h1\b/, 'an H1 of its own: the shell draws it from data-title or the page label');
    assert.doesNotMatch(html, /\sid="(page-header|crumbs|title|description|badges)"/, 'an id the shell header owns');
    if (/<title>__TITLE__/.test(html)) {
      assert.match(html, /<body[^>]*\sdata-title="__TITLE__"/, 'a server-side title reaches the header');
      assert.match(html, /<body[^>]*\sdata-description="__DESCRIPTION__"/, 'and its description');
    }
    const roles = [
      ['card', (e) => e.cls.has('bg-white') && e.cls.has('rounded-lg') && e.cls.has('border-gray-200')],
      ['cardTitle', (e) => e.tag === 'h2' && !e.cls.has('sr-only')],
      ['primary', (e) => /^(button|a)$/.test(e.tag) && [...e.cls].some((c) => /^bg-(primary|blue|green)-[67]00$/.test(c))],
      ['secondary', (e) => /^(button|a)$/.test(e.tag) && e.cls.has('border-gray-300')],
      ['select', (e) => e.tag === 'select'],
      ['input', (e) => e.tag === 'textarea' || (e.tag === 'input' && /type="(text|number|search)"/.test(e.attrs))],
      ['check', (e) => e.tag === 'input' && /type="checkbox"/.test(e.attrs) && !e.cls.has('sr-only')],
      ['tableWrap', (e) => e.tag === 'div' && e.cls.has('overflow-x-auto')],
      ['table', (e) => e.tag === 'table'],
      ['thead', (e) => e.tag === 'thead'],
      ['cardHead', (e) => e.cls.has('justify-between') && e.cls.has('flex-wrap')],
    ];
    const all = tags(html.replace(/<section id="console"[\s\S]*?<\/section>/, ''));
    // A card header row's first child is its title block, which grows so the row's actions stay on
    // its row (f450a7dacd: Maximize fell 80 px below its title on a wrapped row).
    all.forEach((el, i) => {
      if (!roles.at(-1)[1](el)) return;
      assert.deepEqual(lacks(all[i + 1], UI.cardHeadTitle), [], `the title block after a cardHead row lacks UI.cardHeadTitle`);
    });
    for (const el of all) {
      for (const [role, is] of roles) {
        if (!is(el)) continue;
        assert.deepEqual(lacks(el, UI[role]), [], `<${el.tag}${el.attrs.slice(0, 60)}…> is a ${role} without the shell's UI.${role}`);
      }
    }
  });
}
