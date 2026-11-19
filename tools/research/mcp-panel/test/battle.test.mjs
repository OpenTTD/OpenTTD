// The Battle page's pure parts (static/battle.js): what it renders from battle.py's /api/state.
import test from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const { leaderboardHtml, tuningHtml, hostsHtml, liveHtml, eventsHtml, kpis, money } = createRequire(import.meta.url)('../static/battle.js');
const lb = [
  { key: 'AAAHogEx', points: 30, avg_points: 1.0, avg_rank: 1.0, played: 6, avg_value: 9e7, anomalies: 0 },
  { key: 'PrismaticAI GA g1.1', points: 20, avg_points: 0.7, avg_rank: 2.0, played: 6, avg_value: 4e6, anomalies: 1 },
  { key: 'CivilAI', points: 2, avg_points: 0.1, avg_rank: 4.8, played: 6, avg_value: 1e5, anomalies: 0 },
];

test('the leaderboard ranks by points, marks PrismaticAI and the last place, and escapes names', () => {
  const html = leaderboardHtml([lb[2], lb[0], lb[1]]);
  assert.ok(html.indexOf('AAAHogEx') < html.indexOf('PrismaticAI GA g1.1') && html.indexOf('PrismaticAI GA g1.1') < html.indexOf('CivilAI'));
  assert.match(html, /text-primary-600[^>]*>PrismaticAI GA g1\.1/);
  assert.match(html, /CivilAI <span[^>]*>last<\/span>/);
  assert.match(leaderboardHtml([{ key: '<b>x</b>', points: 1 }]), /&lt;b&gt;x&lt;\/b&gt;/);
  const idle = leaderboardHtml([...lb, { key: 'PrismaticAI new', points: 0, avg_points: 0, played: 0 }]);
  assert.doesNotMatch(idle, /PrismaticAI new <span[^>]*>last/, 'a contestant that has not played is never last');
  assert.match(idle, /CivilAI <span[^>]*>last/);
});

test('never-last holds while a non-PrismaticAI contestant has the worst average rank', () => {
  const k = kpis({ leaderboard: lb, hosts: [{ state: 'online', running: 2, slots: 4 }, { state: 'offline', running: 0, slots: 2 }], tuning: { generation: 2 } });
  assert.equal(k.neverLast, true);
  assert.equal(k.lastKey, 'CivilAI');
  assert.equal(k.arenas, '1 / 2');
  assert.equal(k.busy, '2 / 6');
  assert.equal(k.best, 'PrismaticAI GA g1.1 · 2');
  assert.equal(kpis({ leaderboard: [lb[0], { ...lb[1], avg_rank: 5 }] }).neverLast, false);
});

test('the GA table orders by score and marks the brain offspring', () => {
  const html = tuningHtml({ variants: [
    { label: 'PrismaticAI GA g1.0', settings: 'a=1', fitness: { score: 0.5, matches: 4 } },
    { label: 'PrismaticAI GA g1.brain0', settings: 'a=2', fitness: { score: 0.7, matches: 4 } },
  ] });
  assert.ok(html.indexOf('g1.brain0') < html.indexOf('g1.0'));
  assert.match(html, /g1\.brain0 <span[^>]*>brain<\/span>/);
  const unmeasured = tuningHtml({ variants: [{ label: 'g2.0', settings: '', fitness: { score: -9, matches: 0 } }] });
  assert.doesNotMatch(unmeasured, />-9</, 'a sentinel score is not shown as a fitness');
});

test('arenas, live matches and events render from the API shapes', () => {
  assert.match(hostsHtml([{ name: 'mbp', state: 'online', running: 3, slots: 9, done: 18, version: 'v' }]), /3 \/ 9/);
  const live = liveHtml([{ id: 'm-1', host: 'mbp', standings: [{ key: 'AAAHogEx' }, { key: 'PrismaticAI' }], series: { AAAHogEx: [{ t: '1965-01' }] } }]);
  assert.match(live, /1965-01/);
  assert.match(live, /ours #2\/2/);
  assert.match(eventsHtml([{ at: 1, msg: 'ALERT fleet_drop' }]), /text-amber-500/);
  assert.equal(money(2.5e6), '2.5M');
  assert.equal(money('x'), '–');
});
