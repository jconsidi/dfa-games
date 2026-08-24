# DFA File Format, version 1.0

A single-file, self-describing encoding for a layered deterministic finite
automaton over fixed-length strings.

## 1. Scope and model

A file encodes one DFA that accepts a set of strings of a fixed length
`ndim`. The character at index `i` is drawn from the alphabet
`{0, 1, ..., shape[i] - 1}`. Different indices may have different alphabet
sizes.

Because all accepted strings have the same length, the automaton is
*layered*: the states reachable after reading exactly `i` characters form
layer `i`, and every transition out of layer `i` leads into layer `i + 1`.
Layers are numbered `0` through `ndim - 1`. Layer `0` is the layer entered
before any character is read; layer `i` is consulted when reading the
character at index `i`.

The file stores no counts, no minimality claim, and no other derived data.
See section 9.

## 2. Conventions

All integers are unsigned and little-endian.

`align8(x)` denotes `x` rounded up to the next multiple of 8.

Byte offsets are absolute from the start of the file.

## 3. Layout

```
offset                    size          contents
0                         64            header
64                        8 * ndim      layer_size[]   u64
64 + 8 * ndim             8 * ndim      layer_offset[] u64
64 + 16 * ndim            4 * ndim      shape[]        u32
64 + 20 * ndim            pad           zero bytes to align8
align8(64 + 20 * ndim)    ...           transition blocks, ascending
```

### 3.1 Header

```
offset  size  field
0       8     magic:  44 46 41 31 0D 0A 1A 0A   ("DFA1" + CR LF SUB LF)
8       2     version_major  u16   = 1
10      2     version_minor  u16   = 0
12      4     header_bytes   u32   = 64
16      32    digest         SHA-256 of bytes [48, EOF)
48      4     ndim           u32
52      4     flags          u32
56      8     initial_state  u64
```

`magic` carries the usual byte-order and line-ending damage detector: the
CR LF SUB LF tail causes files mangled by text-mode transfer to fail
immediately rather than subtly.

`header_bytes` gives the offset of the first table. A reader must reject a
file whose `header_bytes` it does not understand, rather than guessing.

`digest` covers every byte from offset 48 to the end of the file — that is,
all content after the digest field itself. Verifying it is optional
(section 7).

`flags` bit 0 asserts canonical state numbering (section 8). All other bits
are reserved and must be written as zero.

`initial_state` is a state index in layer `0`, and must be less than
`layer_size[0]`.

### 3.2 Tables

`layer_size[i]` is the number of states in layer `i`, counting the two
reserved states. It is at least 2.

`shape[i]` is the alphabet size at index `i`. It is at least 1.

`layer_offset[i]` is the absolute byte offset of layer `i`'s transition
block. Its value is fully determined by the other tables (section 3.3); it
is stored so that readers can validate the layout cheaply and so that a
future version may relax it.

For the purposes of the rules below, define the *terminal pseudo-layer*
index `ndim`, with `layer_size[ndim] = 2`. It has no transition block.

### 3.3 Transition blocks

Layer `i`'s block holds `layer_size[i]` rows of `shape[i]` entries each,
in ascending row order and, within a row, ascending character order. Each
entry is `width[i]` bytes.

`width[i]` is **derived, never stored**: the smallest value in
`{1, 2, 4, 8}` satisfying

```
256 ** width[i] >= layer_size[i + 1]
```

Note that `width[ndim - 1]` is always 1, since `layer_size[ndim]` is 2.

Blocks appear in ascending layer order. Block `0` begins at
`align8(64 + 20 * ndim)`. Each subsequent block begins at the next 8-byte
boundary at or after the end of the previous block. Padding bytes between
blocks, and between the tables and block `0`, must be zero. The file ends
at the end of block `ndim - 1`; there are no trailing bytes.

Row `r` of layer `i` therefore begins at

```
layer_offset[i] + r * shape[i] * width[i]
```

and entry `c` of that row occupies `width[i]` bytes at

```
layer_offset[i] + (r * shape[i] + c) * width[i]
```

Because `width[i]` is derived and the layout is fixed, the encoded bytes
are a function of the automaton alone.

## 4. State semantics

Within every layer, two state indices are reserved:

- Index **0** is the *reject-all* state. No string continuing from it is
  accepted.
- Index **1** is the *accept-all* state. Every string continuing from it is
  accepted.

These meanings are fixed by index and hold at every layer, including layer
`ndim - 1` and the terminal pseudo-layer.

