// render_move_graph.cpp

// Writes one standalone HTML page drawing the forward and backward move
// graphs, for every side, of each game named, as GameBase builds them
// (after split_joins and optimize). The page lays the graphs out with
// dagre from cdnjs, so viewing it needs network access.

#include <format>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "GameBase.h"
#include "MoveGraph.h"
#include "game_utils.h"

static const char *page_template = R"HTML(<title>Move Graph Joins</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=IBM+Plex+Mono:wght@400;500&family=IBM+Plex+Sans+Condensed:wght@400;500;600&display=swap">
<style>
/* Layout: control strip over a scrollable left-to-right DAG, with a node inspector beside it (stacked below on phones). */
:root {
  --bg: #f5f6f4;
  --surface: #ffffff;
  --ink: #1d2422;
  --muted: #5e6a66;
  --line: #c9d0cd;
  --edge: #9aa6a1;
  --move: #e6ebe9;
  --move-ink: #2b3532;
  --join: #c7672a;
  --join-soft: #f6e2d4;
  --term: #2f5d7c;
  --term-soft: #dbe7ef;
  --hi: #c7672a;
  --font-ui: "IBM Plex Sans Condensed", "Arial Narrow", system-ui, sans-serif;
  --font-mono: "IBM Plex Mono", ui-monospace, Menlo, Consolas, monospace;
}
@media (prefers-color-scheme: dark) {
  :root:not([data-theme="light"]) {
    --bg: #141917; --surface: #1c2220; --ink: #e3e8e6; --muted: #98a5a0; --line: #34403c;
    --edge: #55635e; --move: #26302d; --move-ink: #d5ddda; --join: #e08a4f; --join-soft: #3d2a1d;
    --term: #7fb0d1; --term-soft: #1d3140; --hi: #e08a4f; color-scheme: dark;
  }
}
:root[data-theme="dark"] {
  --bg: #141917; --surface: #1c2220; --ink: #e3e8e6; --muted: #98a5a0; --line: #34403c;
  --edge: #55635e; --move: #26302d; --move-ink: #d5ddda; --join: #e08a4f; --join-soft: #3d2a1d;
  --term: #7fb0d1; --term-soft: #1d3140; --hi: #e08a4f; color-scheme: dark;
}
* { box-sizing: border-box; }
body { background: var(--bg); color: var(--ink); font-family: var(--font-ui); font-size: 15px; line-height: 1.45; }
.wrap { padding-inline: 16px; padding-block: 20px 28px; max-width: 1400px; margin: 0 auto; display: flex; flex-direction: column; gap: 16px; }
header { display: flex; flex-wrap: wrap; align-items: baseline; justify-content: space-between; gap: 8px 24px; }
h1 { font-size: 1.6rem; font-weight: 600; margin: 0; letter-spacing: -0.01em; text-wrap: balance; }
.sub { color: var(--muted); margin: 0; max-width: 70ch; }
.controls { display: flex; flex-wrap: wrap; gap: 10px 16px; align-items: center; }
.seg { display: inline-flex; border: 1px solid var(--line); border-radius: 6px; overflow: hidden; background: var(--surface); }
.seg button { font: inherit; font-size: 0.9rem; border: 0; background: transparent; color: var(--ink); padding: 6px 12px; cursor: pointer; }
.seg button + button { border-left: 1px solid var(--line); }
.seg button[aria-pressed="true"] { background: var(--ink); color: var(--bg); }
.seg button:focus-visible, .tool:focus-visible, input:focus-visible, .node:focus-visible { outline: 2px solid var(--hi); outline-offset: 2px; }
.label { font-size: 0.75rem; text-transform: uppercase; letter-spacing: 0.08em; color: var(--muted); }
.tool { font: inherit; font-size: 0.9rem; border: 1px solid var(--line); border-radius: 6px; background: var(--surface); color: var(--ink); padding: 5px 10px; cursor: pointer; }
input[type="search"] { font: inherit; font-family: var(--font-mono); font-size: 0.85rem; border: 1px solid var(--line); border-radius: 6px; background: var(--surface); color: var(--ink); padding: 5px 10px; width: 14rem; max-width: 100%; }
.stats { display: flex; flex-wrap: wrap; gap: 8px 28px; font-variant-numeric: tabular-nums; }
.stat b { font-size: 1.25rem; font-weight: 600; display: block; }
.splits { font-family: var(--font-mono); font-size: 0.8rem; color: var(--muted); margin: 0; display: flex; flex-direction: column; gap: 2px; overflow-wrap: anywhere; }
.legend { display: flex; flex-wrap: wrap; gap: 6px 18px; font-size: 0.85rem; color: var(--muted); }
.swatch { display: inline-block; width: 12px; height: 12px; border-radius: 3px; vertical-align: -1px; margin-right: 6px; border: 1px solid var(--line); }
.main { display: grid; grid-template-columns: minmax(0, 1fr) 320px; gap: 16px; align-items: start; }
@media (max-width: 860px) { .main { grid-template-columns: minmax(0, 1fr); } }
.canvas { background: var(--surface); border: 1px solid var(--line); border-radius: 8px; overflow: auto; height: 72vh; min-height: 360px; }
.canvas svg { display: block; }
.inspector { background: var(--surface); border: 1px solid var(--line); border-radius: 8px; padding: 14px 16px; display: flex; flex-direction: column; gap: 12px; min-width: 0; max-height: 72vh; overflow: auto; }
.inspector h2 { font-family: var(--font-mono); font-size: 0.95rem; font-weight: 500; margin: 0; overflow-wrap: anywhere; }
.inspector ul { list-style: none; margin: 0; padding: 0; display: flex; flex-direction: column; gap: 3px; }
.inspector li { font-family: var(--font-mono); font-size: 0.78rem; overflow-wrap: anywhere; }
.inspector li button { font: inherit; background: none; border: 0; padding: 0; color: var(--ink); cursor: pointer; text-align: left; text-decoration: underline; text-decoration-color: var(--line); text-underline-offset: 2px; }
.inspector .note { color: var(--muted); font-size: 0.85rem; margin: 0; }
.kind { display: inline-block; font-size: 0.72rem; text-transform: uppercase; letter-spacing: 0.08em; padding: 1px 8px; border-radius: 999px; }
.kind.join { background: var(--join-soft); color: var(--join); }
.kind.move { background: var(--move); color: var(--move-ink); }
.kind.term { background: var(--term-soft); color: var(--term); }
svg text { font-family: var(--font-mono); font-size: 10.5px; }
.edge { fill: none; stroke: var(--edge); stroke-width: 1; opacity: 0.55; }
.edge.hi { stroke: var(--hi); stroke-width: 1.8; opacity: 1; }
.edge.dim { opacity: 0.12; }
.node rect { stroke: var(--line); stroke-width: 1; }
.node.move rect { fill: var(--move); }
.node.move text { fill: var(--move-ink); }
.node.join rect { fill: var(--join-soft); stroke: var(--join); }
.node.join text { fill: var(--join); }
.node.term rect { fill: var(--term-soft); stroke: var(--term); }
.node.term text { fill: var(--term); font-weight: 500; }
.node { cursor: pointer; }
.node.dim { opacity: 0.25; }
.node.sel rect { stroke: var(--hi); stroke-width: 2.4; }
.node.match rect { stroke: var(--hi); stroke-width: 2; stroke-dasharray: 3 2; }
.foot { color: var(--muted); font-size: 0.85rem; margin: 0; max-width: 80ch; }
code { font-family: var(--font-mono); font-size: 0.88em; }
@media (prefers-reduced-motion: no-preference) { .edge, .node { transition: opacity 120ms; } }
</style>

