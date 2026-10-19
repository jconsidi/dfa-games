#!/usr/bin/env python3
"""When the union a running solve is building will finish.

usage: union_eta.py GAME [--ref NODE] [--trace FILE] [--log FILE]

Runs from src/ (found relative to this file). The trace defaults to
GAME.jsonl (DFA_PROFILE_TRACE) and the log to GAME.log, both in src/. The
trace and log parsing comes from the solve-breakdown skill's
solve_breakdown.py.

Prints whether the solve is running, free scratch space and the union in
progress. Every union finished in the trace is a candidate reference,
scored by how far its unique pairs per forward layer are from the current
union's, over the layers the current union has finished, or by its input
state counts before any layer has. The best few are listed with their own
estimates; the best one, or --ref, is compared layer by layer.

An estimate scales the reference's time left, after the latest layer both
unions have finished, by the ratio of their times up to that layer. Its
range scales it by the lowest and highest per layer time ratios, over
layers of 5 minutes or more in the reference.

Exits nonzero, with the reason, on anything it does not recognize.
"""

import argparse
import math
import os
import re
import shutil
import struct
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "solve-breakdown"))
import solve_breakdown as sb  # noqa: E402

# Per layer ratios count toward the range only for layers this long in the
# reference, since short layers vary more than they matter.
RANGE_LAYER_MIN_S = 300
# Unique pair counts below this are too small to score a reference by.
SCORE_PAIRS_MIN = 10**8
CANDIDATES_SHOWN = 3
# The timing estimate needs the shared layers to cover this share of the
# reference's time; before that, loading and small layers dominate, and the
# estimate goes by size.
TIMING_MIN_SHARE = 0.15
SAME_CODE_SIZE_FACTOR = 1.25
# Scaling by input states overestimated join_0_32 by 32% and join_36_40 by
# 21%, so the size estimate's range reaches this share of its scaled time.
SIZE_LOW_FACTOR = 0.75


def builds(lines):
    """Each build of a join node in the log, in order, with its per layer
    forward and backward seconds, filtered pairs and unique pairs, keyed by
    the layer the pairs are for, and its input state counts."""
    out, b = [], None
    states = {}
    for k, line in enumerate(lines):
        m = re.match(r"(?:built|loaded) move_nodes/.*,node=(\S+) => (\d+) states$", line)
        if m:
            states[m.group(1)] = int(m.group(2))
        if line.startswith("building move_nodes/") and ",node=begin.join_" in line:
            prefix, node = line[len("building move_nodes/"):].rsplit(",node=", 1)
            b = {"node": node, "prefix": prefix, "begin": k, "built": False, "inputs": None, "loaded": [],
                 "states": states, "edges": {}, "edge_count": None,
                 "seconds": {"forward": {}, "backward": {}}, "pairs": {}, "unique": {}}
            # The node state counts logged up to this build, and during it.
            states = dict(states)
            b["states"] = states
            out.append(b)
            continue
        if b is None:
            continue
        # A join of joins logs "(child to this join)"; a join of moves logs
        # "(begin to move node)", whose input is the move node.
        m = re.match(r" node \d+/\d+ \((\S+)\), incoming edge (\d+)/(\d+) \((\S+) to (\S+)\)$", line)
        if m:
            b["edges"][int(m.group(2))] = m.group(4) if m.group(5) == m.group(1) else m.group(5)
            b["edge_count"] = int(m.group(3))
            continue
        if line.startswith("==="):
            b = None
            continue
        if line.startswith("built move_nodes/") and (",node=" + b["node"] + " ") in line:
            b["built"] = True
            b = None
            continue
        m = re.match(r"build_quadratic_(forward|backward) layer=(\d+) took (\d+)s$", line)
        if m:
            b["seconds"][m.group(1)][int(m.group(2))] = int(m.group(3))
            continue
        # Each forward layer logs one filtered and one unique count, for
        # layers 1, 2, ... in turn.
        m = re.match(r"pair count = (\d+) \(filtered", line)
        if m:
            b["pairs"][len(b["pairs"]) + 1] = int(m.group(1))
            continue
        m = re.match(r"pair count = (\d+) \(post sort unique\)$", line)
        if m:
            b["unique"][len(b["unique"]) + 1] = int(m.group(1))
            continue
        # Each input's state count is logged as it loads, and both again
        # when the merge starts.
        m = re.match(r"\s*conditions for node \d+ \(.*\) => (\d+) states$", line)
        if m:
            b["loaded"].append(int(m.group(1)))
            continue
        m = re.match(r"\s*merging DFAs with ([\d,]+) states and ([\d,]+) states", line)
        if m and b["inputs"] is None:
            b["inputs"] = (int(m.group(1).replace(",", "")), int(m.group(2).replace(",", "")))
    return out


