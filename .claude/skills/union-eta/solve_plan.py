#!/usr/bin/env python3
"""When each remaining union of a running solve will finish, and where it
needs gc to keep scratch free space above a floor.

usage: solve_plan.py GAME [--all] [--floor TIB] [--trace FILE] [--log FILE]

Runs from src/ (found relative to this file), like union_eta.py, and reads
only the log, the trace, and scratch's links, DFA headers and free space.

The join tree is rebuilt from the log's "node begin fan-in N -> ... (max
fan-in K)" line the way MoveGraph::split_joins builds it: edges sorted by
name, aligned chunks of K, K^2, ... until begin has at most K inputs, built
depth first. Finished unions show their actual minutes and result. The rest
are projected from the finished joins of this solve, by the number of moves
each covers:

- result states: the input states times the median result/sum ratio;
- forward unique pairs: the input states times the median pairs/input ratio;
- minutes: pairs (G) x (c0 + c1 x max(0, pairs (G) - KNEE_GPAIRS)), fitted
  to the unions finished at the latest code version;
- peak working space: PEAK_BYTES_PER_PAIR per forward unique pair;
- free space lost until the next gc: the result's DFA file.

A span with no finished join uses the largest finished span below it. A
union gets a gc stop before it when its low point, free space less
PEAK_MARGIN times its peak, would be under the floor. A gc keeps the DFAs
linked from scratch outside the caches, and frees the rest.

The union in progress takes its ETA from union_eta.py.

Exits nonzero, with the reason, on anything it does not recognize.
"""

import argparse
import datetime
import os
import re
import shutil
import statistics
import subprocess
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "solve-breakdown"))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import solve_breakdown as sb  # noqa: E402
import union_eta as ue  # noqa: E402

TIB = 2**40
# Files in scratch/binarydfa during join_32_48's backward layer 39 (pairs
# files, transition pairs, next pair outputs, rank outputs) came to about
# 2.5 times the forward pass's pairs files, at 8 bytes per unique pair.
PEAK_BYTES_PER_PAIR = 2.5 * 8
# Peaks are projected, so gc is planned for this multiple of each.
PEAK_MARGIN = 1.3
# Minutes per G unique pairs are flat up to about this many G pairs, and
# grow linearly above it.
KNEE_GPAIRS = 20
# Bytes per state of a DFA file: 8 for the two-move joins' results of about
# 4G states, 13.2 for join_0_32 and join_32_48 at 33-35G.
SMALL_DFA_STATES = 5e9
SMALL_BYTES_PER_STATE = 8.0
LARGE_BYTES_PER_STATE = 13.2
GC_MINUTES = 5


def dfa_bytes(states):
    return states * (SMALL_BYTES_PER_STATE if states < SMALL_DFA_STATES else LARGE_BYTES_PER_STATE)


def join_tree(lines):
    """(edge count, top items, joins in build order). An item is (start,
    end) of the sorted edges; a join is (start, end, children)."""
    found = [l for l in lines if re.match(r"move graph backward side_to_move=\d+: node begin fan-in \d+ -> ", l)]
    if not found:
        sb.fail("no \"move graph backward ...: node begin fan-in\" line in the log")
    m = re.search(r"node begin fan-in (\d+) -> .*\(max fan-in (\d+)\)$", found[-1])
    if not m:
        sb.fail(f"unrecognized fan-in line: {found[-1]}")
    n, k = int(m.group(1)), int(m.group(2))
    items = [(i, i + 1) for i in range(n)]
    children = {}
    chunk = 1
    while len(items) > k:
        chunk *= k
        nxt, i = [], 0
        while i < len(items):
            j = i + 1
            while j < len(items) and items[j][0] // chunk == items[i][0] // chunk:
                j += 1
            if j - i == 1:
                nxt.append(items[i])
            else:
                item = (items[i][0], items[j - 1][1])
                children[item] = items[i:j]
                nxt.append(item)
            i = j
        items = nxt
    order = []

    def visit(item):
        if item in children:
            for c in children[item]:
                visit(c)
            order.append((item[0], item[1], children[item]))
    for item in items:
        visit(item)
    return n, items, order


