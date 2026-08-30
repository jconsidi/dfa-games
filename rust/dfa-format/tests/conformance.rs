//! The test vectors of FORMAT-DFA.md section 11, plus negative tests.
//!
//! Every vector is built as an in-memory `Automaton` and run through
//! `write_automaton`, so these exercise this crate's one writer rather than a
//! parallel implementation written for the tests.

use std::path::{Path, PathBuf};

use dfa_format::layout::{self, Layout};
use dfa_format::{validate, write_automaton, Automaton, Dfa, ValidateOptions};
use tempfile::TempDir;

/// Write `automaton` into a fresh temp directory and return the file path.
/// The `TempDir` is returned too, since dropping it deletes the file.
fn build(automaton: &Automaton) -> (TempDir, PathBuf) {
    let tmp = TempDir::new().expect("temp dir");
    let out = tmp.path().join("out");
    let converted = write_automaton(automaton, &out, true).expect("write");
    (tmp, converted.path)
}

fn assert_valid(path: &Path) {
    let report = validate(path, &ValidateOptions::default()).expect("validate");
    assert!(
        report.ok(),
        "expected a valid file, got: {:?}",
        report.violations
    );
}

/// Every string of the shape must be classified the same way by the file and
/// by the in-memory reference implementation.
fn assert_same_language(automaton: &Automaton, path: &Path) {
    let dfa = Dfa::open(path).expect("open converted file");
    for s in automaton.all_strings() {
        assert_eq!(
            dfa.accepts(&s).expect("accepts"),
            automaton.accepts(&s),
            "disagreement on {s:?}"
        );
    }
}

#[test]
fn empty_set() {
    let mut a = Automaton::new(vec![3, 2, 4]);
    a.set_initial_state(0);
    let (_tmp, path) = build(&a);
    assert_valid(&path);
    assert_same_language(&a, &path);

    let dfa = Dfa::open(&path).unwrap();
    assert!(!dfa.accepts(&[0, 0, 0]).unwrap());
    // Only the reserved rows are present (spec 6).
    assert_eq!(dfa.layout().layer_size(), &[2, 2, 2]);
}

#[test]
fn universal_set() {
    let mut a = Automaton::new(vec![3, 2, 4]);
    a.set_initial_state(1);
    let (_tmp, path) = build(&a);
    assert_valid(&path);
    assert_same_language(&a, &path);

    let dfa = Dfa::open(&path).unwrap();
    assert!(dfa.accepts(&[2, 1, 3]).unwrap());
}

/// A singleton, which needs one distinct ordinary state per layer.
fn singleton(shape: Vec<u32>, word: &[u32]) -> Automaton {
    let mut a = Automaton::new(shape.clone());
    let ndim = shape.len();
    // Build backwards: the last layer sends the right character to accept-all.
    let mut next = 1u32;
    for layer in (0..ndim).rev() {
        let mut row = vec![0u32; shape[layer] as usize];
        row[word[layer] as usize] = next;
        next = a.add_state(layer, row);
    }
    a.set_initial_state(next);
    a
}

#[test]
fn singleton_set() {
    let a = singleton(vec![3, 2, 4], &[2, 0, 3]);
    let (_tmp, path) = build(&a);
    assert_valid(&path);
    assert_same_language(&a, &path);

    let dfa = Dfa::open(&path).unwrap();
    assert!(dfa.accepts(&[2, 0, 3]).unwrap());
    assert!(!dfa.accepts(&[2, 0, 2]).unwrap());
    assert_eq!(dfa.layout().layer_size(), &[3, 3, 3]);
}

#[test]
fn differing_alphabet_sizes() {
    let a = singleton(vec![1, 7, 2, 5], &[0, 6, 1, 4]);
    let (_tmp, path) = build(&a);
    assert_valid(&path);
    assert_same_language(&a, &path);
    assert_eq!(Dfa::open(&path).unwrap().layout().shape(), &[1, 7, 2, 5]);
}

#[test]
fn odd_ndim_pads_after_the_shape_table() {
    // 64 + 20 * 5 = 164, so four padding bytes precede block 0.
    let a = singleton(vec![2, 3, 2, 3, 2], &[1, 2, 0, 1, 1]);
    let (_tmp, path) = build(&a);
    assert_valid(&path);
    assert_same_language(&a, &path);

    let dfa = Dfa::open(&path).unwrap();
    assert_eq!(dfa.layout().tables_end(), 164);
    assert_eq!(dfa.layout().layer_offset()[0], 168);
}