def file_states(prefix, node):
    """A move node's state count from its DFA header, through its link in
    scratch, or None if the link is gone."""
    path = os.path.join(sb.SRC_DIR, "scratch", "move_nodes", prefix + ",node=" + node)
    if not os.path.exists(path):
        return None
    with open(path, "rb") as f:
        header = f.read(64)
        if header[:8] != b"DFA1\r\n\x1a\n":
            sb.fail(f"{path} is not a DFA file")
        ndim = struct.unpack_from("<I", header, 48)[0]
        return sum(struct.unpack_from(f"<{ndim}Q", f.read(8 * ndim)))


def source_counts(b, live):
    """([states of each input node, before edge conditions, or None while
    unknown], number of inputs), or None.

    A join of joins takes the two joins built before it that split its moves;
    a join of moves takes the move nodes its edge lines name. Counts come from
    the log, or for the union in progress, from the input's DFA header.
    """
    m = re.match(r"begin\.join_(\d+)_(\d+)$", b["node"])
    names, n = None, None
    if m:
        lo, hi = int(m.group(1)), int(m.group(2))
        for x in range(lo + 1, hi):
            a, c = f"begin.join_{lo}_{x}", f"begin.join_{x}_{hi}"
            if a in b["states"] and c in b["states"]:
                names, n = [a, c], 2
    if names is None:
        if not b["edges"]:
            return None
        names, n = [b["edges"][i] for i in sorted(b["edges"])], b["edge_count"]
    counts = []
    for name in names:
        v = b["states"].get(name)
        if v is None and live:
            v = file_states(b["prefix"], name)
        counts.append(v)
    return counts, n


def leading(counts):
    """The counts before the first unknown one."""
    out = []
    for v in counts:
        if v is None:
            break
        out.append(v)
    return out


def input_states(b):
    """(total conditioned input states, whether every input has loaded), or None."""
    if b["inputs"]:
        return sum(b["inputs"]), True
    if b["loaded"]:
        return sum(b["loaded"]), False
    return None


def compared_states(current, ref):
    """(current states, ref states, basis), or None.

    Input node counts come first: over every input when both unions have
    them all, else over the current union's inputs known so far and as many
    of the reference's. The conditioned input counts the merge logs are the
    fallback.
    """
    cur, other = source_counts(current, True), source_counts(ref, False)
    if cur and other:
        k, r = leading(cur[0]), leading(other[0])
        if k and len(k) == cur[1] and len(r) == other[1]:
            return sum(k), sum(r), "input node states"
        if k and len(k) < cur[1] and len(r) >= len(k):
            return sum(k), sum(r[:len(k)]), f"first {len(k)} input node states"
    c = input_states(current)
    if c is None:
        return None
    if c[1]:
        o = input_states(ref)
        return (c[0], o[0], "conditioned input states") if o and o[1] else None
    n = len(current["loaded"])
    return (c[0], sum(ref["loaded"][:n]), f"first {n} conditioned input states") if len(ref["loaded"]) >= n else None


def size_estimate(t0, current, candidates):
    """For a union without enough finished layers to compare: each
    reference's wall time scaled by the ratio of input states, from t0.
    Returns the closest references by input states, each with its estimate."""
    out = []
    for c in candidates:
        pair = compared_states(current, c["build"])
        if pair is None or pair[0] == 0 or pair[1] == 0:
            continue
        scale = pair[0] / pair[1]
        out.append(dict(c, cur_states=pair[0], ref_states=pair[1], basis=pair[2], size_off=abs(math.log(scale)),
                        scale=scale, size_eta=t0 + c["union"]["wall_us"] * scale))
    # Older code ran slower, so references at the same code come first, those
    # within this factor of the size before the rest.
    out.sort(key=lambda c: (not c["same"], not c["size_off"] < math.log(SAME_CODE_SIZE_FACTOR), c["size_off"], -c["run"]))
    return out[:CANDIDATES_SHOWN]


def layer_ends(records, t0, t1):
    """End time of each finished layer record in [t0, t1), keyed by
    (pass, layer)."""
    ends = {}
    for r in records:
        if r["name"] in ("build_quadratic_forward", "build_quadratic_backward") and t0 <= r["start_us"] < t1:
            m = re.match(r"layer=(\d+)$", r["label"])
            if m:
                ends[(r["name"].rsplit("_", 1)[1], int(m.group(1)))] = r["start_us"] + r["wall_us"]
    return ends


