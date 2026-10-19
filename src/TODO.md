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
fixed-name layer files still there. It is now scoped per instance -- bare
pid, same as `DFA`'s own `build/` staging -- so that two concurrent
`BinaryDFA` constructions (even in the same process) cannot race through the
same filenames and corrupt each other; `binary_build_in_progress`
(`BinaryDFA.cpp`) makes that an asserted invariant, mirroring `build/`'s own
`build_in_progress`. `BinaryRestartDFA`'s constructor now creates its own,
empty `binarydfa/<pid>` directory before its restart scan, so it always
finds zero pairs and throws "previous pairs not found" immediately.

This is a deliberate, known regression, not an oversight: restarting from a
previous process's on-disk progress needs a different mechanism now (passing
the specific directory to resume from explicitly, rather than discovering it
by a shared fixed name) and that redesign is out of scope for pid-scoping
`binarydfa/` itself.

**Fixing this** needs at least:

- A way for a new process to *adopt* an existing `binarydfa/<old-pid>`
  directory instead of `create_binary_directory` always making a fresh one --
  the new process has a different pid, so it has no way to reconstruct the
  old path itself; it has to be told.
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

## live garbage collection

`gc.pl` is only safe while no solver is running.
A live solver holds DFAs that only `*_cache` links name, which gc deletes, and it unmaps DFAs and reopens them by path.
So every DFA a solve stops needing keeps its disk until the solve is stopped by hand for gc.

The biggest case is `MoveGraph`'s join outputs.
Each join's inputs are released (their `move_nodes` links unlinked) once the join is built, but their files stay until the next gc.
In cram_7x7, building `begin.join_0_16` releases `begin.join_0_8` and `begin.join_8_16`, 442 GB and 560 GB.

The idea is to flag a DFA as ephemeral when it is built, and restrict how it may be saved, so its builder can delete it safely itself:

- An ephemeral DFA's file is named only by links its builder controls, so releasing those links can delete the file at once, with no gc pass needed.
- Any save that would give it another name, such as `save_durable`, a `union_cache` entry or a link from another game, throws, unless it is first explicitly promoted to an ordinary DFA.
- Releasing one checks that it has no other names before deleting it, and throws if it does.

What the design has to settle:

- **Content addressing.** `dfas_by_hash/<sha256>.dfa` is shared by every name with the same contents. An ephemeral DFA whose hash matches an existing ordinary DFA, or one saved later, must not delete the shared file. It needs either its own file outside `dfas_by_hash` or a check at release time that nothing else links the hash.
- **Restart.** Restarts resume from saved join outputs today: the restart at de312f2 reloaded `begin.join_0_8` and `begin.join_8_16` from their `move_nodes` links. An ephemeral output still has to survive the process until its parent is built, so a crash before then must leave it loadable, and gc must still clean it up once released.
- **Caches.** `get_union` saves every result under `union_cache/`, and gc deletes those entries only up to the newest `move_nodes` link. An ephemeral output must not get that entry, or must have it removed when it is released.
- **Who is ephemeral.** Join outputs are the clear candidates. Anything a later ply or a verifier reads must stay ordinary.

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

- recorded hashes for small solves, so a change can be checked for byte identity without building and rerunning a baseline from git each time.
  Today that check means `git archive` of HEAD, a second build, and both binaries run on `solve_backward breakthrough_4x4` and `build_forward breakthrough_4x4 4` in fresh scratch roots, comparing every result symlink's hash.
  - A config file listing the solves to run (command, arguments, expected run time) and, for each, every result name it publishes with its expected hash.
    Start with breakthrough_4x4 backward and forward, and add a few cheap cases from other games, since `config/` tests only cover tictactoe.
  - A runner that executes each solve in a fresh scratch root (`DFA_LOCAL_DIR` and `DFA_ARCHIVE_DIR` both set), then compares the published symlinks with the recorded hashes.
    Every mismatch, missing name and extra name fails loudly, and the runner prints how many names it checked per solve.
  - A way to rerecord the hashes, used only when a change is meant to renumber states, with the old and new hashes both shown in the commit so the change is deliberate and reviewable.
  - Hashes pin the bytes, not just the language, so a change that renumbers states correctly still fails; the language check for that case stays with the `verify_*` programs.

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
  iterators are built serially, then `TRY_PARALLEL_3(std::for_each, ...)`
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

## BinaryDFA packed direct keys