def name_of(item, n):
    return "begin" if item == (0, n) else f"begin.join_{item[0]}_{item[1]}"


def gc_kept_bytes(prefix):
    """Bytes of the DFAs linked from scratch outside the caches and this
    solve's move nodes, which a gc keeps whatever the solve does next."""
    scratch = os.path.join(sb.SRC_DIR, "scratch")
    kept = set()
    for d in os.listdir(scratch):
        if d.endswith("_cache") or d in ("dfas_by_hash", "binarydfa", "build"):
            continue
        for root, _, files in os.walk(os.path.join(scratch, d)):
            for f in files:
                p = os.path.join(root, f)
                if d == "move_nodes" and f.startswith(prefix + ",node="):
                    continue
                if os.path.islink(p) and os.path.exists(p):
                    kept.add(os.path.realpath(p))
    return sum(os.path.getsize(p) for p in kept)


def working_bytes():
    """Bytes allocated to the files of builds in progress."""
    total = 0
    for d in ("binarydfa", "build"):
        for root, _, files in os.walk(os.path.join(sb.SRC_DIR, "scratch", d)):
            for f in files:
                try:
                    total += os.lstat(os.path.join(root, f)).st_blocks * 512
                except FileNotFoundError:
                    pass
    return total


def ratio_for(table, span):
    spans = [s for s in table if s <= span]
    if not spans:
        sb.fail(f"no finished join over {span} moves or fewer to project from")
    return table[max(spans)]


def fit_minutes(points):
    """c0, c1 of minutes = g (c0 + c1 max(0, g - KNEE_GPAIRS)), least
    squares over (G pairs, minutes)."""
    xs = [(g, g * max(0.0, g - KNEE_GPAIRS)) for g, _ in points]
    ys = [m for _, m in points]
    a11 = sum(x[0] * x[0] for x in xs)
    a12 = sum(x[0] * x[1] for x in xs)
    a22 = sum(x[1] * x[1] for x in xs)
    b1 = sum(x[0] * y for x, y in zip(xs, ys))
    b2 = sum(x[1] * y for x, y in zip(xs, ys))
    det = a11 * a22 - a12 * a12
    if det == 0:
        sb.fail("the finished unions do not determine the time model; it needs some above and below the knee")
    return (b1 * a22 - b2 * a12) / det, (a11 * b2 - a12 * b1) / det


