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