When layer i + 1 has more than 2^32 states, `build_quadratic_backward_layer` sorts layer i by SHA-256 of each row (`BinaryDFA.cpp:742-789`), even when the rows would fit the key directly at fewer bits per transition.
That path creates and destroys an EVP context per row, and it clears the canonical claim via `hashed_any_layer`.

Measured on cram_7x7 ply 10, union 7 of `begin.join_0_8`, on 2026-10-14: `transitions hash` took 64s on layer 41 (direct keys) and 1837s on layer 40 (hashed, next layer 4.48e9 states).

The key is 96 bits with narrow pair ranks and 88 bits with wide ones, since the rest of the 16 byte record holds the rank.
Two 64 bit transitions do not fit, but at ceil(log2(next layer size)) bits each they often do: layer 40's are 33 bits, 66 of the 88.

- Use direct keys whenever shape * bits fits the key, packing transition 0 into the most significant bits of word 0 and so on, zero filling the rest.
  Comparing keys word by word then orders rows by their transitions, so the layer stays canonical.
- The constant rows' reserved keys are the packed form of an all-c row, so they still cannot collide with a non-constant row.
- For cram's 2 character layers, this covers next layers up to 2^44 states.
- Narrow layers where today's 32 bit direct keys apply must keep exactly today's keys, so existing DFAs stay byte identical.
- Test with `binary_dfa_key_narrow_max` lowered: same language and state count as the default, output canonical and reloading with a valid digest.
  Also test a shape that fits packed but not at 32 bits, and one that needs the hash.

### random reads of `transitions` after the sort

Two steps after the sort read rows of `transitions` at random ranks, since the records are in key order and the ranks within them are scattered.
This is cheap while `transitions` fits in RAM and very slow once it does not.

- `sort hash check` (`BinaryDFA.cpp:810-829`) runs on every layer and `memcmp`s the rows of each adjacent pair of equal keys.
  On cram_7x7 layer 40 (14.7e9 pairs, `transitions` 235 GB at 8 bytes per id, about twice RAM) it ran for hours at about 107,000 16 KB page-ins per second, each fetching one 16 byte row; layer 41 (117 GB at 4 bytes per id, mostly resident) took 195s.
- `states write` (`BinaryDFA.cpp:916-932`) does one random row read per new state.

With direct keys, packed or not:

- Skip `sort hash check`. The key is the row, so equal keys already mean equal rows; the check repeats the sort's own comparison.
  For packed keys, verify instead that each key unpacks to its row while the keys are built, which reads `transitions` in order.
- Decode each new state's transitions from its sorted key in `states write`, in order, since direct keys number states in sorted key order.
- Then nothing reads `transitions` after the hash step, so unlink it there, freeing its disk (235 GB on cram layer 40) before the sort.

With hashed keys, none of this applies, and `sort hash check` is the only guard against a SHA-256 collision, so it has to stay.
It may need redesign to avoid the random reads: comparing in rank order does not help, since a group's members and its first row are scattered across ranks just as much.
`states write` on the hashed path has the same problem.

### flashsort on row and rank records

A possible replacement for the hash, the sort, the check and the random reads of `states write` together, on every path.

- At `transitions populate`, write records of each pair's row and its rank, at position rank, in place of `transitions`.
- Sort the records by row bytes with an in place, depth first, most significant byte first flashsort (American flag sort): 256 buckets per level, each filled from one advancing pointer.
  Digit k is byte `width - 1 - (k % width)` of transition `k / width`, so byte order is the numeric order the direct keys use today, and narrow layers keep today's numbering.
- Records carry their rows, so every level reads its digits from the bucket's own contiguous region.
  Sorting ranks alone and reading digits through them from `transitions` goes sparse after a level or two, the same random reads as today's check.
- In the histogram pass, also find the bucket's common prefix, so a skewed bucket (rows whose transition 0 is reject, or the zero top bytes of wide ids) skips all its shared digits in one pass.
- Equal neighbours are then equal rows exactly. A sequential is sorted pass is the check, and `states write` reads the rows in order. Every layer is canonical.
- Cost: records are row plus rank, 24 bytes on cram layer 40 against today's 16, and each level above RAM moves them once.
  Wider alphabets cost more, since a 13 character row at 4 bytes per id is 52 bytes. Measure before deciding whether hashing stays for rows over some size.
- Layers hashed today (more characters than `direct_max`, e.g. amazons, chess) would get new numbering and digests.

