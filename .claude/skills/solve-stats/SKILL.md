---
name: solve-stats
description: Report how a game in this repo is or was solved and its per-ply results. Finds the solve families in src/scratch/<game>, runs the matching stats_* binaries, and presents winning/losing/unknown positions and DFA states per ply as tables. Use when asked for solve stats or results for a game such as breakthrough_6x6 or cram_7x7. A status request while a solve is running goes to the union-eta skill instead, and a union time breakdown to solve-breakdown.
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
  Each run's table is the binary's TSV output as a Markdown table, with the TSV's column names, order and values as printed, except that underscores in the names become spaces, so they wrap between words, and values in scientific notation are cut to 2 digits after the decimal point.
  The checks use the values as printed, before that rounding.
  `stats_backward` prints both sides in one row, which is too wide for a terminal to draw, so `backward` gets one table per side to move, each with that side's columns and the `_0` or `_1` suffix dropped from their names.
  When every value is the same for both sides, it prints one table headed "Both Sides" instead.
  The script appends one column, "positions per state": the sum of the row's positions over its results, divided by the sum of their DFA states.
  Before printing, it checks that winning + losing + unknown positions equal the total at every ply, within the binaries' 6 significant digits.
  For `forward_backward` and `bound_backward` the total is the binary's own reachable or bound column.
  `stats_backward` prints no total, so for `backward` the total is the number of strings of the shape in `config/GAME/game.json`, checked for both sides at every ply_max.
  A game without a config is checked against the first row's sum instead, which shows only that every row agrees, and the note says so.
  The notes under each table confirm that check, list missing plies and remaining unknowns, and give ply 0's verdict when ply 0 is a single position.
- Exits nonzero with the reason, and prints no tables, if a binary fails, a binary is not built, the output does not parse, or the positions at some ply do not add up to the total.

The binaries need a DFA's position count.
`DFA::size()` caches it under `scratch/sizes/`, and gc keeps each entry until its DFA is deleted.
An uncached size costs a full digest check and count over the file, so a first run on a large game can take many minutes.
Run the script with `run_in_background` and say so, then report when it finishes.
A second run reads the cached sizes and is fast.

The binaries only read DFAs, so they are safe to run alongside a solve.
They do compete for disk bandwidth, so mention that when a solve is running.

## 3. Present the results

- Show the script's output as it is, including the notes under each table.
  Do not retype or recompute the numbers, rename or reorder columns, or drop columns, rows or notes.
- Above the tables, sum up in a few lines:
  - which solve was run, with its parameters;
  - how far it got, including missing plies or remaining unknown positions;
  - the verdict at ply 0, if the script states one.
- If the script failed, lead with that and quote its message.
- A family the script found but did not run, such as an incomplete `backward`, gets one line saying so.
