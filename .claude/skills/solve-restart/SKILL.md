---
name: solve-restart
description: Run solve_backward on one or more games, one ply and one game at a time, killing the solve when scratch runs low, running gc, and restarting the same ply from its saved move graph nodes. Use when asked to solve a game backward and keep it going through space shortages, or to keep restarting a solve.
---

# solve-restart

Solve games with `solve_backward`, restarting through gc whenever the scratch volume runs low.
Each restart resumes from the move graph nodes already saved under `scratch/move_nodes`, so what gc can free grows as the solve gets further through the move graph.

## 1. Check before starting

- Confirm no solver is running, with step 1 of the `gc` skill.
  The driver runs gc between attempts, which is only safe when it is the only solver.
- Build `solve_backward` in the `src/` it will run from (`make -j8 solve_backward`).
  For a worktree, pass its `src/` as `--src`; its `scratch` must be this checkout's `scratch`, which the driver checks.

## 2. Run the driver

From the repository root, with `run_in_background`:

    zsh .claude/skills/solve-restart/solve_restart.zsh [--src DIR] GAME...

Games run one after another, never in parallel, so their peak space does not add up.
For each game, it calls `solve_backward GAME PLY` for ply 0, 1, 2, …; earlier plies load from cache.

- Before each attempt, gc runs if under 1 TiB is free.
- During an attempt, free space is checked every 20 s. Under 100 GB the solve is killed, gc runs, and the same ply restarts.
- It gives up (exit 5) only when an attempt that started after gc is killed without saving a new move graph node.
- It stops on anything else: the solver check not passing (3), gc failing (4), or a solve failing other than by being killed (6).

Logs go to the `--src` directory (by default `src/`), each appended to across runs:

- `GAME.log`: the solve's output, stdout and stderr.
- `GAME.restart.log`: the driver's lines, one per attempt with its exit status, time, lowest free space and move graph nodes saved, plus one per gc. The driver also prints these on stdout.
- `GAME.gc.log`: gc's output, under a header line per run.

## 3. Report

- When the driver exits, lead with its exit status and the result line (`P1 wins in N ply.` or `P1 loses in N ply.`), or why it stopped.
- Give per-ply times, the number of restarts, and the lowest free space.
- For state counts per ply, use the `solve-stats` skill.