<div class="wrap">
  <header>
    <div>
      <h1>Move graph joins</h1>
      <p class="sub">The move graphs <code>GameBase</code> builds, after <code>split_joins</code> and <code>optimize</code>, as written by <code>render_move_graph</code>. Inputs flow left to right, from the first node to the output node.</p>
    </div>
  </header>

  <div class="controls">
    <span class="label">Game</span>
    <div class="seg" id="game-seg"></div>
    <span class="label">Direction</span>
    <div class="seg" id="dir-seg"></div>
    <span class="label" id="side-label">Side to move</span>
    <div class="seg" id="side-seg"></div>
    <input type="search" id="search" placeholder="Find node, e.g. join_0_8" aria-label="Find node by name">
    <button class="tool" id="zoom-out" type="button" aria-label="Zoom out">−</button>
    <button class="tool" id="zoom-in" type="button" aria-label="Zoom in">+</button>
    <button class="tool" id="zoom-fit" type="button">Fit height</button>
  </div>

  <div class="stats" id="stats"></div>
  <p class="splits" id="splits"></p>
  <div class="legend">
    <span><span class="swatch" style="background:var(--term-soft);border-color:var(--term)"></span>first and output node</span>
    <span><span class="swatch" style="background:var(--move)"></span>move or game node</span>
    <span><span class="swatch" style="background:var(--join-soft);border-color:var(--join)"></span>join node from <code>split_joins</code></span>
  </div>

  <div class="main">
    <div class="canvas" id="canvas" aria-label="Move graph diagram"></div>
    <aside class="inspector" id="inspector" aria-live="polite"></aside>
  </div>

  <p class="foot"><span id="fan-in-note"></span> A join named <code>&lt;target&gt;.join_&lt;start&gt;_&lt;end&gt;</code> covers the half-open range of the target's incoming edges, sorted by edge name. Select a node to see its inputs, outputs and changes, and to trace everything upstream and downstream of it.</p>
