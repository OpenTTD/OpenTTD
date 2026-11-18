// node --test tools/research/mcp-panel/test/ : the interface view (static/ui.js), every window and
// widget drawn from an openttd.ui.windows fixture, in the fake browser of fake-dom.mjs. The
// fixture is fictional; nothing here talks to a game.
import { UI } from './panel-ui.mjs';
import test from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import { fakeDom } from './fake-dom.mjs';

const PUA = String.fromCharCode(0xe000);
const classes = { 0: { name: 'WC_MAIN_WINDOW' }, 12: { name: 'WC_BUILD_TOOLBAR' } };
const windows = {
  screen: { w: 1600, h: 900 },
  windows: [
    { class: 0, rect: { x: 0, y: 0, w: 1600, h: 900 }, widgets: [] },
    {
      class: 12,
      number: 2,
      caption: `${PUA}Build`,
      rect: { x: 100, y: 50, w: 200, h: 40 },
      widgets: [
        {
          index: 1,
          type: 'button',
          text: `${PUA}Rail`,
          rect: { x: 110, y: 60, w: 20, h: 20, cx: 120, cy: 70 },
        },
        {
          index: 2,
          type: 'button',
          tooltip: 'Road <tip>',
          disabled: true,
          rect: { x: 140, y: 60, w: 20, h: 20, cx: 150, cy: 70 },
        },
        { index: 3, type: 'panel', rect: { x: 100, y: 50, w: 200, h: 40 } },
        { index: 4, type: 'image', rect: { x: 0, y: 0, w: 0, h: 0 } },
      ],
    },
    { class: 99, rect: { x: 5, y: 5, w: 10, h: 10 } },
  ],
};
let armed = false;
const calls = [];
const env = fakeDom({
  url: 'http://127.0.0.1:8800/ui?window=1',
  routes: (url, init) => {
    if (url === '/api/ui/classes') return classes;
    const name = url.replace('/api/client/', '');
    calls.push([name, JSON.parse(init.body || '{}')]);
    if (name === 'openttd.ui.windows') return windows;
    if (name === 'openttd.input.state') return { armed };
    if (name === 'openttd.ui.at')
      return { window: { class: 12 }, widget: { text: 'Rail' }, tile: 77 };
    return {};
  },
});
env.document.body.innerHTML =
  '<div data-frame id="frame"><button data-maximize id="max">Maximize</button></div>';
const ui = createRequire(import.meta.url)('../static/ui.js');
const $ = env.byId;

test('the address keeps the view: widgets shown and the window picked', () => {
  assert.deepEqual(ui.readState(''), { widgets: true, window: 'all' });
  assert.deepEqual(ui.readState('?widgets=0&window=3'), { widgets: false, window: '3' });
  assert.equal(ui.stateQuery({ widgets: false, window: '3' }), '?widgets=0&window=3');
  assert.equal(
    env.location.search,
    '?widgets=1&window=1',
    'the page writes its state back at once',
  );
});

test("names, labels and texts come from the fixture, without the game's control codes", () => {
  assert.equal(ui.clean(`${PUA} Rail `), 'Rail');
  assert.equal(ui.classNameOf(classes, 12), 'WC_BUILD_TOOLBAR');
  assert.equal(ui.classNameOf(classes, 7), 'class 7');
  const [, w] = windows.windows;
  assert.deepEqual(w.widgets.map(ui.widgetLabel), ['Rail', 'Road <tip>', 'panel', 'image']);
  assert.match(
    ui.windowOptions(windows, classes),
    /value="all">All windows.*value="1">WC_BUILD_TOOLBAR #2.*value="2">class 99/,
  );
  assert.equal(ui.statsText(windows), '3 windows · 4 widgets · screen 1600x900');
  assert.equal(ui.atText(classes, 1, 2, { class: 12 }), '(1,2): WC_BUILD_TOOLBAR');
  assert.equal(
    ui.atText(classes, 1, 2, { window: { class: 0 }, widget: { type: 'x' }, tile: 5 }),
    '(1,2): WC_MAIN_WINDOW / x · tile 5',
  );
  assert.ok(
    ui.armedBadge(true).startsWith(UI.badge) &&
      /green/.test(ui.armedBadge(true)) &&
      /yellow/.test(ui.armedBadge(false)),
  );
});

