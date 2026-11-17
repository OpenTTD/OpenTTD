'use strict';
/* The one shell of every panel page (rule project.panel-items-are-routable-pages): the Flowbite Pro admin
   navbar, sidebar and page header, declared once here and never copied into a page. A page is only its <main>:
   - <body data-page="KEY"> names it (a key of PAGES); its sidebar entry is marked current;
   - the page header (breadcrumb from the registry, H1, description, #badges) opens <main>: the H1 is
     <body data-title> or the page's label, the description <body data-description> (both may be filled
     server-side, __TITLE__ / __DESCRIPTION__); a crumb a page script appends to #crumbs is redrawn as
     the shell's own (the bare "/" separators dropped);
   - UI holds the Pro class sets (card, card title, buttons, select, table, badge) every page uses;
   - elements in <main> carrying data-nav="Label" become its "On this page" entries (derived, not listed);
   - elements carrying data-nav-extra move into the navbar's right side (page stats, endpoints);
   - the connection badge (#conn) and the console buttons (#open-console, #side-console) exist on every
     page; a page with its own console (the home) binds them, every other page sends them to /#console.
   Load it as the first element of <body>: the navbar and sidebar are in place before Flowbite inits its
   drawer on DOMContentLoaded. The pure parts are exported for Node (test/shell.test.mjs). */

