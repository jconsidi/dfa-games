#!/usr/bin/env python3
"""Where does a solver run's time go, and how much of it uses one core?

    ./analyze_log.py <file>

<file> is either a Profile trace (written when DFA_PROFILE_TRACE=<path> is set;
see Profile.cpp) or a solver's captured stdout. The format is detected from
the first line.

A trace is the reliable input: per-phase self time (time spent while the phase
was innermost) and CPU time come straight from the process, so cores in use are
measured rather than guessed. Phases under the trace's min_us are folded into
their enclosing phase. A trace may hold several runs, one per restart.

Stdout logs are the fallback for runs without a trace. Their limits bound what
this can say:
  * Profile::tic only prints phases that took more than 1s, floored to whole
    seconds. Anything shorter is invisible here except as residual time in its
    parent.
  * The log has no timestamps, so total wall time and CPU utilization cannot be
    recovered from it.
  * Profile output has no "start" markers. Nesting is reconstructed from a fixed
    ranking of Profile names (outer -> inner, below): when a phase finishes, every
    unclaimed finished phase of a deeper rank printed since is taken to be its
    child, and the parent's self time is its total minus those children. A
    child whose own parent was under 1s (so never printed) gets claimed by the
    next printed sibling of that parent instead, so self times are approximate.
"""

import json
import re
import sys
import time
from collections import defaultdict

# Profile name -> nesting rank, outer (small) to inner (large). Derived from the
# call structure in MoveGraph / DNFBuilder / DFAUtil / BinaryDFA / MemoryMap.
RANKS = [
    (r"^load_or_build (?!\w+/)", 0),                           # GameBase: bare DFA name
    (r"^load_or_build (?!\w+_cache/)(?!move_nodes/)\w+/", 1),  # DFAUtil: <game>/<name>
    (r"^get_moves_(backward|forward)\b", 2),
    (r"^load_or_build move_nodes/", 3),
    (r"^get_moves\b", 4),
    (r"^to_dfa\b", 5),
    (r"^(compact_longest|compact_last|compact|add_clause)\b", 6),
    (r"^_reduce_aci\b", 7),
    (r"^get_(union|intersection|difference|change|inverse)\b", 8),
    (r"^load_or_build \w+_cache/", 9),
    (r"^(build_quadratic|build_linear|build_one_pass|build_two_pass)\b(?!_)", 10),
    (r"^build_quadratic_(forward|backward) ", 11),
    (r"^build_quadratic_(forward|backward)_layer\b", 12),
    (r"^serialize\b", 12),
    (r"^build_quadratic_transition_pairs\b", 13),
    (r"^(MemoryMap via populate_func|_?flashsort)", 14),
    (r"^calculate_digest\b", 15),
]

# Threading of a phase's *self* time, from reading the code. Only phases whose
# code was checked are tagged; everything else is "?".
#   serial   - no parallel algorithm anywhere in the phase
#   parallel - the phase body is a TRY_PARALLEL_* call
#   mixed    - parallel work plus unprinted serial work (writes, unlinks, ...)
THREADING = [
    (r"^calculate_digest\b", "serial"),          # one SHA-256 stream over the file
    (r"^serialize serialize (layer|digest)", "serial"),
    (r"^build_linear (reachable|linear states)$", "serial"),
    (r"^build_linear states$", "mixed"),        # build_layer populate is parallel; index, materialize, writes are not
    (r"^MemoryMap via populate_func chunk populate", "parallel"),
    (r"^MemoryMap via populate_func (chunk write|chunk iota|truncate|mmap|close)", "serial"),
    (r"^build_quadratic_forward_layer layer=\* (filter|pre-unique|sort)$", "parallel"),
    (r"^build_quadratic_forward_layer layer=\* munmap", "serial"),
    (r"^build_quadratic_forward_layer layer=\* transitions$", "mixed"),
    (r"^build_quadratic_backward_layer layer=\* (transitions input|transitions populate|transitions hash|sort hash|sort permutation|states identification|invert)$", "mixed"),
    (r"^build_quadratic_backward_layer layer=\* (unlink .*|states write)$", "serial"),
    (r"^build_quadratic_transition_pairs (left|right)$", "mixed"),
    (r"^build_quadratic_transition_pairs .*unlink", "serial"),
    (r"^build_one_pass\b", "serial"),          # no parallel code in ChangeDFA.cpp
]

