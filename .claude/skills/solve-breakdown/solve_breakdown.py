#!/usr/bin/env python3
"""Where a solve's union time goes, for the latest solver code version.

usage: solve_breakdown.py GAME breakdown [--wait] [--trace FILE] [--log FILE]
       solve_breakdown.py GAME lookups NODE [--trace FILE] [--log FILE]

Runs from src/ (found relative to this file). The trace defaults to
GAME.jsonl (DFA_PROFILE_TRACE) and the log to GAME.log, both in src/.

A code version is the solver code a run used: runs whose restart commits
differ only outside src/ and third_party/, or only in Markdown files, are the
same version. Each trace run is matched to the log's "=== restart" line
written just before it.

breakdown: the unions finished at the latest run's code version, then
minutes, share and cores busy per phase group, over those unions and for the
latest one. With --wait, it first waits for the next union to finish, and
exits 3 if the solve exits instead.

lookups: one union's forward and backward passes, split by the BinaryDFA
record producer's lookups, with per layer minutes and own major faults. The
input DFAs' layer sizes are read from scratch when they still exist there.

The union-eta skill imports this file for its trace and log parsing.

Exits nonzero, with the reason, on anything it does not recognize.
"""

import argparse
import collections
import json
import os
import re
import struct
import subprocess
import sys
import time

SRC_DIR = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "src"))
REPO_DIR = os.path.dirname(SRC_DIR)

# Major faults are counted per phase, not bytes; the trace's inblock is 0 on
# macOS. Each fault is shown as one 16 KiB page, a lower bound on the read.
PAGE_BYTES = 16384

# A trace run starts when the solver process does, right after the log's
# restart line.
RESTART_TO_RUN_MAX_S = 300

GROUPS = [
    ("records: bucket partition", lambda k: k.startswith("bucket_populate partition")),
    ("records: map populate", lambda k: k.startswith("MemoryMap via populate_func chunk populate")),
    ("records: map write", lambda k: k.startswith("MemoryMap via populate_func chunk write")),
    ("sort: forward sort and unique", lambda k: k.endswith("sort and unique")),
    ("sort: rows sort and count", lambda k: k.endswith("rows sort and count")),
    ("sort: rows number", lambda k: k.endswith("rows number")),
    ("sort: rank outputs", lambda k: k.endswith("rank outputs")),
    ("save: serialize layer", lambda k: k == "serialize serialize layer"),
    ("save: serialize digest", lambda k: k == "serialize serialize digest"),
    ("load: calculate_digest", lambda k: k == "calculate_digest"),
    ("states write", lambda k: k.endswith("states write")),
    ("get_union / bounds", lambda k: k.startswith("get_union") or k.startswith("get_linear_bound")),
]


def fail(message):
    print(f"{os.path.basename(sys.argv[0])}: {message}", file=sys.stderr)
    sys.exit(1)


def read_runs(trace):
    """Each run in the trace as (run record, phase records)."""
    if not os.path.exists(trace):
        fail(f"no trace at {trace}")
    runs = []
    with open(trace) as f:
        for number, line in enumerate(f, 1):
            if not line.strip():
                continue
            try:
                record = json.loads(line)
            except json.JSONDecodeError as e:
                # The solve may be partway through writing the last line.
                if f.read(1) == "":
                    break
                fail(f"{trace}:{number}: {e}")
            if record.get("t") == "run":
                runs.append((record, []))
            elif record.get("t") == "phase":
                if not runs:
                    fail(f"{trace}:{number}: phase record before any run record")
                runs[-1][1].append(record)
            else:
                fail(f"{trace}:{number}: unknown record type {record.get('t')!r}")
    if not runs:
        fail(f"no run record in {trace}")
    return runs


def read_log(log):
    if not os.path.exists(log):
        fail(f"no log at {log}")
    with open(log) as f:
        return f.read().splitlines()


def union_node(record):
    if record["name"].startswith("load_or_build move_nodes") and ",node=begin.join_" in record["name"]:
        return record["name"].rsplit(",node=", 1)[1]
    return None


def run_start(run):
    return run["start_epoch_ms"] / 1000


def restart_marks(lines):
    """(epoch seconds, commit) of each "=== restart" line."""
    marks = []
    for line in lines:
        if line.startswith("=== restart"):
            m = re.match(r"=== restart (\d{4}-\d\d-\d\d \d\d:\d\d:\d\d) at ([0-9a-f]{7,40}) ", line)
            if not m:
                fail(f"unrecognized restart line: {line}")
            marks.append((time.mktime(time.strptime(m.group(1), "%Y-%m-%d %H:%M:%S")), m.group(2)))
    return marks


