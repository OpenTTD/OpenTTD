'use strict';
/* The interface view: every window and widget of the owner's game, generic, derived from
   openttd.ui.windows (through the derived route /api/client/openttd.ui.windows); class names from
   src/window_type.h (/api/ui/classes). A click on a widget presses it in the game through
   openttd.input.* when the owner armed input; otherwise it says what is there (openttd.ui.at). */

const $ = (id) => document.getElementById(id);
const esc = (v) => String(v ?? '').replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
const clean = (t) => String(t ?? '').replace(/[-]/g, '').trim(); // the game's private-use control codes
/** The shell's Pro class sets, one per role (static/shell.js publishes them as PanelUI first). */
const UI = globalThis.PanelUI;

/* ---- address state and markup (pure) --------------------------------------------------------- */
/** The view's state from the address: widgets shown (widgets=0 hides them), the window picked. */
const readState = (search) => {
  const p = new URLSearchParams(search);
  return { widgets: p.get('widgets') !== '0', window: p.get('window') || 'all' };
};
const stateQuery = (s) => `?${new URLSearchParams({ widgets: s.widgets ? '1' : '0', window: s.window })}`;
/** A window class's name from src/window_type.h (/api/ui/classes), or its number. */
const classNameOf = (classes, c) => classes[String(c)]?.name || `class ${c}`;
const widgetLabel = (g) => clean(g.text) || clean(g.tooltip) || g.type;
const windowTitle = (classes, w) => `${esc(classNameOf(classes, w.class))}${w.number ? ' #' + w.number : ''}`;
const windowOptions = (ui, classes) => '<option value="all">All windows</option>'
  + ui.windows.map((w, i) => `<option value="${i}">${windowTitle(classes, w)}</option>`).join('');
/** The frames of the windows shown and, unless hidden, a button over every widget, scaled by k. */
function overlayHtml(ui, classes, state, k) {
  const shown = ui.windows.map((w, i) => [w, i]).filter(([, i]) => state.window === 'all' || String(i) === state.window);
  let html = '';
  for (const [w, i] of shown) {
    if (w.class === 0) continue; // the main viewport covers the screen; clicks on it go to openttd.ui.at
    const r = w.rect;
    html += `<div class="absolute border-2 border-blue-500/70 pointer-events-none" style="left:${r.x * k}px;top:${r.y * k}px;width:${r.w * k}px;height:${r.h * k}px"><span class="absolute -top-5 left-0 text-[10px] px-1 rounded-sm bg-blue-600 text-white">${esc(classNameOf(classes, w.class))}</span></div>`;
    if (!state.widgets) continue;
    for (const g of w.widgets || []) {
      const b = g.rect; if (!b || b.w <= 0 || b.h <= 0 || g.type === 'panel' || g.type === 'other') continue;
      html += `<button type="button" data-w="${i}" data-g="${g.index}" title="${esc(widgetLabel(g))}${g.disabled ? ' (disabled)' : ''}" class="absolute rounded-sm ${g.disabled ? 'bg-gray-500/20 cursor-not-allowed' : 'bg-yellow-300/20 hover:bg-yellow-300/50 cursor-pointer'} border border-yellow-400/60" style="left:${b.x * k}px;top:${b.y * k}px;width:${b.w * k}px;height:${b.h * k}px"></button>`;
    }
  }
  return html;
}
const windowsHtml = (ui, classes) => ui.windows.map((w, i) => {
  const texts = (w.widgets || []).map((g) => clean(g.text)).filter(Boolean).slice(0, 6);
  return `<li class="p-2 border border-gray-200 rounded-lg dark:border-gray-700 cursor-pointer hover:bg-gray-50 dark:hover:bg-gray-700" data-pick="${i}"><div class="font-medium text-gray-900 dark:text-white">${windowTitle(classes, w)}</div><div class="text-xs text-gray-500 dark:text-gray-400">${(w.widgets || []).length} widgets${w.caption ? ' · ' + esc(clean(w.caption)) : ''}</div>${texts.length ? `<div class="mt-1 text-xs text-gray-700 dark:text-gray-300">${texts.map(esc).join(' · ')}</div>` : ''}</li>`;
}).join('');
const statsText = (ui) => `${ui.windows.length} windows · ${ui.windows.reduce((n, w) => n + (w.widgets || []).length, 0)} widgets · screen ${ui.screen.w}x${ui.screen.h}`;
/** What openttd.ui.at found under a point. */
const atText = (classes, x, y, at) => `(${x},${y}): ${classNameOf(classes, at.window?.class ?? at.class)}${at.widget ? ' / ' + widgetLabel(at.widget) : ''}${at.tile !== undefined ? ` · tile ${at.tile}` : ''}`;
/** What the page says when openttd.ui.windows has no answer: a headless game (a duel, a --no-gui
    stack) has no GUI client to read. The panel's own error, when it gave one, is kept. */
const noClientText = (answer) => `No GUI client is attached to this game, so there are no windows to show: `
  + `start the stack without --no-gui (tools/research/mcp-e2e) to join one.${answer && answer.error ? ` (${String(answer.error).slice(0, 160)})` : ''}`;