TOOK_RE = re.compile(r"^(?P<label>.*?) took (?P<secs>\d+)s$")
HEX_RE = re.compile(r"[0-9a-f]{20,}")
LAYER_RE = re.compile(r"\blayer=\d+")
BUILDING_RE = re.compile(r"^building (\S+)")
BUILT_RE = re.compile(r"^built (\S+) => (\d+) states")
PAIRS_RE = re.compile(r"^pair count = (\d+) \(post sort unique\)")
STATS_RE = re.compile(r"^PLY (\d+) STATS (.*)")
NODE_RE = re.compile(r"^node (\d+)/(\d+) \((.*)\)")
TOTAL_RE = re.compile(r"^Profile : (.*) = (\d+)$")


def rank_of(label):
    for pattern, rank in RANKS:
        if re.search(pattern, label):
            return rank
    return 99  # unknown: treat as a leaf


def normalize(label):
    """Collapse per-instance detail so phases aggregate across layers and DFAs."""
    label = LAYER_RE.sub("layer=*", label)
    label = HEX_RE.sub("<hash>", label)
    label = re.sub(r"^(load_or_build \w+?)(_cache)?/.*? build$", r"\1\2/* build", label)
    label = re.sub(r"^(load_or_build breakthrough_\w+/[^,]+),.* build$", r"\1,* build", label)
    label = re.sub(r"\s+", " ", label)
    return label


def threading_of(label):
    for pattern, tag in THREADING:
        if re.search(pattern, label):
            return tag
    return "?"


def fmt_us(us):
    s = us / 1e6
    if s < 60:
        return f"{s:.2f}s"
    return fmt_secs(int(s))


def fmt_secs(s):
    if s >= 3600:
        return f"{s / 3600:.1f}h"
    if s >= 60:
        return f"{s / 60:.1f}m"
    return f"{s}s"


