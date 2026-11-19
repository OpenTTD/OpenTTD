// The Play page's pure parts (static/play.js): where the WebAssembly client's sockets go and how it joins.
import test from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const { bridgeUrl, playArgs, playConfig, statusText, PLAY_CFG } = createRequire(import.meta.url)('../static/play.js');

test('the game server socket goes to the panel bridge, anything else to upstream', () => {
  assert.equal(bridgeUrl('127.0.0.1', 3979, 'tcp', 3979, '127.0.0.1:8800', false), 'ws://127.0.0.1:8800/ws/game');
  assert.equal(bridgeUrl('localhost', '3979', 'tcp', 3979, '127.0.0.1:8800', true), 'wss://127.0.0.1:8800/ws/game');
  const upstream = (h, p, proto) => `upstream:${h}:${p}:${proto}`;
  assert.equal(bridgeUrl('content.openttd.org', 3978, 'tcp', 3979, 'x', false, upstream), 'upstream:content.openttd.org:3978:tcp');
  assert.equal(bridgeUrl('127.0.0.1', 3979, 'udp', 3979, 'x', false), null, 'only TCP goes to the bridge');
  assert.equal(bridgeUrl('10.0.0.5', 3979, 'tcp', 3979, 'x', false), null, 'another host never reaches the bridge');
});

test('the client joins the game server as a spectator', () => {
  assert.deepEqual(playArgs(3979), ['-n', '127.0.0.1:3979#255', '-c', PLAY_CFG]);
});

test('the client gets a player name, or the game refuses to join without saying so', () => {
  assert.equal(playConfig('browser-ab12'), '[network]\nclient_name = "browser-ab12"\n');
  assert.equal(playConfig('a"]\n[x'), '[network]\nclient_name = "ax"\n', 'nothing that breaks the ini');
});

test('Emscripten status lines become short percentages', () => {
  assert.equal(statusText('Downloading data... (50/200)'), 'Downloading data... 25 %');
  assert.equal(statusText('Running...'), 'Running...');
  assert.equal(statusText(''), '');
});