Test it against `std::sort` by row on: shapes 1 to 13, ids at 4 and 8 bytes, all equal, all distinct, few distinct, a heavily skewed first digit, and sizes around the in memory cutoff.
Then benchmark against the current hash, sort and check on in memory inputs, and out of core on a layer about twice RAM, watching page-ins.

## consolidate the sort implementations

There are three sort modules, plus scattered `std::sort` calls:

- `Flashsort.h` / `Flashsort.cpp`: `flashsort_partition` and `flashsort_permutation`, keyed by a `std::function`.
  Nothing calls them; `BinaryDFA.cpp` still includes the header.
- `sort_utils.h`: `parallel_sort`, which picks `std::sort(par)` or `parallel_sort_in_place` by memory, tested by `test_sort_utils`.
  Its only BinaryDFA caller left is the hashed backward path's key sort (`build_quadratic_backward_layer_hashed`).
- `flash_sort.h`: `FlashSort` over any record with a fixed length key read through `sort_key.h`, with RegionSort for large ranges, tested by `test_flash_sort`.
  It sorts the backward rows path, the rank output records, and the forward pass's transition pairs.

Pick one home for the sorts and one test program.
- Remove `Flashsort.*` unless something needs it, or fold what is worth keeping into `flash_sort.h`.
- Measure the forward pair sort out of core on cram against the `parallel_sort` times in the 2026-10-15 trace (7,055s on `begin.join_0_8`).
  In memory, on 1e9 pairs, FlashSort took 3.2-3.7s against 5.9-6.7s for `parallel_sort_in_place`.
- Then nothing but the hashed path calls `parallel_sort`, so either of these would retire it.
  Test both and compare.
- Keep the post sort check (`FlashSort` throws if the result is not sorted) on every sort that replaces another.

### hashed path: FlashSort its keys

The hashed path still sorts its `BinaryDFATransitionsHashPlusIndex` records with `parallel_sort`.
It is taken when a layer's shape is over `binary_dfa_row_shape_max` (4), when a narrow layer's rows do not fit a direct key, and when `binary_dfa_rows_enabled` is cleared.
Cram never takes it; larger alphabet games such as chess and amazons can.

- Give the hash records the `sort_key.h` interface over their key words only, as `operator<` compares them: words 0 to 2, or in wide mode words 0 and 1 and the high 24 bits of word 2.
  Then sort them with `FlashSort`.
  Hashed keys are uniform, so every level splits 256 ways and nothing is skipped as shared.
- The output must be byte identical.
  States are numbered in key order, and records with equal keys hold equal rows, so the order among ties does not change the numbering.
  Check that with the hashed path forced (`binary_dfa_rows_enabled` cleared, as `test_rows_path` does), and on an amazons or chess case whose layers go hashed.
  breakthrough_4x4 does not exercise it, since its shape 3 layers take the rows path.
- Time it against `parallel_sort` on a hashed layer large enough to matter.

### hashed path: retire it in favor of the rows path

The rows path already covers layers of shape up to 4 whose rows fit a direct key, and those whose next layer is over 2^32 states.
Its numbering is canonical where the hashed path's is not.
What it leaves to the hashed path is narrow layers whose rows do not fit a direct key (shape 4, or shape 3 with wide pair ranks), and every layer over shape 4.
Extending it to the remaining cases would leave one backward path.

- Rows over shape 4 need more `RowRankRecord` shapes, each its own instantiation, so pick the shapes the games use rather than every one up to the largest.
- A record carries its whole row: at shape 13 and 4 byte ids that is 56 bytes, against 16 for a hash record.
  Measure the disk and the sort time against hashing at the largest hashed layers.
- Layers the hashed path numbers by hash would be numbered by row instead, so their DFAs change hash and every cache built from them is rebuilt once.
  The language must not change: check each affected result against the hashed build by state count, and by `get_difference` both ways being empty.
- Then remove the hashed path, `binary_dfa_rows_enabled`, `binary_dfa_key_narrow_max`, and `hashed_any_layer`, along with the tests that force them.

## BinaryDFA backward pass lookups out of core

Measured on cram_7x7 `begin.join_16_24` (8 moves, 192.7 minutes at d24c8cb) on 2026-10-18, from the profile trace and log.
Faults are each phase's own major faults at 16 KiB each, a lower bound on what it read, since the trace's `inblock` is always 0.
The backward pass took 118.1 minutes, against 37.0 for the forward pass.

