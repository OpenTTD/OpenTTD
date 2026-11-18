// node --test tools/research/mcp-panel/test/ : the catalogue component (static/catalogue.js
// mountCatalogue) driven through the fake browser of fake-dom.mjs: its facets, search, drill,
// address state, "show more", the compact home variant and the maximized map. The OpenAPI fixture
// is fictional; nothing here talks to a game.
import './panel-ui.mjs'; // first: shell.js publishes PanelUI before any page script
import test from 'node:test';
import assert from 'node:assert/strict';
import { fakeDom } from './fake-dom.mjs';
import { catalogueItems, mountCatalogue, toolsHref } from '../static/catalogue.js';

/** 60 construction commands (more than one page of 48 cards), a read-only tool, a client tool
    and a resource. */
const paths = {};
for (let i = 0; i < 60; i++) {
  const cat = i % 3 ? 'landscape_construction' : 'vehicle_construction';
  paths[`/api/tools/openttd.cmd.Build${String(i).padStart(2, '0')}`] = {
    post: { tags: ['mcp tools'], summary: `Approval-gated game command CmdBuild${i} (${cat}; flags: []).` },
  };
}
paths['/api/tools/openttd.ai.list'] = { post: { tags: ['mcp tools'], summary: "Every company's AI" } };
paths['/api/client/openttd.view.ask'] = { post: { tags: ['gui client tools'], summary: 'Ask the player a yes/no question' } };
paths['/api/resources/map/terrain'] = { get: { tags: ['mcp resources'], summary: 'Terrain per tile' } };
const ALL = catalogueItems({ paths });

const env = fakeDom({ url: 'http://127.0.0.1:8800/tools' });

/** Mount the catalogue into a fresh box at an address and let its first draw run. */
async function mount(url, opts = {}, id = 'page') {
  env.location.href = url;
  env.timers.splice(0);
  const box = env.byId(id);
  const handle = mountCatalogue(box, ALL, opts);
  await env.tick();
  const q = (sel) => box.querySelector(sel);
  return { box, handle, q, chart: env.charts.at(-1) };
}
const cards = (q) => q('[data-cards]').querySelectorAll('a').map((a) => a.getAttribute('href'));
const pills = (q, facet) => q(`[data-pills="${facet}"]`).querySelectorAll('button');
const params = () => new URL(env.location.href).searchParams;

test('the full catalogue reads its facets from the address and pages its cards by 48', async () => {
  const { q, chart } = await mount('http://127.0.0.1:8800/tools?kind=tool');
  assert.equal(q('[data-count]').textContent, '61 of 63');
  assert.equal(cards(q).length, 48);
  assert.ok(!q('[data-more]').classList.contains('hidden'));
  assert.equal(q('[data-more]').textContent, 'Show more (13 left)');
  const kind = pills(q, 'kind');
  assert.deepEqual(kind.map((b) => b.dataset.value), ['', 'tool', 'client', 'resource']);
  assert.deepEqual(kind.map((b) => b.getAttribute('aria-pressed')), ['false', 'true', 'false', 'false']);
  assert.match(kind[0].textContent, /^All63$/, 'All counts every kind the other facets let through');
  const ns = q('[data-select="ns"]').querySelectorAll('option').map((o) => o.textContent);
  assert.deepEqual(ns, ['Every namespace', 'Cmd (60)', 'Ai (1)']);
  assert.match(q('[data-crumbs]').innerHTML, /data-root="tool"[^>]*>Server tools</);
  assert.equal(chart.getOption().series[0].type, 'sunburst', 'the map is drawn through the one door');
  assert.ok(chart.resized > 0);
  await q('[data-more]').dispatch('click');
  assert.equal(cards(q).length, 61);
  assert.ok(q('[data-more]').classList.contains('hidden'), 'nothing left to show');
});

test('a pill, a select and a crumb set the facets, and the address follows', async () => {
  const { q } = await mount('http://127.0.0.1:8800/tools');
  const gated = pills(q, 'safety').find((b) => b.dataset.value === 'gated');
  await gated.dispatch('click');
  assert.equal(params().get('safety'), 'gated');
  assert.equal(q('[data-count]').textContent, '60 of 63');
  const ns = q('[data-select="ns"]');
  ns.value = 'cmd';
  await ns.dispatch('change');
  const cat = q('[data-select="cat"]');
  cat.value = 'vehicle_construction';
  await cat.dispatch('change');
  assert.equal(params().get('ns'), 'cmd');
  assert.equal(q('[data-count]').textContent, '20 of 63');
  assert.match(q('[data-select="cat"]').innerHTML, /<option value="vehicle_construction" selected>/);
  // A change of something that is not a facet select draws nothing new.
  const before = env.location.href;
  await q('#cq').dispatch('change');
  assert.equal(env.location.href, before);
  // The crumb "All" clears the drill: kind, namespace and category.
  await pills(q, 'kind').find((b) => b.dataset.value === 'tool').dispatch('click');
  assert.match(q('[data-crumbs]').innerHTML, /Server tools.*Cmd.*Vehicle construction/s);
  const all = q('[data-crumbs]').querySelector('[data-root=""]');
  await all.dispatch('click');
  assert.deepEqual([params().get('kind'), params().get('ns'), params().get('cat')], [null, null, null]);
  assert.equal(params().get('safety'), 'gated', 'the drill leaves the other facets alone');
  // A click on nothing the catalogue owns changes nothing.
  const href = env.location.href;
  await q('[data-count]').dispatch('click');
  assert.equal(env.location.href, href);
});

