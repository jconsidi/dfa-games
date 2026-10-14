#!/usr/bin/env python3
"""Find how a game was solved from its scratch directory, run the matching
stats binaries, and print the results as Markdown tables.

usage: solve_stats.py GAME [--scratch DIR]

Runs from src/ (found relative to this file), where the stats binaries and
the scratch symlink live. Exits nonzero, with the reason, on anything it does
not recognize: a stats binary failing, output it cannot parse, or a scratch
directory that is missing.
"""

import argparse
import collections
import os
import re
import subprocess
import sys
import time

SRC_DIR = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "src"))

RESULTS = ("winning", "losing", "unknown")

# Name families written by the solvers, as stored under scratch/<game>/.
PATTERNS = {
    "forward": re.compile(r"^forward,ply=(\d{3})$"),
    "backward": re.compile(r"^backward,ply_max=(\d{3}),side=(\d),(winning|losing|unknown)$"),
    "forward_backward": re.compile(r"^forward_backward,forward_ply_max=(\d{3}),backward_ply_max=(\d{3}),ply=(\d{3}),(winning|losing|unknown)$"),
    "bound_backward": re.compile(r"^bound_backward,forward_ply_max=(\d{3}),backward_ply_max=(\d{3}),ply=(\d{3}),(winning|losing|unknown)$"),
    "component_bound": re.compile(r"^component_bound,ply=(\d{3})$"),
}

# Profile prints "<label> took <N>s" lines to stdout, and they can land in the
# middle of a stats row, since a row is streamed while its DFAs are measured.
# A label never contains a tab, and every measurement in a row follows one.
PROFILE_LINE = re.compile(r"[^\t\n]* took \d+s\n")


def fail(message):
    print(f"solve_stats: {message}", file=sys.stderr)
    sys.exit(1)


def scan(game_dir):
    families = collections.defaultdict(list)
    other = []
    dangling = collections.Counter()
    for entry in sorted(os.listdir(game_dir)):
        family = None
        for name, pattern in PATTERNS.items():
            match = pattern.match(entry)
            if match:
                family = name
                families[name].append(match.groups())
                break
        if family is None:
            other.append(entry)
        if not os.path.exists(os.path.join(game_dir, entry)):
            dangling[family or "other"] += 1
    return families, other, dangling


def run_stats(binary, args):
    command = [f"./{binary}"] + [str(a) for a in args]
    if not os.access(os.path.join(SRC_DIR, binary), os.X_OK):
        fail(f"{binary} is not built in {SRC_DIR} (run make -j8 {binary})")
    start = time.time()
    proc = subprocess.run(command, cwd=SRC_DIR, capture_output=True, text=True)
    elapsed = time.time() - start
    if proc.returncode != 0:
        fail(f"{' '.join(command)} exited with status {proc.returncode}:\n{proc.stderr.strip()}")

    text = PROFILE_LINE.sub("", proc.stdout)
    lines = [line for line in text.split("\n") if line != ""]
    if not lines:
        fail(f"{' '.join(command)} printed nothing")
    header = lines[0].split("\t")
    if header[0] != "ply":
        fail(f"{' '.join(command)}: unexpected header {lines[0]!r}")
    rows = []
    for line in lines[1:]:
        fields = line.split("\t")
        if len(fields) != len(header):
            fail(f"{' '.join(command)}: row {line!r} has {len(fields)} fields, header has {len(header)}")
        try:
            row = {"ply": int(fields[0])}
            for key, value in zip(header[1:], fields[1:]):
                row[key] = int(value) if "_states" in key else float(value)
        except ValueError:
            fail(f"{' '.join(command)}: could not parse row {line!r}")
        rows.append(row)
    # stats_forward_backward and stats_bound_backward report a missing DFA on
    # stderr and skip the ply.
    missing = [line for line in proc.stderr.split("\n") if line.startswith("DFA ")]
    return " ".join(command), rows, missing, elapsed


def fmt_positions(x):
    # The binaries print positions with ostream's default 6 significant digits.
    if x < 1e6:
        return f"{x:,.0f}"
    mantissa, exponent = f"{x:.3e}".split("e")
    return f"{mantissa}e{int(exponent)}"


def fmt_states(n):
    return f"{n:,d}"


def partition_ok(total, parts):
    # Each figure is rounded to 6 significant digits, so allow for that.
    return abs(sum(parts) - total) <= 2e-5 * max(total, 1) + 0.5


def solved_table(title, command, rows, total_key, total_label, plies_expected, missing, elapsed):
    mismatches = []
    for row in rows:
        total = row[total_key + "_positions"]
        parts = [row[r + "_positions"] for r in RESULTS]
        if not partition_ok(total, parts):
            sum_text = " + ".join("%s %g" % (r, p) for r, p in zip(RESULTS, parts))
            mismatches.append("ply %d: %s != %s %g" % (row["ply"], sum_text, total_key, total))
    if mismatches:
        fail(f"{command}: winning + losing + unknown positions do not match the total:\n" + "\n".join(mismatches))

    out = [f"### {title}", "", f"`{command}` ({elapsed:.0f}s)", ""]
    out.append(f"| Ply | {total_label} positions | Winning | Losing | Unknown | {total_label} states | Winning states | Losing states | Unknown states |")
    out.append("|---:|---:|---:|---:|---:|---:|---:|---:|---:|")
    for row in rows:
        out.append("| " + " | ".join(
            [str(row["ply"]), fmt_positions(row[f"{total_key}_positions"])]
            + [fmt_positions(row[f"{r}_positions"]) for r in RESULTS]
            + [fmt_states(row[f"{total_key}_states"])]
            + [fmt_states(row[f"{r}_states"]) for r in RESULTS]) + " |")
    out.append("")

    present = {row["ply"] for row in rows}
    absent = [p for p in plies_expected if p not in present]
    notes = [f"{len(rows)} plies reported" + (f"; missing plies: {compress(absent)}" if absent else "; no plies missing") + "."]
    if missing:
        notes.append(f"{len(missing)} DFAs reported missing on stderr.")
    notes.append("Winning + losing + unknown positions match the total at every ply shown.")
    unknown_left = [row["ply"] for row in rows if row["unknown_positions"] > 0]
    notes.append(f"Unknown positions remain at plies {compress(unknown_left)}." if unknown_left else "No unknown positions at any ply shown.")
    if 0 in present:
        row0 = next(row for row in rows if row["ply"] == 0)
        if row0[f"{total_key}_positions"] == 1:
            verdict = [r for r in RESULTS if row0[f"{r}_positions"] == 1]
            if len(verdict) == 1:
                notes.append(f"Ply 0 is a single position, and it is **{verdict[0]}** for the side to move.")
    out += notes
    out.append("")
    return out


