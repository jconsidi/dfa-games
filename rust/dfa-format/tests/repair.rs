//! Recovering directories that hold leftover `layer=` files.
//!
//! `DFA::DFA(shape)` builds its scratch directory as `scratch/temp/<id>` with
//! a counter that is static per process, so two concurrent processes reuse the
//! same temp directories.  A shorter DFA landing in one an earlier, longer DFA
//! had used overwrites `layer=0` upward and leaves the tail behind, then
//! renames the mixture into place under its own correct hash.  The name is
//! therefore the evidence needed to recover the intended automaton.

use std::path::{Path, PathBuf};

use dfa_format::legacy::NameCheck;
use dfa_format::{convert, Automaton, Dfa, LegacyDfa};
use tempfile::TempDir;

fn three_layer() -> Automaton {
    let mut a = Automaton::new(vec![3, 2, 2]);
    let l2a = a.add_state(2, vec![0, 1]);
    let l2b = a.add_state(2, vec![1, 0]);
    let l1a = a.add_state(1, vec![l2a, l2b]);
    let l1b = a.add_state(1, vec![l2b, l2a]);
    let start = a.add_state(0, vec![l1a, l1b, 1]);
    a.set_initial_state(start);
    a
}

fn unrelated() -> Automaton {
    let mut b = Automaton::new(vec![4, 3]);
    let l1 = b.add_state(1, vec![1, 0, 1]);
    let start = b.add_state(0, vec![l1, 0, l1, 1]);
    b.set_initial_state(start);
    b
}

/// Write `a` into `store/<its legacy hash>`, then append `extra` layer files
/// taken from an unrelated DFA, reproducing the corruption.
fn mixed_directory(tmp: &Path, a: &Automaton, extra: &Automaton) -> PathBuf {
    let clean = tmp.join("clean");
    a.write_legacy_dir(&clean).unwrap();
    let hash = LegacyDfa::open(&clean).unwrap().legacy_hash().unwrap();

    let mixed = tmp.join("store").join(&hash);
    a.write_legacy_dir(&mixed).unwrap();

    let junk = tmp.join("junk");
    extra.write_legacy_dir(&junk).unwrap();
    for (i, layer) in (a.ndim()..a.ndim() + extra.ndim()).enumerate() {
        std::fs::copy(
            junk.join(format!("layer={i}")),
            mixed.join(format!("layer={layer}")),
        )
        .unwrap();
    }
    mixed
}

#[test]
fn leftover_layers_are_detected_and_dropped() {
    let tmp = TempDir::new().unwrap();
    let a = three_layer();
    let mixed = mixed_directory(tmp.path(), &a, &unrelated());

    let legacy = LegacyDfa::open(&mixed).unwrap();
    assert_eq!(legacy.ndim(), 5, "the mixture really does have five layers");

    match legacy.check_name().unwrap() {
        NameCheck::Repaired {
            ndim,
            extra_layers,
            hash,
        } => {
            assert_eq!(ndim, 3);
            assert_eq!(extra_layers, 2);
            assert_eq!(hash, mixed.file_name().unwrap().to_str().unwrap());
        }
        other => panic!("expected a repair, got {other:?}"),
    }
}

#[test]
fn a_repaired_directory_converts_to_the_intended_automaton() {
    let tmp = TempDir::new().unwrap();
    let a = three_layer();
    let mixed = mixed_directory(tmp.path(), &a, &unrelated());
    let out = tmp.path().join("out");

    let legacy = LegacyDfa::open(&mixed).unwrap();
    let NameCheck::Repaired { ndim, .. } = legacy.check_name().unwrap() else {
        panic!("expected a repair");
    };
    let repaired = convert(&legacy.truncated(ndim), &out, true).unwrap();

    // Identical to converting the uncorrupted directory.
    let clean = LegacyDfa::open(&tmp.path().join("clean")).unwrap();
    let expected = convert(&clean, &tmp.path().join("out2"), true).unwrap();
    assert_eq!(repaired.digest, expected.digest);

    let dfa = Dfa::open(&repaired.path).unwrap();
    assert_eq!(dfa.layout().ndim(), 3);
    for s in a.all_strings() {
        assert_eq!(dfa.accepts(&s).unwrap(), a.accepts(&s), "on {s:?}");
    }
}

#[test]
fn converting_the_mixture_whole_would_have_been_wrong() {
    // Without the repair the converter would happily produce a valid file for
    // a five layer automaton that never existed, so this pins down that the
    // two really do differ.
    let tmp = TempDir::new().unwrap();
    let a = three_layer();
    let mixed = mixed_directory(tmp.path(), &a, &unrelated());

    let legacy = LegacyDfa::open(&mixed).unwrap();
    let whole = convert(&legacy, &tmp.path().join("whole"), false).unwrap();
    let repaired = convert(&legacy.truncated(3), &tmp.path().join("part"), false).unwrap();
    assert_ne!(whole.digest, repaired.digest);
}

#[test]
fn a_name_that_no_prefix_explains_is_a_mismatch() {
    let tmp = TempDir::new().unwrap();
    let a = three_layer();
    let mixed = mixed_directory(tmp.path(), &a, &unrelated());

    // Corrupt a transition in layer 0, so neither the whole directory nor any
    // prefix of it reproduces the name.
    let path = mixed.join("layer=0");
    let mut bytes = std::fs::read(&path).unwrap();
    let last = bytes.len() - 4;
    bytes[last] = 0;
    std::fs::write(&path, &bytes).unwrap();

    let legacy = LegacyDfa::open(&mixed).unwrap();
    match legacy.check_name().unwrap() {
        NameCheck::Mismatch { hash, stored } => assert_ne!(hash, stored),
        other => panic!("expected a mismatch, got {other:?}"),
    }
}

#[test]
fn an_intact_directory_needs_no_repair() {
    let tmp = TempDir::new().unwrap();
    let a = three_layer();
    let clean = tmp.path().join("clean");
    a.write_legacy_dir(&clean).unwrap();
    let hash = LegacyDfa::open(&clean).unwrap().legacy_hash().unwrap();

    let stored = tmp.path().join("store").join(&hash);
    a.write_legacy_dir(&stored).unwrap();

    match LegacyDfa::open(&stored).unwrap().check_name().unwrap() {
        NameCheck::Matches { hash: h } => assert_eq!(h, hash),
        other => panic!("expected a match, got {other:?}"),
    }

    // A directory that is not named by a hash has nothing to be checked against.
    match LegacyDfa::open(&clean).unwrap().check_name().unwrap() {
        NameCheck::Unnamed { .. } => {}
        other => panic!("expected unnamed, got {other:?}"),
    }
}