def union_eta_finish(game, args):
    """The ETA union_eta.py gives the union in progress, as epoch seconds,
    or None."""
    cmd = [sys.executable, os.path.join(os.path.dirname(os.path.abspath(__file__)), "union_eta.py"), game, "--trace", args.trace, "--log", args.log]
    p = subprocess.run(cmd, capture_output=True, text=True)
    if p.returncode != 0:
        sb.fail(f"union_eta.py failed: {p.stderr.strip()}")
    m = re.search(r"^ETA (\w{3}) (\d\d):(\d\d), range", p.stdout, re.M)
    if not m:
        return None
    now = datetime.datetime.now()
    for d in range(-1, 8):
        t = (now + datetime.timedelta(days=d)).replace(hour=int(m.group(2)), minute=int(m.group(3)), second=0, microsecond=0)
        if t.strftime("%a") == m.group(1) and t > now - datetime.timedelta(days=1):
            return t.timestamp()
    sb.fail(f"could not place union_eta.py's ETA {m.group(0)}")


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("game")
    parser.add_argument("--all", action="store_true", help="list every union, not only those over 8 moves or more")
    parser.add_argument("--floor", type=float, default=1.0, help="free space floor in TiB (default 1)")
    parser.add_argument("--ratios", action="store_true", help="print the result/sum and unique pairs/input ratios by moves per join, and stop")
    parser.add_argument("--trace")
    parser.add_argument("--log")
    args = parser.parse_args()
    sb.default_paths(args)

    lines = sb.read_log(args.log)
    # Finished unions are listed from the last stop for gc on.
    last_gc = 0.0
    for line in lines:
        m = re.match(r"=== build_bound_backward stopped by request at (\d{4}-\d\d-\d\d \d\d:\d\d:\d\d) .*for gc", line)
        if m:
            last_gc = time.mktime(time.strptime(m.group(1), "%Y-%m-%d %H:%M:%S"))
    n, tops, order = join_tree(lines)
    all_builds = ue.builds(lines)
    if not all_builds:
        sb.fail("no join build in the log")
    prefix = all_builds[-1]["prefix"]

    # State counts of this solve's nodes, from the log.
    states = {}
    for line in lines:
        m = re.match(r"(?:built|loaded) move_nodes/(.*),node=(\S+) => (\d+) states$", line)
        if m and m.group(1) == prefix:
            states[m.group(2)] = int(m.group(3))
    builds = {}
    for b in all_builds:
        if b["prefix"] == prefix:
            builds[b["node"]] = b
    done = {node for node, b in builds.items() if b["built"]}
    current = [b for b in all_builds if b["prefix"] == prefix and not b["built"]]
    current = current[-1]["node"] if current and current[-1] is all_builds[-1] else None

    # The edges are sorted by edge name, which the log does not give, so
    # the edges this solve's two-move joins logged are placed exactly. The
    # other move nodes the log names, in any solve, fill the other edges in
    # order of node name, which the logged edges follow but for swaps of
    # neighbours. Each has its states from its DFA header, or for one not
    # built yet, the median of those built.
    move_dir = os.path.join(sb.SRC_DIR, "scratch", "move_nodes")
    files = {f.split(",node=", 1)[1] for f in os.listdir(move_dir) if f.startswith(prefix + ",node=move_")}
    moves = sorted({m.group(1) for l in lines for m in [re.match(r"node \d+/\d+ \((move_[^)]*)\)$", l)] if m})
    if len(moves) != n:
        sb.fail(f"found {len(moves)} move node names in the log, but begin has fan-in {n}")
    # Edges this solve has not logged take their places from the latest
    # earlier solve, whose tree has the same edges in the same order.
    placed = {}
    earlier_prefix = next((b["prefix"] for b in reversed(all_builds) if b["prefix"] != prefix), None)
    for p in [prefix, earlier_prefix]:
        for b in all_builds:
            m = re.match(r"begin\.join_(\d+)_(\d+)$", b["node"])
            if b["prefix"] != p or not (m and int(m.group(2)) - int(m.group(1)) == 2):
                continue
            for i, e in b["edges"].items():
                if e not in moves:
                    sb.fail(f"{b['node']} edge {i} names {e}, which no node line in the log does")
                k = int(m.group(1)) + i
                if p == prefix:
                    placed[k] = e
                elif k not in placed and e not in placed.values():
                    placed[k] = e
    move_states = {e: ue.file_states(prefix, e) if e in files else states.get(e) for e in moves}
    known = [v for v in move_states.values() if v is not None]
    unbuilt = [e for e, v in move_states.items() if v is None]
    # A move node not built yet takes the same node's states in the latest
    # earlier solve that logged it, scaled by the median ratio of this
    # solve's built nodes to theirs there, or without one, the median of
    # those built.
    earlier = {}
    for line in lines:
        m = re.match(r"(?:built|loaded) move_nodes/(.*),node=(move_\S+) => (\d+) states", line)
        if m and m.group(1) != prefix:
            earlier[m.group(2)] = int(m.group(3))
    growth = [move_states[e] / earlier[e] for e in moves if move_states[e] is not None and earlier.get(e)]
    move_scale = statistics.median(growth) if growth else None
    scaled = [e for e in unbuilt if move_scale is not None and e in earlier]
    for e in unbuilt:
        move_states[e] = earlier[e] * move_scale if e in scaled else statistics.median(known)
    pool = [e for e in moves if e not in placed.values()]
    for i, e in zip([i for i in range(n) if i not in placed], pool):
        placed[i] = e

    sizes = [os.path.getsize(os.path.join(move_dir, prefix + ",node=" + e)) for e in files]
    move_file_mean = statistics.mean(sizes) if sizes else 0

    def leaf_states(i):
        return move_states[placed[i]]

    def inputs_of(item, kids, size):
        if kids[0][1] - kids[0][0] == 1:
            return [leaf_states(k[0]) for k in kids]
        return [size[name_of(k, n)] for k in kids]

    # Ratios by span, over the finished joins of each solve (each prefix,
    # one per ply), from the state counts the log gives each node and the
    # inputs each join's edge lines name.
    prefix_states = {}
    for line in lines:
        m = re.match(r"(?:built|loaded) move_nodes/(.*),node=(\S+) => (\d+) states", line)
        if m:
            prefix_states[(m.group(1), m.group(2))] = int(m.group(3))
    ratios_by_prefix = {}
    input_sum, span_of = {}, {}
    for b in all_builds:
        m = re.match(r"begin\.join_(\d+)_(\d+)$", b["node"])
        if not (m and b["built"] and b["edges"]):
            continue
        p, span = b["prefix"], int(m.group(2)) - int(m.group(1))
        ins = [prefix_states.get((p, e)) for e in b["edges"].values()]
        out = prefix_states.get((p, b["node"]))
        if None in ins or out is None:
            continue
        input_sum[(p, b["node"])], span_of[(p, b["node"])] = sum(ins), span
        result, pairs = ratios_by_prefix.setdefault(p, ({}, {}))
        result.setdefault(span, []).append(out / sum(ins))
        if b["unique"]:
            pairs.setdefault(span, []).append(sum(b["unique"].values()) / sum(ins))
    # This solve's ratios, and for a span it has not finished, those of the
    # latest earlier solve that has.
    by_span_result, by_span_pairs = {}, {}
    span_source = {}
    prefix_order = list(dict.fromkeys(b["prefix"] for b in all_builds))
    for p in [prefix] + [q for q in reversed(prefix_order) if q != prefix]:
        result, pairs = ratios_by_prefix.get(p, ({}, {}))
        for s, v in result.items():
            if s not in by_span_result:
                by_span_result[s] = v
                span_source[s] = "this solve" if p == prefix else "an earlier solve"
        for s, v in pairs.items():
            by_span_pairs.setdefault(s, v)
    if not by_span_result:
        sb.fail("no finished join with its input and result state counts in the log")
    result_ratio = {s: statistics.median(v) for s, v in by_span_result.items()}
    pairs_ratio = {s: statistics.median(v) for s, v in by_span_pairs.items()}

    # Minutes of the unions finished at the latest code version, in every
    # solve for the time fit, and in this one for the table.
    runs = sb.read_runs(args.trace)
    version, commits = sb.version_runs(runs, lines)
    build_of = {(b["prefix"], b["node"]): b for b in all_builds if b["built"]}
    wall_all = {}
    wall, finish = {}, {}
    for i in version:
        run, records = runs[i]
        for r in records:
            node = sb.union_node(r)
            if not node:
                continue
            p = r["name"][len("load_or_build move_nodes/"):].rsplit(",node=", 1)[0]
            wall_all[(p, node)] = r["wall_us"] / 6e7
            if p == prefix:
                wall[node] = r["wall_us"] / 6e7
                finish[node] = sb.run_start(run) + (r["start_us"] + r["wall_us"]) / 1e6
    points = [(sum(build_of[k]["unique"].values()) / 1e9, m) for k, m in wall_all.items() if k in build_of and build_of[k]["unique"]]
    c0, c1 = fit_minutes(points)
    max_fit_pairs = 1e9 * max(g for g, _ in points)

    # A union the latest earlier solve built takes its result and unique
    # pairs there, scaled by the median ratio of this solve's finished
    # unions to the same unions there, or before any has finished, by the
    # move nodes' ratio.
    earlier_out = {node: v for (p, node), v in prefix_states.items() if p == earlier_prefix and node.startswith("begin")}
    earlier_pairs = {b["node"]: sum(b["unique"].values()) for b in all_builds
                     if b["prefix"] == earlier_prefix and b["built"] and b["unique"]}
    out_growth = [states[node] / earlier_out[node] for node in done if node in states and earlier_out.get(node)]
    pairs_growth = [sum(builds[node]["unique"].values()) / earlier_pairs[node] for node in done
                    if builds[node]["unique"] and earlier_pairs.get(node)]
    out_scale = statistics.median(out_growth) if out_growth else move_scale
    pairs_scale = statistics.median(pairs_growth) if pairs_growth else out_scale
    if args.ratios:
        print(f"{args.game}: ratios by moves per join, over the finished joins of this solve, or for a span it has none, an earlier solve")
        print(f"  {'moves':>5} {'joins':>5}  {'result/sum median':>17} {'min':>5} {'max':>5}  {'unique pairs/input median':>25} {'min':>5} {'max':>5}  from")
        for s in sorted(by_span_result):
            r = by_span_result[s]
            p = by_span_pairs.get(s)
            pairs = f"{statistics.median(p):25.2f} {min(p):5.2f} {max(p):5.2f}" if p else f"{'':25} {'':5} {'':5}"
            print(f"  {s:5d} {len(r):5d}  {statistics.median(r):17.2f} {min(r):5.2f} {max(r):5.2f}  {pairs}  {span_source[s]}")
        print()
        print(f"{args.game}: minutes by moves per join, over the joins finished at the latest code version")
        print(f"  {'moves':>5} {'joins':>5}  {'minutes median':>14} {'min':>6} {'max':>6}"
              f"  {'min/G inputs median':>19} {'min':>5} {'max':>5}  {'min/G pairs median':>18} {'min':>5} {'max':>5}")
        by_span_wall = {}
        for k, m in wall_all.items():
            if k in span_of and k in build_of and build_of[k]["unique"]:
                by_span_wall.setdefault(span_of[k], []).append(
                    (m, m / (input_sum[k] / 1e9), m / (sum(build_of[k]["unique"].values()) / 1e9)))
        for s in sorted(by_span_wall):
            cols = list(zip(*by_span_wall[s]))
            print(f"  {s:5d} {len(cols[0]):5d}  {statistics.median(cols[0]):14.1f} {min(cols[0]):6.1f} {max(cols[0]):6.1f}"
                  f"  {statistics.median(cols[1]):19.2f} {min(cols[1]):5.2f} {max(cols[1]):5.2f}"
                  f"  {statistics.median(cols[2]):18.2f} {min(cols[2]):5.2f} {max(cols[2]):5.2f}")
        return

    def minutes(pairs):
        g = pairs / 1e9
        return g * (c0 + c1 * max(0.0, g - KNEE_GPAIRS))

    usage = shutil.disk_usage(os.path.join(sb.SRC_DIR, "scratch"))
    total, free_now = usage.total, usage.free
    other_kept = gc_kept_bytes(prefix)
    in_use = working_bytes()

    # Walk the tree in build order: finished, then in progress, then
    # projected. live holds the results not yet consumed, as states.
    size = {}
    live = set()
    consumed = 0
    rows = []
    t = None
    free = None
    for start, end, kids in order + ([(0, n, tops)] if len(tops) > 1 else []):
        node, span = name_of((start, end), n), end - start
        ins = inputs_of((start, end), kids, size if node not in done else {**size, **states})
        if node in done:
            out = states.get(node)
            if out is None:
                sb.fail(f"{node} is built but its state count is not in the log")
            size[node] = out
            pairs = sum(builds[node]["unique"].values()) if builds[node]["unique"] else None
            rows.append(dict(node=node, span=span, status="done", ins=sum(ins), out=out, minutes=wall.get(node),
                             when=finish.get(node), peak=pairs * PEAK_BYTES_PER_PAIR if pairs else None, low=None))
        else:
            if node in earlier_out and out_scale is not None:
                out = earlier_out[node] * out_scale
            else:
                out = sum(ins) * ratio_for(result_ratio, span)
            if node in earlier_pairs and pairs_scale is not None:
                pairs = earlier_pairs[node] * pairs_scale
            else:
                pairs = sum(ins) * ratio_for(pairs_ratio, span)
            peak = pairs * PEAK_BYTES_PER_PAIR
            if node == current:
                free = free_now + in_use
                eta = union_eta_finish(args.game, args)
                start_t = finish[max(finish, key=finish.get)] if finish else time.time()
                t = eta if eta is not None else start_t + 60 * minutes(pairs)
                m = (t - start_t) / 60
                status = "running"
            else:
                if free is None:
                    free = free_now
                    t = time.time()
                if free - PEAK_MARGIN * peak < args.floor * TIB:
                    kept = other_kept + sum(dfa_bytes(size[x]) for x in live) + (n - consumed) * move_file_mean
                    t += 60 * GC_MINUTES
                    rows.append(dict(gc=True, when=t, before=free, after=total - kept))
                    free = total - kept
                m = minutes(pairs)
                t += 60 * m
                status = "planned"
            rows.append(dict(node=node, span=span, status=status, ins=sum(ins), out=out, minutes=m, when=t, peak=peak, pairs=pairs,
                             low=free - PEAK_MARGIN * peak))
            free -= dfa_bytes(out)
            size[node] = out
        for k in kids:
            if k[1] - k[0] == 1:
                consumed = max(consumed, k[1])
            else:
                live.discard(name_of(k, n))
        live.add(node)

    print(f"{args.game}: {n} moves, fan-in {len(tops)} at begin; scratch {free_now / TIB:.2f} TiB free of {total / TIB:.2f} TiB,"
          f" {in_use / TIB:.2f} TiB in builds in progress")
    print("projections by moves per join, from the finished joins of this solve, or for a span it has none, an earlier solve:")
    for s in sorted(result_ratio):
        print(f"  {s:3d} moves: result/sum {result_ratio[s]:.2f}" + (f", unique pairs/input {pairs_ratio[s]:.2f}" if s in pairs_ratio else "")
              + f" ({len(by_span_result[s])} joins, {span_source[s]})")
    print(f"  minutes = G pairs x ({c0:.3f} + {c1:.4f} x max(0, G pairs - {KNEE_GPAIRS})), fitted to {len(points)} unions at the latest code;"
          f" peak = {PEAK_BYTES_PER_PAIR:.0f} B per unique pair; low point = free - {PEAK_MARGIN} x peak")
    if out_scale is not None:
        print(f"  unions an earlier solve built take their result there times {out_scale:.2f} and their unique pairs there times"
              f" {pairs_scale:.2f}, the median ratios of the {len(out_growth)} and {len(pairs_growth)} finished here to theirs there"
              + ("" if out_growth else ", or before any has finished, the move nodes' ratio"))
    if scaled:
        print(f"  {len(scaled)} move nodes not built yet take their states in an earlier solve times {move_scale:.2f},"
              f" the median ratio of the {len(growth)} built here to theirs there")
    rest = [e for e in unbuilt if e not in scaled]
    if rest:
        print(f"  {len(rest)} move nodes not built yet take the median of the {len(known)} built, {statistics.median(known) / 1e9:.2f}G states: {' '.join(rest)}")
    print()
    print(f'  {"union":20s} {"status":8s} {"finish":>9s} {"min":>6s} {"inputs":>7s} {"result":>7s} {"peak":>9s} {"low point":>9s}')
    fmt_t = lambda e: time.strftime("%a %H:%M", time.localtime(e)) if e else ""
    after_gc = False
    for r in rows:
        if r.get("gc"):
            print(f'  {"gc":20s} {"planned":8s} {fmt_t(r["when"]):>9s} {GC_MINUTES:6.1f}   free {r["before"] / TIB:.2f} -> {r["after"] / TIB:.2f} TiB')
            after_gc = True
            continue
        shown = args.all or r["span"] >= 8 or r["status"] == "running" or r["node"] == "begin" or after_gc
        after_gc = False
        if not shown:
            continue
        if r["status"] == "done" and not args.all and (r["when"] is None or r["when"] < last_gc):
            continue
        cell = lambda v, w, f: format(v, f).rjust(w) if v is not None else " " * w
        print(f'  {r["node"].replace("begin.", ""):20s} {r["status"]:8s} {fmt_t(r["when"]):>9s} {cell(r["minutes"], 6, ".1f")}'
              f' {r["ins"] / 1e9:6.1f}G {r["out"] / 1e9:6.1f}G {cell(r["peak"] / TIB if r["peak"] else None, 5, ".2f")} TiB'
              + (f' {r["low"] / TIB:5.2f} TiB' if r["low"] is not None else "")
              + ("  beyond the fitted pairs" if r["status"] != "done" and r["pairs"] > max_fit_pairs else ""))


if __name__ == "__main__":
    main()