</div>

<script src="https://cdnjs.cloudflare.com/ajax/libs/dagre/0.8.5/dagre.min.js"></script>
<script>
const DATA = __DATA__;
const META = __META__;
const OUTPUT_NAMES = { forward: "end", backward: "begin" };

const state = { game: Object.keys(DATA)[0], dir: "backward", side: 0, zoom: 1, selected: null, search: "" };
try { const s = JSON.parse(localStorage.getItem("mgj-view") || "{}"); if (DATA[s.game]) state.game = s.game; if (s.dir === "forward" || s.dir === "backward") state.dir = s.dir; if (Number.isInteger(s.side) && s.side >= 0 && s.side < DATA[state.game].sides) state.side = s.side; } catch (e) {}

const canvas = document.getElementById("canvas");
const inspector = document.getElementById("inspector");
let view = null;

function kindOf(name, index, count) {
  if (name.includes(".join_")) return "join";
  if (index === 0 || index === count - 1) return "term";
  return "move";
}

function buildView() {
  const nodes = DATA[state.game][state.dir][state.side];
  const count = nodes.length;
  const outputs = nodes.map(() => []);
  nodes.forEach((n, i) => n.inputs.forEach((e) => outputs[e.from].push({ to: i, name: e.name, conditions: e.conditions })));

  const g = new dagre.graphlib.Graph({ multigraph: true });
  g.setGraph({ rankdir: "LR", nodesep: 5, ranksep: 70, edgesep: 2, marginx: 16, marginy: 16 });
  g.setDefaultEdgeLabel(() => ({}));
  nodes.forEach((n, i) => g.setNode(String(i), { width: Math.max(40, n.name.length * 6.4 + 14), height: 18 }));
  nodes.forEach((n, i) => n.inputs.forEach((e, k) => g.setEdge(String(e.from), String(i), {}, e.name)));
  dagre.layout(g);

  view = { nodes, outputs, g, count };
  render();
}

function closure(start, step) {
  const seen = new Set([start]);
  const stack = [start];
  while (stack.length) { const x = stack.pop(); for (const y of step(x)) if (!seen.has(y)) { seen.add(y); stack.push(y); } }
  return seen;
}