def open_union_start(records, after_us):
    """Start of the union still being built, from its finished children.

    Phase records are written when they finish, so the open union's own
    record is absent. Its descendants name it, or open records under it, as
    their parent. The union is the open record just above the BinaryDFA
    build: the latest open parent, below the one holding the pass layers,
    with a child that is not part of the build itself.
    """
    ids = {r["id"] for r in records}
    children = {}
    for r in records:
        if r["parent"] not in ids and r["start_us"] >= after_us:
            children.setdefault(r["parent"], []).append(r)
    passes = [p for p, kids in children.items() if any(k["name"] in ("build_quadratic_forward", "build_quadratic_backward") for k in kids)]
    if not passes:
        return None
    below = [p for p in children if p < min(passes) and any(not k["name"].startswith("build_quadratic") for k in children[p])]
    if not below:
        return None
    return min(k["start_us"] for k in children[max(below)])


def clock(epoch_s):
    return time.strftime("%a %H:%M", time.localtime(epoch_s))


def score(current, ref):
    """(mean |log ratio|, basis) of ref against current, or None."""
    layers = [n for n in current["unique"] if ref["unique"].get(n, 0) >= SCORE_PAIRS_MIN and current["unique"][n] > 0]
    if layers:
        return sum(abs(math.log(current["unique"][n] / ref["unique"][n])) for n in layers) / len(layers), f"unique pairs, {len(layers)} layers"
    pair = compared_states(current, ref)
    if pair and pair[0] and pair[1]:
        return abs(math.log(pair[0] / pair[1])), pair[2]
    return None


def estimate(cur_t0, cur_ends, ref_union, ref_ends, current, ref):
    """(eta, lo, hi, ratio, through layer, reference minutes left) as epoch
    offsets in the current run's microseconds, or None."""
    common = [k for k in cur_ends if k in ref_ends]
    if not common:
        return None
    k = max(common, key=lambda x: cur_ends[x])
    cur_elapsed = cur_ends[k] - cur_t0
    ref_elapsed = ref_ends[k] - ref_union["start_us"]
    if ref_elapsed < TIMING_MIN_SHARE * ref_union["wall_us"]:
        return None
    ref_left = ref_union["wall_us"] - ref_elapsed
    r = cur_elapsed / ref_elapsed
    ratios = [r]
    for p in ("forward", "backward"):
        for n, s in ref["seconds"][p].items():
            c = current["seconds"][p].get(n)
            if c is not None and s >= RANGE_LAYER_MIN_S:
                ratios.append(c / s)
    at = lambda q: cur_t0 + cur_elapsed + q * ref_left
    return at(r), at(min(ratios)), at(max(ratios)), r, min(ratios), max(ratios), k, ref_left


def span(node):
    m = re.match(r"begin\.join_(\d+)_(\d+)$", node)
    return int(m.group(2)) - int(m.group(1)) if m else None


