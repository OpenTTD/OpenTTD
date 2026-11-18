// node --test tools/research/mcp-panel/test/ : static/shell.js in the browser, driven through the
// fake browser of fake-dom.mjs: it draws the navbar and sidebar, opens <main> with the page header
// (at once, or when the parser creates <main>), adopts a crumb a page script appends, frames <main>,
// derives "On this page", moves [data-nav-extra] into the navbar and keeps the connection badge.
// What the shared fake lacks for this (MutationObserver, `body > main`, sibling insertion, an id
// inside markup written into a parsed element, null for a missing id) is added here, for this file
// only.
import test from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import { fakeDom, respond, FakeElement } from './fake-dom.mjs';

const require = createRequire(import.meta.url);
const PATH = require.resolve('../static/shell.js');

/** The MutationObservers the shell creates, each with its callback and what it observes. */
const observers = [];
globalThis.MutationObserver = class {
  constructor(cb) { Object.assign(this, { cb, target: null, disconnected: false }); observers.push(this); }
  observe(target, opts) { Object.assign(this, { target, opts }); }
  disconnect() { this.disconnected = true; }
  /** The browser reports a change of what it observes. */
  fire() { if (!this.disconnected) this.cb([]); }
};

// insertAdjacentHTML beforebegin/afterend put the markup beside the element, in its parent.
const ownInsert = FakeElement.prototype.insertAdjacentHTML;
FakeElement.prototype.insertAdjacentHTML = function insertAdjacentHTML(pos, html) {
  if ((pos === 'beforebegin' || pos === 'afterend') && this.parentNode?.nodeType === 1) {
    const tmp = new FakeElement('template');
    tmp.innerHTML = html;
    this.parentNode.append(...tmp.children);
    return;
  }
  ownInsert.call(this, pos, html);
};

/** A fake browser at `url` whose body holds `bodyHtml`, with the lookups the shell needs. */
function browser(url, bodyHtml, attrs, routes) {
  const env = fakeDom({ url, routes });
  const { document } = env;
  const { body } = document;
  for (const [k, v] of Object.entries(attrs)) body.setAttribute(k, v);
  body.innerHTML = bodyHtml;
  const ownQuery = document.querySelector;
  document.querySelector = (sel) => (sel === 'body > main'
    ? body.children.find((c) => c.localName === 'main') || null : ownQuery(sel));
  // As a browser: null for an id the page does not have (the shared fake makes one on first use).
  document.getElementById = (id) => body.descendants().find((e) => e.id === id)
    || body.descendants().map((e) => e.querySelector(`#${id}`)).find(Boolean) || null;
  return env;
}
/** Run the shell as the first script of the page. */
function runShell() {
  delete require.cache[PATH];
  return require(PATH);
}

