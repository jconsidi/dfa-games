# TODO

## local dfas_by_hash read-through cache

Deferred until there is a real networked archive filesystem to test against
-- everything below is reasoning, not yet a design to build from.

The motivating case is narrower than "local and archive are different
roots." `save_durable` already does `save_by_hash(local)` then
`save_by_hash(archive)`, and the latter goes through `publish_copy`, which
tries a hardlink before a copy. So whenever local and archive share a
filesystem, every durably saved DFA already ends up in `local_dir/dfas_by_hash`
for free, as a hardlink, at save time. A separate cache adds nothing there.

What is not covered: `DFAUtil::load_by_hash(shape, hash, durable=true)` on a
DFA this local scratch never itself published -- built by a different
process, a different machine, or an earlier scratch lifetime since cleared
or `gc.pl`'d. That read goes straight to `archive_dir` every time, with
nothing local to check first, and `load_by_hash`'s weak_ptr cache only
dedupes within one process's lifetime, not across restarts. This is the
restart-on-a-shared-archive case, and its payoff is exactly proportional to
how much slower archive actually is than local: real and recurring when
archive is genuinely networked, nothing to save when it is not.

Two configurations to keep straight when this gets designed:

- **Local and archive share a filesystem**: no benefit, per above. Caching
  here would be pure overhead.
- **Local and archive are distinct filesystems, both local** (e.g. two
  separate local disks, no network involved): genuinely ambiguous. `publish_copy`
  already pays a real copy cost here via its `EXDEV` fallback, but whether a
  *read-through* cache is worth adding depends on the relative speed of the
  two devices, which nothing in this project currently measures or knows.
  Default off (opt-in) is the safer call than always-on, since this
  project's other size/threshold decisions (the 1MB anonymous-mmap cutoff,
  hardlink-before-copy) all check that something would actually help before
  doing it, rather than doing it unconditionally on the chance it might.

A design wrinkle to resolve before implementing, not after: `local_dir/dfas_by_hash`
today already serves two roles gc.pl has to keep straight -- durable-save
staging (safe to lose once archive has its own copy) and the sole copy of
`save_cache`-only results (not safe to lose, no reference-counting fallback
exists for it). A read-through mirror would be a third role, and it behaves
differently from the other two: by definition it is always recoverable from
archive, so it should be swept by age like the op-caches
(`union_cache`, `intersection_cache`, ...), not reference-counted the way
archive's own `dfas_by_hash` must be. Mixing a third eviction policy into
one directory needs a real design, not just a flag.

## BinaryRestartDFA is broken

`binarydfa/` used to be one directory shared by every `BinaryDFA`, so
`BinaryRestartDFA` could resume a previous process's progress by finding its
fixed-name layer files still there. It is now scoped per instance --
pid-and-counter -- so that two concurrent `BinaryDFA` constructions (even in
the same process) cannot race through the same filenames and corrupt each
other. (`DFA`'s own `build/` staging used to be pid-and-counter for the same
reason; it is now bare pid, since `build_in_progress` makes "at most one DFA
build in flight per process" an asserted invariant there. `binarydfa/` keeps
its counter for now -- the same argument likely applies, since a
`BinaryDFA`'s `binarydfa/` guard is always torn down before its own
`build_in_progress` window closes, but that hasn't been checked through and
changed the way `build/` was.) `BinaryRestartDFA`'s constructor now
creates its own, empty `binarydfa/<pid>-<n>` directory before its restart
scan, so it always finds zero pairs and throws "previous pairs not found"
immediately.

This is a deliberate, known regression, not an oversight: restarting from a
previous process's on-disk progress needs a different mechanism now (passing
the specific directory to resume from explicitly, rather than discovering it
by a shared fixed name) and that redesign is out of scope for pid-scoping
`binarydfa/` itself.

**Fixing this** needs at least:

- A way for a new process to *adopt* an existing `binarydfa/<old-pid>-<n>`
  directory instead of `create_binary_directory` always making a fresh one --
  the new process has a different pid and a process-local counter, so it has
  no way to reconstruct the old path itself; it has to be told.
- Something to tell it: the exact stale directory has to be named explicitly
  (a command line argument, a config value, whatever the restart entry point
  ends up being), which means whatever launches the restart needs to have
  recorded it, since the crashed process cannot clean up after itself to
  leave a note.
- A race against `gc.pl`'s own pid-liveness sweep, which is what this
  project now relies on to reclaim a crashed build's staging directory. Once
  a crash leaves a dead pid behind, that directory is fair game for the next
  `gc.pl` run. A restart mechanism has to grab (or otherwise protect) the
  directory before gc gets to it, or the resumable state is just gone -- gc
  cannot be expected to know a directory is "still wanted" by a restart that
  has not happened yet.

## tests

- solution tests
  - tictactoe

