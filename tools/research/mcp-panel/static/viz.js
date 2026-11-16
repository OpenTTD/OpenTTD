// RESEARCH-ONLY: the world view (I0048), built only on static/components.js and the hub's WebSocket.
// Every number and node comes from an instance's own MCP resources read through the hub; parameters
// live in the address (#instance=..&mode=..&kinds=..&exaggeration=..&max=..), so a view is a link.
import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { HubClient, jsonView, scrollOp, CLS, KINDS, ecInit, maximizable, worldGraphOption, waitingOption, moneyOption } from './components.js';

const $ = (id) => document.getElementById(id);
const echarts = globalThis.echarts;
const d3 = globalThis.d3;

// ---- the address is the state -------------------------------------------------------------
function getState() {
  const p = new URLSearchParams(location.hash.replace(/^#/, ''));
  return {
    instance: p.get('instance') || '', mode: p.get('mode') || 'map', max: p.get('max') || undefined,
    kinds: (p.get('kinds') || KINDS.join(',')).split(',').filter(Boolean),
    exaggeration: Number(p.get('exaggeration') || 6),
  };
}
function setState(patch) {
  const s = { ...getState(), ...patch };
  const p = new URLSearchParams();
  if (s.instance) p.set('instance', s.instance);
  if (s.mode !== 'map') p.set('mode', s.mode);
  if (s.kinds.join(',') !== KINDS.join(',')) p.set('kinds', s.kinds.join(','));
  if (s.exaggeration !== 6) p.set('exaggeration', String(s.exaggeration));
  if (s.max) p.set('max', s.max);
  history.replaceState(null, '', `#${p.toString()}`);
}

// ---- the hub ------------------------------------------------------------------------------
const client = new HubClient(`ws://${location.host}/ws`);
client.on && client.on('open', () => { $('conn').textContent = 'connected'; });
client.connect();
let ops = [];

/** A resource of an instance through the hub, as data (the envelope's data when it has one). */
async function read(instance, uri) {
  let r = await client.call('hub.call', { instance, id: `resource:${uri}` });
  if (r && r.contents && r.contents[0] && typeof r.contents[0].text === 'string') r = JSON.parse(r.contents[0].text);
  if (typeof r === 'string') r = JSON.parse(r);
  return r && r.data !== undefined ? r.data : r;
}

/** Instances that publish the world graph, in the hub's order. */
function candidates() {
  return [...new Set(ops.filter((o) => o.id === 'resource:openttd://world/graph').map((o) => o.instance))];
}

// ---- world graph --------------------------------------------------------------------------
let graph = null;
let graphChart = null;

function forcePositions(g, kinds) {
  const nodes = g.nodes.filter((n) => kinds.includes(n.kind)).map((n) => ({ id: n.id, x: n.x * 8, y: n.y * 8 }));
  const ids = new Set(nodes.map((n) => n.id));
  const links = g.edges.filter((e) => ids.has(e.from) && ids.has(e.to)).map((e) => ({ source: e.from, target: e.to }));
  // d3's start positions are deterministic (from the map), so the same world lays out the same way.
  const sim = d3.forceSimulation(nodes).force('link', d3.forceLink(links).id((n) => n.id).distance(30))
    .force('charge', d3.forceManyBody().strength(-40)).force('center', d3.forceCenter(0, 0)).stop();
  for (let i = 0; i < 300; i++) sim.tick();
  return new Map(nodes.map((n) => [n.id, [n.x, n.y]]));
}

function renderGraph(attempt = 0) {
  if (!graph) return;
  const s = getState();
  graphChart = ecInit($('graph'), echarts);
  if (!graphChart) {
    // The box has no size yet (layout not settled): try again shortly, a bounded number of times.
    if (attempt < 25) setTimeout(() => renderGraph(attempt + 1), 200);
    return;
  }
  const positions = s.mode === 'force' ? forcePositions(graph, s.kinds) : new Map();
  graphChart.setOption(worldGraphOption(graph, { mode: s.mode, positions, kinds: s.kinds }), true);
  graphChart.off('click');
  graphChart.on('click', (p) => {
    if (p.dataType !== 'node') return;
    const node = graph.nodes.find((n) => n.id === p.data.id);
    const edges = graph.edges.filter((e) => e.from === node.id || e.to === node.id);
    const show = scrollOp(ops);
    const button = show ? Object.assign(document.createElement('button'), {
      type: 'button', className: `${CLS.primary} mb-2`, textContent: 'Show in game',
      onclick: () => client.call('hub.call', { instance: show.instance, id: show.id, body: { x: node.x, y: node.y } }),
    }) : document.createTextNode('');
    $('details').replaceChildren(button, jsonView({ node, edges }));
  });
}

function graphControls() {
  const s = getState();
  const mode = document.createElement('select');
  mode.className = CLS.input;
  for (const m of ['map', 'force']) mode.append(new Option(m === 'map' ? 'placed on the map' : 'force layout (D3)', m, false, s.mode === m));
  mode.onchange = () => { setState({ mode: mode.value }); renderGraph(); };
  const kinds = KINDS.map((k) => {
    const box = Object.assign(document.createElement('input'), { type: 'checkbox', checked: s.kinds.includes(k), className: 'me-1' });
    box.onchange = () => {
      const now = getState().kinds;
      setState({ kinds: box.checked ? [...new Set([...now, k])] : now.filter((x) => x !== k) });
      renderGraph();
    };
    const label = document.createElement('label');
    label.className = 'text-sm text-gray-700 dark:text-gray-300 me-2';
    label.append(box, k);
    return label;
  });
  $('graph-controls').replaceChildren(...kinds, mode, maximizable($('graph-card'), 'graph', { getState, setState, onResize: () => graphChart && graphChart.resize() }));
}

// ---- terrain (three.js) -------------------------------------------------------------------
const TYPE_COLOUR = { C: 0x6b8e23, R: 0x8b7355, r: 0x555555, H: 0xb22222, T: 0x2e5e1e, S: 0x9370db, W: 0x1e6fbf, V: 0x000000, I: 0xdaa520, B: 0x777777, O: 0xaaaaaa };
let terrain = null;

async function renderTerrain(instance) {
  const [map, ways] = await Promise.all([read(instance, 'openttd://map'), read(instance, 'openttd://map/ways')]);
  const box = $('terrain');
  if (!terrain) {
    const renderer = new THREE.WebGLRenderer({ antialias: true });
    box.replaceChildren(renderer.domElement);
    const scene = new THREE.Scene();
    scene.background = new THREE.Color(0x111827);
    const camera = new THREE.PerspectiveCamera(45, 1, 0.1, 5000);
    const controls = new OrbitControls(camera, renderer.domElement);
    scene.add(new THREE.AmbientLight(0xffffff, 0.6));
    const sun = new THREE.DirectionalLight(0xffffff, 0.9);
    sun.position.set(-1, 2, 1);
    scene.add(sun);
    terrain = { renderer, scene, camera, controls, mesh: null };
    const resize = () => {
      const w = box.clientWidth, h = box.clientHeight;
      if (!w || !h) return;
      renderer.setSize(w, h);
      camera.aspect = w / h;
      camera.updateProjectionMatrix();
    };
    new ResizeObserver(resize).observe(box);
    resize();
    renderer.setAnimationLoop(() => { controls.update(); renderer.render(scene, camera); });
    terrain.resize = resize;
  }
  // Sample down to at most 256 cells a side; every cell takes its tile's type, height and road way.
  const step = Math.max(1, Math.ceil(Math.max(map.width, map.height) / 256));
  const cols = Math.ceil(map.width / step), rows = Math.ceil(map.height / step);
  const geometry = new THREE.PlaneGeometry(cols, rows, cols - 1, rows - 1);
  geometry.rotateX(-Math.PI / 2);
  const pos = geometry.attributes.position;
  const colours = new Float32Array(pos.count * 3);
  const colour = new THREE.Color();
  const exaggeration = getState().exaggeration;
  for (let j = 0; j < rows; j++) {
    for (let i = 0; i < cols; i++) {
      const x = Math.min(map.width - 1, i * step), y = Math.min(map.height - 1, j * step);
      const k = j * cols + i;
      const h = parseInt(map.heights[y].substr(x * 2, 2), 16) || 0;
      pos.setY(k, h * exaggeration * 0.1);
      const road = ways && ways.roads && ways.roads[y] && ways.roads[y][x] !== '0';
      colour.setHex(road ? 0x2b2b2b : (TYPE_COLOUR[map.types[y][x]] ?? 0x808080));
      colours.set([colour.r, colour.g, colour.b], k * 3);
    }
  }
  geometry.setAttribute('color', new THREE.BufferAttribute(colours, 3));
  geometry.computeVertexNormals();
  if (terrain.mesh) { terrain.scene.remove(terrain.mesh); terrain.mesh.geometry.dispose(); }
  terrain.mesh = new THREE.Mesh(geometry, new THREE.MeshLambertMaterial({ vertexColors: true }));
  terrain.scene.add(terrain.mesh);
  terrain.camera.position.set(0, Math.max(cols, rows) * 0.6, Math.max(cols, rows) * 0.8);
  terrain.controls.target.set(0, 0, 0);
}

function terrainControls(instance) {
  const range = Object.assign(document.createElement('input'), { type: 'range', min: 1, max: 20, value: getState().exaggeration, className: 'w-32' });
  range.title = 'height exaggeration';
  range.onchange = () => { setState({ exaggeration: Number(range.value) }); renderTerrain(instance); };
  $('terrain-controls').replaceChildren(range, maximizable($('terrain-card'), 'terrain', { getState, setState, onResize: () => terrain && terrain.resize() }));
}

// ---- economy ------------------------------------------------------------------------------
const samples = [];
let moneyChart = null, waitingChart = null;

async function sampleEconomy(instance) {
  const c = await read(instance, 'openttd://companies');
  samples.push({ t: Date.now(), companies: (c.companies || []).map((x) => ({ id: x.id, name: x.name || `company ${x.id}`, money: x.money })) });
  if (samples.length > 720) samples.shift();
  moneyChart = moneyChart || ecInit($('money'), echarts);
  if (!moneyChart) return;
  if (moneyChart) moneyChart.setOption(moneyOption(samples));
  if (graph) {
    waitingChart = waitingChart || ecInit($('waiting'), echarts);
    if (waitingChart) waitingChart.setOption(waitingOption(graph), true);
  }
}

// ---- start --------------------------------------------------------------------------------
async function start() {
  ops = await client.call('hub.catalogue');
  const list = candidates();
  const sel = $('instance');
  sel.replaceChildren(...list.map((i) => new Option(i, i)));
  let instance = getState().instance;
  if (!list.includes(instance)) instance = list[0] || '';
  sel.value = instance;
  sel.onchange = () => { setState({ instance: sel.value }); location.reload(); };
  if (!instance) { $('details').textContent = 'No instance publishes openttd://world/graph yet.'; return; }
  setState({ instance });
  graphControls();
  terrainControls(instance);
  $('economy-controls').replaceChildren(maximizable($('economy-card'), 'economy', { getState, setState, onResize: () => { moneyChart && moneyChart.resize(); waitingChart && waitingChart.resize(); } }));
  graph = await read(instance, 'openttd://world/graph');
  // Charts are drawn once their boxes have a size: a short timeout, then a settle pass.
  setTimeout(() => renderGraph(), 30);
  setTimeout(() => graphChart && graphChart.resize(), 300);
  renderTerrain(instance).catch((e) => { $('terrain').textContent = `terrain: ${e.message || e}`; });
  sampleEconomy(instance);
  setInterval(() => sampleEconomy(instance).catch(() => {}), 5000);
  setInterval(async () => { graph = await read(instance, 'openttd://world/graph').catch(() => graph); renderGraph(); }, 15000);
}

const ready = new Promise((resolve) => { if (client.ws && client.ws.readyState === 1) resolve(); else if (client.on) client.on('open', resolve); else setTimeout(resolve, 500); });
ready.then(start).catch((e) => { $('details').textContent = `hub: ${e.message || e}`; });
