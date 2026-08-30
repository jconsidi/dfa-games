//! Legacy directory in, `.dfa` out, same language on the way through.

use dfa_format::{convert, Automaton, Dfa, LegacyDfa};
use tempfile::TempDir;

/// A moderately tangled automaton over a mixed shape, built so that every
/// layer has several distinct ordinary states and the language is not
/// trivially empty or universal.
fn tangled() -> Automaton {
    let shape = vec![3u32, 2, 4, 2, 3];
    let mut a = Automaton::new(shape.clone());

    // Last layer: a couple of distinct accept/reject patterns.
    let l4a = a.add_state(4, vec![1, 0, 1]);
    let l4b = a.add_state(4, vec![0, 1, 0]);

    let l3a = a.add_state(3, vec![l4a, l4b]);
    let l3b = a.add_state(3, vec![l4b, 1]);
    let l3c = a.add_state(3, vec![0, l4a]);

    let l2a = a.add_state(2, vec![l3a, l3b, l3c, 0]);
    let l2b = a.add_state(2, vec![l3c, 1, l3a, l3b]);

    let l1a = a.add_state(1, vec![l2a, l2b]);
    let l1b = a.add_state(1, vec![l2b, 0]);

    let start = a.add_state(0, vec![l1a, l1b, 1]);
    a.set_initial_state(start);
    a
}

#[test]
fn legacy_directory_round_trips_through_the_new_format() {
    let a = tangled();
    let tmp = TempDir::new().unwrap();
    let src = tmp.path().join("legacy");
    a.write_legacy_dir(&src).unwrap();

    let legacy = LegacyDfa::open(&src).unwrap();
    assert_eq!(legacy.shape(), a.shape());
    assert_eq!(legacy.initial_state(), a.initial_state());
    for layer in 0..a.ndim() {
        assert_eq!(legacy.layer_size()[layer], a.layer_size(layer));
    }

    let out = tmp.path().join("out");
    let converted = convert(&legacy, &out, true).unwrap();
    assert!(!converted.already_existed);

    let dfa = Dfa::open(&converted.path).unwrap();
    let strings = a.all_strings();
    assert_eq!(strings.len(), 3 * 2 * 4 * 2 * 3);

    let mut accepted = 0usize;
    for s in &strings {
        let from_file = dfa.accepts(s).unwrap();
        assert_eq!(from_file, a.accepts(s), "disagreement on {s:?}");
        if from_file {
            accepted += 1;
        }
    }
    // Guard against a vacuous test: the language must be a proper subset.
    assert!(
        accepted > 0 && accepted < strings.len(),
        "accepted {accepted}"
    );
}

#[test]
fn republishing_the_same_automaton_is_a_no_op() {
    let a = tangled();
    let tmp = TempDir::new().unwrap();
    let src = tmp.path().join("legacy");
    a.write_legacy_dir(&src).unwrap();
    let legacy = LegacyDfa::open(&src).unwrap();
    let out = tmp.path().join("out");

    let first = convert(&legacy, &out, false).unwrap();
    assert!(!first.already_existed);
    let second = convert(&legacy, &out, false).unwrap();
    assert!(second.already_existed);
    assert_eq!(first.digest, second.digest);
    assert_eq!(first.path, second.path);

    // The temp file must not survive either attempt.
    let leftovers: Vec<_> = std::fs::read_dir(&out)
        .unwrap()
        .filter_map(|e| e.ok())
        .filter(|e| e.file_name().to_string_lossy().starts_with(".tmp-"))
        .collect();
    assert!(leftovers.is_empty(), "temp files left behind");
}