- a C++ runner over `config/<game>/positions-manual.json`, matching the Rust
  `tests/positions_manual.rs`. The file holds hand written positions with
  `expected_moves` and `expected_result` in the same envelope `tests.json`
  uses, so `run_test_cases` can read it with a new case type and check
  `validate_moves` / `validate_outcome` the way `test_perft_u` checks position
  counts. Today only the Rust rules are held to those positions, which is half
  the point of writing them down: the same file should pin both
  implementations. Merging it into `tests.json` outright would work too, and is
  where this is headed.
  - note the C++ can only run the cases for games it has `validate_moves` for
    — amazons, breakthrough and chess. `Game::validate_moves` (`Game.cpp:416`)
    throws for clobber, normalnim, othello and tictactoe, so those cases have
    to be skipped there rather than failing, which is the opposite of the Rust
    runner's rule and worth a comment where it happens.

## memory maps

- revisit the munmap calls added for mapping pressure, now that a saved DFA
  holds one mapping instead of one per layer
  - `DFAUtil.cpp:151` and `:200-202` unmap DFAs around the union/intersection
    reduction. With ~5000 cached DFAs that was ~5000 * ndim mappings before
    the current format and is ~5000 now.
  - `DNFBuilder.cpp:97` unmaps each clause as it is added.
  - the rest are functional and stay: `DFA.cpp:436` and `BinaryDFA.cpp:877`
    unmap so the file can be reopened or truncated, plus MemoryMap's own uses
    in truncate/rename/unlink/move/destructor.
  - the comment at `DFAUtil.cpp:151` says "reduce open files", but munmap does
    not close descriptors: MemoryMap's constructor closes fildes right after
    mapping. It reduces mappings.
- if those go, check whether anything still re-maps after unmapping. Every
  remaining munmap is followed by truncate/rename/unlink rather than a
  re-map, so `MemoryMap::mmap()` may no longer need to be lazy. Dropping the
  laziness would also remove the mutable state that makes a const MemoryMap&
  unsafe to share between threads.
- `DFA::get_transitions` calls `file_map->mmap()` on every transition lookup.
  Caching the base pointer and layout in `DFA::mmap()` would take it off the
  hottest path in the enumerator.

## parallel enumeration

Attempted and reverted (was commit 3ed1c0f, removed by rebase; the object may
still be reachable in the reflog for a while). Worth retrying, but it needs a
bug found first.

### the approach

Enumeration is serial and dominates `for_each_position`, so it only reaches
about 1.4 cores. Cut the string space into ranges instead:

- `DFA::get_prefixes(target)` deepens one layer at a time from the initial
  state, keeping every reachable non-reject state, until there are
  `min(1000, size/100000)` prefixes. Prefixes come out in lexicographic order
  because it walks the current list in order and characters ascending.
- `DFA::cbegin_at_prefix(prefix)` replays the prefix to get its end state, then
  fills forward with the first accepted character at each remaining layer, the
  same scan `cbegin()` does.
- Consecutive prefixes bound ranges that tile the space exactly. Boundary
  iterators are built serially, then `TRY_PARALLEL_PAR_3(std::for_each, ...)`
  over range indices, each worker walking its own iterator and calling func
  directly.
- Batching disappears: no batch buffers, no per batch dispatch. A target of one
  range is the serial walk, so small DFAs need no separate path.
- Failure reporting keys on (range, offset) and takes the minimum, so the first
  failing position in DFA order is still what gets reported. Ranges above the
  earliest failure stop as soon as they see an atomic flag, which keeps the
  all-positions-fail case fast.

### measured

breakthrough_4x4 lost,side_to_move=0, 34573426 positions, 8 cores:
real 6.05s -> 1.83s (3.3x), cores 1.4 -> 6.6. On breakthrough_5x5 utilization
was consistently above 7.5 cores.

### the bug

`verify_lost_sound breakthrough_5x5 backward,ply_max=000,side=0,losing` aborts
with heap corruption after about 40 minutes, roughly 2e10 positions. The
reported guard value was 0x300000003, two adjacent int 3s, and 3 is both the
layer shape and the sentinel character value written when a scan finds no
accepted character.

Ruled out:

- prefix and boundary construction, and their ordering, checked serially on the
  same DFA
- concurrent enumeration on its own: 59.8e9 positions with an empty callback,
  three times past the crash depth, clean
- `dfa_format::Layout` is immutable and `row_offset` is pure
- the read path never writes to MemoryMap once `DFA::mmap()` has run before the
  threads start
- a mis-indexed DFAIterator vector: the existing asserts check
  `states.size() == ndim + 1`, `states[ndim] == 1` and
  `characters.size() == ndim` on every step and never fired

Still open: the interaction of concurrent iterator mutation with heavy
allocator traffic from the callback, and the exception path, where many threads
can build report strings and throw at once while exception_ptrs are captured
and discarded across threads. The batched version survived past the same depth,
which is what points at the range change rather than something older.

