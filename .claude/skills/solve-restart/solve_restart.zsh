#!/bin/zsh
# solve_restart.zsh [--src DIR] GAME... : solve each game with solve_backward,
# one ply at a time and one game at a time, restarting through gc whenever
# the scratch volume runs low.
#
# --src DIR is the src/ directory whose solve_backward runs, such as a
# worktree's; it defaults to this checkout's src/. Its scratch must be this
# checkout's scratch, which gc runs on.
#
# - Before each attempt, gc runs if under 1 TiB is free.
# - During an attempt, the solve is killed if under 100 GB is free. gc then
#   runs and the same ply restarts, resuming from the move graph nodes the
#   killed attempt saved.
# - The driver gives up only when an attempt that started after gc is killed
#   without saving a new move graph node: gc and restart then free too
#   little to make progress.
#
# Logs go to the --src directory, each appended to across runs:
# - GAME.log: solve_backward's output, stdout and stderr.
# - GAME.restart.log: this driver's lines, one per attempt and per gc, also
#   printed on stdout.
# - GAME.gc.log: gc's output, under a header line per run.
#
# Exit status: 0 once every game is decided, 2 for a usage error, 3 if the
# solver check does not pass, 4 if gc fails, 5 on giving up, 6 if a solve
# fails other than by being killed.

MAIN=${0:A:h:h:h:h}/src
SRC=$MAIN

if [[ $1 == --src ]]; then
  SRC=${2:A}
  shift 2
fi
if (( $# == 0 )); then
  echo "usage: solve_restart.zsh [--src DIR] GAME..." >&2
  exit 2
fi
if [[ ! -x $SRC/solve_backward ]]; then
  echo "no solve_backward in $SRC" >&2
  exit 2
fi
src_scratch=$SRC/scratch
main_scratch=$MAIN/scratch
if [[ ${src_scratch:A} != ${main_scratch:A} ]]; then
  echo "$SRC/scratch is not $MAIN/scratch, which gc runs on" >&2
  exit 2
fi

free_gb() { df -g $MAIN/scratch/ | awk 'NR==2 {print $4}' }
stamp() { date '+%Y-%m-%d %H:%M:%S' }
saved_nodes() { ls $MAIN/scratch/move_nodes | grep -c "^$1," }

# say LINE: print a timestamped line and append it to the game's restart log.
say() { echo "$(stamp) $*" | tee -a $restart_log }

run_gc() {
  say "$1: $(free_gb) GB free, running gc"
  (cd $MAIN && python3 ../.claude/skills/gc/check_no_solver.py) 2>&1 | tee -a $restart_log
  if (( pipestatus[1] != 0 )); then
    say "solver check did not pass, stopping"
    exit 3
  fi
  echo "=== $(stamp) gc for $1" >> $gc_log
  if ! (cd $MAIN && ./gc.sh scratch) >> $gc_log 2>&1; then
    say "gc failed, see $gc_log"
    exit 4
  fi
  say "gc done, $(free_gb) GB free"
}

for game in "$@"; do
  log=$SRC/$game.log
  restart_log=$SRC/$game.restart.log
  gc_log=$SRC/$game.gc.log
  say "$game: solving from $SRC, logs $log, $restart_log and $gc_log"
  for ply in {0..100}; do
    attempt=1
    after_gc=0
    while true; do
      if (( $(free_gb) < 1024 )); then
        run_gc "$game-ply$ply-attempt$attempt"
        after_gc=1
      fi

      nodes_before=$(saved_nodes $game)
      start=$(date +%s)
      min_free=$(free_gb)
      (cd $SRC && exec ./solve_backward $game $ply) >> $log 2>&1 &
      pid=$!
      killed=0
      while kill -0 $pid 2>/dev/null; do
        sleep 20
        free=$(free_gb)
        (( free < min_free )) && min_free=$free
        if (( free < 100 && ! killed )); then
          say "$game ply $ply attempt $attempt: ${free} GB free, killing the solve"
          kill $pid
          killed=1
        fi
      done
      wait $pid
      rc=$?
      nodes_after=$(saved_nodes $game)
      say "$game ply $ply attempt $attempt: exit $rc after $(( $(date +%s) - start ))s, lowest free ${min_free} GB, move graph nodes saved $nodes_before -> $nodes_after"

      (( killed )) || break
      if (( after_gc && nodes_after <= nodes_before )); then
        say "$game ply $ply: the attempt after gc saved no new move graph node, giving up"
        exit 5
      fi
      run_gc "$game-ply$ply-after-attempt$attempt"
      after_gc=1
      (( attempt++ ))
    done

    if (( rc == 0 )); then
      say "$game: $(grep -E "wins in|loses in" $log | tail -1)"
      break
    fi
    if ! grep -q "^solution not found within $ply ply\.$" $log; then
      say "$game ply $ply: failed, see $log"
      exit 6
    fi
  done
done
