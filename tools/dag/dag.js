'use strict';

const $ = (sel) => document.querySelector(sel);
const SVG_NS = 'http://www.w3.org/2000/svg';

const state = {
  model: null,
  events: [],
  cursor: -1,
  selected: null,
  view: null,
  groups: [],
  groupSize: new Map(),
  collapsed: new Set(),
};

/* Step annotations: journal lines that are not timeline events but state
   valid for the whole enclosing step. They attach to the step and are
   installed into the world when the step applies. */
const STEP_ANNOTATIONS = {
  predict: (step, e) => { step.predict = new Map(e.rows.map((row) => [row.action, row])); },
};

function parseJournal(text) {
  const model = { meta: null, artifacts: [], actions: [], phantoms: new Set(), plan: [] };
  const events = [];
  let step = null;
  for (const line of text.split('\n')) {
    if (!line.trim()) continue;
    const e = JSON.parse(line);
    switch (e.ev) {
      case 'meta': model.meta = e; break;
      case 'artifact': model.artifacts[e.id] = e; break;
      case 'action': model.actions[e.id] = e; break;
      case 'plan':
        model.plan.push(e);
        if (e.kind === 'phantom') model.phantoms.add(e.artifact);
        break;
      default:
        if (STEP_ANNOTATIONS[e.ev]) {
          if (step) STEP_ANNOTATIONS[e.ev](step, e);
          break;
        }
        if (e.ev === 'step') step = e;
        if (e.ev === 'pass') step = null;
        events.push(e);
        break;
    }
  }
  for (const action of model.actions) {
    for (const obs of action.obs) {
      if (obs.startsWith('g')) model.phantoms.add(Number(obs.slice(1)));
    }
  }
  return { model, events };
}

function initialWorld(model) {
  const world = {
    pass: null,
    state: 'clean',
    artifacts: new Map(),
    phantoms: new Map(),
    predict: null,
    run: new Map(),
    verdicts: [],
    checks: new Map(),
    lastRun: null,
    done: null,
  };
  model.artifacts.forEach((artifact, id) => {
    world.artifacts.set(id, { content: artifact.content, dirty: false, stealth: false, deleted: false });
  });
  for (const id of model.phantoms) {
    world.phantoms.set(id, { present: false, content: null });
  }
  return world;
}

function runOf(world, action) {
  if (!world.run.has(action)) world.run.set(action, { model: null, execs: 0, dag: [] });
  return world.run.get(action);
}

function applyEvent(world, model, e) {
  switch (e.ev) {
    case 'pass': {
      const fresh = initialWorld(model);
      Object.assign(world, fresh);
      world.pass = e.name;
      break;
    }
    case 'step': {
      world.predict = e.predict ?? null;
      if (e.kind === 'run' || e.kind === 'eio' || e.kind === 'crash') {
        world.run = new Map();
        world.verdicts = [];
        world.checks = new Map();
        world.lastRun = null;
        model.artifacts.forEach((artifact, id) => {
          if (artifact.kind === 'source') world.artifacts.get(id).dirty = false;
        });
      }
      const artifact = world.artifacts.get(e.artifact);
      switch (e.kind) {
        case 'mutate':
        case 'revert': artifact.content = e.content; artifact.dirty = true; artifact.stealth = false; break;
        case 'stealth': artifact.content = e.content; artifact.stealth = true; break;
        case 'touch': artifact.dirty = true; break;
        case 'delete': artifact.deleted = true; break;
        case 'phantom': {
          const phantom = world.phantoms.get(e.artifact);
          if (phantom.present) { phantom.present = false; }
          else { phantom.present = true; phantom.content = e.content; }
          break;
        }
      }
      break;
    }
    case 'exec': runOf(world, e.action).execs++; break;
    case 'run_done': world.lastRun = e; break;
    case 'world': world.state = e.state; break;
    case 'check': {
      if (e.kind === 'execs') {
        world.checks = new Map(e.rows.map((row) => [row.action, row]));
      } else {
        for (const row of e.rows) world.verdicts.push({ ...row, check: e.kind });
      }
      break;
    }
    case 'done': world.done = e; break;
    default:
      if (e.ev.startsWith('dag.')) {
        runOf(world, e.action).dag.push(e);
        if (e.ev === 'dag.settle') {
          const live = world.artifacts.get(e.artifact);
          live.deleted = false;
          live.dirty = false;
        }
      }
      break;
  }
}