A conforming writer **must** store row 0 of every layer as all zeros and
row 1 of every layer as all ones. Writing any other values is an error and
produces a malformed file.

A reader is **not required** to consult rows 0 and 1 to determine
membership; the semantics are fixed by index, so a reader may skip those
rows entirely. If a reader does examine them and finds values other than
those above, it must reject the file as malformed.

States with index 2 or greater are ordinary. Their rows carry the
transitions.

## 5. Acceptance

A string `s` of length `ndim`, with `0 <= s[i] < shape[i]` for every `i`,
is accepted as follows:

```
state = initial_state
for i in 0 .. ndim - 1:
    if state == 0: return REJECT
    if state == 1: return ACCEPT
    state = entry(layer = i, row = state, character = s[i])
return state == 1
```

After the loop, `state` is an index into the terminal pseudo-layer and is
therefore 0 or 1.

The early returns are the operative definition, not an optimization: a
reader must treat indices 0 and 1 as terminal decisions wherever they
appear, without following stored transitions. A reader that instead walks
to the terminal pseudo-layer will agree on well-formed files but implements
a different rule, and will diverge on malformed degenerate sets (section 6).

A string with any character outside its alphabet is not in the domain of
the automaton; a reader should signal an error rather than return a
membership result.

## 6. Degenerate sets

The empty set is encoded as `initial_state = 0`, and the universal set as
`initial_state = 1`, in both cases with `layer_size[i] = 2` for every
layer. These files contain only the reserved rows.

They are ordinary conforming files and require no special handling beyond
the acceptance rule in section 5.

## 7. Validation

A reader **must** check, before using the file:

- `magic` matches.
- `version_major` is 1. A reader for version 1 must reject a file with a
  higher `version_major`, and may accept a higher `version_minor` by
  ignoring what it does not understand.
- `header_bytes` is 64.
- `ndim >= 1`; `shape[i] >= 1` and `layer_size[i] >= 2` for every layer.
- Reserved bits of `flags` are zero.
- `initial_state < layer_size[0]`.
- Every `layer_offset[i]` equals the value implied by section 3.3, and the
  file length equals the end of the last block.

A reader **may** additionally check, at its own cost:

- `digest` matches the file contents. Recommended once per file when
  reading from shared or network storage.
- Rows 0 and 1 of each layer hold the required values (section 4).
- Every entry in layer `i` is less than `layer_size[i + 1]`.

The last of these deserves emphasis. It is a single sequential pass, it
parallelizes trivially, and it detects index truncation and similar
encoding faults that would otherwise produce a structurally valid file
denoting the wrong set. Producers of long-running computations should run
it on every file they write.

## 8. Canonical numbering

`flags` bit 0, when set, asserts that ordinary states are numbered
canonically: within each layer, states `2 .. layer_size[i] - 1` are ordered
by their transition rows compared lexicographically as tuples of next-state
indices, ascending. Indices 0 and 1 keep their reserved meanings and are
not part of that ordering.

The ordering is well defined only if no two ordinary states in a layer have
identical rows - that is, if the automaton is minimal and free of
unreachable and dead states. Setting bit 0 therefore also asserts
minimality.

When bit 0 is set, the file is a function of the accepted language alone,
so digest equality is language equality. When it is clear, the file may
still be perfectly valid; nothing in sections 4 through 7 depends on the
numbering.

A reader is never required to verify canonicity, and a reader that ignores
bit 0 entirely is conforming.

## 9. Derived data

Cardinalities, per-layer statistics, bounding information, and any other
quantity computable from the automaton are deliberately excluded.

Storing them inside the file would either make the encoding depend on which
derived values happened to be computed, breaking the property that the
bytes are a function of the automaton, or invite writes into an object that
readers assume is immutable. Keep such data in a sidecar keyed by the
file's digest.

## 10. Writing

Files are immutable once named.

The recommended procedure is: write to a temporary name in the destination
directory, `fsync` the file, `rename` it into place, then `fsync` the
directory. If the destination already exists, skip — do not unlink and
replace, since another reader may hold it open.

Naming files by the hex digest gives content addressing: identical
automata, produced independently, land on the same name with identical
bytes.

## 11. Conformance test vectors

An implementation should be checked against at least:

- the empty set and the universal set, for some non-trivial shape;
- a singleton set, exercising a distinct nontrivial state per layer;
- a shape with differing alphabet sizes across indices;
- a shape crossing at least one `width` boundary, so that layers with
  1-, 2-, and 4-byte entries appear in one file;
- an `ndim` that is odd, exercising the padding after the `shape` table.