test('the overlay frames each window and puts a button over every pressable widget, scaled', () => {
  const all = ui.overlayHtml(windows, classes, { widgets: true, window: 'all' }, 0.5);
  assert.doesNotMatch(all, /WC_MAIN_WINDOW/, 'the main viewport is no frame');
  assert.match(all, /left:50px;top:25px;width:100px;height:20px[^]*WC_BUILD_TOOLBAR/);
  assert.equal(all.split('<button').length - 1, 2, 'a panel and an empty widget get no button');
  assert.match(all, /title="Road &lt;tip&gt; \(disabled\)" class="[^"]*cursor-not-allowed/);
  assert.equal(
    ui.overlayHtml(windows, classes, { widgets: false, window: 'all' }, 1).split('<button').length,
    1,
  );
  assert.doesNotMatch(
    ui.overlayHtml(windows, classes, { widgets: true, window: '2' }, 1),
    /TOOLBAR/,
  );
  const list = ui.windowsHtml(windows, classes);
  assert.match(list, /data-pick="1"[^]*WC_BUILD_TOOLBAR #2[^]*4 widgets · Build[^]*Rail/);
});

test('the windows are drawn once the screenshot is in; input says if it is armed', async () => {
  await env.tick();
  assert.match($('u-armed').textContent, /not armed/);
  env.images.at(-1).onload();
  assert.equal($('screen-img').src, env.images.at(-1).src);
  assert.equal($('p-window').value, '1');
  assert.match($('overlay').innerHTML, /data-w="1" data-g="1"/);
  assert.equal($('u-stats').textContent, '3 windows · 4 widgets · screen 1600x900');
});

test('a press only reads while input is not armed; armed, it presses the widget', async () => {
  const button = () => $('overlay').querySelector('button[data-w]');
  await $('overlay').listeners.click[0]({ target: button() });
  assert.match($('u-msg').textContent, /WC_BUILD_TOOLBAR \/ Rail: input is not armed/);
  armed = true;
  await env.tick();
  await $('overlay').listeners.click[0]({ target: button() });
  await env.settle();
  assert.deepEqual(
    calls
      .filter(([n]) => n.startsWith('openttd.input.'))
      .slice(-3)
      .map(([n, a]) => [n, a]),
    [
      ['openttd.input.pointer', { x: 120, y: 70 }],
      ['openttd.input.button', { state: 'down', button: 'left' }],
      ['openttd.input.button', { state: 'up', button: 'left' }],
    ],
  );
  assert.match($('u-msg').textContent, /pressed WC_BUILD_TOOLBAR \/ Rail at \(120,70\)/);
  await $('overlay').listeners.click[0]({ target: $('overlay'), clientX: 400, clientY: 200 });
  await env.settle();
  assert.deepEqual(
    calls.at(-1),
    ['openttd.ui.at', { x: 800, y: 400 }],
    'a click elsewhere asks what is there',
  );
  assert.equal($('u-msg').textContent, '(800,400): WC_BUILD_TOOLBAR / Rail · tile 77');
});

test('picking a window, hiding widgets and maximizing are kept in the address', async () => {
  const li = $('windows').querySelector('[data-pick="2"]');
  $('windows').listeners.click[0]({ target: li });
  assert.equal(env.location.search, '?widgets=1&window=2');
  $('p-window').onchange({ target: { value: 'all' } });
  $('p-widgets').onchange({ target: { checked: false } });
  assert.equal(env.location.search, '?widgets=0&window=all');
  assert.doesNotMatch($('overlay').innerHTML, /<button/);
  $('windows').listeners.click[0]({ target: $('windows') });
  await $('max').click();
  assert.ok($('frame').classList.contains('fixed'));
  assert.equal($('max').textContent, 'Restore (Esc)');
  await env.document.dispatch('keydown', { key: 'Escape' });
  assert.ok(!$('frame').classList.contains('fixed'), 'Escape restores');
  env.windowListeners.resize();
});

test('without a GUI client the page says so', async () => {
  env.routes = () => {
    throw new Error('503');
  };
  await env.tick();
  assert.match($('u-msg').textContent, /no GUI client attached yet \(503\)/);
});