function worldAt(cursor) {
  const world = initialWorld(state.model);
  for (let i = 0; i <= cursor; i++) applyEvent(world, state.model, state.events[i]);
  return world;
}

function describeStep(e) {
  switch (e.kind) {
    case 'run': return 'run';
    case 'mutate': return `mutate f${e.artifact} ← c${e.content}`;
    case 'revert': return `revert f${e.artifact} ← c${e.content}`;
    case 'stealth': return `stealth f${e.artifact} ← c${e.content}`;
    case 'touch': return `touch f${e.artifact}`;
    case 'delete': return `delete f${e.artifact}`;
    case 'phantom': return `phantom g${e.artifact} c${e.content}`;
    case 'blob': return 'drop a store blob';
    case 'evict': return 'drop a cache entry';
    case 'discovery': return 'reset discovery';
    case 'eio': return `run, io fault every ~${e.rate} syscalls`;
    case 'crash': return `run + crash`;
    default: return e.kind;
  }
}

function describePredictRow(row) {
  return `${row.hit ? 'hit' : 'miss'}${row.resolved ? '' : ' (unresolved)'} · ${row.key}`;
}

function describeExecRow(row) {
  return `execs ${row.execs}/${row.want}${row.requeues ? ` (+${row.requeues} requeue)` : ''}${row.ok ? '' : ' MISMATCH'}`;
}

function describeBytesRow(row) {
  return `f${row.artifact} ${row.check} ${row.ok ? 'ok' : `want ${row.want} got ${row.got}`}`;
}

function describe(e) {
  switch (e.ev) {
    case 'pass': return `pass: ${e.name}`;
    case 'step': return `step ${e.i} — ${describeStep(e)}`;
    case 'exec': return `a${e.action} ran`;
    case 'run_done': {
      const out = e.err ? `run FAILED — ${e.err_str || `err ${e.err}`}` : 'run done';
      const crashed = e.crashed ? ' — crashed mid-run' : '';
      const fired = e.fired ? ` (${e.fired} io fault${e.fired === 1 ? '' : 's'} fired)` : '';
      return out + crashed + fired;
    }
    case 'sim.fault': return 'io fault fired';
    case 'drop': return `dropped ${e.what} ${e.path}`;
    case 'world': return `world ${e.state}`;
    case 'check': {
      const bad = e.rows.filter((row) => !row.ok).length;
      return `check ${e.kind}: ${bad ? `${bad}/${e.rows.length} FAILED` : `${e.rows.length} ok`}`;
    }
    case 'blob': return `f${e.artifact} store blob ${e.want === e.got ? 'has the wanted bytes' : `differs: ${e.got}`}`;
    case 'sim.write': return `f${e.artifact} was written here (${e.path})`;
    case 'done': return e.err ? `FAIL: ${e.str}` : 'done: ok';
    case 'dag.key': return `a${e.action} key ${e.key}`;
    case 'dag.discovery': return `a${e.action} pathset ${e.found ? 'found' : 'absent'}`;
    case 'dag.resolve': return `a${e.action} resolve ${e.ok ? 'ok' : 'failed'}${e.changed ? ' (changed)' : ''}`;
    case 'dag.strong': return `a${e.action} strong ${e.key}`;
    case 'dag.cache': return `a${e.action} cache ${e.hit ? 'hit' : e.present ? 'restore failed' : 'miss'}`;
    case 'dag.execute': return `a${e.action} execute`;
    case 'dag.commit': return `a${e.action} commit${e.recorded ? '' : ' (unrecorded)'}`;
    case 'dag.settle': return `a${e.action} settled f${e.artifact} ${e.digest}${e.skipped ? ' (skipped)' : ''}`;
    case 'dag.defer': return `a${e.action} resolve waits on a${e.producer}`;
    case 'dag.requeue': return `a${e.action} re-runs — a${e.producer} rewrote files it observed`;
    default: return e.ev;
  }
}

