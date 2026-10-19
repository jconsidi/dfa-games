---
name: union-eta
description: Predict when the union a running solve is building will finish, by comparing its progress layer by layer with the most similar finished unions in the profile trace and log. Also reports whether the solve is running and free scratch space. Use for any generic status request while a solve is running, and whenever asked for an ETA or progress of a union, for a game such as cram_7x7.
argument-hint: "[GAME]"
---

# union-eta

Report a running solve's status and when its current union will finish.

## 1. Pick the game

Use the argument if one was given.
Otherwise take the game whose solve is running, as named in the conversation.
If no single game is clear, ask which one.

## 2. Run the script

From `src/`:

    python3 ../.claude/skills/union-eta/union_eta.py GAME

It only reads the log, the trace and the scratch volume's free space, so it is safe alongside a solve and takes about a second.
`--trace` and `--log` name other files, and `--ref NODE` picks the reference union.
It imports the solve-breakdown skill's `solve_breakdown.py` for its parsing.

- Whether the solve is running, from the log's last `===` line, how long ago the log was last written, and its last line.
  If the solve is not running, it stops there.
- Free space on scratch.
- The union in progress since the last restart: its pass, the layers done, its input state counts, and when it started.
- The closest references among all finished unions in the trace, with each one's run, commit, whether it has the same solver code (as the solve-breakdown skill defines it), how far off it is, its minutes, and its own ETA.
  "Off by" is the mean distance between the two unions' unique pairs per forward layer, over the layers the current union has finished, or between their input state counts before it has finished any.
- Per layer against the best reference, or `--ref`: forward and backward seconds, the pairs each forward layer made for the next, and the unique pairs after the sort, each with its ratio.
- The ETA with a range.
  It scales the reference's time left, after the latest layer both have finished, by the ratio of the two unions' times up to that layer.
  The range uses the lowest and highest per layer time ratios, over layers of 5 minutes or more in the reference.
- Early in a union, the ETA goes by size instead: until the layers both have finished cover 15% of the reference's time, when loading and small layers dominate.
  - It scales each reference's minutes linearly by the ratio of input states.
  - It counts from the union's start, or from the previous union's end in this run before any layer has finished.
  - The references are the closest by input states, preferring same-code ones within 25% of the size, since older code ran slower.
  - While only one input has loaded, it compares that input with each reference's first.
  - The range spans the references listed.

## 3. Present

Lead with:

- running or not;
- the union, pass and layer;
- the ETA, its range, and the reference it came from.

Then the per layer rows that matter, and what stands out, such as time ratios drifting away from 1.
A reference at older code can be close in size but not in speed, so prefer a close one at the same code, and say when the references' ETAs disagree.
If the solve is not running, say so first and quote its last `===` line; the solve-stats skill reports results then.
If the script fails, lead with that and quote its message.