test('the panel: navbar and sidebar, a header once <main> exists, crumbs adopted, frame and badge', async () => {
  let status = { mcp_up: true };
  const env = browser('http://127.0.0.1:8800/tool/openttd.ai.list', '', {
    'data-page': 'tools', 'data-title': 'openttd.ai.list', 'data-description': 'Every company\'s AI',
  }, (u) => {
    if (u !== '/api/status') return {};
    if (status instanceof Error) throw status;
    return respond(status);
  });
  const { document } = env;
  const { body } = document;
  const { UI, TONE, FRAME } = runShell();
  assert.equal(globalThis.PanelUI, UI, 'the class sets are published for the page scripts');
  assert.equal(globalThis.PanelTONE, TONE);
  assert.match(body.innerHTML, /^<nav id="shell-nav"[^]*<aside id="sidebar"/, 'navbar and sidebar first');
  assert.match(body.innerHTML, /href="\/tools" aria-current="page"/, 'the page of <body data-page> is current');
  // No <main> yet: the shell waits for the parser to create it.
  const watch = observers.at(-1);
  assert.equal(watch.target, body);
  watch.fire();
  assert.equal(watch.disconnected, false, 'still no <main>');
  const main = document.createElement('main');
  main.className = 'pb-8';
  main.innerHTML = '<section id="yards" data-nav="Transfer yards"></section><section data-nav="No id"></section>'
    + '<span id="stats" data-nav-extra>12 tools</span>';
  body.append(main);
  watch.fire();
  assert.ok(watch.disconnected, 'one header, then the watch stops');
  assert.match(main.innerHTML, /^<div id="page-header"/);
  assert.equal(document.getElementById('title').textContent, 'openttd.ai.list');
  assert.equal(document.getElementById('description').textContent, 'Every company\'s AI');
  const ol = document.getElementById('crumbs');
  assert.match(ol.innerHTML, /<a href="\/tools" [^>]*>Catalogue<\/a>/, 'a tool page links back to the catalogue');
  // A page script appends "/ Server tools" and a link; the shell redraws them as its own crumbs.
  const crumbs = observers.at(-1);
  assert.equal(crumbs.target, ol);
  ol.insertAdjacentHTML('beforeend', '<li class="text-gray-400">/</li><li class="text-gray-500">Server tools</li>'
    + '<li></li><li><a href="/tools?kind=tool">Every server tool</a></li>');
  crumbs.fire();
  const kids = ol.children;
  assert.ok(kids.every((li) => li.hasAttribute('data-crumb')), 'only the shell\'s crumbs remain');
  assert.deepEqual(kids.map((li) => li.textContent.trim()), ['Panel', 'Catalogue', 'Server tools', 'Every server tool']);
  assert.match(kids[2].innerHTML, /<span [^>]*aria-current="page">Server tools<\/span>/);
  assert.match(kids[3].innerHTML, /<a href="\/tools\?kind=tool" /);
  // The console buttons send every page without a console of its own to the home's.
  document.getElementById('open-console').onclick();
  assert.equal(env.location.href, 'http://127.0.0.1:8800/#console');
  env.location.href = 'http://127.0.0.1:8800/tool/openttd.ai.list';
  document.getElementById('side-console').onclick();
  assert.equal(env.location.hash, '#console');
  // DOMContentLoaded: the frame, "On this page", the navbar extras and the connection badge.
  await document.dispatch('DOMContentLoaded');
  await env.settle();
  for (const cls of FRAME) assert.ok(main.classList.contains(cls), cls);
  assert.ok(!main.classList.contains('pb-16'), 'a page\'s own bottom padding is kept');
  assert.equal(document.querySelectorAll('#page-header').length, 1, 'the header is not drawn twice');
  const sections = document.getElementById('shell-sections').innerHTML;
  assert.match(sections, /On this page[^]*href="#yards"[^>]*>Transfer yards</);
  assert.doesNotMatch(sections, /No id/, 'a section without an id has nothing to link to');
  assert.equal(document.getElementById('nav-extra').kids[0].id, 'stats', 'the page\'s extras lead the navbar');
  const conn = document.getElementById('conn');
  assert.equal(conn.textContent, 'MCP up');
  assert.ok(TONE.green.split(' ').every((c) => conn.classList.contains(c)));
  const poll = env.timers.find((t) => t.repeat && t.ms === 5000);
  assert.ok(poll, 'the badge is polled every 5 s');
  for (const [s, text, tone] of [[{ server_running: true }, 'server starting', 'red'], [{}, 'server down', 'red']]) {
    status = s;
    await poll.fn();
    assert.equal(conn.textContent, text);
    assert.ok(TONE[tone].split(' ').every((c) => conn.classList.contains(c)));
  }
  status = new Error('offline');
  await poll.fn();
  assert.equal(conn.textContent, 'panel unreachable');
});

test('the hub: its own pages, the header at once, no console and no status poll', async () => {
  const env = browser('http://127.0.0.1:8900/viz', '<main><div id="page"><p>World</p></div></main>', { 'data-app': 'hub' });
  const { document } = env;
  const { body } = document;
  const before = observers.length;
  runShell();
  assert.match(body.innerHTML, />MCP Hub</);
  assert.doesNotMatch(body.innerHTML, /id="open-console"|id="side-console"/, 'the hub has no console');
  assert.match(body.innerHTML, /href="\/viz" aria-current="page"/, 'the page derived from the path');
  const main = document.querySelector('body > main');
  assert.match(main.innerHTML, /^<div id="page-header"[^]*<h1 id="title" [^>]*>World view<\/h1>/, 'the label, without a title');
  assert.equal(observers.length - before, 1, 'only the crumbs are watched: <main> was already there');
  await document.dispatch('DOMContentLoaded');
  await env.settle();
  assert.ok(main.classList.contains('pb-16'), 'the default bottom padding');
  assert.equal(document.getElementById('shell-sections').innerHTML, '', 'no sections, no heading');
  assert.equal(document.getElementById('conn').textContent, 'connecting', 'the hub sets its badge itself');
  assert.equal(env.fetches.length, 0);
  assert.equal(env.timers.length, 0);
});

test('a page without <main> gets its header when the document is parsed, if ever', async () => {
  const env = browser('http://127.0.0.1:8800/nope', '', {});
  const { document } = env;
  runShell();
  assert.match(document.body.innerHTML, /id="sidebar"/);
  assert.doesNotMatch(document.body.innerHTML, /aria-current="page"/, 'an unknown path marks no page');
  await document.dispatch('DOMContentLoaded');
  assert.ok(observers.at(-1).disconnected, 'the watch stops with the document');
  assert.equal(document.getElementById('page-header'), null, 'no <main>, no header');
});