| Division | Min | User cores | Sys cores | GB faulted |
|---|---|---|---|---|
| `transitions populate`: next pair lookup | 56.2 | 6.7 | 2.0 | 4,115 |
| `transitions input`: rebuild, right lookup | 14.7 | 5.9 | 2.0 | 1,316 |
| `transitions input`: rebuild, left lookup | 7.1 | 2.5 | 3.0 | 819 |
| `next pair outputs` merge | 8.4 | 0.6 | 3.0 | 1,129 |
| `next pair index` | 7.7 | 0.1 | 4.9 | 1,114 |
| rows sorts and numbering | 23.7 | 5.4 | 1.4 | 504 |

### next pair index inside the merge

`next pair index` (`BinaryDFA.cpp:584`) reads every 512th entry of `next_pair_outputs` right after the merge writes it, about one page fault per entry.
It ran at 0.08 user cores for 7.7 minutes.

- Write the first level of the index from the merge's own populate, which writes every pair, so it can write every 512th to the index as it goes.
- Keep the upper levels as they are; they are under 1 MB after the first.

### sequential reads through `pread`

Inputs read in order fault once per 16 KiB page through mmap.
The left lookup's faults (166 GB on layer 41) match reading `curr_pairs` (146 GB) and the left input in order, at 2.7 user and 2.8 sys cores.

- Read the in-order inputs with large `pread` calls instead: `curr_pairs` in the left lookup, `transition_pairs_left` in the right lookup, `curr_transition_pairs` in `transition_for`, and both inputs of the `next pair outputs` merge.
- These are forward and backward alike, since the rebuild runs the forward producer.

### sort join for the next pair lookup

`transition_for` (`BinaryDFA.cpp:644`) binary searches `next_pair_outputs` for each transition, in transition order rather than next pair order.

| Layer | Min | GB faulted | In-order input GB | Table GB |
|---|---|---|---|---|
| 42 | 4.0 | 223 | 195 | 27 |
| 41 | 10.3 | 692 | 291 | 146 |
| 40 | 17.5 | 1,432 | 271 | 291 |
| 39 | 11.4 | 906 | 208 | 271 |
| 38 | 6.0 | 437 | 143 | 208 |

While the table fits RAM (layer 42) the faults are only the in-order read.
Above RAM they reach 2 to 5 times the input and table together, about 2.7 TB re-read on layers 38 to 41, which take 45 of the 56 minutes.

Keeping each transition's next pair rank from the forward pass would remove the lookup and the rebuild, but every layer's list would be on disk at once at the turn from forward to backward: about 2.7 TB at 16 bytes per transition for `join_16_24`, and about twice that for `join_16_32`.
Instead, within each backward layer whose table is over a RAM cutoff:

- Rebuild the transition pairs as now, emitting (next pair, transition index) records straight into the bucket partition.
- Bucket sort them by next pair, and merge each bucket against its contiguous range of `next_pair_outputs` in order.
- Emit (transition index, output) records and bucket sort them by index; consecutive indices are each pair's row, which feeds the rows path.
- Check that every record finds its pair, as `assert(found.pair == next_pair)` does now, and that every index comes back once, as the rank outputs check does.
- Disk is held for one layer only: about 580 GB of records and bucket files at layer 41.
- Estimate, not measured: the two sorts move about twice the bytes of the forward pair sort (3.5 minutes on layer 41), so 7 to 12 minutes against 10 to 17.5 for today's lookup on layers 39 to 41, or 20 to 30 minutes per 8 move join.
  The gain grows with join size, since the random reads grow faster than the sorts once the table is several times RAM.
- Prototype it on one layer in the side tree and time it before building it into the solver.

### keep bucket files out of the page cache

Measured on cram_7x7 `begin.join_24_32` on 2026-10-18: on layer 40 the right lookup faulted 251 GB, against 115 GB of `transition_pairs_left` read in order and a 21 GB right input layer.
The right input fits RAM easily, so it is being evicted and read back about 6 times, probably by the bucket file writes and reads going through the page cache.

- Set `F_NOCACHE` with `fcntl` on bucket files on macOS, and call `posix_fadvise(POSIX_FADV_DONTNEED)` on written ranges on Linux, guarded by platform.
- Check the `fcntl` and `posix_fadvise` return values and throw on failure.
- Measure the right lookup's faults and time on a large layer with and without it, and keep it only if it helps.
- If the right input's faults stay high, look for the other streams evicting it, such as `transition_pairs_left` and the MemoryMap populate writes.