Also noticed while reading the loop: `++iter` and the `iter < end` test sit
outside the try block, so an allocation failure inside `operator++` escapes the
execution policy algorithm and calls terminate. Same in the batched version.
The whole loop body belongs inside the try.

### if retrying

- gate on a DFA large enough to reach 1e10 positions before claiming it works.
  The original gate ran six cases but all on breakthrough_4x4, which is 3.5e7
  positions and never reaches the regime where this fails.
- per position cost varies by about two orders of magnitude between terminal
  DFAs, where validate_moves early returns, and non terminal ones, where it
  builds a DFAString per legal move. Pick test DFAs by expected runtime, not by
  position count.

## move generation

### hybrid dispatch for small inputs

`MoveGraph::get_moves(const DFAString&)` (`MoveGraph.cpp:155`) generates moves
by propagating explicit position lists through the graph, with no set DFAs at
all.
It is much faster than the DFA path for small inputs, which pays for roughly
`4 + H + 3*W*H` node DFAs regardless of how few positions came in.
Dispatch between the two inside
`MoveGraph::get_moves(name_prefix, shared_dfa_ptr)` (`:219`) so both
`get_moves_forward` and `get_moves_backward` benefit and `Game` stays unaware.

- the two paths agree by construction, and this is the load bearing fact.
  `add_node` appends `get_fixed(layer, before_character)` to the node's
  pre conditions and `get_fixed(layer, after_character)` to its post
  conditions for every changed layer (`:124-135`), and `add_edge` folds
  `node_post_conditions[from]`, the explicit conditions, and
  `node_pre_conditions[to]` into every edge (`:60-72`).
  So the per position `contains` checks enforce exactly what the DFA path's
  `get_intersection_vector` plus `get_change` enforce, and the
  `std::logic_error` at `:200` is an unreachable invariant rather than a
  divergence.
  A node change added without the matching pre condition would make the two
  paths disagree silently, so say this at the dispatch point.
- add a third entry point taking `const std::vector<DFAString>&`. The
  implementation is already vector shaped internally, so this is
  `node_input_positions[0] = positions_in;` and both the single position
  version and the fast path call it.
- threshold on positions per state, not on positions. The DFA path is
  empirically linear in the initial state count rather than quadratic, so
  the fast path wins while `size() / states()` is below some constant, and
  that constant should travel across games because both costs scale with the
  move graph. `size()` is cached in a sidecar keyed by digest and `states()`
  is cheap, so the test itself costs nothing.
- watch peak memory once inputs can be large. `node_input_positions` and
  `node_output_positions` hold every input position at every node at once,
  `O(N * nodes * ndim)`, and a positions per state threshold admits a large
  absolute `N` when the DFA has many states. The DFA path already computes
  `last_to_node_index` per node for its cleanup schedule (`:305-315`); the
  same schedule frees a node's output list once its last consumer has run.
- the fast path writes none of the `move_nodes/` cache entries (`:230`, the
  only reference in the tree), which is less scratch churn rather than a
  behaviour change.
- extend `test_move_graph.cpp` to compare the two paths on every configured
  position, on top of the existing comparison against `validate_moves`. Add a
  case with more than one input position, since every configured position is a
  singleton today and would only ever exercise `N == 1`.

### canonical numbering for StringDFA

`StringDFA` claims canonical numbering only for a single input string
(`StringDFA.cpp:12`).
It is minimal, has no unreachable or dead states, and cannot produce a uniform
reject or uniform accept row, since `DFA::add_state` returns the reserved state
directly when every transition matches it (`DFA.cpp:340-365`).
The one thing still missing is the ordering in section 8 of `FORMAT-DFA.md`:
within a layer, ordinary states must be sorted by transition row ascending.
`DedupedDFA::add_state` numbers states in call order, and `build_internal`
recurses depth first, so states come out in subtree completion order instead.

- the recursion does not need to change if a post pass renumbers each layer.
  A layer's rows are indices into the next layer, so the next layer must be
  final first: sweep from the highest layer down to layer 0, with the last
  layer as the base case since its rows are already over `{0, 1}`.
  Single pass canonical numbering would need a breadth first rewrite instead,
  which is a different algorithm.
- `DedupedDFA::set_initial_state` deletes `state_lookup` to block further
  state creation, so the pass has to run before that call or work on `DFA`'s
  staging directly. Confirm the staging is still writable there.
- `BinaryDFA` gets this for free only because below `binary_dfa_hash_width`
  its sort key is the transitions themselves (`BinaryDFA.cpp:513-520`), and it
  drops the claim via `hashed_any_layer` when it has to sort by hash. There is
  no reusable helper to borrow.
- the payoff is that digest equality becomes language equality (section 8), so
  the hybrid's two paths produce byte identical files for the same move set.
  That turns the cross check above into an assert on hashes instead of two
  `get_difference` calls, and keeps the content addressed caches from holding
  two files per language. Determinism already bounds that fragmentation, so
  this is a simplification rather than a correctness fix.