const ARMED = { true: globalThis.PanelTONE.green, false: globalThis.PanelTONE.yellow };
const armedBadge = (armed) => `${UI.badge} ${ARMED[!!armed]}`;

/* ---- the page ------------------------------------------------------------------------------ */
const state = readState(location.search);
const save = () => history.replaceState(null, '', `${location.pathname}${stateQuery(state)}`);
const client = async (name, args) => (await fetch(`/api/client/${name}`, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(args || {}) })).json();

let classes = {}; let ui = null; let armed = false;

function className(c) { return classNameOf(classes, c); }

function draw() {
  if (!ui) return;
  const img = $('screen-img');
  const sel = $('p-window'); const keep = state.window;
  sel.innerHTML = windowOptions(ui, classes);
  sel.value = [...sel.options].some((o) => o.value === keep) ? keep : 'all';
  // The list and the numbers need no picture: a client without a window (the null video driver of
  // a headless stack) has windows but no screenshot. The overlay waits for the picture's size.
  $('windows').innerHTML = windowsHtml(ui, classes);
  $('u-stats').textContent = statsText(ui);
  $('overlay').innerHTML = img.clientWidth ? overlayHtml(ui, classes, state, img.clientWidth / ui.screen.w) : '';
}

async function refresh() {
  try {
    const next = new Image();
    next.onload = () => { $('screen-img').src = next.src; draw(); };
    next.src = `/api/view/screenshot?t=${Date.now()}`;
    const answer = await client('openttd.ui.windows', { widgets: true });
    if (!answer || !Array.isArray(answer.windows)) {
      // Empty cards say nothing: name why there is nothing, and how to get it.
      ui = null;
      $('u-msg').textContent = noClientText(answer);
      $('windows').innerHTML = `<p class="text-sm text-gray-500 dark:text-gray-400">${esc(noClientText(answer))}</p>`;
      return;
    }
    ui = answer;
    draw();
    const st = await client('openttd.input.state');
    armed = !!st.armed;
    const a = $('u-armed');
    a.textContent = armed ? 'input armed: clicks press widgets in the game' : 'input not armed: read only (the owner arms it: mcp-e2e --arm-input)';
    a.className = armedBadge(armed);
  } catch (e) { $('u-msg').textContent = `no GUI client attached yet (${e.message})`; }
}

async function press(w, g) {
  const widget = ui.windows[w].widgets.find((x) => x.index === g);
  const label = widgetLabel(widget);
  if (!armed) { $('u-msg').textContent = `${className(ui.windows[w].class)} / ${label}: input is not armed, so the panel only reads. The owner arms it with mcp-e2e --arm-input.`; return; }
  const { cx, cy } = widget.rect;
  await client('openttd.input.pointer', { x: cx, y: cy });
  await client('openttd.input.button', { state: 'down', button: 'left' });
  await client('openttd.input.button', { state: 'up', button: 'left' });
  $('u-msg').textContent = `pressed ${className(ui.windows[w].class)} / ${label} at (${cx},${cy})`;
  setTimeout(refresh, 400);
}

$('overlay').addEventListener('click', async (e) => {
  const b = e.target.closest('button[data-w]');
  if (b) { press(+b.dataset.w, +b.dataset.g); return; }
  if (!ui) return;
  const r = $('screen').getBoundingClientRect(); const k = ui.screen.w / r.width;
  const x = Math.round((e.clientX - r.left) * k); const y = Math.round((e.clientY - r.top) * k);
  const at = await client('openttd.ui.at', { x, y });
  $('u-msg').textContent = atText(classes, x, y, at);
});
$('windows').addEventListener('click', (e) => { const li = e.target.closest('[data-pick]'); if (li) { state.window = li.dataset.pick; save(); draw(); } });
$('p-window').onchange = (e) => { state.window = e.target.value; save(); draw(); };
$('p-widgets').checked = state.widgets;
$('p-widgets').onchange = (e) => { state.widgets = e.target.checked; save(); draw(); };
document.querySelectorAll('[data-maximize]').forEach((b) => b.addEventListener('click', () => {
  const f = b.closest('[data-frame]'); const on = !f.classList.contains('fixed');
  ['fixed', 'inset-0', 'z-[60]', 'overflow-auto'].forEach((c) => f.classList.toggle(c, on));
  b.textContent = on ? 'Restore (Esc)' : 'Maximize'; setTimeout(draw, 30);
}));
document.addEventListener('keydown', (e) => { if (e.key === 'Escape') document.querySelectorAll('[data-frame].fixed [data-maximize]').forEach((b) => b.click()); });
window.addEventListener('resize', () => setTimeout(draw, 30));
save();
fetch('/api/ui/classes').then((r) => r.json()).then((c) => { classes = c; draw(); });
setTimeout(refresh, 30);
setInterval(refresh, 1500);

if (typeof module !== 'undefined') {
  module.exports = {
    readState, stateQuery, classNameOf, widgetLabel, clean, windowOptions, overlayHtml, windowsHtml,
    statsText, atText, armedBadge, noClientText, draw, refresh, press,
  };
}