#[test]
fn widths_of_one_two_and_four_bytes_in_one_file() {
    // width[i] comes from layer_size[i + 1]: 70000 needs 4 bytes, 300 needs 2,
    // 5 needs 1, and the terminal pseudo-layer always needs 1.
    let mut a = Automaton::new(vec![70000, 2, 2, 2]);

    for i in 2..5u32 {
        a.add_state(3, vec![if i % 2 == 0 { 1 } else { 0 }, 1]);
    }
    for i in 2..300u32 {
        a.add_state(2, vec![2 + (i % 3), 2 + ((i + 1) % 3)]);
    }
    for i in 2..70000u32 {
        a.add_state(1, vec![2 + (i % 298), 2 + ((i * 7) % 298)]);
    }
    let mut row0 = vec![0u32; 70000];
    for (c, slot) in row0.iter_mut().enumerate() {
        *slot = 2 + (c as u32 % 69998);
    }
    let start = a.add_state(0, row0);
    a.set_initial_state(start);

    let (_tmp, path) = build(&a);
    assert_valid(&path);

    let dfa = Dfa::open(&path).unwrap();
    assert_eq!(dfa.layout().width(), &[4, 2, 1, 1]);

    // Spot check rather than enumerating 70000 * 2 * 2 * 2 strings.
    for s in [
        vec![0u32, 0, 0, 0],
        vec![1, 1, 1, 1],
        vec![69999, 0, 1, 0],
        vec![12345, 1, 0, 1],
        vec![65535, 0, 0, 1],
        vec![65536, 1, 1, 0],
    ] {
        assert_eq!(
            dfa.accepts(&s).unwrap(),
            a.accepts(&s),
            "disagreement on {s:?}"
        );
    }
}

#[test]
fn encoding_is_a_function_of_the_automaton() {
    let a = singleton(vec![3, 2, 4], &[2, 0, 3]);
    let (_t1, first) = build(&a);
    let (_t2, second) = build(&a);
    assert_eq!(
        std::fs::read(&first).unwrap(),
        std::fs::read(&second).unwrap(),
        "the same automaton must produce identical bytes"
    );
}

// --- canonical numbering (spec 8) -------------------------------------------

/// Rows strictly ascending in every layer, so bit 0 must end up set.
fn canonical_automaton() -> Automaton {
    let mut a = Automaton::new(vec![2, 2, 2]);
    a.add_state(2, vec![0, 1]); // row 2
    a.add_state(2, vec![1, 0]); // row 3
    a.add_state(1, vec![2, 3]); // row 2
    a.add_state(1, vec![3, 2]); // row 3
    a.add_state(0, vec![2, 3]);
    a.set_initial_state(2);
    a
}

#[test]
fn canonical_source_sets_the_flag() {
    let a = canonical_automaton();
    let (_tmp, path) = build(&a);
    assert_valid(&path);
    assert!(Dfa::open(&path).unwrap().header().canonical());
}

#[test]
fn unordered_source_leaves_the_flag_clear() {
    let mut a = canonical_automaton();
    // Swap two ordinary rows in layer 1, which breaks the ordering without
    // changing the language.
    let r2 = a.row(1, 2).to_vec();
    let r3 = a.row(1, 3).to_vec();
    a.set_row(1, 2, r3);
    a.set_row(1, 3, r2);
    // Keep the language intact by swapping the references to them.
    a.set_row(0, 2, vec![3, 2]);

    let (_tmp, path) = build(&a);
    assert_valid(&path);
    let dfa = Dfa::open(&path).unwrap();
    assert!(!dfa.header().canonical());
    for s in a.all_strings() {
        assert_eq!(dfa.accepts(&s).unwrap(), a.accepts(&s));
    }
}

#[test]
fn canonical_order_compares_integers_not_bytes() {
    // Layer 1 is two bytes wide, and its two ordinary rows begin with 2 and
    // 256.  As integers that ascends; as little-endian bytes, 02 00 sorts
    // after 00 01, so a byte-wise comparison would call this file unordered.
    let mut a = Automaton::new(vec![2, 300, 9]);

    // Layer 2: the 255 smallest non-uniform 9 bit patterns, most significant
    // bit first, which ascend lexicographically as the pattern ascends.
    for pattern in 1u32..=255 {
        let row: Vec<u32> = (0..9u32).rev().map(|bit| (pattern >> bit) & 1).collect();
        a.add_state(2, row);
    }
    assert_eq!(a.layer_size(2), 257);

    // Layer 1: one row counting up from 2 and one counting down from 256, so
    // between them they reach every ordinary state of layer 2.
    let up: Vec<u32> = (0..300u32)
        .map(|c| if c < 255 { 2 + c } else { 2 })
        .collect();
    let down: Vec<u32> = (0..300u32)
        .map(|c| if c < 255 { 256 - c } else { 2 })
        .collect();
    assert_eq!((up[0], down[0]), (2, 256));
    a.add_state(1, up);
    a.add_state(1, down);

    let start = a.add_state(0, vec![2, 3]);
    a.set_initial_state(start);

    let (_tmp, path) = build(&a);
    assert_valid(&path);

    let dfa = Dfa::open(&path).unwrap();
    assert_eq!(dfa.layout().width()[1], 2);
    assert!(
        dfa.header().canonical(),
        "2 < 256 as integers, so this source is canonically ordered"
    );
}

