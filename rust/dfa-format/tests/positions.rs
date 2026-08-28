//! `Dfa::positions()` against the reference implementation in `Automaton`.
//!
//! `Automaton::all_strings()` filtered through `Automaton::accepts()` is the
//! definition of what the iterator should produce, computed a completely
//! different way: brute force over the whole shape, no state chain, no
//! carrying.  Every test here is that comparison on a different automaton.

use dfa_format::{convert, Automaton, Dfa, LegacyDfa};
use tempfile::TempDir;

/// Publish an in-memory automaton and open it back as a real `.dfa` file, so
/// the iterator runs over the same bytes production reads.
fn published(a: &Automaton, tmp: &TempDir, tag: &str) -> Dfa {
    let src = tmp.path().join(format!("legacy-{tag}"));
    a.write_legacy_dir(&src).unwrap();
    let legacy = LegacyDfa::open(&src).unwrap();
    let out = tmp.path().join("out");
    let converted = convert(&legacy, &out, true).unwrap();
    Dfa::open(&converted.path).unwrap()
}

fn expected(a: &Automaton) -> Vec<Vec<u32>> {
    a.all_strings()
        .into_iter()
        .filter(|s| a.accepts(s))
        .collect()
}

fn enumerated(dfa: &Dfa) -> Vec<Vec<u32>> {
    dfa.positions().map(|p| p.unwrap()).collect()
}

/// Same automaton the roundtrip test uses: a mixed shape where every layer has
/// several ordinary states and the language is a proper subset.
fn tangled() -> Automaton {
    let shape = vec![3u32, 2, 4, 2, 3];
    let mut a = Automaton::new(shape);

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
fn enumerates_exactly_the_accepted_strings_in_order() {
    let a = tangled();
    let tmp = TempDir::new().unwrap();
    let dfa = published(&a, &tmp, "tangled");

    let want = expected(&a);
    // Guard against a vacuous comparison: a proper, non-empty subset.
    assert!(
        !want.is_empty() && want.len() < a.all_strings().len(),
        "accepted {} of {}",
        want.len(),
        a.all_strings().len()
    );

    assert_eq!(enumerated(&dfa), want);
}

#[test]
fn order_is_lexicographic() {
    let a = tangled();
    let tmp = TempDir::new().unwrap();
    let dfa = published(&a, &tmp, "order");

    let got = enumerated(&dfa);
    let mut sorted = got.clone();
    sorted.sort();
    sorted.dedup();
    assert_eq!(got, sorted, "not strictly increasing");
}

#[test]
fn count_agrees_with_count_accepted() {
    let a = tangled();
    let tmp = TempDir::new().unwrap();
    let dfa = published(&a, &tmp, "count");

    // The check every verifier makes at the end of a run, on an automaton
    // small enough that the f64 count is exact beyond any doubt.
    let counted = dfa_format::stats::count_accepted(&dfa).unwrap();
    assert_eq!(dfa.positions().count() as f64, counted);
}

#[test]
fn accepts_everything() {
    // The state 1 row is uniform, so the walk has to follow it like any other.
    let mut a = Automaton::new(vec![2u32, 3, 2]);
    a.set_initial_state(1);
    let tmp = TempDir::new().unwrap();
    let dfa = published(&a, &tmp, "all");

    let got = enumerated(&dfa);
    assert_eq!(got.len(), 2 * 3 * 2);
    assert_eq!(got, a.all_strings());
}

#[test]
fn accepts_nothing() {
    // won,side_to_move=N is this DFA for a normal play game, so the verifiers
    // hit it routinely: it must be an empty enumeration, not an error.
    let a = Automaton::new(vec![2u32, 3, 2]);
    assert_eq!(a.initial_state(), 0);
    let tmp = TempDir::new().unwrap();
    let dfa = published(&a, &tmp, "none");

    assert_eq!(enumerated(&dfa), Vec::<Vec<u32>>::new());
    assert_eq!(dfa_format::stats::count_accepted(&dfa).unwrap(), 0.0);
}

#[test]
fn accepts_a_single_string() {
    // One accepted string is the case where carrying has to unwind every
    // layer at once to reach the end.
    let mut a = Automaton::new(vec![3u32, 3, 3]);
    let l2 = a.add_state(2, vec![0, 1, 0]);
    let l1 = a.add_state(1, vec![0, 0, l2]);
    let l0 = a.add_state(0, vec![0, l1, 0]);
    a.set_initial_state(l0);

    let tmp = TempDir::new().unwrap();
    let dfa = published(&a, &tmp, "single");

    assert_eq!(enumerated(&dfa), vec![vec![1, 2, 1]]);
}

#[test]
fn one_layer() {
    // ndim == 1 exercises seeding and carrying with no interior layers.
    let mut a = Automaton::new(vec![4u32]);
    let l0 = a.add_state(0, vec![0, 1, 0, 1]);
    a.set_initial_state(l0);

    let tmp = TempDir::new().unwrap();
    let dfa = published(&a, &tmp, "one");

    assert_eq!(enumerated(&dfa), vec![vec![1], vec![3]]);
}

#[test]
fn last_character_of_every_layer_accepted() {
    // The scan for a live character must be able to end on the final
    // character of a layer without walking off the row.
    let mut a = Automaton::new(vec![2u32, 2, 2]);
    let l2 = a.add_state(2, vec![0, 1]);
    let l1 = a.add_state(1, vec![0, l2]);
    let l0 = a.add_state(0, vec![0, l1]);
    a.set_initial_state(l0);

    let tmp = TempDir::new().unwrap();
    let dfa = published(&a, &tmp, "last");

    assert_eq!(enumerated(&dfa), vec![vec![1, 1, 1]]);
}
