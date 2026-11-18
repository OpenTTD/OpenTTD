// node --test tools/research/mcp-panel/test/ : the panel's home script (static/app.js), its markup
// builders from fixtures and its wiring in the fake browser of fake-dom.mjs. The fixtures are
// fictional; nothing here talks to a game.
import { UI } from './panel-ui.mjs';
import test from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import { fakeDom, respond } from './fake-dom.mjs';

const status = {
  mcp_up: true,
  endpoint: 'http://127.0.0.1:9000/mcp',
  game_port: 3979,
  auto_approve: false,
  pending: [{ id: 3, tool: 'openttd.cmd.BuildRoad', args: { tile: 7 } }],
  events: [{ t: '12:00:01', kind: 'ai.log', text: 'built a <bus> route' }],
};
const overview = {
  state: {
    game_date: '1950-02-01',
    game_tick: 1234567,
    data: {
      fleet: { train: 2, road: 3, ship: 0, aircraft: 0, total: 5 },
      num_companies: 1,
      num_towns: 9,
      map_width: 256,
      map_height: 128,
      paused: false,
    },
  },
  companies: {
    data: {
      companies: [
        {
          id: 0,
          name: 'Fictional & Sons',
          is_ai: true,
          money: 1500000,
          value: 2e6,
          inaugurated_year: 1950,
        },
      ],
    },
  },
  decisions: {
    data: {
      decisions: [
        {
          decision_id: 1,
          action_type: 'build_road',
          approval_id: 3,
          command_result: 'ok',
          verdict: 'VERIFIED',
          observed_tick: 9,
        },
      ],
    },
  },
};
const posts = [];
const routes = (url, init) => {
  if (init.method === 'POST') posts.push([url, JSON.parse(init.body || '{}')]);
  if (url === '/api/status') return status;
  if (url === '/api/overview') return overview;
  if (url === '/api/instances')
    return {
      registry: '/tmp/r.json',
      instances: [
        { name: 'a', role: 'panel', alive: true, endpoint: 'e', url: 'http://a' },
        { name: 'b', role: 'game', alive: false, error: 'refused', mcp: 'm' },
      ],
    };
  if (url.startsWith('/api/news?'))
    return { news: [{ date: '1950-01-02', type: 'general', text: 'A <town> grows' }] };
  if (url === '/api/news') return { ok: true, shown: 'news #1' };
  if (url === '/api/news/close') return { ok: true };
  if (url === '/api/resources/towns')
    return {
      data: {
        towns: [
          { id: 2, name: 'Zed' },
          { id: 1, name: 'Alpha' },
        ],
      },
    };
  if (url === '/api/ask')
    return {
      asks: [
        { id: 1, answer: 'yes', question: 'Build?' },
        { id: 2, answer: null, question: 'Sell?' },
      ],
    };
  if (url === '/api/actionplan') {
    return {
      ok: true,
      result: {
        steps: [{ n: 1, action: 'loan', presses: 2, press: 1, cash: 10, loan: 5 }],
        waves: 1,
        final: { cash: 10, loan: 5 },
        lowest: { cash: 1 },
        graph: {
          nodes: [
            { id: 'a', wave: 0, presses: 2 },
            { id: 'b', wave: 1, label: 'B' },
          ],
          edges: [{ from: 'a', to: 'b' }],
        },
      },
    };
  }
  if (url.startsWith('/api/content?'))
    return {
      items: [
        { id: 11, name: 'Set <x>', type: 'NewGRF', state: 'available' },
        { id: 12, name: 'Old', type: 'AI', state: 'installed' },
      ],
      count: 2,
      query: 'set',
    };
  if (url === '/api/content/download') return { ids: [11], output: ['heartbeat', 'downloaded 11'] };
  if (url === '/api/console') return { output: ['line one', 'line two'] };
  if (url === '/api/rpc') return { result: { tools: [] } };
  if (url.startsWith('/api/resource?')) return { data: { towns: 9 } };
  if (url === '/api/rhai/stream') {
    const chunks = ['event: line\ndata: "rhai says hi"\n\n', 'event: exit\ndata: 0\n\n'].map((c) =>
      new TextEncoder().encode(c),
    );
    return {
      ok: true,
      status: 200,
      json: async () => ({}),
      text: async () => '',
      body: {
        getReader: () => ({
          read: async () =>
            chunks.length ? { done: false, value: chunks.shift() } : { done: true },
        }),
      },
    };
  }
  if (url === '/openapi.json') return { paths: {} };
  return respond({ error: 'no route' }, { status: 404 });
};
const env = fakeDom({ url: 'http://127.0.0.1:8800/#console', routes });
globalThis.ecInit = (box, ec) => ec.init(box);
const app = createRequire(import.meta.url)('../static/app.js');
const $ = env.byId;
const consoleText = () => $('console-out').textContent;