#[test]
fn ordered_but_unreachable_leaves_the_flag_clear() {
    // Spec 8 makes bit 0 assert minimality as well as ordering, so an ordered
    // source with a state nothing can enter must not get the flag.
    let mut a = canonical_automaton();
    // Layer 1 gains a fourth state that sorts after the others but that no
    // transition in layer 0 points at.
    a.add_state(1, vec![3, 3]);

    let (_tmp, path) = build(&a);
    assert_valid(&path);
    let dfa = Dfa::open(&path).unwrap();
    assert!(
        !dfa.header().canonical(),
        "layer 1 state 4 is unreachable, so the file is not minimal"
    );
}

#[test]
fn an_ordinary_row_repeating_a_reserved_state_leaves_the_flag_clear() {
    let mut a = Automaton::new(vec![2, 2]);
    a.add_state(1, vec![0, 1]);
    // An ordinary row that accepts everything simply repeats state 1.
    a.add_state(1, vec![1, 1]);
    let start = a.add_state(0, vec![2, 3]);
    a.set_initial_state(start);

    let (_tmp, path) = build(&a);
    assert_valid(&path);
    assert!(!Dfa::open(&path).unwrap().header().canonical());
}

// --- negative tests ---------------------------------------------------------

/// Copy a good file, corrupt it at `offset`, and return the new path.
fn corrupt(tmp: &TempDir, good: &Path, offset: u64, bytes: &[u8]) -> PathBuf {
    let mut data = std::fs::read(good).unwrap();
    data[offset as usize..offset as usize + bytes.len()].copy_from_slice(bytes);
    let path = tmp.path().join(format!("corrupt-{offset}.dfa"));
    std::fs::write(&path, &data).unwrap();
    path
}

fn violations(path: &Path) -> Vec<String> {
    validate(path, &ValidateOptions::default())
        .unwrap()
        .violations
        .iter()
        .map(|v| v.message.clone())
        .collect()
}

fn assert_rejected(path: &Path, expected: &str) {
    let found = violations(path);
    assert!(
        found.iter().any(|m| m.contains(expected)),
        "expected a violation mentioning {expected:?}, got {found:?}"
    );
}

#[test]
fn negative_cases_are_all_rejected() {
    let a = singleton(vec![3, 2, 4], &[2, 0, 3]);
    let (tmp, good) = build(&a);
    assert_valid(&good);

    assert_rejected(&corrupt(&tmp, &good, layout::OFF_MAGIC, b"XXXX"), "magic");
    assert_rejected(
        &corrupt(&tmp, &good, layout::OFF_VERSION_MAJOR, &2u16.to_le_bytes()),
        "version_major",
    );
    assert_rejected(
        &corrupt(&tmp, &good, layout::OFF_HEADER_BYTES, &48u32.to_le_bytes()),
        "header_bytes",
    );
    assert_rejected(
        &corrupt(&tmp, &good, layout::OFF_DIGEST, &[0xAA; 4]),
        "digest",
    );
    assert_rejected(
        &corrupt(
            &tmp,
            &good,
            layout::OFF_FLAGS,
            &0xFFFF_FFFFu32.to_le_bytes(),
        ),
        "reserved flag bits",
    );
    assert_rejected(
        &corrupt(&tmp, &good, layout::OFF_INITIAL_STATE, &99u64.to_le_bytes()),
        "initial_state",
    );

    // A wrong layer_offset: the table starts right after layer_size[].
    let ndim = 3u64;
    let offset_table = layout::OFF_TABLES + 8 * ndim;
    assert_rejected(
        &corrupt(&tmp, &good, offset_table, &4096u64.to_le_bytes()),
        "layer_offset[0]",
    );

    // An entry pointing past the next layer.  Row 2 of layer 0 is one byte per
    // entry here, so a single byte does it.
    let lay = Layout::new(vec![3, 2, 4], vec![3, 3, 3]).unwrap();
    assert_rejected(
        &corrupt(&tmp, &good, lay.entry_offset(0, 2, 0), &[7]),
        "has only 3 states",
    );

    // Padding after the shape table must be zero.
    assert_rejected(&corrupt(&tmp, &good, lay.tables_end(), &[1]), "padding");

    // Rows 0 and 1 carry fixed values.
    assert_rejected(
        &corrupt(&tmp, &good, lay.entry_offset(0, 1, 0), &[0]),
        "reserved row 1",
    );

    // Truncation.
    let truncated = tmp.path().join("truncated.dfa");
    let data = std::fs::read(&good).unwrap();
    std::fs::write(&truncated, &data[..data.len() - 1]).unwrap();
    assert_rejected(&truncated, "the layout implies");

    let stub = tmp.path().join("stub.dfa");
    std::fs::write(&stub, b"DFA1").unwrap();
    assert_rejected(&stub, "shorter than the 64 byte header");
}

