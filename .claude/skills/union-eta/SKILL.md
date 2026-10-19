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
  - The input states are the input nodes' counts, known from the union's first log lines, before its inputs load:
    - for a join of joins, the two joins built before it that split its moves;
    - for a join of moves, the move nodes its edge lines name.
    Each count comes from the log's `built` or `loaded` line for that node, or for the union in progress from the node's DFA header through its link in `scratch/move_nodes`.
    References get their counts from the log, so this works after gc has removed their files.
  - Without those, it uses the conditioned input counts the merge logs, comparing only the inputs loaded so far with as many of each reference's.
  - Before any input is known, it uses the median time of the joins over as many moves at the same code.
  - It counts from the union's start, or from the previous union's end in this run before any layer has finished.
  - The references are the closest by input states at the same code, those within 25% of the size first, and then older code, since older code ran slower.
    `join_32_48` at its start shows why: the closest by size were at older code and gave 07:35 the next morning, against 23:06 from the closest at the same code.
  - The range spans the same-code references listed, or all of them if none is at the same code, and reaches down to 0.75 of the best reference's scaled time.
  - Linear scaling is rough and has run long: at its start, `join_0_32` scaled from `join_16_32` gave 676 minutes, and it took 512, because its pair counts grew less than its inputs.
    Time follows the pairs per layer, which depend on how the inputs overlap, so per layer input sizes predicted no better than the total when compared on the joins of 2026-10-19.

## 3. Present

Lead with:

- running or not;
- the union, pass and layer;
- the ETA, its range, and the reference it came from.

Then the per layer rows that matter, and what stands out, such as time ratios drifting away from 1.
A reference at older code can be close in size but not in speed, so prefer a close one at the same code, and say when the references' ETAs disagree.
If the solve is not running, say so first and quote its last `===` line; the solve-stats skill reports results then.
If the script fails, lead with that and quote its message.