function eventClass(e) {
  if (e.ev === 'step') return 'event-step';
  if (e.ev === 'pass') return 'event-pass';
  if (e.ev === 'done') return e.err ? 'event-bad' : 'event-ok';
  if (e.ev === 'check') return e.rows.some((row) => !row.ok) ? 'event-bad' : 'event-ok';
  if (e.ev === 'blob' || e.ev === 'sim.write' || e.ev === 'sim.fault' || e.ev === 'drop') return 'event-warn';
  if (e.ev === 'run_done') return e.err || e.crashed ? 'event-warn' : '';
  if (e.ev === 'world') return 'event-warn';
  if (e.ev === 'dag.cache') return e.hit ? 'event-ok' : '';
  if (e.ev === 'dag.requeue') return 'event-warn';
  return '';
}

/* Graph layout: layered left-to-right. Sources, values, and phantoms sit in
   column 0; each action one column past its deepest input; outputs one past
   their producer. Rows are ordered by neighbor barycenter. */

function computeLayers(model) {
  const actionCol = new Map();
  const artifactCol = (id, stack) => {
    const artifact = model.artifacts[id];
    return artifact.producer === undefined ? 0 : actionOf(artifact.producer, stack) + 1;
  };
  const actionOf = (id, stack = new Set()) => {
    if (actionCol.has(id)) return actionCol.get(id);
    if (stack.has(id)) return 0;
    stack.add(id);
    let col = 1;
    for (const f of model.actions[id].consumes) col = Math.max(col, artifactCol(f, stack) + 1);
    stack.delete(id);
    actionCol.set(id, col);
    return col;
  };
  model.actions.forEach((_, id) => actionOf(id));
  return { actionCol, artifactCol: (id) => artifactCol(id) };
}

function buildGraph(model) {
  const { actionCol, artifactCol } = computeLayers(model);
  const nodes = new Map();

  model.artifacts.forEach((artifact, id) => {
    nodes.set(`f${id}`, { key: `f${id}`, kind: artifact.kind, col: artifactCol(id), w: 96, h: 30 });
  });
  model.actions.forEach((action, id) => {
    nodes.set(`a${id}`, { key: `a${id}`, kind: 'action', col: actionCol.get(id), w: 116, h: 34 });
  });
  for (const id of model.phantoms) {
    nodes.set(`g${id}`, { key: `g${id}`, kind: 'phantom', col: 0, w: 44, h: 44 });
  }

  const edges = [];
  model.actions.forEach((action, id) => {
    for (const f of action.consumes) edges.push({ from: `f${f}`, to: `a${id}`, kind: 'consume' });
    for (const f of action.produces) edges.push({ from: `a${id}`, to: `f${f}`, kind: 'produce' });
    for (const obs of action.obs) edges.push({ from: `a${id}`, to: obs, kind: 'obs' });
  });

  const columns = [];
  for (const node of nodes.values()) {
    (columns[node.col] ??= []).push(node);
  }

  const neighbors = new Map();
  const link = (key, other) => {
    if (!neighbors.has(key)) neighbors.set(key, []);
    neighbors.get(key).push(other);
  };
  for (const edge of edges) {
    link(edge.from, edge.to);
    link(edge.to, edge.from);
  }

  columns.forEach((column) => column.forEach((node, row) => { node.row = row; }));
  for (let sweep = 0; sweep < 3; sweep++) {
    for (const column of columns) {
      for (const node of column) {
        const near = (neighbors.get(node.key) ?? []).map((key) => nodes.get(key).row);
        node.bary = near.length ? near.reduce((a, b) => a + b, 0) / near.length : node.row;
      }
      column.sort((a, b) => a.bary - b.bary);
      column.forEach((node, row) => { node.row = row; });
    }
  }

  const colWidth = 190;
  const rowHeight = 56;
  const tallest = Math.max(...columns.map((column) => column.length));
  for (const column of columns) {
    const pad = (tallest - column.length) * rowHeight / 2;
    for (const node of column) {
      node.x = 60 + node.col * colWidth;
      node.y = 40 + pad + node.row * rowHeight;
    }
  }

  return { nodes, edges, width: 120 + columns.length * colWidth, height: 80 + tallest * rowHeight };
}

function edgePath(graph, edge) {
  const from = graph.nodes.get(edge.from);
  const to = graph.nodes.get(edge.to);
  const x1 = from.x + from.w / 2;
  const x2 = to.x - to.w / 2 - 5;
  const bend = Math.max(30, (x2 - x1) / 2);
  return `M ${x1} ${from.y} C ${x1 + bend} ${from.y}, ${x2 - bend} ${to.y}, ${x2} ${to.y}`;
}