#[test]
fn a_digest_named_file_must_be_named_after_its_digest() {
    let a = singleton(vec![3, 2, 4], &[2, 0, 3]);
    let (tmp, good) = build(&a);

    // The converter names its output after the digest, so the file it just
    // produced must already satisfy this.
    assert_valid(&good);
    let digest = good.file_stem().unwrap().to_str().unwrap().to_string();
    assert_eq!(digest.len(), 64);

    // A file under someone else's digest is lying about its contents.
    let wrong = tmp.path().join(format!("{}.dfa", "a".repeat(64)));
    std::fs::copy(&good, &wrong).unwrap();
    assert_rejected(&wrong, "but its digest is");

    // The complaint is about the name alone: the bytes are untouched, so the
    // digest check still passes and that is the only violation.
    let found = violations(&wrong);
    assert_eq!(found.len(), 1, "{found:?}");
}

#[test]
fn a_file_not_named_after_a_digest_makes_no_claim() {
    let a = singleton(vec![3, 2, 4], &[2, 0, 3]);
    let (tmp, good) = build(&a);

    for name in [
        "positions.dfa",                    // an ordinary name
        ".tmp-1234-0.dfa",                  // what the converter writes to
        "ABCDEF0123456789.dfa",             // too short to be a digest
        &format!("{}.bin", "0".repeat(64)), // right stem, wrong extension
    ] {
        let path = tmp.path().join(name);
        std::fs::copy(&good, &path).unwrap();
        let report = validate(&path, &ValidateOptions::default()).unwrap();
        assert!(
            report.ok(),
            "{name} should be accepted: {:?}",
            report.violations
        );
    }
}

#[test]
fn a_renamed_and_edited_file_reports_both_lies() {
    let a = singleton(vec![3, 2, 4], &[2, 0, 3]);
    let (tmp, good) = build(&a);

    // Corrupt the digest field, then store the result under yet another name.
    let corrupted = corrupt(&tmp, &good, layout::OFF_DIGEST, &[0xAA; 4]);
    let renamed = tmp.path().join(format!("{}.dfa", "b".repeat(64)));
    std::fs::rename(&corrupted, &renamed).unwrap();

    let found = violations(&renamed);
    assert!(
        found.iter().any(|m| m.contains("but its digest is")),
        "{found:?}"
    );
    assert!(
        found.iter().any(|m| m.contains("but the contents hash to")),
        "{found:?}"
    );
}

#[test]
fn the_file_length_must_match_exactly_not_merely_suffice() {
    // Spec 3.3: the file ends at the end of the last block, with no trailing
    // bytes.  A file that is long enough is not thereby correct.
    let a = singleton(vec![3, 2, 4], &[2, 0, 3]);
    let (tmp, good) = build(&a);

    let mut data = std::fs::read(&good).unwrap();
    let implied = data.len();
    data.extend_from_slice(&[0u8; 8]);
    let trailing = tmp.path().join("trailing.dfa");
    std::fs::write(&trailing, &data).unwrap();

    assert_rejected(&trailing, &format!("but the layout implies {implied}"));
}

#[test]
fn padding_between_blocks_must_be_zero() {
    // Distinct from the padding after the shape table, and on its own branch.
    let a = singleton(vec![3, 2, 4], &[2, 0, 3]);
    let (tmp, good) = build(&a);

    let lay = Layout::new(vec![3, 2, 4], vec![3, 3, 3]).unwrap();
    let gap = lay.layer_offset()[0] + lay.block_bytes()[0];
    assert!(gap < lay.layer_offset()[1], "this shape must actually pad");

    assert_rejected(&corrupt(&tmp, &good, gap, &[1]), "padding after block 0");
}

#[test]
fn canonical_flag_without_canonical_order_is_rejected() {
    // Take an unordered file and lie about it by setting bit 0 by hand.
    let mut a = canonical_automaton();
    let r2 = a.row(1, 2).to_vec();
    let r3 = a.row(1, 3).to_vec();
    a.set_row(1, 2, r3);
    a.set_row(1, 3, r2);
    a.set_row(0, 2, vec![3, 2]);

    let (tmp, path) = build(&a);
    assert!(!Dfa::open(&path).unwrap().header().canonical());

    let liar = corrupt(&tmp, &path, layout::OFF_FLAGS, &1u32.to_le_bytes());
    // The digest no longer matches either, but the ordering complaint is the
    // one being tested here.
    assert_rejected(&liar, "does not sort after");
}