test('the map is the facet filter: a ring drills in, a leaf opens its page', async () => {
  const { q, chart } = await mount('http://127.0.0.1:8800/tools');
  chart.handlers.click({ data: { id: 'tool/cmd/landscape_construction' } });
  assert.deepEqual(
    [params().get('kind'), params().get('ns'), params().get('cat')],
    ['tool', 'cmd', 'landscape_construction'],
  );
  assert.equal(q('[data-count]').textContent, '40 of 63');
  assert.equal(chart.getOption().series[0].data.every((n) => n.page), true, 'drilled to the leaves');
  chart.handlers.click({ data: {} }); // the centre: neither a ring nor a leaf
  assert.equal(params().get('cat'), 'landscape_construction');
  chart.handlers.click({ data: { page: '/tool/openttd.cmd.Build01', name: 'Build01' } });
  assert.equal(env.location.pathname, '/tool/openttd.cmd.Build01');
  const tip = chart.getOption().tooltip.formatter;
  assert.equal(tip({ name: 'Cmd', value: 2, data: {} }), '<b>Cmd</b><br>2 items · click to zoom in');
  assert.equal(tip({ name: 'X', value: 1, data: { page: '/x' } }), 'Open <b>X</b><br>1 item');
});

test('search waits for typing to pause, and Clear filters empties it and every facet', async () => {
  const { q } = await mount('http://127.0.0.1:8800/tools?kind=client');
  const input = q('#cq');
  input.value = 'yes/no';
  await input.dispatch('input');
  assert.equal(params().get('q'), null, 'not before the pause');
  assert.ok(env.timers.some((t) => t.ms === 120));
  await env.tick();
  assert.equal(params().get('q'), 'yes/no');
  assert.deepEqual(cards(q), ['/client/openttd.view.ask']);
  input.value = 'no such thing';
  await input.dispatch('input');
  await env.tick();
  assert.match(q('[data-cards]').innerHTML, /Nothing matches/);
  await q('[data-reset]').dispatch('click');
  assert.equal(input.value, '');
  assert.equal(env.location.search, '');
  assert.equal(q('[data-count]').textContent, '63 of 63');
});

test('the compact home variant: prefixed keys, nine cards and a link to the same view on /tools', async () => {
  const { q } = await mount('http://127.0.0.1:8800/?c_kind=tool&c_ns=ai&other=1', {
    compact: true,
    prefix: 'c_',
  }, 'catalogue-mount');
  assert.deepEqual(cards(q), ['/tool/openttd.ai.list']);
  const full = q('[data-full]');
  assert.equal(full.getAttribute('href'), '/tools?kind=tool&ns=ai');
  assert.equal(full.getAttribute('href'), toolsHref({ kind: 'tool', ns: 'ai' }));
  assert.equal(full.textContent, 'Open all 1 in the full catalogue →');
  await q('[data-crumbs]').querySelector('[data-root="tool"]').dispatch('click');
  assert.equal(cards(q).length, 9, 'one compact page');
  assert.ok(q('[data-more]').classList.contains('hidden'), 'the home links to /tools instead');
  assert.equal(params().get('other'), '1', 'keys of other components survive');
  assert.equal(params().get('c_ns'), null);
});

test('the map maximizes from its address, and Escape restores it', async () => {
  const { box, q, chart } = await mount('http://127.0.0.1:8800/tools?max=catalogue');
  const card = box.querySelector('#ccatalogue-chart-card');
  assert.ok(card.classList.contains('fixed'), 'maximized from the address');
  assert.equal(q('[data-toolbar]').children[0].textContent, 'Maximize');
  const resized = chart.resized;
  await env.document.dispatch('keydown', { key: 'Escape' });
  assert.ok(!card.classList.contains('fixed'));
  assert.equal(params().get('max'), null);
  await env.tick();
  assert.ok(chart.resized > resized, 'the chart follows its new box');
});

test('a hidden map box draws the cards without a chart', async () => {
  env.location.href = 'http://127.0.0.1:8800/tools';
  env.timers.splice(0);
  const box = env.byId('hidden-box');
  const charts = env.charts.length;
  const { redraw } = mountCatalogue(box, ALL, { prefix: 'h_' });
  box.querySelector('#h_catalogue-chart').offsetWidth = 0;
  await env.tick();
  assert.equal(env.charts.length, charts, 'ecInit refuses a zero-width box');
  assert.equal(box.querySelector('[data-cards]').querySelectorAll('a').length, 48);
  redraw();
  assert.equal(box.querySelector('[data-count]').textContent, '63 of 63');
});
