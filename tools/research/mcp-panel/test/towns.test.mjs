// node --test tools/research/mcp-panel/test/ : the Towns page (static/towns.js). Its pure helpers
// read the committed model (tools/research/town-model/town_model.json, generated); the page is then
// mounted in the fake browser of fake-dom.mjs with live answers shaped like panel_town.rs's (GET
// /api/town/live) with fictional towns. Nothing here talks to a panel or a game.
import './panel-ui.mjs'; // first: shell.js publishes PanelUI before any page script
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { fakeDom, respond } from './fake-dom.mjs';

const model = JSON.parse(fs.readFileSync(new URL('../../town-model/town_model.json', import.meta.url), 'utf8'));
const advice = [{ what: 'plant 9 tree tiles near the town', why: 'rating -260 is not above -200', source: 'src/tree_cmd.cpp CmdPlantTree' }];
const live = {
  company: 0, rated_companies: [0, 1], tolerance: 'hostile', tolerance_source: 'console: setting difficulty.town_council_tolerance',
  companies: [{ id: 0, name: 'Alpha Transport' }, { id: 1, name: 'Beta Lines' }], count: 2, fields: ['ratings', 'idle_stations'],
  advice_error: null, reason: null,
  towns: [
    { id: 0, name: 'Aville', x: 10, y: 4, population: 1200, rating: -260, band: 'TOWN_RATING_VERY_POOR', rating_floor: -260, advice },
    { id: 1, name: 'Btown <b>', x: 20, y: 8, population: 300, rating: null, band: null, rating_floor: 500, advice: [] },
  ],
};
const empty = { company: null, rated_companies: [], companies: [], tolerance: 'lenient', count: 0, towns: [], fields: [], reason: 'no game connected: the MCP server is not up' };
const routes = (url) => {
  if (url === '/api/town/model') return model;
  if (url === '/api/town/live?company=1') return { ...live, company: 1 };
  if (url.startsWith('/api/town/live')) return live;
  return respond({ error: 'no route' }, { status: 404 });
};

const t = await import('../static/towns.js');

test('likes and dislikes come from the effects with a delta, split by sign, largest first', () => {
  const { likes, dislikes, other } = t.likesDislikes(model);
  assert.ok(likes.length > 3 && dislikes.length > 3);
  assert.ok(likes.every((e) => e.delta > 0) && dislikes.every((e) => e.delta < 0));
  assert.ok(likes[0].delta >= likes[likes.length - 1].delta);
  assert.ok(dislikes[0].delta <= dislikes[dislikes.length - 1].delta);
  assert.equal(likes.length + dislikes.length + other.length, model.effects.length, 'no effect is dropped');
  const tree = likes.find((e) => e.function === 'CmdPlantTree');
  assert.equal(tree.delta, 7);
  assert.match(tree.source, /^src\/tree_cmd\.cpp:\d+$/);
});

test('the removal thresholds form a check by tolerance matrix; other gates are listed plainly', () => {
  const m = t.removalMatrix(model);
  assert.deepEqual(m.checks, ['RoadRemove', 'TunnelBridgeRemove']);
  assert.deepEqual(m.tolerances, ['lenient', 'neutral', 'hostile', 'permissive']);
  assert.equal(m.cell('TunnelBridgeRemove', 'hostile').threshold, 400);
  assert.ok(t.plainGates(model).some((g) => g.function === 'CheckIfAuthorityAllowsNewStation' && g.threshold === -200));
  assert.match(t.gatesHtml(model), /RoadRemove[\s\S]*src\/town_cmd\.cpp:\d+/);
});

test('relations are grouped by their source node without losing an edge', () => {
  const g = t.relationGroups(model);
  assert.equal(g.reduce((n, x) => n + x.edges.length, 0), model.relations.length);
  assert.equal(new Set(g.map((x) => x.node)).size, g.length);
  assert.match(t.relationsHtml(model), /event:UpdateTownRating/);
});

test('actions, links and the growth table render with their sources as path:line', () => {
  const a = t.actionsHtml(model);
  for (const x of model.actions) assert.ok(a.includes(x.name) && a.includes(x.source), x.name);
  const l = t.linksHtml(model);
  assert.match(l, /statue_cargo_rating/);
  assert.match(l, /5\+/, 'the growth table has a 5+ column');
  assert.match(l, /difficulty\.town_council_tolerance/);
  assert.equal(t.sourceText('src/town.h:298'), 'src/town.h:298');
  assert.equal(t.signed(7), '+7');
  assert.equal(t.signed(-35), '-35');
});

test('live rows show rating, band and advice with why and source on hover, escaped', () => {
  const rows = t.liveRows(live).join('');
  assert.match(rows, /VERY_POOR/);
  assert.match(rows, /title="rating -260 is not above -200 \(src\/tree_cmd\.cpp CmdPlantTree\)"/);
  assert.match(rows, /Btown &lt;b&gt;/);
  assert.match(rows, /unknown/, 'a town without a published rating says so');
  assert.deepEqual(t.companyChoices(live).map((c) => c.id), [0, 1]);
  assert.deepEqual(t.companyChoices({ rated_companies: [3] }), [{ id: 3, name: 'company 3' }]);
  assert.match(t.liveHtml(empty), /No live towns: no game connected/);
});

test('the address holds the company', () => {
  assert.equal(t.getState('http://x/towns?company=2').company, 2);
  assert.equal(t.getState('http://x/towns?company=x').company, undefined);
});

test('/towns mounts every section and the company selector reloads the live towns', async () => {
  const env = fakeDom({ url: 'http://127.0.0.1:8800/towns', routes });
  const $ = env.byId;
  await t.mount();
  await env.settle();
  for (const id of ['likes', 'gates', 'actions', 'links', 'relations']) assert.match($(id).innerHTML, /<table/, id);
  assert.match($('live').innerHTML, /Aville/);
  assert.match($('live').innerHTML, /hostile/);
  assert.match($('t-msg').textContent, /town-model\/v1/);
  const sel = $('t-company');
  sel.value = '1';
  await sel.onchange({ target: sel });
  await env.settle();
  assert.equal(new URL(env.location.href).searchParams.get('company'), '1');
  assert.ok(env.fetches.some((f) => f.url === '/api/town/live?company=1'));
});