def run_commits(runs, lines):
    """The commit of each trace run, or None for a run with no restart line
    just before it."""
    marks = restart_marks(lines)
    commits = []
    for run, _ in runs:
        start = run_start(run)
        before = [c for t, c in marks if 0 <= start - t <= RESTART_TO_RUN_MAX_S]
        commits.append(before[-1] if before else None)
    return commits


_changes = {}


def solver_changes(a, b):
    """Files that differ between commits a and b in the solver's code: under
    src/ or third_party/, other than Markdown."""
    if (a, b) not in _changes:
        p = subprocess.run(["git", "-C", REPO_DIR, "diff", "--name-only", a, b, "--", "src", "third_party"], capture_output=True, text=True)
        if p.returncode != 0:
            fail(f"git diff {a} {b} failed: {p.stderr.strip()}")
        _changes[(a, b)] = [f for f in p.stdout.split() if not f.endswith(".md")]
    return _changes[(a, b)]


def version_runs(runs, lines):
    """The indices of the runs at the latest run's code version, and that
    run's commit."""
    commits = run_commits(runs, lines)
    latest = commits[-1]
    if latest is None:
        fail("the trace's last run has no restart line in the log, so its commit is unknown")
    return [i for i, c in enumerate(commits) if c is not None and not solver_changes(c, latest)], commits


def group_of(record):
    key = record["name"] + (" " + record["label"] if record["label"] else "")
    for group, matches in GROUPS:
        if matches(key):
            return group
    return "other"


def group_table(records, title):
    totals = collections.defaultdict(lambda: [0, 0, 0])
    for r in records:
        t = totals[group_of(r)]
        t[0] += r["self_us"]
        t[1] += r["self_user_us"]
        t[2] += r["self_sys_us"]
    wall = sum(t[0] for t in totals.values())
    print(f"{title}: {wall / 6e7:.1f} min in finished phases")
    print(f'  {"phase":32s} {"min":>7s} {"%":>5s} {"user cores":>10s} {"sys cores":>9s}')
    for group, t in sorted(totals.items(), key=lambda x: -x[1][0]):
        if t[0] < 0.005 * wall:
            continue
        print(f"  {group:32s} {t[0] / 6e7:7.1f} {100 * t[0] / max(wall, 1):5.1f} {t[1] / max(t[0], 1):10.2f} {t[2] / max(t[0], 1):9.2f}")


def within(records, union):
    t0, t1 = union["start_us"], union["start_us"] + union["wall_us"]
    return [r for r in records if t0 <= r["start_us"] < t1]


def unions_built_since_restart(log):
    lines = read_log(log)
    starts = [k for k, l in enumerate(lines) if l.startswith("=== restart")]
    since = lines[starts[-1]:] if starts else lines
    return sum(1 for l in since if l.startswith("built move_nodes") and ",node=begin.join_" in l), (lines[-1] if lines else "")


def breakdown(args):
    if args.wait:
        count, _ = unions_built_since_restart(args.log)
        while True:
            time.sleep(30)
            now, last = unions_built_since_restart(args.log)
            if now > count:
                break
            if last.startswith("=== build_bound_backward exited"):
                print(last)
                sys.exit(3)
        # The trace record follows the log line.
        time.sleep(5)
        print(time.strftime("%a %b %d %H:%M:%S %Z %Y"))

    runs = read_runs(args.trace)
    lines = read_log(args.log)
    chosen, commits = version_runs(runs, lines)
    latest = commits[-1]
    head = solver_changes(latest, "HEAD")
    print(f"code version of the latest run, {latest}: " + ("HEAD has the same solver code" if not head else "HEAD differs in " + ", ".join(head)))
    print("runs at this code version:")
    unions, inside, total = [], [], 0
    for i in chosen:
        run, records = runs[i]
        finished = [r for r in records if union_node(r)]
        started = time.strftime("%Y-%m-%d %H:%M", time.localtime(run_start(run)))
        print(f"  {started} at {commits[i]}: {len(finished)} unions finished")
        for r in finished:
            unions.append(r)
            inside.extend(within(records, r))
            latest_records = records
        total += sum(r["self_us"] for r in records)
    print("\nunions finished at this code version:")
    for r in unions:
        print(f'  {union_node(r):20s} {r["wall_us"] / 6e7:7.1f} min  majflt {r["majflt"]:>12,}')
    if not unions:
        print("  none yet")
        return
    print()
    group_table(inside, "over those unions")
    outside = total - sum(r["self_us"] for r in inside)
    print(f"  plus {outside / 6e7:.1f} min of finished phases outside them: loading at each start, and unions not finished in their run")
    print()
    group_table(within(latest_records, unions[-1]), "latest union " + union_node(unions[-1]))


