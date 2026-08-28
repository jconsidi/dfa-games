# TODO

## tests

- solution tests
  - tictactoe

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
