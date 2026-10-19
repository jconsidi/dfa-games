---
name: solve-stats
description: Report how a game in this repo is or was solved and its per-ply results. Finds the solve families in src/scratch/<game>, runs the matching stats_* binaries, and presents winning/losing/unknown positions and DFA states per ply as tables. Also breaks down where a solve's union time goes, from its profile trace and log. Use when asked for solve stats, results, or progress for a game such as breakthrough_6x6 or cram_7x7, or for a time breakdown of its unions.
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
  Before printing, it checks that winning + losing + unknown positions equal the total at every ply, within the binaries' 6 significant digits.
  The notes under each table confirm that check, list missing plies and remaining unknowns, and give ply 0's verdict when ply 0 is a single position.
- Exits nonzero with the reason, and prints no tables, if a binary fails, a binary is not built, the output does not parse, or the positions at some ply do not add up to the total.

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
- If the script failed, lead with that and quote its message.
- A family the script found but did not run, such as an incomplete `backward`, gets one line saying so.

## 4. Union time breakdown

When asked where a solve's time goes, use `union_profile.py` from `src/`.
It reads the profile trace (`GAME.jsonl`, written through `DFA_PROFILE_TRACE`) and the solve's log (`GAME.log`); `--trace` and `--log` name others.
It only reads those files, so it is safe alongside a solve.

    python3 ../.claude/skills/solve-stats/union_profile.py GAME breakdown

- Lists the unions finished in the trace's last run, with wall minutes and major faults.
- Then minutes, share and user and sys cores busy per phase group: record production (bucket partition, map populate, map write), the sorts, save, load, and `get_union`.
  One table covers the whole run, and one the latest union.
- With `--wait`, it first waits for the next union to finish, then prints, and exits 3 if the solve exits first.
  Run that form with `run_in_background` to report after each union, and start it again after each report.

    python3 ../.claude/skills/solve-stats/union_profile.py GAME lookups NODE

- One finished union, such as `begin.join_24_32`, split into its forward and backward passes and the BinaryDFA record producer's lookups:
  1. the left lookup into the left input;
  2. the right lookup into the right input;
  3. the backward pass's rebuild of the transition pairs, which repeats 1 and 2;
  4. the backward pass's next pair lookup in `transition_for`.
  The prep phases and the sorts are their own rows.
- Each division gets minutes, share of the union, cores busy, and its own major faults, excluding its children's.
- Per layer, it gives each lookup's minutes and faults, the backward pass's in-order input and table sizes, and the input DFAs' layer sizes while their files are still in `scratch/dfas_by_hash`.
- Faults are shown as GB at 16 KiB each, a lower bound on what was read, since the trace's `inblock` is 0 on macOS.
  Say so when quoting them.

Present the tables as printed, with a few lines above on what stands out.
