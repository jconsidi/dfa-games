#!/bin/zsh
# solve_steps.zsh [--src DIR] [--log FILE] STEP... : runs each STEP in order,
# one at a time, restarting through gc whenever the scratch volume runs low.
#
# A STEP is one argument, "PROGRAM GAME [ARG...]", such as
# "build_bound_backward amazons_4x6 16 0" or
# "verify_proof amazons_4x6 proof_bound_backward,forward_ply_max=016,backward_ply_max=000".
# PROGRAM runs from DIR as ./PROGRAM GAME ARG....
#
# --src DIR is the src/ directory the programs run from and whose scratch gc
# runs on; it defaults to this checkout's src/.
# --log FILE is the driver's own log; it defaults to DIR/solve_steps.log.
#
# - Before the first step, no other solver may be running, since the driver
#   runs gc between attempts.
# - Before each attempt, gc runs if under 1 TiB is free.
# - During an attempt, free space is checked every 20 s. Under 100 GB the
#   step is killed, gc runs, and the step runs again. Every program here
#   saves its named DFAs as it goes and loads them on the next run, so the
#   attempt resumes from what the killed one saved.
# - An attempt that started after gc and is killed without saving a named DFA
#   or move graph node for its game stops the driver: gc and restart then
#   free too little to make progress.
#
# Each step's output, stdout and stderr, is appended to DIR/GAME.PROGRAM.log
# under a "=== TIME attempt N: COMMAND" header. gc's output is appended to
# DIR/GAME.gc.log under a header per run.
#
# Exit status: 0 once every step succeeds, 2 for a usage error, 3 if the
# solver check does not pass, 4 if gc fails, 5 on giving up, 6 if a step
# fails other than by being killed.
#
# SOLVE_STEPS_GC_GB and SOLVE_STEPS_KILL_GB override the 1024 and 100 GB
# thresholds, for testing the driver.

# zsh runs background jobs at nice 5 by default; steps run at full priority.
setopt NO_BG_NICE

GC_GB=${SOLVE_STEPS_GC_GB:-1024}
KILL_GB=${SOLVE_STEPS_KILL_GB:-100}

SKILLS=${0:A:h:h}
SRC=${0:A:h:h:h:h}/src
LOG=

while [[ $1 == --* ]]; do
  case $1 in
    --src) SRC=${2:A}; shift 2 ;;
    --log) LOG=${2:A}; shift 2 ;;
    *) echo "unknown option $1" >&2; exit 2 ;;
  esac
done
if (( $# == 0 )); then
  echo "usage: solve_steps.zsh [--src DIR] [--log FILE] \"PROGRAM GAME [ARG...]\"..." >&2
  exit 2
fi
[[ -n $LOG ]] || LOG=$SRC/solve_steps.log
for step in "$@"; do
  words=(${(z)step})
  if (( ${#words} < 2 )); then
    echo "step \"$step\" is not PROGRAM GAME [ARG...]" >&2
    exit 2
  fi
  if [[ ! -x $SRC/${words[1]} ]]; then
    echo "no ${words[1]} in $SRC" >&2
    exit 2
  fi
done

free_gb() { df -g $SRC/scratch/ | awk 'NR==2 {print $4}' }
stamp() { date '+%Y-%m-%d %H:%M:%S' }
say() { echo "$(stamp) $*" | tee -a $LOG }

# saved GAME LOG LINE: named DFAs and move graph nodes for GAME that LOG
# records saving after line LINE. These are what gc keeps.
saved() { tail -n +$(( $3 + 1 )) $2 | grep -cE "^built ($1/|move_nodes/$1,)" }

# solver_check LABEL: stops the driver unless no solver is running.
solver_check() {
  local out
  out=$(python3 $SKILLS/gc/check_no_solver.py --scratch $SRC/scratch 2>&1)
  local rc=$?
  # macOS python3 can print xcrun cache warnings that are not the check's
  out=$(print -r -- "$out" | grep -v 'xcrun_db')
  print -r -- "$out" | sed 's/^/  /' | tee -a $LOG
  if (( rc != 0 )); then
    say "$1: solver check did not pass, stopping"
    exit 3
  fi
}

# run_gc LABEL GAME
run_gc() {
  say "$1: $(free_gb) GB free, running gc"
  solver_check "$1"
  echo "=== $(stamp) gc for $1" >> $SRC/$2.gc.log
  if ! (cd $SRC && ./gc.sh scratch) >> $SRC/$2.gc.log 2>&1; then
    say "$1: gc failed, see $SRC/$2.gc.log"
    exit 4
  fi
  say "gc done, $(free_gb) GB free"
}

say "starting ${#@} step(s) from $SRC"
solver_check "start"

for step in "$@"; do
  words=(${(z)step})
  program=${words[1]}
  game=${words[2]}
  log=$SRC/$game.$program.log
  say "$step: starting, log $log"
  step_start=$SECONDS
  attempt=1
  after_gc=0
  while true; do
    if (( $(free_gb) < GC_GB )); then
      run_gc "$step, attempt $attempt" $game
      after_gc=1
    fi

    echo "=== $(stamp) attempt $attempt: $step" >> $log
    log_lines=$(( $(wc -l < $log) ))
    start=$SECONDS
    min_free=$(free_gb)
    (cd $SRC && exec ./$program ${words[2,-1]}) >> $log 2>&1 &
    pid=$!
    killed=0
    ticks=0
    while kill -0 $pid 2>/dev/null; do
      sleep 1
      (( ++ticks % 20 == 0 )) || continue
      free=$(free_gb)
      (( free < min_free )) && min_free=$free
      if (( free < KILL_GB && ! killed )); then
        say "$step, attempt $attempt: ${free} GB free, killing it"
        kill $pid
        killed=1
      fi
    done
    wait $pid
    rc=$?
    n=$(saved $game $log $log_lines)
    say "$step, attempt $attempt: exit $rc after $(( (SECONDS - start) / 60 )) min, lowest free ${min_free} GB, saved $n DFA(s)"

    (( killed )) || break
    if (( after_gc && n == 0 )); then
      say "$step: the attempt after gc saved nothing, giving up"
      exit 5
    fi
    run_gc "$step, after attempt $attempt" $game
    after_gc=1
    (( attempt++ ))
  done

  if (( rc != 0 )); then
    say "$step: FAILED with exit $rc: $(tail -1 $log)"
    exit 6
  fi
  say "$step: done in $(( (SECONDS - step_start) / 60 )) min over $attempt attempt(s)"
done

say "all steps done"