function render() {
  const { nodes, outputs, g, count } = view;
  const gw = g.graph().width, gh = g.graph().height;
  const z = state.zoom;
  const svgNS = "http://www.w3.org/2000/svg";
  const svg = document.createElementNS(svgNS, "svg");
  svg.setAttribute("viewBox", `0 0 ${gw} ${gh}`);
  svg.setAttribute("width", gw * z);
  svg.setAttribute("height", gh * z);
  svg.setAttribute("role", "img");
  svg.setAttribute("aria-label", `${state.game} ${state.dir} side_to_move=${state.side} move graph, ${count} nodes`);

  let up = null, down = null;
  if (state.selected !== null) {
    up = closure(state.selected, (x) => nodes[x].inputs.map((e) => e.from));
    down = closure(state.selected, (x) => outputs[x].map((e) => e.to));
  }
  const q = state.search.trim().toLowerCase();

  const edgeLayer = document.createElementNS(svgNS, "g");
  g.edges().forEach((ed) => {
    const pts = g.edge(ed).points;
    const p = document.createElementNS(svgNS, "path");
    let d = `M${pts[0].x},${pts[0].y}`;
    for (let i = 1; i < pts.length; i++) d += ` L${pts[i].x},${pts[i].y}`;
    p.setAttribute("d", d);
    const from = +ed.v, to = +ed.w;
    let cls = "edge";
    if (state.selected !== null) {
      if (from === state.selected || to === state.selected) cls += " hi";
      else if (!((up.has(from) && up.has(to)) || (down.has(from) && down.has(to)))) cls += " dim";
    }
    p.setAttribute("class", cls);
    edgeLayer.appendChild(p);
  });
  svg.appendChild(edgeLayer);

  const nodeLayer = document.createElementNS(svgNS, "g");
  nodes.forEach((n, i) => {
    const box = g.node(String(i));
    const grp = document.createElementNS(svgNS, "g");
    let cls = "node " + kindOf(n.name, i, count);
    if (state.selected !== null) {
      if (i === state.selected) cls += " sel";
      else if (!up.has(i) && !down.has(i)) cls += " dim";
    }
    if (q && n.name.toLowerCase().includes(q)) cls += " match";
    grp.setAttribute("class", cls);
    grp.setAttribute("tabindex", "0");
    grp.setAttribute("role", "button");
    grp.setAttribute("aria-label", n.name);
    const r = document.createElementNS(svgNS, "rect");
    r.setAttribute("x", box.x - box.width / 2); r.setAttribute("y", box.y - box.height / 2);
    r.setAttribute("width", box.width); r.setAttribute("height", box.height); r.setAttribute("rx", 3);
    const t = document.createElementNS(svgNS, "text");
    t.setAttribute("x", box.x); t.setAttribute("y", box.y + 3.6); t.setAttribute("text-anchor", "middle");
    t.textContent = n.name;
    grp.append(r, t);
    grp.addEventListener("click", () => select(i));
    grp.addEventListener("keydown", (ev) => { if (ev.key === "Enter" || ev.key === " ") { ev.preventDefault(); select(i); } });
    nodeLayer.appendChild(grp);
  });
  svg.appendChild(nodeLayer);

  canvas.replaceChildren(svg);
  renderStats();
  renderInspector();
}

function renderStats() {
  const { nodes, count } = view;
  const joins = nodes.filter((n) => n.name.includes(".join_"));
  const edges = nodes.reduce((a, n) => a + n.inputs.length, 0);
  const maxIn = Math.max(...nodes.map((n) => n.inputs.length));
  const items = [["Nodes", count], ["Edges", edges], ["Join nodes", joins.length], ["Largest fan-in", maxIn]];
  document.getElementById("stats").innerHTML = items.map(([k, v]) => `<div class="stat"><span class="label">${k}</span><b>${v}</b></div>`).join("");

  const targets = {};
  joins.forEach((n) => {
    const m = n.name.match(/^(.*)\.join_(\d+)_(\d+)$/);
    if (!m) return;
    const t = targets[m[1]] || (targets[m[1]] = { joins: 0, edges: 0 });
    t.joins++; t.edges = Math.max(t.edges, +m[3]);
  });
  const lines = Object.entries(targets).map(([t, v]) => `${t}: ${v.edges} incoming edges into ${v.joins} joins, then ${nodes.find((n) => n.name === t).inputs.length} inputs into ${t}`);
  document.getElementById("splits").innerHTML = lines.length ? lines.map((l) => `<span>${escapeHtml(l)}</span>`).join("") : `<span>No node over max fan-in ${META.max_fan_in}, so no joins.</span>`;
}

