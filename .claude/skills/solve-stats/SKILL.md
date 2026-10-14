---
name: solve-stats
description: Report how a game in this repo is or was solved and its per-ply results. Finds the solve families in src/scratch/<game>, runs the matching stats_* binaries, and presents winning/losing/unknown positions and DFA states per ply as tables. Use when asked for solve stats, results, or progress for a game such as breakthrough_6x6 or cram_7x7.
argument-hint: "[GAME]"
---

# solve-stats

Report the solve results for one game.

## 1. Pick the game

Use the argument if one was given, for example `/solve-stats breakthrough_6x6`.
Otherwise take the game the conversation is about, such as the one a running solve is working on.
If no single game is clear, ask which one.

## 2. Run the script

From `src/`:

    python3 ../.claude/skills/solve-stats/solve_stats.py GAME

The script:

- Lists `scratch/GAME/` and groups the entries into solve families: `forward`, `backward`, `forward_backward`, `bound_backward`, `component_bound`, and other.
  It counts dangling links in each family.
- Runs the stats binary for each family it finds:
  - `stats_forward_backward GAME F B` for each `forward_backward` parameter pair;
  - `stats_bound_backward GAME F B` for each `bound_backward` parameter pair;
  - `stats_backward GAME N`, where N is the highest ply_max complete for both sides and all results;
  - `stats_forward GAME`, only when there is no `forward_backward` family, whose reachable column already covers it.
- Prints Markdown: the family summary, then one table per run.
  The tables give positions and DFA states per ply.
  They also check that winning + losing + unknown positions equal the total, within the binaries' 6 significant digits.
  The notes under each table list missing plies, remaining unknowns, and ply 0's verdict when ply 0 is a single position.
- Exits nonzero with the reason if a binary fails, a binary is not built, the output does not parse, or the partition check fails.

The binaries need a DFA's position count.
`DFA::size()` caches it under `scratch/sizes/`, and gc ages those entries out.
An uncached size costs a full digest check and count over the file, so a first run on a large game can take many minutes.
Run the script with `run_in_background` and say so, then report when it finishes.
A second run reads the cached sizes and is fast.

The binaries only read DFAs, so they are safe to run alongside a solve.
They do compete for disk bandwidth, so mention that when a solve is running.

## 3. Present the results

- Show the script's tables as they are.
  Do not retype or recompute the numbers.
- Above the tables, sum up in a few lines:
  - which solve was run, with its parameters;
  - how far it got, including missing plies or remaining unknown positions;
  - the verdict at ply 0, if the script states one.
- If the script failed or flagged a mismatch, lead with that and quote its message.
  Do not present partial tables as a finished report.
- A family the script found but did not run, such as an incomplete `backward`, gets one line saying so.
