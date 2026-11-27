#!/usr/bin/env python3
"""Solve breakthrough_4x4 completely from an empty scratch root, verify it,
and check the results against the recorded hashes and stats.

Usage, from a src/ directory with the binaries built:

    python3 .../breakthrough_4x4_from_scratch.py [--bin DIR] [--root DIR]
        [--keep] [--compare ROOT] [--record]

Exits 0 only if every step exits 0, every verifier prints its success line,
and every recorded hash and stats output matches (or, with --record, after
writing them). Any failure exits 1 with the reason.
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

GAME = "breakthrough_4x4"
SKILL_DIR = os.path.dirname(os.path.abspath(__file__))
EXPECTED_FILE = os.path.join(SKILL_DIR, "expected.json")

FORWARD = "024"
BACKWARD = "000"
PROOF_PREFIX = f"proof_forward_backward,forward_ply_max={FORWARD},backward_ply_max={BACKWARD}"

# (program, arguments, regex its output must match, or None). "{B}" is the
# last backward ply, read from what build_backward saved.
STEPS = [
    ("build_backward", [GAME, "100"], None),
    ("solve_backward", [GAME, "{B}"], None),
    ("verify_backward", [GAME, "{B}"], r"^VERIFIED BACKWARD SOLVE "),
    ("build_forward", [GAME], None),
    ("build_forward_backward", [GAME, "24", "0"], None),
    ("verify_forward_backward", [GAME, "24", "0"], r"^VERIFIED SOLVE PLIES "),
    ("build_forward_backward_proof", [GAME, "24", "0"], None),
    ("verify_proof", [GAME, PROOF_PREFIX], r"^PROOF VERIFIED: "),
    ("stats_backward", [GAME, "{B}"], None),
    ("stats_forward_backward", [GAME, "24", "0"], None),
]

STATS_STEPS = ["stats_backward", "stats_forward_backward"]

# The results whose hashes are recorded: both sides' final backward solve,
# ply 0 of the forward_backward result, and every ply of the proof, which
# holds one of winning or losing per ply.
def result_names(game_dir, backward_ply):
    names = ([f"backward,ply_max={backward_ply:03d},side={s},{r}" for s in (0, 1) for r in ("winning", "losing", "unknown")]
             + [f"forward_backward,forward_ply_max={FORWARD},backward_ply_max={BACKWARD},ply=000,{r}" for r in ("winning", "losing", "unknown")])
    proof = sorted(n for n in os.listdir(game_dir) if n.startswith(PROOF_PREFIX + ","))
    if not proof:
        fail("no proof DFAs were saved")
    return names + proof


def last_backward_ply(game_dir):
    """The highest ply_max with winning, losing and unknown saved for both sides."""
    plies = set()
    for name in os.listdir(game_dir):
        match = re.match(r"^backward,ply_max=(\d+),side=0,winning$", name)
        if match:
            plies.add(int(match.group(1)))
    complete = [p for p in plies
                if all(os.path.islink(os.path.join(game_dir, f"backward,ply_max={p:03d},side={s},{r}"))
                       for s in (0, 1) for r in ("winning", "losing", "unknown"))]
    if not complete:
        fail("build_backward saved no complete ply")
    return max(complete)


def fail(message):
    print(f"FAILED: {message}")
    sys.exit(1)


def link_hash(path):
    target = os.readlink(path)
    match = re.search(r"([0-9a-f]{64})\.dfa$", target)
    if not match:
        fail(f"{path} points at {target}, not a dfas_by_hash file")
    return match.group(1)


def all_links(root):
    """Every symbolic link under root except dfas_by_hash, as name -> hash."""
    links = {}
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in ("dfas_by_hash", "logs", "sizes")]
        for name in filenames:
            path = os.path.join(dirpath, name)
            if os.path.islink(path):
                links[os.path.relpath(path, root)] = link_hash(path)
    return links


def main():
    # progress lines appear as each step finishes, even into a file
    sys.stdout.reconfigure(line_buffering=True)

    parser = argparse.ArgumentParser()
    parser.add_argument("--bin", default=".", help="directory holding the binaries, run from there (default: .)")
    parser.add_argument("--root", help="empty or new scratch root (default: a new directory under $TMPDIR)")
    parser.add_argument("--keep", action="store_true", help="keep the scratch root afterward")
    parser.add_argument("--compare", help="another run's kept root: every link present in both must match")
    parser.add_argument("--record", action="store_true", help="write the hashes and stats as the expected ones")
    args = parser.parse_args()

    bin_dir = os.path.abspath(args.bin)
    for program, _, _ in STEPS:
        if not os.access(os.path.join(bin_dir, program), os.X_OK):
            fail(f"{program} is not built in {bin_dir}")

    if args.root:
        root = os.path.abspath(args.root)
        os.makedirs(root, exist_ok=True)
        if os.listdir(root):
            fail(f"{root} is not empty")
    else:
        root = tempfile.mkdtemp(prefix=f"{GAME}-from-scratch-", dir=os.environ.get("TMPDIR"))
    logs = os.path.join(root, "logs")
    os.makedirs(logs)
    print(f"scratch root {root}")

    env = dict(os.environ, DFA_LOCAL_DIR=root, DFA_ARCHIVE_DIR=root)
    outputs = {}
    started = time.time()
    game_dir = os.path.join(root, GAME)
    backward_ply = None
    for program, program_args, success in STEPS:
        if "{B}" in program_args:
            program_args = [str(backward_ply) if a == "{B}" else a for a in program_args]
        step_started = time.time()
        log_path = os.path.join(logs, f"{program}.log")
        with open(log_path, "w") as log:
            result = subprocess.run([os.path.join(bin_dir, program)] + program_args,
                                    cwd=bin_dir, env=env, stdout=log, stderr=subprocess.STDOUT)
        with open(log_path) as log:
            output = log.read()
        outputs[program] = output
        if result.returncode != 0:
            tail = "\n".join(output.splitlines()[-10:])
            fail(f"{program} {' '.join(program_args)} exited {result.returncode}; log {log_path}:\n{tail}")
        line = ""
        if success:
            matches = [l for l in output.splitlines() if re.search(success, l)]
            if not matches:
                fail(f"{program} printed no line matching {success!r}; log {log_path}")
            line = matches[-1]
        print(f"ok {program} {' '.join(program_args)} ({time.time() - step_started:.0f}s) {line}")
        if program == "build_backward":
            backward_ply = last_backward_ply(game_dir)
            print(f"   last complete backward ply: {backward_ply}")

    hashes = {}
    for name in result_names(game_dir, backward_ply):
        path = os.path.join(game_dir, name)
        if not os.path.islink(path):
            fail(f"result {name} was not saved")
        hashes[name] = link_hash(path)

    stats = {program: [l for l in outputs[program].splitlines() if not re.match(r"^(loaded|building|built|load_or_build|Profile|MemoryMap|serialize|calculate_digest)", l)]
             for program in STATS_STEPS}

    if args.record:
        with open(EXPECTED_FILE, "w") as f:
            json.dump({"hashes": hashes, "stats": stats}, f, indent=1, sort_keys=True)
            f.write("\n")
        print(f"recorded {len(hashes)} hashes and {len(stats)} stats outputs in {EXPECTED_FILE}")
    else:
        if not os.path.exists(EXPECTED_FILE):
            fail(f"no {EXPECTED_FILE}; run with --record first")
        with open(EXPECTED_FILE) as f:
            expected = json.load(f)
        bad = [f"{name}: expected {expected['hashes'].get(name)}, got {h}"
               for name, h in hashes.items() if expected["hashes"].get(name) != h]
        if bad:
            fail("result hashes differ from the recorded ones:\n" + "\n".join(bad))
        for program in STATS_STEPS:
            if expected["stats"].get(program) != stats[program]:
                fail(f"{program} output differs from the recorded one; log {os.path.join(logs, program + '.log')}")
        print(f"all {len(hashes)} result hashes and {len(stats)} stats outputs match the recorded ones")

    if args.compare:
        mine = all_links(root)
        theirs = all_links(os.path.abspath(args.compare))
        differ = sorted(n for n in mine.keys() & theirs.keys() if mine[n] != theirs[n])
        only_mine = sorted(mine.keys() - theirs.keys())
        only_theirs = sorted(theirs.keys() - mine.keys())
        print(f"compare: {len(mine.keys() & theirs.keys())} links in both, {len(differ)} differ, "
              f"{len(only_mine)} only here, {len(only_theirs)} only in {args.compare}")
        for label, names in (("only here", only_mine), ("only there", only_theirs)):
            for name in names[:20]:
                print(f"  {label}: {name}")
            if len(names) > 20:
                print(f"  {label}: ... {len(names) - 20} more")
        if differ:
            fail("links differ:\n" + "\n".join(f"{n}: {theirs[n]} -> {mine[n]}" for n in differ[:50]))

    print(f"PASSED {GAME} from scratch in {time.time() - started:.0f}s")
    if args.keep:
        print(f"kept {root}")
    else:
        shutil.rmtree(root)


if __name__ == "__main__":
    main()