function el(ns, tag, attrs = {}) {
  const node = ns ? document.createElementNS(ns, tag) : document.createElement(tag);
  for (const [key, value] of Object.entries(attrs)) node.setAttribute(key, value);
  return node;
}

function renderGraph() {
  const svg = $('#graph');
  svg.innerHTML = '';
  const graph = state.graph;

  const defs = el(SVG_NS, 'defs');
  const marker = el(SVG_NS, 'marker', { id: 'arrow', viewBox: '0 0 8 8', refX: 7, refY: 4, markerWidth: 7, markerHeight: 7, orient: 'auto' });
  marker.append(el(SVG_NS, 'path', { d: 'M 0 0 L 8 4 L 0 8 z' }));
  defs.append(marker);
  svg.append(defs);

  const viewport = el(SVG_NS, 'g', { id: 'viewport' });
  svg.append(viewport);

  for (const edge of graph.edges) {
    const path = el(SVG_NS, 'path', {
      class: `edge edge-${edge.kind}`,
      d: edgePath(graph, edge),
      'marker-end': 'url(#arrow)',
      'data-from': edge.from,
      'data-to': edge.to,
    });
    viewport.append(path);
  }

  for (const node of graph.nodes.values()) {
    const group = el(SVG_NS, 'g', { class: `node node-${node.kind}`, 'data-key': node.key });
    if (node.kind === 'phantom') {
      group.append(el(SVG_NS, 'circle', { cx: node.x, cy: node.y, r: node.h / 2 }));
    } else {
      group.append(el(SVG_NS, 'rect', {
        x: node.x - node.w / 2, y: node.y - node.h / 2, width: node.w, height: node.h,
        rx: node.kind === 'value' ? node.h / 2 : node.kind === 'action' ? 3 : 6,
      }));
    }
    const text = el(SVG_NS, 'text', { x: node.x, y: node.y });
    group.append(text);
    if (node.kind === 'action') {
      group.append(el(SVG_NS, 'circle', {
        class: 'predict-dot is-hidden',
        cx: node.x - node.w / 2, cy: node.y - node.h / 2, r: 4.5,
      }));
    }
    group.addEventListener('click', () => select(node.key));
    viewport.append(group);
  }

  state.view = { x: 0, y: 0, w: graph.width, h: graph.height };
  applyView();
}

function applyView() {
  const view = state.view;
  $('#graph').setAttribute('viewBox', `${view.x} ${view.y} ${view.w} ${view.h}`);
}

function installPanZoom(svg) {
  svg.addEventListener('wheel', (event) => {
    if (!state.view) return;
    event.preventDefault();
    const view = state.view;
    const scale = event.deltaY > 0 ? 1.15 : 1 / 1.15;
    const rect = svg.getBoundingClientRect();
    const px = view.x + (event.clientX - rect.left) / rect.width * view.w;
    const py = view.y + (event.clientY - rect.top) / rect.height * view.h;
    state.view = { x: px - (px - view.x) * scale, y: py - (py - view.y) * scale, w: view.w * scale, h: view.h * scale };
    applyView();
  }, { passive: false });

  svg.addEventListener('mousedown', (event) => {
    if (!state.view || event.target.closest('.node')) return;
    const view = state.view;
    const start = { x: event.clientX, y: event.clientY, vx: view.x, vy: view.y };
    const rect = svg.getBoundingClientRect();
    svg.classList.add('is-panning');
    const move = (ev) => {
      view.x = start.vx - (ev.clientX - start.x) / rect.width * view.w;
      view.y = start.vy - (ev.clientY - start.y) / rect.height * view.h;
      applyView();
    };
    const up = () => {
      svg.classList.remove('is-panning');
      window.removeEventListener('mousemove', move);
      window.removeEventListener('mouseup', up);
    };
    window.addEventListener('mousemove', move);
    window.addEventListener('mouseup', up);
  });
}

function nodeLabel(key, world) {
  const id = Number(key.slice(1));
  if (key[0] === 'a') {
    const action = state.model.actions[id];
    return `a${id} id${action.identity}${action.discover ? ' ◌' : ''}`;
  }
  if (key[0] === 'g') {
    const phantom = world.phantoms.get(id);
    return phantom?.present ? `g${id} c${phantom.content}` : `g${id}`;
  }
  const artifact = state.model.artifacts[id];
  const live = world.artifacts.get(id);
  if (artifact.kind === 'output') return `f${id}`;
  return `f${id} c${live.content}`;
}

