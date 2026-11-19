'use strict';
/* Play (research/play-in-browser.md): the real OpenTTD client, compiled to WebAssembly by upstream's
   Emscripten port, runs in this page and draws every frame itself. It joins the panel's game server
   as a network client: Emscripten turns its TCP socket into a WebSocket, which the panel bridges to
   127.0.0.1:<game port> (GET /ws/game, src/panel_play.rs). Only the game's lockstep packets cross the
   wire, never pictures. /api/play says whether the client is built and which port the game has.

   The pure parts (bridgeUrl, playArgs, statusText) are exported for Node (test/play.test.mjs). */
(function play() {
  /** Where a socket of the WASM client goes: the game server's TCP port to this panel's bridge,
      anything else to upstream's default (null), e.g. the content service's own proxy. */
  function bridgeUrl(host, port, proto, gamePort, pageHost, secure, upstream) {
    if (proto === 'tcp' && Number(port) === Number(gamePort) && (host === '127.0.0.1' || host === 'localhost')) {
      return `${secure ? 'wss' : 'ws'}://${pageHost}/ws/game`;
    }
    return upstream ? upstream(host, port, proto) : null;
  }
  /** The client's command line: join the game server as a spectator (company 255). pre.js adds
      the drivers (-mnull -snull -vsdl) itself. */
  function playArgs(gamePort) { return ['-n', `127.0.0.1:${gamePort}#255`, '-c', PLAY_CFG]; }
  /** The client's own config (outside the persistent IDBFS profile): a fresh web profile has no
      player name, and NetworkClientConnectGame refuses to join without one (src/network/network.cpp,
      NetworkValidateOurClientName), silently. */
  const PLAY_CFG = '/play/openttd.cfg';
  function playConfig(name) { return `[network]\nclient_name = "${String(name).replace(/[^A-Za-z0-9 _-]/g, '')}"\n`; }
  /** "Downloading data... (12/34)" from Emscripten, as a shorter line. */
  function statusText(text) {
    const m = /^([^(]+)\((\d+(?:\.\d+)?)\/(\d+)\)$/.exec(String(text || ''));
    return m ? `${m[1].trim()} ${Math.round((100 * Number(m[2])) / Number(m[3]))} %` : String(text || '');
  }

  if (typeof module !== 'undefined') { module.exports = { bridgeUrl, playArgs, playConfig, statusText, PLAY_CFG }; }
  if (typeof document === 'undefined') return;

  const $ = (id) => document.getElementById(id);
  const DOT = { loading: 'bg-amber-400 animate-pulse', live: 'bg-green-500', lost: 'bg-red-500', idle: 'bg-gray-500' };
  const setState = (text, tone) => {
    $('play-state').textContent = text;
    $('play-dot').className = `inline-block w-2 h-2 rounded-full ${DOT[tone] || DOT.idle}`;
  };
  const overlay = ({ title, message, command, spinner = false, progress = null }) => {
    $('play-overlay').classList.remove('hidden');
    $('play-title').textContent = title;
    $('play-message').textContent = message || '';
    $('play-command').classList.toggle('hidden', !command);
    $('play-command').textContent = command || '';
    $('play-spinner').classList.toggle('hidden', !spinner);
    $('play-progress').classList.toggle('hidden', progress === null);
    if (progress !== null) $('play-progress').value = progress;
  };
  const hideOverlay = () => $('play-overlay').classList.add('hidden');

  /* Fullscreen: the stage (canvas and HUD), so the game fills the screen; SDL follows the size. */
  const stage = $('stage');
  const fullscreen = () => (document.fullscreenElement ? document.exitFullscreen() : stage.requestFullscreen?.())?.catch?.(() => {});
  $('play-fullscreen').onclick = fullscreen;
  $('play-focus').onclick = () => $('canvas').focus();
  document.addEventListener('fullscreenchange', () => {
    const on = document.fullscreenElement === stage;
    stage.classList.toggle('rounded-xl', !on);
    stage.classList.toggle('h-[calc(100vh-14rem)]', !on);
    stage.classList.toggle('h-screen', on);
    window.dispatchEvent(new Event('resize'));
    $('canvas').focus();
  });
  document.addEventListener('keydown', (e) => {
    const typing = /^(INPUT|TEXTAREA|SELECT)$/.test(e.target?.tagName || '');
    if ((e.key === 'f' || e.key === 'F') && !typing && document.activeElement !== $('canvas')) { e.preventDefault(); fullscreen(); }
  });
  /* A hidden tab throttles the client's loop; lockstep then leaves it behind and the server drops it. */
  document.addEventListener('visibilitychange', () => {
    if (document.hidden && started) setState('tab hidden: the game may drop this client', 'loading');
    else if (started) setState('playing', 'live');
  });

  /* Frames drawn per second, measured on the canvas's own animation frames. */
  let frames = 0;
  const tick = () => { frames++; requestAnimationFrame(tick); };
  requestAnimationFrame(tick);
  setInterval(() => { $('play-fps').textContent = `${frames} fps`; frames = 0; }, 1000);

  let started = false;
  async function start() {
    let info;
    try { info = await (await fetch('/api/play')).json(); } catch (e) {
      setState('panel unreachable', 'lost');
      return overlay({ title: 'The panel does not answer', message: String(e) });
    }
    $('play-build').textContent = `client: ${info.dir} · game port ${info.game_port}`;
    if (!info.built) {
      setState('not built', 'idle');
      return overlay({
        title: 'The WebAssembly client is not built yet',
        message: 'Build it once with the upstream Emscripten image, then reload this page.',
        command: 'research/play-in-browser.md, "Build the client" (Docker, upstream emsdk image)',
      });
    }
    setState('loading the client', 'loading');
    overlay({ title: 'Loading the game', message: 'Fetching the WebAssembly client…', spinner: true, progress: 0 });
    const canvas = $('canvas');
    canvas.addEventListener('webglcontextlost', (e) => { e.preventDefault(); setState('graphics context lost: reload', 'lost'); });
    globalThis.Module = {
      // ?debug=net=3 hands the client a -d level (OpenTTD's debug facilities, src/debug.h); its log
      // goes to the browser console.
      arguments: playArgs(info.game_port).concat(new URLSearchParams(location.search).get('debug') ? ['-d', new URLSearchParams(location.search).get('debug')] : []),
      canvas,
      // The data package and the .wasm resolve against the page (/play), not the script: both
      // live under /play/ (GET /play/{file}).
      locateFile: (file) => `/play/${file}`,
      print: (t) => console.log(t),
      printErr: (t) => console.error(t),
      setStatus: (t) => {
        if (!t) return;
        const m = /\((\d+(?:\.\d+)?)\/(\d+)\)$/.exec(t);
        overlay({ title: 'Loading the game', message: statusText(t), spinner: true, progress: m ? Number(m[1]) / Number(m[2]) : 0 });
      },
      preRun: [() => {
        const M = globalThis.Module;
        M.FS_createPath('/', 'play', true, true);
        try { M.FS_createDataFile('/play', 'openttd.cfg', playConfig(`browser-${Math.random().toString(36).slice(2, 6)}`), true, true); } catch { /* there */ }
        // pre.js set Module.websocket.url when the script loaded; SOCKFS reads it when a socket
        // connects, so wrapping it here sends the game's socket to the panel's bridge.
        const upstream = globalThis.Module.websocket?.url;
        globalThis.Module.websocket = globalThis.Module.websocket || {};
        globalThis.Module.websocket.url = (host, port, proto) =>
          bridgeUrl(host, port, proto, info.game_port, location.host, location.protocol === 'https:', upstream);
      }],
      onRuntimeInitialized: () => { started = true; hideOverlay(); setState('playing', 'live'); canvas.focus(); },
      onExit: () => { setState('the game ended', 'idle'); overlay({ title: 'The game ended', message: 'Reload to join again.' }); },
      onAbort: () => { setState('crashed', 'lost'); overlay({ title: 'The client crashed', message: 'Reload to join again; the browser console has the details.' }); },
      onWarningFs: () => {},
      onBootstrap: () => overlay({ title: 'Missing base graphics', message: 'The client is fetching base graphics from the internet: rebuild it with OpenGFX in build-wasm/baseset.' }),
      onBootstrapFailed: () => overlay({ title: 'No base graphics', message: 'Rebuild the client with OpenGFX in build-wasm/baseset.' }),
      onBootstrapReload: () => location.reload(),
    };
    const s = document.createElement('script');
    s.src = '/play/openttd.js';
    s.onerror = () => { setState('client missing', 'lost'); overlay({ title: 'The client did not load', message: '/play/openttd.js failed to load.' }); };
    document.body.appendChild(s);
  }
  start();
})();
