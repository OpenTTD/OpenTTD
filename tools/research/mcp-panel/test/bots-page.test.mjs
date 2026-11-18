// node --test tools/research/mcp-panel/test/ : the My bot card (static/bots.js) mounted on the real
// index.html in the fake browser of fake-dom.mjs: the form for a new bot, the running bots' editors,
// Apply, undo and reset, each through the route the panel serves (panel_bot.rs: GET /api/bots,
// GET /api/bots/choices, POST /api/bots/start, POST /api/bots/setting). The fixtures are shaped like
// those answers with fictional values; nothing here talks to a game or a panel.
import './panel-ui.mjs'; // first: shell.js publishes PanelUI before any page script
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fakeDom, respond } from './fake-dom.mjs';

const here = path.dirname(new URL(import.meta.url).pathname);
const INDEX = fs.readFileSync(path.join(here, '..', 'static', 'index.html'), 'utf8');
const bodyOf = (html) => html.slice(html.indexOf('>', html.indexOf('<body')) + 1, html.lastIndexOf('</body>'));

/** One setting as panel_bot.rs answers it (info_settings, or openttd.ai.list with `ingame` added). */
const st = (name, value, description, extra = {}) => ({
  name, description, min: 0, max: 100, step: 1, default: value, flags: ['in_game'], labels: {}, value,
  editable: true, ingame: true, ...extra,
});
const declared = () => [
  st('aggressiveness', 50, 'How aggressively to expand (0 = passive, 100 = maximal).'),
  st('max_routes', 4, 'How many bus routes the company may run.', { min: 1, max: 30 }),
  st('cash_reserve', 20000, 'Money the company keeps in the bank.', { max: 500000, step: 5000 }),
  st('intercity', 1, '1: connect pairs of towns; 0: routes inside one town only.', { flags: ['in_game', 'boolean'], max: 1 }),
  st('air_a', 2, 'First town of the air route (town id; -1: none).', { min: -1, max: 65535 }),
  st('air_b', -1, 'Second town of the air route (town id; -1: none).', { min: -1, max: 65535 }),
  st('mode', 0, 'How the bot plays.', { labels: { 0: 'buses', 1: 'trains' }, max: 1 }),
  st('debug_log', 0, 'A developer switch.', { flags: [], ingame: false, max: 1 }),
  st('loan_budget', 0, 'Money the bot may borrow.', { max: 1000000 }),
];
const bots = (over = {}) => ({
  source: 'mcp:openttd.ai.list',
  ais: [{ ai: 'PrismaticAI', dir: 'PrismaticAI', settings: declared() }, { ai: 'AdmiralAI', dir: null, settings: [] }],
  running: [{ id: 1, name: 'Fictional Transport', money: 1234567, ai: 'PrismaticAI', running: true, settings: declared() }],
  live: { list: true, set_setting: true },
  ...over,
});
const choices = { town: [{ id: 2, name: 'Fictional Town', population: 1200 }, { id: 5, name: 'Other Town', population: 300 }], industry: [], cargo: [] };

let answer = bots();
// A gated call answers pending_approval; an answer without an approval id still waits for one.
let settingAnswer = (body) => (body.value === 70 ? { ok: true } : { status: 'pending_approval', approval_id: 40 + body.value });
const posts = [];
const routes = (url, init) => {
  const body = init.body ? JSON.parse(init.body) : undefined;
  if (init.method === 'POST') posts.push([url, body]);
  if (url === '/api/bots') return answer;
  if (url === '/api/bots/choices') return choices;
  if (url === '/api/bots/setting') return settingAnswer(body);
  if (url === '/api/bots/start') return { status: 'pending_approval', approval_id: 9 };
  return respond({ error: 'no route' }, { status: 404 });
};

const env = fakeDom({ url: 'http://127.0.0.1:8800/', routes });
env.document.body.innerHTML = bodyOf(INDEX);
// app.js's api(), which the page publishes as a global for bots.js: POST with a JSON body, the answer parsed.
globalThis.api = async (p, body) => {
  const r = await fetch(p, body === undefined ? {} : { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) });
  const text = await r.text();
  try { return JSON.parse(text); } catch { return { status: r.status, text }; }
};
let ticks = 0;
globalThis.tick = () => { ticks++; };
// One instance of the module per process (node's coverage report keeps one per file): later documents
// mount it again through its exported mount().
const { mount } = await import('../static/bots.js');
const $ = env.byId;
const card = () => $('bot-running').querySelector('[data-bot="1"]');
const control = (name, root = card()) => root.querySelector(`[data-setting="${name}"]`);