function updateGraph(world) {
  const failed = new Set(world.verdicts.filter((v) => !v.ok).map((v) => `f${v.artifact}`));

  for (const node of state.graph.nodes.values()) {
    const group = $(`#graph .node[data-key="${node.key}"]`);
    group.querySelector('text').textContent = nodeLabel(node.key, world);
    const cls = ['node', `node-${node.kind}`];
    const id = Number(node.key.slice(1));

    if (node.key[0] === 'a') {
      const run = world.run.get(id);
      if (run?.execs) cls.push('is-exec');
      else if (run?.dag.some((e) => e.ev === 'dag.cache' && e.hit)) cls.push('is-hit');
      const check = world.checks.get(id);
      if (check && !check.ok) cls.push('is-bad');

      const dot = group.querySelector('.predict-dot');
      const row = world.predict?.get(id);
      const dotCls = ['predict-dot'];
      if (!row) dotCls.push('is-hidden');
      else if (!row.resolved) dotCls.push('is-predict-unresolved');
      else dotCls.push(row.hit ? 'is-predict-hit' : 'is-predict-miss');
      if (world.state === 'murky' || world.state === 'tainted') dotCls.push('is-stale');
      dot.setAttribute('class', dotCls.join(' '));
    } else if (node.key[0] === 'g') {
      if (world.phantoms.get(id)?.present) cls.push('is-present');
    } else {
      const live = world.artifacts.get(id);
      if (live.deleted) cls.push('is-deleted');
      if (live.dirty || live.stealth) cls.push('is-dirty');
      if (failed.has(node.key)) cls.push('is-bad');
    }

    if (state.selected === node.key) cls.push('is-selected');
    group.setAttribute('class', cls.join(' '));
  }

  document.querySelectorAll('#graph .edge').forEach((edge) => {
    const touches = edge.dataset.from === state.selected || edge.dataset.to === state.selected;
    edge.classList.toggle('is-selected', touches);
  });
}

function toggleGroup(step) {
  if (state.collapsed.has(step)) state.collapsed.delete(step);
  else state.collapsed.add(step);
  updateTimeline();
}

function renderTimeline() {
  const list = $('#events');
  list.innerHTML = '';
  state.events.forEach((event, index) => {
    const item = el(null, 'li', { class: `event ${eventClass(event)}`, 'data-index': index });
    if (event.ev === 'step' && state.groupSize.get(index)) {
      const chevron = el(null, 'span', { class: 'event-chevron' });
      chevron.addEventListener('click', (ev) => {
        ev.stopPropagation();
        toggleGroup(index);
      });
      item.append(chevron);
    }
    const sys = el(null, 'span', { class: 'event-sys' });
    sys.textContent = event.sys || '';
    const text = el(null, 'span', { class: 'event-text' });
    text.textContent = describe(event);
    item.append(sys, text);
    if (event.ev === 'step') item.append(el(null, 'span', { class: 'event-count muted' }));
    item.addEventListener('click', () => setCursor(index));
    list.append(item);
  });
}

function updateTimeline() {
  document.querySelectorAll('#events .event').forEach((item) => {
    const index = Number(item.dataset.index);
    const event = state.events[index];
    const group = state.groups[index];
    const isCursor = index === state.cursor;
    item.classList.toggle('is-cursor', isCursor);
    item.classList.toggle('is-future', index > state.cursor);
    item.hidden = group >= 0 && group !== index && state.collapsed.has(group) && !isCursor;
    if (event.ev === 'step') {
      const collapsed = state.collapsed.has(index);
      const chevron = item.querySelector('.event-chevron');
      if (chevron) chevron.textContent = collapsed ? '▸' : '▾';
      item.querySelector('.event-count').textContent = collapsed && state.groupSize.get(index) ? `${state.groupSize.get(index)}` : '';
    }
  });
  const current = $(`#events .event[data-index="${state.cursor}"]`);
  current?.scrollIntoView({ block: 'nearest' });
  $('#scrub').value = state.cursor;
  $('#pos').textContent = `${state.cursor + 1}/${state.events.length}`;
}

