import './panel-ui.mjs'; // first: shell.js publishes PanelUI before any page script
import test from 'node:test';
import assert from 'node:assert/strict';
import { catalogueItems, facetsOf, filterItems, facetCounts, hierarchy, drillTo, sunburstOption, readState, toolsHref, rootOf, facetsOfRoot } from '../static/catalogue.js';

const doc = { paths: {
  '/api/tools/openttd.cmd.BuildBridge': { post: { tags: ['mcp tools'], summary: 'Approval-gated game command CmdBuildBridge (landscape_construction; flags: ["auto"]).' } },
  '/api/tools/openttd.cmd.BuildRoad': { post: { tags: ['mcp tools'], summary: 'Approval-gated game command CmdBuildRoad (landscape_construction; flags: []).' } },
  '/api/tools/openttd.ai.list': { post: { tags: ['mcp tools'], summary: "Every company's AI with its settings" } },
  '/api/client/openttd.view.ask': { post: { tags: ['gui client tools'], summary: 'Ask the player a yes/no question' } },
  '/api/resources/map/terrain': { get: { tags: ['mcp resources'], summary: 'Terrain per tile' } },
  '/api/status': { get: { tags: ['panel'], summary: 'not a catalogue item' } },
} };

test('items come from the OpenAPI tags, each with its own page', () => {
  const all = catalogueItems(doc);
  assert.equal(all.length, 5);
  const r = all.find((x) => x.kind === 'resource');
  assert.deepEqual([r.name, r.page, r.ns, r.safety], ['openttd://map/terrain', '/resource/map/terrain', 'map', 'read']);
  const b = all.find((x) => x.name === 'openttd.cmd.BuildBridge');
  assert.deepEqual([b.page, b.ns, b.cat, b.safety], ['/tool/openttd.cmd.BuildBridge', 'cmd', 'landscape_construction', 'gated']);
  assert.equal(all.find((x) => x.kind === 'client').page, '/client/openttd.view.ask');
});

test('facets are read from the name and the description', () => {
  assert.deepEqual(facetsOf('tool', 'openttd.ai.list', "Every company's AI"), { ns: 'ai', cat: 'ai', safety: 'read' });
  assert.equal(facetsOf('client', 'openttd.view.ask', 'Ask the player').safety, 'other');
});

test('faceted search: counts of one facet ignore that facet but honour the others', () => {
  const all = catalogueItems(doc);
  const state = { q: '', kind: 'tool', ns: '', cat: '', safety: 'gated' };
  assert.equal(filterItems(all, state).length, 2);
  assert.deepEqual(facetCounts(all, state, 'kind'), [['tool', 2]]);
  assert.deepEqual(Object.fromEntries(facetCounts(all, state, 'safety')), { gated: 2, read: 1 });
  assert.equal(filterItems(all, { q: 'yes/no' }).length, 1);
});

test('the hierarchy collapses a category equal to its namespace, and drills by path', () => {
  const tree = hierarchy(catalogueItems(doc));
  const tools = tree.find((n) => n.id === 'tool');
  assert.equal(tools.value, 3);
  assert.deepEqual(drillTo(tree, 'tool/cmd').map((n) => n.id), ['tool/cmd/landscape_construction']);
  assert.equal(drillTo(tree, 'tool/ai')[0].page, '/tool/openttd.ai.list');
  assert.equal(drillTo(tree, 'nope'), tree, 'an unknown path falls back to the whole tree');
  const opt = sunburstOption(catalogueItems(doc), { root: 'tool' });
  assert.equal(opt.series[0].nodeClick, false, 'the drill is the chart\'s own');
  assert.equal(opt.series[0].data.length, 2);
});

test('the address round-trips, with or without a prefix', () => {
  assert.deepEqual(readState('c_', '?c_q=bridge&c_kind=tool&q=other'), { q: 'bridge', kind: 'tool', ns: '', cat: '', safety: '', max: '' });
  assert.equal(toolsHref({ q: 'bridge', kind: 'tool', root: 'x' }), '/tools?q=bridge&kind=tool');
});

test('the drill is the facet filter: a path sets the facets and the facets spell the path', () => {
  assert.equal(rootOf({ kind: 'tool', ns: 'cmd', cat: 'landscape_construction' }), 'tool/cmd/landscape_construction');
  assert.equal(rootOf({ kind: 'tool', ns: 'ai', cat: 'ai' }), 'tool/ai', 'a category equal to its namespace adds no ring');
  assert.equal(rootOf({ ns: 'cmd' }), '', 'no kind, no drill');
  assert.deepEqual(facetsOfRoot('tool/cmd'), { kind: 'tool', ns: 'cmd', cat: '' });
  assert.deepEqual(facetsOfRoot(''), { kind: '', ns: '', cat: '' });
});