test('the card fills itself from GET /api/bots and /api/bots/choices', async () => {
  await env.settle();
  assert.deepEqual(env.fetches.map((f) => f.url).sort(), ['/api/bots', '/api/bots/choices']);
  assert.equal($('bot-source').textContent, 'mcp:openttd.ai.list');
  assert.deepEqual($('bot-ai').options.map((o) => o.value), ['PrismaticAI', 'AdmiralAI']);
  const form = $('bot-new-form');
  assert.equal(control('aggressiveness', form).value, '50', 'the new bot starts from the defaults');
  assert.equal(control('air_a', form).dataset.kind, 'picker', 'a town id picks from the live world');
  assert.equal(control('loan_budget', form).dataset.kind, 'number', 'a range too long to slide is a number box');
  assert.equal(control('mode', form).dataset.kind, 'select', 'labels are a select');
  assert.match(form.innerHTML, /Fictional Town \(#2, 1,200 people\)/);
  const c = card();
  assert.match(c.innerHTML, /Fictional Transport/);
  assert.match(c.innerHTML, /company 1 · PrismaticAI · money 1,234,567/);
  assert.equal(control('debug_log'), null, 'a setting that cannot change in game is not offered on a running bot');
  assert.equal(c.querySelector('[data-pending]').textContent, 'no change');
  assert.equal(c.querySelector('[data-apply-bot]').disabled, true, 'nothing to apply yet');
  assert.equal($('bot-msg').innerHTML, '', 'no warning while the game can change a running bot');
});

test('editing a running bot marks the field and says what Apply will ask', async () => {
  const el = control('max_routes');
  el.value = '6';
  await el.dispatch('input');
  assert.equal(el.getAttribute('data-dirty'), 'true');
  assert.equal(control('aggressiveness').getAttribute('data-dirty'), 'false');
  assert.equal(card().querySelector('[data-pending]').textContent, '1 change, 1 approval');
  const apply = card().querySelector('[data-apply-bot]');
  assert.equal(apply.disabled, false);
  assert.equal(apply.textContent, 'Apply 1');
  assert.equal(card().querySelector('[data-reset-bot]').disabled, false);
  // the range and its number box move together; a toggle says on or off
  const range = card().querySelector(`#${control('aggressiveness').dataset.pair}`);
  range.value = '75';
  await range.dispatch('input');
  assert.equal(control('aggressiveness').value, '75', 'the slider writes its number box');
  const flag = control('intercity');
  flag.checked = false;
  await flag.dispatch('input');
  assert.equal(flag.closest('label').querySelector('[data-toggle-text]').textContent, 'off');
  assert.equal(card().querySelector('[data-pending]').textContent, '3 changes, 3 approvals');
  const pick = control('air_b');
  pick.value = '5';
  await pick.dispatch('change');
  assert.equal(card().querySelector('[data-pending]').textContent, '4 changes, 4 approvals');
});

test('a refresh while the owner edits keeps the edit; Reset puts the game values back', async () => {
  answer = bots({ running: [{ ...bots().running[0], money: 999 }] });
  await env.tick();
  assert.equal(control('max_routes').value, '6', 'the running editor is not redrawn under the owner');
  await card().querySelector('[data-reset-bot]').dispatch('click');
  assert.equal(control('max_routes').value, '4');
  assert.equal(card().querySelector('[data-pending]').textContent, 'no change');
  assert.match(card().innerHTML, /money 1,234,567/, 'the answer that came while editing is not taken');
  await env.tick();
  assert.match(card().innerHTML, /money 999/, 'the next refresh after Reset is');
});

test('Apply refuses a value outside the declared range and asks nothing', async () => {
  const el = control('max_routes');
  el.value = '31';
  await el.dispatch('input');
  posts.length = 0;
  await card().querySelector('[data-apply-bot]').dispatch('click');
  assert.equal($('bot-msg').innerHTML, 'max_routes: 31 is outside 1..30');
  assert.deepEqual(posts, []);
});

test('Apply asks exactly the keys that differ, one approval each, and shows what became of them', async () => {
  control('max_routes').value = '6';
  await control('max_routes').dispatch('input');
  control('aggressiveness').value = '70';
  await control('aggressiveness').dispatch('input');
  posts.length = 0;
  ticks = 0;
  await card().querySelector('[data-apply-bot]').dispatch('click');
  await env.settle();
  assert.deepEqual(posts, [
    ['/api/bots/setting', { company: 1, name: 'aggressiveness', value: 70 }],
    ['/api/bots/setting', { company: 1, name: 'max_routes', value: 6 }],
  ]);
  assert.equal($('bot-msg').innerHTML, 'Asked 2 changes of company 1; each waits for its approval.');
  assert.match(card().innerHTML, /approval #46: 4 → 6/);
  assert.match(card().innerHTML, /approval #—: 50 → 70/);
  assert.equal(ticks, 1, 'the home page refreshes its approvals');
  assert.equal(control('max_routes').value, '4', 'the field shows the game value until the game reads it back');
});

test('a value the game reads back is applied, and undo asks the old value again', async () => {
  const running = bots().running[0];
  running.settings.find((s) => s.name === 'max_routes').value = 6;
  answer = bots({ running: [running] });
  await env.tick();
  assert.match(card().innerHTML, /applied 4 → 6/);
  assert.match(card().innerHTML, /approval #—: 50 → 70/, 'a key the game has not read yet still waits');
  const undo = card().querySelector('[data-undo="c1:max_routes"]');
  assert.ok(undo, 'an applied key can be undone');
  posts.length = 0;
  settingAnswer = () => ({ error: 'refused by the owner' });
  await undo.dispatch('click');
  await env.settle();
  assert.deepEqual(posts, [['/api/bots/setting', { company: 1, name: 'max_routes', value: 4 }]]);
  assert.match(card().innerHTML, /refused: refused by the owner/);
});

test('Start this bot sends the form with its AI, or names the empty field', async () => {
  const form = $('bot-new-form');
  control('max_routes', form).value = '';
  posts.length = 0;
  await $('bot-start').onclick();
  assert.equal($('bot-msg').innerHTML, 'max_routes: give it a value');
  assert.deepEqual(posts, []);
  control('max_routes', form).value = '9';
  const before = env.timers.length;
  await $('bot-start').onclick();
  assert.equal(posts.length, 1);
  const [url, body] = posts[0];
  assert.equal(url, '/api/bots/start');
  assert.equal(body.ai, 'PrismaticAI');
  assert.equal(body.settings.max_routes, 9);
  assert.equal(body.settings.intercity, 1);
  assert.equal(body.settings.air_a, 2);
  assert.equal(body.settings.debug_log, 0, 'a new bot gets every declared setting');
  assert.equal($('bot-msg').innerHTML, 'Start waits for approval #9.');
  assert.equal(env.timers.length, before + 1, 'and the card looks again a little later');
  assert.equal(env.timers.at(-1).ms, 3000);
});

test('an AI that declares nothing starts with its defaults', async () => {
  $('bot-ai').value = 'AdmiralAI';
  $('bot-ai').onchange();
  assert.match($('bot-new-form').innerHTML, /declares no setting; it starts with its own defaults/);
  posts.length = 0;
  await $('bot-start').onclick();
  assert.deepEqual(posts, [['/api/bots/start', { ai: 'AdmiralAI', settings: {} }]]);
});

test('a game binary without set_setting or ai.list says so, and a game without bots opens the form', async () => {
  const env2 = fakeDom({ url: 'http://127.0.0.1:8800/', routes });
  env2.document.body.innerHTML = bodyOf(INDEX);
  answer = bots({ live: { list: false, set_setting: false }, running: [{ id: 3, name: '', money: 'n/a', ai: null, running: false, settings: [] }] });
  mount();
  await env2.settle();
  const $2 = env2.byId;
  assert.match($2('bot-msg').innerHTML, /cannot change a running bot's settings \(no openttd\.ai\.set_setting\)/);
  const c = $2('bot-running').querySelector('[data-bot="3"]');
  assert.match(c.innerHTML, /Company 3/, 'a company without a name is named by its id');
  assert.match(c.innerHTML, /\(stopped\) · money n\/a/);
  assert.match(c.innerHTML, /not readable: this game binary has no openttd\.ai\.list/);
  assert.equal(c.querySelector('[data-apply-bot]'), null, 'nothing to apply');

  const env3 = fakeDom({ url: 'http://127.0.0.1:8800/', routes });
  env3.document.body.innerHTML = bodyOf(INDEX);
  answer = bots({ running: [] });
  mount();
  await env3.settle();
  assert.match(env3.byId('bot-running').innerHTML, /No bot is running\. Start one below\./);
  assert.equal(env3.byId('bot-new').open, true, 'with nothing running the new-bot form is open');
});
