// Not a test: the smallest browser a page script of static/ needs to run under node --test.
// Elements are found by id (one is made on first use, as if the page had it) or by a selector over
// the markup a script wrote (innerHTML is parsed into elements that keep their identity until it is
// written again). fetch answers from a route table, timers are recorded and run on request, and
// EventSource, WebSocket, Image and echarts are fakes a test drives. Nothing talks to a game.

const VOID = new Set(['input', 'img', 'br', 'hr', 'meta', 'link', 'source', 'option-void']);
const unescape = (s) =>
  s
    .replace(/&quot;/g, '"')
    .replace(/&lt;/g, '<')
    .replace(/&gt;/g, '>')
    .replace(/&amp;/g, '&');
const camel = (s) => s.replace(/-([a-z])/g, (_, c) => c.toUpperCase());

/** One simple selector: tag, #id, .class, [attr], [attr=value], :checked. */
function matchesOne(el, sel) {
  const re =
    /([a-zA-Z][\w-]*)|#([\w-]+)|\.([\w-[\]:/.]+)|\[([\w-]+)(?:=["']?([^\]"']*)["']?)?\]|:([\w-]+)/g;
  for (const m of sel.matchAll(re)) {
    if (m[1] && el.localName !== m[1].toLowerCase()) return false;
    if (m[2] && el.id !== m[2]) return false;
    if (m[3] && !el.classList.contains(m[3])) return false;
    if (m[4] && (m[5] === undefined ? !el.hasAttribute(m[4]) : el.getAttribute(m[4]) !== m[5])) {
      if (!(m[4] === 'type' && el.type === m[5])) return false;
    }
    if (m[6] === 'checked' && !el.checked) return false;
  }
  return true;
}

function matches(el, selector) {
  return selector.split(',').some((part) => {
    const chain = part.trim().split(/\s+/);
    if (!matchesOne(el, chain.at(-1))) return false;
    let up = el.parentNode;
    for (let i = chain.length - 2; i >= 0; i--) {
      while (up && !(up.nodeType === 1 && matchesOne(up, chain[i]))) up = up.parentNode;
      if (!up) return false;
      up = up.parentNode;
    }
    return true;
  });
}

export class FakeElement {
  constructor(tag = 'div', attrs = {}) {
    Object.assign(this, { nodeType: 1, localName: tag.toLowerCase(), tagName: tag.toUpperCase() });
    Object.assign(this, {
      attrs: {},
      dataset: {},
      style: {},
      listeners: {},
      kids: [],
      parentNode: null,
    });
    Object.assign(this, { _html: '', _text: null, _parsed: null, _cls: new Set() });
    Object.assign(this, {
      offsetWidth: 800,
      offsetHeight: 400,
      clientWidth: 800,
      clientHeight: 400,
    });
    Object.assign(this, {
      scrollTop: 0,
      scrollHeight: 0,
      value: '',
      checked: false,
      disabled: false,
    });
    for (const [k, v] of Object.entries(attrs)) this.setAttribute(k, v);
  }

  setAttribute(k, v) {
    v = String(v);
    this.attrs[k] = v;
    if (k === 'class') this.className = v;
    else if (k === 'id') this.id = v;
    else if (k === 'value') this.value = v;
    else if (k === 'type') this.type = v;
    else if (k === 'checked') this.checked = true;
    else if (k === 'disabled') this.disabled = true;
    else if (k.startsWith('data-')) this.dataset[camel(k.slice(5))] = v;
  }
  getAttribute(k) {
    return k in this.attrs ? this.attrs[k] : null;
  }
  hasAttribute(k) {
    return k in this.attrs;
  }
  removeAttribute(k) {
    delete this.attrs[k];
  }

  get className() {
    return [...this._cls].join(' ');
  }
  set className(v) {
    this._cls = new Set(String(v).split(/\s+/).filter(Boolean));
  }
  get classList() {
    const s = this._cls;
    return {
      add: (...c) => c.forEach((x) => s.add(x)),
      remove: (...c) => c.forEach((x) => s.delete(x)),
      contains: (c) => s.has(c),
      toggle: (c, force) => {
        const on = force === undefined ? !s.has(c) : !!force;
        if (on) s.add(c);
        else s.delete(c);
        return on;
      },
    };
  }

