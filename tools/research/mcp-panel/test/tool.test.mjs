// node --test tools/research/mcp-panel/test/ : one page per tool, client tool and resource
// (static/tool.js), derived from an OpenAPI fixture, in the fake browser of fake-dom.mjs.
// The fixture is fictional; nothing here talks to a game.
import { UI } from './panel-ui.mjs';
import test from 'node:test';
import assert from 'node:assert/strict';
import { fakeDom, respond } from './fake-dom.mjs';
import { catalogueItems, KIND_LABEL } from '../static/catalogue.js';

const schema = {
  type: 'object',
  required: ['company'],
  properties: {
    company: { type: 'integer', minimum: 0, maximum: 14, description: 'company id' },
    name: { type: 'string' },
    value: { type: 'number' },
    mode: { enum: ['a', 'b'] },
    live: { type: 'boolean' },
    extra: { type: 'object' },
    list: { type: 'array' },
  },
};
const doc = {
  paths: {
    '/api/tools/openttd.ai.set_setting': {
      post: {
        tags: ['mcp tools'],
        operationId: 'setSetting',
        summary: 'Approval-gated: set a setting',
        requestBody: { content: { 'application/json': { schema } } },
      },
    },
    '/api/tools/openttd.ai.list': {
      post: { tags: ['mcp tools'], operationId: 'aiList', description: 'Read-only list' },
    },
    '/api/client/openttd.view.ask': {
      post: { tags: ['gui client tools'], operationId: 'ask', summary: 'Ask the player' },
    },
    '/api/resources/map/terrain': {
      get: { tags: ['mcp resources'], operationId: 'terrain', summary: 'Terrain per tile' },
    },
    '/api/status': { get: { tags: ['panel'] } },
  },
};
let answer = { status: 'pending_approval', approval_id: 9 };
const env = fakeDom({
  url: 'http://127.0.0.1:8800/tool/openttd.ai.set_setting?company=3&live=true',
  routes: (url) =>
    url === '/openapi.json' ? doc : url === '/api/resources/map/terrain' ? 'not json' : answer,
});
const $ = env.byId;
let loaded = null;
/** Open an address: the module's first load draws it (boot), a later address is drawn by start(),
    as the browser would on a fresh load (one module instance, so coverage counts every page). */
async function open(url) {
  env.location.href = url;
  if (loaded) await loaded.start();
  else {
    loaded = await import('../static/tool.js');
    await loaded.boot;
  }
  await env.settle();
  return loaded;
}

test('items come from the OpenAPI tags, sorted, each with its page and schema', async () => {
  const { gated, readOnly } = await open(
    'http://127.0.0.1:8800/tool/openttd.ai.set_setting?company=3&live=true',
  );
  const all = catalogueItems(doc);
  assert.deepEqual(
    all.map((i) => [i.kind, i.name, i.page]),
    [
      ['resource', 'openttd://map/terrain', '/resource/map/terrain'],
      ['tool', 'openttd.ai.list', '/tool/openttd.ai.list'],
      ['tool', 'openttd.ai.set_setting', '/tool/openttd.ai.set_setting'],
      ['client', 'openttd.view.ask', '/client/openttd.view.ask'],
    ],
    'sorted by name (localeCompare)',
  );
  assert.equal(all[2].schema, schema);
  assert.equal(all[1].summary, 'Read-only list', 'a description stands in for a missing summary');
  assert.deepEqual(all.map(gated), [false, false, true, false]);
  assert.deepEqual(all.map(readOnly), [true, true, false, false]);
  assert.deepEqual(
    all.map((i) => i.opId),
    ['terrain', 'aiList', 'setSetting', 'ask'],
  );
  assert.equal(all[1].schema, null, 'no request body, no schema');
  assert.deepEqual(catalogueItems({}), []);
});