def main_log(path):
    with open(path) as f:
        lines = [line.rstrip("\n") for line in f]

    # --- nesting reconstruction -------------------------------------------
    stack = []  # (rank, total_secs) of finished phases not yet claimed by a parent
    self_time = defaultdict(int)
    total_time = defaultdict(int)
    count = defaultdict(int)
    max_time = defaultdict(int)
    clamped = 0

    # --- build tracking ---------------------------------------------------
    builds = []           # dicts: name, peak_pairs, layers, states, op_secs
    open_builds = []      # stack of builds in progress, innermost last
    pending_built = None  # most recent finished cache build, awaiting its get_* "took" line

    last_stats = None
    last_node = None
    current_target = None
    profile_totals = []

    for line in lines:
        m = TOOK_RE.match(line)
        if m:
            label, secs = m.group("label").strip(), int(m.group("secs"))
            rank = rank_of(label)
            children = 0
            while stack and stack[-1][0] > rank:
                children += stack.pop()[1]
            own = secs - children
            if own < 0:
                clamped += 1
                own = 0
            key = normalize(label)
            self_time[key] += own
            total_time[key] += secs
            count[key] += 1
            max_time[key] = max(max_time[key], secs)
            stack.append((rank, secs))

            if pending_built and re.match(r"^get_(union|intersection|difference|change|inverse)\b", label):
                pending_built["op_secs"] = secs
                pending_built["op"] = label.split()[0]
                pending_built = None
            continue

        m = BUILDING_RE.match(line)
        if m:
            name = m.group(1)
            b = {"name": name, "pairs": [], "states": None, "op_secs": None, "op": None}
            open_builds.append(b)
            if name.startswith("breakthrough_"):
                current_target = name
            continue

        m = BUILT_RE.match(line)
        if m:
            name = m.group(1)
            idx = next((i for i in range(len(open_builds) - 1, -1, -1) if open_builds[i]["name"] == name), None)
            if idx is None:
                raise RuntimeError(f"'built {name}' with no matching 'building' line")
            b = open_builds.pop(idx)
            b["states"] = int(m.group(2))
            builds.append(b)
            if "_cache/" in name:
                pending_built = b
            continue

        m = PAIRS_RE.match(line)
        if m and open_builds:
            open_builds[-1]["pairs"].append(int(m.group(1)))
            continue

        m = STATS_RE.match(line)
        if m:
            last_stats = (int(m.group(1)), m.group(2))
            continue

        m = NODE_RE.match(line)
        if m:
            last_node = line
            continue

        m = TOTAL_RE.match(line)
        if m:
            profile_totals.append((m.group(1), int(m.group(2))))

    # --- report -------------------------------------------------------------
    print(f"log: {path} ({len(lines)} lines)")
    print()
    print("== progress ==")
    if last_stats:
        print(f"last completed ply: {last_stats[0]}: {last_stats[1]}")
    if current_target:
        state = "in progress" if any(b["name"] == current_target for b in open_builds) else "done"
        print(f"last target: {current_target} ({state})")
    if last_node:
        print(f"last move node: {last_node}")
    if profile_totals:
        print(f"run finished: {len(profile_totals)} 'Profile :' totals present")
    else:
        print("run not finished (no 'Profile :' totals at end of log)")
    print()

    grand = sum(self_time.values())
    by_tag = defaultdict(int)
    for key, secs in self_time.items():
        by_tag[threading_of(key)] += secs

    print("== self time by threading (phases >1s only) ==")
    print(f"total self time accounted: {fmt_secs(grand)}")
    for tag in ("serial", "mixed", "parallel", "?"):
        secs = by_tag.get(tag, 0)
        share = 100 * secs / grand if grand else 0
        print(f"  {tag:<9} {fmt_secs(secs):>8}  {share:5.1f}%")
    if clamped:
        print(f"  ({clamped} phases had printed children exceeding their own total; self time clamped to 0)")
    print()

    print("== phases by self time ==")
    print(f"{'self':>8} {'share':>6} {'total':>8} {'n':>4} {'max':>6}  {'thread':<8} phase")
    for key in sorted(self_time, key=lambda k: -self_time[k]):
        if self_time[key] == 0 and total_time[key] < 5:
            continue
        share = 100 * self_time[key] / grand if grand else 0
        print(f"{fmt_secs(self_time[key]):>8} {share:5.1f}% {fmt_secs(total_time[key]):>8} "
              f"{count[key]:>4} {fmt_secs(max_time[key]):>6}  {threading_of(key):<8} {key}")
    print()

    print("== slowest set operations (cache builds) ==")
    timed = [b for b in builds if b["op_secs"] is not None]
    timed.sort(key=lambda b: -b["op_secs"])
    print(f"{'time':>8} {'op':<17} {'peak pairs':>12} {'layers':>6} {'states':>12}  name")
    for b in timed[:15]:
        peak = f"{max(b['pairs']):,}" if b["pairs"] else "linear"
        name = HEX_RE.sub("<h>", b["name"])
        print(f"{fmt_secs(b['op_secs']):>8} {b['op']:<17} {peak:>12} {len(b['pairs']):>6} {b['states']:>12,}  {name}")
    print()

    if profile_totals:
        print("== Profile totals reported at exit ==")
        for label, secs in sorted(profile_totals, key=lambda t: -t[1])[:30]:
            print(f"{fmt_secs(secs):>8}  {label}")
        print()

    print("caveats: phases under 1s are not logged and times are floored, so short")
    print("serial steps (unlink, munmap, small writes) are undercounted. For exact")
    print("self time and measured core use, run with DFA_PROFILE_TRACE=<path> and")
    print("analyze that file instead.")


# --- trace files -----------------------------------------------------------

def trace_key(name, label):
    """Group a phase across DFAs, plies and layers."""
    if name.startswith("load_or_build "):
        # keep the kind (cache name, move_nodes, game/...), drop the instance
        name = re.sub(r"^(load_or_build [^/,]+)[/,].*$", r"\1/*", name)
    name = HEX_RE.sub("<hash>", name)
    name = re.sub(r"\d+", "N", name) if name.startswith("load_or_build ") else name
    return f"{name} | {label}" if label else name


def read_trace(path):
    """Yield (run_header, [phase records]) per run. Only the final line may be
    incomplete (a killed process mid-write); anything else malformed is an error."""
    with open(path) as f:
        text = f.read()
    lines = text.split("\n")
    truncated = None
    if lines and lines[-1] != "":
        truncated = lines.pop()
    elif lines:
        lines.pop()

    runs = []
    for number, line in enumerate(lines, 1):
        record = json.loads(line)  # raises with the offending position
        if record["t"] == "run":
            runs.append((record, []))
        elif record["t"] == "phase":
            if not runs:
                raise RuntimeError(f"{path}:{number}: phase record before any run header")
            runs[-1][1].append(record)
        else:
            raise RuntimeError(f"{path}:{number}: unknown record type {record['t']!r}")
    return runs, truncated


