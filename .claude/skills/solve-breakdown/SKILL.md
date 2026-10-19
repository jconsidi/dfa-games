---
name: solve-breakdown
description: Break down where a solve's union time goes for the latest solver code version, from its profile trace and log. Covers every run whose restart commit has the same solver code as the latest run's, ignoring commits that only touch Markdown such as TODO.md, or files outside src/ and third_party/ such as skills. Also splits one finished union into its forward and backward lookups. Use when asked for a time breakdown, where the time goes, or stats after a union finishes, for a game such as cram_7x7.
argument-hint: "[GAME]"
---

# solve-breakdown

Report where a solve's union time goes, at the code version it is running now.
Run the commands from `src/`.
They only read the profile trace (`GAME.jsonl`, written through `DFA_PROFILE_TRACE`) and the log (`GAME.log`), so they are safe alongside a solve.
`--trace` and `--log` name other files.

## Code version

Each run in the trace is matched to the log's `=== restart ... at COMMIT` line written just before it.
Two runs are at the same code version when `git diff` between their commits shows no change under `src/` or `third_party/` other than Markdown files.
So a commit that only changes `src/TODO.md` or a skill keeps the version, and any change to a solver source, header or the Makefile starts a new one.
The breakdown covers the runs at the latest run's version.
Its first line says whether HEAD still has that solver code, and lists the files that differ if not.

## Breakdown

    python3 ../.claude/skills/solve-breakdown/solve_breakdown.py GAME breakdown

- The runs at this code version, with their commits and how many unions each finished.
- The unions finished at this version, with wall minutes and major faults.
- Minutes, share, and user and sys cores busy per phase group, over those unions: record production (bucket partition, map populate, map write), the sorts, save, load, and `get_union`.
  The time in finished phases outside them, such as loading at each start and unions a stopped run did not finish, is given on one line after.
- The same table for the latest finished union.

With `--wait`, it first waits for the next union to finish, then prints, and exits 3 if the solve exits first.
Run that form with `run_in_background` to report after each union, and start it again after each report.
Each of those reports also runs the union-eta skill's `union_eta.py GAME` and gives the ETA of the union just started.
That ETA is usually by size, since the new union has little timing data yet.

## Lookups of one union

    python3 ../.claude/skills/solve-breakdown/solve_breakdown.py GAME lookups NODE

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

## Present

Show the tables as printed, with a few lines above on what stands out.
Name the code version and the runs it covers.
If the script fails, lead with that and quote its message.

The union-eta skill imports `solve_breakdown.py` for its parsing, so check it still runs after changing this file.
