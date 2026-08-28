# TODO

## ignored problems

Places where the Rust code discards a condition instead of reporting it, found
by audit. `CLAUDE.md`'s **Priorities** section says to prefer stopping with an
error over silence, so each of these is a deviation with a reason attached
rather than an accident — but the reasons do not all survive inspection.

Ordered by how wrong the resulting behaviour is.

- `dfa-format/src/bitset.rs:21-29` — `Bitset::set` silently drops an index
  past `len`, and the comment justifies it as "a bad entry is reported by the
  bounds check rather than by panicking here". That is true on one of the two
  paths and false on the other.
  - convert is fine: `write.rs:326` runs `check_row(..)?` immediately before
    `tracker.row(..)`, so an out-of-range value errors out before it can reach
    the bitset.
  - validate is not: `read.rs:612` in `check_reachability` feeds unchecked
    entries straight into `next.set(..)`. That pass is gated on
    `opts.canonical`, while the bounds check is gated separately on
    `opts.entry_bounds`, and `dfa-validate --no-entry-bounds` makes the
    combination reachable from the command line.
  - Demonstrated by corrupting one entry of a canonical file to an
    out-of-range value:

        entry_bounds ON:   layer 0 row 2 entry 0 is 200, but layer 1 has only 4 states
                           flags bit 0 is set, but layer 1 state 2 is unreachable
        entry_bounds OFF:  flags bit 0 is set, but layer 1 state 2 is unreachable

    With the bounds check off the corruption is invisible and the surviving
    violation **blames the wrong thing**: it accuses a healthy state of being
    unreachable when the fault is an entry pointing away from it. Worse than
    silence, because it sends a reader after the wrong bug.
  - Fix: either have `set` report out-of-range to its caller, or make
    `check_reachability` bounds-check what it decodes, or run the entry bounds
    check whenever reachability runs regardless of the flag.

- `dfa-format/src/write.rs:236-237` — the directory sync that makes a
  published file survive a crash is entirely best effort:
  `if let Ok(dir) = File::open(out_dir) { let _ = dir.sync_all(); }`. Both the
  open and the sync can fail without a word, and `convert` still reports
  success, so the content addressed store can lose an entry it claims to hold.
  A durability guarantee that reports success when it did not happen is not a
  guarantee.

- `dfa-format/src/legacy.rs:43` —
  `fs::canonicalize(dir).unwrap_or_else(|_| dir.to_path_buf())` degrades to the
  unresolved path when canonicalization fails, e.g. on a permissions error or
  a dangling symlink. `resolved_dir` feeds name checking, so the case this hides
  is exactly the one worth reporting: a scratch symlink pointing at nothing gets
  a vaguer diagnosis than it deserves.

- `dfa-format/src/sample.rs:121` — `let (c, next) = chosen?` returns `None`
  when a state has a positive suffix count but no successor with any weight.
  That is impossible unless the counts are inconsistent or floating point has
  drifted, i.e. exactly when something is wrong. `union.rs:561` then reads the
  `None` as "empty language, nothing to draw" and breaks, so the pre-filter
  quietly draws fewer strings than asked and says nothing.

- `dfa-format/src/iter.rs`, `Positions::next` — a failure from `advance()` is
  parked in `self.error` and returned on the *following* call, so a caller that
  abandons the iterator first never sees it: `.take(n)`, `.find(..)`,
  `.count()`. `verify.rs::for_each_position` drains to exhaustion and is safe
  today, but nothing in the type says a partial consumer is unsound, and the
  condition it hides is a non-trim automaton.

### looked at and left alone

Not error suppression, recorded so the audit does not get repeated:
`lib.rs:57` (`write!` into a `String` cannot fail), `write.rs:155`
(best effort temp file cleanup on a path that is already returning an error),
`registry.rs:25` (a parse failure becomes a message naming the expected form),
the `wrapping_*` arithmetic in `sample.rs` (deliberate, it is a PRNG), and the
`unwrap_or_else` display fallbacks in `read.rs:116`, `dfa-stats` and
`dfa-convert`.

## uniform ordinary rows are always wrong

Only state 0 may reject everything, and only state 1 may accept everything.
An ordinary row (`row >= 2`) that is all `0` or all `1` duplicates a reserved
state and must be a violation **whether or not `flags` bit 0 is set**.

Both tests already exist and are gated too narrowly. `read.rs`, inside
`scan_blocks`, has them under `if check_canonical && row >= 2`, where
`check_canonical` is `opts.canonical && header.canonical()`; `write.rs:105-119`
has the same pair in `CanonicalTracker::row`, used only to decide whether to
set the bit. So today a file with the flag clear can carry an ordinary
reject-everything row and validate cleanly.

The work is to lift the two tests out of the canonical gate and reword them:
they currently read "flags bit 0 is set, but layer N row R rejects everything,
duplicating state 0", and the new message must not mention the flag, because
it no longer has anything to do with it. Keep them reported once per file, as
the `reported_uniform` latch already does.

Note this makes files stricter than `FORMAT-DFA.md` currently requires —
section 8 ties uniform-row freedom to bit 0, and sections 4 through 7 say
nothing about it. Either the spec gains the rule in section 4, next to the
reserved meanings of 0 and 1, or the check has to be an optional one that
`dfa-validate` runs by default. The first is the honest version: a file with
two states meaning "reject everything" is malformed regardless of what it
claims about numbering. Existing scratch files should be swept before the
check is turned on, since anything it rejects has been accepted until now.

Related to **validate trimness** below: an all-`0` ordinary row is the
immediately dead case, so this catches one layer of what a full backward pass
catches everywhere.

## validate trimness

`validate` has no backward pass. `read.rs:586` `check_reachability` walks
forward and reports ordinary states that cannot be *entered*; nothing reports
states that cannot *leave* — a reachable state with no accepting continuation
at all. Add that check.

The gap is visible in the spec. `FORMAT-DFA.md` section 8 says bit 0 asserts
the automaton is "minimal and free of unreachable and dead states", and
`write.rs:35-47` repeats the claim, but of the two only unreachable is
verified. `dfa-convert` catches a row that is entirely `0` as "duplicating
state 0", which is the immediately dead case; a state whose successors are all
dead two layers down is not caught by anything.

The check is one backward pass with the shape of `stats::count_accepted`,
carrying one bit per state instead of an `f64`: a state is live if any of its
transitions reaches a live state in the next layer, with `1` live and `0` dead
in the terminal pseudo-layer. Flag any reachable state that comes out dead.
Cost is one pass over the transition tables, so it belongs with the other
optional checks rather than the required ones.

This is the property behind two of the entries above:

- `iter.rs` (#5) already treats a dead end as an error at enumeration time —
  "automaton is not trim". Validating it turns a failure that surfaces halfway
  through a long enumeration into one a reader gets up front, from the file
  alone.
- `sample.rs` (#4) is the same property seen from the counting side: a dead
  state is exactly one whose suffix count is zero, which is the case
  `chosen?` bails on.

Worth being straight about the second one. Given a structurally valid file the
sampler cannot actually reach that branch — it only ever steps to a successor
whose count is positive, and the initial state is checked before the walk
starts, so the invariant holds by induction. The guard is defensive against
counts that disagree with the transitions they came from. So the value of the
new check is mostly the first bullet plus closing the spec gap; it does not
make `chosen?` reachable or unreachable either way. Fixing #4 is still worth
doing on its own terms, because the `None` is currently read downstream as
"empty language" and says nothing.