def compress(plies):
    plies = sorted(plies)
    ranges = []
    for p in plies:
        if ranges and p == ranges[-1][1] + 1:
            ranges[-1][1] = p
        else:
            ranges.append([p, p])
    return ", ".join(str(a) if a == b else f"{a}-{b}" for a, b in ranges)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("game")
    parser.add_argument("--scratch", default="scratch")
    args = parser.parse_args()

    game_dir = os.path.join(SRC_DIR, args.scratch, args.game)
    if not os.path.isdir(game_dir):
        fail(f"no scratch directory {game_dir}")

    families, other, dangling = scan(game_dir)

    out = [f"## {args.game}", ""]
    out.append(f"Scratch directory `{os.path.join(args.scratch, args.game)}`:")
    out.append("")
    out.append("| Family | DFAs | Parameters | Dangling |")
    out.append("|---|---:|---|---:|")
    for name in PATTERNS:
        if name not in families:
            continue
        entries = families[name]
        if name in ("forward_backward", "bound_backward"):
            params = sorted({(int(f), int(b)) for f, b, _, _ in entries})
            desc = ", ".join(f"forward_ply_max={f} backward_ply_max={b}" for f, b in params)
        elif name == "backward":
            desc = "ply_max " + compress({int(p) for p, _, _ in entries})
        else:
            desc = "ply " + compress({int(g[0]) for g in entries})
        out.append(f"| `{name}` | {len(entries)} | {desc} | {dangling[name]} |")
    if other:
        shown = ", ".join(f"`{o}`" for o in other[:6]) + (", ..." if len(other) > 6 else "")
        out.append(f"| other | {len(other)} | {shown} | {dangling['other']} |")
    out.append("")
    if sum(dangling.values()):
        out.append(f"**{sum(dangling.values())} links dangle; the stats binaries will fail on any they need.**")
        out.append("")

    ran = False

    for name, binary, total_key, total_label in (
            ("forward_backward", "stats_forward_backward", "reachable", "Reachable"),
            ("bound_backward", "stats_bound_backward", "bound", "Bound")):
        for f, b in sorted({(int(f), int(b)) for f, b, _, _ in families.get(name, [])}):
            command, rows, missing, elapsed = run_stats(binary, [args.game, f, b])
            out += solved_table(f"{name}, forward_ply_max={f}, backward_ply_max={b}",
                                command, rows, total_key, total_label, range(f + 1), missing, elapsed)
            ran = True

    if "backward" in families:
        have = {(int(p), int(s), r) for p, s, r in families["backward"]}
        complete = -1
        while all((complete + 1, s, r) in have for s in (0, 1) for r in RESULTS):
            complete += 1
        if complete < 0:
            out.append(f"`backward` DFAs are present ({len(have)}), but ply_max=000 is not complete for both sides and all of {', '.join(RESULTS)}, so `stats_backward` was not run.")
            out.append("")
        else:
            command, rows, missing, elapsed = run_stats("stats_backward", [args.game, complete])
            out += [f"### backward, ply_max 0-{complete}", "", f"`{command}` ({elapsed:.0f}s)", ""]
            out.append("| Ply max | " + " | ".join(f"{r.capitalize()} {s} positions" for s in (0, 1) for r in RESULTS) + " | " + " | ".join(f"{r.capitalize()} {s} states" for s in (0, 1) for r in RESULTS) + " |")
            out.append("|---:" * 13 + "|")
            for row in rows:
                out.append("| " + " | ".join([str(row["ply"])]
                                             + [fmt_positions(row[f"{r}_positions_{s}"]) for s in (0, 1) for r in RESULTS]
                                             + [fmt_states(row[f"{r}_states_{s}"]) for s in (0, 1) for r in RESULTS]) + " |")
            out.append("")
            ran = True

    if "forward" in families and "forward_backward" not in families:
        command, rows, missing, elapsed = run_stats("stats_forward", [args.game])
        out += ["### forward", "", f"`{command}` ({elapsed:.0f}s)", ""]
        out.append("| Ply | Reachable positions | Reachable states |")
        out.append("|---:|---:|---:|")
        for row in rows:
            out.append(f"| {row['ply']} | {fmt_positions(row['reachable_positions'])} | {fmt_states(row['reachable_states'])} |")
        out.append("")
        ran = True
    elif "forward" in families:
        out.append("`forward` DFAs are covered by the reachable columns above, so `stats_forward` was not run separately.")
        out.append("")

    if not ran:
        out.append("No solve results found that a stats binary covers.")
        out.append("")

    print("\n".join(out))


if __name__ == "__main__":
    main()