test("a field per schema property, in the shell's sets, its value from the address", async () => {
  const { field, shell, alert, badge } = await open(
    'http://127.0.0.1:8800/tool/openttd.ai.set_setting?company=3&live=true',
  );
  const num = field('company', schema.properties.company, true, '3');
  assert.match(num, /type="number" step="1" value="3" min="0" max="14"/);
  assert.match(num, /≥ 0 ≤ 14/);
  assert.ok(num.includes(`class="${UI.input} block w-full" required`));
  assert.match(
    field('mode', schema.properties.mode, false, 'b'),
    /<select [^>]*pe-9[^"]*"><option value="">-<\/option><option>a<\/option><option selected>/,
  );
  assert.ok(
    field('live', schema.properties.live, false, 'true').includes(`checked class="${UI.check}"`),
  );
  assert.match(field('extra', schema.properties.extra), /<textarea [^>]*placeholder="\{ \}"/);
  assert.match(field('list', schema.properties.list), /placeholder="\[ \]"/);
  assert.match(field('name', { type: 'string' }, false, '<x>'), /type="text" value="&lt;x&gt;"/);
  assert.equal(shell('plain/path:1'), 'plain/path:1');
  assert.equal(shell("it's"), "'it'\\''s'");
  assert.match(alert('<b>x</b>', 'red'), /role="alert">[^]*<b>x<\/b>/);
  assert.match(badge('t'), new RegExp(`class="${UI.badge} bg-gray-100`));
});

test('the same call on every surface: REST, MCP, the in-game console and OpenAPI', async () => {
  const { toolSurfaces, resourceSurfaces, surfacesHtml } = await open(
    'http://127.0.0.1:8800/tool/openttd.ai.list',
  );
  const byName = Object.fromEntries(catalogueItems(doc).map((i) => [i.name, i]));
  const [list, ask, terrain] = ['openttd.ai.list', 'openttd.view.ask', 'openttd://map/terrain'].map(
    (n) => byName[n],
  );
  const rows = Object.fromEntries(toolSurfaces(list, { company: 1, x: { a: 1 } }, 'h:1'));
  assert.equal(
    rows['REST (this panel)'],
    "curl -s -X POST -H 'Content-Type: application/json' -d '{\"company\":1,\"x\":{\"a\":1}}' " +
      'http://h:1/api/tools/openttd.ai.list',
  );
  assert.equal(rows['In-game console ~'], 'mcp openttd.ai.list company=1 x={"a":1}');
  assert.match(rows['MCP JSON-RPC'], /"method":"tools\/call","params":\{"name":"openttd.ai.list"/);
  assert.equal(rows.OpenAPI, 'POST /api/tools/openttd.ai.list · operationId aiList · /docs');
  assert.match(
    Object.fromEntries(toolSurfaces(ask, {}, 'h'))['In-game console ~'],
    /own MCP server/,
  );
  assert.equal(
    Object.fromEntries(toolSurfaces(list, {}, 'h'))['In-game console ~'],
    'mcp openttd.ai.list',
  );
  assert.equal(resourceSurfaces(terrain, 'h')[2][1], 'mcp read openttd://map/terrain');
  assert.match(surfacesHtml([['k<', 'v>']]), /k&lt;<\/dt>.*v&gt;<\/dd>/);
});

test('a tool page builds its form, keeps values in the address and calls the tool', async () => {
  const m = await open('http://127.0.0.1:8800/tool/openttd.ai.set_setting?company=3&live=true');
  assert.deepEqual(
    env.document.querySelectorAll('#call [data-key]').map((e) => e.dataset.key),
    ['company', 'name', 'value', 'mode', 'live', 'extra', 'list'],
  );
  assert.match($('badges').innerHTML, /server tool[^]*approval-gated/);
  assert.match($('crumbs').innerHTML, /Server tools/);
  assert.ok($('page').innerHTML.includes(`class="${UI.primary}">Call openttd.ai.set_setting`));
  assert.deepEqual(m.readForm(), { company: 3, live: true });
  $('f-value').value = '2.5';
  $('f-extra').value = '{"a":1}';
  $('f-name').value = 'x';
  $('call').oninput();
  assert.equal(
    env.location.search,
    '?company=3&live=true&name=x&value=2.5&extra=%7B%22a%22%3A1%7D',
  );
  assert.match(
    $('surfaces').innerHTML,
    /mcp openttd.ai.set_setting company=3 name=x value=2.5 live=true extra=\{&quot;a&quot;:1\}/,
  );
  await $('share').onclick();
  assert.equal(env.copied, env.location.href);
  await $('call').onsubmit({ preventDefault() {} });
  assert.match($('answer').innerHTML, /Waiting for the owner's approval #9/);
  assert.deepEqual(JSON.parse(env.fetches.at(-1).init.body), {
    company: 3,
    name: 'x',
    value: 2.5,
    live: true,
    extra: { a: 1 },
  });
  answer = respond({ error: 'refused' }, { status: 403 });
  await $('call').onsubmit({ preventDefault() {} });
  assert.match($('answer').innerHTML, /HTTP 403[^]*refused/);
  answer = respond('plain text');
  await $('call').onsubmit({ preventDefault() {} });
  assert.match($('answer').innerHTML, /<pre [^>]*>plain text<\/pre>/);
  $('f-extra').value = '{';
  $('call').oninput();
  await $('call').onsubmit({ preventDefault() {} });
  assert.match($('answer').innerHTML, /A JSON field does not parse/);
});

test('a tool without parameters, a GUI client tool and an unknown name', async () => {
  await open('http://127.0.0.1:8800/tool/openttd.ai.list');
  assert.match($('page').innerHTML, /takes no parameters/);
  assert.match($('badges').innerHTML, /read-only/);
  await open('http://127.0.0.1:8800/client/openttd.view.ask');
  assert.match($('badges').innerHTML, /GUI client tool/);
  await open('http://127.0.0.1:8800/tool/openttd.nope');
  assert.match($('page').innerHTML, /Not in the live catalogue/);
});

test('a resource page reads now and, live, every two seconds, kept in the address', async () => {
  await open('http://127.0.0.1:8800/resource/map/terrain?live=1');
  assert.match($('badges').innerHTML, /resource[^]*read-only/);
  assert.equal($('data').textContent, 'not json');
  assert.match($('read-at').textContent, /HTTP 200$/);
  assert.ok(
    env.timers.some((t) => t.repeat && t.ms === 2000),
    'live reads every 2 s',
  );
  $('live').checked = false;
  $('live').onchange();
  assert.equal(env.location.search, '');
  await $('refresh').onclick();
  assert.match($('page').innerHTML, /mcp read openttd:\/\/map\/terrain/);
});

test('one derivation: /tools and every item page take their items from catalogueItems', async () => {
  const all = catalogueItems(doc);
  env.timers.splice(0); // the resource page's live reads are not part of this view
  await open('http://127.0.0.1:8800/tools');
  await env.tick(); // the catalogue draws on its first timer
  const cards = $('page').querySelector('[data-cards]').querySelectorAll('a');
  assert.deepEqual(
    cards.map((a) => a.getAttribute('href')),
    all.map((i) => i.page),
    'the /tools cards are the derived items, in order',
  );
  assert.equal($('page').querySelector('[data-count]').textContent, `${all.length} of ${all.length}`);
  for (const it of all) {
    await open(`http://127.0.0.1:8800${it.page}`);
    assert.doesNotMatch($('page').innerHTML, /Not in the live catalogue/, it.page);
    assert.ok($('crumbs').innerHTML.endsWith(`${KIND_LABEL[it.kind]}</li>`), it.page);
    if (it.kind !== 'resource') {
      const keys = $('page').querySelectorAll('[data-key]').map((e) => e.dataset.key);
      assert.deepEqual(keys, Object.keys(it.schema?.properties || {}), 'its form is its schema');
      assert.match($('surfaces').innerHTML, new RegExp(`operationId ${it.opId} `));
    } else {
      assert.match($('page').innerHTML, new RegExp(`operationId ${it.opId} `));
    }
  }
});

test('tool.js has no derivation of its own: it imports the catalogue\'s', async () => {
  const fs = await import('node:fs');
  const src = fs.readFileSync(new URL('../static/tool.js', import.meta.url), 'utf8');
  assert.match(src, /^import \{ catalogueItems[^}]*\} from '\.\/catalogue\.js';$/m);
  assert.doesNotMatch(src, /doc\.paths|op\.tags|requestBody/, 'no second reading of OpenAPI');
  const html = fs.readFileSync(new URL('../static/tool.html', import.meta.url), 'utf8');
  assert.match(html, /<script type="module" src="\/static\/tool\.js"><\/script>/);
});
