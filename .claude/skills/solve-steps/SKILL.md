---
name: solve-steps
description: Run a fixed list of solver programs in order, such as build_bound_backward, stats_bound_backward, build_bound_backward_proof and verify_proof for one or more games, killing a step when scratch runs low, running gc, and running the step again from what it saved. Use for bound_backward and forward_backward solves, building proofs, and checking them, instead of writing a one-off driver script. solve_backward's ply by ply solve goes to the solve-restart skill instead.
---

# solve-steps

Run solver programs one after another, restarting each through gc whenever the scratch volume runs low.
A step is one program run, `PROGRAM GAME [ARG...]`.
Every program here saves its named DFAs as it builds them and loads them on the next run, so a restart resumes from what the killed attempt saved.

## 1. Plan the steps

Write out the steps before starting, in the order they must run.
The usual chains, from `src/`:

- **bound_backward solve, proof and check** (amazons and other games with `component_bound` configs):
  1. `build_bound_backward GAME F 0`
  2. `stats_bound_backward GAME F 0`
  3. `build_bound_backward_proof GAME F 0`
  4. `verify_proof GAME proof_bound_backward,forward_ply_max=FFF,backward_ply_max=000`
- **forward_backward solve, proof and check**:
  1. `build_forward GAME F`
  2. `build_forward_backward GAME F B`
  3. `stats_forward_backward GAME F B`
  4. `build_forward_backward_proof GAME F B`
  5. `verify_proof GAME proof_forward_backward,forward_ply_max=FFF,backward_ply_max=BBB`

F and B are the ply limits, and FFF and BBB the same zero-padded to three digits.
Leave out steps whose results already exist; `ls scratch/GAME` shows them.
Proofs come only from `forward_backward` and `bound_backward` solves, never from `solve_backward`.

Before starting:

- Build the programs in the `src/` they will run from (`make -j8`).
- Confirm no solver is running, with step 1 of the `gc` skill.
  The driver checks this too and refuses to start otherwise, since it runs gc between attempts.
- Note free space with `df -g scratch/`.

## 2. Run the driver

From the repository root, with `run_in_background`, one quoted argument per step:

    zsh .claude/skills/solve-steps/solve_steps.zsh [--src DIR] [--log FILE] "STEP" "STEP"...

Steps run one at a time, never in parallel, so their peak space does not add up, and the driver stops at the first step that fails.

- Before each attempt, gc runs if under 1 TiB is free.
- During an attempt, free space is checked every 20 s. Under 100 GB the step is killed, gc runs, and the step runs again.
- It gives up (exit 5) only when an attempt that started after gc is killed without saving anything: no `built GAME/…` or `built move_nodes/GAME,…` line in the step's log.
- It stops on anything else: the solver check not passing (3), gc failing (4), or a step failing other than by being killed (6).

`--src` defaults to this checkout's `src/`.
When running the script from a worktree, pass the `src/` whose binaries and scratch should be used.

Logs, each appended to across runs:

- `--log FILE`, by default `DIR/solve_steps.log`: the driver's lines, one per step start and finish, attempt, kill and gc, also printed on stdout.
  Give a batch its own `--log`, such as `src/amazons.steps.log`, so its history stays together.
- `DIR/GAME.PROGRAM.log`: each step's output, under a `=== TIME attempt N: STEP` header per attempt.
- `DIR/GAME.gc.log`: gc's output, under a header per run.

To watch for milestones, wait in the background on the driver log for lines matching `done in|FAILED|killing|giving up|did not pass|gc failed|all steps done`.
To stop the batch, stop the driver's background task; that also kills the step it is running.
A union cannot resume from partial results, so stopping mid-union loses that union's work, though not the DFAs saved before it.

To test a change to the driver, run it on a stub `src/` holding small scripts in place of the programs and of `gc.sh`, an empty `scratch/build` and `scratch/binarydfa`, never on the real scratch.
`SOLVE_STEPS_GC_GB` and `SOLVE_STEPS_KILL_GB` override the two thresholds, so a test can force a kill and a gc.

## 3. Report

- When the driver exits, lead with its exit status and last line.
- For each finished step, its minutes and attempts.
- For a solve, the stats step's table, via the `solve-stats` skill.
- For a proof check, the `PROOF VERIFIED` line: plies, positions, and the initial position's result.
  Compare that result with any published or expected result, such as `test_solved.cpp`'s, and say plainly whether it matches.
- If a step failed, quote the end of its log.
