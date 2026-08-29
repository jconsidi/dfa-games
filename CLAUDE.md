# dfa-games

Solves combinatorial games by representing sets of positions as layered DFAs.
A position is a string of one character per board square; a set of positions is
the language of a DFA over that alphabet. Set operations (union, intersection,
difference) and move generation are DFA operations, which is what makes solving
whole games tractable.

The C++ solver is in `src/` and builds and runs from there. `rust/` holds an
incremental Rust port of the read side, described under **Rust** below; its
binaries also expect to be run from `src/`.

## Priorities

**Provable correctness overrides everything else here** — speed, convenience,
API tidiness, and consistency with neighbouring code. This project computes
answers nobody will check by hand, so a wrong result that looks like a right
one is the worst thing it can produce.

**Always favor stopping with an error over silence.** In practice:

- A check that disproves what it was asked about raises an error or throws. It
  must not return a verdict as a value the caller can drop — and do not offer a
  second, report-shaped entry point alongside, because that form can be called
  and ignored. Where statistics are also wanted, return them on the success
  path. `verify_dfa_union` (`rust/dfa-format/src/union.rs`) is the worked
  example, and the reason: it once returned a report, a caller discarded it
  with `?`, and the union checks in `verify-backward-sound` passed a corrupted
  DFA while reporting success.
- Fail loudly on an unexpected condition rather than continuing with a
  plausible default or a quietly narrowed check.
- Never weaken or skip a check to make something build, pass, or run faster.
  Raise the tradeoff instead.
- Say what was checked. A check that prints nothing on success cannot be told
  apart from one that never ran.

This is why `-DNDEBUG` is absent from the build (see **Building**) and why the
`verify_*` programs exist at all: they are an independent oracle over results
the solver has no other way to justify.

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

## Rust

`rust/` is a cargo workspace holding a port of the parts of this repo that
*read* solver output. Nothing there constructs a DFA — building is still the
C++'s job.

    cd rust && cargo build --release && cargo test --release

Both crates build clean under `RUSTFLAGS="-D warnings" cargo clippy --release
--all-targets`, and the workspace is `cargo fmt --all --check` clean. Keep them
both that way. Clippy is the closest thing here to the C++ side's `-Werror`;
`fmt` is a gate so that a diff shows what changed rather than how it was
wrapped. Run

    cd rust && cargo fmt --all && RUSTFLAGS="-D warnings" cargo clippy --release --all-targets

before committing Rust, and never hand-wrap around rustfmt — if a line reads
badly after formatting, change the code, not the whitespace.

- `dfa-format` — the single authority on the `.dfa` file format
  (`FORMAT-DFA.md`). `layout.rs` owns every byte offset, so the reader and
  writer cannot drift. `Dfa::positions()` in `iter.rs` is the port of
  `DFAIterator` and enumerates accepted strings in lexicographic order.
  Binaries: `dfa-validate`, `dfa-stats`, `dfa-convert`.
- `dfa-games` — position level rules (`validate_moves`, `validate_result`,
  `position_to_string`) and the verifiers over them. Binaries:
  `verify-lost-sound`, `verify-won-sound`, `verify-losing-sound`,
  `verify-winning-sound`, `verify-backward-sound`, taking the same arguments
  as their C++ namesakes.

Every binary takes `--scratch`, defaulting to `scratch`, so run them from
`src/` where that resolves:

    cd src && ../rust/target/release/verify-backward-sound breakthrough_4x4 1

`cargo test` includes a config driven check of the rules themselves:
`config/<game>/positions-manual.json` holds hand written positions with
`expected_moves` and `expected_result`, read by `tests/positions_manual.rs` the
way `test_perft_u` reads `tests.json`. A file naming a game the Rust cannot
build fails the run rather than being skipped, since test data that never
executes is worse than none.

Coverage is narrower than the C++ and the gaps matter:

- **amazons**, **row-wise breakthrough**, **clobber**, **normalnim** and
  **tictactoe** have Rust rules. `breakthroughcw_`, `chess+` and `othello_` are
  recognized by name so the error says "not ported" rather than
  "unrecognized" — but they are genuinely not verifiable from Rust.
- The rules those modules implement are written down in `GAMES.md`, which
  describes the games and nothing else — no shapes, no square numbering, no
  file names. Rules for a new game come from there, or from the game itself,
  **not** from porting the C++ move graph: a transcription reproduces whatever
  the C++ gets wrong and then agrees with it. Only the encoding is read from
  the C++, because it is the interface the two sides must share; it belongs in
  the Rust module doc. `breakthrough.rs` and `amazons.rs` predate this and are
  ports, which `rust/TODO.md` records.
- Solved sets contain positions unreachable from the starting position — both
  sides holding a winning condition at once, or the side to move holding one.
  Backward solving covers every position on purpose, since testing
  reachability in the set representation would cost more than solving the
  extras, which resolve within about 2 ply. Rules must not treat such a
  position as a failure: tictactoe stops the side to move only on the
  *opponent's* line for exactly this reason.
- The Rust verifiers do **not** consult `lost,side_to_move=N` /
  `won,side_to_move=N` the way `verify_utils.cpp` does. Each position generates
  its moves once and an empty move list branches to `validate_result`, which
  must return the base case the DFA asserts. This is deliberate: it keeps DFA
  construction out of a program that only reads, and `won,side_to_move=N` is
  the reject DFA for a normal play game so that half was dead code anyway. It
  is *stricter* than the C++ on both branches. What it gives up is that the
  losing and winning verifiers no longer incidentally cross check the `lost`
  DFA, so keep running `verify-lost-sound` on `lost,side_to_move=N` itself.

The two implementations are meant to disagree loudly, so when changing either,
check both against the same DFAs. `verify-backward-sound breakthrough_4x4 1`
and `./verify_backward_sound breakthrough_4x4 1` cover eight DFAs in well under
two minutes between them. The messages differ by design — only the counts and
the verdicts are comparable.

Enumeration costs roughly 100 ns per position on its own; the rules dominate
after that, and by a lot. Breakthrough is about 0.2 us per position, amazons
about 16 us, so pick a case by expected runtime rather than position count —
the same trap the C++ benchmarking notes below describe.

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