def main_trace(path):
    runs, truncated = read_trace(path)
    print(f"trace: {path} ({len(runs)} run(s))")
    if truncated is not None:
        print(f"  final line incomplete ({len(truncated)} bytes), ignored -- process killed mid-write")
    print()

    self_wall = defaultdict(int)
    self_cpu = defaultdict(int)
    self_user = defaultdict(int)
    incl_wall = defaultdict(int)
    count = defaultdict(int)
    max_wall = defaultdict(int)
    ncpu = None

    print("== runs ==")
    for header, phases in runs:
        ids = {p["id"] for p in phases}
        orphans = sum(1 for p in phases if p["parent"] and p["parent"] not in ids)
        top_wall = sum(p["wall_us"] for p in phases if p["parent"] == 0)
        top_cpu = sum(p["user_us"] + p["sys_us"] for p in phases if p["parent"] == 0)
        sum_self = sum(p["self_us"] for p in phases)
        ncpu = ncpu or header["hardware_concurrency"]
        start = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(header["start_epoch_ms"] / 1000))
        last = max((p["start_us"] + p["wall_us"] for p in phases), default=0)
        print(f"pid {header['pid']} on {header['host']} started {start}: "
              f"{len(phases)} phases, last ended at +{fmt_us(last)}, "
              f"completed top-level {fmt_us(top_wall)} wall, "
              f"{top_cpu / top_wall if top_wall else 0:.2f} cores avg")
        # Phases still open when a run was killed are absent, so their recorded
        # children are orphans. In a run that finished there must be none, and
        # self time must sum to top-level time.
        if orphans:
            print(f"  {orphans} phases whose parent never ended (run interrupted)")
        elif abs(sum_self - top_wall) > 1000 * max(1, len(phases) // 1000):
            raise RuntimeError(f"pid {header['pid']}: self time {sum_self}us != top-level {top_wall}us")

        for p in phases:
            key = trace_key(p["name"], p["label"])
            self_wall[key] += p["self_us"]
            self_cpu[key] += p["self_user_us"] + p["self_sys_us"]
            self_user[key] += p["self_user_us"]
            incl_wall[key] += p["wall_us"]
            count[key] += 1
            max_wall[key] = max(max_wall[key], p["wall_us"])
    print()

    grand = sum(self_wall.values())
    grand_cpu = sum(self_cpu.values())
    if not grand:
        print("no phases recorded")
        return

    print(f"== self time by measured cores ({ncpu} hardware threads) ==")
    print(f"total self time {fmt_us(grand)}, average {grand_cpu / grand:.2f} cores")
    bands = [("< 1.5 cores (serial)", 0, 1.5), ("1.5 - 4", 1.5, 4),
             ("4 - 8", 4, 8), (">= 8", 8, float("inf"))]
    for label, lo, hi in bands:
        secs = sum(w for k, w in self_wall.items() if w and lo <= self_cpu[k] / w < hi)
        print(f"  {label:<22} {fmt_us(secs):>8}  {100 * secs / grand:5.1f}%")
    idle = grand * ncpu - grand_cpu
    print(f"  unused core-time: {100 * idle / (grand * ncpu):.1f}% of {ncpu} x wall")
    print()

    print("== phases by self time ==")
    print(f"{'self':>8} {'share':>6} {'cores':>6} {'user':>5} {'incl':>8} {'n':>7} {'max':>7}  phase")
    shown = 0
    for key in sorted(self_wall, key=lambda k: -self_wall[k]):
        w = self_wall[key]
        if w < grand * 0.001 and shown >= 15:
            break
        shown += 1
        print(f"{fmt_us(w):>8} {100 * w / grand:5.1f}% "
              f"{self_cpu[key] / w if w else 0:6.2f} {self_user[key] / w if w else 0:5.2f} "
              f"{fmt_us(incl_wall[key]):>8} {count[key]:>7} "
              f"{fmt_us(max_wall[key]):>7}  {key}")
    print()
    print("self = time while the phase was innermost; cores = (user+sys)/self wall, all")
    print("threads; user = user-only cores. Phases under min_us are folded into their parent.")


def main(path):
    with open(path) as f:
        first = f.readline()
    try:
        is_trace = json.loads(first).get("t") == "run"
    except ValueError:
        is_trace = False
    if is_trace:
        main_trace(path)
    else:
        main_log(path)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])
