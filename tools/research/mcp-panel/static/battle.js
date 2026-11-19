'use strict';
/* The LAN AI battle inside the panel (research/ai-battle/README.md): one app for the live game and the
   tournament. Everything comes from battle.py's own API (default http://127.0.0.1:8765, ?api= to
   change): /api/stream (server-sent events, the whole state every few seconds) and /api/state as the
   fallback poll. Nothing here is typed by hand; the API is the source and stays read-only from here.

   The pure render functions are exported for Node (test/battle.test.mjs). */
(function battle() {
  const esc = (v) => String(v ?? '').replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
  const num = (v, d = 0) => (v === null || v === undefined || Number.isNaN(Number(v)) ? '–' : Number(v).toLocaleString('en-US', { maximumFractionDigits: d }));
  const money = (v) => {
    const n = Number(v); if (!Number.isFinite(n)) return '–';
    const a = Math.abs(n);
    return a >= 1e6 ? `${(n / 1e6).toFixed(1)}M` : a >= 1e3 ? `${(n / 1e3).toFixed(0)}k` : `${n.toFixed(0)}`;
  };
  const isPrismatic = (key) => /^PrismaticAI/.test(String(key));
  // The shell's class sets (static/shell.js): in the browser it publishes them first, in Node they are required.
  const shell = typeof module !== 'undefined' ? require('./shell.js') : { UI: globalThis.PanelUI, TONE: globalThis.PanelTONE };
  const badge = (text, tone) => ` <span class="ms-1 ${shell.UI.badge} ${shell.TONE[tone]}">${esc(text)}</span>`;
  const TH = 'px-3 py-2 text-[11px] font-medium text-gray-500 dark:text-gray-400 whitespace-nowrap';
  const TD = 'px-3 py-2 whitespace-nowrap';

  /** The leaderboard rows, best first, each with its points bar against the leader and a mark for
      PrismaticAI and for the last place. */
  function leaderboardHtml(rows) {
    // Who has played, ranked by points per match; who has not played yet after them, never "last".
    const played = (r) => Number(r.played ?? 1) > 0;
    const byPoints = (a, b) => (b.avg_points ?? b.points ?? 0) - (a.avg_points ?? a.points ?? 0);
    const ranked = (rows || []).filter(played).sort(byPoints);
    const list = ranked.concat((rows || []).filter((r) => !played(r)));
    const top = Math.max(1e-9, ...list.map((r) => Number(r.avg_points ?? r.points ?? 0)));
    return list.map((r, i) => {
      const p = Number(r.avg_points ?? r.points ?? 0);
      const pr = isPrismatic(r.key);
      const last = i === ranked.length - 1 && ranked.length > 1;
      const idle = !played(r);
      return `<tr class="border-t border-gray-100 dark:border-gray-700 hover:bg-gray-50 dark:hover:bg-gray-700/40${idle ? ' opacity-50' : ''}">
        <td class="${TD} tabular-nums text-gray-500">${idle ? '–' : i + 1}</td>
        <td class="${TD} font-medium ${pr ? 'text-primary-600 dark:text-primary-400' : 'text-gray-900 dark:text-white'}">${esc(r.key)}${last ? badge('last', 'red') : ''}</td>
        <td class="${TD} w-full min-w-[8rem]"><div class="h-2 rounded-full bg-gray-100 dark:bg-gray-700"><div class="h-2 rounded-full ${pr ? 'bg-primary-500' : 'bg-gray-400 dark:bg-gray-500'}" style="width:${Math.max(2, (100 * p) / top).toFixed(1)}%"></div></div></td>
        <td class="${TD} text-right tabular-nums">${num(p, 3)}</td>
        <td class="${TD} text-right tabular-nums">${num(r.avg_rank, 2)}</td>
        <td class="${TD} text-right tabular-nums">${num(r.played)}</td>
        <td class="${TD} text-right tabular-nums">${money(r.avg_value)}</td>
        <td class="${TD} text-right tabular-nums ${r.anomalies ? 'text-amber-500' : 'text-gray-400'}">${num(r.anomalies)}</td>
      </tr>`;
    }).join('');
  }

  /** The genetic algorithm's variants by fitness score, brain proposals marked (label ".brain"). */
  function tuningHtml(t) {
    // A variant without a match has no fitness yet (the GA's sentinel score is not a measurement).
    const measured = (v) => Number(v.fitness?.matches || 0) > 0;
    const vs = [...((t && t.variants) || [])].sort((a, b) => (measured(b) - measured(a)) || ((b.fitness?.score ?? -1) - (a.fitness?.score ?? -1)));
    return vs.map((v) => {
      const f = measured(v) ? (v.fitness || {}) : { matches: 0 };
      const brain = /\.brain/.test(v.label || '');
      return `<tr class="border-t border-gray-100 dark:border-gray-700">
        <td class="${TD} font-medium text-gray-900 dark:text-white">${esc(v.label)}${brain ? badge('brain', 'purple') : ''}</td>
        <td class="${TD} text-right tabular-nums">${num(f.score, 3)}</td>
        <td class="${TD} text-right tabular-nums">${num(f.log_gap, 3)}</td>
        <td class="${TD} text-right tabular-nums">${num(f.matches)}</td>
        <td class="px-3 py-2 font-mono text-[11px] text-gray-500 dark:text-gray-400 break-all">${esc(v.settings)}</td>
      </tr>`;
    }).join('');
  }

  function hostsHtml(hosts) {
    return (hosts || []).map((h) => {
      const on = h.state === 'online';
      return `<tr class="border-t border-gray-100 dark:border-gray-700">
        <td class="${TD}"><span class="inline-block w-2 h-2 rounded-full me-2 ${on ? 'bg-green-500' : 'bg-red-500'}" aria-hidden="true"></span><span class="font-medium text-gray-900 dark:text-white">${esc(h.name)}</span></td>
        <td class="${TD} text-right tabular-nums">${num(h.running)} / ${num(h.slots)}</td>
        <td class="${TD} text-right tabular-nums">${num(h.done)}</td>
        <td class="${TD} font-mono text-[11px] text-gray-500">${esc(h.version)}</td>
      </tr>`;
    }).join('');
  }

  /** One live match: id, arena, how far it is, and who leads now. */
  function liveHtml(live) {
    return (live || []).slice(0, 24).map((m) => {
      const s = m.standings && m.standings[0];
      const series = m.series && s ? m.series[s.key] : null;
      const t = series && series.length ? series[series.length - 1].t : '';
      const ours = (m.standings || []).findIndex((x) => isPrismatic(x.key));
      return `<li class="flex items-center justify-between gap-3 py-1.5 border-t border-gray-100 dark:border-gray-700 text-sm">
        <span class="font-mono text-xs text-gray-500">${esc(m.id)}</span>
        <span class="text-gray-500 dark:text-gray-400">${esc(m.host)}</span>
        <span class="tabular-nums text-gray-500">${esc(t)}</span>
        <span class="truncate text-gray-900 dark:text-white">${s ? esc(s.key) : '–'}</span>
        <span class="tabular-nums ${ours >= 0 ? 'text-primary-600 dark:text-primary-400' : 'text-gray-400'}">${ours >= 0 ? `ours #${ours + 1}/${m.standings.length}` : ''}</span>
      </li>`;
    }).join('');
  }

  /** The newest events, newest first, alerts and post-mortems toned. */
  function eventsHtml(events) {
    return [...(events || [])].slice(-30).reverse().map((e) => {
      const tone = /ALERT/.test(e.msg) ? 'text-amber-500' : /POSTMORTEM/.test(e.msg) ? 'text-sky-500' : 'text-gray-600 dark:text-gray-300';
      const at = e.at ? new Date(e.at * 1000).toLocaleTimeString() : '';
      return `<li class="py-1 border-t border-gray-100 dark:border-gray-700 text-xs"><span class="tabular-nums text-gray-400 me-2">${esc(at)}</span><span class="${tone}">${esc(e.msg)}</span></li>`;
    }).join('');
  }

  /** The KPIs: edition, arenas, matches, the best PrismaticAI against the leader, never-last. */
  function kpis(st) {
    const lb = st.leaderboard || [];
    const sorted = [...lb].sort((a, b) => (a.avg_rank ?? 99) - (b.avg_rank ?? 99));
    const ours = sorted.filter((r) => isPrismatic(r.key));
    const last = [...lb].sort((a, b) => (b.avg_rank ?? 0) - (a.avg_rank ?? 0))[0];
    const hosts = st.hosts || [];
    return {
      edition: st.edition || '–',
      rev: st.provenance?.prismatic_rev ? String(st.provenance.prismatic_rev).slice(0, 10) : (st.provenance?.rev || ''),
      arenas: `${hosts.filter((h) => h.state === 'online').length} / ${hosts.length}`,
      busy: `${hosts.reduce((a, h) => a + (h.running || 0), 0)} / ${hosts.reduce((a, h) => a + (h.slots || 0), 0)}`,
      leader: sorted[0]?.key || '–',
      best: ours[0] ? `${ours[0].key} · ${num(ours[0].avg_rank, 2)}` : '–',
      neverLast: !!last && !isPrismatic(last.key),
      lastKey: last?.key || '–',
      generation: st.tuning?.generation ?? '–',
    };
  }

  if (typeof module !== 'undefined') { module.exports = { leaderboardHtml, tuningHtml, hostsHtml, liveHtml, eventsHtml, kpis, money }; }
  if (typeof document === 'undefined') return;

  const $ = (id) => document.getElementById(id);
  const api = (new URLSearchParams(location.search).get('api') || 'http://127.0.0.1:8765').replace(/\/$/, '');
  $('b-api').textContent = api;
  let lastAt = 0;
  const render = (st) => {
    lastAt = Date.now();
    const k = kpis(st);
    $('k-edition').textContent = k.edition; $('k-rev').textContent = k.rev ? `PrismaticAI ${k.rev}` : '';
    $('k-arenas').textContent = k.arenas; $('k-busy').textContent = `${k.busy} slots busy`;
    $('k-leader').textContent = k.leader; $('k-best').textContent = k.best;
    $('k-gen').textContent = k.generation;
    $('k-never').textContent = k.neverLast ? 'holds' : 'FAILS';
    $('k-never').className = `text-2xl font-semibold ${k.neverLast ? 'text-green-500' : 'text-red-500'}`;
    $('k-last').textContent = `last on average: ${k.lastKey}`;
    $('lb-body').innerHTML = leaderboardHtml(st.leaderboard);
    $('ga-body').innerHTML = tuningHtml(st.tuning) || '<tr><td class="px-3 py-3 text-sm text-gray-500" colspan="5">No GA generation yet.</td></tr>';
    $('hosts-body').innerHTML = hostsHtml(st.hosts);
    $('live-list').innerHTML = liveHtml(st.live) || '<li class="py-2 text-sm text-gray-500">No match running.</li>';
    $('events-list').innerHTML = eventsHtml(st.events);
    $('b-live').className = 'inline-block w-2 h-2 rounded-full bg-green-500 animate-pulse motion-reduce:animate-none';
    $('b-state').textContent = 'live';
  };
  const stale = () => {
    if (Date.now() - lastAt > 10000) {
      $('b-live').className = 'inline-block w-2 h-2 rounded-full bg-red-500';
      $('b-state').textContent = lastAt ? 'stale: the battle API stopped answering' : `no answer from ${api}`;
    }
  };
  setInterval(stale, 2000);
  const poll = async () => { try { render(await (await fetch(`${api}/api/state`)).json()); } catch { stale(); } };
  try {
    const es = new EventSource(`${api}/api/stream`);
    es.addEventListener('state', (e) => { try { render(JSON.parse(e.data)); } catch { /* skip a bad frame */ } });
    es.onerror = () => stale();
  } catch { /* no SSE: the poll carries it */ }
  poll();
  setInterval(() => { if (Date.now() - lastAt > 8000) poll(); }, 5000);
})();