def span_estimate(current, trace_done, commits, epoch, t0):
    """Before any input has loaded: the finished joins over as many moves,
    at the same code, from the previous union's end."""
    width = span(current["node"])
    if t0 is None or width is None:
        print("\nETA: none yet, since no input of this union has loaded and no layer has finished")
        return
    walls = sorted(r["wall_us"] for node, traced in trace_done.items() if span(node) == width
                   for i, r, rs in traced if commits[i] is not None and not sb.solver_changes(commits[i], commits[-1]))
    if not walls:
        print(f"\nETA: none yet, since no input of this union has loaded, and no {width} move join finished at this code")
        return
    mid = walls[len(walls) // 2]
    print(f"\nETA by span, before any input has loaded: {len(walls)} joins over {width} moves at this code took {walls[0] / 6e7:.1f} to {walls[-1] / 6e7:.1f} min, median {mid / 6e7:.1f}, from a start of {clock(epoch + t0 / 1e6)}")
    print(f"ETA {clock(epoch + (t0 + mid) / 1e6)}, range {clock(epoch + (t0 + walls[0]) / 1e6)} to {clock(epoch + (t0 + walls[-1]) / 1e6)}")


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("game")
    parser.add_argument("--ref")
    parser.add_argument("--trace")
    parser.add_argument("--log")
    args = parser.parse_args()
    sb.default_paths(args)

    now = time.time()
    lines = sb.read_log(args.log)
    marks = [l for l in lines if l.startswith("===")]
    print(time.strftime("%a %b %d %H:%M:%S %Z %Y", time.localtime(now)))
    running = bool(marks) and marks[-1].startswith("=== restart")
    print(("solve running: " if running else "solve not running: ") + (marks[-1][4:] if marks else "no === line in the log"))
    print(f"log last written {(now - os.path.getmtime(args.log)) / 60:.1f} min ago: {lines[-1] if lines else ''}")
    usage = shutil.disk_usage(os.path.join(sb.SRC_DIR, "scratch"))
    print(f"scratch: {usage.free / 2**40:.1f} TiB free of {usage.total / 2**40:.1f} TiB ({100 * usage.used / usage.total:.0f}% used)")
    if not running:
        return

    all_builds = builds(lines)
    restart = max(k for k, l in enumerate(lines) if l.startswith("=== restart"))
    open_builds = [b for b in all_builds if b["begin"] > restart and not b["built"]]
    if not open_builds:
        print("\nno union in progress since the last restart")
        return
    current = open_builds[-1]

    runs = sb.read_runs(args.trace)
    commits = sb.run_commits(runs, lines)
    marks = sb.restart_marks(lines)
    run, records = runs[-1]
    if not 0 <= sb.run_start(run) - marks[-1][0] <= sb.RESTART_TO_RUN_MAX_S:
        sb.fail(f"the trace's last run does not follow the log's last restart line, so {args.trace} is not this solve's trace")
    epoch = sb.run_start(run)
    finished_here = [r for r in records if sb.union_node(r)]
    after = max((r["start_us"] + r["wall_us"] for r in finished_here), default=0)
    cur_t0 = open_union_start(records, after)
    cur_ends = layer_ends(records, cur_t0, float("inf")) if cur_t0 is not None else {}

    phase = "backward" if current["seconds"]["backward"] else "forward"
    done = current["seconds"][phase]
    print(f"\ncurrent union {current['node']}: {phase} pass, {len(done)} layers done"
          + (f", latest layer={min(done) if phase == 'backward' else max(done)}" if done else ""))
    if current["inputs"]:
        print(f"  inputs {current['inputs'][0]:,} and {current['inputs'][1]:,} states")
    if cur_t0 is not None:
        print(f"  started {clock(epoch + cur_t0 / 1e6)}, {(now - epoch - cur_t0 / 1e6) / 60:.0f} min ago")

    # Pair the finished builds in the log with the finished union records in
    # the trace, node by node, from the latest back.
    log_done, trace_done = {}, {}
    for b in all_builds:
        if b["built"]:
            log_done.setdefault(b["node"], []).append(b)
    for i, (rec, rs) in enumerate(runs):
        for r in rs:
            if sb.union_node(r):
                trace_done.setdefault(sb.union_node(r), []).append((i, r, rs))
    candidates = []
    for node, traced in trace_done.items():
        for b, (i, r, rs) in zip(reversed(log_done.get(node, [])), reversed(traced)):
            s = score(current, b)
            if s is None:
                continue
            same = commits[i] is not None and not sb.solver_changes(commits[i], commits[-1])
            ends = layer_ends(rs, r["start_us"], r["start_us"] + r["wall_us"])
            est = estimate(cur_t0, cur_ends, r, ends, current, b) if cur_t0 is not None else None
            candidates.append({"node": node, "build": b, "union": r, "run": i, "same": same, "score": s, "est": est})
    if not candidates:
        if input_states(current) is None and source_counts(current, True) is None:
            span_estimate(current, trace_done, commits, epoch, after if finished_here else None)
            return
        sb.fail("no finished union in the trace and log to compare with")
    candidates.sort(key=lambda c: (c["score"][0], not c["same"], -c["run"]))

    if args.ref:
        chosen = [c for c in candidates if c["node"] == args.ref]
        if not chosen:
            sb.fail(f"no finished union {args.ref} in both the trace and the log")
        ref = chosen[0]
    else:
        ref = candidates[0]

    if ref["est"] is None:
        # Before any layer has finished, the previous union's end in this run
        # stands in for this one's start.
        t0 = cur_t0 if cur_t0 is not None else (after if finished_here else None)
        if t0 is None:
            print("\nETA: not enough of this union has finished to compare times, and no union finished before it in this run to give its start")
            return
        sized = size_estimate(t0, current, [ref] if args.ref else candidates)
        if not sized:
            print("\nETA: not enough of this union has finished to compare times, and no reference's input state counts compare")
            return
        print(f"\nETA by size, before enough of this union has finished to compare times, from a start of {clock(epoch + t0 / 1e6)}")
        print(f'  {"union":18s} {"commit":8s} {"code":5s} {"basis":34s} {"states":>15s} {"ref states":>15s} {"scale":>5s} {"min":>6s} {"scaled":>6s}  ETA')
        for c in sized:
            print(f'  {c["node"]:18s} {commits[c["run"]] or "?":8s} {"same" if c["same"] else "older":5s} {c["basis"]:34s} {c["cur_states"]:>15,} {c["ref_states"]:>15,} {c["scale"]:5.2f} {c["union"]["wall_us"] / 6e7:6.1f} {c["union"]["wall_us"] * c["scale"] / 6e7:6.1f}  {clock(epoch + c["size_eta"] / 1e6)}')
        # The range spans the same-code references, or all of them when none
        # is at the same code, and reaches SIZE_LOW_FACTOR of the best one's
        # scaled time, since scaling by size has run long.
        best = sized[0]
        same = [c for c in sized if c["same"]] or sized
        lo = min([c["size_eta"] for c in same] + [t0 + SIZE_LOW_FACTOR * (best["size_eta"] - t0)])
        hi = max(c["size_eta"] for c in same)
        print(f"ETA {clock(epoch + best['size_eta'] / 1e6)}, range {clock(epoch + lo / 1e6)} to {clock(epoch + hi / 1e6)}, by size against {best['node']}")
        print(f"  each reference's minutes scaled linearly by input states; the range spans the {'same-code ' if same is not sized else ''}references above,"
              f" and down to {SIZE_LOW_FACTOR:.2f} of {best['node']}'s scaled time")
        return

    print("\nreferences, closest first:")
    print(f'  {"union":18s} {"run":11s} {"commit":8s} {"code":5s} {"off by":>6s} {"basis":24s} {"min":>6s}  ETA (range)')
    for c in candidates[:CANDIDATES_SHOWN] + ([ref] if ref not in candidates[:CANDIDATES_SHOWN] else []):
        started = time.strftime("%m-%d %H:%M", time.localtime(sb.run_start(runs[c["run"]][0])))
        eta = c["est"]
        when = f"{clock(epoch + eta[0] / 1e6)} ({clock(epoch + eta[1] / 1e6)} to {clock(epoch + eta[2] / 1e6)})" if eta else "no shared finished layer"
        print(f'  {c["node"]:18s} {started:11s} {commits[c["run"]] or "?":8s} {"same" if c["same"] else "older":5s} {100 * (math.exp(c["score"][0]) - 1):5.1f}% {c["score"][1]:24s} {c["union"]["wall_us"] / 6e7:6.1f}  {when}')

    b = ref["build"]
    print(f"\nper layer against {ref['node']}: seconds, and the pairs each forward layer made for the next, with ratios")
    print(f'  {"layer":>5s} {"ref fwd s":>9s} {"fwd s":>7s} {"ratio":>5s} {"ref pairs":>15s} {"pairs":>15s} {"ratio":>5s} {"ref unique":>15s} {"unique":>15s} {"ratio":>5s} {"ref bwd s":>9s} {"bwd s":>7s} {"ratio":>5s}')
    cell = lambda v, w: f"{v:>{w},}" if v is not None else " " * w
    ratio = lambda c, r: f"{c / r:5.2f}" if c is not None and r else "     "
    for n in sorted(set(b["seconds"]["forward"]) | set(b["seconds"]["backward"])):
        rf, cf = b["seconds"]["forward"].get(n), current["seconds"]["forward"].get(n)
        rb, cb = b["seconds"]["backward"].get(n), current["seconds"]["backward"].get(n)
        if max(rf or 0, rb or 0) < 60:
            continue
        rp, cp = b["pairs"].get(n + 1), current["pairs"].get(n + 1)
        ru, cu = b["unique"].get(n + 1), current["unique"].get(n + 1)
        print(f"  {n:5d} {cell(rf, 9)} {cell(cf, 7)} {ratio(cf, rf)} {cell(rp, 15)} {cell(cp, 15)} {ratio(cp, rp)} {cell(ru, 15)} {cell(cu, 15)} {ratio(cu, ru)} {cell(rb, 9)} {cell(cb, 7)} {ratio(cb, rb)}")

    eta, lo, hi, r, rlo, rhi, k, left = ref["est"]
    print(f"\nETA {clock(epoch + eta / 1e6)}, range {clock(epoch + lo / 1e6)} to {clock(epoch + hi / 1e6)}, against {ref['node']}")
    print(f"  through {k[0]} layer={k[1]} this union took {r:.2f} of the reference's time;"
          f" the reference had {left / 6e7:.0f} min left from there, scaled by {r:.2f} (range {rlo:.2f} to {rhi:.2f})")
    if epoch + hi / 1e6 < now:
        print("  every estimate is already past: the current layer is running longer than the reference's")


if __name__ == "__main__":
    main()
