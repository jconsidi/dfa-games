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
import json
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
    raw = [header]
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
        raw.append(fields)
    # stats_forward_backward and stats_bound_backward report a missing DFA on
    # stderr and skip the ply.
    missing = [line for line in proc.stderr.split("\n") if line.startswith("DFA ")]
    return " ".join(command), rows, raw, missing, elapsed


def tsv_table(raw, per_state, columns=None, suffix=""):
    """The binary's TSV as a Markdown table: its column names, order and
    values as printed, with a positions_per_state column appended. columns
    picks a subset of the TSV's columns, in their TSV order, and suffix is
    dropped from the end of their names."""
    header = raw[0]
    keep = [i for i, name in enumerate(header) if columns is None or name in columns]
    names = [header[i].removesuffix(suffix) if suffix else header[i] for i in keep]
    # Spaces rather than underscores, so a narrow column wraps its name
    # between words.
    names = [name.replace("_", " ") for name in names + ["positions_per_state"]]
    out = ["| " + " | ".join(names) + " |"]
    out.append("|" + "---:|" * (len(keep) + 1))
    for fields, value in zip(raw[1:], per_state):
        out.append("| " + " | ".join([fmt_field(fields[i]) for i in keep] + [value]) + " |")
    out.append("")
    return out


def fmt_field(text):
    # A value the binary printed in scientific notation keeps 2 digits after
    # the decimal point. Integers, such as state counts, stay as printed.
    return f"{float(text):.2e}" if "e" in text else text


def fmt_per_state(positions, states):
    # Positions per DFA state, over every result's positions and states summed.
    if states == 0:
        return "-"
    x = positions / states
    if x < 1e6:
        return f"{x:,.1f}"
    return f"{x:.2e}"


def partition_ok(total, parts):
    # Each figure is rounded to 6 significant digits, so allow for that.
    return abs(sum(parts) - total) <= 2e-5 * max(total, 1) + 0.5


def solved_table(title, command, rows, raw, total_key, plies_expected, missing, elapsed):
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
    out += tsv_table(raw, [fmt_per_state(sum(row[f"{r}_positions"] for r in RESULTS),
                                         sum(row[f"{r}_states"] for r in RESULTS)) for row in rows])

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


def backward_total(game, rows):
    """The number of positions every backward side and ply_max must add up
    to, with a description of where it came from.

    stats_backward prints no total. The game's config shape gives every
    string of the shape. Without a config, fall back to the first row's sum,
    which then checks that every side and ply_max agrees with it."""
    config = os.path.join(SRC_DIR, "config", game, "game.json")
    if os.path.exists(config):
        with open(config) as f:
            shape = json.load(f).get("shape")
        if not shape:
            fail(f"{config} has no shape")
        total = 1
        for size in shape:
            total *= size
        return total, f"all {total:,} strings of the shape in {os.path.relpath(config, SRC_DIR)}"
    first = sum(rows[0][f"{r}_positions_0"] for r in RESULTS)
    return first, f"ply_max {rows[0]['ply']} side 0's sum, {first:g} (no config shape to check against)"


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

    for name, binary, total_key in (
            ("forward_backward", "stats_forward_backward", "reachable"),
            ("bound_backward", "stats_bound_backward", "bound")):
        for f, b in sorted({(int(f), int(b)) for f, b, _, _ in families.get(name, [])}):
            command, rows, raw, missing, elapsed = run_stats(binary, [args.game, f, b])
            out += solved_table(f"{name}, forward_ply_max={f}, backward_ply_max={b}",
                                command, rows, raw, total_key, range(f + 1), missing, elapsed)
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
            command, rows, raw, missing, elapsed = run_stats("stats_backward", [args.game, complete])
            total, total_source = backward_total(args.game, rows)
            mismatches = []
            for row in rows:
                for s in (0, 1):
                    parts = [row[f"{r}_positions_{s}"] for r in RESULTS]
                    if not partition_ok(total, parts):
                        sum_text = " + ".join("%s %g" % (r, p) for r, p in zip(RESULTS, parts))
                        mismatches.append("ply_max %d side %d: %s != %g" % (row["ply"], s, sum_text, total))
            if mismatches:
                fail(f"{command}: winning + losing + unknown positions do not match {total_source}:\n" + "\n".join(mismatches))

            # Both sides in one table is too wide for a terminal to draw.
            # When every value is the same for both sides, as for a game
            # symmetric under swapping the sides, one table covers both.
            columns = {s: [raw[0].index(f"{r}_{q}_{s}") for r in RESULTS for q in ("states", "positions")] for s in (0, 1)}
            same = all([fields[i] for i in columns[0]] == [fields[i] for i in columns[1]] for fields in raw[1:])
            layout = "every value the same for both sides" if same else "one table per side to move"
            out += [f"### backward, ply_max 0-{complete}", "", f"`{command}` ({elapsed:.0f}s), {layout}", ""]
            for s in ((0,) if same else (0, 1)):
                out.append("Both Sides:" if same else f"Side {s}:")
                out.append("")
                out += tsv_table(raw,
                                 [fmt_per_state(sum(row[f"{r}_positions_{s}"] for r in RESULTS),
                                                sum(row[f"{r}_states_{s}"] for r in RESULTS)) for row in rows],
                                 {"ply"} | {f"{r}_{q}_{s}" for r in RESULTS for q in ("states", "positions")},
                                 suffix=f"_{s}")
            plies_max = {int(p) for p, _, _ in families["backward"]}
            beyond = sorted(p for p in plies_max if p > complete)
            notes = [f"{len(rows)} plies reported (ply_max 0-{complete}), the highest complete for both sides and all results."]
            if beyond:
                notes.append(f"`backward` DFAs also exist for ply_max {compress(beyond)}, but not every side and result, so those plies are not shown.")
            notes.append(f"Winning + losing + unknown positions equal {total_source} at every ply_max shown, for both sides.")
            unknown_left = [row["ply"] for row in rows if any(row[f"unknown_positions_{s}"] > 0 for s in (0, 1))]
            notes.append(f"Unknown positions remain at ply_max {compress(unknown_left)}." if unknown_left else "No unknown positions at any ply_max shown.")
            out += notes
            out.append("")
            ran = True

    if "forward" in families and "forward_backward" not in families:
        command, rows, raw, missing, elapsed = run_stats("stats_forward", [args.game])
        out += ["### forward", "", f"`{command}` ({elapsed:.0f}s)", ""]
        out += tsv_table(raw, [fmt_per_state(row["reachable_positions"], row["reachable_states"]) for row in rows])
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