// Its own scope: a classic script's top-level const is global, and app.js declares esc too.
(function shell() {
  // The panel's pages (one game); the hub's (every instance) are APPS.hub. A page picks its app with
  // <body data-app>, the panel by default.
  const PAGES = [
    { key: 'home', href: '/', label: 'Overview', title: 'Panel' },
    { key: 'world', href: '/world', label: 'World: map, plan, yards', title: 'World' },
    { key: 'ui', href: '/ui', label: 'Interface: every window', title: 'Interface' },
    { key: 'tools', href: '/tools', label: 'MCP catalogue', title: 'Catalogue' },
    { key: 'results', href: '/results', label: 'Duels', title: 'Duels' },
    { key: 'docs', href: '/docs', label: 'API (OpenAPI, Swagger UI)', title: 'API' },
  ];

  const APPS = {
    panel: { brand: 'OpenTTD MCP', status: '/api/status', console: '/#console', doc: 'research/mcp-control.md', pages: PAGES },
    // The hub sets #conn from its own WebSocket and has no developer console.
    hub: { brand: 'MCP Hub', status: null, console: null, doc: 'research/mcp-hub.md', pages: [
      { key: 'hub', href: '/', label: 'Hub: every instance', title: 'Hub' },
      { key: 'viz', href: '/viz', label: 'World view', title: 'World view' },
      { key: 'docs', href: '/docs', label: 'API (OpenAPI)', title: 'API' },
    ] },
  };

  /** The page a path belongs to: a tool, client or resource page is part of the catalogue, a run of the duels. */
  function pageOf(path, app = 'panel') {
    if (app === 'hub') return path === '/' || path === '/hub' ? 'hub' : path.startsWith('/viz') ? 'viz' : path.startsWith('/docs') ? 'docs' : '';
    if (path === '/' || path === '/index.html') return 'home';
    if (/^\/(tools|tool|client|resource)(\/|$)/.test(path)) return 'tools';
    if (/^\/(results|result)(\/|$)/.test(path)) return 'results';
    const hit = PAGES.find((p) => p.href !== '/' && (path === p.href || path.startsWith(p.href + '/')));
    return hit ? hit.key : '';
  }

  const esc = (s) => String(s ?? '').replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
  /** The frame every page's <main> gets: under the navbar, beside the sidebar, one gutter and width. */
  const FRAME = ['pt-20', 'px-4', 'max-w-screen-2xl', 'mx-auto', 'lg:ml-64'];
  const LINK = 'flex items-center p-2 text-base text-gray-900 rounded-lg hover:bg-gray-100 dark:text-gray-200 dark:hover:bg-gray-700';
  const SUB = 'flex items-center p-2 ps-4 text-sm text-gray-700 rounded-lg hover:bg-gray-100 dark:text-gray-300 dark:hover:bg-gray-700';
  const CONN = 'shrink-0 whitespace-nowrap text-xs font-medium px-2.5 py-0.5 rounded-sm';
  const BTN = 'text-gray-900 bg-white border border-gray-300 hover:bg-gray-100 focus:ring-4 focus:ring-gray-100 font-medium rounded-lg text-sm px-3 py-1.5 sm:px-4 sm:py-2 whitespace-nowrap dark:bg-gray-800 dark:text-white dark:border-gray-600 dark:hover:bg-gray-700 dark:focus:ring-gray-700';

  /** The Pro class sets of the admin dashboard (reference.md sections 1, 6, 9, 13), one per role. A page's
      markup carries every class of its role's set (test/shell.test.mjs enforces it), plus layout only. */
  const FIELD = 'bg-gray-50 border border-gray-300 text-gray-900 text-sm rounded-lg focus:ring-primary-500 focus:border-primary-500 dark:bg-gray-700 dark:border-gray-600 dark:placeholder-gray-400 dark:text-white dark:focus:ring-primary-500 dark:focus:border-primary-500';
  const UI = Object.freeze({
    card: 'p-4 bg-white border border-gray-200 rounded-lg shadow-sm sm:p-6 dark:border-gray-700 dark:bg-gray-800',
    cardTitle: 'text-xl font-bold leading-none text-gray-900 dark:text-white',
    cardSub: 'text-sm font-normal text-gray-500 dark:text-gray-400',
    h1: 'text-xl font-semibold text-gray-900 sm:text-2xl dark:text-white',
    primary: 'text-white bg-primary-700 hover:bg-primary-800 focus:ring-4 focus:ring-primary-300 font-medium rounded-lg text-sm px-4 py-2 whitespace-nowrap focus:outline-none disabled:opacity-50 dark:bg-primary-600 dark:hover:bg-primary-700 dark:focus:ring-primary-800',
    secondary: 'text-gray-900 bg-white border border-gray-300 hover:bg-gray-100 focus:ring-4 focus:ring-gray-100 font-medium rounded-lg text-sm px-4 py-2 whitespace-nowrap focus:outline-none dark:bg-gray-800 dark:text-white dark:border-gray-600 dark:hover:bg-gray-700 dark:focus:ring-gray-700',
    // A field is an input or a select, as tall as a button (38 px, mcp-overview-check); the select keeps
    // room on its right for the arrow (pe-9).
    input: `${FIELD} px-3 py-2`,
    select: `${FIELD} py-2 ps-3 pe-9`,
    check: 'w-4 h-4 text-primary-600 bg-gray-100 border-gray-300 rounded-sm focus:ring-primary-500 focus:ring-2 dark:bg-gray-700 dark:border-gray-600 dark:focus:ring-primary-600 dark:ring-offset-gray-800',
    tableWrap: 'relative overflow-x-auto rounded-lg',
    table: 'w-full text-sm text-left text-gray-500 dark:text-gray-400',
    thead: 'text-xs text-gray-700 uppercase bg-gray-50 dark:bg-gray-700 dark:text-gray-400',
    badge: 'text-xs font-medium px-2.5 py-0.5 rounded-sm',
  });

  const CHEVRON = '<svg class="w-6 h-6 text-gray-400 rtl:rotate-180" aria-hidden="true" fill="currentColor" viewBox="0 0 20 20"><path fill-rule="evenodd" d="M7.293 14.707a1 1 0 010-1.414L10.586 10 7.293 6.707a1 1 0 011.414-1.414l4 4a1 1 0 010 1.414l-4 4a1 1 0 01-1.414 0z" clip-rule="evenodd"></path></svg>';
  const HOME = '<svg class="w-5 h-5 me-2.5" aria-hidden="true" fill="currentColor" viewBox="0 0 20 20"><path d="M10.707 2.293a1 1 0 00-1.414 0l-7 7a1 1 0 001.414 1.414L4 10.414V17a1 1 0 001 1h2a1 1 0 001-1v-2a1 1 0 011-1h2a1 1 0 011 1v2a1 1 0 001 1h2a1 1 0 001-1v-6.586l.293.293a1 1 0 001.414-1.414l-7-7z"></path></svg>';
  const CRUMB_LINK = 'ms-1 text-sm font-medium text-gray-700 hover:text-gray-900 md:ms-2 dark:text-gray-300 dark:hover:text-white';
  const CRUMB_HERE = 'ms-1 text-sm font-medium text-gray-400 md:ms-2 dark:text-gray-500';

  /** One breadcrumb after the home one: a link, or the current page (no href). */
  function crumbHtml(label, href) {
    const inner = href ? `<a href="${esc(href)}" class="${CRUMB_LINK}">${esc(label)}</a>`
      : `<span class="${CRUMB_HERE}" aria-current="page">${esc(label)}</span>`;
    return `<li data-crumb><div class="flex items-center">${CHEVRON}${inner}</div></li>`;
  }

  /** The breadcrumb, derived from the app's registry: its home page, then the current page (a link
      while the path is below it, as /tool/<name> is below /tools). */
  function crumbsHtml(current, path = '/', app = APPS.panel) {
    const home = app.pages[0];
    const page = app.pages.find((p) => p.key === current);
    const out = [`<li data-crumb class="inline-flex items-center"><a href="${home.href}" class="inline-flex items-center text-sm font-medium text-gray-700 hover:text-gray-900 dark:text-gray-300 dark:hover:text-white">${HOME}${esc(home.title)}</a></li>`];
    if (page && page !== home) out.push(crumbHtml(page.title, path === page.href ? null : page.href));
    return out.join('');
  }

  /** The Pro page header (users/settings.html variant B): breadcrumb, H1, optional description, badges.
      #crumbs, #title, #description and #badges are what tool.js and results.js write to. */
  function headerHtml({ current, path = '/', app = APPS.panel, title = '', description = '' } = {}) {
    const page = app.pages.find((p) => p.key === current);
    return `<div id="page-header" class="mb-4">
      <nav class="flex mb-5" aria-label="Breadcrumb"><ol id="crumbs" class="inline-flex flex-wrap items-center gap-y-1 space-x-1 md:space-x-2 rtl:space-x-reverse">${crumbsHtml(current, path, app)}</ol></nav>
      <h1 id="title" class="${UI.h1} break-words">${esc(title || page?.label || '')}</h1>
      <p id="description" class="mt-1 ${UI.cardSub} empty:hidden">${esc(description)}</p>
      <div id="badges" class="flex flex-wrap gap-2 mt-2 empty:hidden"></div>
    </div>`;
  }

  /** A crumb a page script appended (<li>/</li><li>Name</li>, or a link) becomes the shell's own. */
  function adoptCrumbs(ol) {
    for (const li of [...ol.children]) {
      if (li.hasAttribute('data-crumb')) continue;
      const text = li.textContent.trim();
      if (text === '/' || !text) { li.remove(); continue; }
      const a = li.querySelector('a[href]');
      li.insertAdjacentHTML('beforebegin', crumbHtml(text, a ? a.getAttribute('href') : null));
      li.remove();
    }
  }

  /** The sidebar's page list, the current one marked. */
  function sidebarHtml(current, pages = PAGES) {
    return pages.map((p) => `<li><a href="${p.href}"${p.key === current ? ' aria-current="page"' : ''} class="${LINK}${p.key === current ? ' bg-gray-100 dark:bg-gray-700' : ''}">${esc(p.label)}</a></li>`).join('');
  }

  /** "On this page": one entry per [data-nav] element with an id ({id, label}), in document order. */
  function sectionsHtml(sections) {
    if (!sections.length) return '';
    return `<p class="px-2 pt-4 pb-1 text-xs font-semibold tracking-wide text-gray-500 uppercase dark:text-gray-400">On this page</p>`
      + sections.map((s) => `<li><a href="#${esc(s.id)}" class="${SUB}">${esc(s.label)}</a></li>`).join('');
  }

  function navbarHtml(app = APPS.panel) {
    return `<nav id="shell-nav" class="fixed top-0 z-30 w-full bg-white border-b border-gray-200 dark:bg-gray-800 dark:border-gray-700">
    <div class="px-3 py-3 lg:px-5 lg:ps-3"><div class="flex items-center justify-between gap-2">
      <div class="flex items-center gap-3 min-w-0">
        <button data-drawer-target="sidebar" data-drawer-toggle="sidebar" aria-controls="sidebar" type="button" class="p-2 text-gray-600 rounded-sm cursor-pointer lg:hidden hover:text-gray-900 hover:bg-gray-100 focus:bg-gray-100 dark:focus:bg-gray-700 focus:ring-2 focus:ring-gray-100 dark:focus:ring-gray-700 dark:text-gray-400 dark:hover:bg-gray-700 dark:hover:text-white"><span class="sr-only">Toggle sidebar</span><svg class="w-6 h-6" aria-hidden="true" fill="currentColor" viewBox="0 0 20 20"><path fill-rule="evenodd" d="M3 5a1 1 0 011-1h12a1 1 0 110 2H4a1 1 0 01-1-1zM3 10a1 1 0 011-1h12a1 1 0 110 2H4a1 1 0 01-1-1zM3 15a1 1 0 011-1h12a1 1 0 110 2H4a1 1 0 01-1-1z" clip-rule="evenodd"></path></svg></button>
        <a href="/" class="self-center text-xl font-semibold whitespace-nowrap sm:text-2xl dark:text-white">${esc(app.brand)}</a>
        <span class="hidden sm:inline bg-yellow-100 text-yellow-800 text-xs font-medium px-2.5 py-0.5 rounded-sm dark:bg-yellow-900 dark:text-yellow-300">RESEARCH-ONLY</span>
        <span id="conn" class="shrink-0 whitespace-nowrap bg-gray-100 text-gray-800 text-xs font-medium px-2.5 py-0.5 rounded-sm dark:bg-gray-700 dark:text-gray-300">connecting</span>
      </div>
      <div id="nav-extra" class="flex items-center gap-2 min-w-0 text-sm text-gray-500 dark:text-gray-400">
        <a href="/docs" class="hidden sm:inline-block ${BTN}">API</a>
        ${app.console ? `<button id="open-console" type="button" class="${BTN}">Console <kbd class="px-1.5 py-0.5 text-xs font-semibold text-gray-800 bg-gray-100 border border-gray-200 rounded-lg dark:bg-gray-600 dark:text-gray-100 dark:border-gray-500">~</kbd></button>` : ''}
      </div>
    </div></div></nav>`;
  }

  function asideHtml(current, app = APPS.panel) {
    return `<aside id="sidebar" class="fixed top-0 left-0 z-20 flex flex-col flex-shrink-0 w-64 h-full pt-16 transition-transform -translate-x-full bg-white border-r border-gray-200 lg:translate-x-0 dark:bg-gray-800 dark:border-gray-700" aria-label="Sidebar">
    <div class="flex-1 px-3 pt-5 pb-4 overflow-y-auto">
      <ul class="space-y-1 font-medium">${sidebarHtml(current, app.pages)}
        ${app.console ? `<li><button type="button" id="side-console" class="${LINK} w-full">Developer console (~)</button></li>` : ''}
      </ul>
      <ul id="shell-sections" class="space-y-1 font-medium"></ul>
      <div class="pt-4 mt-4 space-y-2 text-sm text-gray-500 border-t border-gray-200 dark:border-gray-700 dark:text-gray-400">
        <p>Docs: <code>${esc(app.doc)}</code></p>
      </div>
    </div></aside>`;
  }

  /* ---- in the browser ------------------------------------------------------------------------- */
  if (typeof document !== 'undefined' && document.body) {
    const app = APPS[document.body.dataset.app] || APPS.panel;
    const current = document.body.dataset.page || pageOf(location.pathname, document.body.dataset.app);
    document.body.insertAdjacentHTML('afterbegin', navbarHtml(app) + asideHtml(current, app));
    // The page header opens <main> as soon as the parser has created it, before any page script runs.
    const header = () => {
      const main = document.querySelector('body > main');
      if (!main || document.getElementById('page-header')) return !!main;
      const { title, description } = document.body.dataset;
      main.insertAdjacentHTML('afterbegin', headerHtml({ current, path: location.pathname, app, title, description }));
      const ol = document.getElementById('crumbs');
      new MutationObserver(() => adoptCrumbs(ol)).observe(ol, { childList: true });
      return true;
    };
    if (!header()) {
      const watch = new MutationObserver(() => { if (header()) watch.disconnect(); });
      watch.observe(document.body, { childList: true });
      document.addEventListener('DOMContentLoaded', () => { watch.disconnect(); header(); });
    }
    globalThis.PanelUI = UI;
    if (app.console) {
      const toConsole = () => { location.href = app.console; };
      document.getElementById('open-console').onclick = toConsole;
      document.getElementById('side-console').onclick = toConsole;
    }
    document.addEventListener('DOMContentLoaded', () => {
      const main = document.querySelector('main');
      if (main) {
        // One frame for every page's content: under the navbar, beside the sidebar, the same gutter.
        main.classList.add(...FRAME);
        if (!/(^|\s)pb-/.test(main.className)) main.classList.add('pb-16');
        const sections = [...main.querySelectorAll('[data-nav][id]')].map((el) => ({ id: el.id, label: el.dataset.nav }));
        document.getElementById('shell-sections').innerHTML = sectionsHtml(sections);
        const extra = document.getElementById('nav-extra');
        for (const el of main.querySelectorAll('[data-nav-extra]')) extra.prepend(el);
      }
      // The connection badge on every page, from the app's own status (the hub's script sets it itself).
      const conn = document.getElementById('conn');
      if (!app.status) return;
      const poll = async () => {
        try {
          const s = await (await fetch(app.status)).json();
          conn.textContent = s.mcp_up ? 'MCP up' : (s.server_running ? 'server starting' : 'server down');
          conn.className = CONN + (s.mcp_up ? ' bg-green-100 text-green-800 dark:bg-green-900 dark:text-green-300'
            : ' bg-red-100 text-red-800 dark:bg-red-900 dark:text-red-300');
        } catch { conn.textContent = 'panel unreachable'; }
      };
      poll();
      setInterval(poll, 5000);
    });
  }

  if (typeof module !== 'undefined') {
    module.exports = { PAGES, APPS, FRAME, UI, pageOf, sidebarHtml, sectionsHtml, navbarHtml, asideHtml, crumbsHtml, headerHtml };
  }
})();