function chip(key) {
  const span = el(null, 'span', { class: 'link-chip' });
  span.textContent = key;
  span.addEventListener('click', () => select(key));
  return span;
}

function fields(pairs) {
  const dl = el(null, 'dl', { class: 'detail-fields' });
  for (const [label, value, cls] of pairs) {
    const dt = el(null, 'dt');
    dt.textContent = label;
    const dd = el(null, 'dd', cls ? { class: cls } : {});
    if (value instanceof Node) dd.append(value);
    else dd.textContent = value;
    dl.append(dt, dd);
  }
  return dl;
}

function chipList(keys) {
  const span = el(null, 'span');
  keys.forEach((key) => span.append(chip(key)));
  if (!keys.length) span.textContent = '—';
  return span;
}

function section(title, body) {
  const div = el(null, 'div', { class: 'detail-section' });
  const h3 = el(null, 'h3');
  h3.textContent = title;
  div.append(h3, body);
  return div;
}

function listItems(entries) {
  const ul = el(null, 'ul', { class: 'detail-events' });
  for (const entry of entries) {
    const li = el(null, 'li', entry.cls ? { class: entry.cls } : {});
    li.textContent = entry.text;
    ul.append(li);
  }
  if (!entries.length) ul.append(Object.assign(el(null, 'li', { class: 'muted' }), { textContent: '—' }));
  return ul;
}

function eventItems(events) {
  return listItems(events.map((event) => {
    const cls = eventClass(event);
    return { text: describe(event), cls: cls === 'event-bad' ? 'bad' : cls === 'event-ok' ? 'ok' : '' };
  }));
}

function renderDetails(world) {
  const panel = $('#details');
  panel.innerHTML = '';
  const head = el(null, 'div', { class: 'details-head' });
  panel.append(head);

  if (!state.selected) {
    head.textContent = 'iteration';
    const meta = state.model.meta;
    panel.append(fields([
      ['seed', meta.seed],
      ['iter', meta.iter],
      ['mode', meta.run_ex ? 'run_ex' : 'run'],
      ['stores', ['store_fs', 'disco_fs', 'cache_fs'].filter((k) => meta[k]).join(' ') || 'mem'],
      ['world', world.state],
      ['pass', world.pass ?? '—'],
    ]));
    panel.append(section('plan', eventItems(state.model.plan.map((p) => ({ ...p, ev: 'step' })))));
    if (world.done) {
      panel.append(section('result', eventItems([world.done])));
    }
    return;
  }

  const id = Number(state.selected.slice(1));
  head.textContent = state.selected;

  if (state.selected[0] === 'a') {
    const action = state.model.actions[id];
    const run = world.run.get(id);
    const check = world.checks.get(id);
    const predict = world.predict?.get(id);
    panel.append(fields([
      ['identity', `id${action.identity}`],
      ['discover', action.discover ? 'yes' : 'no'],
      ['consumes', chipList(action.consumes.map((f) => `f${f}`))],
      ['produces', chipList(action.produces.map((f) => `f${f}`))],
      ['obs', chipList(action.obs)],
    ]));
    panel.append(section('model', listItems(predict ? [{ text: describePredictRow(predict) }] : [])));
    panel.append(section('engine', eventItems(run ? run.dag : [])));
    panel.append(section('verdict', listItems(check ? [{ text: describeExecRow(check), cls: check.ok ? 'ok' : 'bad' }] : [])));
  } else if (state.selected[0] === 'g') {
    const phantom = world.phantoms.get(id);
    panel.append(fields([
      ['path', `/gone/g${id}`],
      ['present', phantom.present ? 'yes' : 'no', phantom.present ? 'ok' : ''],
      ['content', phantom.present ? `c${phantom.content}` : '—'],
      ['probed by', chipList(state.model.actions.filter((a) => a.obs.includes(`g${id}`)).map((a) => `a${a.id}`))],
    ]));
  } else {
    const artifact = state.model.artifacts[id];
    const live = world.artifacts.get(id);
    const verdicts = world.verdicts.filter((v) => v.artifact === id);
    const consumers = state.model.actions.filter((a) => a.consumes.includes(id) || a.obs.includes(`f${id}`));
    const rows = [
      ['kind', artifact.kind],
      ['path', artifact.path ?? '—'],
    ];
    if (artifact.kind === 'output') rows.push(['producer', chip(`a${artifact.producer}`)]);
    else rows.push(['content', `c${live.content}`]);
    rows.push(['consumers', chipList(consumers.map((a) => `a${a.id}`))]);
    if (live.dirty) rows.push(['state', 'dirty', 'bad']);
    if (live.stealth) rows.push(['state', 'stealth-written', 'bad']);
    if (live.deleted) rows.push(['state', 'deleted', 'bad']);
    panel.append(fields(rows));
    panel.append(section('byte checks', listItems(verdicts.map((row) => ({
      text: describeBytesRow(row),
      cls: row.ok ? 'ok' : 'bad',
    })))));
  }
}

