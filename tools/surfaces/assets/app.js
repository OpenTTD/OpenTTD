// Client layer of the generated site: theme, search, table filters, Mermaid diagrams and
// ECharts charts. Every chart is a view the reader drives: its parameters live in the URL
// (?activity.weeks=26&max=activity), each card maximizes to the viewport, Escape restores.
(function () {
  'use strict';

  const root = document.body.dataset.root || '';
  const params = new URLSearchParams(location.search);
  const charts = {};
  let siteData = null;

  const isDark = () => document.documentElement.classList.contains('dark');

  function setParam(key, value) {
    if (value === null || value === undefined || value === '') params.delete(key);
    else params.set(key, value);
    const qs = params.toString();
    history.replaceState(null, '', location.pathname + (qs ? '?' + qs : '') + location.hash);
  }

  function loadScript(src) {
    return new Promise((resolve, reject) => {
      const s = document.createElement('script');
      s.src = root + src;
      s.onload = resolve;
      s.onerror = reject;
      document.head.appendChild(s);
    });
  }

  // ---------------------------------------------------------------- theme

  document.getElementById('theme-toggle')?.addEventListener('click', () => {
    const dark = !isDark();
    document.documentElement.classList.toggle('dark', dark);
    try {
      localStorage.setItem('color-theme', dark ? 'dark' : 'light');
    } catch (e) { /* private mode: the toggle still works for this page */ }
    renderMermaid();
    renderCharts();
  });

  // ---------------------------------------------------------------- search

  const input = document.getElementById('search');
  const results = document.getElementById('search-results');
  let index = null;

  async function search(q) {
    if (!index) index = await (await fetch(root + 'data/search.json')).json();
    const terms = q.toLowerCase().split(/\s+/).filter(Boolean);
    if (!terms.length) {
      results.classList.add('hidden');
      return;
    }
    const hits = index
      .map((d) => {
        const hay = [d.t, d.p, d.s, (d.h || []).join(' '), d.b || ''].join(' ').toLowerCase();
        if (!terms.every((t) => hay.includes(t))) return null;
        const score = terms.reduce((n, t) => n + (d.t.toLowerCase().includes(t) ? 3 : 0)
          + (d.p.toLowerCase().includes(t) ? 2 : 0), 0);
        return { d, score };
      })
      .filter(Boolean)
      .sort((a, b) => b.score - a.score)
      .slice(0, 12);
    results.innerHTML = hits.length
      ? hits.map(({ d }) => '<a href="' + root + d.u + '" class="block border-b border-gray-100 '
        + 'px-4 py-2 hover:bg-gray-100 dark:border-gray-600 dark:hover:bg-gray-600">'
        + '<div class="text-sm font-medium text-gray-900 dark:text-white">' + escapeHtml(d.t)
        + '</div><div class="truncate text-xs text-gray-500 dark:text-gray-400">'
        + escapeHtml(d.p) + '</div></a>').join('')
      : '<div class="px-4 py-2 text-sm text-gray-500 dark:text-gray-400">No document matches</div>';
    results.classList.remove('hidden');
  }

  function escapeHtml(s) {
    return String(s).replace(/[&<>"']/g, (c) => ({
      '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;',
    })[c]);
  }

  if (input) {
    input.addEventListener('input', () => search(input.value));
    input.addEventListener('keydown', (e) => {
      if (e.key === 'Enter') {
        const first = results.querySelector('a');
        if (first) location.href = first.href;
      }
    });
    document.addEventListener('keydown', (e) => {
      if (e.key === '/' && document.activeElement !== input && !e.target.closest('input,select,textarea')) {
        e.preventDefault();
        input.focus();
      }
    });
    document.addEventListener('click', (e) => {
      if (!e.target.closest('[role=search]')) results.classList.add('hidden');
    });
  }

  // ---------------------------------------------------------------- table filters

  document.querySelectorAll('[data-filter-for]').forEach((box) => {
    const table = document.querySelector('[data-filter="' + box.dataset.filterFor + '"]');
    if (!table) return;
    const key = 'filter';
    box.value = params.get(key) || '';
    const apply = () => {
      const terms = box.value.toLowerCase().split(/\s+/).filter(Boolean);
      table.querySelectorAll('tbody tr').forEach((tr) => {
        const text = tr.textContent.toLowerCase();
        tr.classList.toggle('hidden', !terms.every((t) => text.includes(t)));
      });
      setParam(key, box.value);
    };
    box.addEventListener('input', apply);
    apply();
  });

  // ---------------------------------------------------------------- maximize

  function maximize(card, on) {
    card.classList.toggle('is-maximized', on);
    document.body.classList.toggle('overflow-hidden', on);
    setParam('max', on ? card.id : null);
    Object.values(charts).forEach((c) => c.resize());
  }

  document.querySelectorAll('.maximize').forEach((btn, i) => {
    const card = btn.closest('.chart-card, .diagram');
    if (!card.id) card.id = 'diagram-' + i;
    btn.addEventListener('click', () => maximize(card, !card.classList.contains('is-maximized')));
  });

  document.addEventListener('keydown', (e) => {
    if (e.key !== 'Escape') return;
    document.querySelectorAll('.is-maximized').forEach((c) => maximize(c, false));
  });

  // ---------------------------------------------------------------- Mermaid

  const diagrams = document.querySelectorAll('pre.mermaid');
  let mermaidReady = null;

  function renderMermaid() {
    if (!diagrams.length) return;
    mermaidReady = mermaidReady || loadScript('js/mermaid.min.js');
    mermaidReady.then(() => {
      diagrams.forEach((el) => {
        el.removeAttribute('data-processed');
        el.textContent = el.dataset.src;
      });
      window.mermaid.initialize({
        startOnLoad: false,
        securityLevel: 'strict',
        theme: isDark() ? 'dark' : 'default',
        fontFamily: 'Inter, ui-sans-serif, system-ui, sans-serif',
      });
      return window.mermaid.run({ nodes: diagrams });
    }).catch((err) => console.error('[surfaces] mermaid', err));
  }

  // ---------------------------------------------------------------- ECharts

  const STATUS = { done: '#0E9F6E', active: '#1C64F2', open: '#9CA3AF' };
  const PRIMARY = '#1A56DB';

  const builders = {
    activity(data) {
      const weeks = Number(params.get('activity.weeks') || 52);
      const rows = weeks > 0 ? data.activity.slice(-weeks) : data.activity;
      return {
        tooltip: { trigger: 'axis' },
        grid: { left: 40, right: 16, top: 16, bottom: 56 },
        xAxis: { type: 'category', data: rows.map((r) => r[0]) },
        yAxis: { type: 'value', minInterval: 1 },
        dataZoom: [{ type: 'inside' }, { type: 'slider', height: 18, bottom: 8 }],
        series: [{
          type: 'bar', name: 'commits', data: rows.map((r) => r[1]),
          itemStyle: { color: PRIMARY, borderRadius: [3, 3, 0, 0] },
        }],
      };
    },
    docs(data) {
      const byCount = params.get('docs.metric') === 'count';
      const weigh = (n) => (n.children
        ? { ...n, children: n.children.map(weigh) }
        : { ...n, value: byCount ? 1 : n.lines });
      return {
        tooltip: {
          formatter: (p) => escapeHtml((p.data.title || p.name)) + '<br>'
            + (p.data.lines ? p.data.lines + ' lines' : p.value + (byCount ? ' documents' : ' lines')),
        },
        series: [{
          type: 'treemap', roam: false, nodeClick: 'zoomToNode', leafDepth: 2,
          breadcrumb: { bottom: 4 }, top: 8, bottom: 36, left: 4, right: 4,
          data: data.docs.map(weigh),
          label: { show: true, formatter: '{b}' },
          upperLabel: { show: true, height: 22 },
          levels: [
            { itemStyle: { borderWidth: 2, gapWidth: 2, borderColor: isDark() ? '#1F2937' : '#fff' } },
            { colorSaturation: [0.35, 0.6], itemStyle: { gapWidth: 1, borderColorSaturation: 0.6 } },
          ],
        }],
      };
    },
    freshness(data) {
      const n = Number(params.get('freshness.n') || 20);
      const rows = data.freshness.slice(0, n).reverse();
      return {
        tooltip: { trigger: 'axis', axisPointer: { type: 'shadow' } },
        grid: { left: 8, right: 24, top: 8, bottom: 24, containLabel: true },
        xAxis: { type: 'value', name: 'days' },
        yAxis: {
          type: 'category', data: rows.map((r) => r.path),
          axisLabel: { width: 220, overflow: 'truncate' },
        },
        series: [{
          type: 'bar', data: rows.map((r) => ({ value: r.days, url: r.url })),
          itemStyle: { color: '#FDBA8C', borderRadius: [0, 3, 3, 0] },
        }],
      };
    },
    links(data) {
      // Every link between published documents, as a force graph: a node per document that links
      // or is linked, sized by how often it is linked, coloured by its top-level directory.
      const min = Number(params.get('links.min') || 1);
      const keep = new Set(data.links.nodes.filter((n) => n.in >= min || (min === 1 && n.out > 0)).map((n) => n.id));
      const groups = [...new Set(data.links.nodes.filter((n) => keep.has(n.id)).map((n) => n.group))].sort();
      return {
        tooltip: {
          formatter: (p) => (p.dataType === 'node'
            ? escapeHtml(p.data.name) + '<br><code>' + escapeHtml(p.data.id) + '</code><br>linked from '
              + p.data.in + ', links to ' + p.data.out
            : ''),
        },
        legend: { type: 'scroll', bottom: 0, data: groups },
        series: [{
          type: 'graph', layout: 'force', roam: true, draggable: true,
          categories: groups.map((g) => ({ name: g })),
          force: { repulsion: 60, edgeLength: [30, 90], gravity: 0.08 },
          emphasis: { focus: 'adjacency', lineStyle: { width: 2 } },
          lineStyle: { opacity: 0.25, curveness: 0.1 },
          label: { show: false },
          data: data.links.nodes.filter((n) => keep.has(n.id)).map((n) => ({
            ...n, category: groups.indexOf(n.group), symbolSize: 4 + 3 * Math.sqrt(n.in),
            label: { show: n.in >= 8, formatter: n.name.length > 28 ? n.name.slice(0, 26) + '…' : n.name },
          })),
          links: data.links.edges.filter(([a, b]) => keep.has(a) && keep.has(b))
            .map(([a, b]) => ({ source: a, target: b })),
        }],
      };
    },
    plan(data) {
      const rows = data.plan.slice().reverse();
      return {
        tooltip: { trigger: 'axis', axisPointer: { type: 'shadow' } },
        legend: { bottom: 0 },
        grid: { left: 8, right: 24, top: 8, bottom: 36, containLabel: true },
        xAxis: { type: 'value', minInterval: 1 },
        yAxis: {
          type: 'category', data: rows.map((r) => r.id),
          axisLabel: { width: 240, overflow: 'truncate' },
        },
        series: ['done', 'active', 'open'].map((s) => ({
          name: s, type: 'bar', stack: 'issues', itemStyle: { color: STATUS[s] },
          data: rows.map((r) => ({ value: r[s], url: r.url })),
        })),
      };
    },
  };

  function renderCharts() {
    const nodes = document.querySelectorAll('[data-chart]');
    if (!nodes.length || !siteData || !window.echarts) return;
    nodes.forEach((el) => {
      const id = el.dataset.chart;
      if (!builders[id]) return;
      if (charts[el.id || id]) charts[el.id || id].dispose();
      const chart = window.echarts.init(el, isDark() ? 'dark' : null, { renderer: 'canvas' });
      const option = builders[id](siteData);
      option.backgroundColor = 'transparent';
      option.textStyle = { fontFamily: 'Inter, ui-sans-serif, system-ui, sans-serif' };
      chart.setOption(option);
      chart.on('click', (p) => {
        const url = p.data && p.data.url;
        if (url) location.href = root + url;
      });
      charts[el.id || id] = chart;
    });
  }

  document.querySelectorAll('select[data-param]').forEach((sel) => {
    const key = sel.dataset.param;
    if (params.get(key)) sel.value = params.get(key);
    sel.addEventListener('change', () => {
      setParam(key, sel.value);
      renderCharts();
    });
  });

  window.addEventListener('resize', () => Object.values(charts).forEach((c) => c.resize()));

  async function startCharts() {
    if (!document.querySelector('[data-chart]')) return;
    const [data] = await Promise.all([
      fetch(root + 'data/site.json').then((r) => r.json()),
      loadScript('js/echarts.min.js'),
    ]);
    siteData = data;
    renderCharts();
  }

  startCharts()
    .catch((err) => console.error('[surfaces] charts', err))
    .finally(() => {
      const max = params.get('max');
      const card = max && document.getElementById(max);
      if (card) maximize(card, true);
    });
  renderMermaid();
})();
