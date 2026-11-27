---
name: breakthrough-4x4-from-scratch
description: Solve breakthrough_4x4 completely from an empty scratch root with the binaries in a given src directory, run every verifier on it, and check the final results' hashes and the stats against the recorded ones. Use before committing solver or DFA changes, to compare two builds (A/B), or when asked for a from-scratch breakthrough_4x4 solve or test.
---

# breakthrough-4x4-from-scratch

A complete breakthrough_4x4 solve in a fresh scratch root, so nothing is loaded from caches built by other code, with every verifier, checked against recorded results.

## 1. Run it

From the `src/` directory whose binaries are under test (built with `make -j8`):

    python3 <path to this skill>/breakthrough_4x4_from_scratch.py

It takes about 9 minutes, so run it with `run_in_background` and report when it finishes.
In a worktree, use that worktree's copy of the skill and binaries, or pass `--bin`.

Options:
- `--bin DIR`: the directory holding the binaries; they run from there, so `DIR/../config` must exist (default `.`).
- `--root DIR`: the scratch root, which must be empty or new (default a new directory under `$TMPDIR`).
- `--keep`: keep the root afterward, for `--compare` or inspection. A failed run always keeps it.
- `--compare ROOT`: also compare every saved name (results, `move_nodes`, caches) with another run's kept root; names in both must point at the same DFA, and names on only one side are listed.
- `--record`: write this run's hashes and stats as the expected ones, instead of checking.

The steps, each with `DFA_LOCAL_DIR` and `DFA_ARCHIVE_DIR` set to the root:
1. `build_backward breakthrough_4x4 100`, which builds both sides every ply and stops at its fixed point (ply 14), then `solve_backward breakthrough_4x4 14` over that, then `verify_backward breakthrough_4x4 14`;
2. `build_forward breakthrough_4x4`, `build_forward_backward breakthrough_4x4 24 0`, `verify_forward_backward breakthrough_4x4 24 0`;
3. `build_forward_backward_proof breakthrough_4x4 24 0`, `verify_proof` on its proof;
4. `stats_backward` and `stats_forward_backward`.

`bound_backward` needs a config-based game, which breakthrough becomes on the `breakthrough-config` branch; add its build, verify and stats steps once that is on main.

## 2. What passing means

- Every step exits 0, and every verifier prints its success line (quoted in the output).
- The final results' hashes match `expected.json`: both sides' `backward,ply_max=014` winning, losing and unknown sets, and ply 0 of the `forward_backward` and proof results.
- The two stats outputs match the recorded ones line for line, apart from loading and profiling lines.
- Views (DFAs computed on demand) are written only when saved as game results: no `saving view` log line names a cache or `move_nodes` entry, and there are no more `materialized view` lines than `saving view` ones. The counts are printed either way.
- With `--compare`, no name present in both roots points at different DFAs.

The last line is `PASSED breakthrough_4x4 from scratch in Ns`; anything else is a failure, printed as `FAILED: <reason>` with the log to look at under `ROOT/logs/`.

## 3. When the hashes differ

A changed hash with matching stats means the same sets in different bytes, such as a new state numbering.
That can be intended, but it changes every cache key built from those DFAs, so say so plainly and ask before re-recording with `--record`.
Matching stats are not proof of identical languages; for a changed hash, compare the old and new DFAs' languages before calling it benign.
A stats difference is a wrong result until shown otherwise.

## 4. A/B comparison of two builds

Build the baseline from git as CLAUDE.md describes (`git archive <commit> src config`, `make -j8`), then:

    python3 .../breakthrough_4x4_from_scratch.py --bin BASE/src --root ROOT_A --keep
    python3 .../breakthrough_4x4_from_scratch.py --bin NEW/src --root ROOT_B --keep --compare ROOT_A

Remove both roots afterward.