function update() {
  const world = worldAt(state.cursor);
  updateTimeline();
  updateGraph(world);
  renderDetails(world);

  const status = $('#status');
  const done = state.events.find((e) => e.ev === 'done');
  if (done) {
    status.innerHTML = '';
    const pill = el(null, 'span', { class: `pill ${done.err ? 'pill-danger' : 'pill-ok'}` });
    pill.textContent = done.err ? done.str : 'ok';
    pill.title = done.err ? `err ${done.err}` : '';
    status.append(pill);
  }
}

function setCursor(index) {
  state.cursor = Math.max(-1, Math.min(index, state.events.length - 1));
  update();
}

function select(key) {
  state.selected = state.selected === key ? null : key;
  update();
}

function nextStep(direction) {
  let index = state.cursor + direction;
  while (index >= 0 && index < state.events.length && state.events[index].ev !== 'step') index += direction;
  setCursor(index < 0 ? -1 : Math.min(index, state.events.length - 1));
}

function load(text, name) {
  const { model, events } = parseJournal(text);
  if (!model.meta) {
    alert(`${name}: not a fuzz_dag journal (no meta line)`);
    return;
  }
  state.model = model;
  state.events = events;
  state.cursor = events.length - 1;
  state.selected = null;
  state.graph = buildGraph(model);

  state.groups = [];
  state.groupSize = new Map();
  state.collapsed = new Set();
  let group = -1;
  events.forEach((event, index) => {
    if (event.ev === 'step') group = index;
    else if (event.ev === 'pass' || event.ev === 'done' || (event.ev === 'check' && event.kind === 'schedule')) group = -1;
    state.groups[index] = group;
    if (group >= 0 && group !== index) state.groupSize.set(group, (state.groupSize.get(group) ?? 0) + 1);
    if (event.ev === 'step') state.collapsed.add(index);
  });

  $('#empty').hidden = true;
  $('#shell').hidden = false;
  $('#meta').textContent = `${name} · iter ${model.meta.iter} · seed ${model.meta.seed}`;
  $('#scrub').max = events.length - 1;

  renderGraph();
  renderTimeline();

  const firstBad = events.findIndex((e) => e.ev === 'check' && e.rows.some((row) => !row.ok));
  setCursor(firstBad >= 0 ? firstBad : events.length - 1);
}

function loadFile(file) {
  file.text().then((text) => load(text, file.name));
}

$('#file').addEventListener('change', (event) => {
  if (event.target.files[0]) loadFile(event.target.files[0]);
});

document.addEventListener('dragover', (event) => {
  event.preventDefault();
  $('#empty').classList.add('is-drop');
});
document.addEventListener('dragleave', () => $('#empty').classList.remove('is-drop'));
document.addEventListener('drop', (event) => {
  event.preventDefault();
  $('#empty').classList.remove('is-drop');
  if (event.dataTransfer.files[0]) loadFile(event.dataTransfer.files[0]);
});

$('#prev').addEventListener('click', () => setCursor(state.cursor - 1));
$('#next').addEventListener('click', () => setCursor(state.cursor + 1));
$('#scrub').addEventListener('input', (event) => setCursor(Number(event.target.value)));

document.addEventListener('keydown', (event) => {
  if (!state.model) return;
  if (event.key === 'ArrowLeft') { event.shiftKey ? nextStep(-1) : setCursor(state.cursor - 1); event.preventDefault(); }
  if (event.key === 'ArrowRight') { event.shiftKey ? nextStep(1) : setCursor(state.cursor + 1); event.preventDefault(); }
  if (event.key === 'Escape') select(state.selected);
});

installPanZoom($('#graph'));