function escapeHtml(s) { return s.replace(/[&<>"]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c])); }

function nodeButton(i, extra) {
  return `<li><button type="button" data-node="${i}">${escapeHtml(view.nodes[i].name)}</button>${extra ? ` <span class="note">${escapeHtml(extra)}</span>` : ""}</li>`;
}

function renderInspector() {
  const { nodes, outputs, count } = view;
  if (state.selected === null) {
    inspector.innerHTML = `<span class="label">Inspector</span><p class="note">Select a node in the diagram, or find one by name. The output node is <code>${OUTPUT_NAMES[state.dir]}</code>.</p>` +
      `<span class="label">Join nodes</span><ul>${nodes.map((n, i) => n.name.includes(".join_") ? nodeButton(i, `${n.inputs.length} inputs`) : "").join("")}</ul>`;
  } else {
    const i = state.selected, n = nodes[i], kind = kindOf(n.name, i, count);
    const kindLabel = { join: "join", term: i === 0 ? "first node" : "output node", move: "node" }[kind];
    const changes = n.changes.length ? n.changes.map(([l, a, b]) => `<li>layer ${l}: ${a} → ${b}</li>`).join("") : `<li class="note">no change</li>`;
    let upCount = closure(i, (x) => nodes[x].inputs.map((e) => e.from)).size - 1;
    inspector.innerHTML =
      `<div><span class="kind ${kind}">${kindLabel}</span></div><h2>${escapeHtml(n.name)}</h2>` +
      `<div><span class="label">Changes</span><ul>${changes}</ul></div>` +
      `<div><span class="label">Inputs (${n.inputs.length})</span><ul>${n.inputs.map((e) => nodeButton(e.from, `via ${e.name}, ${e.conditions} condition${e.conditions === 1 ? "" : "s"}`)).join("") || '<li class="note">none</li>'}</ul></div>` +
      `<div><span class="label">Outputs (${outputs[i].length})</span><ul>${outputs[i].map((e) => nodeButton(e.to, `via ${e.name}`)).join("") || '<li class="note">none</li>'}</ul></div>` +
      `<p class="note">${upCount} node${upCount === 1 ? "" : "s"} upstream.</p>` +
      `<button class="tool" type="button" id="clear">Clear selection</button>`;
    document.getElementById("clear").addEventListener("click", () => { state.selected = null; render(); });
  }
  inspector.querySelectorAll("button[data-node]").forEach((b) => b.addEventListener("click", () => { select(+b.dataset.node); scrollToNode(+b.dataset.node); }));
}

function select(i) { state.selected = state.selected === i ? null : i; render(); }

function scrollToNode(i) {
  const box = view.g.node(String(i));
  canvas.scrollTo({ left: box.x * state.zoom - canvas.clientWidth / 2, top: box.y * state.zoom - canvas.clientHeight / 2, behavior: "smooth" });
}

function segment(el, options, current, onPick) {
  el.innerHTML = options.map(([v, label]) => `<button type="button" data-v="${v}" aria-pressed="${v === current}">${label}</button>`).join("");
  el.querySelectorAll("button").forEach((b) => b.addEventListener("click", () => onPick(b.dataset.v)));
}

function save() { try { localStorage.setItem("mgj-view", JSON.stringify({ game: state.game, dir: state.dir, side: state.side })); } catch (e) {} }

function renderControls() {
  segment(document.getElementById("game-seg"), Object.keys(DATA).map((k) => [k, k]), state.game, (v) => { state.game = v; state.side = 0; state.selected = null; save(); renderControls(); buildView(); });
  segment(document.getElementById("dir-seg"), [["forward", "Forward"], ["backward", "Backward"]], state.dir, (v) => { state.dir = v; state.selected = null; save(); renderControls(); buildView(); });
  const sides = DATA[state.game].sides;
  const sideSeg = document.getElementById("side-seg");
  sideSeg.hidden = sides < 2;
  document.getElementById("side-label").hidden = sides < 2;
  segment(sideSeg, Array.from({ length: sides }, (_, i) => [String(i), String(i)]), String(state.side), (v) => { state.side = +v; state.selected = null; save(); renderControls(); buildView(); });
}

document.getElementById("zoom-in").addEventListener("click", () => { state.zoom = Math.min(3, state.zoom * 1.25); render(); });
document.getElementById("zoom-out").addEventListener("click", () => { state.zoom = Math.max(0.15, state.zoom / 1.25); render(); });
document.getElementById("zoom-fit").addEventListener("click", () => { state.zoom = Math.max(0.15, (canvas.clientHeight - 4) / view.g.graph().height); render(); });
document.getElementById("search").addEventListener("input", (ev) => {
  state.search = ev.target.value;
  render();
  const q = state.search.trim().toLowerCase();
  const hit = q ? view.nodes.findIndex((n) => n.name.toLowerCase().includes(q)) : -1;
  if (hit >= 0) scrollToNode(hit);
});

document.getElementById("fan-in-note").textContent = `Max fan-in is ${META.max_fan_in} (${META.max_fan_in_source}). `;
renderControls();
buildView();
</script>
)HTML";

static nlohmann::json graph_to_json(const MoveGraph& graph, size_t& edges)
{
  nlohmann::json nodes = nlohmann::json::array();
  for(int i = 0; i < int(graph.size()); ++i)
    {
      nlohmann::json changes = nlohmann::json::array();
      const change_vector& node_changes = graph.get_node_changes(i);
      for(size_t layer = 0; layer < node_changes.size(); ++layer)
	{
	  if(node_changes[layer])
	    {
	      changes.push_back({layer, node_changes[layer]->first, node_changes[layer]->second});
	    }
	}

      nlohmann::json inputs = nlohmann::json::array();
      for(const auto& [from_node_index, edge] : graph.get_node_inputs(i))
	{
	  inputs.push_back({{"from", from_node_index},
			    {"name", std::get<0>(edge)},
			    {"conditions", std::get<1>(edge).size()}});
	  ++edges;
	}

      nodes.push_back({{"name", graph.get_node_name(i)}, {"changes", changes}, {"inputs", inputs}});
    }

  return nodes;
}

// JSON placed inside a <script> element must not contain "</", or a
// node name could end the element early.
static std::string script_safe(std::string json)
{
  for(size_t pos = json.find("</"); pos != std::string::npos; pos = json.find("</", pos + 3))
    {
      json.replace(pos, 2, "<\\/");
    }
  return json;
}

static void replace_once(std::string& text, const std::string& placeholder, const std::string& value)
{
  size_t pos = text.find(placeholder);
  if((pos == std::string::npos) || (text.find(placeholder, pos + 1) != std::string::npos))
    {
      throw std::logic_error("page template must contain " + placeholder + " exactly once");
    }
  text.replace(pos, placeholder.size(), value);
}

int main(int argc, char **argv)
{
  if(argc < 3)
    {
      std::cerr << "usage: " << argv[0] << " OUTPUT.html GAME [GAME...]" << std::endl;
      return 1;
    }

  std::string output_path(argv[1]);

  nlohmann::json data = nlohmann::json::object();
  std::vector<std::string> summaries;
  for(int a = 2; a < argc; ++a)
    {
      std::string game_name(argv[a]);
      if(data.contains(game_name))
	{
	  throw std::invalid_argument("game " + game_name + " listed twice");
	}

      std::unique_ptr<GameBase> game(get_gamebase(game_name));
      int sides = game->get_sides();
      data[game_name]["sides"] = sides;

      for(std::string direction : {"forward", "backward"})
	{
	  nlohmann::json per_side = nlohmann::json::array();
	  for(int side_to_move = 0; side_to_move < sides; ++side_to_move)
	    {
	      const MoveGraph& graph = (direction == "forward") ? game->get_move_graph_forward(side_to_move) : game->get_move_graph_backward(side_to_move);
	      size_t edges = 0;
	      per_side.push_back(graph_to_json(graph, edges));
	      summaries.push_back(std::format("{:s} {:s} side_to_move={:d}: {:d} nodes, {:d} edges", game_name, direction, side_to_move, graph.size(), edges));
	    }
	  data[game_name][direction] = per_side;
	}
    }

  nlohmann::json meta = {{"max_fan_in", move_graph_max_fan_in()},
			 {"max_fan_in_source", move_graph_max_fan_in_is_default() ? "default" : "DFA_MOVE_GRAPH_MAX_FAN_IN"}};

  std::string page(page_template);
  replace_once(page, "__DATA__", script_safe(data.dump()));
  replace_once(page, "__META__", script_safe(meta.dump()));

  std::ofstream output(output_path, std::ios::binary);
  if(!output)
    {
      throw std::runtime_error("could not open " + output_path + " for writing");
    }
  output << page;
  output.close();
  if(!output)
    {
      throw std::runtime_error("error writing " + output_path);
    }

  for(const std::string& summary : summaries)
    {
      std::cout << summary << std::endl;
    }
  std::cout << "wrote " << page.size() << " bytes to " << output_path << std::endl;

  return 0;
}