def classify(labels):
    """(pass, layer, division) for a phase from its ancestors' labels."""
    layer = None
    for label in labels:
        m = re.match(r"build_quadratic_(forward|backward)\|layer=(\d+)$", label)
        if m:
            layer = (m.group(1), int(m.group(2)))
            break
    has = lambda s: any(s in label for label in labels)
    if layer is None:
        if any(label.startswith("serialize|") for label in labels):
            return ("save", None, "serialize " + labels[0].split("|", 1)[1])
        if labels and labels[0].startswith("calculate_digest|"):
            return ("load", None, "calculate_digest")
        return ("outside", None, "get_union and other")
    p, n = layer
    if p == "forward":
        if has("build_quadratic_transition_pairs|left"):
            return (p, n, "1 left lookup")
        if has("build_quadratic_transition_pairs|right"):
            return (p, n, "2 right lookup")
        if has("|sort and unique"):
            return (p, n, "sort and unique")
        return (p, n, "other")
    if has("|transitions input"):
        if has("build_quadratic_transition_pairs|left"):
            return (p, n, "3 rebuild: left lookup")
        if has("build_quadratic_transition_pairs|right"):
            return (p, n, "3 rebuild: right lookup")
        return (p, n, "3 rebuild: other")
    if has("|transitions populate"):
        return (p, n, "4 next pair lookup")
    if has("|next pair outputs") or has("|next pair mmap") or has("|unlink next pairs"):
        return (p, n, "prep: next pair outputs")
    if has("|next pair index"):
        return (p, n, "prep: next pair index")
    if any(has("|" + x) for x in ("rows sort and count", "rows number", "states write", "rank outputs")):
        return (p, n, "sorts and numbering")
    return (p, n, "other")


def layer_bytes(path):
    """Each layer's transition block bytes, from FORMAT-DFA.md's tables."""
    with open(path, "rb") as f:
        header = f.read(64)
        if header[:8] != b"DFA1\r\n\x1a\n":
            fail(f"{path} is not a DFA file")
        ndim = struct.unpack_from("<I", header, 48)[0]
        tables = f.read(16 * ndim)
    offsets = struct.unpack_from(f"<{ndim}Q", tables, 8 * ndim)
    end = os.path.getsize(path)
    return [(offsets[i + 1] if i + 1 < ndim else end) - offsets[i] for i in range(ndim)]


