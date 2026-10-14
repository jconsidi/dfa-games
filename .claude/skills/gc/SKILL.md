---
name: gc
description: Garbage collect src/scratch with gc.sh, after confirming no solver or other src/ binary is running on this host. Use when asked to run gc, clean up scratch, or free disk space between solves.
---

# gc

Run `gc.sh` on `scratch`, but only once nothing built from `src/` is running.

gc is not safe alongside a live solver: the solver unmaps DFAs and reopens them by path, and holds DFAs that only `*_cache` links name, which gc deletes.

## 1. Confirm no solver is running

From `src/`:

    python3 ../.claude/skills/gc/check_no_solver.py

It checks the pid of each staging directory under `scratch/build` and `scratch/binarydfa` with `kill(pid, 0)`, the same test `gc.pl` uses.
A staging directory exists only while a DFA is being built, so a process between builds does not show up.

- **Exit 0** (`no solver running`): go on to step 2.
- **Exit 1** (`RUNNING: ...` lines): stop. Report what is running and do not run gc.
- **Exit 2**: the check could not be done. Stop, quote its message, and do not run gc.

Never treat a check that failed to run as a clean one.

## 2. Run gc

From `src/`:

    ./gc.sh scratch

With no timestamps, `gc.pl` deletes unlinked DFAs and cache entries written up to the newest `move_nodes` link, and removes staging directories whose pid is gone.
It can take a while on a large scratch volume, so run it with `run_in_background` and wait for it.

## 3. Report

- Quote gc's own counts: DFAs kept, found, not saved, deleted, stale temporary files and staging directories removed.
- Show free space on the scratch volume before and after (`df -h scratch/`).
- If gc exited nonzero, lead with that and quote its message.