  get innerHTML() {
    return this._html;
  }
  set innerHTML(v) {
    this._html = String(v);
    this._text = null;
    this._parsed = null;
    this.kids = [];
  }
  get textContent() {
    if (this._text !== null) return this._text;
    return (
      unescape(this._html.replace(/<[^>]*>/g, '')) + this.kids.map((k) => k.textContent).join('')
    );
  }
  set textContent(v) {
    this.innerHTML = '';
    this._text = String(v);
  }
  insertAdjacentHTML(pos, html) {
    this.innerHTML = /begin/.test(pos) ? html + this._html : this._html + html;
  }

  /** The elements the markup holds, parsed once per write: [{el, depth}]. */
  parsed() {
    if (this._parsed) return this._parsed;
    const out = [];
    const stack = [{ el: this }];
    for (const m of this._html.matchAll(/<(\/?)([a-zA-Z][\w-]*)((?:[^>"']|"[^"]*"|'[^']*')*)>/g)) {
      const [, close, tag, rest] = m;
      if (close) {
        const at = stack.findLastIndex((s) => s.tag === tag.toLowerCase());
        if (at > 0) {
          for (const s of stack.splice(at)) s.el._html ||= this._html.slice(s.start, m.index);
        }
        continue;
      }
      const attrs = {};
      for (const a of rest.matchAll(/([\w:-]+)(?:\s*=\s*(?:"([^"]*)"|'([^']*)'|([^\s>]+)))?/g)) {
        attrs[a[1]] = unescape(a[2] ?? a[3] ?? a[4] ?? '');
      }
      const el = new FakeElement(tag, attrs);
      el.parentNode = stack.at(-1).el;
      out.push({ el, depth: stack.length - 1 });
      if (!VOID.has(tag.toLowerCase()) && !/\/$/.test(rest)) {
        stack.push({ el, tag: tag.toLowerCase(), start: m.index + m[0].length });
      }
    }
    this._parsed = out;
    return out;
  }

  /** Every element below this one: parsed markup and appended children, depth first. */
  descendants() {
    const out = [];
    for (const { el } of this.parsed()) out.push(el, ...el.kidDescendants());
    out.push(...this.kidDescendants());
    return out;
  }
  kidDescendants() {
    return this.kids.filter((k) => k.nodeType === 1).flatMap((k) => [k, ...k.descendants()]);
  }
  querySelectorAll(sel) {
    return this.descendants().filter((e) => matches(e, sel));
  }
  querySelector(sel) {
    return this.querySelectorAll(sel)[0] || null;
  }
  matches(sel) {
    return matches(this, sel);
  }
  closest(sel) {
    for (let e = this; e && e.nodeType === 1; e = e.parentNode) if (matches(e, sel)) return e;
    return null;
  }

  get childElementCount() {
    return this.parsed().filter((p) => p.depth === 0).length + this.kids.length;
  }
  get children() {
    return [
      ...this.parsed()
        .filter((p) => p.depth === 0)
        .map((p) => p.el),
      ...this.kids,
    ];
  }
  get firstElementChild() {
    return this.children[0] || null;
  }
  get lastChild() {
    return this.children.at(-1) || null;
  }
  get options() {
    return this.querySelectorAll('option').concat(
      this.kids.filter((k) => k.localName === 'option'),
    );
  }

  adopt(nodes) {
    return nodes
      .flat()
      .filter((n) => n !== null && n !== undefined)
      .map((n) => {
        const node = typeof n === 'object' ? n : textNode(String(n));
        node.parentNode = this;
        return node;
      });
  }
  append(...n) {
    this.kids.push(...this.adopt(n));
  }
  prepend(...n) {
    this.kids.unshift(...this.adopt(n));
  }
  replaceChildren(...n) {
    this.innerHTML = '';
    this.kids = this.adopt(n);
  }
  remove() {
    const p = this.parentNode;
    if (!p) return;
    p.kids = p.kids.filter((k) => k !== this);
    if (p._parsed) p._parsed = p._parsed.filter((x) => x.el !== this);
    this.parentNode = null;
  }

  addEventListener(type, fn) {
    (this.listeners[type] ||= []).push(fn);
  }
  removeEventListener(type, fn) {
    this.listeners[type] = (this.listeners[type] || []).filter((f) => f !== fn);
  }
  /** Fire an event at this element; it bubbles to its ancestors and to the document. */
  dispatch(type, extra = {}) {
    const ev = {
      type,
      target: this,
      defaultPrevented: false,
      key: '',
      clientX: 0,
      clientY: 0,
      ...extra,
    };
    ev.preventDefault = () => {
      ev.defaultPrevented = true;
    };
    const results = [];
    for (let e = this; e; e = e.parentNode) {
      ev.currentTarget = e;
      if (typeof e[`on${type}`] === 'function') results.push(e[`on${type}`](ev));
      for (const f of e.listeners?.[type] || []) results.push(f(ev));
    }
    return Promise.all(results).then(() => ev);
  }
  click() {
    return this.dispatch('click');
  }
  focus() {
    globalThis.document.activeElement = this;
  }
  getBoundingClientRect() {
    return { left: 0, top: 0, width: this.offsetWidth, height: this.offsetHeight };
  }
  getContext() {
    const calls = (this.drawn ||= []);
    return new Proxy(
      {},
      {
        get: (t, k) =>
          k in t
            ? t[k]
            : (...a) => {
                calls.push([k, ...a]);
              },
        set: (t, k, v) => {
          t[k] = v;
          return true;
        },
      },
    );
  }
}

const textNode = (text) => ({ nodeType: 3, textContent: text, parentNode: null });

/** A fetch answer: JSON (or text) with a status. */
export function respond(body, { status = 200 } = {}) {
  const text = typeof body === 'string' ? body : JSON.stringify(body);
  return { ok: status < 400, status, json: async () => JSON.parse(text), text: async () => text };
}

/**
 * Install the fake browser on globalThis. `routes(url, init)` answers fetch (a value is sent as
 * JSON, a respond() as it is, a thrown error rejects). Returns the handles a test drives.
 */
export function fakeDom({ url = 'http://127.0.0.1:8800/', routes = () => ({}) } = {}) {
  const env = { fetches: [], timers: [], sources: [], sockets: [], images: [], charts: [], routes };
  const body = new FakeElement('body');
  const byId = new Map();
  const document = {
    body,
    documentElement: new FakeElement('html', { class: 'dark' }),
    activeElement: null,
    hidden: false,
    listeners: {},
    getElementById(id) {
      const found = body.descendants().find((e) => e.id === id);
      if (found) return found;
      if (!byId.has(id)) {
        const el = new FakeElement('div', { id });
        byId.set(id, el);
        body.append(el);
      }
      return byId.get(id);
    },
    querySelectorAll: (sel) => body.querySelectorAll(sel),
    querySelector: (sel) => body.querySelector(sel),
    createElement: (tag) => new FakeElement(tag),
    createTextNode: textNode,
    addEventListener(type, fn) {
      (this.listeners[type] ||= []).push(fn);
    },
    /** Fire a document-level event (keydown, click with a target, DOMContentLoaded). */
    dispatch(type, extra = {}) {
      const ev = {
        type,
        key: '',
        target: body,
        preventDefault() {
          ev.defaultPrevented = true;
        },
        ...extra,
      };
      return Promise.all((this.listeners[type] || []).map((f) => f(ev))).then(() => ev);
    },
  };
  body.parentNode = document;
  env.document = document;

  const loc = { reloads: 0 };
  const setHref = (v) => {
    loc.url = new URL(v, loc.url || url);
  };
  setHref(url);
  const location = {
    get href() {
      return loc.url.href;
    },
    set href(v) {
      setHref(v);
    },
    get pathname() {
      return loc.url.pathname;
    },
    get search() {
      return loc.url.search;
    },
    get hash() {
      return loc.url.hash;
    },
    set hash(v) {
      loc.url.hash = v;
    },
    get host() {
      return loc.url.host;
    },
    get origin() {
      return loc.url.origin;
    },
    reload() {
      loc.reloads++;
    },
    assign: setHref,
    toString() {
      return loc.url.href;
    },
  };
  env.location = location;
  env.reloads = () => loc.reloads;

  class FakeImage {
    constructor() {
      env.images.push(this);
    }
  }
  class FakeEventSource {
    constructor(src) {
      Object.assign(this, { src, listeners: {}, closed: false });
      env.sources.push(this);
    }
    addEventListener(k, f) {
      (this.listeners[k] ||= []).push(f);
    }
    close() {
      this.closed = true;
    }
    /** The server sends one event of a kind. */
    emit(kind, data) {
      for (const f of this.listeners[kind] || []) f({ data: JSON.stringify(data) });
    }
  }
  class FakeWebSocket {
    constructor(u) {
      Object.assign(this, { url: u, sent: [], readyState: 0 });
      env.sockets.push(this);
    }
    send(m) {
      this.sent.push(JSON.parse(m));
    }
  }
  class FakeOption extends FakeElement {
    constructor(text = '', value = text, _def = false, selected = false) {
      super('option', { value });
      this.textContent = text;
      this.selected = selected;
    }
  }
  const chartOf = (box) => env.charts.find((c) => c.box === box);
  const echarts = {
    init(box, theme) {
      const chart = {
        box,
        theme,
        options: [],
        handlers: {},
        resized: 0,
        setOption(o) {
          this.options.push(o);
        },
        getOption() {
          return this.options.at(-1);
        },
        on(k, f) {
          this.handlers[k] = f;
        },
        off(k) {
          delete this.handlers[k];
        },
        resize() {
          this.resized++;
        },
      };
      env.charts.push(chart);
      return chart;
    },
    getInstanceByDom: (box) => chartOf(box),
  };

  const realSetImmediate = globalThis.setImmediate;
  const timer = (fn, ms, repeat) => {
    env.timers.push({ fn, ms, repeat });
    return env.timers.length;
  };
  const fetch = async (u, init = {}) => {
    env.fetches.push({ url: String(u), init });
    const r = await env.routes(String(u), init);
    return r && typeof r.json === 'function' && 'ok' in r ? r : respond(r ?? {});
  };
  const globals = {
    document,
    location,
    window: globalThis,
    fetch,
    echarts,
    Image: FakeImage,
    Option: FakeOption,
    EventSource: FakeEventSource,
    WebSocket: FakeWebSocket,
    history: { replaceState: (_s, _t, u) => setHref(String(u)) },
    navigator: {
      clipboard: {
        writeText: async (t) => {
          env.copied = t;
        },
      },
    },
    setTimeout: (fn, ms) => timer(fn, ms, false),
    setInterval: (fn, ms) => timer(fn, ms, true),
    clearTimeout: () => {},
    clearInterval: () => {},
    ResizeObserver: class {
      observe() {}
      disconnect() {}
    },
    matchMedia: () => ({ matches: false }),
    addEventListener: (type, fn) => {
      (env.windowListeners ||= {})[type] = fn;
    },
  };
  for (const [k, v] of Object.entries(globals)) {
    Object.defineProperty(globalThis, k, { value: v, configurable: true, writable: true });
  }
  /** Let every pending promise chain run (fetch answers, awaits), without real timers. */
  env.settle = async (turns = 30) => {
    for (let i = 0; i < turns; i++) await new Promise((r) => realSetImmediate(r));
  };
  /** Run the recorded timers once each (those added while running wait for the next call). */
  env.tick = async () => {
    const due = env.timers.splice(0);
    for (const t of due) {
      await t.fn();
      if (t.repeat) env.timers.push(t);
    }
    await env.settle();
  };
  env.byId = (id) => document.getElementById(id);
  return env;
}