def lookups(args):
    node = args.node
    found = None
    for run, records in read_runs(args.trace):
        for r in records:
            if union_node(r) == node:
                found = (records, r)
    if found is None:
        fail(f"no finished union {node} in {args.trace}")
    records, union = found
    by_id = {r["id"]: r for r in records}
    children = collections.defaultdict(list)
    for r in records:
        children[r["parent"]].append(r)

    def labels(r):
        out = []
        while r is not None and r["id"] != union["id"]:
            out.append(r["name"] + "|" + r["label"])
            r = by_id.get(r["parent"])
        return out

    totals = collections.defaultdict(lambda: [0, 0, 0, 0])
    for r in within(records, union):
        t = totals[classify(labels(r))]
        t[0] += r["self_us"]
        t[1] += r["self_user_us"]
        t[2] += r["self_sys_us"]
        t[3] += r["majflt"] - sum(c["majflt"] for c in children[r["id"]])

    log = read_log(args.log)
    begins = [k for k, l in enumerate(log) if l.startswith("building move_nodes/") and l.endswith(",node=" + node)]
    if not begins:
        fail(f"no building line for {node} in {args.log}")
    a = begins[-1]
    b = next((k for k in range(a, len(log)) if log[k].startswith("built move_nodes/") and (",node=" + node + " ") in log[k]), None)
    if b is None:
        fail(f"no built line for {node} in {args.log}")

    # Lookups per layer are the transitions made from its pairs, the
    # "original" pair count logged before the forward layer's "took" line.
    count, last = {}, None
    ids = {}
    inputs = []
    for line in log[a:b]:
        m = re.match(r"pair count = (\d+) \(original", line)
        if m:
            last = int(m.group(1))
        m = re.match(r"build_quadratic_forward layer=(\d+) took", line)
        if m:
            count[int(m.group(1))] = last
        m = re.match(r"layer=(\d+) pair_rank_to_output: \d+ .*state bytes: (\d+) next pairs, (\d+) next ids", line)
        if m:
            ids[int(m.group(1))] = (int(m.group(2)), int(m.group(3)))
        m = re.match(r"UNION ([0-9a-f]{64}) ([0-9a-f]{64})$", line)
        if m and not inputs:
            inputs = [m.group(1), m.group(2)]
    if not count:
        fail(f"no forward layer counts for {node} in {args.log}")
    if not inputs:
        fail(f"no UNION line for {node} in {args.log}")

    # The solve unlinks a join's input nodes once it is built, so the inputs
    # are found by digest, until gc removes them.
    sizes = []
    for digest in inputs:
        path = os.path.join(SRC_DIR, "scratch", "dfas_by_hash", digest + ".dfa")
        sizes.append((digest[:12], layer_bytes(path) if os.path.exists(path) else None))

    print(f'{node}: {union["wall_us"] / 6e7:.1f} min wall, {union["majflt"]:,} major faults')
    for name, s in sizes:
        print(f"  input {name}: " + ("layer sizes read from scratch" if s else "no longer in scratch, so no input layer sizes"))
    have_sizes = len(sizes) == 2 and all(s for _, s in sizes)

    for p in ("forward", "backward"):
        divisions = collections.defaultdict(lambda: [0, 0, 0, 0])
        for (q, n, c), t in totals.items():
            if q == p:
                for i in range(4):
                    divisions[c][i] += t[i]
        print(f"\n{p} pass: {sum(t[0] for t in divisions.values()) / 6e7:.1f} min")
        print(f'  {"division":26s} {"min":>6s} {"%union":>6s} {"user":>5s} {"sys":>5s} {"own majflt":>13s} {"GB faulted":>10s}')
        for c in sorted(divisions):
            t = divisions[c]
            print(f'  {c:26s} {t[0] / 6e7:6.1f} {100 * t[0] / union["wall_us"]:6.1f} {t[1] / max(t[0], 1):5.2f} {t[2] / max(t[0], 1):5.2f} {t[3]:>13,} {t[3] * PAGE_BYTES / 1e9:10.0f}')

        shown = [c for c in sorted(divisions) if c[0] in "1234"]
        header = "  layer  lookups(G) " + " ".join(f"{c[:24]:>24s}" for c in shown)
        if have_sizes:
            header += "  left GB right GB"
        if p == "backward":
            header += "  in-order GB  table GB"
        print("  per layer, minutes and own major faults as GB:")
        print(header)
        for n in sorted({n for (q, n, c) in totals if q == p}, reverse=True):
            lookups_n = count.get(n)
            if not lookups_n or lookups_n < 1e8:
                continue
            cells = []
            for c in shown:
                t = totals.get((p, n, c))
                cells.append(f"{t[0] / 6e7:5.1f} min {t[3] * PAGE_BYTES / 1e9:5.0f} GB" if t else "")
            row = f"  {n:5d} {lookups_n / 1e9:10.1f}  " + " ".join(f"{x:>24s}" for x in cells)
            if have_sizes:
                row += f"  {sizes[0][1][n] / 1e9:7.1f} {sizes[1][1][n] / 1e9:8.1f}"
            if p == "backward" and n in ids and (n + 1) in count:
                # transition_for reads curr_transition_pairs in order and
                # searches the next layer's pair and output records.
                pair_bytes, id_bytes = ids[n]
                record = 2 * pair_bytes + id_bytes
                record += (-record) % max(pair_bytes, id_bytes)
                row += f"  {lookups_n * 2 * pair_bytes / 1e9:11.0f} {count[n + 1] / 2 * record / 1e9:9.0f}"
            print(row)

    outside = collections.defaultdict(lambda: [0, 0])
    for (q, n, c), t in totals.items():
        if q not in ("forward", "backward"):
            outside[c][0] += t[0]
            outside[c][1] += t[3]
    print("\noutside the passes:")
    for c, t in sorted(outside.items(), key=lambda x: -x[1][0]):
        print(f'  {c:26s} {t[0] / 6e7:6.1f} {100 * t[0] / union["wall_us"]:6.1f}% {t[1]:>13,} own majflt')


def default_paths(args):
    args.trace = args.trace or os.path.join(SRC_DIR, args.game + ".jsonl")
    args.log = args.log or os.path.join(SRC_DIR, args.game + ".log")
    if not os.path.exists(args.log):
        fail(f"no log at {args.log}")


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("game")
    parser.add_argument("--trace")
    parser.add_argument("--log")
    commands = parser.add_subparsers(dest="command", required=True)
    b = commands.add_parser("breakdown")
    b.add_argument("--wait", action="store_true")
    l = commands.add_parser("lookups")
    l.add_argument("node")
    args = parser.parse_args()
    default_paths(args)
    if args.command == "breakdown":
        breakdown(args)
    else:
        lookups(args)


if __name__ == "__main__":
    main()