test('the markup builders draw approvals, events, companies and decisions from the answers', () => {
  const p = app.pendingHtml(status.pending);
  assert.match(p, /data-approve="3"/);
  assert.ok(p.includes(UI.primary), "Approve is the shell's primary button");
  assert.match(p, /openttd\.cmd\.BuildRoad\(\{&quot;tile&quot;:7\}\)/);
  assert.equal(app.pendingHtml([]), '<li>none</li>');
  assert.match(
    app.eventsHtml(status.events),
    /12:00:01 · ai\.log<\/time>[\s\S]*built a &lt;bus&gt; route/,
  );
  const c = app.companiesHtml(overview.companies.data.companies);
  assert.match(c, /Fictional &amp; Sons/);
  assert.match(c, /1,500,000/);
  assert.ok(c.includes(`<span class="${UI.badge} `), "the AI mark is the shell's badge");
  assert.match(app.companiesHtml([{ id: 1, is_ai: false }]), />human</);
  assert.match(app.companiesHtml([]), /no company yet/);
  assert.match(app.decisionsHtml(overview.decisions.data.decisions), /#3<\/td>[\s\S]*VERIFIED/);
  assert.match(app.decisionsHtml([]), /none yet/);
  assert.equal(app.money(1234), '1,234');
  assert.equal(app.money(undefined), '-');
  assert.equal(app.money('n/a'), 'n/a');
});

test('the fleet counts vehicles by type, or says when only the pool count is known', () => {
  assert.deepEqual(
    app.fleetText({ fleet: { train: 2, road: 0, ship: 1, aircraft: 0, total: 3 } }),
    { total: 3, types: '2 trains · 1 ships' },
  );
  assert.deepEqual(app.fleetText({ fleet: { total: 0 } }), { total: 0, types: 'none yet' });
  assert.deepEqual(app.fleetText({ num_vehicles: 40 }), {
    total: 40,
    types: 'pool items (wagons, effects included)',
  });
});

test('cluster, news, towns, questions, the action plan and content come from their answers', () => {
  const cl = app.clusterHtml([
    { name: 'a', role: 'r', alive: true, url: 'http://a' },
    { name: 'b', alive: false, error: 'x', mcp: 'm' },
  ]);
  assert.match(cl, />alive</);
  assert.match(cl, />down<\/span> <span class="text-xs">x</);
  assert.match(cl, /href="http:\/\/a"/);
  assert.match(app.clusterHtml([]), /no instance registered/);
  assert.match(
    app.newsHtml([{ date: 'd', type: 't', text: '<n>' }]),
    /d · t<\/time>[\s\S]*&lt;n&gt;/,
  );
  assert.match(app.newsHtml([]), /no news yet/);
  assert.match(
    app.townOptions([
      { id: 2, name: 'Zed' },
      { id: 1, name: 'Alpha' },
    ]),
    /No viewport.*value="1">Alpha.*value="2">Zed/,
  );
  const asks = app.asksHtml([
    { id: 1, answer: 'yes', question: 'q' },
    { id: 2, answer: 'no' },
    { id: 3, answer: 'replaced' },
    { id: 4 },
  ]);
  for (const w of ['>yes<', '>no<', '>replaced<', '>waiting for you<'])
    assert.ok(asks.includes(w), w);
  assert.match(app.asksHtml([]), /no question asked yet/);
  const res = routes('/api/actionplan', {}).result;
  assert.equal(
    app.planSummary(res),
    '1 press(es) in 1 wave(s); final cash 10, loan 5; lowest cash 1',
  );
  assert.equal(app.planSummary({ ...res, lowest: 3 }).endsWith('lowest cash 3'), true);
  assert.match(app.planStepsHtml(res.steps), /1\. <b>loan<\/b> \(1\/2\) &rarr; cash 10, loan 5/);
  const g = app.planGraphOption(res.graph).series[0];
  assert.deepEqual(
    g.data.map((n) => [n.name, n.x, n.y]),
    [
      ['a x2', 0, 0],
      ['B', 160, 0],
    ],
  );
  assert.deepEqual(g.links, [{ source: 'a', target: 'b' }]);
  const rows = app.contentRowsHtml(routes('/api/content?q=', {}).items);
  assert.ok(rows.includes(`class="${UI.check}"`), "a content box is the shell's check");
  assert.match(rows, /value="12" disabled/, 'installed content cannot be ticked');
});

test('console lines are classified, JSON coloured, and /tail adds the child kinds', () => {
  assert.equal(app.lineClass('> /help'), 'text-white font-semibold');
  assert.equal(app.lineClass('Runtime error at 1'), 'text-red-400');
  assert.equal(app.lineClass('NOT RUN yet'), 'text-yellow-300');
  assert.equal(app.lineClass('gate PASS'), 'text-green-400');
  assert.equal(app.lineClass('[company.founded] 1'), 'text-sky-300');
  assert.equal(app.lineClass('dbg: [x]'), 'text-gray-500');
  assert.equal(app.lineClass('plain'), 'text-green-200');
  const j = app.consoleHtml('{"a": 1, "b": "x", "c": true}');
  assert.match(j, /text-sky-300">&quot;a&quot;<\/span>: <span class="text-purple-300">1/);
  assert.match(j, /text-amber-200">&quot;x&quot;/);
  assert.match(j, /text-pink-300">true/);
  assert.match(app.consoleHtml('{not json'), /class="block text-green-200">\{not json/);
  assert.equal(app.consoleHtml('a\nb').split('<span').length - 1, 2);
  assert.deepEqual(app.tailNames('company'), [
    'company',
    'company.founded',
    'company.renamed',
    'company.removed',
    'company.observed',
  ]);
  assert.deepEqual(app.tailNames('ai.log'), ['ai.log']);
});

test('notable events become toasts; the rest stays quiet', () => {
  const n = (kind, data) => app.NOTABLE({ kind, data });
  assert.equal(n('ai.log', { text: 'PrismaticAI: built a bus route' }), 'built a bus route');
  assert.equal(n('ai.log', { text: 'idle' }), null);
  assert.equal(n('ai.log', { text: 'x', level: 'error' }), 'x');
  assert.equal(n('company.founded', { id: 1 }), 'Company #1 founded');
  assert.equal(n('company.renamed', { id: 1, to: 'B' }), 'Company #1 is now B');
  assert.equal(
    n('decision.recorded', { decision_id: 2, action_type: 'a', command_result: 'ok' }),
    'Decision #2: a ok',
  );
  assert.equal(n('game.paused', { date: 'd' }), 'Paused at d');
  assert.equal(n('game.resumed', { date: 'd' }), 'Resumed at d');
  assert.equal(n('game.month', { date: 'd', vehicles: 3 }), 'd: 3 vehicles');
  assert.equal(n('game.new_world', { towns: 4 }), 'New world, 4 towns');
  assert.equal(
    n('panel.mcp', { text: 'pending approval #3' }),
    'Approval needed: pending approval #3',
  );
  assert.equal(n('panel.mcp', { text: 'other' }), null);
  assert.equal(n('vehicles.count', {}), null);
  assert.match(app.toastHtml('game.month', '<d>'), /game\.month<\/span>&lt;d&gt;/);
});

test('the page fills itself: status, overview, cluster, news and questions', async () => {
  await env.settle();
  assert.equal($('endpoint').textContent, 'http://127.0.0.1:9000/mcp · game 127.0.0.1:3979');
  assert.match($('pending').innerHTML, /data-approve="3"/);
  assert.equal($('s-date').textContent, '1950-02-01');
  assert.equal($('s-tick').textContent, 'tick 1,234,567');
  assert.equal($('s-vehicles').textContent, '5');
  assert.equal($('s-fleet-types').textContent, '2 trains · 3 road');
  assert.equal($('s-map-size').textContent, 'map 256×128');
  assert.equal($('s-paused').textContent, 'running');
  assert.match($('companies').innerHTML, /Fictional &amp; Sons/);
  assert.match($('cluster-reg').textContent, /2 instance\(s\)/);
  assert.match($('news-list').innerHTML, /A &lt;town&gt; grows/);
  assert.match($('news-town').innerHTML, /Alpha.*Zed/);
  assert.match($('ask-list').innerHTML, /waiting for you/);
  assert.ok(
    !$('console').classList.contains('hidden'),
    '#console in the address opens the console',
  );
  assert.match(consoleText(), /developer console/);
});

test('buttons call their routes: approve, pause, resume, AI, auto-approve', async () => {
  const btn = $('pending').querySelector('[data-approve]');
  await env.document.dispatch('click', { target: btn });
  await $('btn-pause').onclick();
  await $('btn-resume').onclick();
  await $('btn-ai').onclick();
  await $('auto').onchange({ target: { checked: true } });
  await env.settle();
  const sent = posts.map(([u, b]) => `${u} ${JSON.stringify(b)}`);
  for (const s of [
    '/api/approve {"id":3}',
    '/api/pause {"pause":true}',
    '/api/pause {"pause":false}',
    '/api/start-ai {}',
    '/api/auto-approve {"on":true}',
  ]) {
    assert.ok(sent.includes(s), s);
  }
  const link = env.document.createElement('a');
  link.setAttribute('data-read', 'openttd://map/terrain');
  await env.document.dispatch('click', { target: link });
  assert.equal(
    env.location.pathname,
    '/resource/map/terrain',
    'a read link opens the resource page',
  );
  await env.document.dispatch('click', { target: env.document.createElement('div') });
});

test('the console runs panel commands, MCP, Rhai and the console of the game', async () => {
  const run = async (line) => {
    $('console-in').value = line;
    await $('console-form').listeners.submit[0]({ preventDefault() {} });
    await env.settle();
  };
  await run('/help');
  assert.match(consoleText(), /raw MCP JSON-RPC/);
  await run('/mcp tools/list {"cursor":null}');
  assert.deepEqual(posts.at(-1), ['/api/rpc', { method: 'tools/list', params: { cursor: null } }]);
  await run('/read openttd://towns');
  assert.match(consoleText(), /"towns": 9/);
  await run('/pause');
  await run('/approve 4');
  assert.deepEqual(posts.at(-1), ['/api/approve', { id: 4 }]);
  await run('/auto off');
  await run('/rhai print("hi")');
  assert.match(consoleText(), /rhai says hi[\s\S]*rhai exit 0/);
  await run('/tail company');
  const src = env.sources.at(-1);
  assert.match(src.src, /kinds=company$/);
  assert.ok(src.listeners['company.founded'], 'a child kind is listened to under its own name');
  src.emit('company.founded', { kind: 'company.founded', data: { text: 'Company #1 founded' } });
  assert.match(consoleText(), /\[company\.founded\] Company #1 founded/);
  await run('/untail');
  assert.ok(src.closed);
  await run('companies');
  assert.match(consoleText(), /line one\nline two|line oneline two/);
  await run('/mcp tools/list {broken');
  assert.match(consoleText(), /error: /);
  await run('/clear');
  assert.equal(consoleText(), '');
  await run('   ');
  const keys = $('console-in').listeners.keydown[0];
  const ev = (key) => ({ key, target: $('console-in'), preventDefault() {} });
  const up = ev('ArrowUp');
  keys(up);
  assert.equal(up.target.value, '/clear', 'ArrowUp brings back the last line typed');
  keys(ev('ArrowDown'));
});

test('the console toggles with ~ and its buttons', async () => {
  const hidden = () => $('console').classList.contains('hidden');
  $('close-console').onclick();
  assert.ok(hidden());
  env.document.activeElement = null;
  await env.document.dispatch('keydown', { key: '~' });
  assert.ok(!hidden(), '~ opens it');
  $('console-in').value = '';
  await env.document.dispatch('keydown', { key: '`' });
  assert.ok(hidden(), '` in an empty console closes it');
  $('open-console').onclick();
  $('side-console').onclick();
  assert.ok(!hidden());
});

test("live events become toasts with the shell's card, at most five", async () => {
  const es = env.sources.find((s) => /panel\.mcp$/.test(s.src));
  for (let i = 0; i < 7; i++)
    es.emit('game.month', { kind: 'game.month', data: { date: `1950-0${i}`, vehicles: i } });
  es.emit('ai.log', { kind: 'ai.log', data: { text: 'idle' } });
  const toasts = $('toasts').children;
  assert.equal(toasts.length, 5);
  for (const c of UI.card.split(' ')) assert.ok(toasts[0].classList.contains(c), c);
  assert.match(toasts[0].innerHTML, /1950-06: 6 vehicles/);
  toasts[0].querySelector('button').onclick();
  assert.equal($('toasts').children.length, 4, 'its close button removes it');
});

test('news, the action plan and content act through their routes', async () => {
  $('news-text').value = 'Hello';
  $('news-town').value = '2';
  $('news-style').value = 'bold';
  await $('news-form').listeners.submit[0]({ preventDefault() {} });
  assert.deepEqual(posts.find(([u]) => u === '/api/news')[1], {
    text: 'Hello',
    town: 2,
    style: 'bold',
  });
  assert.match($('news-msg').textContent, /shown in your game: news #1/);
  await $('news-close').listeners.click[0]();
  assert.equal($('news-msg').textContent, 'news window closed');

  $('plan-actions').value = '{broken';
  await $('plan-run').onclick();
  assert.match($('plan-msg').textContent, /actions are not JSON/);
  $('plan-actions').value = '[{"action":"loan"}]';
  await $('plan-run').onclick();
  assert.match($('plan-msg').textContent, /1 press\(es\) in 1 wave/);
  assert.match($('plan-steps').innerHTML, /<b>loan<\/b>/);
  assert.equal(
    env.charts.at(-1).getOption().series[0].data.length,
    2,
    'the DAG is drawn through the one door',
  );

  $('content-q').value = 'set';
  await $('content-form').onsubmit({ preventDefault() {} });
  assert.match($('content-msg').textContent, /2 item\(s\) for "set"/);
  const box = $('content-rows').querySelector('input[type=checkbox]');
  box.checked = true;
  box.onchange();
  assert.equal($('content-download').disabled, false);
  await $('content-download').onclick();
  assert.deepEqual(posts.at(-1), ['/api/content/download', { ids: [11] }]);
  assert.match($('content-msg').innerHTML, /Done: downloaded 11/);
});

test('timers refresh the page and the live view swaps in each new screenshot', async () => {
  const before = env.fetches.length;
  await env.tick();
  assert.ok(env.fetches.length > before, 'the intervals poll again');
  const img = env.images.at(-1);
  img.onload();
  assert.match($('live-msg').textContent, /^updated /);
  await env.tick();
  env.images.at(-1).onerror();
  assert.equal($('live-msg').textContent, 'no GUI client attached yet');
});
