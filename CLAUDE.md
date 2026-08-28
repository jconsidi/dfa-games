# dfa-games

Solves combinatorial games by representing sets of positions as layered DFAs.
A position is a string of one character per board square; a set of positions is
the language of a DFA over that alphabet. Set operations (union, intersection,
difference) and move generation are DFA operations, which is what makes solving
whole games tractable.

Everything builds and runs from `src/`.

## Building

    cd src && make -j8

**Always pass `-j`.** A header change invalidates nearly every translation unit
(`DFA.h` is reached through `Game.h` by almost everything), so a full rebuild is
~86 objects plus ~40 links. Serially that is 10+ minutes; with `-j8` it is a
couple of minutes.

Flags to know, from `Makefile:5`:

- `-Werror -Wall -Wextra -Wconversion -Warith-conversion --pedantic` — warnings
  are hard failures, so a clean build is a real signal
- `-O2`
- **no `-DNDEBUG`**, so every `assert` is live and runs in production paths.
  This is deliberate and load-bearing: the DFA invariant asserts in
  `DFAIterator::operator++` and `DFAString`'s constructor are what catch
  corruption in derived data that nothing else checks. Do not add `-DNDEBUG`
  casually.

`make depend` regenerates `.depend`. Run it after adding or removing `#include`s,
otherwise incremental builds can miss a rebuild.

## Testing

    make test

Runs `test_bitset`, `test_sort_unique`, `test_dfa`, `test_change_dfa`,
`test_perft_u`. `test_chess_game` is currently commented out in the recipe
because it is slow. `make test_chess` runs the chess tests.

Coverage is uneven and worth knowing before trusting a green run: `config/` only
holds `tictactoe_2/3/4`, so the config-driven tests exercise tictactoe only.
`AmazonsGame`, `OthelloGame` and `ClobberGame` have no test data at all — changes
there compile-check but are otherwise unverified by `make test`.

## Test data

`src/scratch` is a symlink to a large scratch volume. DFAs are content addressed
in `scratch/dfas_by_hash/<sha256>.dfa`, with `scratch/<game>/<name>` symlinks
per game. Caches (`intersection_cache`, `union_cache`, `binarydfa`, …) live
alongside.

This directory is a cache and gets cleared. When symlinks dangle, regenerate:

    ./solve_backward breakthrough_4x4      # backward,ply_max=*, lost, won
    ./build_forward breakthrough_4x4 4     # forward,ply=*

Regenerating breakthrough_4x4 takes a few minutes. Note that `get_dfa` throws a
`runtime_error` naming the game and DFA when one is missing, so a dangling
symlink reports clearly rather than failing obscurely.

Game names are parsed from the argument: `breakthrough_WxH`, `breakthroughcw_WxH`,
`amazons_WxH`, `clobber_WxH`, `othello_WxH`, `normalnim_*`, `tictactoe_N`, and
`chess+0` / `chess+1` / `chess+2*`.

## Core types

- `DFA` (`DFA.h`) — layered automaton. A **saved** DFA is one file and one
  mmap (`file_map`); a DFA **under construction** holds one staging map per
  layer (`layer_transitions`). State 0 is reject, 1 is accept.
  See `FORMAT-DFA.md` for the file format.
- `DFAString` — one position. Holds only `characters`; the shape is passed to
  the constructor for validation but deliberately **not stored**, because
  copying it per position doubled allocations during enumeration.
- `DFAIterator` — walks accepted strings in lexicographic order. Caches the
  state chain across `operator++` so advancing costs the carry depth rather
  than a full re-walk. Constructed only by `DFA::cbegin()` / `cend()`.
- `DFAUtil` — statics over `shared_dfa_ptr`: set operations, load/build with
  caching, and `for_each_position`.
- `Game` → `NormalPlayGame` → concrete games. `validate_moves` and
  `validate_result` take `const DFAString&` and are the position-level oracle
  the `verify_*` programs check the DFAs against.

## Parallelism

`parallel.h` wraps execution policies so non-libstdc++ builds fall back to
serial:

- `TRY_PARALLEL_2..6` use `std::execution::par_unseq` — only for callables that
  neither allocate nor lock.
- `TRY_PARALLEL_PAR_3` uses `std::execution::par` — for callables that do.
  Anything calling into `Game` validation or building vectors belongs here.

These are macros, so a lambda argument containing a top-level comma (e.g.
`std::pair<size_t, uint64_t>`) will not compile. Name the lambda first.

Thread-safety rules that hold today:

- The DFA read path is safe **provided `DFA::mmap()` runs before threads start**.
  After that `_mapped` is only read. `MemoryMap`'s `const` methods mutate
  (`mmap() const` sets `_mapped`, `munmap() const` clears it), so a
  `const MemoryMap&` is not thread-safe in general — never `munmap` a DFA while
  it is being read concurrently.
- `dfa_format::Layout` is immutable; `row_offset` is pure.
- Lazily-filled function-local statics are a hazard. `GameUtil::get_queen_moves`
  had exactly this bug — a `static` vector filled behind an `if (empty)` guard,
  which is not the same as a thread-safe magic static. It is now built in a
  `static const` initializer. Watch for the pattern elsewhere.

## Benchmarking and verifying changes

The convention that worked well here, and is worth repeating:

1. Build the baseline from git rather than reusing a stale binary:
   `git archive <commit> src | tar -x -C <tmp> --strip-components=1`, then
   `make -j8` there with the same flags.
2. Compare **byte-identical** stdout, stderr and exit status across a set of
   cases, including the degenerate ones — the reject DFA (`won,side_to_move=0`
   for a normal-play game is empty) and a deliberate failure case.
3. Time with **alternating** A/B runs, several each, and report the ratio.
   Absolute numbers drift with background load; check `uptime` before trusting
   them.
4. Watch `user` as well as `real`. Copy-elimination work shows up in `user`
   long before it shows up in `real`.

Two traps that cost real time in this repo:

- **Pick benchmark DFAs by expected runtime, not position count.** Per-position
  cost differs by ~100× between terminal DFAs, where `validate_moves` early
  returns, and non-terminal ones, where it builds a `DFAString` per legal move.
- **Gate on a DFA large enough to matter.** breakthrough_4x4 is 3.5×10⁷
  positions and runs in seconds; a parallel-enumeration change passed six cases
  there and then corrupted the heap on breakthrough_5x5 at ~2×10¹⁰ positions.
  See `src/TODO.md`.

Note that many DFA operations write to the shared caches under `scratch/`, so a
first run and a second run of the same command produce different output
(`building …` vs `loaded …`). Warm the caches with both binaries before
comparing.

## Shell

The interactive shell is zsh, which does **not** word-split unquoted parameters.
`$FLAGS` passes as a single argument where bash would split it. Use arrays or
write flags literally.
